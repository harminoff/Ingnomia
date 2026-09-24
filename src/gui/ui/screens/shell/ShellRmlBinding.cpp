/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "ShellRmlBinding.h"
#include "../../localization/RmlText.h"

#include "ShellRmlAdapter.h"
#include "../../state/UiRegistries.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <QDebug>

#include <array>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

namespace ingnomia::ui::shell
{
namespace
{
Rml::String rml( std::string_view value ) { return { value.data(), value.size() }; }

std::string messageText( const localization::UiText& text, const Message& message )
{
	std::vector<localization::TextArgument> arguments;arguments.reserve(message.arguments.size());for(std::size_t i=0;i<message.arguments.size();++i)arguments.push_back({std::to_string(i),message.arguments[i]});return text.format(message.key,arguments);
}

std::string escape( std::string_view value )
{
	std::string result;
	result.reserve( value.size() );
	for( const char character : value )
	{
		switch( character )
		{
		case '&': result += "&amp;"; break;
		case '<': result += "&lt;"; break;
		case '>': result += "&gt;"; break;
		case '"': result += "&quot;"; break;
		default: result += character; break;
		}
	}
	return result;
}

std::string quantity( std::string value, std::string_view singular, std::string_view plural )
{
	if ( value == "1" ) value += " " + std::string( singular );
	else value += " " + std::string( plural );
	return value;
}
}

ShellRmlBinding::ShellRmlBinding( Rml::Context& context ) : context_( context ) {}
ShellRmlBinding::~ShellRmlBinding() { shutdown(); }

bool ShellRmlBinding::initialize( ShellController& controller )
{
	if( initialized() ) return false;
	controller_ = &controller;
	if( !createModels() ) return false;
	appShell_ = context_.LoadDocument( "documents/app_shell.rml" );
	if( !appShell_ ) return false;
	localization::applyRmlText( *appShell_, textCatalog_ );
	appShell_->Show();
	routeValue_ = controller.state().route.value;
	return loadRoute( controller.state().route.value );
}

void ShellRmlBinding::shutdown()
{
	detachModalListeners();
	detachRouteListeners();
	detachLoadRowListeners();
	if( confirmation_ ) { context_.UnloadDocument( confirmation_ ); confirmation_ = nullptr; }
	if( routeDocument_ ) { context_.UnloadDocument( routeDocument_ ); routeDocument_ = nullptr; }
	if( appShell_ ) { context_.UnloadDocument( appShell_ ); appShell_ = nullptr; }
	callbacks_.clear();
	modalCallbacks_.clear();
	settingCallbacks_.clear();
	newGameFieldCallbacks_.clear();
	newGameTabCallbacks_.clear();
	loadRowCallbacks_.clear();
	loadRowListeners_.clear();
	shellModel_ = {};
	settingsModel_ = {};
	newGameModel_ = {};
	loadGameModel_ = {};
	controller_ = nullptr;
}

bool ShellRmlBinding::reloadDocuments()
{
	if ( !controller_ || !appShell_ ) return false;
	const bool wasVisible = appShell_->IsVisible();
	detachModalListeners();
	detachRouteListeners();
	detachLoadRowListeners();
	if ( confirmation_ ) { context_.UnloadDocument( confirmation_ ); confirmation_ = nullptr; }
	if ( routeDocument_ ) { context_.UnloadDocument( routeDocument_ ); routeDocument_ = nullptr; }
	context_.UnloadDocument( appShell_ );
	appShell_ = context_.LoadDocument( "documents/app_shell.rml" );
	if ( !appShell_ ) return false;
	localization::applyRmlText( *appShell_, textCatalog_ );
	appShell_->Show();
	routeValue_ = controller_->state().route.value;
	if ( !loadRoute( routeValue_ ) ) return false;
	stateChanged( controller_->state() );
	setInMenu( wasVisible );
	return true;
}

void ShellRmlBinding::setInMenu( bool inMenu )
{
	if ( !appShell_ ) return;
	if ( inMenu )
	{
		appShell_->Show();
		if ( routeDocument_ ) routeDocument_->Show();
	}
	else
	{
		if ( confirmation_ ) confirmation_->Hide();
		if ( routeDocument_ ) routeDocument_->Hide();
		appShell_->Hide();
	}
}

// The in-game HUD is owned by HudRmlBinding.  The shell still owns the route
// state, but must not load a second copy of screens/game_hud.rml: that copy
// would sit above the bound HUD document and swallow every native click.
bool ShellRmlBinding::initialized() const noexcept { return controller_ && appShell_; }

bool ShellRmlBinding::createModels()
{
	auto shell = context_.CreateDataModel( "ui_shell" );
	if( !shell ) return false;
	shell.Bind( "route", &routeValue_ );
	shell.Bind( "version", &versionValue_ );
	shell.Bind( "status", &statusValue_ );
	shell.Bind( "pending", &pendingValue_ );
	shell.Bind( "continue_available", &continueValue_ );
	shell.Bind( "paused", &pausedValue_ );
	shellModel_ = shell.GetModelHandle();

	auto settings = context_.CreateDataModel( "ui_settings" );
	if( !settings ) return false;
	settings.Bind( "supported_count", &supportedSettingsCount_ );
	settingsModel_ = settings.GetModelHandle();

	auto newGame = context_.CreateDataModel( "ui_new_game" );
	if( !newGame ) return false;
	newGame.Bind( "validation_error_count", &newGameErrorCount_ );
	newGameModel_ = newGame.GetModelHandle();

	auto loadGame = context_.CreateDataModel( "ui_load_game" );
	if( !loadGame ) return false;
	loadGame.Bind( "kingdom_count", &kingdomCount_ );
	loadGame.Bind( "save_count", &saveCount_ );
	loadGameModel_ = loadGame.GetModelHandle();
	return true;
}

bool ShellRmlBinding::loadRoute( std::string_view route )
{
	const auto* definition = UiRegistries::findRoute( route );
	if( !definition ) return false;
	const auto* document = UiRegistries::findDocument( definition->documentId );
	if( !document || document->builtIn || document->path.empty() ) return false;
	detachRouteListeners();
	detachLoadRowListeners();
	if( routeDocument_ ) context_.UnloadDocument( routeDocument_ );
	if( route == "game.hud" )
	{
		routeDocument_ = nullptr;
		callbacks_.clear();
		settingCallbacks_.clear();
		newGameFieldCallbacks_.clear();
		newGameTabCallbacks_.clear();
		loadRowCallbacks_.clear();
		qInfo() << "Shell route delegated to dedicated HUD binding";
		return true;
	}
	callbacks_.clear();
	settingCallbacks_.clear();
	newGameFieldCallbacks_.clear();
	newGameTabCallbacks_.clear();
	loadRowCallbacks_.clear();
	routeDocument_ = context_.LoadDocument( rml( document->path ) );
	if( !routeDocument_ ) return false;
	localization::applyRmlText( *routeDocument_, textCatalog_ );
	bindCallbacks();
	if( route == "shell.new_game" )
	{
		activeNewGameTab_ = "world";
		syncNewGameTabs();
	}
	routeDocument_->Show();
	focusInitial( route );
	qInfo().noquote() << "Shell route loaded:" << QString::fromUtf8( route.data(), static_cast<qsizetype>( route.size() ) )
		<< "document" << QString::fromUtf8( document->path.data(), static_cast<qsizetype>( document->path.size() ) );
	return true;
}

void ShellRmlBinding::Callback::ProcessEvent( Rml::Event& event )
{
	event.StopPropagation();
	if( event.GetCurrentElement() && event.GetCurrentElement()->HasAttribute( "disabled" ) ) return;
	owner_.controller_->activate( control_ );
}

void ShellRmlBinding::SettingCallback::ProcessEvent( Rml::Event& event )
{
	if( owner_.updatingDom_ ) return;
	auto* element = event.GetCurrentElement();
	if( !element ) return;
	if( setting_.value == "display.fullscreen" || setting_.value == "display.follow_monitor_refresh" || setting_.value == "camera.wheel_changes_level" || setting_.value == "game.autosave_continue" )
		owner_.controller_->setSettingDraft( setting_, event.GetParameter<bool>( "checked", element->HasAttribute( "checked" ) ) );
	else if( setting_.value == "interface.ui_scale" )
	{
		// RmlUi's range widget dispatches Change with the new value first and
		// updates the element's value attribute immediately afterwards. Read the
		// event payload or this callback re-dispatches the previous slider value.
		const auto value = event.GetParameter<float>( "value", element->GetAttribute( "value", 0.0f ) );
		owner_.controller_->setSettingDraft( setting_, value / 100.0f );
	}
	else
	{
		const auto value = event.GetParameter<float>( "value", element->GetAttribute( "value", 0.0f ) );
		owner_.controller_->setSettingDraft( setting_, static_cast<std::int32_t>( value ) );
	}
}

void ShellRmlBinding::NewGameFieldCallback::ProcessEvent( Rml::Event& event )
{
	if( owner_.updatingDom_ ) return;
	auto* element = event.GetCurrentElement();
	if( !element ) return;
	if( field_.value == "peaceful" )
	{
		owner_.controller_->setNewGameField( field_, element->HasAttribute( "checked" ) );
		return;
	}
	if( field_.value == "kingdom_name" || field_.value == "seed" )
	{
		const auto value = event.GetParameter<Rml::String>( "value", element->GetAttribute<Rml::String>( "value", "" ) );
		owner_.controller_->setNewGameField( field_, std::string( value.data(), value.size() ) );
		return;
	}
	try
	{
		const auto value = event.GetParameter<float>( "value", element->GetAttribute( "value", 0.0f ) );
		owner_.controller_->setNewGameField( field_, static_cast<std::int32_t>( std::lround( value ) ) );
	}
	catch( ... )
	{
		// RmlUi may deliver an intermediate empty value while a range/input is edited.
	}
}

void ShellRmlBinding::NewGameTabCallback::ProcessEvent( Rml::Event& event )
{
	event.StopPropagation();
	owner_.selectNewGameTab( tab_ );
}

void ShellRmlBinding::LoadRowCallback::ProcessEvent( Rml::Event& event )
{
	if( !owner_.controller_ ) return;
	event.StopPropagation();
	if( kind_ == Kind::Kingdom ) owner_.controller_->selectKingdom( SaveKingdomId{ id_ } );
	else owner_.controller_->selectSave( SaveSlotId{ id_ } );
}

void ShellRmlBinding::bindCallbacks()
{
	static constexpr const char* ids[] = { "shell-continue", "shell-tutorial", "shell-new-default", "shell-new-setup",
		"shell-load", "shell-settings", "shell-exit", "shell-back", "new-start", "new-random-name",
		"new-random-seed", "load-refresh", "load-selected", "settings-apply", "settings-revert",
		"settings-reset", "pause-resume", "pause-save", "pause-load", "pause-settings", "pause-menu",
		"loading-retry" };
	for( const char* id : ids ) if( const auto control = ShellRmlAdapter::controlForElement( id ) ) bindCallback( id, *control );
	bindSettingCallback( "setting-fullscreen", "display.fullscreen" );
	bindSettingCallback( "setting-follow-refresh", "display.follow_monitor_refresh" );
	bindSettingCallback( "setting-frame-rate", "display.frame_rate_limit" );
	bindSettingCallback( "setting-ui-scale", "interface.ui_scale" );
	bindSettingCallback( "setting-keyboard-speed", "camera.keyboard_pan_speed" );
	bindSettingCallback( "setting-minimum-light", "display.minimum_light" );
	bindSettingCallback( "setting-wheel-level", "camera.wheel_changes_level" );
	bindSettingCallback( "setting-master-volume", "audio.master_volume" );
	bindSettingCallback( "setting-autosave-interval", "game.autosave_interval" );
	bindSettingCallback( "setting-autosave-continue", "game.autosave_continue" );
	bindNewGameFieldCallback( "new-kingdom-name", "kingdom_name" );
	bindNewGameFieldCallback( "new-seed", "seed" );
	bindNewGameFieldCallback( "new-peaceful", "peaceful" );
	bindNewGameFieldCallback( "new-world-size", "world_size" );
	bindNewGameFieldCallback( "new-z-levels", "z_levels" );
	bindNewGameFieldCallback( "new-ground", "ground" );
	bindNewGameFieldCallback( "new-flatness", "flatness" );
	bindNewGameFieldCallback( "new-ocean-size", "ocean_size" );
	bindNewGameFieldCallback( "new-rivers", "rivers" );
	bindNewGameFieldCallback( "new-river-size", "river_size" );
	bindNewGameFieldCallback( "new-tree-density", "tree_density" );
	bindNewGameFieldCallback( "new-plant-density", "plant_density" );
	bindNewGameFieldCallback( "new-wild-animals", "wild_animals" );
	bindNewGameFieldCallback( "new-gnomes", "gnomes" );
	bindNewGameFieldCallback( "new-start-zone", "start_zone" );
	bindNewGameTabCallback( "new-tab-world", "world" );
	bindNewGameTabCallback( "new-tab-settlement", "settlement" );
	bindNewGameTabCallback( "new-tab-terrain", "terrain" );
	bindNewGameTabCallback( "new-tab-review", "review" );
}

void ShellRmlBinding::bindSettingCallback( const char* id, const char* setting )
{
	if( !routeDocument_ ) return;
	if( auto* element = routeDocument_->GetElementById( id ) )
	{
		auto callback = std::make_unique<SettingCallback>( *this, SettingId{ setting } );
		 element->AddEventListener( Rml::EventId::Change, callback.get(), false );
		routeListeners_.push_back( { element, "change", callback.get() } );
		settingCallbacks_.push_back( std::move( callback ) );
	}
}

void ShellRmlBinding::bindNewGameFieldCallback( const char* id, const char* field )
{
	if( !routeDocument_ ) return;
	if( auto* element = routeDocument_->GetElementById( id ) )
	{
		auto callback = std::make_unique<NewGameFieldCallback>( *this, NewGameFieldId{ field } );
		element->AddEventListener( Rml::EventId::Change, callback.get(), false );
		routeListeners_.push_back( { element, "change", callback.get() } );
		newGameFieldCallbacks_.push_back( std::move( callback ) );
	}
}

void ShellRmlBinding::bindNewGameTabCallback( const char* id, const char* tab )
{
	if( !routeDocument_ ) return;
	if( auto* element = routeDocument_->GetElementById( id ) )
	{
		auto callback = std::make_unique<NewGameTabCallback>( *this, tab );
		element->AddEventListener( Rml::EventId::Click, callback.get(), false );
		routeListeners_.push_back( { element, "click", callback.get() } );
		newGameTabCallbacks_.push_back( std::move( callback ) );
	}
}

void ShellRmlBinding::bindCallback( const char* id, ShellControl control )
{
	if( !routeDocument_ ) return;
	if( auto* element = routeDocument_->GetElementById( id ) )
	{
		auto callback = std::make_unique<Callback>( *this, control );
		element->AddEventListener( Rml::EventId::Click, callback.get(), false );
		routeListeners_.push_back( { element, "click", callback.get() } );
		callbacks_.push_back( std::move( callback ) );
	}
}

void ShellRmlBinding::selectNewGameTab( std::string_view tab )
{
	static constexpr std::array tabs{
		std::pair{ "world", "new-tab-world" },
		std::pair{ "settlement", "new-tab-settlement" },
		std::pair{ "terrain", "new-tab-terrain" },
		std::pair{ "review", "new-tab-review" } };
	static constexpr std::array panels{
		std::pair{ "world", "new-panel-world" },
		std::pair{ "settlement", "new-panel-settlement" },
		std::pair{ "terrain", "new-panel-terrain" },
		std::pair{ "review", "new-panel-review" } };
	const auto valid = std::ranges::find_if( tabs, [&]( const auto& entry ) { return tab == entry.first; } );
	if( valid == tabs.end() || !routeDocument_ ) return;
	activeNewGameTab_ = std::string( tab );
	for( const auto& [name, id] : tabs )
	{
		if( auto* element = routeDocument_->GetElementById( id ) )
		{
			const bool selected = name == activeNewGameTab_;
			element->SetClass( "is-selected", selected );
			element->SetAttribute( "aria-selected", selected ? "true" : "false" );
			element->SetAttribute( "tab-index", selected ? "0" : "-1" );
		}
	}
	for( const auto& [name, id] : panels ) setVisible( id, name == activeNewGameTab_ );
}

void ShellRmlBinding::syncNewGameTabs()
{
	selectNewGameTab( activeNewGameTab_ );
}

bool ShellRmlBinding::activateElement( std::string_view id )
{
	static constexpr std::array tabs{
		std::pair{ "new-tab-world", "world" },
		std::pair{ "new-tab-settlement", "settlement" },
		std::pair{ "new-tab-terrain", "terrain" },
		std::pair{ "new-tab-review", "review" } };
	if( const auto tab = std::ranges::find_if( tabs, [&]( const auto& entry ) { return id == entry.first; } ); tab != tabs.end() )
	{
		if( !controller_ || routeValue_ != "shell.new_game" ) return false;
		selectNewGameTab( tab->second );
		return true;
	}
	const auto control = ShellRmlAdapter::controlForElement( id );
	if( !controller_ || !control ) return false;
	if( *control == ShellControl::ContinueLastGame && !controller_->state().continueAvailable ) return false;
	controller_->activate( *control, FocusToken{ ++lastFocusToken_ } );
	return true;
}

bool ShellRmlBinding::dispatchSettingChangeForProbe( std::string_view id, float value, bool checked )
{
	if( !routeDocument_ || routeValue_ != "shell.settings" ) return false;
	if( auto* element = routeDocument_->GetElementById( std::string( id ) ) )
	{
		Rml::Dictionary parameters;
		parameters["value"] = value;
		parameters["checked"] = checked;
		element->DispatchEvent( Rml::EventId::Change, parameters );
		return true;
	}
	return false;
}

void ShellRmlBinding::stateChanged( const ShellState& state )
{
	if( routeValue_ != state.route.value ) loadRoute( state.route.value );
	syncModel( state );
	syncDom( state );
}

void ShellRmlBinding::syncModel( const ShellState& state )
{
	routeValue_ = state.route.value;
	versionValue_ = state.version;
	statusValue_ = state.actionError ? messageText( textCatalog_, state.actionError->message ) : std::string{};
	pendingValue_ = state.pendingRequest.has_value();
	continueValue_ = state.continueAvailable;
	pausedValue_ = state.authoritativePaused;
	supportedSettingsCount_ = static_cast<int>( state.settings.rows.size() );
	newGameErrorCount_ = static_cast<int>( state.newGame.validationErrors.size() );
	kingdomCount_ = static_cast<int>( state.loadGame.kingdoms.size() );
	saveCount_ = static_cast<int>( state.loadGame.saves.size() );
	for( auto* model : { &shellModel_, &settingsModel_, &newGameModel_, &loadGameModel_ } ) model->DirtyAllVariables();
}

void ShellRmlBinding::syncDom( const ShellState& state )
{
	updatingDom_ = true;
	setText( "shell-version", state.version.empty() ? "Version unavailable" : state.version );
	setEnabled( "shell-continue", state.continueAvailable );
	setText( "shell-continue-reason", state.continueAvailable
		? escape( state.continueSaveName + " · Last saved " + state.continueSavedAt )
		: "No compatible save found" );
	setVisible( "shell-action-error", state.actionError.has_value() );
	if( state.actionError ) setText( "shell-action-error-detail", messageText( textCatalog_, state.actionError->message ) );
	setVisible( "load-kingdoms-empty", state.loadGame.kingdomsStatus == RequestStatus::Empty );
	setVisible( "load-saves-empty", state.loadGame.savesStatus == RequestStatus::Empty );
	setText( "load-saves-empty-title", state.loadGame.selectedKingdom ? "No saves for this kingdom" : "Select a kingdom" );
	setText( "load-saves-empty-detail", state.loadGame.selectedKingdom ? "Start a new kingdom or refresh the save list." : "Choose a kingdom to see its saves." );
	// Empty-state cards replace their list rather than layering over it. Leaving
	// the empty list in the flow makes the message appear at the bottom of the
	// pane and can cover the footer controls on the load screen.
	setVisible( "load-kingdoms", state.loadGame.kingdomsStatus != RequestStatus::Empty );
	setVisible( "load-saves", state.loadGame.savesStatus != RequestStatus::Empty );
	setVisible( "load-error", state.loadGame.error.has_value() );
	if( state.loadGame.error ) setText( "load-error-detail", messageText( textCatalog_, state.loadGame.error->message ) );
	const auto selectedSave = state.loadGame.selectedSlot
		? std::ranges::find_if( state.loadGame.saves, [&]( const SaveSlotRow& row ) { return row.id == *state.loadGame.selectedSlot; } )
		: state.loadGame.saves.end();
	setEnabled( "load-selected", selectedSave != state.loadGame.saves.end() && selectedSave->compatible && !state.pendingRequest );
	if( routeDocument_ )
	{
		if( auto* kingdoms = routeDocument_->GetElementById( "load-kingdoms" ) )
		{
			std::string markup;
			detachLoadRowListeners();
			loadRowCallbacks_.clear();
			for( std::size_t index = 0; index < state.loadGame.kingdoms.size(); ++index )
			{
				const auto& row = state.loadGame.kingdoms[index];
				// Keep the DOM id transport-safe. The typed SaveKingdomId remains the
				// callback payload; it must not be exposed as an RML identifier.
				const auto elementId = "kingdom-row-" + std::to_string( index );
				markup += "<button id=\"" + elementId + "\" class=\"c-list__row";
				if( state.loadGame.selectedKingdom && *state.loadGame.selectedKingdom == row.id ) markup += " is-selected";
				markup += "\" data-load-row=\"kingdom\"><span class=\"c-list__primary\">" + escape( row.displayName ) + "</span></button>";
			}
			kingdoms->SetInnerRML( rml( markup ) );
			for( std::size_t index = 0; index < state.loadGame.kingdoms.size(); ++index )
			{
				const auto& row = state.loadGame.kingdoms[index];
				if( auto* element = kingdoms->GetElementById( rml( "kingdom-row-" + std::to_string( index ) ) ) )
				{
					auto callback = std::make_unique<LoadRowCallback>( *this, LoadRowCallback::Kind::Kingdom, row.id.relativeKey );
					element->AddEventListener( Rml::EventId::Click, callback.get(), false );
					loadRowListeners_.push_back( { element, "click", callback.get() } );
					loadRowCallbacks_.push_back( std::move( callback ) );
				}
			}
		}
		if( auto* saves = routeDocument_->GetElementById( "load-saves" ) )
		{
			std::string markup;
			for( std::size_t index = 0; index < state.loadGame.saves.size(); ++index )
			{
				const auto& row = state.loadGame.saves[index];
				const auto elementId = "save-row-" + std::to_string( index );
				markup += "<button id=\"" + elementId + "\" class=\"c-list__row";
				if( state.loadGame.selectedSlot && *state.loadGame.selectedSlot == row.id ) markup += " is-selected";
				markup += "\"><span class=\"c-list__primary\">" + escape( row.displayName )
					+ "</span><span class=\"c-list__meta\">" + escape( row.version ) + ( row.compatible ? " Compatible" : " Incompatible" ) + "</span></button>";
			}
			saves->SetInnerRML( rml( markup ) );
			for( std::size_t index = 0; index < state.loadGame.saves.size(); ++index )
			{
				const auto& row = state.loadGame.saves[index];
				if( auto* element = saves->GetElementById( rml( "save-row-" + std::to_string( index ) ) ) )
				{
					auto callback = std::make_unique<LoadRowCallback>( *this, LoadRowCallback::Kind::Save, row.id.relativeKey );
					element->AddEventListener( Rml::EventId::Click, callback.get(), false );
					loadRowListeners_.push_back( { element, "click", callback.get() } );
					loadRowCallbacks_.push_back( std::move( callback ) );
				}
			}
		}
	}
	setVisible( "settings-loading", state.settings.status == RequestStatus::Loading );
	setVisible( "settings-supported", state.settings.status == RequestStatus::Ready );
	setVisible( "settings-error", state.settings.error.has_value() );
	bool followMonitorRefresh = true;
	for( const auto& row : state.settings.rows )
	{
		const char* elementId = nullptr;
		if( row.id.value == "display.fullscreen" ) elementId = "setting-fullscreen";
		else if( row.id.value == "display.follow_monitor_refresh" ) elementId = "setting-follow-refresh";
		else if( row.id.value == "display.frame_rate_limit" ) elementId = "setting-frame-rate";
		else if( row.id.value == "interface.ui_scale" ) elementId = "setting-ui-scale";
		else if( row.id.value == "camera.keyboard_pan_speed" ) elementId = "setting-keyboard-speed";
		else if( row.id.value == "display.minimum_light" ) elementId = "setting-minimum-light";
		else if( row.id.value == "camera.wheel_changes_level" ) elementId = "setting-wheel-level";
		else if( row.id.value == "audio.master_volume" ) elementId = "setting-master-volume";
		else if( row.id.value == "game.autosave_interval" ) elementId = "setting-autosave-interval";
		else if( row.id.value == "game.autosave_continue" ) elementId = "setting-autosave-continue";
		if( !elementId || !routeDocument_ ) continue;
		const char* valueId = nullptr;
		if( row.id.value == "display.frame_rate_limit" ) valueId = "setting-frame-rate-value";
		else if( row.id.value == "interface.ui_scale" ) valueId = "setting-ui-scale-value";
		else if( row.id.value == "display.minimum_light" ) valueId = "setting-minimum-light-value";
		else if( row.id.value == "camera.keyboard_pan_speed" ) valueId = "setting-keyboard-speed-value";
		else if( row.id.value == "audio.master_volume" ) valueId = "setting-master-volume-value";
		else if( row.id.value == "game.autosave_interval" ) valueId = "setting-autosave-interval-value";
		if( auto* element = routeDocument_->GetElementById( elementId ) )
		{
			if( const auto* boolValue = std::get_if<bool>( &row.authoritative ) )
			{
				if( *boolValue ) element->SetAttribute( "checked", "checked" ); else element->RemoveAttribute( "checked" );
				if( row.id.value == "display.follow_monitor_refresh" ) followMonitorRefresh = *boolValue;
			}
			else if( const auto* intValue = std::get_if<std::int32_t>( &row.authoritative ) )
			{
				element->SetAttribute( "value", *intValue );
				if( valueId )
				{
					const auto suffix = row.id.value == "display.frame_rate_limit" ? " FPS" : row.id.value == "display.minimum_light" || row.id.value == "audio.master_volume" ? "%" : row.id.value == "game.autosave_interval" ? ( *intValue == 1 ? " day" : " days" ) : "";
					setText( valueId, std::to_string( *intValue ) + suffix );
				}
			}
			else if( const auto* floatValue = std::get_if<float>( &row.authoritative ) )
			{
				element->SetAttribute( "value", *floatValue * 100.0f );
				if( valueId ) setText( valueId, std::to_string( static_cast<int>( std::lround( *floatValue * 100.0f ) ) ) + "%" );
			}
		}
	}
	setEnabled( "setting-frame-rate", !followMonitorRefresh );
	setVisible( "new-loading", state.newGame.status == RequestStatus::Loading );
	setVisible( "new-form", state.newGame.status == RequestStatus::Ready );
	setVisible( "new-validation", !state.newGame.validationErrors.empty() );
	setEnabled( "new-start", state.newGame.status == RequestStatus::Ready && state.newGame.validationErrors.empty() && !state.pendingRequest );
	const auto syncNewGameField = [&]( const char* elementId, const char* fieldId ) {
		if( !routeDocument_ ) return;
		const auto field = std::ranges::find_if( state.newGame.draft.fields, [&]( const NewGameFieldValue& value ) {
			return value.field.value == fieldId;
		} );
		if( field == state.newGame.draft.fields.end() ) return;
		auto* element = routeDocument_->GetElementById( elementId );
		if( !element ) return;
		std::visit( [&]( const auto& value ) {
			using Value = std::decay_t<decltype( value )>;
			if constexpr( std::is_same_v<Value, bool> )
			{
				if( value ) element->SetAttribute( "checked", "checked" );
				else element->RemoveAttribute( "checked" );
			}
			else if constexpr( std::is_same_v<Value, std::string> )
			{
				if( auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( element ) ) input->SetValue( rml( value ) );
				else element->SetAttribute( "value", rml( value ) );
			}
			else if constexpr( std::is_same_v<Value, CatalogId> )
				element->SetAttribute( "value", rml( value.value ) );
			else element->SetAttribute( "value", value );
		}, field->value );
	};
	syncNewGameField( "new-kingdom-name", "kingdom_name" );
	syncNewGameField( "new-seed", "seed" );
	syncNewGameField( "new-peaceful", "peaceful" );
	syncNewGameField( "new-world-size", "world_size" );
	syncNewGameField( "new-z-levels", "z_levels" );
	syncNewGameField( "new-ground", "ground" );
	syncNewGameField( "new-flatness", "flatness" );
	syncNewGameField( "new-ocean-size", "ocean_size" );
	syncNewGameField( "new-rivers", "rivers" );
	syncNewGameField( "new-river-size", "river_size" );
	syncNewGameField( "new-tree-density", "tree_density" );
	syncNewGameField( "new-plant-density", "plant_density" );
	syncNewGameField( "new-wild-animals", "wild_animals" );
	syncNewGameField( "new-gnomes", "gnomes" );
	syncNewGameField( "new-start-zone", "start_zone" );
	const auto newGameFieldText = [&]( std::string_view fieldId, std::string fallback ) {
		const auto field = std::ranges::find_if( state.newGame.draft.fields, [&]( const NewGameFieldValue& value ) {
			return value.field.value == fieldId;
		} );
		if( field == state.newGame.draft.fields.end() ) return fallback;
		return std::visit( []( const auto& value ) -> std::string {
			using Value = std::decay_t<decltype( value )>;
			if constexpr( std::is_same_v<Value, bool> ) return value ? "Peaceful" : "Standard";
			else if constexpr( std::is_same_v<Value, std::string> ) return value;
			else if constexpr( std::is_same_v<Value, CatalogId> ) return value.value;
			else return std::to_string( value );
		}, field->value );
	};
	setText( "new-summary-kingdom", escape( newGameFieldText( "kingdom_name", "Unnamed kingdom" ) ) );
	setText( "new-summary-seed", escape( newGameFieldText( "seed", "Default seed" ) ) );
	setText( "new-summary-settlement", escape( quantity( newGameFieldText( "gnomes", "—" ), "settler", "settlers" ) + " / zone " + newGameFieldText( "start_zone", "—" ) + " / " + newGameFieldText( "peaceful", "Standard" ) ) );
	setText( "new-summary-world", escape( newGameFieldText( "world_size", "—" ) + " tiles / " + quantity( newGameFieldText( "z_levels", "—" ), "level", "levels" ) + " / ground " + newGameFieldText( "ground", "—" ) + " levels high / flatness " + newGameFieldText( "flatness", "—" ) ) );
	setText( "new-summary-life", escape( newGameFieldText( "ocean_size", "—" ) + " ocean / " + quantity( newGameFieldText( "rivers", "—" ), "river", "rivers" ) + " / trees " + newGameFieldText( "tree_density", "—" ) + " / plants " + newGameFieldText( "plant_density", "—" ) + " / wildlife " + quantity( newGameFieldText( "wild_animals", "—" ), "animal", "animals" ) ) );
	const auto setNewGameRangeValue = [&]( const char* elementId, std::string_view fieldId, std::string_view suffix ) {
		auto value = newGameFieldText( fieldId, "—" );
		if ( fieldId == "gnomes" ) value = quantity( std::move( value ), "settler", "settlers" );
		else if ( fieldId == "rivers" ) value = quantity( std::move( value ), "river", "rivers" );
		else if ( fieldId == "wild_animals" ) value = quantity( std::move( value ), "animal", "animals" );
		else value += std::string( suffix );
		setText( elementId, escape( value ) );
	};
	setNewGameRangeValue( "new-value-world-size", "world_size", " tiles" );
	setNewGameRangeValue( "new-value-z-levels", "z_levels", " levels" );
	setNewGameRangeValue( "new-value-ground", "ground", " levels high" );
	setNewGameRangeValue( "new-value-flatness", "flatness", " / 20" );
	setNewGameRangeValue( "new-value-gnomes", "gnomes", " settlers" );
	setNewGameRangeValue( "new-value-start-zone", "start_zone", " tiles from center" );
	setNewGameRangeValue( "new-value-ocean-size", "ocean_size", " / 15 edge-water" );
	setNewGameRangeValue( "new-value-rivers", "rivers", " rivers" );
	setNewGameRangeValue( "new-value-river-size", "river_size", " tiles wide" );
	setNewGameRangeValue( "new-value-tree-density", "tree_density", "% cover" );
	setNewGameRangeValue( "new-value-plant-density", "plant_density", "% abundance" );
	setNewGameRangeValue( "new-value-wild-animals", "wild_animals", " animals" );
	if( routeValue_ == "shell.new_game" ) syncNewGameTabs();
	setText( "pause-authoritative-state", state.authoritativePaused ? "Simulation paused" : "Pause request pending" );
	const auto localized = [this]( const char* key ) {
		return messageText( textCatalog_, Message{ LocalizationKey{ key }, {} } );
	};
	const auto saveStatus = state.saveStatus == RequestStatus::Loading ? localized( "ui.status.saving_game" )
		: state.saveStatus == RequestStatus::Ready ? localized( "ui.status.game_saved" )
		: state.saveStatus == RequestStatus::Error ? localized( "ui.error.save_failed" ) : std::string{};
	setText( "pause-save-status", saveStatus );
	const bool pauseActionAvailable = state.authoritativePaused && !state.pendingRequest;
	setEnabled( "pause-resume", pauseActionAvailable );
	setEnabled( "pause-save", pauseActionAvailable );
	setEnabled( "pause-load", pauseActionAvailable );
	setEnabled( "pause-settings", pauseActionAvailable );
	setEnabled( "pause-menu", pauseActionAvailable );
	setVisible( "pause-error", state.actionError.has_value() );
	if( state.actionError ) setText( "pause-error-detail", messageText( textCatalog_, state.actionError->message ) );
	setVisible( "loading-error", state.lifecycle.error.has_value() );
	setVisible( "loading-error-actions", state.lifecycle.error.has_value() );
	setEnabled( "loading-retry", state.lifecycle.error && state.lifecycle.error->retryable );
	if( !state.lifecycle.progressText.empty() ) setText( "loading-progress", state.lifecycle.progressText );
	else if( state.lifecycle.progress ) setText( "loading-progress", messageText( textCatalog_, *state.lifecycle.progress ) );
	updatingDom_ = false;
}

void ShellRmlBinding::setText( const char* id, std::string_view text )
{
	if( routeDocument_ ) if( auto* element = routeDocument_->GetElementById( id ) ) element->SetInnerRML( rml( text ) );
}

void ShellRmlBinding::setVisible( const char* id, bool visible )
{
	if( routeDocument_ ) if( auto* element = routeDocument_->GetElementById( id ) )
	{
		element->SetClass( "u-hidden", !visible );
		if( visible ) element->RemoveProperty( "display" );
		else element->SetProperty( "display", "none" );
	}
}

void ShellRmlBinding::setEnabled( const char* id, bool enabled )
{
	if( routeDocument_ ) if( auto* element = routeDocument_->GetElementById( id ) )
	{
		if( enabled ) element->RemoveAttribute( "disabled" );
		else element->SetAttribute( "disabled", "disabled" );
		element->SetAttribute( "tab-index", enabled ? "0" : "-1" );
	}
}

void ShellRmlBinding::focusInitial( std::string_view route )
{
	const auto id = route == "shell.main_menu" && controller_ && !controller_->state().continueAvailable
		? std::string_view{ "shell-load" } : ShellRmlAdapter::initialFocusForRoute( route );
	if( !id.empty() ) if( auto* element = routeDocument_->GetElementById( rml( id ) ) ) element->Focus();
}

void ShellRmlBinding::showConfirmation( const Message& title, const Message& detail, FocusToken returnFocus )
{
	lastFocusToken_ = returnFocus.value;
	if( confirmation_ ) { detachModalListeners(); context_.UnloadDocument( confirmation_ ); }
	confirmation_ = context_.LoadDocument( "modals/confirm_destructive.rml" );
	if( !confirmation_ ) return;
	localization::applyRmlText( *confirmation_, textCatalog_ );
	if( auto* element = confirmation_->GetElementById( "confirm-title" ) ) element->SetInnerRML( rml( messageText( textCatalog_, title ) ) );
	if( auto* element = confirmation_->GetElementById( "confirm-detail" ) ) element->SetInnerRML( rml( messageText( textCatalog_, detail ) ) );
	for( const char* id : { "confirm-cancel", "confirm-accept" } )
	{
		const auto control = ShellRmlAdapter::controlForElement( id );
		if( auto* element = confirmation_->GetElementById( id ); element && control )
		{
			auto callback = std::make_unique<Callback>( *this, *control );
			element->AddEventListener( Rml::EventId::Click, callback.get(), false );
			modalListeners_.push_back( { element, "click", callback.get() } );
			modalCallbacks_.push_back( std::move( callback ) );
		}
	}
	confirmation_->Show( Rml::ModalFlag::Modal );
	if( auto* cancel = confirmation_->GetElementById( "confirm-cancel" ) ) cancel->Focus();
}

void ShellRmlBinding::closeConfirmation()
{
	if( !confirmation_ ) return;
	detachModalListeners();
	context_.UnloadDocument( confirmation_ );
	confirmation_ = nullptr;
}

void ShellRmlBinding::restoreFocus( FocusToken token )
{
	lastFocusToken_ = token.value;
	focusInitial( routeValue_ );
}

void ShellRmlBinding::detachRouteListeners()
{
	for( const auto& binding : routeListeners_ )
		if( binding.element && binding.listener ) binding.element->RemoveEventListener( binding.event, binding.listener );
	routeListeners_.clear();
}

void ShellRmlBinding::detachModalListeners()
{
	for( const auto& binding : modalListeners_ )
		if( binding.element && binding.listener ) binding.element->RemoveEventListener( binding.event, binding.listener );
	modalListeners_.clear();
}

void ShellRmlBinding::detachLoadRowListeners()
{
	for( const auto& binding : loadRowListeners_ )
		if( binding.element && binding.listener ) binding.element->RemoveEventListener( binding.event, binding.listener );
	loadRowListeners_.clear();
}

} // namespace ingnomia::ui::shell
