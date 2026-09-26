/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "HudRmlBinding.h"
#include "../../localization/RmlText.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>
namespace ingnomia::ui::hud
{
namespace
{
std::string esc( const std::string& value ) { return Rml::StringUtilities::EncodeRml( value ); }

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
}

// Status bar messages (PDF p.289): a present-tense sentence about the control under the pointer.
struct StatusText { const char* id; const char* text; const char* unavailable; };
constexpr StatusText statusTexts[] = {
	{ "hud_pause", "Pauses or resumes the game.", nullptr },
	{ "hud_speed_normal", "Runs the game at normal speed.", nullptr },
	{ "hud_speed_fast", "Runs the game at fast speed.", nullptr },
	{ "hud_level_down", "Shows the level below.", nullptr },
	{ "hud_level_up", "Shows the level above.", nullptr },
	{ "hud_open_inventory", "Opens the inventory of stored items.", nullptr },
	{ "hud_open_population", "Opens the citizens, their skills, professions and schedules.", nullptr },
	{ "hud_open_military", "Opens squads, roles, uniforms and target priorities.", nullptr },
	{ "hud_open_diplomacy", "Opens neighboring kingdoms and missions.", nullptr },
	{ "hud_tool_inspect", "Inspects the tile you click on the map.", nullptr },
	{ "hud_tool_build", "Opens the Build window to place constructions, furniture and workshops.", nullptr },
	{ "hud_tool_deconstruct", "Removes the constructed object you click on the map.", nullptr },
	{ "hud_tool_mine", "Shows the mining orders.", nullptr },
	{ "hud_tool_agriculture", "Shows the tree and plant orders.", nullptr },
	{ "hud_tool_designations", "Shows the area designations.", nullptr },
	{ "hud_tool_jobs", "Shows the job commands.", nullptr },
	{ "hud_tool_view", "Shows or hides map overlays.", nullptr },
	{ "hud_tool_cancel", "Cancels the current tool.", "Unavailable because no tool is active." },
	{ "hud_tool_rotate", "Rotates the object being placed.", "Unavailable because the current tool places nothing that turns." },
	{ "hud_whats_this", "Explains the next item you click in the game window.", nullptr },
	{ "hud_mine_walls", "Marks walls for mining.", nullptr },
	{ "hud_mine_explorative", "Marks an area for exploratory mining.", nullptr },
	{ "hud_mine_remove_floor", "Removes the floor from the selected area.", nullptr },
	{ "hud_mine_hole", "Digs a hole through the selected floor.", nullptr },
	{ "hud_mine_stairs_down", "Digs stairs leading down.", nullptr },
	{ "hud_mine_stairs_up", "Mines stairs leading up.", nullptr },
	{ "hud_mine_ramp_down", "Digs a ramp leading down.", nullptr },
	{ "hud_tool_fell_tree", "Marks trees to be cut down.", nullptr },
	{ "hud_tool_plant_tree", "Marks an area for planting trees.", nullptr },
	{ "hud_tool_harvest_tree", "Marks trees to harvest their products.", nullptr },
	{ "hud_tool_forage", "Gathers food and materials from wild plants.", nullptr },
	{ "hud_tool_remove_plant", "Removes plants from the selected area.", nullptr },
	{ "hud_tool_stockpile", "Designates an area for storing items.", nullptr },
	{ "hud_tool_farm", "Designates an area for growing crops.", nullptr },
	{ "hud_tool_grove", "Designates an area for growing and harvesting trees.", nullptr },
	{ "hud_tool_pasture", "Designates an area for keeping animals.", nullptr },
	{ "hud_tool_personal_room", "Designates a room for one resident.", nullptr },
	{ "hud_tool_dormitory", "Designates a shared sleeping area.", nullptr },
	{ "hud_tool_dining_hall", "Designates an area for dining.", nullptr },
	{ "hud_tool_hospital", "Designates an area for medical care.", nullptr },
	{ "hud_tool_forbidden", "Blocks access to the selected area.", nullptr },
	{ "hud_tool_remove_designation", "Clears an existing area designation.", nullptr },
	{ "hud_tool_suspend_job", "Pauses the job you click without removing it.", nullptr },
	{ "hud_tool_resume_job", "Resumes the suspended job you click.", nullptr },
	{ "hud_tool_cancel_job", "Cancels the job you click.", nullptr },
	{ "hud_tool_raise_priority", "Moves the job you click higher in the work queue.", nullptr },
	{ "hud_tool_lower_priority", "Moves the job you click lower in the work queue.", nullptr },
	{ "hud_overlay_designations", "Shows or hides designation markers.", nullptr },
	{ "hud_overlay_jobs", "Shows or hides job markers.", nullptr },
	{ "hud_overlay_walls", "Shows walls lowered for easier map viewing.", nullptr },
	{ "hud_overlay_axles", "Shows powered mechanical axles.", nullptr },
};

// Menu items and the tool each one arms, grouped by the toolbar menu button that opens them.
struct MenuTool { const char* id; const char* tool; const char* label; };
constexpr MenuTool mineTools[] = { { "hud_mine_walls", "mine", "Mine Walls" }, { "hud_mine_explorative", "explorative_mine", "Explorative Mine" }, { "hud_mine_remove_floor", "remove_floor", "Remove Floor" }, { "hud_mine_hole", "dig_hole", "Dig Hole" },
	{ "hud_mine_stairs_down", "dig_stairs_down", "Dig Stairs Down" }, { "hud_mine_stairs_up", "mine_stairs_up", "Mine Stairs Up" }, { "hud_mine_ramp_down", "dig_ramp_down", "Dig Ramp Down" } };
constexpr MenuTool agricultureTools[] = { { "hud_tool_fell_tree", "fell_tree", "Cut Tree" }, { "hud_tool_plant_tree", "plant_tree", "Plant Tree" }, { "hud_tool_harvest_tree", "harvest_tree", "Harvest Tree" }, { "hud_tool_forage", "forage", "Forage" }, { "hud_tool_remove_plant", "remove_plant", "Remove Plant" } };
constexpr MenuTool designationTools[] = { { "hud_tool_stockpile", "create_stockpile", "Stockpile" }, { "hud_tool_farm", "create_farm", "Farm" }, { "hud_tool_grove", "create_grove", "Grove" }, { "hud_tool_pasture", "create_pasture", "Pasture" },
	{ "hud_tool_personal_room", "create_personal_room", "Personal Room" }, { "hud_tool_dormitory", "create_dormitory", "Dormitory" }, { "hud_tool_dining_hall", "create_dining_hall", "Dining Hall" }, { "hud_tool_hospital", "create_hospital", "Hospital" },
	{ "hud_tool_forbidden", "create_forbidden_area", "Forbidden Area" }, { "hud_tool_remove_designation", "remove_designation", "Remove Designation" } };
constexpr MenuTool jobTools[] = { { "hud_tool_suspend_job", "suspend_job", "Suspend Job" }, { "hud_tool_resume_job", "resume_job", "Resume Job" }, { "hud_tool_cancel_job", "cancel_job", "Cancel Job" },
	{ "hud_tool_raise_priority", "raise_job_priority", "Raise Priority" }, { "hud_tool_lower_priority", "lower_job_priority", "Lower Priority" } };
template <std::size_t N> bool armsOneOf( const MenuTool ( &tools )[N], std::string_view active )
{
	return std::any_of( std::begin( tools ), std::end( tools ), [active]( const MenuTool& t ) { return active == t.tool; } );
}
template <std::size_t N> const char* labelOf( const MenuTool ( &tools )[N], std::string_view active )
{
	for( const auto& t : tools ) if( active == t.tool ) return t.label;
	return nullptr;
}
} // namespace

void HudRmlBinding::Callback::ProcessEvent( Rml::Event& event ) { if( eventFn_ ) eventFn_( event ); else if( fn_ ) fn_(); }
HudRmlBinding::HudRmlBinding( Rml::Context& context, Presentation presentation, ToolPanel toolPanel ) : context_( context ), presentation_( presentation ), toolPanel_( toolPanel ) {}
HudRmlBinding::~HudRmlBinding() { shutdown(); }

bool HudRmlBinding::initialize( HudController& controller )
{
	controller_ = &controller;
	const char* documentPath = presentation_ == Presentation::OrdersTools ? "screens/orders_tools.rml" : "screens/game_hud.rml";
	document_ = documentLoader_ ? documentLoader_( documentPath ) : context_.LoadDocument( documentPath );
	if( !document_ ) return false;
	localization::applyRmlText( *document_, textCatalog_ );
	const auto arm = [this]( const char* tool ) { closeMenus(); controller_->activateTool( ToolId{ tool } ); stateChanged( controller_->state() ); };

	// Speed and level.
	bind( "hud_pause", [this] { if( openPause_ ) openPause_(); else controller_->setPaused( !controller_->state().clock.paused ); } );
	bind( "hud_speed_normal", [this] { controller_->setSpeed( GameSpeed::Normal ); } );
	bind( "hud_speed_fast", [this] { controller_->setSpeed( GameSpeed::Fast ); } );
	bind( "hud_level_down", [this] { controller_->changeLevel( controller_->state().camera.viewLevel - 1 ); } );
	bind( "hud_level_up", [this] { controller_->changeLevel( controller_->state().camera.viewLevel + 1 ); } );

	// Windows.
	bind( "hud_open_population", [this] { closeMenus(); if( openPopulation_ ) openPopulation_( FocusToken{ 1 } ); } );
	bind( "hud_open_inventory", [this] { closeMenus(); if( openInventory_ ) openInventory_( FocusToken{ 2 } ); } );
	bind( "hud_open_military", [this] { closeMenus(); if( openMilitary_ ) openMilitary_( FocusToken{ 3 } ); } );
	bind( "hud_open_diplomacy", [this] { closeMenus(); if( openDiplomacy_ ) openDiplomacy_( FocusToken{ 4 } ); } );

	// Tools. Menu buttons open their drop-down menu; a menu item arms its tool and closes the menu.
	bind( "hud_tool_inspect", [this] { closeMenus(); buildMenuOpen_ = false; controller_->cancelTool(); if( inspect_ ) inspect_(); stateChanged( controller_->state() ); } );
	bind( "hud_tool_mine", [this] { if( presentation_ == Presentation::Full ) toggleActionMenu( "mine" ); else if( openOrdersTools_ ) openOrdersTools_( "hud_tool_mine" ); } );
	bind( "hud_tool_agriculture", [this] { if( presentation_ == Presentation::Full ) toggleActionMenu( "agriculture" ); else if( openOrdersTools_ ) openOrdersTools_( "hud_tool_agriculture" ); } );
	bind( "hud_tool_designations", [this] { if( presentation_ == Presentation::Full ) toggleActionMenu( "designations" ); else if( openOrdersTools_ ) openOrdersTools_( "hud_tool_designations" ); } );
	bind( "hud_tool_jobs", [this] { if( presentation_ == Presentation::Full ) toggleActionMenu( "jobs" ); else if( openOrdersTools_ ) openOrdersTools_( "hud_tool_jobs" ); } );
	bind( "hud_tool_view", [this] { toggleActionMenu( "view" ); } );
	bind( "hud_whats_this", [this] { if ( whatsThis_ ) whatsThis_(); } );
	bind( "hud_tool_build", [this]
	{
		closeMenus();
		if( openOrdersTools_ ) { openOrdersTools_( "hud_tool_build" ); return; }
		buildMenuOpen_ = presentation_ == Presentation::OrdersTools ? true : !buildMenuOpen_;
		selectedBuildCategory_.clear(); selectedBuildType_.clear(); selectedBuild_.clear(); buildCategoryPage_ = true;
		controller_->closeBuildMenu();
		stateChanged( controller_->state() );
	} );
	bind( "hud_tool_deconstruct", [arm] { arm( "deconstruct" ); } );
	for( const auto& t : mineTools ) bind( t.id, [arm, tool = t.tool] { arm( tool ); } );
	for( const auto& t : agricultureTools ) bind( t.id, [arm, tool = t.tool] { arm( tool ); } );
	for( const auto& t : designationTools ) bind( t.id, [arm, tool = t.tool] { arm( tool ); } );
	for( const auto& t : jobTools ) bind( t.id, [arm, tool = t.tool] { arm( tool ); } );
	bind( "hud_tool_cancel", [this] { closeMenus(); buildMenuOpen_ = false; controller_->closeBuildMenu(); controller_->cancelTool(); if( inspect_ ) inspect_(); stateChanged( controller_->state() ); } );
	bind( "hud_tool_rotate", [this] { controller_->rotateTool(); } );

	// View menu: independent settings with check marks.
	const auto overlay = [this]( OverlayKind kind, bool on ) { closeMenus(); controller_->setOverlay( kind, on ); stateChanged( controller_->state() ); };
	bind( "hud_overlay_designations", [this, overlay] { overlay( OverlayKind::Designations, !controller_->state().overlays.designations ); } );
	bind( "hud_overlay_jobs", [this, overlay] { overlay( OverlayKind::Jobs, !controller_->state().overlays.jobs ); } );
	bind( "hud_overlay_walls", [this, overlay] { overlay( OverlayKind::LoweredWalls, !controller_->state().overlays.loweredWalls ); } );
	bind( "hud_overlay_axles", [this, overlay] { overlay( OverlayKind::Axles, !controller_->state().overlays.axles ); } );

	// Status bar messages for every control that has one.
	for( const auto& entry : statusTexts ) bindStatusMessage( entry.id );

	// Esc closes an open menu first (PDF p.48).
	bindEvent( "hud_root", "keydown", [this]( Rml::Event& event )
	{
		if( event.GetParameter<int>( "key_identifier", 0 ) != Rml::Input::KI_ESCAPE || openMenu_.empty() ) return;
		closeMenus();
		stateChanged( controller_->state() );
		event.StopPropagation();
	} );

	// Tutorial and events.
	bind( "tutorial-continue", [this] { controller_->tutorialAdvance(); } );
	bind( "tutorial-skip", [this] { controller_->tutorialSkip(); } );
	bind( "tutorial-restart", [this] { controller_->tutorialRestart(); } );
	bind( "tutorial-hints", [this] { controller_->tutorialToggleHints(); } );
	bind( "tutorial-finish", [this] { controller_->tutorialFinish(); } );
	bind( "hud_event_ack", [this] { controller_->respondToPrompt( EventResponse::Acknowledge ); } );
	bind( "hud_event_yes", [this] { controller_->respondToPrompt( EventResponse::Yes ); } );
	bind( "hud_event_no", [this] { controller_->respondToPrompt( EventResponse::No ); } );

	// Build window: category list box, material drop-down, item list view, and the item's commands.
	bind( "hud_build_close", [this]
	{
		const auto active = controller_->state().tool.active;
		const bool buildActive = active && active->value == "build";
		buildMenuOpen_ = false; buildCategoryPage_ = true; selectedBuildCategory_.clear(); selectedBuildType_.clear(); selectedBuild_.clear();
		controller_->closeBuildMenu();
		if( buildActive ) controller_->cancelTool();
		stateChanged( controller_->state() );
		if( presentation_ == Presentation::OrdersTools && close_ ) close_();
	} );
	const struct { const char* id; BuildSelection selection; } categories[] = {
		{ "hud_build_furniture", BuildSelection::Furniture }, { "hud_build_workshop", BuildSelection::Workshop }, { "hud_build_containers", BuildSelection::Containers },
		{ "hud_build_utility", BuildSelection::Utility }, { "hud_build_wall", BuildSelection::Wall }, { "hud_build_floor", BuildSelection::Floor },
		{ "hud_build_stairs", BuildSelection::Stairs }, { "hud_build_ramps", BuildSelection::Ramps }, { "hud_build_fence", BuildSelection::Fence } };
	for( const auto& category : categories ) bind( category.id, [this, id = category.id, selection = category.selection] { selectBuildCategory( id, selection, {} ); } );
	bindEvent( "hud_build_type_list", "change", [this]( Rml::Event& event )
	{
		if( updatingBuildCatalog_ ) return;
		auto* select = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetTargetElement() );
		if( !select ) return;
		const std::string type = select->GetValue();
		if( type.empty() || type == selectedBuildType_ ) return;
		selectedBuildType_ = type;
		selectedBuild_.clear();
		stateChanged( controller_->state() );
	} );
	bindEvent( "hud_build_catalog", "click", [this]( Rml::Event& event )
	{
		if( updatingBuildCatalog_ ) { event.StopPropagation(); return; }
		for( auto* element = event.GetTargetElement(); element && element != event.GetCurrentElement(); element = element->GetParentNode() )
		{
			if( !element->GetAttribute<Rml::String>( "data-build-component", "" ).empty() ) { event.StopPropagation(); return; }
			const auto id = element->GetAttribute<Rml::String>( "data-build", "" );
			if( id.empty() ) continue;
			if( element->HasAttribute( "disabled" ) ) { event.StopPropagation(); return; }
			const auto action = element->GetAttribute<Rml::String>( "data-build-action", "" );
			selectedBuild_ = id;
			if( !action.empty() )
			{
				const auto buildAction = action == "FillHole" ? BuildAction::FillHole : action == "Replace" ? BuildAction::Replace : BuildAction::Build;
				controller_->chooseBuildAction( CatalogId{ id }, buildAction );
			}
			event.StopPropagation();
			stateChanged( controller_->state() );
			break;
		}
	} );
	bindEvent( "hud_build_catalog", "change", [this]( Rml::Event& event )
	{
		if( updatingBuildCatalog_ ) { event.StopPropagation(); return; }
		auto* element = event.GetTargetElement();
		if( !element ) return;
		const auto id = element->GetAttribute<Rml::String>( "data-build", "" );
		const auto component = element->GetAttribute<Rml::String>( "data-build-component", "" );
		auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( element );
		if( id.empty() || component.empty() || !control ) return;
		try
		{
			controller_->selectBuildMaterial( CatalogId{ id }, static_cast<std::uint32_t>( std::stoul( std::string( component ) ) ), CatalogId{ control->GetValue() }, false );
			event.StopPropagation();
		}
		catch( ... ) {}
	} );
	bindEvent( "hud_watch_rows", "click", [this]( Rml::Event& event )
	{
		for( auto* element = event.GetTargetElement(); element && element != event.GetCurrentElement(); element = element->GetParentNode() )
			if( !element->GetAttribute<Rml::String>( "data-watch", "" ).empty() ) { if( openInventory_ ) openInventory_( FocusToken{ 2 } ); event.StopPropagation(); break; }
	} );
	bind( "hud_tools_close", [this] { if( close_ ) close_(); } );
	stateChanged( controller.state() );
	// The game window's HUD is an overlay and must not take the keyboard focus from the screen the player is using;
	// the Build window takes the focus in its own window.
	if( presentation_ == Presentation::OrdersTools ) document_->Show(); else document_->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
	return true;
}

bool HudRmlBinding::reloadDocument()
{
	auto* controller = controller_;
	if( !controller ) return false;
	shutdown();
	return initialize( *controller );
}
void HudRmlBinding::setWorkbenchHandlers( WorkbenchHandler population, WorkbenchHandler inventory, WorkbenchHandler military, WorkbenchHandler diplomacy )
{
	openPopulation_ = std::move( population ); openInventory_ = std::move( inventory ); openMilitary_ = std::move( military ); openDiplomacy_ = std::move( diplomacy );
}
void HudRmlBinding::setOrdersToolsHandler( OrdersToolsHandler handler ) { openOrdersTools_ = std::move( handler ); }
void HudRmlBinding::setToolPanel( ToolPanel panel )
{
	if( toolPanel_ == panel ) return;
	toolPanel_ = panel;
	if( document_ && controller_ ) stateChanged( controller_->state() );
}
void HudRmlBinding::setCloseHandler( CloseHandler handler ) { close_ = std::move( handler ); }
void HudRmlBinding::setDocumentLoader( DocumentLoader loader ) { documentLoader_ = std::move( loader ); }
void HudRmlBinding::setPauseHandler( PauseHandler pause ) { openPause_ = std::move( pause ); }

// A toolbar menu button opens its drop-down menu under itself; pressing it again, or Esc, closes it.
void HudRmlBinding::toggleActionMenu( std::string_view menu )
{
	const bool open = openMenu_ != menu;
	closeMenus();
	if( open )
	{
		openMenu_ = std::string( menu );
		const auto buttonId = "hud_tool_" + openMenu_;
		const auto menuId = "hud_" + openMenu_ + "_menu";
		auto* button = document_->GetElementById( buttonId );
		auto* popup = document_->GetElementById( menuId );
		if( button && popup )
		{
			// Absolute offsets are rendered pixels; px keeps the context density from being applied twice.
			const auto offset = button->GetAbsoluteOffset( Rml::BoxArea::Border );
			const auto size = button->GetBox().GetSize( Rml::BoxArea::Border );
			popup->SetProperty( "left", std::to_string( static_cast<int>( offset.x ) ) + "px" );
			popup->SetProperty( "top", std::to_string( static_cast<int>( offset.y + size.y ) ) + "px" );
		}
	}
	if( controller_ ) stateChanged( controller_->state() );
}
void HudRmlBinding::closeMenus() { openMenu_.clear(); }

void HudRmlBinding::bindStatusMessage( const char* id )
{
	auto* element = document_ ? document_->GetElementById( id ) : nullptr;
	if( !element ) return;
	for( const char* event : { "mouseover", "focus" } )
	{
		auto show = std::make_unique<Callback>( [this, id]( Rml::Event& ) { hoverStatus_ = id; if( controller_ ) renderStatus( controller_->state() ); } );
		element->AddEventListener( event, show.get() );
		listenerTargets_.emplace_back( element, event );
		callbacks_.push_back( std::move( show ) );
	}
	for( const char* event : { "mouseout", "blur" } )
	{
		auto hide = std::make_unique<Callback>( [this, id]( Rml::Event& ) { if( hoverStatus_ == id ) hoverStatus_.clear(); if( controller_ ) renderStatus( controller_->state() ); } );
		element->AddEventListener( event, hide.get() );
		listenerTargets_.emplace_back( element, event );
		callbacks_.push_back( std::move( hide ) );
	}
}

std::string HudRmlBinding::statusMessage( const HudState& s ) const
{
	if( !hoverStatus_.empty() )
		for( const auto& entry : statusTexts )
			if( hoverStatus_ == entry.id )
			{
				auto* element = document_ ? document_->GetElementById( entry.id ) : nullptr;
				const bool unavailable = element && element->HasAttribute( "disabled" );
				return std::string( entry.text ) + ( unavailable && entry.unavailable ? std::string( " " ) + entry.unavailable : std::string{} );
			}
	if( s.tool.active )
		return "Click or drag on the map to use the tool." + std::string( s.tool.canRotate ? " Rotate turns the object." : "" ) + " Right-click or Esc cancels.";
	if( !s.status.empty() ) return textCatalog_.contains( LocalizationKey{ s.status } ) ? textCatalog_.format( LocalizationKey{ s.status } ) : s.status;
	return "Ready";
}
void HudRmlBinding::renderStatus( const HudState& s ) { text( "hud_status", statusMessage( s ) ); }

void HudRmlBinding::selectBuildCategory( const char* elementId, BuildSelection selection, std::string_view category )
{
	selectedBuildCategory_ = elementId; selectedBuildType_.clear(); selectedBuild_.clear(); buildCategoryPage_ = false; buildMenuOpen_ = true;
	controller_->requestBuildItems( selection, category );
	stateChanged( controller_->state() );
}
void HudRmlBinding::backToBuildCategories()
{
	buildCategoryPage_ = true; selectedBuildCategory_.clear(); selectedBuildType_.clear(); selectedBuild_.clear();
	controller_->closeBuildMenu();
	stateChanged( controller_->state() );
}
void HudRmlBinding::restoreWorkbenchFocus( FocusToken token )
{
	const char* id = token.value == 1 ? "hud_open_population" : token.value == 2 ? "hud_open_inventory" : token.value == 3 ? "hud_open_military" : token.value == 4 ? "hud_open_diplomacy" : nullptr;
	if( id ) if( auto* element = document_->GetElementById( id ) ) element->Focus();
}
bool HudRmlBinding::activateElement( std::string_view id )
{
	if( !document_ ) return false;
	auto* element = document_->GetElementById( std::string( id ) );
	if( !element ) return false;
	element->DispatchEvent( "click", Rml::Dictionary{} );
	return true;
}
void HudRmlBinding::shutdown()
{
	for( std::size_t i = 0; i < callbacks_.size() && i < listenerTargets_.size(); ++i ) listenerTargets_[i].first->RemoveEventListener( listenerTargets_[i].second, callbacks_[i].get() );
	listenerTargets_.clear();
	callbacks_.clear();
	if( document_ ) { context_.UnloadDocument( document_ ); document_ = nullptr; }
	// A reloaded document starts empty, so nothing counts as rendered any more.
	renderedBuildTypes_.clear(); renderedBuildCatalog_.clear(); renderedBuildList_.clear(); renderedBuildDetails_.clear(); renderedWatch_.clear();
	renderedSelectedBuild_.clear(); renderedSelectedBuildType_.clear(); openMenu_.clear(); hoverStatus_.clear(); focusedPrompt_.reset();
	controller_ = nullptr;
}
void HudRmlBinding::bind( const char* id, std::function<void()> callback )
{
	if( auto* e = document_->GetElementById( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( callback ) );
		e->AddEventListener( Rml::EventId::Click, cb.get() );
		listenerTargets_.emplace_back( e, "click" );
		callbacks_.push_back( std::move( cb ) );
	}
}
void HudRmlBinding::bindEvent( const char* id, const char* event, std::function<void( Rml::Event& )> callback )
{
	if( auto* e = document_->GetElementById( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( callback ) );
		e->AddEventListener( event, cb.get() );
		listenerTargets_.emplace_back( e, event );
		callbacks_.push_back( std::move( cb ) );
	}
}
void HudRmlBinding::text( const char* id, const std::string& value ) { if( auto* e = document_->GetElementById( id ) ) e->SetInnerRML( esc( value ) ); }
void HudRmlBinding::visible( const char* id, bool value ) { if( auto* e = document_->GetElementById( id ) ) e->SetClass( "is-hidden", !value ); }
void HudRmlBinding::setInspectionActive( bool active )
{
	inspectionActive_ = active;
	if( !document_ ) return;
	document_->SetClass( "is-inspecting", active );
	if( auto* button = document_->GetElementById( "hud_tool_inspect" ) )
	{
		button->SetClass( "is-selected", active );
		button->SetAttribute( "aria-pressed", active ? "true" : "false" );
	}
}

// Build window: categories in a list box, a Material drop-down for the category's types, a list view of the
// items (Item / Status), and a group for the chosen item with its material drop-downs and commands.
void HudRmlBinding::renderBuild( const HudState& s, bool showBuild )
{
	std::vector<std::string> buildTypes;
	for( const auto& row : s.buildCatalog )
		if( std::find( buildTypes.begin(), buildTypes.end(), row.type ) == buildTypes.end() ) buildTypes.push_back( row.type );
	std::sort( buildTypes.begin(), buildTypes.end(), []( const std::string& left, const std::string& right )
	{
		const int l = buildTypeRank( left ), r = buildTypeRank( right );
		return l == r ? left < right : l < r;
	} );
	if( !buildCategoryPage_ )
	{
		if( buildTypes.empty() ) selectedBuildType_.clear();
		else if( std::find( buildTypes.begin(), buildTypes.end(), selectedBuildType_ ) == buildTypes.end() ) selectedBuildType_ = buildTypes.front();
	}
	visible( "hud_build_hint", showBuild && buildCategoryPage_ );
	visible( "hud_build_types_page", showBuild && !buildCategoryPage_ && buildTypes.size() > 1 );
	visible( "hud_build_catalog", showBuild && !buildCategoryPage_ );
	visible( "hud_build_categories", showBuild );
	visible( "hud_build_categories_page", showBuild );
	// Lists are rebuilt only when their content changes, never for a selection: a click must not destroy the
	// element it is being dispatched to. Selection is shown by restyling the rows in place.
	updatingBuildCatalog_ = true;
	if( auto* typeList = document_->GetElementById( "hud_build_type_list" ); typeList && renderedBuildTypes_ != buildTypes )
	{
		std::string markup = "<select id='hud_build_type_select' class='w98-select'>";
		for( const auto& type : buildTypes ) markup += "<option value='" + esc( type ) + "'>" + esc( buildTypeName( type ) ) + "</option>";
		markup += "</select>";
		typeList->SetInnerRML( markup );
		renderedBuildTypes_ = buildTypes;
	}
	if( auto* select = rmlui_dynamic_cast<Rml::ElementFormControl*>( document_->GetElementById( "hud_build_type_select" ) ); select && select->GetValue() != selectedBuildType_ )
		select->SetValue( selectedBuildType_ );
	std::vector<const BuildCatalogRow*> rows;
	for( const auto& row : s.buildCatalog )
		if( selectedBuildType_.empty() || row.type == selectedBuildType_ ) rows.push_back( &row );
	if( !rows.empty() && std::none_of( rows.begin(), rows.end(), [this]( const BuildCatalogRow* row ) { return row->id.value == selectedBuild_; } ) )
		selectedBuild_ = rows.front()->id.value;
	std::string list;
	const BuildCatalogRow* chosen = nullptr;
	for( const auto* row : rows )
	{
		if( row->id.value == selectedBuild_ ) chosen = row;
		list += "<button id='" + domId( "hud_build_", row->id.value ) + "' class='w98-list-item c-hud-build-row" + ( row->available ? "" : " is-unavailable" )
			+ "' role='option' data-build='" + esc( row->id.value ) + "'><span class='w98-cell w98-cell--grow'>" + esc( row->name )
			+ "</span><span class='w98-cell l-hud-build-status'>" + ( row->available ? "Ready" : "Needs items" ) + "</span></button>";
	}
	if( rows.empty() ) list += "<p class='w98-list-empty'>No items in this category.</p>";
	if( auto* items = document_->GetElementById( "hud_build_items" ); items && list != renderedBuildList_ )
	{
		items->SetInnerRML( list );
		renderedBuildList_ = list;
	}
	for( const auto* row : rows )
		if( auto* element = document_->GetElementById( domId( "hud_build_", row->id.value ) ) )
		{
			const bool selected = row == chosen;
			element->SetClass( "is-selected", selected );
			element->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
	std::string details;
	if( chosen )
	{
		const auto id = esc( chosen->id.value );
		details += "<div class='w98-group'><span class='w98-group__title'>" + esc( chosen->name ) + "</span>";
		if( !chosen->available )
			details += "<p class='w98-text'>Missing: " + esc( chosen->unavailableReason.empty() ? std::string( "materials" ) : chosen->unavailableReason ) + ". A blueprint waits for them.</p>";
		for( std::size_t index = 0; index < chosen->components.size(); ++index )
		{
			const auto& component = chosen->components[index];
			details += "<div class='w98-line'><span class='w98-label w98-label--wide'>" + std::to_string( component.amount ) + " x " + esc( displayName( component.item.value ) ) + ":</span>";
			if( component.options.empty() ) details += "<p class='w98-text w98-grow'>None available</p>";
			else
			{
				details += "<select id='" + domId( "hud_build_material_", chosen->id.value + "_" + std::to_string( index ) ) + "' class='w98-select' data-build='" + id + "' data-build-component='" + std::to_string( index ) + "'>";
				for( const auto& option : component.options )
					details += "<option value='" + esc( option.id.value ) + "'" + ( option.id == component.selected ? " selected" : "" ) + ">" + esc( displayName( option.id.value ) ) + " (" + std::to_string( option.available ) + ")</option>";
				details += "</select>";
			}
			details += "</div>";
		}
		details += "<div class='w98-line w98-line--end c-hud-build-actions'>";
		if( chosen->kind == BuildKind::Terrain )
		{
			const auto off = chosen->available ? std::string{} : std::string( " disabled='disabled' aria-disabled='true'" );
			details += "<button id='hud_build_action_FillHole_" + domId( "", chosen->id.value ) + "' class='w98-button' data-build='" + id + "' data-build-action='FillHole'" + off + ">Fill Hole</button>";
			details += "<button id='hud_build_action_Replace_" + domId( "", chosen->id.value ) + "' class='w98-button' data-build='" + id + "' data-build-action='Replace'" + off + ">Replace</button>";
		}
		details += "<button id='hud_build_action_Build_" + domId( "", chosen->id.value ) + "' class='w98-button w98-button--default' data-build='" + id + "' data-build-action='Build'>"
			+ std::string( chosen->available ? "Build" : "Place Blueprint" ) + "</button></div></div>";
	}
	// A material choice updates the catalog without a notification, so the details are not rebuilt under the
	// drop-down the player is using; any other change to the chosen item's details rebuilds them.
	if( auto* container = document_->GetElementById( "hud_build_details" ); container && details != renderedBuildDetails_ )
	{
		container->SetInnerRML( details );
		renderedBuildDetails_ = details;
	}
	renderedBuildCatalog_ = s.buildCatalog;
	renderedSelectedBuild_ = selectedBuild_;
	renderedSelectedBuildType_ = selectedBuildType_;
	updatingBuildCatalog_ = false;
	for( const auto* id : { "hud_build_furniture", "hud_build_workshop", "hud_build_containers", "hud_build_utility", "hud_build_wall", "hud_build_floor", "hud_build_stairs", "hud_build_ramps", "hud_build_fence" } )
		if( auto* element = document_->GetElementById( id ) )
		{
			const bool selected = selectedBuildCategory_ == id;
			element->SetClass( "is-selected", selected );
			element->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
}

void HudRmlBinding::stateChanged( const HudState& s )
{
	if( !document_ ) return;
	setInspectionActive( inspectionActive_ && s.acceptsWorldActions );
	if( !s.acceptsWorldActions ) { closeMenus(); buildMenuOpen_ = false; }
	const bool compactToolWindow = presentation_ == Presentation::OrdersTools;
	const bool showBuild = s.acceptsWorldActions && ( compactToolWindow ? toolPanel_ == ToolPanel::Build : buildMenuOpen_ );
	visible( "hud_root", s.acceptsWorldActions );
	visible( "hud_top_rail", s.acceptsWorldActions && !compactToolWindow );
	visible( "hud_status_bar", s.acceptsWorldActions && !compactToolWindow );
	visible( "hud_build_panel", showBuild );
	renderBuild( s, showBuild );

	// Toolbar: option-set buttons for states and modes that are on (PDF p.322); menu buttons pressed while open.
	const auto activeTool = s.tool.active ? std::string_view( s.tool.active->value ) : std::string_view{};
	const auto set = [this]( const char* id, bool on )
	{
		if( auto* element = document_->GetElementById( id ) )
		{
			element->SetClass( "is-selected", on );
			element->SetAttribute( "aria-pressed", on ? "true" : "false" );
		}
	};
	const auto enable = [this]( const char* id, bool on )
	{
		if( auto* element = document_->GetElementById( id ) ) { if( on ) element->RemoveAttribute( "disabled" ); else element->SetAttribute( "disabled", "disabled" ); }
	};
	set( "hud_pause", s.clock.paused );
	set( "hud_speed_normal", s.clock.speed == GameSpeed::Normal );
	set( "hud_speed_fast", s.clock.speed == GameSpeed::Fast );
	set( "hud_tool_build", activeTool == "build" || ( !compactToolWindow && buildMenuOpen_ ) );
	set( "hud_tool_deconstruct", activeTool == "deconstruct" );
	set( "hud_tool_mine", armsOneOf( mineTools, activeTool ) );
	set( "hud_tool_agriculture", armsOneOf( agricultureTools, activeTool ) );
	set( "hud_tool_designations", armsOneOf( designationTools, activeTool ) );
	set( "hud_tool_jobs", armsOneOf( jobTools, activeTool ) );
	enable( "hud_level_down", s.camera.minLevel >= s.camera.maxLevel || s.camera.viewLevel > s.camera.minLevel );
	enable( "hud_level_up", s.camera.minLevel >= s.camera.maxLevel || s.camera.viewLevel < s.camera.maxLevel );
	enable( "hud_tool_cancel", s.tool.active.has_value() );
	enable( "hud_tool_rotate", s.tool.active.has_value() && s.tool.canRotate );
	for( const char* menu : { "mine", "agriculture", "designations", "jobs", "view" } )
	{
		const bool open = openMenu_ == menu && !compactToolWindow && s.acceptsWorldActions;
		visible( ( "hud_" + std::string( menu ) + "_menu" ).c_str(), open );
		if( auto* button = document_->GetElementById( "hud_tool_" + std::string( menu ) ) )
		{
			button->SetClass( "is-open", open );
			button->SetAttribute( "aria-expanded", open ? "true" : "false" );
		}
	}
	// Menu marks: the chosen tool in a group has a dot; each overlay that is on has a check mark.
	const auto chosen = [this, activeTool]( const auto& tools )
	{
		for( const auto& t : tools )
			if( auto* element = document_->GetElementById( t.id ) )
			{
				const bool on = activeTool == t.tool;
				element->SetClass( "is-chosen", on );
				element->SetAttribute( "aria-checked", on ? "true" : "false" );
			}
	};
	chosen( mineTools );
	chosen( agricultureTools );
	chosen( designationTools );
	chosen( jobTools );
	const auto checked = [this]( const char* id, bool on )
	{
		if( auto* element = document_->GetElementById( id ) ) { element->SetClass( "is-checked", on ); element->SetAttribute( "aria-checked", on ? "true" : "false" ); }
	};
	checked( "hud_overlay_designations", s.overlays.designations );
	checked( "hud_overlay_jobs", s.overlays.jobs );
	checked( "hud_overlay_walls", s.overlays.loweredWalls );
	checked( "hud_overlay_axles", s.overlays.axles );
	if( toolCursor_ ) toolCursor_( s.acceptsWorldActions && s.tool.active.has_value() && !compactToolWindow );

	// Status bar panes: read-only facts; the message pane explains the control under the pointer or the mode.
	renderStatus( s );
	std::string toolName;
	if( s.tool.active )
	{
		const char* item = labelOf( mineTools, activeTool );
		if( !item ) item = labelOf( agricultureTools, activeTool );
		if( !item ) item = labelOf( designationTools, activeTool );
		if( !item ) item = labelOf( jobTools, activeTool );
		toolName = item ? std::string( item ) : activeTool == "deconstruct" ? std::string( "Deconstruct" ) : activeTool == "build" ? std::string( "Build" ) : displayName( s.tool.active->value );
	}
	text( "hud_active_tool", toolName );
	visible( "hud_active_tool", !toolName.empty() );
	text( "hud_kingdom", s.settlement.kingdomName );
	visible( "hud_kingdom", !s.settlement.kingdomName.empty() );
	text( "hud_level", "Level " + std::to_string( s.camera.viewLevel ) );
	text( "hud_date", "Day " + std::to_string( s.clock.day ) + ", Year " + std::to_string( s.clock.year ) );
	char clock[16];
	std::snprintf( clock, sizeof clock, "%02u:%02u", s.clock.hour, s.clock.minute );
	text( "hud_clock", clock );
	text( "hud_daylight", s.clock.daylight == DaylightPhase::Day ? "Day" : s.clock.daylight == DaylightPhase::Night ? "Night" : "Unknown" );
	text( "hud_gnomes", std::to_string( s.settlement.gnomes ) + ( s.settlement.gnomes == 1 ? " gnome" : " gnomes" ) );
	text( "hud_animals", std::to_string( s.settlement.animals ) + ( s.settlement.animals == 1 ? " animal" : " animals" ) );
	text( "hud_items", std::to_string( s.settlement.items ) + ( s.settlement.items == 1 ? " item" : " items" ) );
	if( auto* watch = document_->GetElementById( "hud_watch_rows" ) )
	{
		std::string markup;
		for( const auto& row : s.watchRows )
		{
			const auto key = row.id.category.value + "_" + row.id.group.value + "_" + row.id.item.value + "_" + row.id.material.value;
			markup += "<span class='w98-statusbar__pane' data-watch='" + domId( "watch_", key ) + "'>" + esc( row.label ) + ": " + std::to_string( row.count ) + "</span>";
		}
		if( markup != renderedWatch_ ) { watch->SetInnerRML( markup ); renderedWatch_ = markup; }
	}

	// Tutorial targets get the focus rectangle while hints are on.
	for( const auto* id : { "hud_tool_inspect", "hud_open_population", "hud_open_inventory", "hud_tool_build", "hud_tool_mine", "hud_tool_agriculture", "hud_tool_designations", "hud_tool_fell_tree", "hud_mine_stairs_down", "hud_mine_walls", "hud_tool_stockpile", "hud_tool_farm", "hud_tool_dormitory", "hud_build_workshop", "hud_build_furniture", "hud_pause", "hud_speed_normal", "hud_speed_fast", "hud_level_down", "hud_level_up", "tutorial-finish" } )
	{
		bool highlighted = false;
		if( s.tutorial.hintsEnabled ) for( const auto& target : s.tutorial.highlightedIds ) if( target == id ) { highlighted = true; break; }
		if( auto* element = document_->GetElementById( id ) ) element->SetClass( "tutorial-target", highlighted );
	}

	// Tutorial palette: the lesson's steps and the lesson list as read-only check boxes.
	visible( "hud_tutorial_panel", s.tutorial.active || s.tutorial.completed || s.tutorial.incompatible );
	const auto tutorialText = [this]( const std::string& value ) { const LocalizationKey key{ value }; return textCatalog_.contains( key ) ? textCatalog_.format( key ) : value; };
	const auto checkRow = []( bool done, const std::string& label ) { return "<label class='w98-check'><input type='checkbox' disabled='disabled'" + std::string( done ? " checked='checked'" : "" ) + "/><span>" + esc( label ) + "</span></label>"; };
	text( "tutorial_title", s.tutorial.title.empty() ? std::string( "Tutorial" ) : tutorialText( s.tutorial.title ) );
	text( "tutorial_explanation", tutorialText( s.tutorial.explanation ) );
	text( "tutorial_objective", tutorialText( s.tutorial.objective ) );
	text( "tutorial_progress", s.tutorial.progress );
	visible( "tutorial_explanation", s.tutorial.hintsEnabled || s.tutorial.incompatible );
	std::string steps;
	for( std::size_t index = 0; index < s.tutorial.steps.size(); ++index )
		steps += checkRow( index < s.tutorial.completedSteps.size() && s.tutorial.completedSteps[index], std::to_string( index + 1 ) + ". " + tutorialText( s.tutorial.steps[index] ) );
	if( auto* e = document_->GetElementById( "tutorial_steps" ) ) e->SetInnerRML( steps );
	visible( "tutorial_steps", s.tutorial.hintsEnabled || s.tutorial.incompatible );
	text( "tutorial-hints", s.tutorial.hintsEnabled ? "Hide Hints" : "Show Hints" );
	static const char* const tutorialSteps[] = { "tutorial.step.orientation", "tutorial.step.inspect_assign", "tutorial.step.gathering", "tutorial.step.mining_levels", "tutorial.step.stockpile", "tutorial.step.crafting", "tutorial.step.farming", "tutorial.step.shelter", "tutorial.step.graduation" };
	constexpr std::size_t tutorialStepCount = 9;
	std::string checklist, skipped;
	for( std::size_t i = 0; i < tutorialStepCount; ++i )
	{
		const auto label = tutorialText( tutorialSteps[i] );
		const auto bit = 1u << i;
		const bool done = ( s.tutorial.completedMask & bit ) != 0;
		const bool wasSkipped = ( s.tutorial.skippedMask & bit ) != 0;
		checklist += checkRow( done, label + ( wasSkipped ? " (skipped)" : i == s.tutorial.step && !done ? " (current)" : "" ) );
		if( wasSkipped ) skipped += ( skipped.empty() ? "" : ", " ) + label;
	}
	if( auto* e = document_->GetElementById( "tutorial_checklist" ) ) e->SetInnerRML( checklist );
	const bool showSkippedWarning = s.tutorial.step == s.tutorial.stepCount - 1 && !skipped.empty();
	text( "tutorial_warning", showSkippedWarning ? "Skipped lessons: " + skipped + "." : "" );
	visible( "tutorial_warning", showSkippedWarning );
	visible( "tutorial-continue", s.tutorial.active && ( ( s.tutorial.completedMask & ( 1u << s.tutorial.step ) ) != 0 || ( s.tutorial.skippedMask & ( 1u << s.tutorial.step ) ) != 0 ) );
	visible( "tutorial-finish", s.tutorial.active && s.tutorial.step == s.tutorial.stepCount - 1 );
	if( auto* panel = document_->GetElementById( "hud_tutorial_panel" ) ) panel->SetClass( "is-complete", s.tutorial.completed );

	// Events that need an answer: one message box at a time (PDF p.184); the first button has the focus.
	const bool prompt = !s.prompts.empty();
	visible( "hud_event_blocker", prompt );
	if( auto* blocker = document_->GetElementById( "hud_event_blocker" ) ) blocker->SetAttribute( "aria-hidden", prompt ? "false" : "true" );
	if( prompt )
	{
		const auto& p = s.prompts.front();
		text( "hud_event_title", p.title );
		text( "hud_event_body", p.body );
		visible( "hud_event_ack", p.responses == EventResponseKind::Acknowledge );
		visible( "hud_event_yes", p.responses == EventResponseKind::YesNo );
		visible( "hud_event_no", p.responses == EventResponseKind::YesNo );
		if( !focusedPrompt_ || focusedPrompt_->value != p.instanceId.value )
		{
			const char* focusId = p.responses == EventResponseKind::Acknowledge ? "hud_event_ack" : "hud_event_yes";
			if( auto* button = document_->GetElementById( focusId ) ) button->Focus( true );
			focusedPrompt_ = p.instanceId;
		}
	}
	else
		focusedPrompt_.reset();
}
} // namespace ingnomia::ui::hud
