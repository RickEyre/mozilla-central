/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackCueBinding.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "nsIFrame.h"
#include "nsDeque.h"
#include "nsTextNode.h"
#include "nsVideoFrame.h"
#include "webvtt/cue.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(TextTrackCue,
                                        mGlobal,
                                        mTrack,
                                        mTrackElement)

NS_IMPL_ADDREF_INHERITED(TextTrackCue, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackCue, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackCue)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

// Set cue setting defaults based on step 19 & seq.
// in http://dev.w3.org/html5/webvtt/#parsing
void
TextTrackCue::SetDefaultCueSettings()
{
  mPosition = 50;
  mSize = 100;
  mPauseOnExit = false;
  mSnapToLines = true;
  mLine = WEBVTT_AUTO;
  mAlign = TextTrackCueAlign::Middle;
}

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText)
  : mGlobal(aGlobal)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mHead(nullptr)
{
  SetDefaultCueSettings();
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
}

TextTrackCue::TextTrackCue(nsISupports* aGlobal,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText,
                           HTMLTrackElement* aTrackElement,
                           webvtt_node* head)
  : mGlobal(aGlobal)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mTrackElement(aTrackElement)
  , mHead(head)
{
  // Use the webvtt library's reference counting.
  webvtt_ref_node(mHead);
  SetDefaultCueSettings();
  MOZ_ASSERT(aGlobal);
  SetIsDOMBinding();
}

TextTrackCue::~TextTrackCue()
{
  if (mHead) {
    // Release our reference and null mHead.
    webvtt_release_node(&mHead);
  }
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
  nsRefPtr<DocumentFragment> frag =
    mTrackElement->OwnerDoc()->CreateDocumentFragment();

  ConvertNodeTreeToDOMTree(frag);

  return frag.forget();
}

void
TextTrackCue::ConvertNodeTreeToDOMTree(nsIContent* parentContent)
{
  nsDeque nodeStack;
  nsDeque childBranchStack;

  nodeStack.Push((void *)mHead);

  webvtt_node* node;
  while (nodeStack.GetSize() > 0) {
    node = (webvtt_node*)nodeStack.Pop();

    nsCOMPtr<nsIContent> content;
    if (WEBVTT_IS_VALID_LEAF_NODE(node->kind)) {
      content = ConvertLeafNodeToContent(node);
    } else if (WEBVTT_IS_VALID_INTERNAL_NODE(node->kind)) {
      content = ConvertInternalNodeToContent(node);

      uint16_t childCount = node->data.internal_data->length;
      if (childCount > 0) {
        if (nodeStack.GetSize() > 0) {
          // Remember the last branching parent so that we know when we need to
          // move back up the DOM tree.
          parentStack.Push((void*)nodeStack.Peek());
        }
        for (uint16_t i = childCount; i > 0; i--) {
          // Append the child nodes in reverse order so we process them in the
          // right order -- left to right.
          nodeStack.Push((void*)node->data.internal_data->children[i - 1]);
        }
      }
    }

    if (content && parentContent) {
      ErrorResult rv;
      nsCOMPtr<nsINode> childNode = do_QueryInterface(content);
      nsCOMPtr<nsINode> parentNode = do_QueryInterface(parentContent);
      if (childNode && parentNode) {
        parentNode->AppendChild(*childNode, rv);
      }
      // If we're on an internal node and we have children then move deeper into
      // the DOM tree.
      if (WEBVTT_IS_VALID_INTERNAL_NODE(node->kind) &&
          node->data.internal_data->length > 0) {
        parentContent = content;
      }
    }

    // If we've reached the last branching parent then we have processed all the
    // current child nodes -- move back up the DOM tree.
    if (nodeStack.GetSize() > 0 && (parentStack.Peek() == nodeStack.Peek())) {
        parentStack.Pop();
        nsCOMPtr<nsIContent> temp = parentContent->GetParent();
        if (temp) {
          parentContent = temp;
        }
    }
  }
}

nsCOMPtr<nsIContent>
TextTrackCue::ConvertInternalNodeToContent( const webvtt_node* aWebVTTNode )
{
  nsIAtom* atomName;

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
  nsNodeInfoManager* nimgr = mTrackElement->NodeInfo()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo = nimgr->GetNodeInfo(atomName, nullptr,
                                                      kNameSpaceID_XHTML,
                                                      nsIDOMNode::ELEMENT_NODE);

  nsCOMPtr<nsIContent> cueTextContent;
  NS_NewHTMLElement(getter_AddRefs(cueTextContent), nodeInfo.forget(),
                    mozilla::dom::NOT_FROM_PARSER);

  nsCOMPtr<nsGenericHTMLElement> genericHtmlElement;
  const char* text;
  if (aWebVTTNode->kind == WEBVTT_VOICE) {
    genericHtmlElement = do_QueryInterface(cueTextContent);

    if (genericHtmlElement) {
      text = webvtt_string_text(&aWebVTTNode->data.internal_data->annotation);
      if (text) {
        genericHtmlElement->SetTitle(NS_ConvertUTF8toUTF16(text));
      }
    }
  }

  webvtt_stringlist* classes = aWebVTTNode->data.internal_data->css_classes;
  if (classes && classes->length > 0) {
    nsAutoString classString;

    text = webvtt_string_text(classes->items);
    if (text) {
      classString.Append(NS_ConvertUTF8toUTF16(text));
    }

    for (uint32_t i = 1; i < classes->length; i++) {
      classString.Append(NS_LITERAL_STRING("."));
      text = webvtt_string_text(classes->items + i);
      if (text) {
        classString.Append(NS_ConvertUTF8toUTF16(text));
      }
    }

    genericHtmlElement = do_QueryInterface(cueTextContent);
    if (genericHtmlElement) {
      genericHtmlElement->SetClassName(classString);
    }
  }
  return cueTextContent;
}

nsCOMPtr<nsIContent>
TextTrackCue::ConvertLeafNodeToContent( const webvtt_node* aWebVTTNode )
{
  nsCOMPtr<nsIContent> cueTextContent;
  nsNodeInfoManager* nimgr = mTrackElement->NodeInfo()->NodeInfoManager();
  switch (aWebVTTNode->kind) {
      case WEBVTT_TEXT:
      {
        cueTextContent = new nsTextNode(nimgr);

        if (!cueTextContent) {
          return nullptr;
        }

        const char* text = webvtt_string_text(&aWebVTTNode->data.text);
        if (text) {
          cueTextContent->SetText(NS_ConvertUTF8toUTF16(text), false);
        }
        break;
      }
      case WEBVTT_TIME_STAMP:
      {
        nsAutoString timeStamp;
        timeStamp.AppendInt(aWebVTTNode->data.timestamp);
        cueTextContent =
          NS_NewXMLProcessingInstruction(nimgr, NS_LITERAL_STRING("timestamp"),
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
TextTrackCue::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
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
