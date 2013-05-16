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
#include "nsTArray.h"

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

  ConvertNodeTreeToDOMTree(frag);

  return frag.forget();
}

void
TextTrackCue::ConvertNodeTreeToDOMTree(nsIContent *parentContent)
{
  nsTArray<webvtt_node*> nodeStack;
  nsTArray<uint16_t> countStack;
  nsCOMPtr<nsIContent> content;

  nodeStack.AppendElement(mHead);

  webvtt_node *node;
  while (nodeStack.Length() > 0) {
    node = nodeStack[nodeStack.Length() - 1];
    nodeStack.RemoveElementAt(nodeStack.Length() - 1);

    if (WEBVTT_IS_VALID_LEAF_NODE(node->kind)) {
      content = ConvertLeafNodeToContent(node);
    } else if (WEBVTT_IS_VALID_INTERNAL_NODE(node->kind)) {
      content = ConvertInternalNodeToContent(node);

      uint16_t childCount = node->data.internal_data->length;
      for (uint16_t i = childCount; i > 0; i--) {
        nodeStack.AppendElement(node->data.internal_data->children[i]);
      }
      if (childCount > 0) {
        countStack.AppendElement(childCount);
      }
    }

    ErrorResult rv;
    nsCOMPtr<nsINode> childNode = do_QueryInterface(content);
    nsCOMPtr<nsINode> parentNode = do_QueryInterface(parentContent);
    if (childNode && parentNode) {
      parentNode->AppendChild(*childNode, rv);
    }

    if ((nodeStack.Length() - countStack[countStack.Length() - 1]) == 
         countStack[countStack.Length() - 1]) {
      nsCOMPtr<nsIContent> temp = parentContent->GetParent();
      if (temp) {
        parentContent = temp;
      }
      countStack.RemoveElementAt(countStack.Length() - 1);
    }
  }
}

nsCOMPtr<nsIContent>
TextTrackCue::ConvertInternalNodeToContent( const webvtt_node *aWebVTTNode )
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
  nsNodeInfoManager *nimgr = mTrackElement->NodeInfo()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo = nimgr->GetNodeInfo(atomName, nullptr,
                                                      kNameSpaceID_XHTML,
                                                      nsIDOMNode::ELEMENT_NODE);

  nsCOMPtr<nsIContent> cueTextContent;
  NS_NewHTMLElement(getter_AddRefs(cueTextContent), nodeInfo.forget(),
                    mozilla::dom::NOT_FROM_PARSER);

  nsCOMPtr<nsGenericHTMLElement> genericHtmlElement;
  const char *text;
  if (aWebVTTNode->kind == WEBVTT_VOICE) {
    genericHtmlElement = do_QueryInterface(cueTextContent);

    if (genericHtmlElement) {
      text = webvtt_string_text(&aWebVTTNode->data.internal_data->annotation);
      genericHtmlElement->SetTitle(NS_ConvertUTF8toUTF16(text));
    }
  }

  webvtt_stringlist *classes = aWebVTTNode->data.internal_data->css_classes;
  if (classes && classes->length > 0) {
    nsAutoString classString;

    text = webvtt_string_text(classes->items);
    classString.Append(NS_ConvertUTF8toUTF16(text));

    for (uint32_t i = 1; i < classes->length; i++) {
      classString.Append(NS_LITERAL_STRING("."));
      text = webvtt_string_text(classes->items + i);
      classString.Append(NS_ConvertUTF8toUTF16(text));
    }

    genericHtmlElement = do_QueryInterface(cueTextContent);
    if (genericHtmlElement) {
      genericHtmlElement->SetClassName(classString);
    }
  }

  return cueTextContent;
}

nsCOMPtr<nsIContent>
TextTrackCue::ConvertLeafNodeToContent( const webvtt_node *aWebVTTNode )
{
  nsCOMPtr<nsIContent> cueTextContent;
  nsNodeInfoManager *nimgr = mTrackElement->NodeInfo()->NodeInfoManager();
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
