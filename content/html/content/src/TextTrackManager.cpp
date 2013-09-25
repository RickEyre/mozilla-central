/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsComponentManagerUtils.h"
#include "nsVideoFrame.h"
#include "nsIFrame.h"
#include "nsTArrayHelpers.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(TextTrackManager, mTextTracks)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TextTrackManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TextTrackManager, Release)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackManager::sParserWrapper;

TextTrackManager::TextTrackManager(HTMLMediaElement *aMediaElement)
  : mMediaElement(aMediaElement)
{
  MOZ_COUNT_CTOR(TextTrackManager);
  mTextTracks = new TextTrackList(mMediaElement->OwnerDoc()->GetParentObject());

 nsresult rv;
  nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
    do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID, &rv);
  sParserWrapper = parserWrapper;
  ClearOnShutdown(&sParserWrapper);
}

TextTrackManager::~TextTrackManager()
{
  MOZ_COUNT_DTOR(TextTrackManager);
}

TextTrackList*
TextTrackManager::TextTracks() const
{
  return mTextTracks;
}

already_AddRefed<TextTrack>
TextTrackManager::AddTextTrack(TextTrackKind aKind, const nsAString& aLabel,
                               const nsAString& aLanguage)
{
  return mTextTracks->AddTextTrack(mMediaElement, aKind, aLabel, aLanguage);
}

void
TextTrackManager::AddTextTrack(TextTrack* aTextTrack)
{
  mTextTracks->AddTextTrack(aTextTrack);
}

void
TextTrackManager::RemoveTextTrack(TextTrack* aTextTrack)
{
  mTextTracks->RemoveTextTrack(aTextTrack);
}

void
TextTrackManager::DidSeek()
{
  mTextTracks->DidSeek();
}

bool
TextTrackManager::GetDirtyCues(nsTArray<nsRefPtr<TextTrackCue> >& aCues)
{
  mTextTracks->GetAllActiveCues(aCues);
  for (uint32_t i = 0; i < aCues.Length(); i++) {
    if (aCues[i]->Reset()) {
      return true;
    }
  }
  return false;
}

template<class T> JS::Value
TextTrackManager::ArrayToJSArray(const nsTArray<T>& aArray)
{
  AutoPushJSContext cx(
    nsContentUtils::GetContextFromDocument(mMediaElement->OwnerDoc()));

  JSObject* jsArray;
  nsTArrayToJSArray(cx, aArray, &jsArray);

  return JS::ObjectValue(*jsArray);
}

void
TextTrackManager::UpdateCueDisplay()
{
  nsIFrame* frame = mMediaElement->GetPrimaryFrame();
  if (!frame) {
    return;
  }

  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    return;
  }

  nsIContent* overlay =  videoFrame->GetCaptionOverlay();
  if (!overlay) {
    return;
  }

  nsTArray<nsRefPtr<TextTrackCue> > activeCues;
  if (GetDirtyCues(activeCues)) {

    nsTArray<nsRefPtr<TextTrackRegion> > regions;
    mTextTracks->GetAllRegions(regions);

    JS::Value jsCues = ArrayToJSArray<nsRefPtr<TextTrackCue> >(activeCues);
    JS::Value jsRegions = ArrayToJSArray<nsRefPtr<TextTrackRegion> >(regions);

    nsPIDOMWindow* window = mMediaElement->OwnerDoc()->GetWindow();
    if (window) {
      sParserWrapper->ProcessCues(window, jsCues, jsRegions, overlay);
    }
  }

  nsContentUtils::SetNodeTextContent(overlay, EmptyString(), true);

  nsGenericHTMLElement* element = static_cast<nsGenericHTMLElement*>(overlay);
  nsCOMPtr<nsIDOMNode> throwAway;
  for (uint32_t i = 0; i < activeCues.Length(); i++) {
    element->AppendChild(activeCues[i]->DisplayState(),
                         getter_AddRefs(throwAway));
  }

  return;
}

} // namespace dom
} // namespace mozilla
