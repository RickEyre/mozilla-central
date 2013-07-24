/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://webvtt/vtt.jsm");

function nsWebVTTParser()
{
  this.parser = new WebVTTParser(new TextDecoder("utf8"));
}

nsWebVTTParser.prototype =
{
  classDescription: "Wrapper for the JS WebVTTParser (vtt.js)",
  classID:          Components.ID("{71bf19e5-4f03-4807-abb8-3a646b530e60}"),
  QueryInterface:   XPCOMUtils.generateQI([Components.interfaces.nsIWebVTTParser]),
  contractID: "@mozilla.org/nswebvttparser;1",
  parse: function(data)
  {
    // We can safely translate the string data to a Uint8Array as we are
    // guaranteed character codes only from \u0000 => \u00ff
    var buffer = new Uint8Array(data.length)
    for (var i = 0; i < data.length; i++)
      buffer[i] = data.charCodeAt(i);

    this.parser.parse(buffer);
  },
  flush: function()
  {
    this.parser.flush();
  },
  watch: function(callback)
  {
    this.parser.oncue = callback.onCue || null;
    this.parser.onregion = callback.onRegion || null;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsWebVTTParser]);
