/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

function createParser(callback)
{
  var parser = Cc["@mozilla.org/nswebvttparser;1"].
               createInstance(Ci.nsIWebVTTParser); 
  parser.watch(callback);
  return parser;
}

function nsWebVTTParserService()
{
  this.parser = createParser();
}

nsWebVTTParserService.prototype = {
  classDescription: "Service for working with WebVTTParser objects.",
  classID:          Components.ID("{04de1967-5f50-422e-b7d8-7f6cef70e148}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIWebVTTParserService]),
  contractID: "@mozilla.org/nswebvttparserservice;1",
  newParser: function(callback)
  {
    return createParser(callback);
  },
  convertCueToDOMTree: function(window, cuetext)
  {
    this.parser = this.parser || createParser();
    return this.parser.convertCueToDOMTree(window, cuetext);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsWebVTTParserService]);
