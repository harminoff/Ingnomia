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
#include "../../runtime/SelectOptions.h"

#include <QDateTime>

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

ShellRmlBinding::ShellRmlBinding( Rml::Context& context ) : context_( context ), dialog_( context )
{
	// Exit and Main Menu ask in a Windows 98 message box (Stage 19).
	dialog_.setDocumentPath( "modals/win98_message_box.rml" );
}
ShellRmlBinding::~ShellRmlBinding() { shutdown(); }

std::string ShellRmlBinding::focusedElementIdForProbe() const
{
	const auto* focused = context_.GetFocusElement();
	return focused ? focused->GetId() : std::string {};
}

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
    dialog_.close(false);
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
	renderedKingdomKeys_.clear(); renderedSavesMarkup_.clear(); renderedSaveKeys_.clear();
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
    if(dialog_.active()) controller_->activate(ShellControl::CancelDestructive);
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
	renderedKingdomKeys_.clear(); renderedSavesMarkup_.clear(); renderedSaveKeys_.clear();
		qInfo() << "Shell route delegated to dedicated HUD binding";
		return true;
	}
	callbacks_.clear();
	settingCallbacks_.clear();
	newGameFieldCallbacks_.clear();
	newGameTabCallbacks_.clear();
	loadRowCallbacks_.clear();
	renderedKingdomKeys_.clear(); renderedSavesMarkup_.clear(); renderedSaveKeys_.clear();
	routeDocument_ = context_.LoadDocument( rml( document->path ) );
	if( !routeDocument_ ) return false;
	localization::applyRmlText( *routeDocument_, textCatalog_ );
	// In the game, dialogs sit over the paused map; on the main menu they sit on the desktop colour (Stage 19).
	routeDocument_->SetClass( "is-in-game", route.starts_with( "game." ) );
	routeDocument_->SetClass( "is-loading", route == "shell.loading" );
	bindCallbacks();
	if( route == "shell.new_game" )
	{
		activeNewGameTab_ = "welcome";
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
	// A key binding (F5 refreshes the save list) reacts to its key only.
	if( event.GetId() == Rml::EventId::Keydown && event.GetParameter<int>( "key_identifier", 0 ) != Rml::Input::KI_F5 ) return;
	event.StopPropagation();
	if( event.GetCurrentElement() && event.GetCurrentElement()->HasAttribute( "disabled" ) ) return;
	// Returning to the main menu gives the focus back to the command that left it (NEW-002).
	if( owner_.routeValue_ == "shell.main_menu" && event.GetCurrentElement() ) owner_.mainMenuReturnFocus_ = event.GetCurrentElement()->GetId();
    if(control_==ShellControl::StartConfiguredGame) {
        for(auto& [id,editor] : owner_.numericEditors_) if(!editor->commit()) {
            auto* range=owner_.routeDocument_->GetElementById(id);
            for(auto* panel=range;panel;panel=panel->GetParentNode())
                if(panel->GetId().starts_with("new-panel-")) {
                    owner_.selectNewGameTab(panel->GetId().substr(10));
                    break;
                }
            if(auto* input=owner_.routeDocument_->GetElementById(id+"-exact"))input->Focus(true);
            return;
        }
    }
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

void ShellRmlBinding::NewGameTabCallback::ProcessEvent(Rml::Event& event)
{
    auto* target = event.GetCurrentElement();
    if(!target || !target->IsVisible(true) || target->HasAttribute("disabled")) return;
    event.StopPropagation();
    if(tab_=="back") owner_.moveWizard(-1);
    else if(tab_=="next") owner_.moveWizard(1);
    else owner_.selectNewGameTab(tab_);
}

void ShellRmlBinding::LoadRowCallback::ProcessEvent( Rml::Event& event )
{
	if( !owner_.controller_ || owner_.updatingDom_ ) return;
	event.StopPropagation();
	if( kind_ == Kind::Kingdom )
	{
		// "Look in:" names a kingdom by its index in the rendered list; the typed key never reaches the DOM.
		auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() );
		const Rml::String value = control ? control->GetValue() : event.GetParameter<Rml::String>( "value", "" );
		std::size_t index = 0;
		if( value.empty() || std::from_chars( value.data(), value.data() + value.size(), index ).ec != std::errc{} || index >= owner_.renderedKingdomKeys_.size() ) return;
		// Choosing the kingdom already shown asks for nothing (RmlUi reports a change for SetValue as well).
		const auto& selected = owner_.controller_->state().loadGame.selectedKingdom;
		if( !selected || selected->relativeKey != owner_.renderedKingdomKeys_[index] ) owner_.controller_->selectKingdom( SaveKingdomId{ owner_.renderedKingdomKeys_[index] } );
		return;
	}
	owner_.controller_->selectSave( SaveSlotId{ id_ } );
	// A double-click opens the save, like Open (PDF p.170).
	if( kind_ == Kind::Open ) (void)owner_.activateElement( "load-selected" );
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
	if( routeDocument_ )
		if( auto* dialog = routeDocument_->GetElementById( "load-dialog" ) )
		{
			auto callback = std::make_unique<Callback>( *this, ShellControl::RefreshLoads );
			dialog->AddEventListener( Rml::EventId::Keydown, callback.get(), false );
			routeListeners_.push_back( { dialog, "keydown", callback.get() } );
			callbacks_.push_back( std::move( callback ) );
		}
	bindNewGameTabCallback( "new-back", "back" );
	bindNewGameTabCallback( "new-next", "next" );
	if( routeDocument_ )
		if( auto* kingdoms = routeDocument_->GetElementById( "load-kingdoms" ) )
		{
			auto callback = std::make_unique<LoadRowCallback>( *this, LoadRowCallback::Kind::Kingdom, std::string{} );
			kingdoms->AddEventListener( Rml::EventId::Change, callback.get(), false );
			routeListeners_.push_back( { kingdoms, "change", callback.get() } );
			loadRowCallbacks_.push_back( std::move( callback ) );
		}
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
        if(element->GetAttribute<Rml::String>("type", "") == "range") {
            const std::string numericId = std::string(id) + "-exact";
            if(routeDocument_->GetElementById(numericId)) {
                auto editor = std::make_unique<NumericEditor>(*routeDocument_, numericId, [this, field=std::string(field)](int value) {
                    controller_->setNewGameField(NewGameFieldId{field}, static_cast<std::int32_t>(value));
                    const auto& fields=controller_->state().newGame.draft.fields;
                    auto found=std::ranges::find_if(fields,[&](const auto& entry){return entry.field.value==field;});
                    return found!=fields.end() && std::get_if<std::int32_t>(&found->value) && std::get<std::int32_t>(found->value)==value;
                });
                editor->sync(element->GetAttribute<int>("value",element->GetAttribute<int>("min",0)),element->GetAttribute<int>("min",0),element->GetAttribute<int>("max",100));
                numericEditors_.emplace_back(id,std::move(editor));
            }
        }
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

// Custom Game wizard pages (PDF p.304-309): Welcome, World, Settlement, Terrain and Life, Completion.
// < Back is unavailable on the first page; Finish takes the place of Next > on the last page.
namespace
{
constexpr std::array wizardPages{ "welcome", "world", "settlement", "terrain", "review" };
}
void ShellRmlBinding::selectNewGameTab( std::string_view tab )
{
	const auto valid = std::ranges::find( wizardPages, tab );
	if( valid == wizardPages.end() || !routeDocument_ ) return;
	activeNewGameTab_ = std::string( tab );
	for( const char* page : wizardPages ) setVisible( ( std::string( "new-panel-" ) + page ).c_str(), activeNewGameTab_ == page );
	const bool first = valid == wizardPages.begin();
	const bool last = valid + 1 == wizardPages.end();
	setEnabled( "new-back", !first );
	setVisible( "new-next", !last );
	setVisible( "new-start", last );
}
void ShellRmlBinding::moveWizard( int step )
{
	const auto current = std::ranges::find( wizardPages, activeNewGameTab_ );
	if( current == wizardPages.end() || !routeDocument_ ) return;
	if( step > 0 )
		// Next accepts the page only when its typed numbers are in range; the invalid box keeps the focus.
		for( auto& [id, editor] : numericEditors_ )
		{
			auto* range = routeDocument_->GetElementById( id );
			bool onPage = false;
			for( auto* e = range; e && !onPage; e = e->GetParentNode() ) onPage = e->GetId() == "new-panel-" + activeNewGameTab_;
			if( onPage && !editor->commit() )
			{
				if( auto* input = routeDocument_->GetElementById( id + "-exact" ) ) input->Focus( true );
				return;
			}
		}
	const auto index = static_cast<int>( current - wizardPages.begin() ) + step;
	if( index < 0 || index >= static_cast<int>( wizardPages.size() ) ) return;
	selectNewGameTab( wizardPages[static_cast<std::size_t>( index )] );
	if( auto* button = routeDocument_->GetElementById( index + 1 == static_cast<int>( wizardPages.size() ) ? "new-start" : "new-next" ) ) button->Focus();
}

void ShellRmlBinding::syncNewGameTabs()
{
	selectNewGameTab( activeNewGameTab_ );
}

bool ShellRmlBinding::activateElement( std::string_view id )
{
	// Probes and scripts still name wizard pages by their former tab ids.
	static constexpr std::array tabs{
		std::pair{ "new-tab-welcome", "welcome" },
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

bool ShellRmlBinding::dispatchElementClickForProbe( std::string_view id, std::string* focusedTarget )
{
	if ( focusedTarget ) focusedTarget->clear();
	for ( auto* document : { routeDocument_, appShell_, dialog_.document() } )
		if ( document )
			if ( auto* element = document->GetElementById( rml( id ) ) )
			{
				element->Focus();
				if ( focusedTarget ) *focusedTarget = focusedElementIdForProbe();
				element->DispatchEvent( "click", Rml::Dictionary {} );
				return true;
			}
	return false;
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
	// The save scan finishes after the menu appears: once Continue is available it becomes the default command,
	// unless the player already moved the focus or is returning to the menu.
	if( routeValue_ == "shell.main_menu" && state.continueAvailable && mainMenuReturnFocus_.empty() && focusedElementIdForProbe() == "shell-load" && routeDocument_ )
		if( auto* element = routeDocument_->GetElementById( "shell-continue" ) ) element->Focus();
	setText( "shell-continue-reason", state.continueAvailable
		? escape( state.continueSaveName + " · Last saved " + state.continueSavedAt )
		: "No compatible save found" );
	setVisible( "shell-action-error", state.actionError.has_value() );
	if( state.actionError ) setText( "shell-action-error-detail", messageText( textCatalog_, state.actionError->message ) );
	setVisible( "load-kingdoms-empty", state.loadGame.kingdomsStatus == RequestStatus::Empty );
	setVisible( "load-saves-empty", state.loadGame.savesStatus == RequestStatus::Empty );
	setText( "load-saves-empty-title", state.loadGame.selectedKingdom ? "This kingdom has no saves." : "Choose a kingdom in Look in." );
	setText( "load-saves-empty-detail", state.loadGame.selectedKingdom ? "Start a new kingdom from the main menu, or press F5 to refresh the list." : "" );
	setVisible( "load-error", state.loadGame.error.has_value() );
	if( state.loadGame.error ) setText( "load-error-detail", messageText( textCatalog_, state.loadGame.error->message ) );
	const auto selectedSave = state.loadGame.selectedSlot
		? std::ranges::find_if( state.loadGame.saves, [&]( const SaveSlotRow& row ) { return row.id == *state.loadGame.selectedSlot; } )
		: state.loadGame.saves.end();
	setEnabled( "load-selected", selectedSave != state.loadGame.saves.end() && selectedSave->compatible && !state.pendingRequest );
	renderLoadGame( state );
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
    for(auto& [id, editor] : numericEditors_)
        if(auto* range=routeDocument_->GetElementById(id)) editor->sync(range->GetAttribute<int>("value",0),range->GetAttribute<int>("min",0),range->GetAttribute<int>("max",100));
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
	setVisible( "loading-progress", !state.lifecycle.error.has_value() );
	if( routeDocument_ ) routeDocument_->SetClass( "is-failed", state.lifecycle.error.has_value() );
	if( routeDocument_ )
		if( auto* symbol = routeDocument_->GetElementById( "loading-symbol" ) ) symbol->SetClass( "is-info", !state.lifecycle.error.has_value() );
	setVisible( "loading-error-actions", state.lifecycle.error.has_value() );
	setEnabled( "loading-retry", state.lifecycle.error && state.lifecycle.error->retryable );
	if(state.lifecycle.error)
	{
		auto text=messageText(textCatalog_,state.lifecycle.error->message);
		if(!text.empty() && text.back()!='.') text+='.';
		setText("loading-error-detail",escape(text));
	}
	if( !state.lifecycle.progressText.empty() ) setText( "loading-progress", escape(state.lifecycle.progressText) );
	else if( state.lifecycle.progress ) setText( "loading-progress", escape(messageText( textCatalog_, *state.lifecycle.progress )) );
    else setText("loading-progress",textCatalog_.format(LocalizationKey{"shell.loading.loading-progress"}));
	updatingDom_ = false;
}

namespace
{
// Windows 98 short date and time, for example "9/25/2026 10:05 AM".
std::string shortDateTime( std::int64_t utcSeconds )
{
	if( utcSeconds <= 0 ) return "Unknown";
	const auto dateTime = QDateTime::fromSecsSinceEpoch( utcSeconds ).toLocalTime();
	return dateTime.toString( QStringLiteral( "M/d/yyyy h:mm AP" ) ).toStdString();
}
}

// Load Game (Open dialog model): "Look in:" lists the kingdoms, the list view their saves. The list is rebuilt only
// when its rows change, never for a selection, so a click never destroys the row it is dispatched to.
void ShellRmlBinding::renderLoadGame( const ShellState& state )
{
	if( !routeDocument_ ) return;
	const auto& load = state.loadGame;
	if( auto* kingdoms = routeDocument_->GetElementById( "load-kingdoms" ) )
	{
		std::vector<std::string> keys;
		std::vector<std::pair<std::string, std::string>> options;
		for( std::size_t index = 0; index < load.kingdoms.size(); ++index )
		{
			keys.push_back( load.kingdoms[index].id.relativeKey );
			options.emplace_back( std::to_string( index ), load.kingdoms[index].displayName );
		}
		if( keys != renderedKingdomKeys_ )
		{
			setSelectOptions( kingdoms, options, true );
			renderedKingdomKeys_ = keys;
		}
		std::string selected;
		for( std::size_t index = 0; index < load.kingdoms.size(); ++index )
			if( load.selectedKingdom && *load.selectedKingdom == load.kingdoms[index].id ) selected = std::to_string( index );
		if( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( kingdoms ); control && control->GetValue() != selected ) control->SetValue( selected );
	}
	if( auto* saves = routeDocument_->GetElementById( "load-saves" ) )
	{
		std::string markup;
		std::vector<std::string> keys;
		for( std::size_t index = 0; index < load.saves.size(); ++index )
		{
			const auto& row = load.saves[index];
			keys.push_back( row.id.relativeKey );
			markup += "<button id=\"save-row-" + std::to_string( index ) + "\" class=\"w98-list-item" + std::string( row.compatible ? "" : " is-incompatible" ) + "\" role=\"option\">"
				+ "<span class=\"w98-cell w98-cell--grow\">" + escape( row.displayName ) + "</span><span class=\"w98-cell l-load-modified\">" + escape( shortDateTime( row.modifiedUtcSeconds ) )
				+ "</span><span class=\"w98-cell l-load-version\">" + escape( row.version.empty() ? std::string( "Unknown" ) : row.version ) + "</span></button>";
		}
		if( markup != renderedSavesMarkup_ || keys != renderedSaveKeys_ )
		{
			detachLoadRowListeners();
			std::erase_if( loadRowCallbacks_, []( const auto& callback ) { return callback->kind() != LoadRowCallback::Kind::Kingdom; } );
			saves->SetInnerRML( rml( markup ) );
			renderedSavesMarkup_ = markup;
			renderedSaveKeys_ = keys;
			for( std::size_t index = 0; index < keys.size(); ++index )
				if( auto* element = saves->GetElementById( rml( "save-row-" + std::to_string( index ) ) ) )
					for( const auto kind : { LoadRowCallback::Kind::Save, LoadRowCallback::Kind::Open } )
					{
						auto callback = std::make_unique<LoadRowCallback>( *this, kind, keys[index] );
						element->AddEventListener( kind == LoadRowCallback::Kind::Open ? Rml::EventId::Dblclick : Rml::EventId::Click, callback.get(), false );
						loadRowListeners_.push_back( { element, kind == LoadRowCallback::Kind::Open ? "dblclick" : "click", callback.get() } );
						loadRowCallbacks_.push_back( std::move( callback ) );
					}
		}
		const SaveSlotRow* chosen = nullptr;
		for( std::size_t index = 0; index < load.saves.size(); ++index )
			if( auto* element = saves->GetElementById( rml( "save-row-" + std::to_string( index ) ) ) )
			{
				const bool selected = load.selectedSlot && *load.selectedSlot == load.saves[index].id;
				if( selected ) chosen = &load.saves[index];
				element->SetClass( "is-selected", selected );
				element->SetAttribute( "aria-selected", selected ? "true" : "false" );
			}
		if( auto* name = rmlui_dynamic_cast<Rml::ElementFormControl*>( routeDocument_->GetElementById( "load-file-name" ) ) )
			name->SetValue( rml( chosen ? chosen->displayName : std::string{} ) );
		setText( "load-status", escape( chosen && !chosen->compatible
			? "This save was made by version " + ( chosen->version.empty() ? std::string( "unknown" ) : chosen->version ) + " and cannot be opened by this version."
			: std::string{} ) );
	}
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
	if( route == "shell.main_menu" && !mainMenuReturnFocus_.empty() && routeDocument_ )
		if( auto* element = routeDocument_->GetElementById( rml( mainMenuReturnFocus_ ) ); element && !element->HasAttribute( "disabled" ) )
		{
			element->Focus();
			return;
		}
	const auto id = route == "shell.main_menu" && controller_ && !controller_->state().continueAvailable
		? std::string_view{ "shell-load" } : ShellRmlAdapter::initialFocusForRoute( route );
	if( !id.empty() ) if( auto* element = routeDocument_->GetElementById( rml( id ) ) ) element->Focus();
}

void ShellRmlBinding::showConfirmation( const Message& title, const Message& detail, FocusToken returnFocus )
{
    if(dialog_.active()) return;
    lastFocusToken_ = returnFocus.value;
    confirmationFocusId_ = focusedElementIdForProbe();
    // A message box names the program in its caption, asks the whole question in its text, and answers with Yes / No
    // (PDF p.182-185).
    dialog_.show("Ingnomia", messageText(textCatalog_,title) + " " + messageText(textCatalog_,detail), "Yes", "No",
        [this]{controller_->activate(ShellControl::ConfirmDestructive);},
        [this]{controller_->activate(ShellControl::CancelDestructive);});
}
void ShellRmlBinding::closeConfirmation() { dialog_.close(); }
void ShellRmlBinding::restoreFocus(FocusToken token)
{
    lastFocusToken_ = token.value;
    if(routeDocument_) if(auto* e=routeDocument_->GetElementById(confirmationFocusId_); e && e->IsVisible(true)) { e->Focus(); return; }
    if(routeDocument_) focusInitial(routeValue_);
}

void ShellRmlBinding::detachRouteListeners()
{
    numericEditors_.clear();
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
