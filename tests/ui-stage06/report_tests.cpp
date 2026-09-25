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
 auto*c=Rml::CreateContext("stage06",{720,720});c->SetDefaultScrollBehavior(Rml::ScrollBehavior::Instant,1.f);auto tick=[&]{c->Update();c->Update();};
 {
 PortB port;ViewB view;mb::Management6BController controller(port,view);mb::Management6BRmlBinding binding(*c);check(binding.initialize(controller),"binding loads");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{6});controller.open(mb::View::Inventory);
 std::vector<mb::InventoryRow> data;for(int i=0;i<10000;++i){mb::InventoryRow r;r.id={CatalogId{"Materials"},CatalogId{"Raw"},CatalogId{"Item"+std::to_string(i)},CatalogId{"Oak"},InventoryDepth::Material};r.name="Item "+std::to_string(i);r.stockpiled=i%7;r.total=i%19;data.push_back(r);}
 auto start=std::chrono::steady_clock::now();controller.applyInventory({WorldEpoch{6},Revision{1},data});tick();c->Render();auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto*doc=binding.inventoryDocument();auto*rows=doc->GetElementById("inventory_rows");std::cout<<"PERF initial_ms="<<ms<<" elements="<<countElements(doc)<<" row_children="<<rows->GetNumChildren()<<"\n";
 start=std::chrono::steady_clock::now();for(int i=0;i<20;++i){rows->SetScrollTop(float(i*470));tick();c->Render();}std::cout<<"PERF scroll20_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" elements="<<countElements(doc)<<"\n";
 check(rows->GetNumChildren()<120,"10000 records remain virtualized");

 RmlUiQtInputAdapter input(c);auto key=[&](int k){input.keyDown(k,{});input.keyUp(k,{});tick();};
 controller.selectInventory(data[500].id);tick();auto selected=controller.state().selectedInventory;controller.setInventorySort(mb::Sort::Total);tick();check(controller.state().selectedInventory==selected,"sort retains stable ID");controller.toggleSelectedWatch();check(std::get<WatchPayload>(port.sent.back().payload).row==*selected,"sorted action retains target");
 controller.applyInventoryPatch({WorldEpoch{6},Revision{1},Revision{2},data[100]});tick();check(controller.state().selectedInventory==selected,"unrelated patch preserves selection");
 auto remaining=data;std::erase_if(remaining,[&](const auto&r){return r.id==*selected;});controller.applyInventory({WorldEpoch{6},Revision{3},remaining});tick();check(!controller.state().selectedInventory,"deleted selection clears without retargeting");auto commands=port.sent.size();controller.toggleSelectedWatch();check(commands==port.sent.size(),"deleted target cannot dispatch watch");
 controller.setInventoryFilter("Item 99");tick();check(controller.inventoryPage().size()>0&&controller.inventoryPage().size()<10000,"Find changes actual records");check(static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("inventory_search"))->GetValue()=="Item 99","Find shows the text that filters the list");controller.setInventoryFilter("");tick();
  controller.setInventoryColumnFilter(2,"no such item");tick();check(controller.inventoryPage().empty()&&!controller.state().selectedInventory,"empty filter cannot leave an actionable hidden target");controller.setInventoryColumnFilter(2,"");tick();
 // Density changes must keep all records reachable, including the end of the virtual list.
 for(float density:{1.f,1.25f,1.5f,2.f}){c->SetDensityIndependentPixelRatio(density);tick();rows->Focus();key(Qt::Key_End);check(controller.state().selectedInventory.has_value(),"End selects final stable row");auto*f=c->GetFocusElement();check(f&&f->HasAttribute("data-item"),"scaled virtual end materializes focused row");check(rows->GetNumChildren()<120,"scaled list remains bounded");check(rows->GetScrollWidth()<=rows->GetClientWidth()+1.f,"the columns fit the list without horizontal scrolling"); }
 auto watched=controller.state().selectedInventory;commands=port.sent.size();input.keyDown(Qt::Key_Space,{});input.keyDown(Qt::Key_Space,{},true);input.keyUp(Qt::Key_Space,{});tick();std::cout<<"WATCH delta="<<port.sent.size()-commands<<" action="<<port.sent.back().id.value<<"\n";check(port.sent.size()==commands+1&&port.sent.back().id.value=="watch.set","held row Space emits Watch once");check(std::get<WatchPayload>(port.sent.back().payload).row==*watched&&!controller.state().inventoryDetail,"row Space retains target without opening detail");
 c->SetDensityIndependentPixelRatio(1);c->SetDimensions({720,720});tick();
 controller.closeInventory();controller.open(mb::View::Schedules);mb::ScheduleRow first;first.creature={51};first.name="Ada";mb::ScheduleRow second;second.creature={52};second.name="Ben";controller.applySchedules({WorldEpoch{6},Revision{1},{first,second}});controller.selectScheduleCell({first.creature,2});tick();auto*pop=binding.populationDocument();pop->GetElementById("schedule_51_2")->Focus();commands=port.sent.size();key(Qt::Key_Right);check(controller.state().selectedScheduleCell->creature==first.creature&&controller.state().selectedScheduleCell->hour==3,"matrix horizontal focus uses stable row and hour");key(Qt::Key_Down);check(controller.state().selectedScheduleCell->creature==second.creature,"matrix vertical focus keeps row identity");check(port.sent.size()==commands,"matrix navigation never mutates values");input.keyDown(Qt::Key_Space,{});input.keyDown(Qt::Key_Space,{},true);input.keyUp(Qt::Key_Space,{});tick();check(port.sent.size()==commands+1,"held matrix Space activates once");controller.applySchedules({WorldEpoch{6},Revision{2},{second,first}});check(controller.state().selectedScheduleCell->creature==second.creature,"matrix reorder retains creature identity");controller.applySchedules({WorldEpoch{6},Revision{3},{first}});check(!controller.state().selectedScheduleCell,"matrix deletion clears focus target");
 input.setContext(nullptr);

 controller.removeViewPort(binding);binding.shutdown();tick();
 }
 {
 auto*doc=c->LoadDocumentFromMemory("<rml><head><link type='text/rcss' href='/styles/base.rcss'/><link type='text/rcss' href='/styles/components.rcss'/></head><body><div id='scroll' tab-index='0' style='width:240dp;height:160dp;overflow:auto;'><div style='width:700dp;height:1800dp;'><div id='inner' style='width:120dp;height:80dp;overflow:auto;'><div style='height:400dp;'>Nested</div></div></div></div><div id='tiny' style='width:100dp;height:100dp;overflow:auto;'><div style='height:101dp;'>Tiny</div></div><div id='none' style='width:100dp;height:100dp;overflow:auto;'>No range</div></body></rml>");doc->Show();tick();auto*scroll=doc->GetElementById("scroll");auto*v=part(scroll,"scrollbarvertical");auto*h=part(scroll,"scrollbarhorizontal");check(v&&h&&part(scroll,"scrollbarcorner"),"both generated axes and corner exist");
 auto click=[&](Rml::Element*e){auto pos=e->GetAbsoluteOffset(Rml::BoxArea::Border);c->ProcessMouseMove(int(pos.x+e->GetOffsetWidth()/2),int(pos.y+e->GetOffsetHeight()/2),0);c->ProcessMouseButtonDown(0,0);c->ProcessMouseButtonUp(0,0);tick();};
 std::cout<<"SCROLL range="<<scroll->GetScrollHeight()<<"/"<<scroll->GetClientHeight()<<" arrow="<<part(v,"sliderarrowinc")->GetOffsetWidth()<<"x"<<part(v,"sliderarrowinc")->GetOffsetHeight()<<"\n";click(part(v,"sliderarrowinc"));check(scroll->GetScrollTop()>0,"vertical generated arrow scrolls");click(part(h,"sliderarrowinc"));check(scroll->GetScrollLeft()>0,"horizontal generated arrow scrolls");scroll->SetScrollTop(0);tick();click(part(v,"slidertrack"));check(scroll->GetScrollTop()>30,"track press pages");
 scroll->SetScrollTop(0);tick();auto*bar=part(v,"sliderbar");auto pos=bar->GetAbsoluteOffset(Rml::BoxArea::Border);c->ProcessMouseMove(int(pos.x+bar->GetOffsetWidth()/2),int(pos.y+3),0);c->ProcessMouseButtonDown(0,0);c->ProcessMouseMove(int(pos.x+bar->GetOffsetWidth()/2),int(pos.y+70),0);c->ProcessMouseButtonUp(0,0);tick();check(scroll->GetScrollTop()>0,"generated thumb drag scrolls");
 scroll->SetScrollTop(0);scroll->SetScrollLeft(0);tick();auto*inner=doc->GetElementById("inner");pos=inner->GetAbsoluteOffset(Rml::BoxArea::Border);c->ProcessMouseMove(int(pos.x+20),int(pos.y+20),0);c->ProcessMouseWheel(1.f,0);tick();check(inner->GetScrollTop()>0&&scroll->GetScrollTop()==0,"nested wheel goes to inner range first");
 auto*tiny=doc->GetElementById("tiny");tiny->SetScrollTop(999);tick();check(tiny->GetScrollTop()>0&&tiny->GetScrollTop()<25,"tiny range clamps");auto*none=doc->GetElementById("none");none->SetScrollTop(999);check(none->GetScrollTop()==0,"no-range cannot scroll");
 doc->Close();tick();
 }
 {
 c->SetDimensions({720,720});ManagerPort port;ManagerView view;management6a::Management6AController controller(port,view);management6a::Management6ARmlBinding binding(*c);check(binding.initialize(controller),"stockpile loads");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{7});management6a::StockpileSnapshot snapshot;snapshot.id={7};snapshot.name="Report Stockpile";snapshot.maxPriority=3;
 for(int i=0;i<2000;++i){management6a::StockpileFilterRow row;row.id={StockpileId{7},CatalogId{"Materials"},CatalogId{"Raw"},CatalogId{"Item"+std::to_string(i)},CatalogId{"Oak"},FilterDepth::Material};row.label="Item "+std::to_string(i);snapshot.filters.push_back(row);}
 controller.showStockpile(snapshot,Revision{1},WorldPosition{1,2,3});controller.setStockpilePane(management6a::StockpilePane::AllowList);tick();auto*doc=binding.stockpileDocument();auto*list=doc->GetElementById("stockpile_filters");check(list->GetNumChildren()<65,"Stockpile 2000 rules remain virtualized");auto*firstRule=list->GetFirstChild();const auto headerX=doc->GetElementById("stockpile_allow_sort_material")->GetAbsoluteOffset(Rml::BoxArea::Border).x,cellX=firstRule->GetChild(2)->GetAbsoluteOffset(Rml::BoxArea::Border).x;std::cout<<"ALIGN stockpile header="<<headerX<<" row="<<cellX<<"\n";check(std::abs(headerX-cellX)<4.f,"Stockpile Material heading aligns with its column in the virtual body");RmlUiQtInputAdapter input(c);auto key=[&](int k){input.keyDown(k,{});input.keyUp(k,{});tick();};
 check(binding.setFormValueForProbe("stockpile_allow_search","Item 19"),"Stockpile Find");tick();check(controller.state().stockpile.visibleFilters.size()==111&&list->GetNumChildren()<=112,"Stockpile Find narrows the virtual list");check(binding.setFormValueForProbe("stockpile_allow_search",""),"Stockpile Find clears");tick();check(controller.state().stockpile.visibleFilters.size()==2000,"clearing Find restores every rule");
 for(float scale:{1.f,2.f}){c->SetDensityIndependentPixelRatio(scale);tick();list->SetScrollTop(1000000);tick();check(list->GetNumChildren()<65,"Stockpile end stays bounded at density");check(list->GetScrollTop()>0,"Stockpile scaled last rules reachable");}
 c->SetDensityIndependentPixelRatio(1);input.setContext(nullptr);controller.removeViewPort(binding);binding.shutdown();tick();
 }
 Rml::RemoveContext("stage06");Rml::Shutdown();std::cout<<"Stage06 PASS checks="<<checks<<"\n";
}

