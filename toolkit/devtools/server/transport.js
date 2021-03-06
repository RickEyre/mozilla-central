/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
Components.utils.import("resource://gre/modules/NetUtil.jsm");

/* Turn the error e into a string, without fail. */
function safeErrorString(aError) {
  try {
    var s = aError.toString();
    if (typeof s === "string")
      return s;
  } catch (ee) { }

  return "<failed trying to find error description>";
}

/**
 * Given a handler function that may throw, return an infallible handler
 * function that calls the fallible handler, and logs any exceptions it
 * throws.
 *
 * @param aHandler function
 *      A handler function, which may throw.
 * @param aName string
 *      A name for aHandler, for use in error messages. If omitted, we use
 *      aHandler.name.
 *
 * (SpiderMonkey does generate good names for anonymous functions, but we
 * don't have a way to get at them from JavaScript at the moment.)
 */
function makeInfallible(aHandler, aName) {
  if (!aName)
    aName = aHandler.name;

  return function (/* arguments */) {
    try {
      return aHandler.apply(this, arguments);
    } catch (ex) {
      let msg = "Handler function ";
      if (aName) {
        msg += aName + " ";
      }
      msg += "threw an exception: " + safeErrorString(ex);
      if (ex.stack) {
        msg += "\nCall stack:\n" + ex.stack;
      }

      dump(msg + "\n");

      if (Cu.reportError) {
        /*
         * Note that the xpcshell test harness registers an observer for
         * console messages, so when we're running tests, this will cause
         * the test to quit.
         */
        Cu.reportError(msg);
      }
    }
  }
}

/**
 * An adapter that handles data transfers between the debugger client and
 * server. It can work with both nsIPipe and nsIServerSocket transports so
 * long as the properly created input and output streams are specified.
 *
 * @param aInput nsIInputStream
 *        The input stream.
 * @param aOutput nsIAsyncOutputStream
 *        The output stream.
 *
 * Given a DebuggerTransport instance dt:
 * 1) Set dt.hooks to a packet handler object (described below).
 * 2) Call dt.ready() to begin watching for input packets.
 * 3) Send packets as you please, and handle incoming packets passed to 
 *    hook.onPacket.
 * 4) Call dt.close() to close the connection, and disengage from the event
 *    loop.
 *
 * A packet handler object is an object with two methods:
 *
 * - onPacket(packet) - called when we have received a complete packet.
 *   |Packet| is the parsed form of the packet --- a JavaScript value, not
 *   a JSON-syntax string.
 *
 * - onClosed(status) - called when the connection is closed. |Status| is
 *   an nsresult, of the sort passed to nsIRequestObserver.
 * 
 * Data is transferred as a JSON packet serialized into a string, with the
 * string length prepended to the packet, followed by a colon
 * ([length]:[packet]). The contents of the JSON packet are specified in
 * the Remote Debugging Protocol specification.
 */
this.DebuggerTransport = function DebuggerTransport(aInput, aOutput)
{
  this._input = aInput;
  this._output = aOutput;

  this._converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);
  this._converter.charset = "UTF-8";

  this._outgoing = "";
  this._incoming = "";

  this.hooks = null;
}

DebuggerTransport.prototype = {
  /**
   * Transmit a packet.
   * 
   * This method returns immediately, without waiting for the entire
   * packet to be transmitted, registering event handlers as needed to
   * transmit the entire packet. Packets are transmitted in the order
   * they are passed to this method.
   */
  send: function DT_send(aPacket) {
    // TODO (bug 709088): remove pretty printing when the protocol is done.
    let data = JSON.stringify(aPacket, null, 2);
    data = this._converter.ConvertFromUnicode(data);
    data = data.length + ':' + data;
    this._outgoing += data;
    this._flushOutgoing();
  },

  /**
   * Close the transport.
   */
  close: function DT_close() {
    this._input.close();
    this._output.close();
  },

  /**
   * Flush the outgoing stream.
   */
  _flushOutgoing: function DT_flushOutgoing() {
    if (this._outgoing.length > 0) {
      var threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
      this._output.asyncWait(this, 0, 0, threadManager.currentThread);
    }
  },

  onOutputStreamReady:
  makeInfallible(function DT_onOutputStreamReady(aStream) {
    let written = aStream.write(this._outgoing, this._outgoing.length);
    this._outgoing = this._outgoing.slice(written);
    this._flushOutgoing();
  }, "DebuggerTransport.prototype.onOutputStreamReady"),

  /**
   * Initialize the input stream for reading. Once this method has been
   * called, we watch for packets on the input stream, and pass them to
   * this.hook.onPacket.
   */
  ready: function DT_ready() {
    let pump = Cc["@mozilla.org/network/input-stream-pump;1"]
      .createInstance(Ci.nsIInputStreamPump);
    pump.init(this._input, -1, -1, 0, 0, false);
    pump.asyncRead(this, null);
  },

  // nsIStreamListener
  onStartRequest:
  makeInfallible(function DT_onStartRequest(aRequest, aContext) {},
                 "DebuggerTransport.prototype.onStartRequest"),

  onStopRequest:
  makeInfallible(function DT_onStopRequest(aRequest, aContext, aStatus) {
    this.close();
    if (this.hooks) {
      this.hooks.onClosed(aStatus);
      this.hooks = null;
    }
  }, "DebuggerTransport.prototype.onStopRequest"),

  onDataAvailable:
  makeInfallible(function DT_onDataAvailable(aRequest, aContext,
                                             aStream, aOffset, aCount) {
    this._incoming += NetUtil.readInputStreamToString(aStream,
                                                      aStream.available());
    while (this._processIncoming()) {};
  }, "DebuggerTransport.prototype.onDataAvailable"),

  /**
   * Process incoming packets. Returns true if a packet has been received, either
   * if it was properly parsed or not. Returns false if the incoming stream does
   * not contain a full packet yet. After a proper packet is parsed, the dispatch
   * handler DebuggerTransport.hooks.onPacket is called with the packet as a
   * parameter.
   */
  _processIncoming: function DT__processIncoming() {
    // Well this is ugly.
    let sep = this._incoming.indexOf(':');
    if (sep < 0) {
      return false;
    }

    let count = parseInt(this._incoming.substring(0, sep));
    if (this._incoming.length - (sep + 1) < count) {
      // Don't have a complete request yet.
      return false;
    }

    // We have a complete request, pluck it out of the data and parse it.
    this._incoming = this._incoming.substring(sep + 1);
    let packet = this._incoming.substring(0, count);
    this._incoming = this._incoming.substring(count);

    try {
      packet = this._converter.ConvertToUnicode(packet);
      var parsed = JSON.parse(packet);
    } catch(e) {
      let msg = "Error parsing incoming packet: " + packet + " (" + e + " - " + e.stack + ")";
      if (Cu.reportError) {
        Cu.reportError(msg);
      }
      dump(msg + "\n");
      return true;
    }

    dumpn("Got: " + packet);
    let self = this;
    Services.tm.currentThread.dispatch(makeInfallible(function() {
      self.hooks.onPacket(parsed);
    }, "DebuggerTransport instance's this.hooks.onPacket"), 0);

    return true;
  }
}


/**
 * An adapter that handles data transfers between the debugger client and
 * server when they both run in the same process. It presents the same API as
 * DebuggerTransport, but instead of transmitting serialized messages across a
 * connection it merely calls the packet dispatcher of the other side.
 *
 * @param aOther LocalDebuggerTransport
 *        The other endpoint for this debugger connection.
 *
 * @see DebuggerTransport
 */
this.LocalDebuggerTransport = function LocalDebuggerTransport(aOther)
{
  this.other = aOther;
  this.hooks = null;

  /*
   * A packet number, shared between this and this.other. This isn't used
   * by the protocol at all, but it makes the packet traces a lot easier to
   * follow.
   */
  this._serial = this.other ? this.other._serial : { count: 0 };
}

LocalDebuggerTransport.prototype = {
  /**
   * Transmit a message by directly calling the onPacket handler of the other
   * endpoint.
   */
  send: function LDT_send(aPacket) {
    let serial = this._serial.count++;
    if (wantLogging) {
      if (aPacket.to) {
        dumpn("Packet " + serial + " sent to " + uneval(aPacket.to));
      } else if (aPacket.from) {
        dumpn("Packet " + serial + " sent from " + uneval(aPacket.from));
      }
    }
    this._deepFreeze(aPacket);
    let other = this.other;
    Services.tm.currentThread.dispatch(makeInfallible(function() {
      // Avoid the cost of JSON.stringify() when logging is disabled.
      if (wantLogging) {
        dumpn("Received packet " + serial + ": " + JSON.stringify(aPacket, null, 2));
      }
      other.hooks.onPacket(aPacket);
    }, "LocalDebuggerTransport instance's this.other.hooks.onPacket"), 0);
  },

  /**
   * Close the transport.
   */
  close: function LDT_close() {
    if (this.other) {
      // Remove the reference to the other endpoint before calling close(), to
      // avoid infinite recursion.
      let other = this.other;
      delete this.other;
      other.close();
    }
    if (this.hooks) {
      this.hooks.onClosed();
      this.hooks = null;
    }
  },

  /**
   * An empty method for emulating the DebuggerTransport API.
   */
  ready: function LDT_ready() {},

  /**
   * Helper function that makes an object fully immutable.
   */
  _deepFreeze: function LDT_deepFreeze(aObject) {
    Object.freeze(aObject);
    for (let prop in aObject) {
      // Freeze the properties that are objects, not on the prototype, and not
      // already frozen. Note that this might leave an unfrozen reference
      // somewhere in the object if there is an already frozen object containing
      // an unfrozen object.
      if (aObject.hasOwnProperty(prop) && typeof aObject === "object" &&
          !Object.isFrozen(aObject)) {
        this._deepFreeze(o[prop]);
      }
    }
  }
};
