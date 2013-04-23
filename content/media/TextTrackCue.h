/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCue_h
#define mozilla_dom_TextTrackCue_h

#define WEBVTT_NO_CONFIG_H 1
#define WEBVTT_STATIC 1

#include "TextTrack.h"
#include "mozilla/dom/TextTrackCueBinding.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "webvtt/node.h"

namespace mozilla {
namespace dom {

class TextTrack;

class TextTrackCue MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(TextTrackCue,
                                                         nsDOMEventTargetHelper)
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  // TextTrackCue WebIDL
  static already_AddRefed<TextTrackCue>
  Constructor(GlobalObject& aGlobal,
              const double aStartTime,
              const double aEndTime,
              const nsAString& aText,
              ErrorResult& aRv)
  {
    nsRefPtr<TextTrackCue> ttcue = new TextTrackCue(aGlobal.Get(), aStartTime,
                                                    aEndTime, aText);
    return ttcue.forget();
  }
  TextTrackCue(nsISupports* aGlobal, const double aStartTime,
               const double aEndTime, const nsAString& aText);

  TextTrackCue(nsISupports* aGlobal,  const double aStartTime,
               const double aEndTime, const nsAString& aText,
               HTMLTrackElement *aTrackElement, webvtt_node *head);

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject()
  {
    return mGlobal;
  }

  TextTrack* GetTrack() const
  {
    return mTrack;
  }

  void GetId(nsAString& aId) const
  {
    aId = mId;
  }

  void SetId(const nsAString& aId)
  {
    if (mId == aId) {
      return;
    }

    mId = aId;
    CueChanged();
  }

  double StartTime() const
  {
    return mStartTime;
  }

  void SetStartTime(const double aStartTime)
  {
    //XXXhumph: validate?
    if (mStartTime == aStartTime)
      return;

    mStartTime = aStartTime;
    CueChanged();
  }

  double EndTime() const
  {
    return mEndTime;
  }

  void SetEndTime(const double aEndTime)
  {
    //XXXhumph: validate?
    if (mEndTime == aEndTime)
      return;

    mEndTime = aEndTime;
    CueChanged();
  }

  bool PauseOnExit()
  {
    return mPauseOnExit;
  }

  void SetPauseOnExit(const bool aPauseOnExit)
  {
    if (mPauseOnExit == aPauseOnExit)
      return;

    mPauseOnExit = aPauseOnExit;
    CueChanged();
  }

  void GetVertical(nsAString& aVertical)
  {
    aVertical = mVertical;
  }

  void SetVertical(const nsAString& aVertical)
  {
    if (mVertical == aVertical)
      return;

    mVertical = aVertical;
    CueChanged();
  }

  bool SnapToLines()
  {
    return mSnapToLines;
  }

  void SetSnapToLines(bool aSnapToLines)
  {
    if (mSnapToLines == aSnapToLines)
      return;

    mSnapToLines = aSnapToLines;
    CueChanged();
  }

  double Line() const
  {
    return mLine;
  }

  void SetLine(double aLine)
  {
    //XXX: validate?
    mLine = aLine;
  }

  int32_t Position() const
  {
    return mPosition;
  }

  void SetPosition(int32_t aPosition)
  {
    // XXXhumph: validate?
    if (mPosition == aPosition)
      return;

    mPosition = aPosition;
    CueChanged();
  }

  int32_t Size() const
  {
    return mSize;
  }

  void SetSize(int32_t aSize)
  {
    if (mSize == aSize) {
      return;
    }

    if (aSize < 0 || aSize > 100) {
      //XXX:throw IndexSizeError;
    }

    mSize = aSize;
    CueChanged();
  }

  TextTrackCueAlign Align() const
  {
    return mAlign;
  }

  void SetAlign(TextTrackCueAlign& aAlign)
  {
    mAlign = aAlign;
    CueChanged();
  }

  void GetText(nsAString& aText) const
  {
    aText = mText;
  }

  void SetText(const nsAString& aText)
  {
    // XXXhumph: validate?
    if (mText == aText)
      return;

    mText = aText;
    CueChanged();
  }

  bool
  operator==(const TextTrackCue& rhs) const
  {
    return (mId.Equals(rhs.mId));
  }

  /**
   * Overview of WEBVTT cuetext and anonymous content setup.
   *
   * webvtt_nodes are the parsed version of WEBVTT cuetext. WEBVTT cuetext is
   * the portion of a WEBVTT cue that specifies what the caption will actually
   * show up as on screen.
   *
   * WEBVTT cuetext can contain markup that loosely relates to HTML markup. It
   * can contain tags like <b>, <u>, <i>, <c>, <v>, <ruby>, <rt>, <lang>,
   * including timestamp tags.
   *
   * When the caption is ready to be displayed the webvtt_nodes are converted
   * over to anonymous DOM content. <i>, <u>, <b>, <ruby>, and <rt> all become
   * HTMLElements of their corresponding HTML markup tags. <c> and <v> are
   * converted to <span> tags. Timestamp tags are converted to XML processing
   * instructions. Additionally, all cuetext tags support specifying of classes.
   * This takes the form of <foo.class.subclass>. These classes are then parsed
   * and set as the anonymous content's class attribute.
   *
   * Rules on constructing DOM objects from webvtt_nodes can be found here
   * http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules.
   * Current rules are taken from revision on April 15, 2013.
   */

  /**
   * Converts the TextTrackCue's cuetext into a tree of DOM objects and attaches
   * it to a div on it's owning TrackElement's MediaElement's caption overlay.
   */
  void RenderCue();

  /**
   * Produces a tree of anonymous content based on the tree structure of
   * aWebVTTNode. aWebVTTNode is the head of a tree of webvtt_nodes that
   * represents this TextTrackCue's cuetext.
   *
   * Returns a DocumentFragment that is the head of the tree of anonymous
   * content.
   */
  already_AddRefed<DocumentFragment> GetCueAsHTML();

  /**
   * Converts aWebVTTNode to the appropriate anonymous DOM object.
   *
   * Returns the anonymous content that was constructed based on aWebVTTNode.
   */
  nsCOMPtr<nsIContent>
  ConvertNodeToCueTextContent(const webvtt_node *aWebVTTNode);

  IMPL_EVENT_HANDLER(enter)
  IMPL_EVENT_HANDLER(exit)

private:
  void CueChanged();
  void CreateCueOverlay();

  nsCOMPtr<nsISupports> mGlobal;
  nsString mText;
  double mStartTime;
  double mEndTime;

  nsRefPtr<TextTrack> mTrack;
  HTMLTrackElement* mTrackElement;
  webvtt_node *mHead;
  nsString mId;
  int32_t mPosition;
  int32_t mSize;
  bool mPauseOnExit;
  bool mSnapToLines;
  nsString mVertical;
  double mLine;
  TextTrackCueAlign mAlign;

  // Anonymous child which is appended to VideoFrame's caption display div.
  nsCOMPtr<nsIContent> mCueDiv;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCue_h
