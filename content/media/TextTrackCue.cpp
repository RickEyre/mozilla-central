/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextTrackCue.h"
#include "mozilla/dom/TextTrackCueBinding.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsIFrame.h"
#include "nsVideoFrame.h"
#include "webvtt/string.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(TextTrackCue, mGlobal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackCue)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TextTrackCue, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackCue, nsDOMEventTargetHelper)

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           const double aStartTime,
                           const double aEndTime,
                           const nsAString& aText)
  : mGlobal(aGlobal)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mPosition(50)
  , mSize(100)
  , mPauseOnExit(false)
  , mSnapToLines(true)
{
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
}

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           const double aStartTime,
                           const double aEndTime,
                           const nsAString& aText,
                           HTMLTrackElement* aTrackElement,
                           webvtt_node* head)
  : mGlobal(aGlobal)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mTrackElement(aTrackElement)
  , mHead(head)
  , mPosition(50)
  , mSize(100)
  , mPauseOnExit(false)
  , mSnapToLines(true)
{
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
}

void
TextTrackCue::CreateCueOverlay() 
{
  nsNodeInfoManager *nodeInfoManager = mTrackElement->NodeInfo()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::div,
                                nullptr,
                                kNameSpaceID_XHTML,
                                nsIDOMNode::ELEMENT_NODE);
  mCueDiv = NS_NewHTMLDivElement(nodeInfo.forget());
}

void
TextTrackCue::RenderCue()
{
  ErrorResult rv;
  nsRefPtr<DocumentFragment> frag = GetCueAsHTML();
  if (!frag.get() || rv.Failed()) {
    // TODO: Do something with rv.ErrorCode here.
  }

  if (!mCueDiv) {
    CreateCueOverlay();
  }

  HTMLMediaElement* parent =
      static_cast<HTMLMediaElement*>(mTrackElement->mMediaParent.get());

  nsIFrame* frame = parent->GetPrimaryFrame();
  if (frame && frame->GetType() == nsGkAtoms::HTMLVideoFrame) {

    nsIContent *overlay =
      static_cast<nsVideoFrame*>(frame)->GetCaptionOverlay();
    nsCOMPtr<nsIDOMNode> div = do_QueryInterface(overlay);
    nsCOMPtr<nsIDOMNode> cueDiv = do_QueryInterface(mCueDiv);

    if (div) {
      nsCOMPtr<nsIDOMNode> resultNodeCueDiv;
      nsCOMPtr<nsIDOMNode> resultNode;

      nsCOMPtr<nsIContent> content = do_QueryInterface(div);
      uint32_t childCount = content->GetChildCount();
      for (uint32_t i = 0; i < childCount; ++i) {
        content->RemoveChildAt(i, true);
      }

      div->AppendChild(cueDiv, getter_AddRefs(resultNodeCueDiv));

      content = do_QueryInterface(cueDiv);
      childCount = content->GetChildCount();
      for (uint32_t i = 0; i < childCount; ++i) {
        content->RemoveChildAt(i, true);
      }

      cueDiv->AppendChild(frag.get(), getter_AddRefs(resultNode));
    }
  }
}
  
already_AddRefed<DocumentFragment>
TextTrackCue::GetCueAsHTML()
{
  ErrorResult rv;
  
  // TODO: Do we need to do something with this error result?
  nsRefPtr<DocumentFragment> frag =
    mTrackElement->OwnerDoc()->CreateDocumentFragment(rv);
    
  // TODO: Should this happen?
  if (!frag.get()) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMNode> resultNode;
  for (webvtt_uint i = 0; i < mHead->data.internal_data->length; i++) {    
    nsCOMPtr<nsIContent> cueTextContent = 
      ConvertNodeToCueTextContent(mHead->data.internal_data->children[i]);
    
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(cueTextContent);
    
    frag.get()->AppendChild(node, getter_AddRefs(resultNode));
  }

  return frag.forget();
}

// TODO: Change to iterative solution instead of recursive
nsCOMPtr<nsIContent>
TextTrackCue::ConvertNodeToCueTextContent(const webvtt_node *aWebVTTNode)
{
  nsCOMPtr<nsIContent> cueTextContent;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsNodeInfoManager *nimgr = mTrackElement->NodeInfo()->NodeInfoManager();
  
  if (WEBVTT_IS_VALID_INTERNAL_NODE(aWebVTTNode->kind))
  {
    nsIAtom *atomName;
    switch (aWebVTTNode->kind) {
      case WEBVTT_BOLD:
        atomName = nsGkAtoms::b;
        break;
      case WEBVTT_ITALIC:
        atomName = nsGkAtoms::i;
        break;
      case WEBVTT_UNDERLINE:
        atomName = nsGkAtoms::u;
        break;
      case WEBVTT_RUBY:
        atomName = nsGkAtoms::ruby;
        break;
      case WEBVTT_RUBY_TEXT:
        atomName = nsGkAtoms::rt;
        break;
      case WEBVTT_VOICE:
      {
        atomName = nsGkAtoms::span;
        break;
      case WEBVTT_CLASS:
        atomName = nsGkAtoms::span;
        break;
      }
      default:
        // TODO: What happens here?
        cueTextContent = nullptr;
        break;
    }
    nodeInfo = nimgr->GetNodeInfo(atomName, nullptr, kNameSpaceID_XHTML,
                                  nsIDOMNode::ELEMENT_NODE);
    
    NS_NewHTMLElement(getter_AddRefs(cueTextContent), nodeInfo.forget(),
                      mozilla::dom::NOT_FROM_PARSER);
    
    if (aWebVTTNode->kind == WEBVTT_VOICE) {
      nsCOMPtr<nsGenericHTMLElement> genericHtmlElement =
        do_QueryInterface(cueTextContent);
      
      const char* text = reinterpret_cast<const char *>(
          webvtt_string_text(&aWebVTTNode->data.internal_data->annotation));
      
      genericHtmlElement->SetTitle(NS_ConvertUTF8toUTF16(text));
    }
    
    webvtt_stringlist *cssClasses = aWebVTTNode->data.internal_data->css_classes;
    
    if (cssClasses->length > 0) {
      nsAutoString classes;
      const char *text;
  
      text = reinterpret_cast<const char *>(webvtt_string_text(cssClasses->items));
      classes.Append(NS_ConvertUTF8toUTF16(text));
      
      for (webvtt_uint i = 1; i < cssClasses->length; i++) {
        classes.Append(NS_LITERAL_STRING(" "));
        text = reinterpret_cast<const char *>(webvtt_string_text(cssClasses->items + i));
        classes.Append(NS_ConvertUTF8toUTF16(text));
      }
      
      nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(cueTextContent);
      htmlElement->SetClassName(classes);
    }

    for (webvtt_uint i = 0; i < aWebVTTNode->data.internal_data->length; i++) {   
      nsCOMPtr<nsIDOMNode> resultNode, childNode;
      nsCOMPtr<nsIContent> childCueTextContent;

      childCueTextContent = ConvertNodeToCueTextContent(
        aWebVTTNode->data.internal_data->children[i]);
      
      childNode = do_QueryInterface(childCueTextContent); 
      nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(cueTextContent);  
      htmlElement->AppendChild(childNode, getter_AddRefs(resultNode));
    }
  }
  else if (WEBVTT_IS_VALID_LEAF_NODE(aWebVTTNode->kind))
  {
    switch (aWebVTTNode->kind) {
      case WEBVTT_TEXT:
      {
        NS_NewTextNode(getter_AddRefs(cueTextContent), nimgr);

        if (!cueTextContent) {
          return nullptr;
        }
  
        const char* text = reinterpret_cast<const char *>(
          webvtt_string_text(&aWebVTTNode->data.text));

        cueTextContent->SetText(NS_ConvertUTF8toUTF16(text), false);
  
        break;
      }
      case WEBVTT_TIME_STAMP:
      {
        nsAutoString timeStamp;
        timeStamp.AppendInt(aWebVTTNode->data.timestamp);
        NS_NewXMLProcessingInstruction(getter_AddRefs(cueTextContent),
                                       nodeInfo->NodeInfoManager(),
                                       NS_LITERAL_STRING("timestamp"),
                                       timeStamp);
        break;
      }
      default:
        // TODO: What happens here?
        cueTextContent = nullptr;
        break;
    }
  }

  return cueTextContent;
}

JSObject*
TextTrackCue::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return TextTrackCueBinding::Wrap(aCx, aScope, this);
}

void
TextTrackCue::CueChanged()
{
  if (mTrack) {
    mTrack->CueChanged(*this);
  }
}
} // namespace dom
} // namespace mozilla
