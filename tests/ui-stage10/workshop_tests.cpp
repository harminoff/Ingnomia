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


using namespace management6a;

int main(int argc,char**argv) {
 check(argc==2,"assets");Files files;files.root=argv[1];Renderer renderer;ClipboardSystem system;
 Rml::SetFileInterface(&files);Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);check(Rml::Initialise(),"initialize");ClassicFocusInstancer focus;Rml::Factory::RegisterDecoratorInstancer("win98-focus",&focus);Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Bold.ttf");
 auto*c=Rml::CreateContext("stage10",{720,720});
 {
 ManagerPort port;ManagerView view;Management6AController controller(port,view);Management6ARmlBinding binding(*c);check(binding.initialize(controller),"binding loads");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{10});
 WorkshopSnapshot ws;ws.id={10};ws.name="Carpenter";ws.subtype="Carpenter";ws.maxPriority=4;ws.canLinkStockpile=true;ws.stockpiles={{{8},"Wood",false},{{9},"Stone",true}};
 WorkshopProductRow product;product.id=CatalogId{"Plank"};ws.products={product};CraftQueueRow job;job.id={101};job.craft=CatalogId{"Plank"};job.count=3;ws.queue={job};job.id={102};ws.queue.push_back(job);
 controller.showWorkshop(ws,Revision{1});update(*c);auto*doc=binding.workshopDocument();
 auto activate=[&](const char* id){check(binding.activateElement(id),id);update(*c);};
 auto field=[&](const char* id,const char* value){check(binding.setFormValueForProbe(id,value),id);update(*c);};
 check(doc->GetElementById("workshop_tabs")->IsClassSet("c-connected-tabs"),"connected property tabs");
 activate("workshop_view_settings");field("workshop_name","Draft carpenter");field("workshop_priority","2");auto n=port.sent.size();activate("workshop_priority_down");check(port.sent.size()==n,"priority stages only");check(controller.state().workshop.draft.priority=="3","arrow exact staged value");
 activate("workshop_toggle_generated");check(port.sent.size()==n && controller.state().workshop.draft.options.acceptGenerated && controller.state().workshop.draft.dirty,"check box stages a pending change");
 check(!doc->GetElementById("workshop_apply")->HasAttribute("disabled"),"Apply is available once the sheet has changes");
 activate("workshop_view_craft");activate("workshop_view_settings");check(controller.state().workshop.draft.name=="Draft carpenter","tab retains draft");
 activate("workshop_apply");check(port.sent.size()==n+1,"Apply sends one basics command");check(controller.state().workshop.draft.pending,"wait for authoritative basics");
 {auto basics=std::get<SetWorkshopBasicsPayload>(port.sent.back().payload);check(basics.name=="Draft carpenter" && basics.priority==2 && basics.acceptGenerated,"Apply carries name, priority and options together");}
 ws.name="Draft carpenter";ws.priority=2;ws.acceptGenerated=true;controller.showWorkshop(ws,Revision{2});check(!controller.state().workshop.draft.dirty,"snapshot accepts draft");update(*c);check(doc->GetElementById("workshop_apply")->HasAttribute("disabled"),"Apply is unavailable without changes");
 field("workshop_priority","9999");n=port.sent.size();activate("workshop_apply");check(port.sent.size()==n,"invalid priority does not mutate");check(!binding.canClose(),"validation message box owns input");activate("workshop_review_cancel");
 check(!binding.canClose() && port.sent.size()==n,"Close with pending changes asks instead of closing");activate("workshop_review_alternate");check(!port.sent.empty() && port.sent.back().id.value=="nav.close" && !controller.state().workshop.draft.dirty,"No discards the changes and closes");
 controller.showWorkshop(ws,Revision{3});update(*c);field("workshop_name","Cancelled");activate("workshop_cancel");check(port.sent.back().id.value=="nav.close" && controller.state().workshop.draft.name=="Draft carpenter","Cancel discards pending changes and closes");
 controller.showWorkshop(ws,Revision{4});update(*c);field("workshop_name","Retained");auto other=ws;other.id={11};other.name="Other";controller.showWorkshop(other,Revision{3});controller.showWorkshop(ws,Revision{5});check(controller.state().workshop.draft.name=="Retained","object switch retains draft by ID");
 controller.close();controller.showWorkshop(ws,Revision{6});update(*c);check(controller.state().workshop.draft.name=="Retained","close reopen retains draft");
 n=port.sent.size();activate("workshop_ok");check(port.sent.size()==n+1 && port.sent.back().id.value=="workshop.set_basics","OK applies the pending sheet");
 ws.name="Retained";controller.showWorkshop(ws,Revision{7});update(*c);check(port.sent.back().id.value=="nav.close","OK closes once the snapshot confirms the change");
 controller.showWorkshop(ws,Revision{8});update(*c);
 controller.selectWorkshopJob({102});controller.setWorkshopPane(WorkshopPane::Queue);update(*c);field("workshop_job_count","7");std::reverse(ws.queue.begin(),ws.queue.end());controller.showWorkshop(ws,Revision{9});activate("workshop_job_apply");check(std::get<SetCraftJobPayload>(port.sent.back().payload).job.value==102 && std::get<SetCraftJobPayload>(port.sent.back().payload).count==7,"reordered queue retains target and quantity draft");
 n=port.sent.size();activate("workshop_job_move_back");field("workshop_job_mode","maintain");check(port.sent.size()==n,"order options stay local until Update Order");
 activate("workshop_job_apply");{auto edit=std::get<SetCraftJobPayload>(port.sent.back().payload);check(port.sent.size()==n+1 && edit.job.value==102 && edit.moveBack && edit.mode==CraftRepeatMode::Maintain,"Update Order sends type, quantity and move-back for the selected job");}
 ws.queue.erase(ws.queue.begin());controller.showWorkshop(ws,Revision{10});check(!controller.state().workshop.selectedJob,"disappeared job clears selection");n=port.sent.size();controller.setSelectedJob(CraftRepeatMode::Once,5,false,false);check(port.sent.size()==n,"missing job cannot target replacement");
 controller.setWorkshopPane(WorkshopPane::Craft);update(*c);field("workshop_order_count","bad");n=port.sent.size();activate("workshop_queue_once");check(port.sent.size()==n,"invalid craft quantity blocked");field("workshop_order_count","2");activate("workshop_queue_once");activate("workshop_queue_once");check(port.sent.size()==n+1,"held duplicate creates one order");controller.onWorkshopOrderResult(ws.id,true);
 controller.setWorkshopPane(WorkshopPane::Craft);update(*c);doc->GetElementById("workshop_queue_once")->Focus();RmlUiQtInputAdapter input;input.setContext(c);n=port.sent.size();input.keyDown(Qt::Key_Space,Qt::NoModifier,false);controller.onWorkshopOrderResult(ws.id,true);update(*c);input.keyDown(Qt::Key_Space,Qt::NoModifier,true);input.keyUp(Qt::Key_Space,Qt::NoModifier,false);check(port.sent.size()==n+1,"held Space remains one order after acknowledgement");
 n=port.sent.size();controller.setButcherOptions(true,true);check(port.sent.size()==n,"unsupported special option rejected");
 ws.subtype="TradingPost";controller.showWorkshop(ws,Revision{11});controller.setWorkshopPane(WorkshopPane::Trade);
 TradeRow a{{TradeParty::Trader,CatalogId{"Plank"},CatalogId{"Oak"},2},"Oak plank",10,2,5};TradeRow b{{TradeParty::Player,CatalogId{"Stone"},CatalogId{"Granite"},2},"Granite",20,10,2};
 controller.setTradeSnapshot(ws.id,42,1,{a},{b},10,20);controller.selectTradeRow(a.id);update(*c);field("workshop_trade_count","4");activate("workshop_trade_set");auto offer=std::get<SetTradeOfferPayload>(port.sent.back().payload);check(offer.row==a.id && offer.count==4 && offer.traderId==42,"exact offer targets selected row and merchant");n=port.sent.size();controller.setTradeOffer(a.id,5);check(port.sent.size()==n,"pending exact offer cannot duplicate");
 a.offered=4;controller.setTradeSnapshot(ws.id,42,2,{a},{b},20,20);controller.executeTrade();check(controller.state().workshop.tradeConfirmationRequired,"review opens");check(!binding.canClose(),"review owns modal input");controller.cancelTrade();check(binding.canClose(),"cancel releases modal");check(port.sent.size()==n,"cancel exchanges nothing");
 controller.executeTrade();controller.setTradeSnapshot(ws.id,42,3,{a},{b},20,20);check(!controller.state().workshop.tradeConfirmationRequired,"changed snapshot invalidates review");controller.confirmTrade();check(port.sent.size()==n,"stale review cannot commit");controller.executeTrade();controller.confirmTrade();controller.confirmTrade();check(port.sent.size()==n+1,"one reviewed commit");auto commit=std::get<WorkshopTargetPayload>(port.sent.back().payload);check(commit.tradeRevision==3 && commit.traderId==42,"commit bound to revision and merchant");
 controller.rejectWorkshop(ws.id,"Merchant left");check(!controller.state().workshop.tradePending,"rejection clears pending");
 for(float scale:{1.f,1.25f,1.5f,2.f}){c->SetDensityIndependentPixelRatio(scale);const int k=std::max(1,int(scale+0.5f));c->SetDimensions({384*k,380*k});for(auto pane:{WorkshopPane::Craft,WorkshopPane::Queue,WorkshopPane::Settings,WorkshopPane::Trade}){controller.setWorkshopPane(pane);update(*c);auto* frame=doc->GetElementById("workshop_scroll");check(doc->GetElementById("workshop_tabs")->GetOffsetWidth()<=384.f*k,"tabs fit the property sheet at scale");check(frame->GetScrollHeight()<=frame->GetClientHeight()+1.f && frame->GetScrollWidth()<=frame->GetClientWidth()+1.f,"property page fits the fixed property sheet without scrolling");}}
 c->SetDensityIndependentPixelRatio(1.f);c->SetDimensions({720,720});
 // Special-GUI workshops expose only their supported pages and options.
 WorkshopSnapshot market;market.id={20};market.name="Market";market.subtype="Trader";market.maxPriority=4;
 n=port.sent.size();controller.showWorkshop(market,Revision{1});update(*c);
 check(controller.state().workshop.pane==WorkshopPane::Trade,"market opens on Trade");
 check(port.sent.size()==n+1 && port.sent.back().id.value=="trade.refresh","opening Trade loads the ledger once");
 check(!doc->GetElementById("workshop_view_craft")->IsVisible(true) && !doc->GetElementById("workshop_view_queue")->IsVisible(true),"market hides Craft and Queue tabs");
 controller.setWorkshopPane(WorkshopPane::Craft);check(controller.state().workshop.pane==WorkshopPane::Trade,"unsupported Craft page rejected");
 TradeRow m{{TradeParty::Trader,CatalogId{"Plank"},CatalogId{"Oak"},2},"Oak plank",10,0,5};TradeRow g{{TradeParty::Player,CatalogId{"Stone"},CatalogId{"Granite"},2},"Granite",20,0,2};
 controller.setTradeSnapshot(market.id,7,1,{m},{g},0,0);update(*c);
 check(doc->GetElementById("workshop_trade_execute")->HasAttribute("disabled"),"review disabled without offers");
 m.offered=2;controller.setTradeSnapshot(market.id,7,2,{m},{g},10,0);update(*c);
 check(doc->GetElementById("workshop_trade_execute")->HasAttribute("disabled") && doc->GetElementById("workshop_trade_blocked")->GetInnerRML().find("10 more value")!=Rml::String::npos,"unbalanced trade explains the shortfall at Review");
 g.offered=5;controller.setTradeSnapshot(market.id,7,3,{m},{g},10,10);update(*c);
 check(!doc->GetElementById("workshop_trade_execute")->HasAttribute("disabled"),"balanced trade can be reviewed");
 check(doc->GetElementById("workshop_trade_merchant")->GetInnerRML().find("Oak plank")!=Rml::String::npos && doc->GetElementById("workshop_trade_settlement")->GetInnerRML().find("Granite")!=Rml::String::npos,"merchant and settlement goods use separate lists");
 WorkshopSnapshot butcherWs;butcherWs.id={21};butcherWs.name="Butcher";butcherWs.subtype="Butcher";butcherWs.maxPriority=4;butcherWs.canLinkStockpile=true;butcherWs.stockpiles={{{8},"Wood",false}};butcherWs.butcherCorpses=true;
 n=port.sent.size();controller.showWorkshop(butcherWs,Revision{1});update(*c);
 check(controller.state().workshop.pane==WorkshopPane::Settings && port.sent.size()==n,"butcher opens on Settings without a trade request");
 check(!doc->GetElementById("workshop_production_section")->IsVisible(true) && !doc->GetElementById("workshop_view_stockpiles")->IsVisible(true),"craft-only options and the Stockpiles page are hidden on butcher");
 check(doc->GetElementById("workshop_butcher_actions")->IsVisible(true) && !doc->GetElementById("workshop_fisher_actions")->IsVisible(true) && !doc->GetElementById("workshop_view_trade")->IsVisible(true),"only butcher options shown");
 check(rmlui_dynamic_cast<Rml::ElementFormControlInput*>(doc->GetElementById("workshop_toggle_corpses"))->HasAttribute("checked") && !doc->GetElementById("workshop_toggle_excess")->HasAttribute("checked"),"butcher checkboxes match the workshop");
 ws.subtype="Carpenter";controller.showWorkshop(ws,Revision{20});controller.setWorkshopPane(WorkshopPane::Settings);update(*c);
 check(doc->GetElementById("workshop_view_stockpiles")->IsVisible(true),"crafting workshop shows the Stockpiles page");controller.setWorkshopPane(WorkshopPane::Stockpiles);update(*c);
 {auto* frame=doc->GetElementById("workshop_scroll");check(frame->GetScrollHeight()<=frame->GetClientHeight()+1.f,"Stockpiles page fits without scrolling");}
 check(doc->GetElementById("workshop_view_craft")->IsVisible(true) && !doc->GetElementById("workshop_production_section")->IsClassSet("is-hidden"),"crafting workshop keeps Craft and production options");
 doc->GetElementById("workshop_link_8")->Focus();n=port.sent.size();activate("workshop_link_8");check(port.sent.size()==n && std::ranges::count(controller.state().workshop.draft.options.linked,8u)==1,"checking a stockpile stages the link");activate("workshop_apply");
 check(port.sent.size()==n+1 && port.sent.back().id.value=="workshop.set_stockpile_link" && std::get<SetWorkshopStockpileLinkPayload>(port.sent.back().payload).stockpile.value==8 && std::get<SetWorkshopStockpileLinkPayload>(port.sent.back().payload).linked,"Link targets its own stockpile ID");
 ws.stockpiles[0].linked=true;controller.showWorkshop(ws,Revision{21});update(*c);
 check(c->GetFocusElement()&&c->GetFocusElement()->GetId()=="workshop_link_8" && c->GetFocusElement()->HasAttribute("checked"),"link status refreshes without losing keyboard focus");
 n=port.sent.size();controller.setWorkshopStockpileLink(StockpileId{77},true);check(port.sent.size()==n,"a stockpile missing from the snapshot cannot be linked");
 controller.endWorld();check(controller.state().workshop.draft.name.empty(),"world ends drafts");controller.removeViewPort(binding);binding.shutdown();
 }
 Rml::RemoveContext("stage10");Rml::Shutdown();std::cout<<checks<<" checks passed\n";
}
