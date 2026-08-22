#include "gui/ui/screens/shell/ShellController.h"
#include "gui/ui/screens/shell/ShellDataAdapter.h"
#include "gui/ui/screens/shell/ShellRmlAdapter.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace ingnomia::ui;
using namespace ingnomia::ui::shell;

namespace
{
void check( bool condition, std::string_view message )
{
	if( !condition )
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit( 1 );
	}
}

class Commands final : public ShellCommandPort
{
public:
	CommandResult dispatch( const UiActionEnvelope& action ) override
	{
		actions.push_back( action );
		return nextResult;
	}

	std::vector<UiActionEnvelope> actions;
	CommandResult nextResult;
};

class View final : public ShellViewPort
{
public:
	void stateChanged( const ShellState& ) override { ++updates; }
	void showConfirmation( const Message&, const Message&, FocusToken focus ) override
	{
		confirmationOpen = true;
		confirmationFocus = focus;
	}
	void closeConfirmation() override { confirmationOpen = false; }
	void restoreFocus( FocusToken focus ) override { restoredFocus = focus; }

	std::size_t updates{};
	bool confirmationOpen{};
	FocusToken confirmationFocus{};
	FocusToken restoredFocus{};
};

void testClosedRmlCallbacks()
{
	check( ShellRmlAdapter::controlForElement( "shell-load" ) == ShellControl::OpenLoadGame,
		"known RML element maps to a closed callback" );
	check( !ShellRmlAdapter::controlForElement( "app.exit" ), "ActionId text is not accepted as an RML callback" );
	check( ShellRmlAdapter::initialFocusForRoute( "game.pause" ) == "pause-resume",
		"pause route has deterministic safe focus" );
}

void testNavigationAndContinue()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.activate( ShellControl::StartDefaultGame );
	check( commands.actions.back().id.value == "app.start_new_game", "default New game dispatches immediately" );
	controller.activate( ShellControl::ContinueLastGame );
	check( commands.actions.size() == 1, "unavailable Continue does not dispatch" );
	controller.setContinueAvailability( true );
	controller.activate( ShellControl::ContinueLastGame );
	check( commands.actions.back().id.value == "app.continue_last_game", "Continue uses exact ActionId" );
	controller.activate( ShellControl::OpenLoadGame );
	check( commands.actions.back().id.value == "nav.open", "route opening uses nav.open" );
	check( std::get<RoutePayload>( commands.actions.back().payload ).route.value == "shell.load_game",
		"Load route uses exact RouteId" );
}

void testNewGameFieldsRemainTyped()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.setNewGameField( NewGameFieldId{ "kingdom_name" }, std::string{ "Copperdeep" } );
	check( commands.actions.back().id.value == "new_game.set_field", "new-game field edits use the registered action" );
	const auto& field = std::get<SetNewGameFieldPayload>( commands.actions.back().payload );
	check( field.field.value == "kingdom_name", "new-game field keeps its stable field id" );
	check( std::get<std::string>( field.value ) == "Copperdeep", "new-game field keeps its typed string value" );
	controller.activate( ShellControl::StartConfiguredGame );
	check( commands.actions.back().id.value == "app.start_new_game", "configured start dispatches after an edited field" );
	controller.setNewGameField( NewGameFieldId{ "gnomes" }, std::int32_t{ 0 } );
	check( !controller.state().newGame.validationErrors.empty(), "out-of-range new-game field is surfaced" );
	const auto count = commands.actions.size();
	controller.activate( ShellControl::StartConfiguredGame );
	check( commands.actions.size() == count, "invalid new-game draft cannot dispatch" );
	controller.setNewGameField( NewGameFieldId{ "gnomes" }, std::int32_t{ 12 } );
	check( controller.state().newGame.validationErrors.empty(), "valid new-game field clears validation" );
}

void testSafeLoadSelection()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	LoadGameState loads = ShellDataAdapter::saves(
		{ SaveKingdomRow{ SaveKingdomId{ "deep-home" }, "Deep Home", 100 } },
		{ SaveSlotRow{ SaveSlotId{ "deep-home/old" }, "Deep Home", "0.8", 80, false },
			SaveSlotRow{ SaveSlotId{ "deep-home/current" }, "Deep Home", "0.9", 100, true } } );
	controller.setLoadGameState( loads );
	controller.selectSave( SaveSlotId{ "deep-home/old" } );
	controller.activate( ShellControl::LoadSelected );
	check( commands.actions.empty(), "incompatible save cannot dispatch Load" );
	controller.selectSave( SaveSlotId{ "deep-home/current" } );
	controller.activate( ShellControl::LoadSelected );
	check( commands.actions.back().id.value == "app.load_game", "compatible save uses exact load action" );
	check( std::get<LoadGamePayload>( commands.actions.back().payload ).slot.relativeKey == "deep-home/current",
		"only relative SaveSlotId crosses the command boundary" );
	controller.moveSaveSelection( -1 );
	check( controller.state().loadGame.selectedSlot == SaveSlotId{ "deep-home/old" },
		"keyboard-style row movement preserves stable SaveSlotId selection" );
	controller.moveSaveSelection( -1 );
	check( controller.state().loadGame.selectedSlot == SaveSlotId{ "deep-home/old" },
		"row movement clamps at list boundary" );
}

void testEmptyLoadState()
{
	const auto empty = ShellDataAdapter::saves( {}, {} );
	check( empty.kingdomsStatus == RequestStatus::Empty, "empty kingdom scan reports Empty" );
	check( empty.savesStatus == RequestStatus::Empty, "empty save scan reports Empty" );
	check( !empty.selectedKingdom && !empty.selectedSlot, "empty load scan has no implicit selection" );
}

void testSettingsProjectionAndDispatch()
{
	const auto settings = ShellDataAdapter::settings( { true, true, 144, 1.25f, 240, -3, true } );
	check( settings.rows.size() == 7, "seven verified settings are projected" );
	check( settings.rows[0].id.value == "display.fullscreen", "fullscreen SettingId is exact" );
	check( settings.rows[1].id.value == "display.follow_monitor_refresh", "monitor refresh SettingId is exact" );
	check( settings.rows[2].id.value == "display.frame_rate_limit" && std::get<std::int32_t>( settings.rows[2].authoritative ) == 144,
		"manual frame rate limit preserves high-refresh values" );
	check( settings.rows[3].id.value == "interface.ui_scale", "UI scale SettingId is exact" );
	check( std::get<std::int32_t>( settings.rows[4].authoritative ) == 200, "keyboard speed is bounded" );
	check( std::get<std::int32_t>( settings.rows[5].authoritative ) == 0, "minimum light is bounded" );
	for( const auto& row : settings.rows )
		check( row.id.value != "audio.master_volume" && row.id.value != "interface.language",
			"blocked settings are omitted" );

	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.setSettingsState( settings );
	controller.setSettingDraft( SettingId{ "display.fullscreen" }, false );
	check( commands.actions.back().id.value == "settings.set_draft", "settings use exact draft action" );
	check( std::get<SetSettingDraftPayload>( commands.actions.back().payload ).setting.value == "display.fullscreen",
		"settings preserve typed SettingId" );
	const auto count = commands.actions.size();
	controller.setSettingDraft( SettingId{ "audio.master_volume" }, 50.0f );
	check( commands.actions.size() == count, "unsupported settings cannot dispatch" );
	controller.setSettingDraft( SettingId{ "camera.keyboard_pan_speed" }, std::int32_t{ 201 } );
	check( commands.actions.size() == count, "out-of-range setting cannot dispatch" );
	controller.setSettingDraft( SettingId{ "camera.keyboard_pan_speed" }, 20.0f );
	check( commands.actions.size() == count, "wrong setting value type cannot dispatch" );
}

void testConfirmationAndFocus()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.activate( ShellControl::Exit, FocusToken{ 42 } );
	check( commands.actions.empty(), "exit does not dispatch before confirmation" );
	check( view.confirmationOpen && view.confirmationFocus == FocusToken{ 42 }, "confirmation records focus token" );
	controller.activate( ShellControl::CancelDestructive );
	check( !view.confirmationOpen && view.restoredFocus == FocusToken{ 42 }, "cancel restores focus" );
	controller.activate( ShellControl::Exit, FocusToken{ 7 } );
	controller.activate( ShellControl::ConfirmDestructive );
	check( commands.actions.back().id.value == "app.exit", "confirmed exit dispatches typed pending action" );
	check( std::holds_alternative<NoPayload>( commands.actions.back().payload ), "exit payload remains typed" );
}

void testPauseResumeAndReturn()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.setWorld( WorldEpoch{ 9 } );
	commands.nextResult.pending = true;
	controller.activate( ShellControl::Resume );
	check( commands.actions.back().id.value == "sim.set_paused", "resume requests authoritative pause mutation" );
	check( std::get<SetPausedPayload>( commands.actions.back().payload ).paused == false, "resume requests false" );
	check( commands.actions.back().world == WorldEpoch{ 9 }, "world action carries epoch" );
	check( controller.state().pendingRequest.has_value(), "resume remains pending until authoritative pause state" );
	commands.nextResult.pending = false;
	controller.onPauseState( false, PauseReason::None );
	check( commands.actions.back().id.value == "nav.close", "pause overlay closes only after authoritative resume" );
	check( !controller.state().pendingRequest.has_value(), "resume completion clears the pending request" );

	controller.activate( ShellControl::ReturnToMenu, FocusToken{ 11 } );
	check( view.confirmationOpen, "return to menu requires confirmation" );
	commands.nextResult.pending = true;
	controller.activate( ShellControl::ConfirmDestructive );
	check( commands.actions.back().id.value == "app.end_world", "return dispatches exact end-world action" );
	const auto endRequest = commands.actions.back().request;
	controller.onActionFinished( endRequest, {} );
	check( commands.actions.back().id.value == "nav.open", "accepted end-world transitions to a route" );
	check( std::get<RoutePayload>( commands.actions.back().payload ).route.value == "shell.main_menu",
		"return transitions to exact main-menu route" );
}

void testEscapeRoutesThroughShell()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.setWorld( WorldEpoch{ 21 } );
	controller.finishWorldTransition( true );

	check( controller.handleEscape(), "Escape is handled by the active game HUD route" );
	check( controller.state().route.value == "game.pause", "Escape opens the pause route" );
	check( commands.actions.size() >= 2 && commands.actions[commands.actions.size() - 2].id.value == "sim.set_paused",
		"Escape requests the authoritative pause mutation" );
	check( std::get<SetPausedPayload>( commands.actions[commands.actions.size() - 2].payload ).paused, "Escape pauses the simulation" );

	controller.activate( ShellControl::ReturnToMenu, FocusToken{ 4 } );
	check( view.confirmationOpen, "return to menu still opens its confirmation" );
	check( controller.handleEscape(), "Escape is handled by the shell confirmation" );
	check( !view.confirmationOpen, "Escape cancels the shell confirmation" );

	check( controller.handleEscape(), "Escape is handled by the pause route" );
	check( commands.actions.back().id.value == "sim.set_paused", "pause Escape requests resume" );
	check( !std::get<SetPausedPayload>( commands.actions.back().payload ).paused, "pause Escape resumes the simulation" );
}

void testPauseSaveLifecycle()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.setWorld( WorldEpoch{ 12 } );
	commands.nextResult.pending = true;
	controller.activate( ShellControl::Save );
	check( commands.actions.back().id.value == "app.save_game", "pause Save dispatches the production save action" );
	check( controller.state().saveStatus == RequestStatus::Loading, "save status reports Saving while the game-thread request is active" );
	check( controller.state().pendingRequest == commands.actions.back().request, "save keeps the matching request pending" );
	controller.onSaveGameFinished( true );
	check( controller.state().saveStatus == RequestStatus::Ready, "successful save reports Saved" );
	check( !controller.state().pendingRequest.has_value(), "successful save clears the pending request" );
	check( !controller.state().actionError.has_value(), "successful save clears the previous error" );

	controller.activate( ShellControl::Save );
	controller.onSaveGameFinished( false );
	check( controller.state().saveStatus == RequestStatus::Error, "failed save reports an error state" );
	check( controller.state().actionError && controller.state().actionError->message.key.value == "ui.error.save_failed",
		"failed save exposes the typed save error" );
}

void testWorldTransitionLifecycle()
{
	Commands commands;
	View view;
	ShellController controller( commands, view );
	controller.beginWorldTransition( false );
	check( controller.state().route.value == "shell.loading", "load transition opens the loading route" );
	check( controller.state().lifecycle.phase == WorldPhase::Loading, "load transition reports Loading" );
	check( controller.state().lifecycle.blocksWorldInput, "loading blocks world input" );
	controller.setLifecycleProgress( "Create height map." );
	check( controller.state().lifecycle.progressText == "Create height map.", "authoritative generator text is retained" );
	controller.finishWorldTransition( true );
	check( controller.state().route.value == "game.hud", "successful transition targets the HUD route" );
	check( controller.state().lifecycle.phase == WorldPhase::Ready, "successful transition reports Ready" );
	check( !controller.state().lifecycle.blocksWorldInput, "ready world accepts input" );
	controller.endWorld();
	check( controller.state().route.value == "shell.main_menu", "world end restores the main menu route" );
	check( controller.state().lifecycle.phase == WorldPhase::NoWorld, "world end clears lifecycle phase" );
	controller.beginWorldTransition( true );
	controller.finishWorldTransition( false );
	check( controller.state().lifecycle.phase == WorldPhase::Failed, "failed transition reports Failed" );
	check( controller.state().lifecycle.error.has_value(), "failed transition exposes error" );
	check( !controller.state().lifecycle.error->retryable, "fixture-only transition has no fabricated retry command" );
	controller.endWorld();
	controller.setContinueAvailability( true );
	controller.activate( ShellControl::ContinueLastGame );
	controller.beginWorldTransition( false );
	controller.finishWorldTransition( false );
	check( controller.state().lifecycle.error && controller.state().lifecycle.error->retryable,
		"duplicate transition-start notification retains the retry command" );
	controller.activate( ShellControl::Retry );
	check( commands.actions.back().id.value == "app.continue_last_game", "Retry redispatches the retained typed command" );
	check( controller.state().lifecycle.phase == WorldPhase::Loading, "Retry returns to the loading state" );
}
}

int main()
{
	testClosedRmlCallbacks();
	testNavigationAndContinue();
	testNewGameFieldsRemainTyped();
	testSafeLoadSelection();
	testEmptyLoadState();
	testSettingsProjectionAndDispatch();
	testConfirmationAndFocus();
	testPauseResumeAndReturn();
	testEscapeRoutesThroughShell();
	testPauseSaveLifecycle();
	testWorldTransitionLifecycle();
	std::cout << "ui shell controller tests passed\n";
}
