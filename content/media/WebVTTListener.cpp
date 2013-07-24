/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebVTTListener.h"
#include "mozilla/dom/TextTrackCue.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "VideoUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_2(WebVTTListener, mElement, mParser)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebVTTListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebVTTListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebVTTListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebVTTListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebVTTListener)

#ifdef PR_LOGGING
PRLogModuleInfo* gTextTrackLog;
# define LOG(...) PR_LOG(gTextTrackLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
# define LOG(msg)
#endif

WebVTTListener::WebVTTListener(HTMLTrackElement* aElement)
  : mElement(aElement)
{
  MOZ_ASSERT(mElement, "Must pass an element to the callback");
#ifdef PR_LOGGING
  if (!gTextTrackLog) {
    gTextTrackLog = PR_NewLogModule("TextTrack");
  }
#endif
  LOG("WebVTTListener created.");
}

WebVTTListener::~WebVTTListener()
{
  LOG("WebVTTListener destroyed.");
}

NS_IMETHODIMP
WebVTTListener::GetInterface(const nsIID &aIID,
                             void** aResult)
{
  return QueryInterface(aIID, aResult);
}

nsresult
WebVTTListener::LoadResource()
{
  if (!HTMLTrackElement::IsWebVTTEnabled()) {
    NS_WARNING("WebVTT support disabled."
               " See media.webvtt.enabled in about:config. ");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  mWebVTTService = do_GetService(NS_WEBVTTPARSERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mWebVTTService->NewParser(this, getter_AddRefs(mParser));
  NS_ENSURE_SUCCESS(rv, rv);

  mElement->mReadyState = HTMLTrackElement::LOADING;
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                       nsIChannel* aNewChannel,
                                       uint32_t aFlags,
                                       nsIAsyncVerifyRedirectCallback* cb)
{
  if (mElement) {
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnStartRequest(nsIRequest* aRequest,
                               nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnStopRequest(nsIRequest* aRequest,
                              nsISupports* aContext,
                              nsresult aStatus)
{
  if(mElement->mReadyState != HTMLTrackElement::ERROR) {
    mElement->mReadyState = HTMLTrackElement::LOADED;
  }
  // Attempt to parse any final data the parser might still have.
  mParser->Flush();
  return NS_OK;
}

NS_METHOD
WebVTTListener::ParseChunk(nsIInputStream* aInStream, void* aClosure,
                           const char* aFromSegment, uint32_t aToOffset,
                           uint32_t aCount, uint32_t* aWriteCount)
{
  WebVTTListener* listener = static_cast<WebVTTListener*>(aClosure);
  if (NS_FAILED(listener->mParser->Parse(aFromSegment))) {
    LOG("Unable to parse chunk of WEBVTT text. Aborting.");
    *aWriteCount = 0;
    return NS_ERROR_FAILURE;
  }
  *aWriteCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnDataAvailable(nsIRequest* aRequest,
                                nsISupports* aContext,
                                nsIInputStream* aStream,
                                uint64_t aOffset,
                                uint32_t aCount)
{
  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv = aStream->ReadSegments(ParseChunk, this, count, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!read) {
      return NS_ERROR_FAILURE;
    }
    count -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnCue(nsIWebVTTCue *aCue)
{
  double start, end;
  nsAutoString text, vertical, align, id;

  nsresult rv;
  rv = aCue->GetStartTime(&start);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCue->GetEndTime(&end);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCue->GetText(text);
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult res;
  nsRefPtr<TextTrackCue> cue =
    new TextTrackCue(mElement->OwnerDoc()->GetParentObject(),
                    MS_TO_SECONDS(start), MS_TO_SECONDS(end),
                    text, mElement, res);
  if (res.Failed()) {
    return res.ErrorCode();
  }

  rv = aCue->GetVertical(vertical);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetVertical(vertical);

  rv = aCue->GetAlign(align);
  NS_ENSURE_SUCCESS(rv, rv);
  // TODO: Add string method to take here.
  //cue->SetAlign(align);

  rv = aCue->GetId(id);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetId(id);

  int32_t position, size, line;
  rv = aCue->GetPosition(&position);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetPosition(position);

  rv = aCue->GetSize(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetSize(size);

  rv = aCue->GetLine(&line);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetLine(line);

  bool snapToLines;
  rv = aCue->GetSnapToLines(&snapToLines);
  NS_ENSURE_SUCCESS(rv, rv);
  cue->SetSnapToLines(snapToLines);

  mElement->mTrack->AddCue(*cue);
  return NS_OK;
}

NS_IMETHODIMP
WebVTTListener::OnRegion(nsIWebVTTRegion *aRegion)
{
  // TODO: Implement Regions in Gecko. See bug 897504.
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
