/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "HudRmlBinding.h"
#include "../../localization/RmlText.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StringUtilities.h>
#include <cctype>
#include <cstdio>
#include <algorithm>
namespace ingnomia::ui::hud
{
namespace
{
std::string domId( std::string_view prefix, std::string_view value )
{
	std::string out( prefix );
	for( const unsigned char c : value ) out.push_back( std::isalnum( c ) || c == '_' || c == '-' ? static_cast<char>( c ) : '_' );
	return out;
}

std::string displayName( std::string_view value )
{
	std::string out;
	bool capitalize = true;
	char previous = 0;
	for ( const unsigned char character : value )
	{
		if ( character == '_' || character == '-' ) { out += ' '; capitalize = true; previous = ' '; continue; }
		if ( std::isupper( character ) && previous && std::islower( static_cast<unsigned char>( previous ) ) ) out += ' ';
		out += static_cast<char>( capitalize ? std::toupper( character ) : character );
		capitalize = false;
		previous = static_cast<char>( character );
	}
	return out;
}


std::string buildTypeName( std::string_view value )
{
	if ( value == "Craft" ) return "Crafts";
	return displayName( value );
}

int buildTypeRank( std::string_view value )
{
	constexpr std::string_view preferred[] = { "Wood", "Stone", "Metal", "Food", "Craft", "Mechanics", "Misc", "Soil", "Other", "Containers" };
	for ( std::size_t index = 0; index < sizeof( preferred ) / sizeof( preferred[0] ); ++index ) if ( value == preferred[index] ) return static_cast<int>( index );
	return 100;
}std::string buildGlyph( const BuildCatalogRow& row )
{
	if ( row.kind == BuildKind::Workshop ) return "W";
	if ( row.kind == BuildKind::Terrain ) return "T";
	for ( const unsigned char c : row.name )
		if ( std::isalnum( c ) ) return std::string( 1, static_cast<char>( std::toupper( c ) ) );
	return "?";
}

std::string buildSpriteMarkup( const BuildCatalogRow& row )
{
	if ( row.spriteSheet.empty() || row.spriteWidth <= 0 || row.spriteHeight <= 0 || row.spriteSheetWidth <= 0 || row.spriteSheetHeight <= 0 ) return {};
	for ( const unsigned char c : row.spriteSheet )
		if ( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' ) ) return {};
	const double scale = std::min( 42.0 / static_cast<double>( row.spriteWidth ), 42.0 / static_cast<double>( row.spriteHeight ) );
	const double scaledSheetWidth = static_cast<double>( row.spriteSheetWidth ) * scale;
	const double scaledSheetHeight = static_cast<double>( row.spriteSheetHeight ) * scale;
	const double left = ( 44.0 - static_cast<double>( row.spriteWidth ) * scale ) * 0.5 - static_cast<double>( row.spriteX ) * scale;
	const double top = ( 44.0 - static_cast<double>( row.spriteHeight ) * scale ) * 0.5 - static_cast<double>( row.spriteY ) * scale;
	char style[256]{};
	std::snprintf( style, sizeof style, "width:%.2fdp;height:%.2fdp;left:%.2fdp;top:%.2fdp;", scaledSheetWidth, scaledSheetHeight, left, top );
	// RmlUi resolves this URL relative to screens/game_hud.rml, so ../tilesheet
	// lands in the mirrored content/rmlui/tilesheet sandbox directory.
	return "<div class='c-hud-build-sprite'><img class='c-hud-build-sprite-image' src='../tilesheet/" + row.spriteSheet + "' style='" + style + "' /></div>";
}
}
void HudRmlBinding::Callback::ProcessEvent( Rml::Event& event ) { if( eventFn_ ) eventFn_( event ); else if( fn_ ) fn_(); }
HudRmlBinding::HudRmlBinding( Rml::Context& context, Presentation presentation, ToolPanel toolPanel ) : context_( context ), presentation_( presentation ), toolPanel_( toolPanel ) {}
HudRmlBinding::~HudRmlBinding() { shutdown(); }
bool HudRmlBinding::initialize( HudController& controller )
{
	controller_ = &controller; const char* documentPath = presentation_ == Presentation::OrdersTools ? "screens/orders_tools.rml" : "screens/game_hud.rml"; document_ = documentLoader_ ? documentLoader_( documentPath ) : context_.LoadDocument( documentPath ); if( !document_ ) return false; localization::applyRmlText( *document_, textCatalog_ );
	bind("hud_pause",[this]{if(openPause_)openPause_();else controller_->setPaused(!controller_->state().clock.paused);}); bind("hud_speed_normal",[this]{controller_->setSpeed(GameSpeed::Normal);}); bind("hud_speed_fast",[this]{controller_->setSpeed(GameSpeed::Fast);});
	bind("tutorial-continue",[this]{controller_->tutorialAdvance();}); bind("tutorial-skip",[this]{controller_->tutorialSkip();}); bind("tutorial-restart",[this]{controller_->tutorialRestart();}); bind("tutorial-hints",[this]{controller_->tutorialToggleHints();}); bind("tutorial-finish",[this]{controller_->tutorialFinish();});
	bind("hud_level_down",[this]{controller_->changeLevel(controller_->state().camera.viewLevel-1);}); bind("hud_level_up",[this]{controller_->changeLevel(controller_->state().camera.viewLevel+1);});
	bind("hud_overlay_designations",[this]{auto s=controller_->state().overlays;controller_->setOverlay(OverlayKind::Designations,!s.designations);}); bind("hud_overlay_jobs",[this]{auto s=controller_->state().overlays;controller_->setOverlay(OverlayKind::Jobs,!s.jobs);}); bind("hud_overlay_walls",[this]{auto s=controller_->state().overlays;controller_->setOverlay(OverlayKind::LoweredWalls,!s.loweredWalls);}); bind("hud_overlay_axles",[this]{auto s=controller_->state().overlays;controller_->setOverlay(OverlayKind::Axles,!s.axles);});
	bind("hud_tool_inspect",[this]{mineMenuOpen_=false;agricultureMenuOpen_=false;designationMenuOpen_=false;jobsMenuOpen_=false;buildMenuOpen_=false;kingdomPanelOpen_=false;controller_->cancelTool();stateChanged(controller_->state());});
	bind("hud_tool_mine",[this]{if(presentation_==Presentation::Full){toggleActionMenu("mine");return;}if(openOrdersTools_){openOrdersTools_("hud_tool_mine");return;}mineMenuOpen_=true;controller_->activateTool(ToolId{"mine"});stateChanged(controller_->state());});
	bind("hud_tool_build",[this]{
		if(openOrdersTools_){openOrdersTools_("hud_tool_build");return;}
		if(presentation_==Presentation::OrdersTools){buildMenuOpen_=true;buildCategoryPage_=true;selectedBuildCategory_.clear();selectedBuildType_.clear();selectedBuild_.clear();controller_->closeBuildMenu();stateChanged(controller_->state());return;}
		buildMenuOpen_=!buildMenuOpen_;
		if(buildMenuOpen_){buildCategoryPage_=true;selectedBuildCategory_.clear();selectedBuildType_.clear();selectedBuild_.clear();controller_->closeBuildMenu();}else controller_->closeBuildMenu();
		stateChanged(controller_->state());
	});
	bind("hud_tool_agriculture",[this]{if(presentation_==Presentation::Full){toggleActionMenu("agriculture");return;}if(openOrdersTools_){openOrdersTools_("hud_tool_agriculture");return;}agricultureMenuOpen_=true;stateChanged(controller_->state());});
	bind("hud_tool_designations",[this]{if(presentation_==Presentation::Full){toggleActionMenu("designations");return;}if(openOrdersTools_){openOrdersTools_("hud_tool_designations");return;}designationMenuOpen_=true;stateChanged(controller_->state());});
	bind("hud_tool_jobs",[this]{if(presentation_==Presentation::Full){toggleActionMenu("jobs");return;}if(openOrdersTools_){openOrdersTools_("hud_tool_jobs");return;}jobsMenuOpen_=true;stateChanged(controller_->state());});
	bind("hud_tool_fell_tree",[this]{controller_->activateTool(ToolId{"fell_tree"});stateChanged(controller_->state());}); bind("hud_tool_plant_tree",[this]{controller_->activateTool(ToolId{"plant_tree"});stateChanged(controller_->state());}); bind("hud_tool_harvest_tree",[this]{controller_->activateTool(ToolId{"harvest_tree"});stateChanged(controller_->state());}); bind("hud_tool_forage",[this]{controller_->activateTool(ToolId{"forage"});stateChanged(controller_->state());}); bind("hud_tool_remove_plant",[this]{controller_->activateTool(ToolId{"remove_plant"});stateChanged(controller_->state());});
	bind("hud_tool_stockpile",[this]{controller_->activateTool(ToolId{"create_stockpile"});stateChanged(controller_->state());}); bind("hud_tool_farm",[this]{controller_->activateTool(ToolId{"create_farm"});stateChanged(controller_->state());}); bind("hud_tool_grove",[this]{controller_->activateTool(ToolId{"create_grove"});stateChanged(controller_->state());}); bind("hud_tool_pasture",[this]{controller_->activateTool(ToolId{"create_pasture"});stateChanged(controller_->state());}); bind("hud_tool_personal_room",[this]{controller_->activateTool(ToolId{"create_personal_room"});stateChanged(controller_->state());}); bind("hud_tool_dormitory",[this]{controller_->activateTool(ToolId{"create_dormitory"});stateChanged(controller_->state());}); bind("hud_tool_dining_hall",[this]{controller_->activateTool(ToolId{"create_dining_hall"});stateChanged(controller_->state());}); bind("hud_tool_hospital",[this]{controller_->activateTool(ToolId{"create_hospital"});stateChanged(controller_->state());}); bind("hud_tool_forbidden",[this]{controller_->activateTool(ToolId{"create_forbidden_area"});stateChanged(controller_->state());}); bind("hud_tool_remove_designation",[this]{controller_->activateTool(ToolId{"remove_designation"});stateChanged(controller_->state());});
	bind("hud_tool_suspend_job",[this]{controller_->activateTool(ToolId{"suspend_job"});stateChanged(controller_->state());}); bind("hud_tool_resume_job",[this]{controller_->activateTool(ToolId{"resume_job"});stateChanged(controller_->state());}); bind("hud_tool_cancel_job",[this]{controller_->activateTool(ToolId{"cancel_job"});stateChanged(controller_->state());}); bind("hud_tool_lower_priority",[this]{controller_->activateTool(ToolId{"lower_job_priority"});stateChanged(controller_->state());}); bind("hud_tool_raise_priority",[this]{controller_->activateTool(ToolId{"raise_job_priority"});stateChanged(controller_->state());});
	bind("hud_tool_cancel",[this]{mineMenuOpen_=false;agricultureMenuOpen_=false;designationMenuOpen_=false;jobsMenuOpen_=false;buildMenuOpen_=false;kingdomPanelOpen_=false;controller_->closeBuildMenu();controller_->cancelTool();stateChanged(controller_->state());}); bind("hud_tool_rotate",[this]{controller_->rotateTool();});
	bind("hud_mine_walls",[this]{selectMineMode("mine");}); bind("hud_mine_explorative",[this]{selectMineMode("explorative_mine");}); bind("hud_mine_remove_floor",[this]{selectMineMode("remove_floor");}); bind("hud_mine_hole",[this]{selectMineMode("dig_hole");}); bind("hud_mine_stairs_down",[this]{selectMineMode("dig_stairs_down");}); bind("hud_mine_stairs_up",[this]{selectMineMode("mine_stairs_up");}); bind("hud_mine_ramp_down",[this]{selectMineMode("dig_ramp_down");}); bind("hud_mine_close",[this]{if(presentation_==Presentation::OrdersTools&&close_){close_();return;}mineMenuOpen_=false;stateChanged(controller_->state());});
	bind("hud_agriculture_close",[this]{if(presentation_==Presentation::OrdersTools&&close_){close_();return;}agricultureMenuOpen_=false;stateChanged(controller_->state());}); bind("hud_designations_close",[this]{if(presentation_==Presentation::OrdersTools&&close_){close_();return;}designationMenuOpen_=false;stateChanged(controller_->state());}); bind("hud_jobs_close",[this]{if(presentation_==Presentation::OrdersTools&&close_){close_();return;}jobsMenuOpen_=false;stateChanged(controller_->state());});
	bind("hud_mine_back",[this]{backToSidebar();}); bind("hud_agriculture_back",[this]{backToSidebar();}); bind("hud_designations_back",[this]{backToSidebar();}); bind("hud_jobs_back",[this]{backToSidebar();});
	bind("hud_tool_deconstruct",[this]{controller_->activateTool(ToolId{"deconstruct"});stateChanged(controller_->state());}); bind("hud_build_close",[this]{const auto active=controller_->state().tool.active;const bool buildActive=active&&active->value=="build";buildMenuOpen_=false;buildCategoryPage_=true;selectedBuildCategory_.clear();selectedBuildType_.clear();selectedBuild_.clear();controller_->closeBuildMenu();if(buildActive)controller_->cancelTool();stateChanged(controller_->state());if(presentation_==Presentation::OrdersTools&&close_)close_();});
	bind("hud_build_furniture",[this]{selectBuildCategory("hud_build_furniture",BuildSelection::Furniture,{});}); bind("hud_build_workshop",[this]{selectBuildCategory("hud_build_workshop",BuildSelection::Workshop,{});}); bind("hud_build_containers",[this]{selectBuildCategory("hud_build_containers",BuildSelection::Containers,{});}); bind("hud_build_utility",[this]{selectBuildCategory("hud_build_utility",BuildSelection::Utility,{});}); bind("hud_build_wall",[this]{selectBuildCategory("hud_build_wall",BuildSelection::Wall,{});}); bind("hud_build_floor",[this]{selectBuildCategory("hud_build_floor",BuildSelection::Floor,{});}); bind("hud_build_stairs",[this]{selectBuildCategory("hud_build_stairs",BuildSelection::Stairs,{});}); bind("hud_build_ramps",[this]{selectBuildCategory("hud_build_ramps",BuildSelection::Ramps,{});}); bind("hud_build_fence",[this]{selectBuildCategory("hud_build_fence",BuildSelection::Fence,{});});
	bind("hud_build_back",[this]{backToBuildCategories();});
	bind("hud_open_kingdom",[this]{kingdomPanelOpen_=!kingdomPanelOpen_;stateChanged(controller_->state());}); bind("hud_open_population",[this]{if(openPopulation_)openPopulation_(FocusToken{1});});bind("hud_open_inventory",[this]{if(openInventory_)openInventory_(FocusToken{2});});bind("hud_open_military",[this]{if(openMilitary_)openMilitary_(FocusToken{3});});bind("hud_open_diplomacy",[this]{if(openDiplomacy_)openDiplomacy_(FocusToken{4});}); bind("hud_kingdom_close",[this]{kingdomPanelOpen_=false;stateChanged(controller_->state());}); bind("hud_tools_close",[this]{if(close_)close_();});
	bindSidebarTooltip("hud_open_inventory","hud.tip.inventory"); bindSidebarTooltip("hud_open_military","hud.tip.military"); bindSidebarTooltip("hud_open_population","hud.tip.population"); bindSidebarTooltip("hud_open_diplomacy","hud.tip.missions");
	bindSidebarTooltip("hud_tool_build","hud.tip.build"); bindSidebarTooltip("hud_tool_deconstruct","hud.tip.deconstruct"); bindSidebarTooltip("hud_tool_mine","hud.tip.mine"); bindSidebarTooltip("hud_tool_agriculture","hud.tip.agriculture"); bindSidebarTooltip("hud_tool_designations","hud.tip.designations"); bindSidebarTooltip("hud_tool_jobs","hud.tip.jobs");
	bindSidebarTooltip("hud_overlay_designations","hud.tip.overlay_designations"); bindSidebarTooltip("hud_overlay_jobs","hud.tip.overlay_jobs"); bindSidebarTooltip("hud_overlay_walls","hud.tip.overlay_walls"); bindSidebarTooltip("hud_overlay_axles","hud.tip.mechanics");
	bind("hud_event_ack",[this]{controller_->respondToPrompt(EventResponse::Acknowledge);}); bind("hud_event_yes",[this]{controller_->respondToPrompt(EventResponse::Yes);}); bind("hud_event_no",[this]{controller_->respondToPrompt(EventResponse::No);});
	bindEvent("hud_build_catalog","click",[this](Rml::Event& event){if(updatingBuildCatalog_){event.StopPropagation();return;}for(auto* element=event.GetTargetElement();element&&element!=event.GetCurrentElement();element=element->GetParentNode()){if(!element->GetAttribute<Rml::String>("data-build-component","").empty()){event.StopPropagation();return;}const auto id=element->GetAttribute<Rml::String>("data-build","");if(!id.empty()){if(element->GetAttribute<Rml::String>("aria-disabled","")=="true"){event.StopPropagation();return;}selectedBuild_=id;const auto action=element->GetAttribute<Rml::String>("data-build-action","");const auto buildAction=action=="FillHole"?BuildAction::FillHole:action=="Replace"?BuildAction::Replace:BuildAction::Build;controller_->chooseBuildAction(CatalogId{id},buildAction);event.StopPropagation();stateChanged(controller_->state());break;}}});
	bindEvent("hud_build_catalog","change",[this](Rml::Event& event){if(updatingBuildCatalog_){event.StopPropagation();return;}auto* element=event.GetTargetElement();if(!element)return;const auto id=element->GetAttribute<Rml::String>("data-build","");const auto component=element->GetAttribute<Rml::String>("data-build-component","");if(id.empty()||component.empty())return;try{const auto value=element->GetAttribute<Rml::String>("value","");controller_->selectBuildMaterial(CatalogId{id},static_cast<std::uint32_t>(std::stoul(std::string(component.data(),component.size()))),CatalogId{value},false);event.StopPropagation();}catch(...){}});
	bindEvent("hud_build_type_list","click",[this](Rml::Event& event){for(auto* element=event.GetTargetElement();element&&element!=event.GetCurrentElement();element=element->GetParentNode()){const auto type=element->GetAttribute<Rml::String>("data-build-type","");if(!type.empty()){selectedBuildType_=type;selectedBuild_.clear();event.StopPropagation();stateChanged(controller_->state());break;}}});
	bindEvent("hud_watch_rows","click",[this](Rml::Event& event){for(auto* element=event.GetTargetElement();element&&element!=event.GetCurrentElement();element=element->GetParentNode()){if(!element->GetAttribute<Rml::String>("data-watch","").empty()){if(openInventory_)openInventory_(FocusToken{2});event.StopPropagation();break;}}});
	stateChanged( controller.state() ); document_->Show(); return true;
}
void HudRmlBinding::setWorkbenchHandlers(WorkbenchHandler population,WorkbenchHandler inventory,WorkbenchHandler military,WorkbenchHandler diplomacy){openPopulation_=std::move(population);openInventory_=std::move(inventory);openMilitary_=std::move(military);openDiplomacy_=std::move(diplomacy);}
void HudRmlBinding::setOrdersToolsHandler( OrdersToolsHandler handler ){ openOrdersTools_ = std::move( handler ); }
void HudRmlBinding::setToolPanel( ToolPanel panel )
{
	if ( toolPanel_ == panel ) return;
	toolPanel_ = panel;
	if ( document_ && controller_ ) stateChanged( controller_->state() );
}
void HudRmlBinding::setCloseHandler( CloseHandler handler ){ close_ = std::move( handler ); }
void HudRmlBinding::setDocumentLoader( DocumentLoader loader ){ documentLoader_ = std::move( loader ); }
void HudRmlBinding::setPauseHandler(PauseHandler pause){openPause_=std::move(pause);}
void HudRmlBinding::toggleActionMenu( std::string_view menu )
{
	bool* target = menu == "mine" ? &mineMenuOpen_ : menu == "agriculture" ? &agricultureMenuOpen_ : menu == "designations" ? &designationMenuOpen_ : &jobsMenuOpen_;
	const bool open = !*target;
	mineMenuOpen_ = false;
	agricultureMenuOpen_ = false;
	designationMenuOpen_ = false;
	jobsMenuOpen_ = false;
	*target = open;
	hideSidebarTooltip();
	stateChanged( controller_->state() );
}
void HudRmlBinding::backToSidebar()
{
	mineMenuOpen_ = false;
	agricultureMenuOpen_ = false;
	designationMenuOpen_ = false;
	jobsMenuOpen_ = false;
	hideSidebarTooltip();
	if ( controller_->state().tool.active ) controller_->cancelTool();
	stateChanged( controller_->state() );
}

void HudRmlBinding::bindSidebarTooltip( const char* id, const char* textKey )
{
	if ( auto* element = document_->GetElementById( id ) )
	{
		auto show = std::make_unique<Callback>( [this, textKey]( Rml::Event& event ) { showSidebarTooltip( textKey, event.GetCurrentElement() ); } );
		element->AddEventListener( "mouseover", show.get() );
		listenerTargets_.emplace_back( element, "mouseover" );
		callbacks_.push_back( std::move( show ) );
		auto focus = std::make_unique<Callback>( [this, textKey]( Rml::Event& event ) { showSidebarTooltip( textKey, event.GetCurrentElement() ); } );
		element->AddEventListener( "focus", focus.get() );
		listenerTargets_.emplace_back( element, "focus" );
		callbacks_.push_back( std::move( focus ) );
		auto hide = std::make_unique<Callback>( [this]( Rml::Event& ) { hideSidebarTooltip(); } );
		element->AddEventListener( "mouseout", hide.get() );
		listenerTargets_.emplace_back( element, "mouseout" );
		callbacks_.push_back( std::move( hide ) );
		auto blur = std::make_unique<Callback>( [this]( Rml::Event& ) { hideSidebarTooltip(); } );
		element->AddEventListener( "blur", blur.get() );
		listenerTargets_.emplace_back( element, "blur" );
		callbacks_.push_back( std::move( blur ) );
	}
}
void HudRmlBinding::showSidebarTooltip( const char* textKey, Rml::Element* source )
{
	if ( !document_ || !source ) return;
	if ( auto* tooltip = document_->GetElementById( "hud_sidebar_tooltip" ) )
	{
		const auto textValue = textCatalog_.format( LocalizationKey{ textKey } );
		tooltip->SetInnerRML( Rml::StringUtilities::EncodeRml( textValue ) );
		// RmlUi reports absolute offsets and box sizes in rendered pixels. Use px
		// here so the context density scale is not applied a second time when the
		// tooltip's inline position is resolved.
		const auto offset = source->GetAbsoluteOffset();
		const auto size = source->GetBox().GetSize();
		float tooltipTop = offset.y + size.y * 0.5f;
		// The sidebar now uses flex centering, which is reflected in the source's
		// absolute offset. The tooltip transform centers this top coordinate on
		// the hovered control, so no sidebar-height correction is required.
		tooltip->SetProperty( "left", std::to_string( offset.x + size.x + 10.0f ) + "px" );
		tooltip->SetProperty( "top", std::to_string( tooltipTop ) + "px" );
		tooltip->SetClass( "is-visible", true );
		tooltip->SetAttribute( "aria-hidden", "false" );
	}
}
void HudRmlBinding::hideSidebarTooltip()
{
	if ( !document_ ) return;
	if ( auto* tooltip = document_->GetElementById( "hud_sidebar_tooltip" ) )
	{
		tooltip->SetClass( "is-visible", false );
		tooltip->SetAttribute( "aria-hidden", "true" );
	}
}
void HudRmlBinding::selectBuildCategory(const char* elementId, BuildSelection selection, std::string_view category){selectedBuildCategory_=elementId;selectedBuildType_.clear();selectedBuild_.clear();buildCategoryPage_=false;buildMenuOpen_=true;controller_->requestBuildItems(selection,category);stateChanged(controller_->state());}
void HudRmlBinding::backToBuildCategories(){buildCategoryPage_=true;selectedBuildCategory_.clear();selectedBuildType_.clear();selectedBuild_.clear();controller_->closeBuildMenu();stateChanged(controller_->state());}
void HudRmlBinding::selectMineMode(std::string_view tool){controller_->activateTool(ToolId{std::string(tool)});mineMenuOpen_=true;stateChanged(controller_->state());}
void HudRmlBinding::restoreWorkbenchFocus(FocusToken token){const char*id=token.value==1?"hud_open_population":token.value==2?"hud_open_inventory":token.value==3?"hud_open_military":token.value==4?"hud_open_diplomacy":nullptr;if(id)if(auto*element=document_->GetElementById(id))element->Focus();}
bool HudRmlBinding::activateElement( std::string_view id )
{
	if( !document_ ) return false;
	auto* element = document_->GetElementById( std::string( id ) );
	if( !element ) return false;
	element->DispatchEvent( "click", Rml::Dictionary{} );
	return true;
}
void HudRmlBinding::shutdown(){ for(std::size_t i=0;i<callbacks_.size()&&i<listenerTargets_.size();++i)listenerTargets_[i].first->RemoveEventListener(listenerTargets_[i].second,callbacks_[i].get()); listenerTargets_.clear(); callbacks_.clear(); if(document_){context_.UnloadDocument(document_);document_=nullptr;} controller_=nullptr; }
void HudRmlBinding::bind( const char* id, std::function<void()> callback ){ if(auto* e=document_->GetElementById(id)){auto cb=std::make_unique<Callback>(std::move(callback));e->AddEventListener(Rml::EventId::Click,cb.get());listenerTargets_.emplace_back(e,"click");callbacks_.push_back(std::move(cb));} }
void HudRmlBinding::bindEvent( const char* id, const char* event, std::function<void( Rml::Event& )> callback ){ if(auto* e=document_->GetElementById(id)){auto cb=std::make_unique<Callback>(std::move(callback));e->AddEventListener(event,cb.get());listenerTargets_.emplace_back(e,event);callbacks_.push_back(std::move(cb));} }
void HudRmlBinding::text( const char* id, const std::string& value ){if(auto* e=document_->GetElementById(id))e->SetInnerRML(Rml::StringUtilities::EncodeRml(value));}
void HudRmlBinding::visible( const char* id, bool value ){if(auto* e=document_->GetElementById(id))e->SetClass("is-hidden",!value);}
void HudRmlBinding::stateChanged( const HudState& s )
{
	if(!document_)return; char b[96];auto tr=[this](const char*key,std::initializer_list<localization::TextArgument>args={}){return textCatalog_.format(LocalizationKey{key},args);}; text("hud_kingdom",s.settlement.kingdomName);text("hud_gnomes",tr("hud.gnomes",{{"count",std::to_string(s.settlement.gnomes)}}));text("hud_animals",tr("hud.animals",{{"count",std::to_string(s.settlement.animals)}}));text("hud_items",tr("hud.items",{{"count",std::to_string(s.settlement.items)}}));
	if(auto* watch=document_->GetElementById("hud_watch_rows")){std::string markup;for(const auto& row:s.watchRows){const auto key=row.id.category.value+"_"+row.id.group.value+"_"+row.id.item.value+"_"+row.id.material.value;markup += "<button class='c-hud-watch-row' data-watch='" + domId("watch_",key) + "'><span class='c-hud-watch-label'>" + Rml::StringUtilities::EncodeRml(row.label) + "</span><strong>" + std::to_string(row.count) + "</strong></button>";}watch->SetInnerRML(markup);}
	if(!s.acceptsWorldActions){mineMenuOpen_=false;agricultureMenuOpen_=false;designationMenuOpen_=false;jobsMenuOpen_=false;buildMenuOpen_=false;kingdomPanelOpen_=false;}
	const bool compactToolWindow = presentation_ == Presentation::OrdersTools;
	const bool showMine = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Mine : mineMenuOpen_ );
	const bool showAgriculture = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Agriculture : agricultureMenuOpen_ );
	const bool showDesignations = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Designations : designationMenuOpen_ );
	const bool showJobs = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Jobs : jobsMenuOpen_ );
	const bool showBuild = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Build : buildMenuOpen_ );
	visible("hud_root",s.acceptsWorldActions);
	visible("hud_orders_tools_header",s.acceptsWorldActions && !compactToolWindow);
	visible("hud_tool_shelf",s.acceptsWorldActions && !compactToolWindow);
	const bool sidebarSubmenuOpen = !compactToolWindow
		&& ( mineMenuOpen_ || agricultureMenuOpen_ || designationMenuOpen_ || jobsMenuOpen_ );
	visible("hud_sidebar_root",s.acceptsWorldActions && !compactToolWindow && !sidebarSubmenuOpen);
	if(auto* shelf=document_->GetElementById("hud_tool_shelf"))shelf->SetClass("is-submenu-open",sidebarSubmenuOpen);
	// The detached Build panel owns its close/cancel affordance; hide the global
	// footer only for that richer workflow, not for an in-sidebar action page.
	const bool fixedToolMenuOpen = !compactToolWindow && buildMenuOpen_;
	const bool showHints = s.acceptsWorldActions && s.tool.active.has_value() && !fixedToolMenuOpen;
	visible("hud_hint_strip", showHints);
	visible("hud_keyboard_commands", showHints);
	visible("hud_tool_rotate", showHints && s.tool.canRotate);
	if ( compactToolWindow ) visible("hud_mine_menu", showMine); else visible("hud_mine_menu",mineMenuOpen_ && s.acceptsWorldActions);
	visible("hud_agriculture_menu",showAgriculture);
	visible("hud_designations_menu",showDesignations);
	visible("hud_jobs_menu",showJobs);
	visible("hud_kingdom_panel",kingdomPanelOpen_ && s.acceptsWorldActions);
	if ( compactToolWindow ) visible("hud_build_panel", showBuild); else visible("hud_build_panel",s.acceptsWorldActions && buildMenuOpen_);
	visible("hud_build_categories",showBuild);
	visible("hud_build_categories_page",showBuild && buildCategoryPage_);
	visible("hud_build_types_page",showBuild && !buildCategoryPage_);
	visible("hud_build_catalog",showBuild && !buildCategoryPage_);
	text("hud_kingdom_panel_name",s.settlement.kingdomName); text("hud_kingdom_panel_gnomes",tr("hud.gnomes",{{"count",std::to_string(s.settlement.gnomes)}})); text("hud_kingdom_panel_animals",tr("hud.animals",{{"count",std::to_string(s.settlement.animals)}})); text("hud_kingdom_panel_items",tr("hud.items",{{"count",std::to_string(s.settlement.items)}}));
	if(auto* panel=document_->GetElementById("hud_build_panel"))
	{
		const bool show=showBuild;
		panel->SetProperty("display",show?"block":"none");
	}
	std::vector<std::string> buildTypes;
	for ( const auto& row : s.buildCatalog )
		if ( std::find( buildTypes.begin(), buildTypes.end(), row.type ) == buildTypes.end() ) buildTypes.push_back( row.type );
	std::sort( buildTypes.begin(), buildTypes.end(), []( const std::string& left, const std::string& right )
	{
		const int leftRank = buildTypeRank( left );
		const int rightRank = buildTypeRank( right );
		return leftRank == rightRank ? left < right : leftRank < rightRank;
	} );
	if ( !buildCategoryPage_ )
	{
		if ( buildTypes.empty() ) selectedBuildType_.clear();
		else if ( std::find( buildTypes.begin(), buildTypes.end(), selectedBuildType_ ) == buildTypes.end() ) selectedBuildType_ = buildTypes.front();
	}
	if ( auto* typeList = document_->GetElementById( "hud_build_type_list" ); typeList && renderedBuildTypes_ != buildTypes )
	{
		std::string typeMarkup;
		for ( const auto& type : buildTypes )
		{
			typeMarkup += "<button id='hud_build_type_" + domId( "", type ) + "' class='c-button c-hud-build-type' data-build-type='" + Rml::StringUtilities::EncodeRml( type ) + "' aria-pressed='false'>" + Rml::StringUtilities::EncodeRml( buildTypeName( type ) ) + "</button>";
		}
		typeList->SetInnerRML( typeMarkup );
		renderedBuildTypes_ = buildTypes;
	}
	for ( const auto& type : buildTypes )
	{
		const auto buttonId = "hud_build_type_" + domId( "", type );
		if ( auto* button = document_->GetElementById( buttonId ) )
		{
			const bool selected = type == selectedBuildType_;
			button->SetClass( "is-selected", selected );
			button->SetAttribute( "aria-pressed", selected ? "true" : "false" );
		}
	}
	if(auto* catalog=document_->GetElementById("hud_build_catalog"); renderedBuildCatalog_ != s.buildCatalog || renderedSelectedBuild_ != selectedBuild_ || renderedSelectedBuildType_ != selectedBuildType_)
	{
		std::string markup = "<header class='c-hud-build-catalog-header'><strong>" + textCatalog_.format( LocalizationKey{ "hud.build.catalog_title" } ) + "</strong><span>" + textCatalog_.format( LocalizationKey{ "hud.build.catalog_hint" } ) + "</span></header><div id='hud_build_items' class='l-hud-build-items'>";
		bool renderedAny = false;
		for(const auto& row:s.buildCatalog)
		{
			if ( !selectedBuildType_.empty() && row.type != selectedBuildType_ ) continue;
			renderedAny = true;
			const auto selected = selectedBuild_ == row.id.value ? " is-selected" : "";
			const auto unavailable = row.available ? "" : " is-unavailable";
			const auto material = row.defaultMaterials.empty() ? std::string{} : row.defaultMaterials.front().value;
			const bool canPlace = row.available || row.kind == BuildKind::Workshop;
			const auto blueprint = !row.available && row.kind == BuildKind::Workshop ? " can-place-blueprint" : "";
			const auto detail = row.available ? material : ( row.unavailableReason.empty() ? textCatalog_.format( LocalizationKey{ "hud.build.unavailable" } ) : row.unavailableReason );
			const auto sprite = buildSpriteMarkup( row );
			const auto icon = sprite.empty() ? "<span class='c-hud-build-glyph'>" + Rml::StringUtilities::EncodeRml( buildGlyph( row ) ) + "</span>" : sprite;
			markup += std::string{ "<article class='c-hud-build-card" } + selected + unavailable + "'><button id='" + domId("hud_build_",row.id.value) + "' class='c-button c-hud-build-row" + selected + unavailable + "' data-build='" + Rml::StringUtilities::EncodeRml(row.id.value) + "' aria-disabled='" + ( canPlace ? "false" : "true" ) + "'" + ( canPlace ? "" : " disabled='disabled'" ) + " aria-label='" + Rml::StringUtilities::EncodeRml(row.name) + "' title='" + Rml::StringUtilities::EncodeRml(detail.empty() ? row.name : row.name + " - " + detail) + "'>" + icon + "<span class='c-hud-build-copy'><strong class='c-hud-build-name'>" + Rml::StringUtilities::EncodeRml(row.name) + "</strong></span></button>";
			if ( !row.components.empty() )
			{
				markup += "<div class='c-hud-build-requirements'>";
				for ( std::size_t componentIndex = 0; componentIndex < row.components.size(); ++componentIndex )
				{
					const auto& component = row.components[componentIndex];
					markup += std::string{ "<label class='c-hud-build-requirement'><span>" } + std::to_string( component.amount ) + " x " + Rml::StringUtilities::EncodeRml( displayName( component.item.value ) ) + "</span>";
					if ( component.options.empty() ) markup += "<small class='c-hud-build-material is-unavailable'>none available</small>";
					else
					{
						markup += "<select id='" + domId( "hud_build_material_", row.id.value + "_" + std::to_string( componentIndex ) ) + "' class='c-hud-build-material-select' data-build='" + Rml::StringUtilities::EncodeRml( row.id.value ) + "' data-build-component='" + std::to_string( componentIndex ) + "'>";
						for ( const auto& option : component.options ) markup += "<option value='" + Rml::StringUtilities::EncodeRml( option.id.value ) + "'" + ( option.id == component.selected ? " selected" : "" ) + ">" + Rml::StringUtilities::EncodeRml( displayName( option.id.value ) ) + " (" + std::to_string( option.available ) + ")</option>";
						markup += "</select>";
					}
					markup += "</label>";
				}
				markup += "</div>";
			}
			const auto encodedId = Rml::StringUtilities::EncodeRml( row.id.value );
			const auto availabilityLabel = textCatalog_.format( LocalizationKey{
				row.available ? "hud.build.available"
					: row.kind == BuildKind::Workshop ? "hud.build.awaiting_resources"
					: "hud.build.cannot_build" } );
			markup += std::string{ "<div class='c-hud-build-availability " } + ( row.available ? "is-available'>" : "is-unavailable'>" ) + Rml::StringUtilities::EncodeRml( availabilityLabel ) + "</div>";
			markup += "<div class='c-hud-build-actions'>";
			if ( row.kind == BuildKind::Terrain ) markup += "<button id='hud_build_action_FillHole_" + domId( "", row.id.value ) + "' class='c-button c-hud-build-action' data-build='" + encodedId + "' data-build-action='FillHole'" + ( row.available ? "" : " disabled='disabled' aria-disabled='true'" ) + ">Fill hole</button><button id='hud_build_action_Replace_" + domId( "", row.id.value ) + "' class='c-button c-hud-build-action' data-build='" + encodedId + "' data-build-action='Replace'" + ( row.available ? "" : " disabled='disabled' aria-disabled='true'" ) + ">Replace</button>";
			const auto buildLabel = textCatalog_.format( LocalizationKey{
				!row.available && row.kind == BuildKind::Workshop
					? "hud.build.action_place_blueprint"
					: "hud.build.action_build" } );
			markup += "<button id='hud_build_action_Build_" + domId( "", row.id.value ) + "' class='c-button c-hud-build-action" + blueprint + "' data-build='" + encodedId + "' data-build-action='Build' aria-label='" + Rml::StringUtilities::EncodeRml( buildLabel ) + "'" + ( canPlace ? " aria-disabled='false'" : " disabled='disabled' aria-disabled='true'" ) + ">" + Rml::StringUtilities::EncodeRml( buildLabel ) + "</button></div></article>";
		}
		if ( !renderedAny ) markup += "<p class='c-hud-build-empty'>" + Rml::StringUtilities::EncodeRml( textCatalog_.format( LocalizationKey{ "hud.build.no_items" } ) ) + "</p>";
		markup += "</div>";
		updatingBuildCatalog_ = true;
		catalog->SetInnerRML(markup);
		updatingBuildCatalog_ = false;
		renderedBuildCatalog_ = s.buildCatalog;
		renderedSelectedBuild_ = selectedBuild_;
		renderedSelectedBuildType_ = selectedBuildType_;
	}
	for(const auto* id:{"hud_build_furniture","hud_build_workshop","hud_build_containers","hud_build_utility","hud_build_wall","hud_build_floor","hud_build_stairs","hud_build_ramps","hud_build_fence"})if(auto* element=document_->GetElementById(id)){const bool selected=selectedBuildCategory_==id;element->SetClass("is-selected",selected);element->SetAttribute("aria-pressed",selected?"true":"false");}
	const auto activeTool=s.tool.active?std::string_view(s.tool.active->value):std::string_view{};
	for(const auto& mode: {std::pair{"hud_mine_walls",std::string_view{"mine"}},std::pair{"hud_mine_explorative",std::string_view{"explorative_mine"}},std::pair{"hud_mine_remove_floor",std::string_view{"remove_floor"}},std::pair{"hud_mine_hole",std::string_view{"dig_hole"}},std::pair{"hud_mine_stairs_down",std::string_view{"dig_stairs_down"}},std::pair{"hud_mine_stairs_up",std::string_view{"mine_stairs_up"}},std::pair{"hud_mine_ramp_down",std::string_view{"dig_ramp_down"}}})if(auto*e=document_->GetElementById(mode.first))e->SetClass("is-selected",activeTool==mode.second);
	const auto pressed = [this]( const char* id, bool active )
	{
		if( auto* element = document_->GetElementById( id ) )
		{
			element->SetClass( "is-selected", active );
			element->SetAttribute( "aria-pressed", active ? "true" : "false" );
		}
	};
	pressed( "hud_pause", s.clock.paused );
	pressed( "hud_speed_normal", s.clock.speed == GameSpeed::Normal );
	pressed( "hud_speed_fast", s.clock.speed == GameSpeed::Fast );
	pressed( "hud_overlay_designations", s.overlays.designations );
	pressed( "hud_overlay_jobs", s.overlays.jobs );
	pressed( "hud_overlay_walls", s.overlays.loweredWalls );
	pressed( "hud_overlay_axles", s.overlays.axles );
	pressed( "hud_tool_deconstruct", activeTool == "deconstruct" );
	std::snprintf(b,sizeof b,"%02u:%02u",s.clock.hour,s.clock.minute);text("hud_clock",b);text("hud_date",tr("hud.date",{{"day",std::to_string(s.clock.day)},{"year",std::to_string(s.clock.year)}}));text("hud_level",tr("hud.level",{{"level",std::to_string(s.camera.viewLevel)}}));text("hud_pause",tr(s.clock.paused?"hud.resume":"hud.pause"));
	text("hud_active_tool",s.tool.active?displayName(s.tool.active->value):tr("hud.inspect"));text("hud_rotate_hint",s.tool.canRotate?tr("hud.rotate_hint"):"");
	const auto statusKey = s.status;
	const auto statusText = statusKey.empty() ? std::string{} : ( textCatalog_.contains( LocalizationKey{ statusKey } ) ? textCatalog_.format( LocalizationKey{ statusKey } ) : statusKey );
	text( "hud_status", statusText );
	visible( "hud_status", !statusText.empty() );
	for( const auto* id : { "hud_tool_inspect", "hud_open_population", "hud_open_inventory", "hud_tool_build", "hud_tool_mine", "hud_tool_agriculture", "hud_pause", "hud_speed_normal", "hud_level_down", "hud_level_up", "tutorial-finish" } )
	{
		bool highlighted = false;
		if( s.tutorial.hintsEnabled ) for( const auto& target : s.tutorial.highlightedIds ) if( target == id ) { highlighted = true; break; }
		if( auto* element = document_->GetElementById( id ) ) element->SetClass( "tutorial-target", highlighted );
	}
	visible( "hud_tutorial_panel", s.tutorial.active || s.tutorial.completed || s.tutorial.incompatible );
	const auto tutorialText = [this]( const std::string& value ) { const LocalizationKey key{ value }; return textCatalog_.contains( key ) ? textCatalog_.format( key ) : value; };
	text( "tutorial_title", tutorialText( s.tutorial.title ) ); text( "tutorial_explanation", tutorialText( s.tutorial.explanation ) ); text( "tutorial_objective", tutorialText( s.tutorial.objective ) ); text( "tutorial_progress", s.tutorial.progress );
	visible( "tutorial_explanation", s.tutorial.hintsEnabled || s.tutorial.incompatible );
	std::string tutorialInstructions;
	for( std::size_t index = 0; index < s.tutorial.steps.size(); ++index )
	{
		const bool completed = index < s.tutorial.completedSteps.size() && s.tutorial.completedSteps[index];
		tutorialInstructions += completed ? "[x] " : "[ ] ";
		tutorialInstructions += std::to_string( index + 1 ) + ". " + tutorialText( s.tutorial.steps[index] );
		if( index + 1 < s.tutorial.steps.size() ) tutorialInstructions += "\n";
	}
	text( "tutorial_steps", tutorialInstructions );
	visible( "tutorial_steps", s.tutorial.hintsEnabled || s.tutorial.incompatible );
	text( "tutorial-hints", tutorialText( s.tutorial.hintsEnabled ? "tutorial.action.hide_hints" : "tutorial.action.show_hints" ) );
	static const char* const tutorialSteps[] = { "tutorial.step.orientation", "tutorial.step.inspect_assign", "tutorial.step.shelter_storage", "tutorial.step.mining_levels", "tutorial.step.farming", "tutorial.step.crafting", "tutorial.step.cooking", "tutorial.step.population", "tutorial.step.graduation" }; constexpr std::size_t tutorialStepCount = 9;
	std::string checklist; std::string skipped;
	for( std::size_t i = 0; i < tutorialStepCount; ++i )
	{
		const auto label = tutorialText( tutorialSteps[i] );
		const auto bit = 1u << i;
		const char* marker = ( s.tutorial.completedMask & bit ) ? "[x] " : ( s.tutorial.skippedMask & bit ) ? "[-] " : ( i == s.tutorial.step ? "> " : "[ ] " );
		checklist += marker + label + ( i + 1 == tutorialStepCount ? "" : "\n" );
		if( s.tutorial.skippedMask & bit ) skipped += ( skipped.empty() ? "" : ", " ) + label;
	}
	text( "tutorial_checklist", checklist );
	const bool showSkippedWarning = s.tutorial.step == s.tutorial.stepCount - 1 && !skipped.empty();
	text( "tutorial_warning", showSkippedWarning ? "Skipped lessons: " + skipped : "" ); visible( "tutorial_warning", showSkippedWarning );
	visible( "tutorial-continue", s.tutorial.active && ( ( s.tutorial.completedMask & ( 1u << s.tutorial.step ) ) != 0 || ( s.tutorial.skippedMask & ( 1u << s.tutorial.step ) ) != 0 ) );
	visible( "tutorial-finish", s.tutorial.active && s.tutorial.step == s.tutorial.stepCount - 1 );
	if( auto* panel = document_->GetElementById( "hud_tutorial_panel" ) ) panel->SetClass( "is-complete", s.tutorial.completed );
	const bool prompt=!s.prompts.empty();visible("hud_event_blocker",prompt);if(auto*blocker=document_->GetElementById("hud_event_blocker"))blocker->SetAttribute("aria-hidden",prompt?"false":"true");if(prompt){const auto&p=s.prompts.front();text("hud_event_title",p.title);text("hud_event_body",p.body);visible("hud_event_ack",p.responses==EventResponseKind::Acknowledge);visible("hud_event_yes",p.responses==EventResponseKind::YesNo);visible("hud_event_no",p.responses==EventResponseKind::YesNo);if(!focusedPrompt_||focusedPrompt_->value!=p.instanceId.value){const char*focusId=p.responses==EventResponseKind::Acknowledge?"hud_event_ack":"hud_event_yes";if(auto*button=document_->GetElementById(focusId))button->Focus(true);focusedPrompt_=p.instanceId;}}else focusedPrompt_.reset();
}
}
