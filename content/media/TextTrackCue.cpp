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
  nsNodeInfoManager *nodeInfoManager =
    mTrackElement->NodeInfo()->NodeInfoManager();
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
  nsRefPtr<DocumentFragment> frag = GetCueAsHTML();
  if (!frag) {
    return;
  }

  if (!mCueDiv) {
    CreateCueOverlay();
  }

  if (!mTrackElement) {
    return;
  }
  
  HTMLMediaElement* parent =
      static_cast<HTMLMediaElement*>(mTrackElement->mMediaParent.get());

  nsIFrame* frame = parent->GetPrimaryFrame();
  if(!frame || frame->GetType() != nsGkAtoms::HTMLVideoFrame) {
    return;
  }

  nsIContent *overlay = static_cast<nsVideoFrame*>(frame)->GetCaptionOverlay();
  nsCOMPtr<nsINode> div = do_QueryInterface(overlay);
  nsCOMPtr<nsINode> cueDiv = do_QueryInterface(mCueDiv);

  if (!div || !cueDiv) {
    return;
  }

  ErrorResult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(div);
  if (content) {
    nsContentUtils::SetNodeTextContent(content, EmptyString(), true);
    div->AppendChild(*cueDiv, rv);
  }

  content = do_QueryInterface(cueDiv);
  if (content) {
    nsContentUtils::SetNodeTextContent(content, EmptyString(), true);
    cueDiv->AppendChild(*frag, rv);
  }
}

already_AddRefed<DocumentFragment>
TextTrackCue::GetCueAsHTML()
{
  ErrorResult rv;

  nsRefPtr<DocumentFragment> frag =
    mTrackElement->OwnerDoc()->CreateDocumentFragment(rv);
  if (rv.Failed()) {
    return nullptr;
  }

  for (webvtt_uint i = 0; i < mHead->data.internal_data->length; i++) {
    nsCOMPtr<nsIContent> cueTextContent = ConvertNodeToCueTextContent(
                                  mHead->data.internal_data->children[i]);
    if (!cueTextContent) {
      return nullptr;
    }

    nsCOMPtr<nsINode> contentNode = do_QueryInterface(cueTextContent);
    if (!contentNode) {
      return nullptr;
    }

    nsINode *fragNode = frag;
    fragNode->AppendChild(*contentNode, rv);
  }

  return frag.forget();
}

// TODO: Change to iterative solution instead of recursive
nsCOMPtr<nsIContent>
TextTrackCue::ConvertNodeToCueTextContent(const webvtt_node *aWebVTTNode)
{
  nsCOMPtr<nsIContent> cueTextContent;
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
        return nullptr;
        break;
    }
    nsCOMPtr<nsINodeInfo> nodeInfo =
                          nimgr->GetNodeInfo(atomName, nullptr,
                                             kNameSpaceID_XHTML,
                                             nsIDOMNode::ELEMENT_NODE);

    NS_NewHTMLElement(getter_AddRefs(cueTextContent), nodeInfo.forget(),
                      mozilla::dom::NOT_FROM_PARSER);

    if (aWebVTTNode->kind == WEBVTT_VOICE) {
      nsCOMPtr<nsGenericHTMLElement> genericHtmlElement =
        do_QueryInterface(cueTextContent);

      if (genericHtmlElement) {
        const char* text =
            webvtt_string_text(&aWebVTTNode->data.internal_data->annotation);
        genericHtmlElement->SetTitle(NS_ConvertUTF8toUTF16(text));
      }
    }

    webvtt_stringlist *classes = aWebVTTNode->data.internal_data->css_classes;
    if (classes && classes->length > 0) {
      nsAutoString classString;
      const char *text;

      text = webvtt_string_text(classes->items);
      classString.Append(NS_ConvertUTF8toUTF16(text));

      for (webvtt_uint i = 1; i < classes->length; i++) {
        classString.Append(NS_LITERAL_STRING(" "));
        text = webvtt_string_text(classes->items + i);
        classString.Append(NS_ConvertUTF8toUTF16(text));
      }

      nsCOMPtr<nsGenericHTMLElement> genericHtmlElement =
        do_QueryInterface(cueTextContent);
      if (genericHtmlElement) {
        genericHtmlElement->SetClassName(classString);
      }
    }

    ErrorResult rv;
    for (webvtt_uint i = 0; i < aWebVTTNode->data.internal_data->length; i++) {
      nsCOMPtr<nsIContent> childCueTextContent = ConvertNodeToCueTextContent(
        aWebVTTNode->data.internal_data->children[i]);

      if (childCueTextContent) {
        nsCOMPtr<nsINode> childNode = do_QueryInterface(childCueTextContent);
        nsCOMPtr<nsINode> htmlElement = do_QueryInterface(cueTextContent);
        if (childNode && htmlElement) {
          htmlElement->AppendChild(*childNode, rv);
        }
      }
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

        const char* text = webvtt_string_text(&aWebVTTNode->data.text);
        cueTextContent->SetText(NS_ConvertUTF8toUTF16(text), false);
        break;
      }
      case WEBVTT_TIME_STAMP:
      {
        nsAutoString timeStamp;
        timeStamp.AppendInt(aWebVTTNode->data.timestamp);
        NS_NewXMLProcessingInstruction(getter_AddRefs(cueTextContent),
                                       nimgr,
                                       NS_LITERAL_STRING("timestamp"),
                                       timeStamp);
        break;
      }
      default:
        return nullptr;
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
