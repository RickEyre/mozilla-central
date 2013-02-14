/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WEBVTTLoadListener.h"
#include "TextTrack.h"
#include "TextTrackCue.h"
#include "TextTrackCueList.h"
#include "nsAutoRefTraits.h"

// We might want to look into using #if defined(MOZ_WEBVTT)
class nsAutoRefTraits<webvtt_parser> : public nsPointerRefTraits<webvtt_parser>
{
public:
  static void Release(webvtt_parser aParser) { webvtt_delete_parser(aParser); }
};

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS5(WebVTTLoadListener, nsIRequestObserver,
                   nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor, nsIObserver)

WebVTTLoadListener(HTMLTrackElement *aElement)
  : mElement(aElement),
    mLoadID(aElement->GetCurrentLoadID())
{
  NS_ABORT_IF_FALSE(mElement, "Must pass an element to the callback");
}

WebVTTLoadListener::~WebVTTLoadListener()
{
  if (mParser) {
    mParser.reset();
  }
}

nsresult WebVTTLoadListener::LoadResource()
{
  webvtt_parser *parser;
  
  webvtt_create_parser(&__parsedCue, &__reportError, this, parser);
  if (!parser) {
    return NS_ERROR_FAILURE;
  }

  mParser.own(parser);
  if (!mParser) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_METHOD WevVTTLoadListener::WebVTTParseChunk(nsIInputStream *aInStream, void *aClosure,
                                               const char *aFromSegment, uint32_t aToOffset,
                                               uint32_t aCount, uint32_t *aWriteCount)
{
  // How to determine if this is the final chunk?
  if (!webvtt_parse_chunk(mParser, aFromSegment, aCount, 0))
  {
    // TODO: Handle error
  }

  *aWriteCount = aCount;

  return NS_OK && (*aWriteCount > 0)
}

NS_IMETHODIMP
WebVTTLoadListener::Observe(nsISupports* aSubject,
                            const char *aTopic,
                            const PRUnichar* aData)
{
  nsContentUtils::UnregisterShutdownObserver(this);

  // Clear mElement to break cycle so we don't leak on shutdown
  mElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnStartRequest(nsIRequest* aRequest,
                                   nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnStopRequest(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsresult aStatus)
{
  nsContentUtils::UnregisterShutdownObserver(this);
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnDataAvailable(nsIRequest* aRequest,
                                    nsISupports *aContext,
                                    nsIInputStream* aStream,
                                    uint64_t aOffset,
                                    uint32_t aCount)
{
  nsresult rv;
  uint64_t available;
  bool blocking;

  rv = aStream->IsNonBlocking(&blocking);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aStream->Available(&available);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv = aStream->ReadSegments(WebVTTParseChunk, this, count, &read)

    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ASSERTION(read > 0, "Read 0 bytes while data was available?" );
    count -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                           nsIChannel* aNewChannel,
                                           uint32_t aFlags,
                                           nsIAsyncVerifyRedirectCallback* cb)
{
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::GetInterface(const nsIID &aIID,
                                 void **aResult)
{
  return QueryInterface(aIID, aResult);
}

void 
WebVTTLoadListener::parsedCue(webvtt_cue *aCue) 
{
  ErrorResult rv;

  TextTrackCue textTrackCue = CCueToTextTrackCue(*aCue);
  mElement.Track.addCue(textTrackCue);

  already_AddRefed<DocumentFragment> frag = CNodeListToDomFragment(*aCue, rv);
  if (!frag || rv.Failed()) {
    // Do something with rv.ErrorCode here.
  }

  nsHTMLMediaElement* parent =
      static_cast<nsHTMLMediaElement*>(mElement->mMediaParent.get());

  nsIFrame* frame = mElement->mMediaParent->GetPrimaryFrame();
  if (frame && frame->GetType() == nsGkAtoms::HTMLVideoFrame) {

    nsIContent *overlay = static_cast<nsVideoFrame*>(frame)->GetCaptionOverlay();
    nsCOMPtr<nsIDOMNode> div = do_QueryInterface(overlay);

    if (div) {
      div->appendChild(frag);
    }
  }
}

void 
WebVTTLoadListener::reportError(uint32_t aLine, uint32_t aCol, webvtt_error aError)
{
  // TODO: Handle error here
}

TextTrackCue 
WebVTTLoadListener::CCueToTextTrackCue(const webvtt_cue aCue)
{
  // TODO: Have to figure out what the constructor is here. aNodeInfo??
  TextTrackCue domCue(/* nsISupports *aGlobal here */);

  domeCue.Init(aCue.from, aCue.until, NS_ConvertUTF8toUTF16(aCue->id.d->text), /* ErrorResult &rv */);
  
  domCue.SetSnapToLines(aCue.snap_to_lines);
  domCue.SetSize(aCue.settings.size);
  domCue.SetPosition(aCue.settings.position);
  
  domCue.SetVertical();
  domCue.SetAlign();

  // Not specified in webvtt so we may not need this.
  domCue.SetPauseOnExit();

  return domCue;
}

already_AddRefed<DocumentFragment>
WebVTTLoadListener::CNodeListToDocFragment(const webvtt_node aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  if (!content) {
    return nullptr;
  }

  ErrorResult rv;
  already_AddRefed<DocumentFragment> frag = content.CreateDocumentFragment(rv);
  if (!frag) {
    return nullptr;
  }

  HtmlElement htmlElement;
  for (int i = 0; i < aNode.data->internal_data.length; i++)
  {
    htmlElement = CNodeToHtmlElement(aNode.data->internal_data.children[i]);
    frag.appendChild(htmlElement);
  }

  return frag;
}

HtmlElement
WebVTTLoadListener::CNodeToHtmlElement(const webvtt_node aWebVttNode)
{
  nsAString htmlNamespace = NS_LITERAL_STRING("html");

  if (WEBVTT_IS_VALID_INTERNAL_NODE(aWebVttNode.kind))
  {
    switch (aWebVttNode.kind) {
      case WEBVTT_ITALIC:
        htmlElement.setAttributeNS(htmlNamespace, NS_LITERAL_STRING("u"), nullptr);
        break;
    }

    for (int i = 0; i < aWebVttNode.data->internal_data.length; i++)
    {
      htmlElement.appendChild(CNodeToHtmlElement(aWebVttNode.data->internal_data.children[i]);
    }
  }
  else if (WEBVTT_IS_VALID_LEAF_NODE(aWebVttNode.kind))
  {
    // TODO: Convert to HtmlElement
  }

  return htmlElement;
}

static void WEBVTT_CALLBACK
WebVTTLoadListener::__parsedCue(void *aUserData, webvtt_cue *aCue)
{
  WebVTTLoadListener *self = reinterpret_cast<WebVTTLoadListener *>( userdata );
  self->parsedCue(aCue);
}

static int WEBVTT_CALLBACK 
WebVTTLoadListener::__reportError(void *aUserData, uint32_t aLine, 
                                  uint32_t aCol, webvtt_error aError)
{
  WebVTTLoadListener *self = reinterpret_cast<WebVTTLoadListener *>( userdata );
  self->reportError(aLine, aCol, aError);
}

} // namespace dom
} // namespace mozilla

