#include "gui/ui/screens/management6a/Management6AController.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
using namespace ingnomia::ui;using namespace ingnomia::ui::management6a;
namespace
{
void check(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
CatalogId cid(const char*v){return CatalogId{v};}
struct Port:CommandPort{std::vector<UiActionEnvelope>actions;std::vector<DispatchOrigin>origins;CommandResult next{};CommandResult dispatch(const UiActionEnvelope&a,DispatchOrigin origin)override{if(a.id.value=="trade.execute"&&origin!=DispatchOrigin::DestructiveConfirmation)return{CommandStatus::Rejected,false,"ui.error.confirmation_required"};actions.push_back(a);origins.push_back(origin);return next;}};
struct View:ViewPort{Management6AState state;int changes{};void stateChanged(const Management6AState&s)override{state=s;++changes;}};
struct SinkView:ViewPort{void stateChanged(const Management6AState&)override{}};
WorkshopComponentRow component(const char*item,std::uint32_t amount,const char*material,std::uint32_t available){WorkshopComponentRow out;out.item=cid(item);out.amount=amount;out.requireSameMaterial=true;out.materials.push_back({cid(material),available});return out;}
CraftQueueRow job(std::uint32_t id,const char*craft,CraftRepeatMode mode,std::uint32_t count){CraftQueueRow out;out.id={id};out.craft=cid(craft);out.item=cid(craft);out.mode=mode;out.count=count;out.materials.push_back(cid("iron"));return out;}
}
void filterPreorderTest(){Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{62});StockpileSnapshot sp;sp.id={20};StockpileFilterRowId category{sp.id,cid("raw"),{},{},{},FilterDepth::Category};StockpileFilterRowId group{sp.id,cid("raw"),cid("stone"),{},{},FilterDepth::Group};StockpileFilterRowId item{sp.id,cid("raw"),cid("stone"),cid("block"),{},FilterDepth::Item};StockpileFilterRowId material{sp.id,cid("raw"),cid("stone"),cid("block"),cid("granite"),FilterDepth::Material};sp.filters={{category,"Z Category",TriState::Mixed},{group,"A Group",TriState::Mixed},{item,"B Item",TriState::Mixed},{material,"C Material",TriState::On}};c.showStockpile(StockpileSnapshot{sp},Revision{1},WorldPosition{4,5,6});check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters[0].label=="C Material","stockpile allow list projects one deepest row");c.setStockpileFilterCategory(cid("raw"));check(c.state().stockpile.filterCategory==cid("raw")&&c.state().stockpile.visibleFilters.size()==1,"stockpile category filters flat rows");c.toggleStockpileFilterExpansion(group);check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters[0].id==material,"legacy expansion state does not alter flat rows");sp.filters[3].state=TriState::Off;c.showStockpile(std::move(sp),Revision{2},WorldPosition{4,5,6});check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters[0].state==TriState::Off,"flat row keeps authoritative permission state");}
void filterSearchAncestorTest(){Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{66});StockpileSnapshot sp;sp.id={24};StockpileFilterRowId category{sp.id,cid("food"),{},{},{},FilterDepth::Category};StockpileFilterRowId group{sp.id,cid("food"),cid("raw"),{},{},FilterDepth::Group};StockpileFilterRowId item{sp.id,cid("food"),cid("raw"),cid("fruit"),{},FilterDepth::Item};StockpileFilterRowId material{sp.id,cid("food"),cid("raw"),cid("fruit"),cid("apple"),FilterDepth::Material};sp.filters={{category,"Food",TriState::Mixed},{group,"Raw",TriState::Mixed},{item,"Fruit",TriState::Mixed},{material,"Apple",TriState::Off}};c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});c.setStockpileFilterSearch("raw");check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters[0].label=="Apple","stockpile search matches ancestor columns but returns only the flat leaf");c.setStockpileRulesShown(true);check(port.actions.empty()&&c.state().stockpile.draft.rules.size()==1&&c.state().stockpile.draft.rules.begin()->second.id==material&&c.state().stockpile.draft.rules.begin()->second.allowed,"Allow All stages exactly the shown leaves until Apply");c.revertStockpileDraft();c.setStockpileFilterSearch("   ");check(c.state().stockpile.filterSearch.empty()&&c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters.front().label=="Apple","clearing stockpile search restores every flat row");c.setStockpileFilterSearch("apple");check(c.state().stockpile.visibleFilters.size()==1,"stockpile search matches the deepest column");c.setStockpileFilterSearch("");check(c.state().stockpile.visibleFilters.size()==1,"clearing search retains the flat projection");c.setStockpileColumnFilter(true,0,"food");c.setStockpileColumnFilter(true,1,"raw");c.setStockpileColumnFilter(true,2,"fruit");c.setStockpileColumnFilter(true,3,"apple");c.setStockpileColumnFilter(true,4,"blocked");check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.matchingFilterLeaves.size()==1,"Allow List headers filter their own columns and preserve bulk-match scope");for(std::size_t column=0;column<5;++column)c.setStockpileColumnFilter(true,column,"");c.toggleStockpileColumnSelection(true,0,"Food");check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.allowColumnSelections[0].size()==1,"Allow List checklist values filter exactly");c.toggleStockpileColumnSelection(true,0,"");check(c.state().stockpile.allowColumnSelections[0].empty(),"Allow List All clears the checklist");c.setStockpileAllowSort(StockpileSortKey::Category);c.setStockpileAllowSort(StockpileSortKey::Group);c.setStockpileAllowSort(StockpileSortKey::Material);c.setStockpileAllowSort(StockpileSortKey::Status);check(c.state().stockpile.allowSort==StockpileSortKey::Status&&c.state().stockpile.contentSort==StockpileSortKey::Item,"Allow List sorting is independent from Contents sorting");}
void stockpileContentCategoryTest()
{
	Port port; View view; Management6AController c(port,view); c.beginWorld(WorldEpoch{67});
	StockpileSnapshot sp; sp.id={25};
	const StockpileFilterRowId foodCategory{sp.id,cid("food"),{},{},{},FilterDepth::Category};
	const StockpileFilterRowId rawGroup{sp.id,cid("food"),cid("raw"),{},{},FilterDepth::Group};
	const StockpileFilterRowId fruitItem{sp.id,cid("food"),cid("raw"),cid("fruit"),{},FilterDepth::Item};
	const StockpileFilterRowId appleMaterial{sp.id,cid("food"),cid("raw"),cid("fruit"),cid("apple"),FilterDepth::Material};
	const StockpileFilterRowId drinkCategory{sp.id,cid("drinks"),{},{},{},FilterDepth::Category};
	const StockpileFilterRowId alcoholGroup{sp.id,cid("drinks"),cid("alcoholic"),{},{},FilterDepth::Group};
	const StockpileFilterRowId ciderItem{sp.id,cid("drinks"),cid("alcoholic"),cid("cider"),{},FilterDepth::Item};
	const StockpileFilterRowId ciderMaterial{sp.id,cid("drinks"),cid("alcoholic"),cid("cider"),cid("apple"),FilterDepth::Material};
	sp.filters={{drinkCategory,"Drinks",TriState::On},{alcoholGroup,"Alcoholic",TriState::On},{ciderItem,"Cider",TriState::On},{ciderMaterial,"Apple",TriState::On},{foodCategory,"Food",TriState::On},{rawGroup,"Raw",TriState::On},{fruitItem,"Fruit",TriState::On},{appleMaterial,"Apple",TriState::On}};
	sp.contents={
		{{cid("drinks"),{},{},{},FilterDepth::Category},"Drinks",3,200,{}},
		{{cid("drinks"),cid("alcoholic"),{},{},FilterDepth::Group},"Alcoholic",3,200,{}},
		{{cid("drinks"),cid("alcoholic"),cid("cider"),{},FilterDepth::Item},"Cider",3,100,{}},
		{{cid("drinks"),cid("alcoholic"),cid("cider"),cid("apple"),FilterDepth::Material},"Apple",3,100,{}},
		{{cid("food"),{},{},{},FilterDepth::Category},"Food",6,100,{}},
		{{cid("food"),cid("raw"),{},{},FilterDepth::Group},"Raw",6,100,{}},
		{{cid("food"),cid("raw"),cid("fruit"),{},FilterDepth::Item},"Fruit",6,100,{}},
		{{cid("food"),cid("raw"),cid("fruit"),cid("apple"),FilterDepth::Material},"Apple",6,90,{}}
	};
	c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});
	check(c.state().stockpile.visibleContents.size()==2,"stockpile Contents initially shows deepest flat rows");
	c.setStockpileFilterCategory(cid("food"));
	check(c.state().stockpile.visibleContents.size()==1&&c.state().stockpile.visibleContents.front().name=="Apple","stockpile Contents category rail filters flat rows");
	c.toggleStockpileContentExpansion({cid("food"),cid("raw"),{},{},FilterDepth::Group});
	check(c.state().stockpile.visibleContents.size()==1,"legacy content expansion does not alter flat rows");
	c.toggleStockpileContentExpansion({cid("food"),cid("raw"),cid("fruit"),{},FilterDepth::Item});
	check(c.state().stockpile.visibleContents.size()==1&&c.state().stockpile.visibleContents.back().name=="Apple","flat content row remains visible");
	c.setStockpileContentSearch("apple");
	check(c.state().stockpile.visibleContents.size()==1,"stockpile Contents search returns one flat matching row");
	c.setStockpileContentSearch("   ");
	check(c.state().stockpile.contentSearch.empty()&&c.state().stockpile.visibleContents.size()==1,"clearing Contents search restores all category rows");
	c.setStockpileFilterCategory({});
	check(c.state().stockpile.visibleContents.size()==2,"All category restores all flat rows");
	c.setStockpileColumnFilter(false,0,"drinks");c.setStockpileColumnFilter(false,1,"alcoholic");c.setStockpileColumnFilter(false,2,"cider");c.setStockpileColumnFilter(false,3,"apple");c.setStockpileColumnFilter(false,4,"Has (>0)");c.setStockpileColumnFilter(false,5,"Has (>0)");
	check(c.state().stockpile.visibleContents.size()==1&&c.state().stockpile.visibleContents.front().stockpiled==3,"Stock headers filter every displayed column independently");
	for(std::size_t column=0;column<6;++column)c.setStockpileColumnFilter(false,column,"");
	check(c.state().stockpile.visibleContents.size()==2,"clearing Stock header filters restores every stored item");
	c.toggleStockpileColumnSelection(false,0,"Drinks");check(c.state().stockpile.visibleContents.size()==1,"one Stock checklist value filters exactly");
	c.toggleStockpileColumnSelection(false,0,"Food");check(c.state().stockpile.visibleContents.size()==2,"multiple Stock checklist values use OR within one column");
	c.toggleStockpileColumnSelection(false,0,"");check(c.state().stockpile.visibleContents.size()==2&&c.state().stockpile.contentColumnSelections[0].empty(),"Stock All clears the checklist");
}
void mixedFilterMutationTest(){Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{63});StockpileSnapshot sp;sp.id={21};StockpileFilterRowId group{sp.id,cid("raw"),cid("stone"),{},{},FilterDepth::Group};sp.filters.push_back({group,"Stone",TriState::Mixed});c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});c.selectStockpileFilter(group);c.toggleSelectedStockpileFilter();check(port.actions.empty()&&c.state().stockpile.draft.rules.empty(),"a group row is not a rule: only leaf rules have check boxes");}
void filterKeyboardSelectionTest(){Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{65});StockpileSnapshot sp;sp.id={23};StockpileFilterRowId category{sp.id,cid("raw"),{},{},{},FilterDepth::Category};StockpileFilterRowId group{sp.id,cid("raw"),cid("stone"),{},{},FilterDepth::Group};StockpileFilterRowId item{sp.id,cid("raw"),cid("stone"),cid("block"),{},FilterDepth::Item};sp.filters={{category,"category",TriState::Mixed},{group,"group",TriState::Mixed},{item,"item",TriState::On}};c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});check(c.state().stockpile.visibleFilters.size()==1&&c.state().stockpile.visibleFilters.front().id==item,"three-level branch projects its deepest item");c.moveStockpileFilterSelection(1);check(c.state().stockpile.selectedFilter&&c.state().stockpile.selectedFilter->depth==FilterDepth::Item,"stockpile filter keyboard enters flat item row");c.moveStockpileFilterSelection(1);check(c.state().stockpile.selectedFilter&&c.state().stockpile.selectedFilter->depth==FilterDepth::Item,"stockpile filter keyboard clamps at last row");c.moveStockpileFilterSelection(-2147483647);check(c.state().stockpile.selectedFilter&&c.state().stockpile.selectedFilter->depth==FilterDepth::Item,"stockpile filter keyboard Home stays on first flat row");}
void stockpilePullPayloadTest(){Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{64});StockpileSnapshot sp;sp.id={22};sp.name="Pull test";sp.priority=1;sp.maxPriority=4;c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});check(c.state().stockpile.pane==StockpilePane::Contents,"new stockpiles always open on the Contents view");c.editStockpileOptions({false,true,false});check(port.actions.empty(),"hauling options stay pending");c.applyStockpileDraft();const auto& payload=std::get<SetStockpileBasicsPayload>(port.actions.back().payload);check(port.actions.back().id.value=="stockpile.set_basics"&&payload.priority==1&&payload.pull&&!payload.allowPull,"stockpile pull payload preserves original flag order");}
void filterSearchPerformanceTest()
{
	Port port; SinkView view; Management6AController c(port,view); c.beginWorld(WorldEpoch{68});
	StockpileSnapshot sp; sp.id={26};
	for(int categoryIndex=0;categoryIndex<8;++categoryIndex)
	{
		const CatalogId category{"category_"+std::to_string(categoryIndex)};
		sp.filters.push_back({{sp.id,category,{},{},{},FilterDepth::Category},"Category "+std::to_string(categoryIndex),TriState::Mixed});
		for(int groupIndex=0;groupIndex<8;++groupIndex)
		{
			const CatalogId group{"group_"+std::to_string(groupIndex)};
			sp.filters.push_back({{sp.id,category,group,{},{},FilterDepth::Group},"Group "+std::to_string(groupIndex),TriState::Mixed});
			for(int itemIndex=0;itemIndex<10;++itemIndex)
			{
				const CatalogId item{"item_"+std::to_string(itemIndex)};
				sp.filters.push_back({{sp.id,category,group,item,{},FilterDepth::Item},"Item "+std::to_string(itemIndex),TriState::Mixed});
				for(int materialIndex=0;materialIndex<2;++materialIndex)
				{
					const CatalogId material{"material_"+std::to_string(materialIndex)};
					sp.filters.push_back({{sp.id,category,group,item,material,FilterDepth::Material},"Material "+std::to_string(materialIndex),TriState::Off});
				}
			}
		}
	}
	check(sp.filters.size()==1992,"large stockpile fixture remains representative");
	const auto started=std::chrono::steady_clock::now();
	c.showStockpile(std::move(sp),Revision{1},WorldPosition{4,5,6});
	for(const char* query:{"m","ma","mat","mater","material",""}) c.setStockpileFilterSearch(query);
	const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
	std::cout<<"Large stockpile controller search: "<<elapsed<<" ms for load plus six queries\n";
	check(elapsed<250,"large stockpile controller search stays within the input-latency budget");
}
void workshopOrderParityTest()
{
	Port port;
	View view;
	Management6AController c( port, view );
	c.beginWorld( WorldEpoch { 60 } );
	WorkshopSnapshot workshop;
	workshop.id = { 9 };
	WorkshopProductRow product;
	product.id = cid( "plank" );
	auto requirement = component( "log", 2, "oak", 0 );
	requirement.materials.push_back( { cid( "pine" ), 8 } );
	product.components.push_back( requirement );
	workshop.products.push_back( product );
	workshop.queue.push_back( job( 91, "z-last", CraftRepeatMode::Once, 1 ) );
	workshop.queue.push_back( job( 92, "a-first", CraftRepeatMode::Repeat, 3 ) );
	c.showWorkshop( workshop, Revision { 1 }, WorldPosition { 1, 1, 1 } );
	check( c.state().workshop.visibleQueue.front().id.value == 91, "workshop queue keeps authoritative order" );
	c.queueSelectedCraftOrder();
	const auto missing = std::get<QueueCraftPayload>( port.actions.back().payload );
	check( missing.materials.front().value == "oak" && missing.count == 1, "missing material stock leaves order placeable" );
	const auto submitted = port.actions.size();
	c.queueSelectedCraftOrder();
	check(c.state().workshop.orderPending && port.actions.size()==submitted,"pending order blocks duplicate submissions");
	c.onWorkshopOrderResult(workshop.id,true);
	check(!c.state().workshop.orderPending && c.state().workshop.orderFeedback=="Order added to queue.","authoritative acknowledgement confirms the order");
	c.cycleWorkshopOrderMaterial( 0 );
	c.setWorkshopOrderMode( CraftRepeatMode::Maintain );
	c.setWorkshopOrderCount( 12 );
	c.queueSelectedCraftOrder();
	const auto configured = std::get<QueueCraftPayload>( port.actions.back().payload );
	check( configured.mode == CraftRepeatMode::Maintain && configured.count == 12 && configured.materials.front().value == "pine", "workshop order mode count and material are retained" );
	c.selectWorkshopJob( CraftJobId { 92 } );
	c.setSelectedJob( CraftRepeatMode::Once, 7, true, true );
	const auto edited = std::get<SetCraftJobPayload>( port.actions.back().payload );
	check( edited.count == 7 && edited.suspended && edited.moveBack, "queued order settings remain editable" );
	c.moveSelectedJob( MoveDirection::Front );
	check( std::get<MoveCraftJobPayload>( port.actions.back().payload ).direction == MoveDirection::Front, "queued order can move to top" );
	c.moveSelectedJob( MoveDirection::Back );
	check( std::get<MoveCraftJobPayload>( port.actions.back().payload ).direction == MoveDirection::Back, "queued order can move to bottom" );
}
int main()
{
	workshopOrderParityTest();
	filterPreorderTest();
	filterSearchAncestorTest();
	stockpileContentCategoryTest();
	mixedFilterMutationTest();
	filterKeyboardSelectionTest();
	stockpilePullPayloadTest();
	filterSearchPerformanceTest();
	Port port;View view;Management6AController c(port,view);c.beginWorld(WorldEpoch{61});c.showLoading(ManagementView::Workshop);check(c.state().workshop.request.status==RequestStatus::Loading,"workshop loading state");c.showError(ManagementView::Workshop,"ui.error.fixture");check(c.state().workshop.request.status==RequestStatus::Error,"workshop error state");
	WorkshopSnapshot ws;ws.id={10};ws.name="Forge";ws.priority=2;ws.maxPriority=5;WorkshopProductRow saw;saw.id=cid("saw");saw.components.push_back(component("blade",1,"iron",3));WorkshopProductRow axe;axe.id=cid("axe");axe.components.push_back(component("head",1,"iron",4));ws.products.push_back(saw);ws.products.push_back(axe);ws.queue.push_back(job(81,"saw",CraftRepeatMode::Once,2));ws.queue.push_back(job(82,"axe",CraftRepeatMode::Repeat,1));ws.queue.back().moveBack=true;c.showWorkshop(ws,Revision{1},WorldPosition{1,2,3});c.selectWorkshopProduct(cid("saw"));c.setSearch("axe");check(c.state().workshop.selectedProduct->value=="saw"&&c.state().workshop.selectionFiltered,"filter preserves hidden stable selection");c.toggleSort();check(c.state().workshop.selectedProduct->value=="saw","sort preserves selection");c.setSearch("");
	c.setWorkshopBasics("Forge II",3,true,true,false);check(port.actions.back().id.value=="workshop.set_basics"&&std::get<SetWorkshopBasicsPayload>(port.actions.back().payload).priority==3,"workshop basics action");c.setWorkshopBasics("Forge II",3,true,true,false,true);check(std::get<SetWorkshopBasicsPayload>(port.actions.back().payload).linkStockpile==std::optional<bool>{true},"workshop link action enables explicit link state");c.setWorkshopBasics("Forge II",3,true,true,false,false);check(std::get<SetWorkshopBasicsPayload>(port.actions.back().payload).linkStockpile==std::optional<bool>{false},"workshop link action disables explicit link state");c.queueSelectedCraftDefault();check(port.actions.back().id.value=="workshop.queue_craft"&&std::get<QueueCraftPayload>(port.actions.back().payload).materials.front().value=="iron","queue uses real available material");c.selectWorkshopJob(CraftJobId{82});c.moveSelectedJob(MoveDirection::Up);check(port.actions.back().id.value=="workshop.move_job"&&std::get<MoveCraftJobPayload>(port.actions.back().payload).job.value==82,"stable queue move action");c.cancelSelectedJob();check(port.actions.back().id.value=="workshop.cancel_job","queue cancel action");
	auto updated=job(82,"axe",CraftRepeatMode::Repeat,5);updated.moveBack=true;StableRowPatch<CraftJobId,CraftQueueRow> patch{PatchKind::Update,CraftJobId{82},updated,{}};check(c.patchWorkshopQueue(Revision{1},Revision{2},{patch}),"matching queue patch applies");check(c.state().workshop.selectedJob->value==82&&c.state().workshop.value.queue.back().count==5,"queue patch retains selection");const auto before=c.state().workshop.value.queue;check(!c.patchWorkshopQueue(Revision{1},Revision{3},{patch})&&c.state().workshop.value.queue==before,"stale patch is atomic");c.showWorkshop(ws,Revision{4},WorldPosition{1,2,3});
	TradeRow trade{{TradeParty::Trader,cid("cloth"),cid("wool"),0},"Cloth",4,1,12};c.setTradeRows(TradeParty::Trader,{trade});c.selectTradeRow(trade.id);check(c.state().workshop.selectedTradeRow==trade.id,"direct stable trade selection");c.setTradeValues(12,5);const auto actionCount=port.actions.size();c.executeTrade();check(port.actions.size()==actionCount&&c.state().status=="ui.trade.offer_value_too_low","trade value constraint blocks execution");c.setTradeValues(12,20);c.executeTrade();check(c.state().workshop.tradeConfirmationRequired&&port.actions.size()==actionCount,"trade requires explicit confirmation");c.setWorkshopBasics("blocked",1,false,false,false);check(port.actions.size()==actionCount&&c.state().status=="ui.error.input_blocked_by_confirmation","confirmation blocks workbench input");c.cancelTrade();check(!c.state().workshop.tradeConfirmationRequired,"cancel clears confirmation blocker");UiActionEnvelope direct{ActionId{"trade.execute"},RequestId{999},WorldEpoch{61},std::nullopt,WorkshopTargetPayload{WorkshopId{10}}};check(port.dispatch(direct,DispatchOrigin::Workbench).status==CommandStatus::Rejected,"direct unconfirmed trade execution rejected");c.executeTrade();c.confirmTrade();check(port.actions.back().id.value=="trade.execute"&&port.origins.back()==DispatchOrigin::DestructiveConfirmation,"confirmed trade uses exact action and origin");
	StockpileSnapshot sp; sp.id={20}; sp.name="Stone"; sp.maxPriority=4;
	const StockpileFilterRowId stoneId{sp.id,cid("raw"),cid("stone"),cid("block"),cid("granite"),FilterDepth::Material};
	sp.filters.push_back({stoneId,"Granite",TriState::On});
	sp.contents={
		{{cid("raw"),{},{},{},FilterDepth::Category},"Raw",7,12,{}},
		{{cid("raw"),cid("stone"),{},{},FilterDepth::Group},"Stone",7,12,{}},
		{{cid("raw"),cid("stone"),cid("block"),{},FilterDepth::Item},"Block",7,12,{}},
		{{cid("raw"),cid("stone"),cid("block"),cid("granite"),FilterDepth::Material},"Granite",7,12,{}}
	};
	sp.templateNames={"Building stone","Food basics"};
	c.showStockpile(sp,Revision{1},WorldPosition{4,5,6});
	check(c.state().stockpile.visibleContents.size()==1&&c.state().stockpile.visibleContents.front().name=="Granite","stockpile content projects the deepest row");
	c.selectStockpileFilter(stoneId); c.setSearch("missing");
	check(c.state().stockpile.selectedFilter==stoneId&&c.state().stockpile.selectionFiltered,"stockpile filter preserves stable selection");
	c.setSearch(""); const auto beforeToggle=port.actions.size(); c.toggleSelectedStockpileFilter();
	check(port.actions.size()==beforeToggle&&c.state().stockpile.draft.rules.size()==1&&!c.state().stockpile.draft.rules.begin()->second.allowed,"a rule toggle stays pending");
	c.editStockpileDraft("Stone II","2"); c.applyStockpileDraft();
	check(port.actions.size()==beforeToggle+2&&port.actions[beforeToggle].id.value=="stockpile.set_basics"&&port.actions.back().id.value=="stockpile.set_filters"&&!std::get<SetStockpileFiltersPayload>(port.actions.back().payload).active,"Apply sends basics and the pending rule");
	c.toggleStockpileTemplateMenu();
	check(c.state().stockpile.templateMenuOpen,"stockpile template combo opens its saved-template list");
	c.selectStockpileTemplate("Food basics");
	check(!c.state().stockpile.templateMenuOpen&&c.state().stockpile.templateName=="Food basics"&&port.actions.back().id.value=="stockpile.apply_template","selecting a combo option applies that template");
	c.setStockpileTemplateName("Wood only"); c.saveStockpileTemplate();
	check(port.actions.back().id.value=="stockpile.save_template"&&std::get<StockpileTemplatePayload>(port.actions.back().payload).name=="Wood only","a unique stockpile template saves without confirmation");
	const auto beforeOverwrite=port.actions.size();
	c.setStockpileTemplateName("food BASICS"); c.updateStockpileTemplate();
	check(port.actions.size()==beforeOverwrite&&c.state().stockpile.templateOverwriteConfirmationRequired&&c.state().stockpile.pendingTemplateOverwrite=="Food basics","an existing template name requires overwrite confirmation");
	c.revertStockpileDraft(); c.editStockpileDraft("Blocked during prompt","2"); c.applyStockpileDraft();
	check(port.actions.size()==beforeOverwrite,"overwrite confirmation blocks other workbench mutations");
	c.confirmStockpileTemplateOverwrite();
	check(!c.state().stockpile.templateOverwriteConfirmationRequired&&port.actions.back().id.value=="stockpile.save_template"&&port.origins.back()==DispatchOrigin::DestructiveConfirmation&&std::get<StockpileTemplatePayload>(port.actions.back().payload).name=="Food basics","confirmed overwrite uses the canonical saved name and destructive-confirmation origin");
	c.setStockpileTemplateName("Building stone"); c.updateStockpileTemplate(); c.cancelStockpileTemplateOverwrite();
	check(!c.state().stockpile.templateOverwriteConfirmationRequired&&c.state().stockpile.pendingTemplateOverwrite.empty(),"template overwrite can be cancelled without mutation");
	c.setStockpilePane(StockpilePane::Settings);check(c.state().stockpile.pane==StockpilePane::Settings,"stockpile Settings view opens its pane");
	AgricultureSnapshot ag;ag.target={AgricultureKind::Pasture,DesignationId{30}};ag.name="Yak pasture";ag.maxMale=2;ag.maxFemale=4;PastureAnimalRow animal;animal.id={99};animal.name="Moss";animal.species=cid("yak");ag.animals.push_back(animal);PastureFoodRow food;food.item=cid("hay");food.material=cid("grass");food.name="Grass hay";food.allowed=true;ag.foods.push_back(food);c.showAgriculture(ag,Revision{1},WorldPosition{7,8,9});c.selectAgricultureAnimal(CreatureId{99});c.toggleSelectedAnimalButchering();check(port.actions.back().id.value=="agriculture.set_butchering"&&std::get<SetButcheringPayload>(port.actions.back().payload).creature.value==99,"pasture roster mutation");c.setPastureCap(Gender::Female,5);check(port.actions.back().id.value=="agriculture.set_population_caps","pasture cap mutation");c.setPastureFood(cid("hay"),cid("grass"),false);check(port.actions.back().id.value=="agriculture.set_food_allowed","pasture food mutation");c.locate();check(port.actions.back().id.value=="view.center_on","locate action");
	AgricultureSnapshot farm;farm.target={AgricultureKind::Farm,DesignationId{31}};farm.name="South farm";farm.product=cid("Wheat");farm.catalog={{cid("Wheat"),"Wheat",34,12,16}};FarmPlotRow plotA;plotA.position={8,9,10};FarmPlotRow plotB;plotB.position={9,9,10};farm.fields={plotA,plotB};c.showAgriculture(farm,Revision{2},WorldPosition{8,9,10});
	check(c.state().agriculture.pane==AgriculturePane::Plots,"new farm opens its Plots page");
	c.setAgriculturePane(AgriculturePane::Crops);check(c.state().agriculture.pane==AgriculturePane::Crops,"Crops tab opens its page");
	c.toggleFarmPlot({8,9,10});c.toggleFarmPlot({9,9,10});c.assignSelectedFarmPlotCrop();check(port.actions.back().id.value=="agriculture.set_plot_crop"&&std::get<SetFarmPlotCropPayload>(port.actions.back().payload).plots.size()==2,"Farm crop assignment scopes to selected real plots");
	c.queueSelectedFarmPlotCrop(3,false);check(port.actions.back().id.value=="agriculture.queue_plot_crop"&&std::get<QueueFarmPlotCropPayload>(port.actions.back().payload).count==3&&std::get<QueueFarmPlotCropPayload>(port.actions.back().payload).plots.size()==2,"Farm queue count applies per selected plot");
	c.queueSelectedFarmPlotCrop(1,true);check(std::get<QueueFarmPlotCropPayload>(port.actions.back().payload).repeat,"Farm repeat queue action is explicit");
	c.moveFarmPlotOrder({8,9,10},42,MoveDirection::Up);check(port.actions.back().id.value=="agriculture.move_plot_order","Farm queue reordering dispatches");
	c.cancelFarmPlotOrder({8,9,10},42);check(port.actions.back().id.value=="agriculture.cancel_plot_order","Farm queue cancellation dispatches");
	farm.ready=4;c.showAgriculture(farm,Revision{3},WorldPosition{8,9,10});check(c.state().agriculture.pane==AgriculturePane::Crops&&c.state().agriculture.value.ready==4&&c.state().agriculture.selectedPlots.size()==2,"live Farm updates preserve the pane and selected plots");
	farm.target.designation=DesignationId{32};c.showAgriculture(farm,Revision{4},WorldPosition{9,9,10});check(c.state().agriculture.pane==AgriculturePane::Plots,"new designation resets to its first page");
	c.endWorld();const auto after=port.actions.size();c.refresh();c.setAgricultureBasics("late",1,false);check(port.actions.size()==after&&!c.state().world&&c.state().view==ManagementView::None,"world unload clears and blocks late actions");std::cout<<"Management 6A controller lifecycle, identity, patch, filter, and mutation tests passed\n";
}
