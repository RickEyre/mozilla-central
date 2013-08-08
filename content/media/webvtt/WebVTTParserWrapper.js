/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
// TODO: Import vtt.js

var Ci = Components.interfaces;

var WEBVTTPARSERWRAPPER_CID = "{1604a67f-3b72-4027-bcba-6dddd5be6b10}";
var WEBVTTPARSERWRAPPER_CONTRACTID = "@mozilla.org/webvttParserWrapper;1";

function WebVTTParserWrapper()
{
  // Nothing
}

WebVTTParserWrapper.prototype =
{
  loadParser: function(window)
  {
    // TODO: Instantiate JS WebVTT parser
  },

  parse: function(data)
  {
    // We can safely translate the string data to a Uint8Array as we are
    // guaranteed character codes only from \u0000 => \u00ff
    var buffer = new Uint8Array(data.length);
    for (var i = 0; i < data.length; i++) {
      buffer[i] = data.charCodeAt(i);
    }

    // TODO: Call parse on JS WebVTT parser
  },

  flush: function()
  {
    // TODO: Call flush on JS WebVTT parser
  },

  watch: function(callback)
  {
    // TODO: Set callbacks for oncue and onregion for JS WebVTT parser
  },

  convertCueToDOMTree: function(window, cue)
  {
    // TODO: Call convertCueToDOMTree on JS WebVTT parser
  },

  classDescription: "Wrapper for the JS WebVTTParser (vtt.js)",
  classID: Components.ID(WEBVTTPARSERWRAPPER_CID),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebVTTParserWrapper]),
  classInfo: XPCOMUtils.generateCI({
    classID:    WEBVTTPARSERWRAPPER_CID,
    contractID: WEBVTTPARSERWRAPPER_CONTRACTID,
    interfaces: [Ci.nsIWebVTTParserWrapper],
    flags:      Ci.nsIClassInfo.DOM_OBJECT
  })
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebVTTParserWrapper]);
