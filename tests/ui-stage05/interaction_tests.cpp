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
int checks = 0;
void check(bool value, const char* label) { ++checks; if(std::getenv("STAGE_TRACE")) std::cerr << "ok? " << label << std::endl; if(!value) { std::cerr << "FAIL " << label << "\n"; std::exit(1); } }
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

struct Commands : shell::ShellCommandPort {
 std::vector<UiActionEnvelope> sent;
 shell::CommandResult dispatch(const UiActionEnvelope& action) override { sent.push_back(action); return {}; }
 size_t count(const char* id) const { return std::count_if(sent.begin(),sent.end(),[&](const auto& a){return a.id.value==id;}); }
};
void update(Rml::Context& c) { c.Update(); if(connected_tabs::reconcile(c)) c.Update(); }
struct ManagerPort : management6a::CommandPort {
 std::vector<UiActionEnvelope> sent;bool reject=false;
 management6a::CommandResult dispatch(const UiActionEnvelope& a,management6a::DispatchOrigin) override {sent.push_back(a);return reject?management6a::CommandResult{management6a::CommandStatus::Rejected,false,"Rejected"}:management6a::CommandResult{};}
};
struct ManagerView : management6a::ViewPort {void stateChanged(const management6a::Management6AState&) override {}};
struct ClipboardSystem : Rml::SystemInterface {Rml::String clipboard;void GetClipboardText(Rml::String& text) override {text=clipboard;}};
int main(int argc,char** argv) {
 check(argc==2,"assets");Files files;files.root=argv[1];Renderer renderer;ClipboardSystem system;
 Rml::SetFileInterface(&files);Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);
 check(Rml::Initialise(),"initialize");ClassicFocusInstancer focus;Rml::Factory::RegisterDecoratorInstancer("win98-focus",&focus);Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Bold.ttf");
 auto* c=Rml::CreateContext("stage05",{1280,900});
 const auto tick=[&]{update(*c);};
 {
  RmlUiQtInputAdapter input(c);auto* doc=c->LoadDocumentFromMemory("<rml><head><link type='text/rcss' href='/styles/base.rcss'/><link type='text/rcss' href='/styles/components.rcss'/></head><body><input id='n' class='text' value='2'/><span id='n-error' class='c-field__error u-hidden'/><button id='n-up'>Up</button><button id='n-down'>Down</button><label id='label'><input id='check' class='checkbox' type='checkbox'/><span>Click label</span></label><input id='r1' type='radio' class='radio' name='choices' value='one' checked='checked'/><input id='r2' type='radio' class='radio' name='choices' value='two'/><input id='ro' readonly='readonly' class='text' value='Fixed'/></body></rml>");
  check(doc!=nullptr,"form loads");doc->Show();tick();auto* n=static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("n"));int commits=0,value=2;bool reject=false;
  {
   NumericEditor editor(*doc,"n",[&](int v){++commits;if(reject)return false;value=v;return true;});editor.sync(2,1,10);
   const auto key=[&](int k){input.keyDown(k,{});input.keyUp(k,{});tick();};
   n->Focus(true);
   for(auto raw:{""," ","x","2x","1.5","0","11","999999999999999999999"}) {n->SetValue(raw);key(Qt::Key_Return);check(commits==0,"invalid draft emits no commit");check(n->GetAttribute<Rml::String>("aria-invalid","")=="true","inline invalid state");}
   n->SetValue(" 3 ");key(Qt::Key_Return);check(commits==1&&value==3&&n->GetValue()=="3","whitespace normalized at commit");
   key(Qt::Key_Return);check(commits==1,"same value no duplicate");
   n->SetValue("9");key(Qt::Key_Escape);check(value==3&&n->GetValue()=="3","Escape restores accepted value");
   n->SetValue("");doc->GetElementById("n-up")->Click();check(value==3,"invalid draft cannot be silently stepped");editor.cancel();
   doc->GetElementById("n-up")->Click();check(value==4,"step increments");n->SetValue("10");key(Qt::Key_Return);int before=commits;doc->GetElementById("n-up")->Click();check(value==10&&commits==before,"max step no duplicate");
   n->SetValue("1");key(Qt::Key_Return);before=commits;doc->GetElementById("n-down")->Click();check(value==1&&commits==before,"min step no duplicate");
   reject=true;n->SetValue("5");key(Qt::Key_Return);check(value==1&&n->IsClassSet("is-invalid"),"controller rejection retained as invalid draft");key(Qt::Key_Escape);check(n->GetValue()=="1","rejection Escape restores");tick();check(!doc->GetElementById("n-error")->IsVisible(),"recovered error text is hidden");reject=false;
   n->Focus(true);input.keyDown(Qt::Key_A,Qt::ControlModifier);input.keyUp(Qt::Key_A,Qt::ControlModifier);input.committedText("6");doc->GetElementById("n-up")->Focus(true);tick();check(value==6,"blur commits valid draft once");
   n->Focus(true);n->SetValue("7");n->DispatchEvent("change",{});editor.sync(8,1,10);check(n->GetValue()=="7","snapshot does not destroy draft");key(Qt::Key_Escape);check(n->GetValue()=="8","Escape uses latest authoritative snapshot");
   n->Focus(true);system.clipboard=" 9 ";input.keyDown(Qt::Key_A,Qt::ControlModifier);input.keyUp(Qt::Key_A,Qt::ControlModifier);input.keyDown(Qt::Key_V,Qt::ControlModifier);input.keyUp(Qt::Key_V,Qt::ControlModifier);key(Qt::Key_Return);check(value==9&&n->GetValue()=="9","real clipboard paste trims whitespace on commit");
   system.clipboard="no";input.keyDown(Qt::Key_A,Qt::ControlModifier);input.keyUp(Qt::Key_A,Qt::ControlModifier);input.keyDown(Qt::Key_V,Qt::ControlModifier);input.keyUp(Qt::Key_V,Qt::ControlModifier);before=commits;key(Qt::Key_Return);check(commits==before&&n->IsClassSet("is-invalid"),"invalid pasted text stays uncommitted");key(Qt::Key_Escape);
   n->SetAttribute("disabled",true);before=commits;check(!editor.commit()&&commits==before,"disabled numeric cannot commit");n->RemoveAttribute("disabled");
  }
  struct Changes:Rml::EventListener {int count=0;void ProcessEvent(Rml::Event&)override{++count;}} changes;
  auto* choice=doc->GetElementById("check");choice->AddEventListener("change",&changes);const auto size=choice->GetBox().GetSize();
  doc->GetElementById("label")->Click();tick();check(choice->HasAttribute("checked")&&changes.count==1,"label emits exactly one checkbox change");check(choice->GetBox().GetSize()==size,"check mark does not resize box");
  choice->Focus(true);input.keyDown(Qt::Key_Space,{});for(int i=0;i<5;++i)input.keyDown(Qt::Key_Space,{},true);check(changes.count==1,"held choice unchanged");input.keyUp(Qt::Key_Space,{});check(changes.count==2&&!choice->HasAttribute("checked"),"Space release toggles once");
  choice->SetAttribute("disabled",true);doc->GetElementById("label")->Click();check(changes.count==2,"disabled label cannot toggle");choice->RemoveEventListener("change",&changes);
  auto* r1=doc->GetElementById("r1");auto* r2=doc->GetElementById("r2");r1->Focus(true);input.keyDown(Qt::Key_Right,{});input.keyUp(Qt::Key_Right,{});tick();check(!r1->HasAttribute("checked")&&r2->HasAttribute("checked"),"radio arrows select exclusively");
  auto* ro=static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("ro"));ro->Focus(true);input.keyDown(Qt::Key_Backspace,{});input.committedText("changed");input.keyDown(Qt::Key_V,Qt::ControlModifier);check(ro->GetValue()=="Fixed","read-only rejects typing delete and paste");
  input.setContext(nullptr);c->UnloadDocument(doc);tick();
 }
 {
  RmlUiQtInputAdapter input(c);Commands commands;shell::ShellRmlBinding view(*c);shell::ShellController controller(commands,view);check(view.initialize(controller),"shell binding");controller.activate(shell::ShellControl::OpenNewGame);tick();
  check(view.activateElement("new-tab-world"),"World page of the wizard");tick();
  auto* doc=view.routeDocument();auto* exact=static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("new-world-size-exact"));auto* slider=doc->GetElementById("new-world-size");check(exact&&slider,"production slider exact pair");
  exact->Focus(true);const auto before=commands.count("new_game.set_field");exact->SetValue("145");check(commands.count("new_game.set_field")==before,"typing numeric draft does not dispatch");input.keyDown(Qt::Key_Return,{});input.keyUp(Qt::Key_Return,{});tick();check(commands.count("new_game.set_field")==before+1,"Enter exact dispatches one field update");check(slider->GetAttribute<int>("value",0)==145,"exact reflects into slider");check(commands.count("app.start_new_game")==0,"Enter field does not start game");
  slider->Focus(true);input.keyDown(Qt::Key_Right,{});input.keyUp(Qt::Key_Right,{});tick();check(exact->GetValue()=="146","slider reflects into exact field");check(commands.count("new_game.set_field")==before+2,"slider sync no feedback dispatch");exact->Focus(true);exact->SetValue("999");input.keyDown(Qt::Key_Escape,{});input.keyUp(Qt::Key_Escape,{});check(exact->GetValue()=="146","production Escape restores exact value");
  exact->SetValue("999");exact->DispatchEvent("change",{});doc->GetElementById("new-next")->Click();tick();check(doc->GetElementById("new-panel-world")->IsVisible(true),"Next keeps an invalid page open");check(view.activateElement("new-tab-review"),"Completion page");tick();doc->GetElementById("new-start")->Click();tick();check(commands.count("app.start_new_game")==0&&doc->GetElementById("new-panel-world")->IsVisible(true),"Finish reveals invalid draft instead of skipping it");
  input.keyDown(Qt::Key_Escape,{});input.keyUp(Qt::Key_Escape,{});
  input.setContext(nullptr);view.shutdown();tick();
 }
 {
  c->SetDimensions({720,720});RmlUiQtInputAdapter input(c);ManagerPort port;ManagerView screen;management6a::Management6AController controller(port,screen);management6a::Management6ARmlBinding binding(*c);
  check(binding.initialize(controller),"manager binding");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{3});management6a::StockpileSnapshot snapshot;snapshot.id=StockpileId{7};snapshot.name="Supplies";snapshot.maxPriority=3;snapshot.priority=1;snapshot.templateNames={"Food","Wood","Stone"};controller.showStockpile(snapshot,Revision{1},WorldPosition{1,2,3});controller.setStockpilePane(management6a::StockpilePane::Settings);tick();auto* doc=binding.stockpileDocument();
  auto* n=static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("stockpile_priority"));n->Focus(true);auto before=port.sent.size();n->SetValue("0");input.keyDown(Qt::Key_Return,{});input.keyUp(Qt::Key_Return,{});check(port.sent.size()==before,"stockpile invalid priority blocked");input.keyDown(Qt::Key_Escape,{});input.keyUp(Qt::Key_Escape,{});check(n->GetValue()=="2","stockpile restores 1-based display");
  doc->GetElementById("stockpile_priority_up")->Click();check(port.sent.size()==before,"priority arrow stages without dispatch");doc->GetElementById("stockpile_apply")->Click();check(port.sent.size()==before+1,"Apply emits one staged action");check(std::get<SetStockpileBasicsPayload>(port.sent.back().payload).priority==0,"raise priority lowers displayed rank");snapshot.priority=0;controller.showStockpile(snapshot,Revision{2},WorldPosition{1,2,3});tick();
  auto* pull=doc->GetElementById("stockpile_toggle_pull");before=port.sent.size();pull->GetParentNode()->Click();check(port.sent.size()==before&&controller.state().stockpile.draft.options.pull,"production hauling label stages one pending change");
  controller.setStockpilePane(management6a::StockpilePane::AllowList);tick();auto* name=doc->GetElementById("stockpile_template_name");name->Focus(true);input.keyDown(Qt::Key_Down,{});input.keyUp(Qt::Key_Down,{});tick();check(controller.state().stockpile.templateMenuOpen,"combo keyboard opens");check(c->GetFocusElement()->HasAttribute("data-template"),"combo focuses actual catalog option");
  auto* popup=doc->GetElementById("stockpile_template_options");const auto popupOrigin=popup->GetAbsoluteOffset(Rml::BoxArea::Border);const auto popupSize=popup->GetBox().GetSize(Rml::BoxArea::Border);check(popupOrigin.x>=0&&popupOrigin.y>=0&&popupOrigin.x+popupSize.x<=720&&popupOrigin.y+popupSize.y<=720,"template popup inside supported host bounds");
  before=port.sent.size();input.keyDown(Qt::Key_Down,{});input.keyUp(Qt::Key_Down,{});check(port.sent.size()==before,"combo navigation does not apply");input.keyDown(Qt::Key_Escape,{});input.keyUp(Qt::Key_Escape,{});tick();check(!controller.state().stockpile.templateMenuOpen&&c->GetFocusElement()==name,"combo Escape cancels and restores focus");
  input.keyDown(Qt::Key_Down,{});input.keyUp(Qt::Key_Down,{});tick();input.keyDown(Qt::Key_Return,{});input.keyUp(Qt::Key_Return,{});tick();check(port.sent.size()==before&&!controller.state().stockpile.templateName.empty(),"combo Enter fills the text box without applying");check(!controller.state().stockpile.templateMenuOpen&&c->GetFocusElement()==name,"selection closes popup and returns focus");check(binding.activateElement("stockpile_template_load"),"Load");tick();check(port.sent.size()==before,"Load asks before replacing the allow list");check(binding.activateElement("stockpile_review_accept"),"accept template replacement");tick();check(port.sent.size()==before+1&&port.sent.back().id.value=="stockpile.apply_template","confirmed Load applies the template once");
  doc->GetElementById("stockpile_template_toggle")->Click();tick();doc->GetElementById("stockpile_view_settings")->DispatchEvent("mousedown",{});check(!controller.state().stockpile.templateMenuOpen,"click-away closes popup");
  controller.toggleStockpileTemplateMenu();controller.close();check(!controller.state().stockpile.templateMenuOpen,"parent close discards open popup");
  management6a::WorkshopSnapshot workshop;workshop.id={91};workshop.name="Crude workbench";workshop.maxPriority=3;
  management6a::WorkshopProductRow plank;plank.id=CatalogId{"Plank"};plank.components.push_back({CatalogId{"RawWood"},1,true,{{CatalogId{"any"},150},{CatalogId{"AppleWood"},0}}});workshop.products.push_back(plank);
  controller.showWorkshop(workshop,Revision{1},WorldPosition{1,2,3});tick();auto* workshopDoc=binding.workshopDocument();
  // Order type is a documented drop-down list box on the Windows 98 property sheet.
  auto* orderType=static_cast<Rml::ElementFormControlSelect*>(workshopDoc->GetElementById("workshop_order_mode"));
  check(orderType&&orderType->GetValue()=="once"&&orderType->GetNumOptions()==3,"production order type initial state");before=port.sent.size();orderType->SetValue("maintain");tick();check(controller.state().workshop.orderMode==CraftRepeatMode::Maintain&&orderType->GetValue()=="maintain","order type drop-down updates the real order draft");check(port.sent.size()==before,"order type change does not queue craft");
  auto* select=static_cast<Rml::ElementFormControlSelect*>(workshopDoc->GetElementById("workshop_material_0"));check(select&&select->GetNumOptions()==2,"material combo uses supplied legal catalog");
  select->Focus(true);select->Click();tick();check(select->IsSelectBoxVisible(),"native material combo opens");input.keyDown(Qt::Key_Down,{});input.keyUp(Qt::Key_Down,{});input.keyDown(Qt::Key_Escape,{});input.keyUp(Qt::Key_Escape,{});tick();check(!select->IsSelectBoxVisible(),"native combo Escape closes");check(select->GetValue()=="any"&&controller.state().workshop.orderMaterials.front().value=="any","native combo Escape restores opening value and draft");
  select->Click();tick();input.keyDown(Qt::Key_Down,{});input.keyUp(Qt::Key_Down,{});input.keyDown(Qt::Key_Return,{});input.keyUp(Qt::Key_Return,{});tick();check(!select->IsSelectBoxVisible()&&controller.state().workshop.orderMaterials.front().value=="AppleWood","native material choice reaches order draft");
  select->Click();tick();c->SetDimensions({640,480});tick();check(select==workshopDoc->GetElementById("workshop_material_0"),"resize preserves native combo identity");controller.close();tick();check(!workshopDoc->IsVisible(),"parent closes with native combo open");
  input.setContext(nullptr);controller.removeViewPort(binding);binding.shutdown();tick();
 }
 Rml::RemoveContext("stage05");Rml::Shutdown();std::cout<<"Stage05 PASS checks="<<checks<<"\n";
}
