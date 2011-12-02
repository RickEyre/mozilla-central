/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lucas Rocha <lucasr@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.db;

import java.io.ByteArrayOutputStream;
import java.util.Date;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.CursorWrapper;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.provider.Browser;
import android.provider.Browser.BookmarkColumns;

public class AndroidBrowserDB implements BrowserDB.BrowserDBIface {
    private static final String LOGTAG = "GeckoAndroidBrowserDB";

    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { BookmarkColumns.URL, BookmarkColumns.TITLE, BookmarkColumns.FAVICON, "thumbnail" },
                            // The length restriction on URL is for the same reason as in the general bookmark query
                            // (see comment earlier in this file).
                            "(" + Browser.BookmarkColumns.URL + " LIKE ? OR " + Browser.BookmarkColumns.TITLE + " LIKE ?)"
                            + " AND LENGTH(" + Browser.BookmarkColumns.URL + ") > 0",
                            new String[] {"%" + constraint.toString() + "%", "%" + constraint.toString() + "%",},
                            // ORDER BY is number of visits times a multiplier from 1 - 120 of how recently the site
                            // was accessed with a site accessed today getting 120 and a site accessed 119 or more
                            // days ago getting 1
                            Browser.BookmarkColumns.VISITS + " * MAX(1, (" +
                            Browser.BookmarkColumns.DATE + " - " + new Date().getTime() + ") / 86400000 + 120) DESC LIMIT " + limit);

        return new AndroidDBCursor(c);
    }

    public void updateVisitedHistory(ContentResolver cr, String uri) {
        Browser.updateVisitedHistory(cr, uri, true);
    }

    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public Cursor getAllVisitedHistory(ContentResolver cr) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { Browser.BookmarkColumns.URL },
                            Browser.BookmarkColumns.BOOKMARK + " = 0 AND " +
                            Browser.BookmarkColumns.VISITS + " > 0",
                            null,
                            null);

        return new AndroidDBCursor(c);
    }

    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { BookmarkColumns.URL,
                                           BookmarkColumns.TITLE,
                                           BookmarkColumns.FAVICON,
                                           BookmarkColumns.DATE },
                            // Bookmarks that have not been visited have a date value
                            // of 0, so don't pick them up in the history view.
                            Browser.BookmarkColumns.DATE + " > 0",
                            null,
                            Browser.BookmarkColumns.DATE + " DESC LIMIT " + limit);

        return new AndroidDBCursor(c);
    }

    public void clearHistory(ContentResolver cr) {
        Browser.clearHistory(cr);
    }

    public Cursor getAllBookmarks(ContentResolver cr) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { BookmarkColumns.URL, BookmarkColumns.TITLE, BookmarkColumns.FAVICON },
                            // Select all bookmarks with a non-empty URL. When the history
                            // is empty there appears to be a dummy entry in the database
                            // which has a title of "Bookmarks" and no URL; the length restriction
                            // is to avoid picking that up specifically.
                            Browser.BookmarkColumns.BOOKMARK + " = 1 AND LENGTH(" + Browser.BookmarkColumns.URL + ") > 0",
                            null,
                            Browser.BookmarkColumns.TITLE);

        return new AndroidDBCursor(c);
    }

    public boolean isBookmark(ContentResolver cr, String uri) {
        Cursor cursor = cr.query(Browser.BOOKMARKS_URI,
                                 new String[] { BookmarkColumns.URL },
                                 Browser.BookmarkColumns.URL + " = ? and " + Browser.BookmarkColumns.BOOKMARK + " = ?",
                                 new String[] { uri, "1" },
                                 Browser.BookmarkColumns.URL);

        int count = cursor.getCount();
        cursor.close();

        return (count == 1);
    }

    public void addBookmark(ContentResolver cr, String title, String uri) {
        Cursor cursor = cr.query(Browser.BOOKMARKS_URI,
                                 new String[] { BookmarkColumns.URL },
                                 Browser.BookmarkColumns.URL + " = ?",
                                 new String[] { uri },
                                 Browser.BookmarkColumns.URL);

        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.BOOKMARK, "1");
        values.put(Browser.BookmarkColumns.TITLE, title);

        if (cursor.getCount() == 1) {
            // entry exists, update the bookmark flag
            cr.update(Browser.BOOKMARKS_URI,
                      values,
                      Browser.BookmarkColumns.URL + " = ?",
                      new String[] { uri });
        } else {
            // add a new entry
            values.put(Browser.BookmarkColumns.URL, uri);
            cr.insert(Browser.BOOKMARKS_URI, values);
        }

        cursor.close();
    }

    public void removeBookmark(ContentResolver cr, String uri) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.BOOKMARK, "0");

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public BitmapDrawable getFaviconForUrl(ContentResolver cr, String uri) {
        Cursor c = cr.query(Browser.BOOKMARKS_URI,
                            new String[] { Browser.BookmarkColumns.FAVICON },
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { uri },
                            null);

        if (!c.moveToFirst()) {
            c.close();
            return null;
        }

        int faviconIndex = c.getColumnIndexOrThrow(Browser.BookmarkColumns.FAVICON);

        byte[] b = c.getBlob(faviconIndex);
        c.close();

        if (b == null)
            return null;

        Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
        return new BitmapDrawable(bitmap);
    }

    public void updateFaviconForUrl(ContentResolver cr, String uri,
            BitmapDrawable favicon) {
        Bitmap bitmap = favicon.getBitmap();

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);

        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.FAVICON, stream.toByteArray());
        values.put(Browser.BookmarkColumns.URL, uri);

        cr.update(Browser.BOOKMARKS_URI,
                  values,
                  Browser.BookmarkColumns.URL + " = ?",
                  new String[] { uri });
    }

    public void updateThumbnailForUrl(ContentResolver cr, String uri,
            BitmapDrawable thumbnail) {
        Bitmap bitmap = thumbnail.getBitmap();

        Cursor cursor = cr.query(Browser.BOOKMARKS_URI,
                                 new String[] { BookmarkColumns.URL },
                                 Browser.BookmarkColumns.URL + " = ?",
                                 new String[] { uri },
                                 Browser.BookmarkColumns.URL);

        ContentValues values = new ContentValues();
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 0, bos);
        values.put("thumbnail", bos.toByteArray());

        if (cursor.getCount() == 1) {
            // entry exists, simply update image
            cr.update(Browser.BOOKMARKS_URI,
                      values,
                      Browser.BookmarkColumns.URL + " = ?",
                      new String[] { uri });
        } else {
            // add a new entry
            values.put(Browser.BookmarkColumns.URL, uri);
            cr.insert(Browser.BOOKMARKS_URI, values);
        }

        cursor.close();
    }

    private static class AndroidDBCursor extends CursorWrapper {
        public AndroidDBCursor(Cursor c) {
            super(c);
        }

        private String translateColumnName(String columnName) {
            if (columnName.equals(BrowserDB.URLColumns.URL)) {
                columnName = Browser.BookmarkColumns.URL;
            } else if (columnName.equals(BrowserDB.URLColumns.TITLE)) {
                columnName = Browser.BookmarkColumns.TITLE;
            } else if (columnName.equals(BrowserDB.URLColumns.FAVICON)) {
                columnName = Browser.BookmarkColumns.FAVICON;
            } else if (columnName.equals(BrowserDB.URLColumns.THUMBNAIL)) {
                columnName = "thumbnail";
            } else if (columnName.equals(BrowserDB.URLColumns.DATE_LAST_VISITED)) {
                columnName = Browser.BookmarkColumns.DATE;
            }

            return columnName;
        }

        @Override
        public int getColumnIndex(String columnName) {
            return super.getColumnIndex(translateColumnName(columnName));
        }

        @Override
        public int getColumnIndexOrThrow(String columnName) {
            return super.getColumnIndexOrThrow(translateColumnName(columnName));
        }
    }
}
