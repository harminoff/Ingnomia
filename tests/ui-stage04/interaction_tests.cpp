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
#include <cmath>
#include <cstdlib>
using namespace ingnomia::ui;
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

struct Commands : shell::ShellCommandPort {
 std::vector<UiActionEnvelope> sent;
 shell::CommandResult dispatch(const UiActionEnvelope& action) override { sent.push_back(action); return {}; }
 size_t count(const char* id) const { return std::count_if(sent.begin(),sent.end(),[&](const auto& a){return a.id.value==id;}); }
};
void update(Rml::Context& c) { c.Update(); if(connected_tabs::reconcile(c)) c.Update(); }
int main(int argc,char** argv) {
 check(argc==2,"asset path"); Files files; files.root=argv[1]; Renderer renderer; Rml::SystemInterface system;
 Rml::SetFileInterface(&files); Rml::SetRenderInterface(&renderer); Rml::SetSystemInterface(&system);
 check(Rml::Initialise(),"initialize"); ClassicFocusInstancer focus; Rml::Factory::RegisterDecoratorInstancer("win98-focus",&focus);
 check(Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf"),"font");
 auto* c=Rml::CreateContext("stage04",{1280,900});
 {
  RmlUiQtInputAdapter input(c); Commands commands; shell::ShellRmlBinding view(*c); shell::ShellController controller(commands,view);
  check(view.initialize(controller),"production shell initializes");
  const auto open = [&] { controller.activate(shell::ShellControl::OpenNewGame); update(*c); };
  const auto element = [&](const char* id) { auto* e=view.routeDocument()->GetElementById(id);check(e!=nullptr,id);return e; };
  const auto key = [&](int k,Qt::KeyboardModifiers mods=Qt::NoModifier) { auto down=input.keyDown(k,mods);input.keyUp(k,mods);update(*c);return down.uiConsumed; };
  // Stage 18: Custom Game is a wizard. Next is the default command; hidden pages never take the focus.
  const auto page = [&](const char* id) { return element(id)->IsVisible(true); };
  open(); check(c->GetFocusElement()==element("new-next"),"initial focus on Next");
  check(page("new-panel-welcome") && element("new-back")->HasAttribute("disabled"),"wizard starts on Welcome with Back unavailable");
  check(key(Qt::Key_Return),"Enter consumed");check(page("new-panel-world"),"Enter chooses Next");
  controller.setNewGameField(NewGameFieldId{"kingdom_name"},std::string("Retained draft"));
  auto* field=dynamic_cast<Rml::ElementFormControlInput*>(element("new-kingdom-name"));check(field!=nullptr,"text input");
  auto* hiddenField=element("new-kingdom-name");
  for(int i=0;i<12;++i) {key(Qt::Key_Tab);check(c->GetFocusElement()!=hiddenField,"hidden page excluded from Tab");}
  element("new-next")->Click();update(*c);check(page("new-panel-settlement"),"Next shows Settlement");
  field->Focus(true);check(key(Qt::Key_Return),"Enter in a text box consumed");check(page("new-panel-terrain"),"Enter in a text box chooses the default Next");
  element("new-back")->Click();update(*c);check(page("new-panel-settlement") && field->GetValue()=="Retained draft","Back keeps the draft");
  auto* command=element("new-random-name"); command->Focus(true); update(*c);const auto size=command->GetBox().GetSize(Rml::BoxArea::Border); // the border box must not move; a Win98 pressed label shifts inside it
  const auto mouseBefore=commands.count("new_game.randomize_name");
  const auto position=command->GetAbsoluteOffset(Rml::BoxArea::Border);
  input.mouseMove(QPointF(position.x+size.x/2,position.y+size.y/2),1,{});
  input.mouseButtonDown(Qt::LeftButton,{});input.mouseButtonUp(Qt::LeftButton,{});update(*c);
  check(commands.count("new_game.randomize_name")==mouseBefore+1,"mouse hit dispatches command once");
  const auto normalPadding=command->GetBox().GetEdge(Rml::BoxArea::Padding,Rml::BoxEdge::Left); // resolved pixels: Win98 lengths are written in em
  const auto count=commands.count("new_game.randomize_name");
  check(input.keyDown(Qt::Key_Space,{}).suppressText,"Space owns text suppression");update(*c);
  check(command->IsClassSet("is-key-pressed"),"Space paints held state");{const auto held=command->GetBox().GetSize(Rml::BoxArea::Border);check(std::abs(held.x-size.x)<0.01f&&std::abs(held.y-size.y)<0.01f,"held button dimensions stable");}
  check(std::abs(command->GetBox().GetEdge(Rml::BoxArea::Padding,Rml::BoxEdge::Left)-(normalPadding+1.f))<0.01f,"pressed label offset");
  for(int i=0;i<8;++i) {input.keyUp(Qt::Key_Space,{},true);input.keyDown(Qt::Key_Space,{},true);}
  check(commands.count("new_game.randomize_name")==count,"Space never activates while held");
  input.keyUp(Qt::Key_Space,{});check(commands.count("new_game.randomize_name")==count+1,"Space release dispatches exactly once");
  check(!command->IsClassSet("is-key-pressed"),"release clears pressed state");
  input.keyDown(Qt::Key_Return,{});for(int i=0;i<8;++i)input.keyDown(Qt::Key_Return,{},true);input.keyUp(Qt::Key_Return,{});
  check(commands.count("new_game.randomize_name")==count+2,"Enter repeat dispatches once");
  input.keyDown(Qt::Key_Space,{});field->Focus(true);command->Focus(true);input.keyUp(Qt::Key_Space,{});
  check(commands.count("new_game.randomize_name")==count+2,"blur cancels even after focus returns");
  input.keyDown(Qt::Key_Space,{});input.cancelInteraction();input.keyUp(Qt::Key_Space,{});
  check(commands.count("new_game.randomize_name")==count+2,"host focus loss cancels");
  command->SetAttribute("disabled",true);key(Qt::Key_Return);key(Qt::Key_Space);check(commands.count("new_game.randomize_name")==count+2,"disabled command inert");command->RemoveAttribute("disabled");
  field->Focus(true);auto text=input.keyDown(Qt::Key_Space,{});check(!text.suppressText,"editable Space retained");input.committedText(" ");input.keyUp(Qt::Key_Space,{});
  const auto start=commands.count("app.start_new_game");
  check(view.activateElement("new-tab-review"),"Completion page");update(*c);check(page("new-panel-review") && element("new-start")->IsVisible(true) && !element("new-next")->IsVisible(true),"Finish replaces Next");
  element("new-start")->Focus(true);element("new-start")->SetAttribute("disabled",true);key(Qt::Key_Return);check(commands.count("app.start_new_game")==start,"disabled default inert");element("new-start")->RemoveAttribute("disabled");
  auto multiline=view.routeDocument()->CreateElement("textarea");auto* multi=multiline.get();view.routeDocument()->AppendChild(std::move(multiline));update(*c);multi->Focus(true);key(Qt::Key_Return);
  check(commands.count("app.start_new_game")==start,"multiline Enter never invokes default");element("new-start")->Focus(true);
  input.keyDown(Qt::Key_Return,{});for(int i=0;i<6;++i)input.keyDown(Qt::Key_Return,{},true);input.keyUp(Qt::Key_Return,{});
  check(commands.count("app.start_new_game")==start+1,"default Enter dispatches once across route unload");
  controller.endWorld();open();element("shell-back")->Focus(true);
  const auto back=commands.count("nav.back");input.keyDown(Qt::Key_Space,{});view.reloadDocuments();update(*c);input.keyUp(Qt::Key_Space,{});
  check(commands.count("nav.back")==back,"document reload cancels held Space safely");
  check(controller.handleEscape(),"setup Escape consumed");check(controller.state().route.value=="shell.main_menu","Escape goes back");
  open();element("shell-back")->Focus(true);key(Qt::Key_Space);check(commands.count("nav.back")==back+2,"real navigation release dispatch");
  input.setContext(nullptr);view.shutdown();
 }
 {
  RmlUiQtInputAdapter input(c);auto* fixture=c->LoadDocument("fixtures/components.rml");check(fixture!=nullptr,"shared fixture loads");fixture->Show();update(*c);
  auto* overview=fixture->GetElementById("fixture-tab-overview");auto* orders=fixture->GetElementById("fixture-tab-orders");
  check(overview && orders && fixture->GetElementById("fixture-panel-overview"),"fixture associations exist");
  check(fixture->GetElementById("fixture-panel-overview")->IsVisible(),"initial selected panel visible");
  check(!fixture->GetElementById("fixture-panel-orders")->IsVisible() && !fixture->GetElementById("fixture-panel-materials")->IsVisible(),"initial inactive panels hidden before input");
  overview->Focus(true);input.keyDown(Qt::Key_Right,{});input.keyUp(Qt::Key_Right,{});update(*c);check(orders->IsClassSet("is-selected"),"shared horizontal navigation");
  const auto size=orders->GetBox().GetSize();const auto position=orders->GetAbsoluteOffset();
  input.keyDown(Qt::Key_Space,{});update(*c);check(orders->GetBox().GetSize()==size && orders->GetAbsoluteOffset()==position,"selected horizontal tab stable while held");input.keyUp(Qt::Key_Space,{});
  check(orders->GetComputedValues().border_bottom_color()==fixture->GetElementById("fixture-panel-orders")->GetComputedValues().background_color(),"selected tab page seam matches");
  fixture->SetClass("is-high-contrast",true);update(*c);check(orders->GetComputedValues().border_bottom_color()==fixture->GetElementById("fixture-panel-orders")->GetComputedValues().background_color(),"high contrast selected seam");
  auto* command=fixture->GetElementById("fixture-command");check(command!=nullptr,"fixture command");
  struct Count : Rml::EventListener {int value=0;void ProcessEvent(Rml::Event&) override {++value;}} local, remote;
  command->AddEventListener("click",&local);command->Focus(true);input.keyDown(Qt::Key_Space,{});
  auto* peer=Rml::CreateContext("stage04-peer",{400,300});
  {
   RmlUiQtInputAdapter peerInput(peer);
   auto* doc=peer->LoadDocumentFromMemory("<rml><head><style>body {font-family:LatoLatin;} button {tab-index:auto;display:block;width:100px;height:40px;}</style></head><body><button id='peer'>Peer</button></body></rml>");doc->Show();update(*peer);
   auto* button=doc->GetElementById("peer");button->AddEventListener("click",&remote);button->Focus(true);
   peerInput.keyDown(Qt::Key_Return,{});peerInput.keyUp(Qt::Key_Return,{});
   check(remote.value==1 && local.value==0,"peer context cannot activate held main command");
   peerInput.setContext(nullptr);button->RemoveEventListener("click",&remote);
  }
  Rml::RemoveContext("stage04-peer");input.keyUp(Qt::Key_Space,{});check(local.value==1 && remote.value==1,"key ownership remains local to each context");
  command->RemoveEventListener("click",&local);input.setContext(nullptr);c->UnloadDocument(fixture);update(*c);
 }
 Rml::RemoveContext("stage04");Rml::Shutdown();std::cout<<"Stage04 PASS checks="<<checks<<"\n";
}
