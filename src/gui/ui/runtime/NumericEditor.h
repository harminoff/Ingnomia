/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <charconv>
#include <functional>
#include <optional>
#include <algorithm>

namespace ingnomia::ui {
// An integer draft is separate from the value accepted by the owning controller.
// RmlUi text controls emit change during editing; only Enter, blur or arrows commit.
class NumericEditor final : public Rml::EventListener {
public:
 using Commit = std::function<bool(int)>;
 NumericEditor(Rml::ElementDocument& document, const Rml::String& id, Commit commit, int upDelta=1, Rml::String upId={}, Rml::String downId={})
  : commit_(std::move(commit)), upDelta_(upDelta) {
  input_=document.GetElementById(id)->GetObserverPtr();
  for(const auto& suffix : {"-up","-down","-error"}) {
   auto* e=document.GetElementById(suffix==Rml::String("-up") && !upId.empty()?upId:suffix==Rml::String("-down") && !downId.empty()?downId:id+suffix);
   if(e) { if(suffix==Rml::String("-up")) up_=e->GetObserverPtr(); else if(suffix==Rml::String("-down")) down_=e->GetObserverPtr(); else error_=e->GetObserverPtr(); }
  }
  input_->SetAttribute("data-numeric-editor",true);
  for(const char* event : {"change","blur","keydown"}) input_->AddEventListener(event,this,Rml::String(event)=="keydown");
  if(up_) up_->AddEventListener("click",this);
  if(down_) down_->AddEventListener("click",this);
 }
 ~NumericEditor() override {
  if(input_) for(const char* event : {"change","blur","keydown"}) input_->RemoveEventListener(event,this,Rml::String(event)=="keydown");
  if(up_) up_->RemoveEventListener("click",this);
  if(down_) down_->RemoveEventListener("click",this);
 }
 static std::optional<int> parse(Rml::String text,int minimum,int maximum) {
  const auto first=text.find_first_not_of(" \t\r\n"),last=text.find_last_not_of(" \t\r\n");
  if(first==Rml::String::npos) return {};
  text=text.substr(first,last-first+1);int n=0;
  const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
  if(r.ec!=std::errc{} || r.ptr!=text.data()+text.size() || n<minimum || n>maximum) return {};
  return n;
 }
 void sync(int value,int minimum,int maximum) {
  minimum_=minimum;maximum_=std::max(minimum,maximum);accepted_=std::clamp(value,minimum_,maximum_);
  if(!input_) return;
  input_->SetAttribute("min",minimum_);input_->SetAttribute("max",maximum_);
  if(!dirty_) write(accepted_);
 }
 bool commit() {
  if(!input_ || input_->HasAttribute("disabled")) return false;
  const auto value=parse(control()->GetValue(),minimum_,maximum_);
  if(!value) {showError(true);return false;}
  if(*value!=accepted_ && !commit_(*value)) {showError(true,"Value was not accepted. Press Escape to restore.");return false;}
  accepted_=*value;dirty_=false;write(accepted_);showError(false);return true;
 }
 void cancel() {dirty_=false;write(accepted_);showError(false);}
 [[nodiscard]] bool disabled() const {return !input_ || input_->HasAttribute("disabled");}
 [[nodiscard]] int minimum() const {return minimum_;}
 [[nodiscard]] int maximum() const {return maximum_;}
 void ProcessEvent(Rml::Event& event) override {
  if(writing_ || !input_) return;
  if(event==Rml::EventId::Change) {dirty_=true;showError(!parse(control()->GetValue(),minimum_,maximum_));}
  else if(event==Rml::EventId::Blur) {if(dirty_) commit();}
  else if(event==Rml::EventId::Click) {step(event.GetCurrentElement()==up_.get()?upDelta_:-upDelta_);}
  else if(event==Rml::EventId::Keydown) {
   const int key=event.GetParameter<int>("key_identifier",0);
   if(key==Rml::Input::KI_RETURN || key==Rml::Input::KI_NUMPADENTER) commit();
   else if(key==Rml::Input::KI_ESCAPE) cancel();
   else if(key==Rml::Input::KI_UP) step(upDelta_);
   else if(key==Rml::Input::KI_DOWN) step(-upDelta_);
   else return;
   event.StopPropagation();
  }
 }
private:
 Rml::ElementFormControlInput* control() const {return static_cast<Rml::ElementFormControlInput*>(input_.get());}
 void write(int value) {if(!input_) return;writing_=true;control()->SetValue(std::to_string(value));writing_=false;}
 void step(int delta) {
  if(input_->HasAttribute("disabled"))return;
  auto current=parse(control()->GetValue(),minimum_,maximum_);
  if(!current){showError(true);return;}
  const auto next=std::clamp(static_cast<long long>(*current)+delta,static_cast<long long>(minimum_),static_cast<long long>(maximum_));
  write(static_cast<int>(next));dirty_=true;commit();
 }
 void showError(bool invalid,Rml::String message={}) {
  input_->SetClass("is-invalid",invalid);input_->SetAttribute("aria-invalid",invalid?"true":"false");
  if(error_) {error_->SetClass("u-hidden",!invalid);if(invalid) error_->SetInnerRML(Rml::StringUtilities::EncodeRml(message.empty()?"Enter a whole number from "+std::to_string(minimum_)+" to "+std::to_string(maximum_)+".":message));}
 }
 Rml::ObserverPtr<Rml::Element> input_,up_,down_,error_;
 Commit commit_;int minimum_=0,maximum_=0,accepted_=0,upDelta_=1;bool dirty_=false,writing_=false;
};
}
