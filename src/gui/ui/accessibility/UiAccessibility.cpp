/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "UiAccessibility.h"
#include <algorithm>
namespace ingnomia::ui::accessibility
{
EscapeLayer escapeTarget(const EscapeContext&c)noexcept{if(c.composing)return EscapeLayer::Composition;if(c.modal&&c.modalDismissible)return EscapeLayer::DismissibleModal;if(c.modal)return EscapeLayer::None;if(c.overlay)return EscapeLayer::Overlay;if(c.workbench)return EscapeLayer::Workbench;if(c.dock)return EscapeLayer::Dock;if(c.activeTool)return EscapeLayer::ActiveTool;return EscapeLayer::Game;}
float clampScale(float value)noexcept{return std::clamp(value,0.8f,2.0f);}
void FocusHistory::remember(FocusToken token){if(!token.document.empty()&&!token.element.empty())token_=std::move(token);}
std::optional<FocusToken>FocusHistory::take(std::string_view document){if(!token_||token_->document!=document)return{};auto out=std::move(token_);token_.reset();return out;}
void FocusHistory::clear(){token_.reset();}
}
