/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <functional>
#include <string>

namespace ingnomia::ui
{
// Owns a real modal document. Callbacks are moved out before dispatch, so a
// synchronous snapshot or another dialog cannot execute the reviewed action twice.
class ModalDialog final : public Rml::EventListener
{
public:
    explicit ModalDialog(Rml::Context& context) : context_(context) {}
    ~ModalDialog() { close(false); }
    bool active() const { return document_ != nullptr; }
    /// Uses another dialog document with the same element IDs (for example the Windows 98 message box).
    void setDocumentPath(std::string path) { path_=std::move(path); }
    Rml::ElementDocument* document() const { return document_; }
    bool show(const std::string& title, const std::string& detail,
        const std::string& accept, const std::string& cancel,
        std::function<void()> action, std::function<void()> canceled = {},
        const std::string& alternate = {}, std::function<void()> alternateAction = {})
    {
        if(active()) return false; // Never replace an outstanding review.
        if(auto* focus=context_.GetFocusElement()) returnFocus_=focus->GetObserverPtr();
        document_=context_.LoadDocument(path_);
        if(!document_) return false;
        action_=std::move(action); canceled_=std::move(canceled); alternate_=std::move(alternateAction);
        for(const auto& [id,value] : {std::pair{"confirm-title",title}, {"confirm-detail",detail}})
            document_->GetElementById(id)->SetInnerRML(Rml::StringUtilities::EncodeRml(value));
        // Command buttons get an access key on their first letter, underlined; OK and Cancel have none (PDF p.328).
        for(const auto& [id,value] : {std::pair{"confirm-accept",accept},{"confirm-cancel",cancel},{"confirm-alternate",alternate}})
        {
            auto* button=document_->GetElementById(id);
            if(value.empty() || value=="OK" || value=="Cancel") { button->SetInnerRML(Rml::StringUtilities::EncodeRml(value)); button->RemoveAttribute("accesskey"); continue; }
            button->SetInnerRML("<span class=\"w98-ak\">"+Rml::StringUtilities::EncodeRml(value.substr(0,1))+"</span>"+Rml::StringUtilities::EncodeRml(value.substr(1)));
            button->SetAttribute("accesskey",value.substr(0,1));
        }
        document_->GetElementById("confirm-alternate")->SetProperty("display",alternate.empty() ? "none" : "block");
        // An empty accept label makes a single-button message box (OK only).
        document_->GetElementById("confirm-accept")->SetProperty("display",accept.empty() ? "none" : "block");
        document_->AddEventListener("click",this);
        document_->AddEventListener("keydown",this);
        document_->Show(Rml::ModalFlag::Modal);
        document_->GetElementById("confirm-cancel")->Focus();
        return true;
    }
    void close(bool restore=true)
    {
        if(!document_) return;
        auto* old=document_; document_=nullptr;
        old->RemoveEventListener("click",this); old->RemoveEventListener("keydown",this);
        old->Hide(); context_.UnloadDocument(old);
        action_={}; canceled_={}; alternate_={};
        if(restore && returnFocus_ && returnFocus_->IsVisible(true)) returnFocus_->Focus();
        returnFocus_.reset();
    }
    void ProcessEvent(Rml::Event& event) override
    {
        if(!document_) return;
        std::function<void()> action;
        if(event.GetId()==Rml::EventId::Keydown)
        {
            if(event.GetParameter<int>("key_identifier",0)!=Rml::Input::KI_ESCAPE) return;
            action=std::move(canceled_);
        }
        else
        {
            auto* target=event.GetTargetElement();
            while(target && target->GetTagName()!="button") target=target->GetParentNode();
            if(!target || target->HasAttribute("disabled")) return;
            const auto id=target->GetId();
            if(id=="confirm-accept") action=std::move(action_);
            else if(id=="confirm-alternate") action=std::move(alternate_);
            else if(id=="confirm-cancel" || id=="confirm-close") action=std::move(canceled_);
            else return;
        }
        event.StopPropagation();
        close();
        if(action) action();
    }
private:
    Rml::Context& context_;
    std::string path_{"modals/confirm_destructive.rml"};
    Rml::ElementDocument* document_{};
    Rml::ObserverPtr<Rml::Element> returnFocus_;
    std::function<void()> action_, canceled_, alternate_;
};
}
