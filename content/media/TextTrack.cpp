/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextTrack.h"
#include "mozilla/dom/TextTrackBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(TextTrack, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrack)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TextTrack, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrack, nsDOMEventTargetHelper)

TextTrack::TextTrack(nsISupports* aParent,
                     const nsAString& aKind,
                     const nsAString& aLabel,
                     const nsAString& aLanguage)
  : mParent(aParent)
  , mKind(aKind)
  , mLabel(aLabel)
  , mLanguage(aLanguage)
  , mMode(TextTrackMode::Hidden)
  , mCueList(new TextTrackCueList(aParent))
  , mActiveCueList(new TextTrackCueList(aParent))
{
  SetIsDOMBinding();
}

TextTrack::TextTrack(nsISupports* aParent)
  : mParent(aParent)
  , mKind(NS_LITERAL_STRING("subtitles"))
  , mMode(TextTrackMode::Disabled)
  , mCueList(new TextTrackCueList(aParent))
  , mActiveCueList(new TextTrackCueList(aParent))
{
  SetIsDOMBinding();
}

void
TextTrack::Update(double time)
{
  mCueList->Update(time);
}

JSObject*
TextTrack::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return TextTrackBinding::Wrap(aCx, aScope, this);
}

void
TextTrack::SetMode(TextTrackMode aValue)
{
  mMode = aValue;
}

void
TextTrack::AddCue(TextTrackCue& aCue,
                  ErrorResult& aRv)
{
  // TODO
  // If the text track list of cues does not yet have any associated rules for
  // updating the text track rendering, then associate the text track list of
  // cues with the rules for updating the text track rendering appropriate to
  // cue.

  // TODO
  // If text track list of cues' associated rules for updating the text track
  // rendering are not the same rules for updating the text track rendering as
  // appropriate for cue, then throw an InvalidStateError exception and abort
  // these steps.

  // If the given cue is in a text track list of cues, then remove cue from
  // that text track list of cues.
  if( DoesContainCue(aCue) == true){
    RemoveCue(aCue, aRv);
  }

  // Add cue to the method's TextTrack object's text track's text track list
  // of cues.
  mCueList->AddCue(aCue);

  // TODO
  // If the TextTrack object's text track is in a media element's list of text
  // tracks, run the time marches on steps for that media element.
}

void
TextTrack::RemoveCue(TextTrackCue& aCue,
                     ErrorResult& aRv)
{
  // If the given cue is not currently listed in the
  // method's TextTrack object's text track's text
  // track list of cues, then throw a NotFoundError
  // exception and abort these steps.
  if( DoesContainCue(aCue) == false ) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  // Remove cue from the method's TextTrack object's text track's text track
  // list of cues.
  mCueList->RemoveCue(aCue);
}

void
TextTrack::CueChanged(TextTrackCue& aCue)
{
  //XXX: cue changed handling
}

bool
TextTrack::DoesContainCue(TextTrackCue& aCue)
{
  nsString cueId;
  aCue.GetId(cueId);

  TextTrackCue* cue = mCueList->GetCueById(cueId);
  if(cue == nullptr) {
    return false;
  }
  return true;
}

} // namespace dom
} // namespace mozilla
