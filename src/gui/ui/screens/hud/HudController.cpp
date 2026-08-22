/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "HudController.h"

#include <algorithm>

namespace ingnomia::ui::hud
{
HudController::HudController( HudCommandPort& commands, HudViewPort& view ) : commands_( commands ), view_( view ) { notify(); }
void HudController::notify() { view_.stateChanged( state_ ); }
void HudController::beginWorld( WorldEpoch world )
{
	// Tutorial snapshots can be queued just before the world-transition signal.
	// Preserve that event-driven guidance while the HUD receives its new world epoch.
	auto tutorial = std::move( state_.tutorial );
	state_ = {};
	state_.tutorial = std::move( tutorial );
	state_.world = world;
	state_.acceptsWorldActions = static_cast<bool>( world );
	notify();
}
void HudController::endWorld() { state_ = {}; notify(); }
void HudController::setSettlement( SettlementSummary value ) { state_.settlement = std::move( value ); notify(); }
void HudController::setClock( ClockCalendarState value ) { state_.clock = value; notify(); }
void HudController::setCamera( CameraState value ) { state_.camera = value; notify(); }
void HudController::setOverlays( RenderOverlayState value ) { state_.overlays = value; notify(); }
void HudController::setTool( ToolState tool, SelectionSummary selection ) { state_.tool = std::move( tool ); state_.selection = std::move( selection ); notify(); }
void HudController::setWatchRows( std::vector<HudWatchRow> rows ) { state_.watchRows = std::move( rows ); notify(); }
void HudController::setBuildCatalog( std::vector<BuildCatalogRow> rows ) { state_.buildCatalog = std::move( rows ); notify(); }
void HudController::setTutorial( TutorialViewState value ) { state_.tutorial = std::move( value ); notify(); }
void HudController::tutorialAdvance() { dispatch( "tutorial.advance", NoPayload{} ); }
void HudController::tutorialSkip() { dispatch( "tutorial.skip", NoPayload{} ); }
void HudController::tutorialRestart() { dispatch( "tutorial.restart", NoPayload{} ); }
void HudController::tutorialToggleHints() { dispatch( "tutorial.toggle_hints", NoPayload{} ); }
void HudController::tutorialFinish() { dispatch( "tutorial.finish", NoPayload{} ); }

bool HudController::enqueuePrompt( std::optional<EventResponseTargetId> target, std::string title,
	std::string body, bool requestsPause, bool yesNo )
{
	if( yesNo && ( !target || !*target ) ) return false;
	state_.prompts.push_back( { PromptInstanceId{ nextPrompt_++ }, yesNo ? target : std::nullopt,
		std::move( title ), std::move( body ), yesNo ? EventResponseKind::YesNo : EventResponseKind::Acknowledge,
		requestsPause, false } );
	commands_.registerPrompt( state_.prompts.back().instanceId, state_.prompts.back().responseTarget );
	if( requestsPause && !state_.clock.paused ) dispatch( "sim.set_paused", SetPausedPayload{ true } );
	notify();
	return true;
}

bool HudController::dispatch( std::string_view id, UiActionPayload payload )
{
	if( !state_.acceptsWorldActions || !state_.world ) return false;
	UiActionEnvelope action{ ActionId{ id }, RequestId{ nextRequest_++ }, state_.world, std::nullopt, std::move( payload ) };
	const auto result = commands_.dispatch( action );
	if( result.status == CommandStatus::Rejected ) { state_.status = result.error; notify(); return false; }
	state_.status.clear();
	if( result.pending ) state_.pendingAction = action.request;
	notify();
	return true;
}

void HudController::setPaused( bool paused ) { dispatch( "sim.set_paused", SetPausedPayload{ paused } ); }
void HudController::setSpeed( GameSpeed speed ) { dispatch( "sim.set_speed", SetSpeedPayload{ speed } ); }
void HudController::changeLevel( std::int32_t level )
{
	// The game-thread bridge owns the authoritative world bounds.  The HUD can
	// briefly have a stale maxLevel of zero (notably immediately after loading a
	// save), so only apply the local clamp when the bounds are known. The bridge
	// still clamps the final request against the loaded world's real extent.
	const bool boundsKnown = state_.camera.maxLevel > state_.camera.minLevel
		|| state_.camera.viewLevel == state_.camera.minLevel;
	if ( boundsKnown ) level = std::clamp( level, state_.camera.minLevel, state_.camera.maxLevel );
	dispatch( "view.change_level", ChangeLevelPayload{ level } );
}
void HudController::setOverlay( OverlayKind overlay, bool enabled ) { dispatch( "view.set_overlay", OverlayPayload{ overlay, enabled } ); }
void HudController::activateTool( ToolId tool )
{
	const auto id = tool.value;
	if ( !dispatch( "tool.activate", ActivateToolPayload{ tool, std::nullopt, {} } ) ) return;
	state_.tool = {};
	state_.tool.active = ToolId{ id };
	state_.tool.phase = ToolPhase::Preview;
	if ( id == "mine" || id == "explorative_mine" || id == "remove_floor" || id == "dig_hole" || id == "dig_stairs_down" || id == "mine_stairs_up" || id == "dig_ramp_down" )
		state_.tool.category = ToolCategory::Dig;
	else if ( id == "fell_tree" || id == "plant_tree" || id == "harvest_tree" || id == "forage" || id == "remove_plant" )
		state_.tool.category = ToolCategory::Agriculture;
	else if ( id == "suspend_job" || id == "resume_job" || id == "cancel_job" || id == "raise_job_priority" || id == "lower_job_priority" )
		state_.tool.category = ToolCategory::Job;
	else
		state_.tool.category = ToolCategory::Designation;
	notify();
}
void HudController::cancelTool()
{
	if ( !dispatch( "tool.cancel", NoPayload{} ) ) return;
	// The right-click command is queued to the game thread. Clear the local
	// projection immediately so the HUD cannot advertise a stale active tool
	// while the authoritative selection emits its next update.
	state_.tool = {};
	state_.selection = {};
	notify();
}
void HudController::rotateTool()
{
	if ( !state_.tool.canRotate || !dispatch( "tool.rotate", NoPayload{} ) ) return;
	state_.tool.rotation = static_cast<std::uint8_t>( ( state_.tool.rotation + 1 ) % 4 );
	notify();
}
void HudController::chooseBuild( CatalogId item )

{
	chooseBuildAction( item, BuildAction::Build );
}
void HudController::chooseBuildAction( CatalogId item, BuildAction action )
{
	const auto found = std::ranges::find_if( state_.buildCatalog, [&]( const BuildCatalogRow& row ) { return row.id == item; } );
	if ( found == state_.buildCatalog.end() ) return;
	if ( !found->available )
	{
		state_.status = found->unavailableReason.empty() ? "hud.build.unavailable" : found->unavailableReason;
		notify();
		return;
	}
	if( dispatch( "tool.choose_build", ChooseBuildPayload{ found->id, found->kind, found->defaultMaterials, action } ) )
	{
		state_.tool.active = ToolId{ "build" };
		state_.tool.category = ToolCategory::Build;
		state_.tool.phase = ToolPhase::Preview;
		state_.tool.item = found->id;
		state_.tool.materials = found->defaultMaterials;
		state_.tool.canRotate = true;
		notify();
	}
}
void HudController::selectBuildMaterial( CatalogId item, std::uint32_t componentIndex, CatalogId material, bool notifyView )
{
	const auto found = std::ranges::find_if( state_.buildCatalog, [&]( const BuildCatalogRow& row ) { return row.id == item; } );
	if ( found == state_.buildCatalog.end() || componentIndex >= found->components.size() ) return;
	const auto& component = found->components[componentIndex];
	const auto option = std::ranges::find_if( component.options, [&]( const BuildCatalogRow::MaterialOption& value ) { return value.id == material; } );
	if ( option == component.options.end() ) return;
	const bool activeBuild = state_.tool.item && *state_.tool.item == item;
	if ( activeBuild && !dispatch( "tool.set_material", SetBuildMaterialPayload{ componentIndex, material } ) ) return;
	found->components[componentIndex].selected = material;
	found->defaultMaterials.clear();
	for ( const auto& selected : found->components )
		if ( !selected.selected.value.empty() ) found->defaultMaterials.push_back( selected.selected );
	if ( state_.tool.item && *state_.tool.item == item ) state_.tool.materials = found->defaultMaterials;
	if ( notifyView ) notify();
}
void HudController::requestBuildItems( BuildSelection selection, std::string_view category )
{
	if( !state_.acceptsWorldActions || !state_.world ) return;
	const auto result = commands_.requestBuildItems( selection, category );
	if( result.status == CommandStatus::Rejected ) state_.status = result.error;
	else state_.status.clear();
	notify();
}
void HudController::closeBuildMenu()
{
	state_.buildCatalog.clear();
	state_.status.clear();
	notify();
}
void HudController::respondToPrompt( EventResponse response )
{
	if( state_.prompts.empty() ) return;
	const auto& prompt = state_.prompts.front();
	if( prompt.responses == EventResponseKind::Acknowledge && response != EventResponse::Acknowledge ) return;
	if( prompt.responses == EventResponseKind::YesNo && response == EventResponse::Acknowledge ) return;
	if( dispatch( "event.respond", EventResponsePayload{ prompt.instanceId, response } ) ) state_.prompts.pop_front();
	notify();
}
void HudController::onActionFinished( RequestId request, CommandResult result )
{
	if( state_.pendingAction != request ) return;
	state_.pendingAction.reset();
	state_.status = result.status == CommandStatus::Rejected ? result.error : std::string{};
	notify();
}
} // namespace ingnomia::ui::hud
