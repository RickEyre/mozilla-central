/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackListBinding.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/dom/TextTrackRegionList.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(TextTrackList,
                                     nsDOMEventTargetHelper,
                                     mGlobal,
                                     mTextTracks)

NS_IMPL_ADDREF_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrackList::TextTrackList(nsISupports* aGlobal) : mGlobal(aGlobal)
{
  SetIsDOMBinding();
}

void
TextTrackList::GetAllActiveCues(nsTArray<nsRefPtr<TextTrackCue> >& aCues)
{
  nsTArray< nsRefPtr<TextTrackCue> > cues;
  for (uint32_t i = 0; i < Length(); i++) {
    if (mTextTracks[i]->Mode() != TextTrackMode::Disabled) {
      mTextTracks[i]->GetActiveCueArray(cues);
      aCues.AppendElements(cues);
    }
  }
}

void
TextTrackList::GetAllRegions(nsTArray<nsRefPtr<TextTrackRegion> >& aRegions)
{
  nsTArray<nsRefPtr<TextTrackRegion> > regions;
  for (uint32_t i = 0; i < Length(); i++) {
    if (mTextTracks[i]->Mode() != TextTrackMode::Disabled) {
      mTextTracks[i]->GetRegions()->GetArray(regions);
      aRegions.AppendElements(regions);
    }
  }
}

JSObject*
TextTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackListBinding::Wrap(aCx, aScope, this);
}

TextTrack*
TextTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTextTracks.Length();
  return aFound ? mTextTracks[aIndex] : nullptr;
}

already_AddRefed<TextTrack>
TextTrackList::AddTextTrack(HTMLMediaElement* aMediaElement,
                            TextTrackKind aKind,
                            const nsAString& aLabel,
                            const nsAString& aLanguage)
{
  nsRefPtr<TextTrack> track = new TextTrack(mGlobal, aMediaElement, aKind,
                                            aLabel, aLanguage);
  mTextTracks.AppendElement(track);
  // TODO: dispatch addtrack event
  return track.forget();
}

TextTrack*
TextTrackList::GetTrackById(const nsAString& aId)
{
  nsAutoString id;
  for (uint32_t i = 0; i < Length(); i++) {
    mTextTracks[i]->GetId(id);
    if (aId.Equals(id)) {
      return mTextTracks[i];
    }
  }
  return nullptr;
}

void
TextTrackList::RemoveTextTrack(TextTrack* aTrack)
{
  mTextTracks.RemoveElement(aTrack);
}

void
TextTrackList::DidSeek()
{
  for (uint32_t i = 0; i < mTextTracks.Length(); i++) {
    mTextTracks[i]->SetDirty();
  }
}

} // namespace dom
} // namespace mozilla
