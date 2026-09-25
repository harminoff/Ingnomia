/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "ShellController.h"

#include <algorithm>
#include <iterator>
#include <type_traits>

namespace ingnomia::ui::shell
{
namespace
{
Message message( const char* key )
{
	return { LocalizationKey{ key }, {} };
}

bool validNewGameField( const NewGameFieldValue& field )
{
	if( field.field.value == "kingdom_name" || field.field.value == "seed" )
	{
		const auto* value = std::get_if<std::string>( &field.value );
		return value && !value->empty();
	}
	if( field.field.value == "peaceful" ) return std::holds_alternative<bool>( field.value );
	const auto* value = std::get_if<std::int32_t>( &field.value );
	if( !value ) return false;
	const auto inRange = [&]( std::int32_t minimum, std::int32_t maximum ) { return *value >= minimum && *value <= maximum; };
	if( field.field.value == "world_size" ) return inRange( 1, 300 );
	if( field.field.value == "z_levels" ) return inRange( 71, 200 );
	if( field.field.value == "ground" ) return inRange( 7, 192 );
	if( field.field.value == "flatness" ) return inRange( 0, 20 );
	if( field.field.value == "ocean_size" ) return inRange( 0, 15 );
	if( field.field.value == "rivers" ) return inRange( 0, 10 );
	if( field.field.value == "river_size" ) return inRange( 0, 20 );
	if( field.field.value == "tree_density" || field.field.value == "plant_density" ) return inRange( 0, 100 );
	if( field.field.value == "wild_animals" ) return inRange( 0, 500 );
	if( field.field.value == "gnomes" ) return inRange( 1, 100 );
	if( field.field.value == "start_zone" ) return inRange( 0, 20 );
	return false;
}
}

ShellController::ShellController( ShellCommandPort& commands, ShellViewPort& view )
	: commands_( commands ), view_( view )
{
	// The legacy game manager already owns a valid default NewGameSettings object.
	// Keep the default shell action usable until the optional setup projection has
	// supplied a more detailed draft; an Idle status made the visible New game
	// button silently do nothing in the RmlUi shell.
	state_.newGame.status = RequestStatus::Ready;
}

void ShellController::setVersion( std::string version )
{
	state_.version = std::move( version );
	notify();
}

void ShellController::setReducedMotion( bool enabled )
{
	state_.reducedMotion = enabled;
	notify();
}

void ShellController::setContinueAvailability( bool available, std::optional<Message> blocker,
	std::string saveName, std::string savedAt )
{
	state_.continueAvailable = available;
	state_.continueBlocker = std::move( blocker );
	state_.continueSaveName = std::move( saveName );
	state_.continueSavedAt = std::move( savedAt );
	notify();
}

void ShellController::setNewGameState( NewGameState state ) { state_.newGame = std::move( state ); notify(); }
void ShellController::setLoadGameState( LoadGameState state ) { state_.loadGame = std::move( state ); notify(); }
void ShellController::setSettingsState( SettingsState state ) { state_.settings = std::move( state ); notify(); }
void ShellController::setLifecycle( LifecycleView state ) { state_.lifecycle = std::move( state ); notify(); }
void ShellController::beginWorldTransition( bool generating, std::optional<UiActionEnvelope> retryAction )
{
	// The authoritative game-thread signal is emitted after the controller has
	// already started a transition for a typed shell action.  That second
	// notification does not carry the action, so do not erase the retry payload
	// while mirroring the same transition into the view.  A transition that
	// starts outside the shell while no transition is active is a genuinely new
	// operation and must not inherit an old retry target.
	if( retryAction ) retryAction_ = std::move( retryAction );
	else if( state_.lifecycle.phase != WorldPhase::Generating && state_.lifecycle.phase != WorldPhase::Loading ) retryAction_.reset();
	state_.lifecycle.phase = generating ? WorldPhase::Generating : WorldPhase::Loading;
	state_.lifecycle.blocksWorldInput = true;
	state_.lifecycle.error.reset();
	state_.lifecycle.progress = message( "shell.loading.loading-progress" );
	state_.lifecycle.progressText.clear();
	state_.route = RouteId{ "shell.loading" };
	if( generating ) state_.newGame.status = RequestStatus::Loading;
	notify();
}

void ShellController::setLifecycleProgress( std::string progress )
{
	if( state_.lifecycle.phase != WorldPhase::Generating && state_.lifecycle.phase != WorldPhase::Loading ) return;
	state_.lifecycle.progressText = std::move( progress );
	state_.lifecycle.progress = message( "shell.loading.loading-progress" );
	notify();
}

void ShellController::finishWorldTransition( bool success )
{
	state_.lifecycle.blocksWorldInput = false;
	// The request that started the transition (continue, load, start) is answered by the transition itself; left
	// pending, it kept every Pause command unavailable in the loaded game (Stage 19).
	if( state_.pendingRequest && state_.pendingRequest != pendingPauseRequest_ && state_.pendingRequest != pendingSaveRequest_ ) state_.pendingRequest.reset();
	state_.lifecycle.progressText.clear();
	if( success )
	{
		state_.lifecycle.phase = WorldPhase::Ready;
		state_.lifecycle.progress.reset();
		state_.lifecycle.error.reset();
		state_.route = RouteId{ "game.hud" };
		state_.newGame.status = RequestStatus::Ready;
		retryAction_.reset();
	}
	else
	{
		state_.lifecycle.phase = WorldPhase::Failed;
		state_.lifecycle.error = ShellError{ message( "shell.loading.the_kingdom_could_not_be_prepared" ), retryAction_.has_value(),
			retryAction_ };
		state_.lifecycle.progress = message( "shell.loading.loading-progress" );
		state_.route = RouteId{ "shell.loading" };
		state_.newGame.status = RequestStatus::Error;
	}
	notify();
}

void ShellController::endWorld()
{
    if(confirmation_) activate(ShellControl::CancelDestructive);
	state_.lifecycle = LifecycleView{};
	state_.route = RouteId{ "shell.main_menu" };
	state_.pendingRequest.reset();
	state_.saveStatus = RequestStatus::Idle;
	state_.actionError.reset();
	state_.authoritativePaused = false;
	state_.newGame.status = RequestStatus::Ready;
	retryAction_.reset();
	pendingPauseRequest_.reset();
	pendingPauseValue_.reset();
	pendingSaveRequest_.reset();
	notify();
}
void ShellController::setWorld( WorldEpoch world ) { world_ = world; }

void ShellController::onPauseState( bool paused, PauseReason reason )
{
	state_.authoritativePaused = paused;
	state_.pauseReason = reason;
	if( pendingPauseRequest_ && pendingPauseValue_ && *pendingPauseValue_ == paused )
	{
		if( state_.pendingRequest == pendingPauseRequest_ ) state_.pendingRequest.reset();
		pendingPauseRequest_.reset();
		pendingPauseValue_.reset();
	}
	if( !paused && closePauseWhenUnpaused_ )
	{
		closePauseWhenUnpaused_ = false;
		if( dispatch( action( "nav.close", RoutePayload{ RouteId{ "game.pause" } } ) ) ) state_.route = RouteId{ "game.hud" };
	}
	notify();
}

void ShellController::onSaveGameFinished( bool success )
{
	if( !pendingSaveRequest_ || state_.pendingRequest != pendingSaveRequest_ ) return;
	state_.pendingRequest.reset();
	pendingSaveRequest_.reset();
	state_.saveStatus = success ? RequestStatus::Ready : RequestStatus::Error;
	state_.actionError = success ? std::nullopt : std::optional<ShellError>{ ShellError{ message( "ui.error.save_failed" ), false, std::nullopt } };
	notify();
}

void ShellController::onActionFinished( RequestId request, CommandResult result )
{
	if( state_.pendingRequest != request ) return;
	state_.pendingRequest.reset();
	state_.actionError = std::move( result.error );
	if( result.status == CommandStatus::Accepted && routeAfterConfirmedAction_ )
	{
		auto route = std::move( *routeAfterConfirmedAction_ );
		routeAfterConfirmedAction_.reset();
		navigate( route.value );
	}
	else if( result.status == CommandStatus::Rejected ) routeAfterConfirmedAction_.reset();
	notify();
}

bool ShellController::handleEscape()
{
	if( confirmation_ )
	{
		activate( ShellControl::CancelDestructive );
		return true;
	}
    if(state_.route.value == "shell.new_game" || state_.route.value == "shell.load_game" ||
        state_.route.value == "shell.settings" || state_.route.value == "game.settings")
    {
        back();
        return true;
    }
	if( state_.route.value == "game.hud" )
	{
		const bool pauseAlreadyRequested = state_.authoritativePaused ||
			( pendingPauseValue_ && *pendingPauseValue_ );
		activate( pauseAlreadyRequested ? ShellControl::OpenPauseMenu : ShellControl::OpenPause );
		return true;
	}
	if( state_.route.value == "game.pause" )
	{
		activate( ShellControl::Resume );
		return true;
	}
	return false;
}

UiActionEnvelope ShellController::action( std::string_view id, UiActionPayload payload, bool worldAction )
{
	UiActionEnvelope envelope{ ActionId{ id }, RequestId{ nextRequest_++ }, std::nullopt,
		std::nullopt, std::move( payload ) };
	if( worldAction ) envelope.world = world_;
	return envelope;
}

bool ShellController::dispatch( UiActionEnvelope envelope )
{
	const auto request = envelope.request;
	const auto result = commands_.dispatch( envelope );
	if( result.status == CommandStatus::Rejected )
	{
		state_.actionError = result.error ? result.error : ShellError{ message( "ui.error.action_rejected" ), false, std::nullopt };
		state_.pendingRequest.reset();
		if( envelope.id.value == "sim.set_paused" )
		{
			pendingPauseRequest_.reset();
			pendingPauseValue_.reset();
		}
		if( envelope.id.value == "app.save_game" )
		{
			pendingSaveRequest_.reset();
			state_.saveStatus = RequestStatus::Error;
		}
		notify();
		return false;
	}
	state_.actionError.reset();
	if( result.pending )
	{
		state_.pendingRequest = request;
		if( envelope.id.value == "sim.set_paused" )
		{
			pendingPauseRequest_ = request;
			if( const auto* pause = std::get_if<SetPausedPayload>( &envelope.payload ) ) pendingPauseValue_ = pause->paused;
		}
		if( envelope.id.value == "app.save_game" )
		{
			pendingSaveRequest_ = request;
			state_.saveStatus = RequestStatus::Loading;
		}
	}
	notify();
	return true;
}

void ShellController::navigate( std::string_view route )
{
	if( dispatch( action( "nav.open", RoutePayload{ RouteId{ route } } ) ) )
	{
		state_.route = RouteId{ route };
		notify();
	}
}

void ShellController::back()
{
	if( dispatch( action( "nav.back", NoPayload{} ) ) )
	{
		state_.route = RouteId{ state_.route.value == "game.settings" ? "game.pause" : "shell.main_menu" };
		notify();
	}
}

void ShellController::requestConfirmation( UiActionEnvelope pending, Message title, Message detail,
	FocusToken returnFocus, std::optional<RouteId> routeAfterAccepted )
{
	if(confirmation_) return;
	confirmation_ = PendingConfirmation{ std::move( pending ), std::move( title ), std::move( detail ),
		returnFocus, std::move( routeAfterAccepted ) };
	view_.showConfirmation( confirmation_->title, confirmation_->detail, returnFocus );
}

void ShellController::activate( ShellControl control, FocusToken sourceFocus )
{
	switch( control )
	{
	case ShellControl::ContinueLastGame:
		if( state_.continueAvailable )
		{
			auto request = action( "app.continue_last_game", NoPayload{} );
			if( dispatch( request ) ) beginWorldTransition( false, request );
		}
		break;
	case ShellControl::StartDefaultGame:
		if( state_.newGame.status == RequestStatus::Ready )
		{
			auto request = action( "app.start_new_game", StartNewGamePayload{ state_.newGame.acceptedDraft } );
			if( dispatch( request ) ) beginWorldTransition( true, request );
		}
		break;
	case ShellControl::StartTutorial:
	{
		auto request = action( "app.start_tutorial", NoPayload{} );
		if( dispatch( request ) ) beginWorldTransition( true, request );
		break;
	}
	case ShellControl::OpenNewGame: navigate( "shell.new_game" ); break;
	case ShellControl::OpenLoadGame: navigate( "shell.load_game" ); break;
	case ShellControl::OpenSettings: navigate( "shell.settings" ); break;
	case ShellControl::Exit:
		requestConfirmation( action( "app.exit", NoPayload{} ), message( "ui.confirm.exit.title" ),
			message( "ui.confirm.exit.detail" ), sourceFocus );
		break;
	case ShellControl::Back: back(); break;
	case ShellControl::StartConfiguredGame:
		if( state_.newGame.status == RequestStatus::Ready && state_.newGame.validationErrors.empty() )
		{
			auto request = action( "app.start_new_game", StartNewGamePayload{ state_.newGame.draft } );
			if( dispatch( request ) ) beginWorldTransition( true, request );
		}
		break;
	case ShellControl::RefreshLoads: dispatch( action( "load.refresh", NoPayload{} ) ); break;
	case ShellControl::LoadSelected:
		if( state_.loadGame.selectedSlot )
		{
			const auto selected = std::ranges::find_if( state_.loadGame.saves, [&]( const SaveSlotRow& row ) {
				return row.id == *state_.loadGame.selectedSlot;
			} );
			if( selected != state_.loadGame.saves.end() && selected->compatible )
			{
				auto request = action( "app.load_game", LoadGamePayload{ selected->id } );
				if( dispatch( request ) ) beginWorldTransition( false, request );
			}
		}
		break;
	case ShellControl::RandomizeName: dispatch( action( "new_game.randomize_name", NoPayload{} ) ); break;
	case ShellControl::RandomizeSeed: dispatch( action( "new_game.randomize_seed", NoPayload{} ) ); break;
	case ShellControl::ApplySettings: dispatch( action( "settings.apply", NoPayload{} ) ); break;
	case ShellControl::RevertSettings: dispatch( action( "settings.revert", NoPayload{} ) ); break;
	case ShellControl::ResetSettings: dispatch( action( "settings.reset", NoPayload{} ) ); break;
	case ShellControl::TogglePause:
	{
		const bool paused = pendingPauseValue_.value_or( state_.authoritativePaused );
		dispatch( action( "sim.set_paused", SetPausedPayload{ !paused }, true ) );
		break;
	}
	case ShellControl::OpenPause:
		if( dispatch( action( "sim.set_paused", SetPausedPayload{ true }, true ) ) ) navigate( "game.pause" );
		break;
	case ShellControl::OpenPauseMenu:
		navigate( "game.pause" );
		break;
	case ShellControl::Resume:
		closePauseWhenUnpaused_ = dispatch( action( "sim.set_paused", SetPausedPayload{ false }, true ) );
		break;
	case ShellControl::Save: dispatch( action( "app.save_game", NoPayload{}, true ) ); break;
	case ShellControl::LoadFromPause:
		requestConfirmation( action( "app.end_world", NoPayload{}, true ), message( "ui.confirm.end_world.title" ),
			message( "ui.confirm.load_other_world.detail" ), sourceFocus, RouteId{ "shell.load_game" } );
		break;
	case ShellControl::SettingsFromPause: navigate( "game.settings" ); break;
	case ShellControl::ReturnToMenu:
		requestConfirmation( action( "app.end_world", NoPayload{}, true ), message( "ui.confirm.end_world.title" ),
			message( "ui.confirm.end_world.detail" ), sourceFocus, RouteId{ "shell.main_menu" } );
		break;
	case ShellControl::ConfirmDestructive:
		if( confirmation_ )
		{
			routeAfterConfirmedAction_ = confirmation_->routeAfterAccepted;
			auto pending = std::move( confirmation_->action );
			confirmation_.reset();
			view_.closeConfirmation();
			dispatch( std::move( pending ) );
		}
		break;
	case ShellControl::CancelDestructive:
		if( confirmation_ )
		{
			const auto focus = confirmation_->returnFocus;
			confirmation_.reset();
			view_.closeConfirmation();
			view_.restoreFocus( focus );
		}
		break;
	case ShellControl::Retry:
		if( state_.lifecycle.error && state_.lifecycle.error->retryAction )
		{
			auto retry = *state_.lifecycle.error->retryAction;
			retry.request = RequestId{ nextRequest_++ };
			const bool generating = retry.id.value == "app.start_new_game";
			if( dispatch( retry ) ) beginWorldTransition( generating, retry );
		}
		break;
	}
}

void ShellController::selectKingdom( SaveKingdomId id )
{
	if( std::ranges::none_of( state_.loadGame.kingdoms, [&]( const SaveKingdomRow& row ) { return row.id == id; } ) ) return;
	state_.loadGame.selectedKingdom = id;
	state_.loadGame.selectedSlot.reset();
	dispatch( action( "load.select_kingdom", SelectKingdomPayload{ std::move( id ) } ) );
}

void ShellController::selectSave( SaveSlotId id )
{
	if( std::ranges::none_of( state_.loadGame.saves, [&]( const SaveSlotRow& row ) { return row.id == id; } ) ) return;
	state_.loadGame.selectedSlot = std::move( id );
	notify();
}

void ShellController::moveKingdomSelection( int delta )
{
	if( state_.loadGame.kingdoms.empty() || delta == 0 ) return;
	auto current = std::ranges::find_if( state_.loadGame.kingdoms, [&]( const SaveKingdomRow& row ) {
		return state_.loadGame.selectedKingdom && row.id == *state_.loadGame.selectedKingdom;
	} );
	const auto index = current == state_.loadGame.kingdoms.end() ? 0 : std::distance( state_.loadGame.kingdoms.begin(), current );
	const auto target = std::clamp<std::ptrdiff_t>( index + delta, std::ptrdiff_t{ 0 },
		static_cast<std::ptrdiff_t>( state_.loadGame.kingdoms.size() - 1 ) );
	selectKingdom( state_.loadGame.kingdoms[static_cast<std::size_t>( target )].id );
}

void ShellController::moveSaveSelection( int delta )
{
	if( state_.loadGame.saves.empty() || delta == 0 ) return;
	auto current = std::ranges::find_if( state_.loadGame.saves, [&]( const SaveSlotRow& row ) {
		return state_.loadGame.selectedSlot && row.id == *state_.loadGame.selectedSlot;
	} );
	const auto index = current == state_.loadGame.saves.end() ? 0 : std::distance( state_.loadGame.saves.begin(), current );
	const auto target = std::clamp<std::ptrdiff_t>( index + delta, std::ptrdiff_t{ 0 },
		static_cast<std::ptrdiff_t>( state_.loadGame.saves.size() - 1 ) );
	selectSave( state_.loadGame.saves[static_cast<std::size_t>( target )].id );
}

void ShellController::setSettingDraft( SettingId id, SettingValue value )
{
	const auto row = std::ranges::find_if( state_.settings.rows, [&]( const SettingRow& candidate ) { return candidate.id == id; } );
	if( row == state_.settings.rows.end() ) return;
	if( row->authoritative.index() != value.index() ) return;
	const auto inRange = [&]( const auto& candidate ) {
		using Value = std::decay_t<decltype( candidate )>;
		if constexpr( std::is_arithmetic_v<Value> && !std::is_same_v<Value, bool> )
		{
			if( row->minimum && std::get<Value>( *row->minimum ) > candidate ) return false;
			if( row->maximum && std::get<Value>( *row->maximum ) < candidate ) return false;
		}
		return true;
	};
	if( !std::visit( inRange, value ) ) return;
	dispatch( action( "settings.set_draft", SetSettingDraftPayload{ std::move( id ), std::move( value ) } ) );
}

void ShellController::setNewGameField( NewGameFieldId id, NewGameScalarValue value )
{
	const auto field = std::ranges::find_if( state_.newGame.draft.fields, [&]( const NewGameFieldValue& candidate ) {
		return candidate.field == id;
	} );
	if( field != state_.newGame.draft.fields.end() && field->value.index() != value.index() ) return;
	if( !dispatch( action( "new_game.set_field", SetNewGameFieldPayload{ id, value } ) ) ) return;
	if( field == state_.newGame.draft.fields.end() )
		state_.newGame.draft.fields.push_back( NewGameFieldValue{ std::move( id ), std::move( value ) } );
	else
		field->value = std::move( value );
	state_.newGame.acceptedDraft = state_.newGame.draft;
	state_.newGame.validationErrors.clear();
	for( const auto& candidate : state_.newGame.draft.fields )
		if( !validNewGameField( candidate ) ) state_.newGame.validationErrors.push_back( message( "ui.error.invalid_payload" ) );
	notify();
}

void ShellController::notify()
{
	view_.stateChanged( state_ );
}

} // namespace ingnomia::ui::shell
