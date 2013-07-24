/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function nsWebVTTRegion(region) {
  this.id = region.id || "";
  this.width = region.width || 100;
  this.lines = region.lines || 3;
  this.regionAnchorX = region.regionAnchorX || 0;
  this.regionAnchorY = region.regionAnchorY || 100;
  this.viewportAnchorX = region.viewportAnchorX || 0;
  this.viewportAnchorY = region.viewportAnchorY || 100;
  this.scroll = region.scroll || "none";
}

nsWebVTTRegion.prototype = {
  classDescription: "Contains the parsed data of a WebVTT region.",
  classID:          Components.ID("{140a1aa2-35cd-441c-867d-70766a2b7009}")
  QueryInterface:   XPCOMUtils.generateQI([Components.interfaces.nsIWebVTTRegion]),
  contractID: "@mozilla.org/nswebvttregion;1"
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsWebVTTRegion]);
