/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "gui/ui/runtime/NumericEditor.h"
#include "gui/ui/screens/management6a/Management6ARmlBinding.h"
using namespace ingnomia::ui;
// Label text without the access-key underline markup (Stage 20).
inline std::string plain( const Rml::String& rml ) { std::string out; bool tag = false; for( char c : rml ) { if( c == '<' ) tag = true; else if( c == '>' ) tag = false; else if( !tag ) out += c; } return out; }
int checks = 0;
void check(bool value, const char* label) { ++checks; if(!value) { std::cerr << "FAIL " << label << "\n"; std::exit(1); } }
struct Files : Rml::FileInterface {
    std::filesystem::path root;
    Rml::FileHandle Open(const Rml::String& name) override {
        auto path = std::filesystem::path(name);
        if (!path.has_root_name()) path = root / path.relative_path();
        return reinterpret_cast<Rml::FileHandle>(std::fopen(path.string().c_str(), "rb"));
    }
    void Close(Rml::FileHandle f) override { std::fclose(reinterpret_cast<FILE*>(f)); }
    size_t Read(void* p, size_t size, Rml::FileHandle f) override { return std::fread(p, 1, size, reinterpret_cast<FILE*>(f)); }
    bool Seek(Rml::FileHandle f, long n, int from) override { return std::fseek(reinterpret_cast<FILE*>(f), n, from) == 0; }
    size_t Tell(Rml::FileHandle f) override { return static_cast<size_t>(std::ftell(reinterpret_cast<FILE*>(f))); }
};
struct Renderer : Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override { return 0; }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
#include "gui/ui/screens/ManagementTooltip.h"
#include "gui/ui/runtime/CommandFeedback.h"
struct ShellPort : shell::ShellCommandPort {
 int exits=0;
 shell::CommandResult dispatch(const UiActionEnvelope& a) override {if(a.id.value=="app.exit") ++exits;return {};}
};
struct Port : management6b::CommandPort {
 std::vector<UiActionEnvelope> sent;bool pending=false,reject=false;
 management6b::CommandResult dispatch(const UiActionEnvelope& a) override {sent.push_back(a);return {reject?management6b::CommandStatus::Rejected:management6b::CommandStatus::Accepted,pending,"Rejected by test"};}
 management6b::CommandResult dispatchConfirmed(const UiActionEnvelope& a) override {return dispatch(a);}
 int count(const char* id) {return std::count_if(sent.begin(),sent.end(),[&](auto& a){return a.id.value==id;});}
};
struct TestView : management6b::ViewPort {void stateChanged(const management6b::Management6BState&) override {}};
int main(int argc,char**argv) {
 localization::UiText catalog;
 check(commandFeedbackText(catalog,"ui.error.invalid_or_stale_action").find("target changed")!=std::string::npos,"shared stale-target feedback is readable");
 check(commandFeedbackText(catalog,"No matching items")=="No matching items","domain empty-state text is preserved");
 check(commandFeedbackText(catalog,"ui.error.unknown_failure").find("ui.error.")==std::string::npos,"unknown error key has readable fallback");
 check(argc==2,"asset path");Files files;files.root=argv[1];Renderer renderer;Rml::SystemInterface system;
 Rml::SetFileInterface(&files);Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);check(Rml::Initialise(),"initialize");
 ClassicFocusInstancer decorator;Rml::Factory::RegisterDecoratorInstancer("win98-focus",&decorator);check(Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf"),"font");Rml::LoadFontFace("fonts/MSW98UI-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Bold.ttf");
 auto*c=Rml::CreateContext("stage07",{900,700});
 {
  RmlUiQtInputAdapter input(c);ShellPort port;shell::ShellRmlBinding binding(*c);shell::ShellController controller(port,binding);
  check(binding.initialize(controller),"production shell");c->Update();
  auto* exit=binding.routeDocument()->GetElementById("shell-exit");check(exit!=nullptr,"exit control");exit->Focus();
  input.keyDown(Qt::Key_Return,{});c->Update();
  check(c->GetFocusElement()->GetId()=="confirm-cancel","safe initial focus");auto*modal=c->GetFocusElement()->GetOwnerDocument();
  check(plain(modal->GetElementById("confirm-accept")->GetInnerRML())=="Yes"&&plain(modal->GetElementById("confirm-cancel")->GetInnerRML())=="No","Exit asks Yes / No (Stage 19 message box)");
  check(!modal->GetElementById("confirm-alternate")->IsVisible(true),"unused alternate action absent");
  input.mouseMove(QPointF(5,690),1,{});auto down=input.mouseButtonDown(Qt::LeftButton,{});auto up=input.mouseButtonUp(Qt::LeftButton,{});
  check(down.owner==PointerOwner::Ui&&up.owner==PointerOwner::Ui,"modal pointer gesture cannot reach map");
  for(int n=0;n<8;++n)input.keyDown(Qt::Key_Return,{},true);
  input.keyUp(Qt::Key_Return,{});check(port.exits==0 && modal->IsVisible(),"opening release and held Enter cannot accept");
  for(int n=0;n<12;++n){input.keyDown(Qt::Key_Tab,{});input.keyUp(Qt::Key_Tab,{});c->Update();check(c->GetFocusElement()->GetOwnerDocument()==modal,"Tab remains modal");}
  exit->Focus();check(c->GetFocusElement()->GetOwnerDocument()==modal,"background focus refused");
  input.keyDown(Qt::Key_Escape,{});for(int n=0;n<8;++n)input.keyDown(Qt::Key_Escape,{},true);input.keyUp(Qt::Key_Escape,{});c->Update();
  check(c->GetFocusElement()==exit,"cancel restores exact opener");check(port.exits==0,"cancel emits no exit");
  exit->Click();c->Update();modal=c->GetFocusElement()->GetOwnerDocument();auto*accept=modal->GetElementById("confirm-accept");accept->Focus();
  input.keyDown(Qt::Key_Return,{});for(int n=0;n<8;++n)input.keyDown(Qt::Key_Return,{},true);input.keyUp(Qt::Key_Return,{});c->Update();
  check(port.exits==1,"held accept emits once");
  exit->Click();c->Update();binding.reloadDocuments();c->Update();check(c->GetFocusElement()->GetOwnerDocument()==binding.routeDocument(),"reload removes modal blocker");
  controller.beginWorldTransition(true);controller.setLifecycleProgress("Generating <terrain>");c->Update();
  check(binding.routeDocument()->GetElementById("loading-progress")->GetInnerRML().find("&lt;")!=std::string::npos,"real progress text encoded without fabricated percentage");
  controller.finishWorldTransition(false);c->Update();check(!binding.routeDocument()->GetElementById("loading-error-detail")->GetInnerRML().empty(),"failure explanation is visible");
  binding.shutdown();input.setContext(nullptr);c->Update();
 }
 {
  using namespace management6b;Port port;TestView view;Management6BController controller(port,view);controller.beginWorld(WorldEpoch{9});controller.open(View::Professions);
  const ProfessionRow miner{ProfessionId{"Miner"},"Miner",{CatalogId{"Mining"}}},farmer{ProfessionId{"Farmer"},"Farmer",{CatalogId{"Farming"}}};
  controller.applyProfessions({WorldEpoch{9},Revision{1},{miner,farmer}});controller.selectProfession(miner.id);controller.setProfessionDraftName("Prospector");controller.selectProfession(farmer.id);
  check(controller.state().selectedProfession==miner.id && controller.state().professionDraftName=="Prospector","dirty target switch guarded");
  controller.open(View::Skills);controller.open(View::Professions);check(controller.state().professionDraftDirty,"tab change preserves draft");
  controller.closePopulation();controller.open(View::Professions);check(controller.state().professionDraftDirty,"close and reopen retain draft");
  port.reject=true;controller.saveProfession();check(controller.state().professionDraftDirty&&!controller.state().professionSavePending,"rejected apply preserves draft");port.reject=false;
  controller.discardProfessionDraft();check(!controller.state().professionDraftDirty&&controller.state().professionDraftName=="Miner","discard restores authoritative values");
  controller.setProfessionDraftName("Prospector");auto changed=miner;changed.skills.push_back(CatalogId{"Smithing"});controller.applyProfessions({WorldEpoch{9},Revision{2},{changed,farmer}});
  const auto writes=port.count("profession.update");controller.saveProfession();check(port.count("profession.update")==writes&&controller.state().professionDraftDirty,"external change blocks stale apply");
  controller.deleteReviewedProfession(WorldEpoch{9},miner);check(port.count("profession.delete")==0,"changed deletion target rejected");
  controller.deleteReviewedProfession(WorldEpoch{8},changed);check(port.count("profession.delete")==0,"old world review rejected");
  controller.discardProfessionDraft();controller.setProfessionDraftName("Prospector");port.pending=true;controller.saveProfession();auto request=port.sent.back().request;
  check(controller.state().professionSavePending&&controller.state().professionDraftDirty,"queued apply remains dirty and pending");controller.saveProfession();check(port.count("profession.update")==writes+1,"repeat pending apply blocked");
  controller.onActionFinished(request,{CommandStatus::Rejected,false,"Rejected later"});check(controller.state().professionDraftDirty&&!controller.state().professionSavePending,"async rejection preserves draft");
  controller.saveProfession();controller.applyProfessions({WorldEpoch{9},Revision{3},{{ProfessionId{"Prospector"},"Prospector",{}},farmer}});
  check(controller.state().professionDraftDirty,"name-only snapshot not success");controller.applyProfessionSkills(WorldEpoch{9},ProfessionId{"Prospector"},changed.skills);
  check(!controller.state().professionDraftDirty&&!controller.state().professionSavePending,"matching authoritative skills acknowledge apply");
  controller.applyPopulation({WorldEpoch{9},Revision{1},{{CreatureId{1},"Ada",farmer.id,{}}}});
  controller.setSkillForAllReviewed(CatalogId{"Mining"},false,WorldEpoch{9},Revision{0});check(port.count("population.set_skill_for_all")==0,"changed bulk scope rejected");
  controller.setSkillForAllReviewed(CatalogId{"Mining"},false,WorldEpoch{9},Revision{1});check(port.count("population.set_skill_for_all")==1,"reviewed current bulk scope dispatched");
  controller.selectProfession(farmer.id);controller.applyProfessions({WorldEpoch{9},Revision{4},{}});controller.deleteReviewedProfession(WorldEpoch{9},farmer);check(port.count("profession.delete")==0,"removed target not deleted");
 }
 {
  using namespace management6b;Port port;Management6BRmlBinding binding(*c);Management6BController controller(port,binding);check(binding.initialize(controller),"population production binding");controller.beginWorld(WorldEpoch{2});controller.open(View::Professions);
  const ProfessionRow miner{ProfessionId{"Miner"},"Miner",{}},farmer{ProfessionId{"Farmer"},"Farmer",{}};controller.applyProfessions({WorldEpoch{2},Revision{1},{miner,farmer}});controller.selectProfession(miner.id);c->Update();
  auto*doc=binding.populationDocument();auto*del=doc->GetElementById("profession_delete");

  // Exercise the actual hover chain: direct helper calls and keyboard focus do
  // not reproduce the recursive Context::Update regression seen in WER dumps.
  RmlUiQtInputAdapter pointer(c);
  for (float density : {1.f, 2.f}) {
   c->SetDensityIndependentPixelRatio(density);c->SetDimensions({1280,900});c->Update();
   for(int round=0;round<25;++round) {
    for(const char* id : {"population_tab_citizens","population_tab_skills","population_tab_schedules","population_tab_professions"}) {
     auto* tab=doc->GetElementById(id);if(!tab->IsVisible(true)){doc->GetElementById("population_views_toggle")->Click();c->Update();}tab->ScrollIntoView();c->Update();
     auto p=tab->GetAbsoluteOffset();
     pointer.mouseMove(QPointF(p.x+tab->GetOffsetWidth()/2,p.y+tab->GetOffsetHeight()/2),1,{});
     pointer.mouseButtonDown(Qt::LeftButton,{});pointer.mouseButtonUp(Qt::LeftButton,{});c->Update();
    }
    c->ProcessMouseLeave();
    // Tabs carry text labels and no ToolTip (Stage 12); the caption Close glyph has no label, so it has one.
    auto* target=doc->GetElementById("population_close");target->ScrollIntoView();c->Update();auto p=target->GetAbsoluteOffset();
    pointer.mouseMove(QPointF(p.x+target->GetOffsetWidth()/2,p.y+target->GetOffsetHeight()/2),1,{});c->Update();
    if(!doc->GetElementById("population_tooltip")->IsVisible(true))std::cerr<<"density="<<density<<" round="<<round<<" pos="<<p.x<<","<<p.y<<" visible="<<target->IsVisible(true)<<" hover="<<(c->GetHoverElement()?c->GetHoverElement()->GetId():"null")<<"\n";
    check(doc->GetElementById("population_tooltip")->IsVisible(true),"pointer hover shows Population tooltip without recursive dispatch");
    pointer.mouseMove(QPointF(1279,899),1,{});c->Update();
    check(!doc->GetElementById("population_tooltip")->IsVisible(true),"pointer leave dismisses Population tooltip");
   }
  }
  pointer.setContext(nullptr);c->ProcessMouseLeave();c->SetDensityIndependentPixelRatio(1.f);c->SetDimensions({900,700});c->Update();
  del->Focus();del->Click();c->Update();auto*modal=c->GetFocusElement()->GetOwnerDocument();check(modal!=doc&&modal->HasAttribute("data-modal-dialog"),"profession uses real shared modal");
  check(modal->GetElementById("confirm-detail")->GetInnerRML().find("Miner")!=std::string::npos,"review names actual target");
  controller.applyProfessions({WorldEpoch{2},Revision{2},{farmer}});modal->GetElementById("confirm-accept")->Click();check(port.count("profession.delete")==0,"binding revalidates target removal");
  controller.selectProfession(farmer.id);controller.setProfessionDraftName("Grower");binding.closePopulation();c->Update();modal=c->GetFocusElement()->GetOwnerDocument();check(modal->GetElementById("confirm-cancel")->GetInnerRML()=="Cancel" && plain(modal->GetElementById("confirm-accept")->GetInnerRML())=="Yes" && plain(modal->GetElementById("confirm-alternate")->GetInnerRML())=="No","close asks Yes / No / Cancel about unsaved profession changes");modal->GetElementById("confirm-cancel")->Click();check(controller.state().populationOpen&&controller.state().professionDraftDirty,"keep editing preserves window and draft");
  binding.closePopulation();c->Update();modal=c->GetFocusElement()->GetOwnerDocument();modal->GetElementById("confirm-alternate")->Click();check(!controller.state().populationOpen&&!controller.state().professionDraftDirty,"discard closes after reverting");
  controller.open(View::Professions);controller.selectProfession(farmer.id);c->Update();del->Click();c->Update();controller.endWorld();c->Update();check(!c->GetFocusElement()||!c->GetFocusElement()->GetOwnerDocument()->HasAttribute("data-modal-dialog"),"world end removes modal");binding.shutdown();c->Update();
 }
 {
  auto* doc=c->LoadDocument("fixtures/components.rml");doc->Show();c->Update();
  ModalDialog dialog(*c);int calls=0;
  for(float scale : {1.f,1.25f,1.5f,2.f}) {
   c->SetDensityIndependentPixelRatio(scale);c->SetDimensions({640,420});c->Update();
   check(dialog.show("Review <named object>","The reviewed object has changed. Keep editing or discard the draft.","Apply","Keep editing",[&]{++calls;},{},"Discard draft",[]{}),"modal opens at density");
   check(!dialog.show("Second","Nested arrival","Accept","Cancel",[]{}),"nested review does not replace current action");c->Update();
   auto* d=dialog.document();auto*button=d->GetElementById("confirm-cancel");button->ScrollIntoView();c->Update();auto pos=button->GetAbsoluteOffset();
   std::cout<<"scale="<<scale<<" cancel="<<pos.x<<","<<pos.y<<" size="<<button->GetOffsetWidth()<<","<<button->GetOffsetHeight()<<"\n";
   check(pos.x>=0 && pos.y>=0 && pos.x+button->GetOffsetWidth()<=641 && pos.y+button->GetOffsetHeight()<=421,"safe action reachable at minimum viewport");
   check(d->GetElementById("confirm-title")->GetInnerRML().find("&lt;")!=std::string::npos,"dynamic title is plain text");
   auto* accept=d->GetElementById("confirm-accept");accept->Click();accept->Click();check(calls==int(scale==1?1:scale==1.25f?2:scale==1.5f?3:4),"repeated accept emits once");c->Update();
  }
  auto* source=doc->GetElementById("fixture-command");check(source!=nullptr,"tooltip pilot source");
  auto help=doc->CreateElement("div");help->SetId("stage07-tooltip");help->SetClass("c-tooltip",true);auto* tooltip=doc->AppendChild(std::move(help));
  source->SetAttribute("title","This is a long keyboard-accessible description of a control and its effect. The full description wraps near the viewport edge.");
  source->SetProperty("position","absolute");source->SetProperty("left","580px");source->SetProperty("top","380px");c->Update();
  showManagementTooltip(doc,*c,"stage07-tooltip",source);c->Update();auto offset=tooltip->GetAbsoluteOffset();
  std::cout<<"tooltip="<<offset.x<<","<<offset.y<<" size="<<tooltip->GetOffsetWidth()<<","<<tooltip->GetOffsetHeight()<<"\n";
  check(offset.x>=0&&offset.y>=0&&offset.x+tooltip->GetOffsetWidth()<=641&&offset.y+tooltip->GetOffsetHeight()<=421,"wrapped tooltip stays in 200 percent viewport");
  hideManagementTooltip(doc,"stage07-tooltip");c->Update();check(!tooltip->IsVisible(true),"tooltip dismisses");
  c->UnloadDocument(doc);c->Update();
 }
 Rml::RemoveContext("stage07");Rml::Shutdown();std::cout<<"Stage 07: "<<checks<<" assertions passed\n";
}
