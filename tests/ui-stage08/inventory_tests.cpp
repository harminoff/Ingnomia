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
struct ManagerPort : management6a::CommandPort {
 std::vector<UiActionEnvelope> sent;bool reject=false;
 management6a::CommandResult dispatch(const UiActionEnvelope& a,management6a::DispatchOrigin) override {sent.push_back(a);return reject?management6a::CommandResult{management6a::CommandStatus::Rejected,false,"Rejected"}:management6a::CommandResult{};}
};
struct ManagerView : management6a::ViewPort {void stateChanged(const management6a::Management6AState&) override {}};
struct ClipboardSystem : Rml::SystemInterface {Rml::String clipboard;void GetClipboardText(Rml::String& text) override {text=clipboard;}};

#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
#include <chrono>
namespace mb=ingnomia::ui::management6b;
struct PortB:mb::CommandPort {std::vector<UiActionEnvelope> sent;mb::CommandResult dispatch(const UiActionEnvelope&a)override{sent.push_back(a);return{};}mb::CommandResult dispatchConfirmed(const UiActionEnvelope&a)override{return dispatch(a);}};
struct ViewB:mb::ViewPort {void stateChanged(const mb::Management6BState&)override{}};
int countElements(Rml::Element* e){int n=1;for(int i=0;i<e->GetNumChildren();++i)n+=countElements(e->GetChild(i));return n;}
Rml::Element* part(Rml::Element*e,const char*tag){if(e->GetTagName()==tag)return e;for(int i=e->GetNumChildren(true)-1;i>=0;--i)if(auto*p=part(e->GetChild(i),tag))return p;return nullptr;}

int main(int argc,char**argv){
 check(argc==2,"assets");Files files;files.root=argv[1];Renderer renderer;ClipboardSystem system;
 Rml::SetFileInterface(&files);Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);check(Rml::Initialise(),"initialize");ClassicFocusInstancer focus;Rml::Factory::RegisterDecoratorInstancer("win98-focus",&focus);Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Bold.ttf");
 auto*c=Rml::CreateContext("stage08",{720,720});c->SetDefaultScrollBehavior(Rml::ScrollBehavior::Instant,1.f);auto tick=[&]{c->Update();c->Update();};
 {
 PortB port;ViewB view;mb::Management6BController controller(port,view);mb::Management6BRmlBinding binding(*c);check(binding.initialize(controller),"binding loads");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{8});controller.open(mb::View::Inventory);
 std::vector<mb::InventoryRow> data;for(int i=0;i<10000;++i){mb::InventoryRow r;r.id={CatalogId{"Materials"},CatalogId{"Raw"},CatalogId{"Item"+std::to_string(i)},CatalogId{"Oak"},InventoryDepth::Material};r.name="A long material description "+std::to_string(i);r.stockpiled=i%7;r.total=i%19;data.push_back(r);}
 for(int i=0;i<10000;++i){auto parent=data[i];parent.id.material={};parent.id.depth=InventoryDepth::Item;parent.name="Item"+std::to_string(i);data.push_back(parent);}
 mb::InventoryRow category;category.id={CatalogId{"Materials"},{},{},{},InventoryDepth::Category};category.name="Materials";data.push_back(category);mb::InventoryRow group;group.id={CatalogId{"Materials"},CatalogId{"Raw"},{},{},InventoryDepth::Group};group.name="Raw";data.push_back(group);
 // Stage 21b: Inventory is a Windows 98 report window (Close only); Item Properties is a subordinate sheet.
 auto start=std::chrono::steady_clock::now();controller.applyInventory({WorldEpoch{8},Revision{1},data});c->SetDimensions({384,380});tick();auto*doc=binding.inventoryDocument();auto*rows=doc->GetElementById("inventory_rows");
 std::cout<<"initial_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" row_children="<<rows->GetNumChildren()<<"\n";
 const auto text=[&](const char* id){auto* e=doc->GetElementById(id);return e?std::string(e->GetInnerRML()):std::string();};
 check(doc->GetElementById("inventory_workbench")->IsClassSet("w98-sheet"),"Inventory is a Windows 98 window");check(text("inventory_heading")=="Inventory","caption");
 check(rows->GetNumChildren()<120,"10000 entries remain virtualized");check(text("inventory_matches")=="10000 items","the count names every item");
 check(doc->GetElementById("inventory_close_button")!=nullptr && doc->GetElementById("inventory_ok")==nullptr,"Close is the only commit button");
 RmlUiQtInputAdapter input(c);auto key=[&](int k){input.keyDown(k,{});input.keyUp(k,{});tick();};
 const char* columns[]={"item","material","stock","total"};
 const mb::Sort sorts[]={mb::Sort::Item,mb::Sort::Material,mb::Sort::Stock,mb::Sort::Total};
 for(int column=0;column<4;++column){
  auto*sort=doc->GetElementById(std::string("inventory_sort_")+columns[column]);sort->Click();tick();check(controller.state().inventorySort==sorts[column],"each heading sorts by its column");auto descending=controller.state().inventorySortDescending;sort->Click();tick();check(controller.state().inventorySortDescending!=descending,"a second click reverses the order");
  check(!doc->GetElementById((std::string("inventory_mark_")+columns[column]).c_str())->IsClassSet("is-hidden"),"the sort arrow sits on the sorted heading");
 }
 auto find=[&](const char* value){auto* f=static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("inventory_search"));f->SetValue(value);f->DispatchEvent("input",Rml::Dictionary{});tick();};
 find("Item9");check(controller.state().inventoryFilter=="Item9"&&!controller.inventoryPage().empty()&&controller.inventoryPage().size()<10000,"Find narrows the list");check(text("inventory_matches").find(" of 10000 items")!=std::string::npos,"the count says how many are shown");
 find("");check(controller.inventoryPage().size()==10000,"clearing Find restores every item");
 {auto* select=static_cast<Rml::ElementFormControlSelect*>(doc->GetElementById("inventory_category"));check(select->GetNumOptions()==2&&select->GetOption(0)->GetInnerRML()=="(All)"&&select->GetOption(1)->GetInnerRML()=="Materials","Category lists (All) and each category");}
 doc->GetElementById("inventory_owned_only")->Click();tick();check(controller.state().inventoryOwnedOnly,"owned-only check box filters");doc->GetElementById("inventory_owned_only")->Click();tick();check(!controller.state().inventoryOwnedOnly,"and clears");
 for(float density:{1.f,1.25f,1.5f,2.f}){
  const int k=std::max(1,int(density+0.5f));c->SetDensityIndependentPixelRatio(density);c->SetDimensions({384*k,380*k});tick();
  {auto* frame=doc->GetElementById("inventory_scroll");check(frame->GetScrollHeight()<=frame->GetClientHeight()+1.f&&frame->GetScrollWidth()<=frame->GetClientWidth()+1.f,"the Inventory page fits the fixed window without scrolling");}
  rows->Focus();key(Qt::Key_End);auto selected=controller.state().selectedInventory;check(selected.has_value(),"last entry reachable");
  auto*selectedElement=c->GetFocusElement();check(selectedElement&&std::abs(selectedElement->GetOffsetHeight()-16.f*k)<1.f,"row height follows the snapped font");
  {auto* header=doc->GetElementById("inventory_sort_material");auto* cell=selectedElement->GetChild(2);check(std::abs(header->GetAbsoluteOffset(Rml::BoxArea::Border).x-cell->GetAbsoluteOffset(Rml::BoxArea::Border).x)<4.f*k,"Material heading aligns with its column");}
  auto sends=port.sent.size();key(Qt::Key_Space);check(port.sent.size()==sends+1&&port.sent.back().id.value=="watch.set"&&std::get<WatchPayload>(port.sent.back().payload).row==*selected,"Space watches the selected item");
  key(Qt::Key_Return);check(controller.state().inventoryDetail==selected,"Enter opens Item Properties for that item");
  check(doc->GetElementById("inventory_detail")->IsVisible(true)&&text("inventory_detail_title").find(" Properties")!=std::string::npos,"Item Properties is titled with the item's name");
  for(const char* tab:{"inventory_detail_tab_general","inventory_detail_tab_stockpiles","inventory_detail_tab_recipes","inventory_detail_tab_history"}){doc->GetElementById(tab)->Click();tick();auto* frame=doc->GetElementById("inventory_detail_frame");check(frame->GetScrollHeight()<=frame->GetClientHeight()+1.f,"each Item Properties page fits without scrolling");}
  doc->GetElementById("inventory_detail_tab_general")->Click();tick();
  sends=port.sent.size();doc->GetElementById("inventory_detail_watch")->Click();tick();check(port.sent.size()==sends+1&&port.sent.back().id.value=="watch.set","Watch this item acts at once");
  doc->GetElementById("inventory_detail_close_button")->Click();tick();check(!controller.state().inventoryDetail&&controller.state().selectedInventory==selected,"Close returns to the list with the item still selected");
 }
 c->SetDensityIndependentPixelRatio(1);c->SetDimensions({384,380});tick();
 {auto* first=rows->QuerySelector("button.w98-list-item");check(first!=nullptr,"rows rendered");auto* box=first->QuerySelector("input");auto sends=port.sent.size();box->Click();tick();check(port.sent.size()==sends+1&&port.sent.back().id.value=="watch.set","a row check box watches its item");}
 controller.selectInventory(data[99].id);controller.openInventoryDetail(data[99].id);tick();auto changed=data;changed[99].total=123;controller.applyInventory({WorldEpoch{8},Revision{2},changed});tick();check(text("inventory_detail_total")=="123","Item Properties follows the game's counts");
 changed.erase(changed.begin()+99);controller.applyInventory({WorldEpoch{8},Revision{3},changed});tick();check(!controller.state().inventoryDetail&&!controller.state().selectedInventory,"a removed item cannot stay selected");check(!doc->GetElementById("inventory_detail")->IsVisible(true),"its Item Properties closes");
 find("no-such-item");check(controller.inventoryPage().empty()&&text("inventory_matches").rfind("0 of ",0)==0,"filtered empty count");find("");
 controller.applyInventory({WorldEpoch{8},Revision{4},{data[0]}});tick();check(controller.inventoryPage().size()==1,"single item");controller.applyInventory({WorldEpoch{8},Revision{5},{}});tick();check(controller.inventoryPage().empty(),"empty snapshot");

 mb::InventoryRow alpha;alpha.id={CatalogId{"Materials"},CatalogId{"Raw"},CatalogId{"Alpha"},{},InventoryDepth::Item};alpha.name="Alpha";alpha.locations={{7,"Supply",3}};alpha.madeBy={{"Recipe","Beta","Beta","Kitchen","Cooking",1,{{"Beta","Beta","","",2}}}};alpha.usedIn=alpha.madeBy;
 auto beta=alpha;beta.id.item=CatalogId{"Beta"};beta.name="Beta";
 controller.applyInventory({WorldEpoch{8},Revision{6},{alpha,beta}});controller.selectInventory(alpha.id);controller.openInventoryDetail(alpha.id);tick();unsigned location=0;binding.setStockpileOpenHandler([&](unsigned id){location=id;});
 doc->GetElementById("inventory_detail_tab_stockpiles")->Click();tick();check(doc->GetElementById("inventory_detail_open_stockpile")->HasAttribute("disabled"),"Properties waits for a stockpile to be selected");
 doc->GetElementById("inventory_stockpile_7")->Click();tick();doc->GetElementById("inventory_detail_open_stockpile")->Click();tick();check(location==7&&controller.state().inventoryDetail==alpha.id,"Properties opens the selected stockpile and keeps Item Properties open");
 doc->GetElementById("inventory_detail_tab_recipes")->Click();tick();check(text("inventory_detail_made_by").find("Kitchen: 1 from 2 Beta")!=std::string::npos,"Made by states workshop, amount and ingredients");
 doc->GetElementById("inventory_detail_used_in")->QuerySelector("button")->Click();tick();doc->GetElementById("inventory_detail_open_product")->Click();tick();check(controller.state().inventoryDetail==beta.id,"Properties on a product opens that item");
 doc->GetElementById("inventory_detail_tab_history")->Click();tick();controller.applyInventoryHistory(WorldEpoch{8},beta.id,{{0,10,3,1}});tick();check(text("inventory_detail_history").find("Day 1")!=std::string::npos&&text("inventory_detail_history").find(">10<")!=std::string::npos,"History lists the day's counts");
 input.setContext(nullptr);controller.removeViewPort(binding);binding.shutdown();tick();
 }
 Rml::RemoveContext("stage08");Rml::Shutdown();std::cout<<"Stage08 PASS checks="<<checks<<"\n";
}
