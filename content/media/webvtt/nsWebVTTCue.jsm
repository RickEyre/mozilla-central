/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function nsWebVTTCue(cue) {
  this.id = cue.id || "";
  this.startTime = cue.startTime || 0;
  this.endTime = cue.endTime || 0;
  this.regionId = cue.regionId || "";
  this.vertical = cue.vertical || "";
  this.snapToLines = cue.snapToLines || true;
  this.line = cue.line || "auto";
  this.position = cue.position || 50;
  this.size = cue.size || 100;
  this.aign = cue.align || "middle";
}

nsWebVTTCue.prototype =
{
  classDescription: "Contains the parsed data of a WebVTT cue.",
  classID:          Components.ID("{778bdcb1-1f80-4db0-a093-348083cb5af9}}"),
  QueryInterface:   XPCOMUtils.generateQI([Components.interfaces.nsIWebVTTCue]),
  contractID: "@mozilla.org/nswebvttcue;1"
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsWebVTTCue]);
