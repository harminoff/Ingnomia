/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stockpile property sheet (Stage 09 behaviour on the Stage 21a Windows 98 sheet): fixed pages, OK / Cancel / Apply,
// every setting and allow-list check box pending until Apply, drafts kept per stockpile, templates acting at once.
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include "gui/ui/runtime/NumericEditor.h"
#include "gui/ui/screens/management6a/Management6ARmlBinding.h"
using namespace ingnomia::ui;
int checks = 0;
void check(bool value, const char* label) { ++checks; if(std::getenv("STAGE_TRACE")) std::cerr << "ok? " << label << "\n"; if(!value) { std::cerr << "FAIL " << label << "\n"; std::exit(1); } }
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
void update(Rml::Context& c) { c.Update(); if(connected_tabs::reconcile(c)) c.Update(); c.Update(); }
struct ManagerPort : management6a::CommandPort {
 std::vector<UiActionEnvelope> sent;bool reject=false;
 management6a::CommandResult dispatch(const UiActionEnvelope& a,management6a::DispatchOrigin) override {sent.push_back(a);return reject?management6a::CommandResult{management6a::CommandStatus::Rejected,false,"Rejected"}:management6a::CommandResult{};}
 std::size_t count(const char* id) const { return std::count_if(sent.begin(),sent.end(),[&](const auto& a){return a.id.value==id;}); }
};
struct ManagerView : management6a::ViewPort {void stateChanged(const management6a::Management6AState&) override {}};
struct ClipboardSystem : Rml::SystemInterface {Rml::String clipboard;void GetClipboardText(Rml::String& text) override {text=clipboard;}};

using namespace management6a;
int main(int argc,char**argv) {
 check(argc==2,"assets");Files files;files.root=argv[1];Renderer renderer;ClipboardSystem system;
 Rml::SetFileInterface(&files);Rml::SetRenderInterface(&renderer);Rml::SetSystemInterface(&system);check(Rml::Initialise(),"initialize");ClassicFocusInstancer focus;Rml::Factory::RegisterDecoratorInstancer("win98-focus",&focus);
 Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Regular.ttf");Rml::LoadFontFace("fonts/MSW98UI-Bold.ttf");
 auto*c=Rml::CreateContext("stage09",{384,380});
 {
 ManagerPort port;ManagerView view;Management6AController controller(port,view);Management6ARmlBinding binding(*c);check(binding.initialize(controller),"binding loads");controller.addViewPort(binding);controller.beginWorld(WorldEpoch{9});
 StockpileSnapshot sp;sp.id={9};sp.name="Supplies";sp.priority=1;sp.maxPriority=4;sp.itemCount=3;sp.reserved=1;sp.templateNames={"Food"};
 sp.filters.push_back({{sp.id,CatalogId{"Raw"},{},{},{},FilterDepth::Category},"Raw material",TriState::Mixed});
 sp.filters.push_back({{sp.id,CatalogId{"Raw"},CatalogId{"Wood"},{},{},FilterDepth::Group},"Wood",TriState::Mixed});
 for(int i=0;i<2000;++i) {auto n=std::to_string(i);StockpileFilterRowId id{sp.id,CatalogId{"Raw"},CatalogId{"Wood"},CatalogId{"Item"+n},CatalogId{"Oak"},FilterDepth::Material};auto parent=id;parent.material={};parent.depth=FilterDepth::Item;sp.filters.push_back({parent,"Item"+n,i<3?TriState::On:TriState::Off});sp.filters.push_back({id,"Oak",i<3?TriState::On:TriState::Off});}
 sp.contents.push_back({{CatalogId{"Raw"},CatalogId{"Wood"},CatalogId{"Item0"},CatalogId{"Oak"},FilterDepth::Material},"Oak",3,90,{}});
 controller.showStockpile(sp,Revision{1});update(*c);auto*doc=binding.stockpileDocument();
 auto activate=[&](const char* id){check(binding.activateElement(id),id);update(*c);};
 auto field=[&](const char*id,const char*value){check(binding.setFormValueForProbe(id,value),id);update(*c);};
 auto boxChecked=[&](const StockpileFilterRowId& id){auto* e=doc->GetElementById(("m6a_filter_"+std::string()).c_str());(void)e;for(const auto& r:controller.state().stockpile.visibleFilters)if(r.id==id)return controller.stockpileRuleAllowed(r);return false;};
 auto text=[&](const char* id){auto* e=doc->GetElementById(id);return e?std::string(e->GetInnerRML()):std::string();};
 auto disabled=[&](const char* id){auto* e=doc->GetElementById(id);return e && e->HasAttribute("disabled");};

 // ---- window and pages
 check(doc->GetElementById("stockpile_workbench")->IsClassSet("w98-sheet"),"Stockpile is a Windows 98 property sheet");
 check(text("stockpile_title")=="Supplies Properties","caption is the object name plus Properties");
 check(text("stockpile_view_contents")=="Contents" && text("stockpile_view_allow")=="Allow List" && text("stockpile_view_settings")=="General","tabs use book-title caps");
 check(doc->GetElementById("stockpile_ok") && doc->GetElementById("stockpile_cancel") && doc->GetElementById("stockpile_apply"),"OK, Cancel and Apply sit outside the pages");
 check(text("stockpile_content_status").find("1 item.")==0,"Contents counts the stored items");
 check(doc->GetElementById("stockpile_rows")->GetNumChildren()==1,"one stored row");

 // ---- General: every setting is pending until Apply
 activate("stockpile_view_settings");field("stockpile_name","Draft");field("stockpile_priority","3");activate("stockpile_toggle_pull");
 check(port.sent.empty(),"name, priority and check boxes send nothing");
 check(controller.state().stockpile.draft.options.pull && controller.state().stockpile.draft.dirty,"check box stages a pending change");
 check(!disabled("stockpile_apply"),"Apply is available once the sheet has changes");
 activate("stockpile_view_allow");activate("stockpile_view_settings");check(controller.state().stockpile.draft.name=="Draft","draft survives tabs");
 auto other=sp;other.id={10};other.name="Other";controller.showStockpile(other,Revision{2});update(*c);
 check(static_cast<Rml::ElementFormControlInput*>(doc->GetElementById("stockpile_name"))->GetValue()=="Other","another stockpile shows its own values");
 controller.showStockpile(sp,Revision{3});update(*c);check(controller.state().stockpile.draft.name=="Draft","draft survives object switch");
 controller.close();controller.showStockpile(sp,Revision{4});update(*c);check(controller.state().stockpile.draft.name=="Draft","draft survives close and reopen");
 port.reject=true;activate("stockpile_apply");check(controller.state().stockpile.draft.dirty&&!controller.state().stockpile.draft.pending,"rejected Apply keeps the draft");port.reject=false;activate("stockpile_review_cancel");
 auto n=port.sent.size();activate("stockpile_apply");check(port.sent.size()==n+1 && controller.state().stockpile.draft.pending,"Apply sends one basics command and waits");
 {auto basics=std::get<SetStockpileBasicsPayload>(port.sent.back().payload);check(basics.name=="Draft" && basics.priority==2 && basics.pull && !basics.allowPull,"Apply carries name, priority and options together");}
 sp.name="Draft";sp.priority=2;sp.pullFromOthers=true;controller.showStockpile(sp,Revision{5});update(*c);
 check(!controller.state().stockpile.draft.dirty && disabled("stockpile_apply"),"matching snapshot commits the draft");
 check(text("stockpile_title")=="Draft Properties","caption follows the applied name");
 field("stockpile_priority","999999");n=port.sent.size();activate("stockpile_apply");check(port.sent.size()==n,"invalid priority cannot apply");check(!binding.canClose(),"the problem is reported in a message box");activate("stockpile_review_cancel");
 activate("stockpile_cancel");check(port.sent.back().id.value=="nav.close" && !controller.state().stockpile.draft.dirty,"Cancel discards pending changes and closes");

 // ---- Allow List: check boxes are pending too
 controller.showStockpile(sp,Revision{6});controller.setStockpilePane(StockpilePane::AllowList);update(*c);
 check(doc->GetElementById("stockpile_filters")->GetNumChildren()<120,"allow list is virtualized");
 check(text("stockpile_rule_scope")=="2000 items shown","the shown rows are counted");
 const StockpileFilterRowId first{sp.id,CatalogId{"Raw"},CatalogId{"Wood"},CatalogId{"Item0"},CatalogId{"Oak"},FilterDepth::Material};
 const StockpileFilterRowId fourth{sp.id,CatalogId{"Raw"},CatalogId{"Wood"},CatalogId{"Item10"},CatalogId{"Oak"},FilterDepth::Material};
 {auto* box=doc->GetElementById("m6a_filter_526177_576f6f64_4974656d3130_4f616b_3_check");check(box!=nullptr,"row check box rendered");check(!box->HasAttribute("checked"),"blocked rule shows an empty check box");
  n=port.sent.size();box->Click();update(*c);check(port.sent.size()==n,"a check box sends nothing");check(boxChecked(fourth) && controller.state().stockpile.draft.rules.size()==1,"a check box stages the rule");
  box=doc->GetElementById("m6a_filter_526177_576f6f64_4974656d3130_4f616b_3_check");check(box && box->HasAttribute("checked"),"the check box shows the pending state");}
 check(disabled("stockpile_template_load") && disabled("stockpile_template_save") && text("stockpile_template_note").find("Apply your changes")==0,"templates wait until rule changes are applied");
 controller.selectStockpileFilter(first);update(*c);doc->GetElementById("m6a_filter_526177_576f6f64_4974656d30_4f616b_3")->Focus();
 {Rml::Dictionary key;key["key_identifier"]=int(Rml::Input::KI_SPACE);doc->GetElementById("m6a_filter_526177_576f6f64_4974656d30_4f616b_3")->DispatchEvent("keydown",key);update(*c);}
 check(!boxChecked(first) && controller.state().stockpile.draft.rules.size()==2,"Space toggles the selected rule");
 {Rml::Dictionary key;key["key_identifier"]=int(Rml::Input::KI_DOWN);doc->GetElementById("m6a_filter_526177_576f6f64_4974656d30_4f616b_3")->DispatchEvent("keydown",key);update(*c);}
 check(controller.state().stockpile.selectedFilter && controller.state().stockpile.selectedFilter->item.value=="Item1","Down moves the selection");
 n=port.sent.size();activate("stockpile_apply");check(port.sent.size()==n+2,"Apply sends the allowed and the blocked rules as two batches");
 {auto a=std::get<SetStockpileFiltersPayload>(port.sent[n].payload),b=std::get<SetStockpileFiltersPayload>(port.sent[n+1].payload);check(a.active && a.rows.size()==1 && a.rows[0]==fourth && !b.active && b.rows.size()==1 && b.rows[0]==first,"each batch names exactly its rules");}
 for(auto& r:sp.filters){if(r.id==fourth)r.state=TriState::On;if(r.id==first)r.state=TriState::Off;}
 controller.showStockpile(sp,Revision{7});update(*c);check(!controller.state().stockpile.draft.dirty && controller.state().stockpile.draft.rules.empty(),"snapshot with the rules commits them");
 field("stockpile_allow_search","Item19");check(text("stockpile_rule_scope")=="111 items shown","Find narrows the list");
 n=port.sent.size();activate("stockpile_allow_bulk");check(port.sent.size()==n && controller.state().stockpile.draft.rules.size()==111,"Allow All stages every shown rule and nothing else");
 activate("stockpile_block_bulk");check(controller.state().stockpile.draft.rules.empty(),"Block All returns them to the game's state");
 activate("stockpile_allow_bulk");activate("stockpile_cancel");check(controller.state().stockpile.draft.rules.empty() && port.sent.size()==n+1 && port.sent.back().id.value=="nav.close","Cancel discards staged rules");
 controller.showStockpile(sp,Revision{8});controller.setStockpilePane(StockpilePane::AllowList);update(*c);field("stockpile_allow_search","");

 // ---- templates act at once, with a message box before anything is replaced
 field("stockpile_template_name","Food");check(!disabled("stockpile_template_load") && !disabled("stockpile_template_save"),"templates are available with no pending rules");
 n=port.sent.size();activate("stockpile_template_save");check(!binding.canClose() && port.sent.size()==n,"saving over a template asks first");activate("stockpile_review_cancel");check(port.sent.size()==n,"No keeps the saved template");
 activate("stockpile_template_save");activate("stockpile_review_accept");check(port.sent.back().id.value=="stockpile.save_template" && std::get<StockpileTemplatePayload>(port.sent.back().payload).replaceExisting,"Yes replaces the saved template");
 field("stockpile_template_name","New template");activate("stockpile_template_save");check(port.sent.back().id.value=="stockpile.save_template" && std::get<StockpileTemplatePayload>(port.sent.back().payload).name=="New template","a new name saves at once");
 field("stockpile_template_name","Food");n=port.sent.size();activate("stockpile_template_load");check(port.sent.size()==n,"Load asks first");activate("stockpile_review_accept");check(port.sent.back().id.value=="stockpile.apply_template","Yes loads the template");
 activate("stockpile_template_toggle");check(controller.state().stockpile.templateMenuOpen && doc->GetElementById("stockpile_template_options")->IsVisible(true),"the arrow drops the saved names");
 doc->GetElementById("stockpile_template_options")->QuerySelector("button")->Click();update(*c);check(!controller.state().stockpile.templateMenuOpen && controller.state().stockpile.templateName=="Food","choosing a name fills the text box");

 // ---- Close with pending changes asks Yes / No / Cancel
 controller.setStockpilePane(StockpilePane::Settings);update(*c);field("stockpile_name","Unsaved");n=port.sent.size();
 check(!binding.canClose() && port.sent.size()==n,"Close with pending changes asks instead of closing");activate("stockpile_review_alternate");
 check(port.sent.back().id.value=="nav.close" && !controller.state().stockpile.draft.dirty,"No discards the changes and closes");

 // ---- every page fits the fixed sheet at every scale
 controller.showStockpile(sp,Revision{9});update(*c);
 for(float scale:{1.f,1.25f,1.5f,2.f}){c->SetDensityIndependentPixelRatio(scale);const int k=std::max(1,int(scale+0.5f));c->SetDimensions({384*k,380*k});
  for(auto pane:{StockpilePane::Contents,StockpilePane::AllowList,StockpilePane::Settings}){controller.setStockpilePane(pane);update(*c);auto* frame=doc->GetElementById("stockpile_scroll");
   check(doc->GetElementById("stockpile_tabs")->GetOffsetWidth()<=384.f*k,"tabs fit the property sheet at scale");
   check(frame->GetScrollHeight()<=frame->GetClientHeight()+1.f && frame->GetScrollWidth()<=frame->GetClientWidth()+1.f,"page fits the fixed property sheet without scrolling");}
  controller.setStockpilePane(StockpilePane::AllowList);update(*c);auto* list=doc->GetElementById("stockpile_filters");list->SetScrollTop(999999);update(*c);check(list->GetClientHeight()>20,"allow list keeps a usable viewport");
  {const auto& last=controller.state().stockpile.visibleFilters.back();const auto hex=[](const std::string& v){static const char* d="0123456789abcdef";std::string o;for(unsigned char ch:v){o+=d[ch>>4];o+=d[ch&15];}return o;};
   check(doc->GetElementById(("m6a_filter_"+hex(last.id.category.value)+"_"+hex(last.id.group.value)+"_"+hex(last.id.item.value)+"_"+hex(last.id.material.value)+"_3").c_str())!=nullptr,"the last rule renders when scrolled to the end");}list->SetScrollTop(0);update(*c);}
 c->SetDensityIndependentPixelRatio(1);c->SetDimensions({384,380});update(*c);
 controller.setStockpilePane(StockpilePane::AllowList);update(*c);{auto* field=doc->GetElementById("stockpile_template_name");check(field->GetOffsetHeight()<=field->GetParentNode()->GetOffsetHeight()-3.5f,"the Template text box sits inside its drop-down combo field");}

 // ---- conflicts and removal
 controller.editStockpileDraft("Conflict","3");sp.name="Changed externally";controller.showStockpile(sp,Revision{20});n=port.sent.size();
 check(!controller.applyStockpileDraft()&&port.sent.size()==n,"external name change cannot be overwritten silently");controller.revertStockpileDraft();controller.stockpileFeedback("");update(*c);
 controller.setStockpileTemplateName("   ");n=port.sent.size();controller.saveStockpileTemplate();check(port.sent.size()==n,"empty template name blocked");controller.stockpileFeedback("");
 controller.setStockpileTemplateName("Food");controller.updateStockpileTemplate();controller.showStockpile(sp,Revision{21});n=port.sent.size();controller.confirmStockpileTemplateOverwrite();check(port.sent.size()==n,"stale template revision rejects overwrite");
 controller.editStockpileDraft("Retain on rejection","3");controller.rejectStockpile(sp.id,"This stockpile no longer exists.");n=port.sent.size();check(!controller.applyStockpileDraft()&&port.sent.size()==n,"removed target cannot dispatch draft");check(controller.state().stockpile.draft.dirty,"removed target retains draft until world ends");
 controller.endWorld();controller.beginWorld(WorldEpoch{10});controller.showStockpile(sp,Revision{1});check(!controller.state().stockpile.draft.dirty,"new world clears retained drafts");
 controller.removeViewPort(binding);binding.shutdown();
 }
 Rml::RemoveContext("stage09");Rml::Shutdown();std::cout<<checks<<" Stage09 checks passed\n";
}
