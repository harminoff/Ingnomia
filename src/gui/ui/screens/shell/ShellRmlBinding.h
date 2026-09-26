/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "ShellController.h"
#include "../../runtime/ModalDialog.h"
#include "../../runtime/NumericEditor.h"
#include "../../localization/UiText.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Types.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Rml { class Context; class Element; class ElementDocument; class Event; }

namespace ingnomia::ui::shell
{

class ShellRmlBinding final : public ShellViewPort
{
public:
	explicit ShellRmlBinding( Rml::Context& context );
	~ShellRmlBinding() override;

	ShellRmlBinding( const ShellRmlBinding& ) = delete;
	ShellRmlBinding& operator=( const ShellRmlBinding& ) = delete;

	bool initialize( ShellController& controller );
	[[nodiscard]] std::string focusedElementIdForProbe() const;
	bool reloadDocuments();
	void shutdown();
	void setInMenu( bool inMenu );
	[[nodiscard]] bool initialized() const noexcept;
	[[nodiscard]] Rml::ElementDocument* routeDocument() const noexcept { return routeDocument_; }
	[[nodiscard]] bool activateElement( std::string_view id );
	[[nodiscard]] bool dispatchElementClickForProbe( std::string_view id, std::string* focusedTarget = nullptr );
	bool dispatchSettingChangeForProbe( std::string_view id, float value, bool checked );

	void stateChanged( const ShellState& state ) override;
	void showConfirmation( const Message& title, const Message& detail, FocusToken returnFocus ) override;
	void closeConfirmation() override;
	void restoreFocus( FocusToken token ) override;

private:
	class Callback final : public Rml::EventListener
	{
	public:
		Callback( ShellRmlBinding& owner, ShellControl control ) : owner_( owner ), control_( control ) {}
		void ProcessEvent( Rml::Event& event ) override;
	private:
		ShellRmlBinding& owner_;
		ShellControl control_;
	};
	class SettingCallback final : public Rml::EventListener
	{
	public:
		SettingCallback( ShellRmlBinding& owner, SettingId setting ) : owner_( owner ), setting_( std::move( setting ) ) {}
		void ProcessEvent( Rml::Event& event ) override;
	private:
		ShellRmlBinding& owner_;
		SettingId setting_;
	};
	class NewGameFieldCallback final : public Rml::EventListener
	{
	public:
		NewGameFieldCallback( ShellRmlBinding& owner, NewGameFieldId field ) : owner_( owner ), field_( std::move( field ) ) {}
		void ProcessEvent( Rml::Event& event ) override;
	private:
		ShellRmlBinding& owner_;
		NewGameFieldId field_;
	};
	class NewGameTabCallback final : public Rml::EventListener
	{
	public:
		NewGameTabCallback( ShellRmlBinding& owner, std::string tab ) : owner_( owner ), tab_( std::move( tab ) ) {}
		void ProcessEvent( Rml::Event& event ) override;
	private:
		ShellRmlBinding& owner_;
		std::string tab_;
	};
	class LoadRowCallback final : public Rml::EventListener
	{
	public:
		// Kingdom: the "Look in:" drop-down changed; Save: a save row was clicked; Open: a save row was double-clicked.
		enum class Kind : std::uint8_t { Kingdom, Save, Open };
		LoadRowCallback( ShellRmlBinding& owner, Kind kind, std::string id ) : owner_( owner ), kind_( kind ), id_( std::move( id ) ) {}
		void ProcessEvent( Rml::Event& event ) override;
		[[nodiscard]] Kind kind() const noexcept { return kind_; }
	private:
		ShellRmlBinding& owner_;
		Kind kind_;
		std::string id_;
	};

	bool createModels();
	bool loadRoute( std::string_view route );
	void bindCallbacks();
	void bindCallback( const char* id, ShellControl control );
	void bindSettingCallback( const char* id, const char* setting );
	void bindNewGameFieldCallback( const char* id, const char* field );
	void bindNewGameTabCallback( const char* id, const char* tab );
	void bindLoadRows( const ShellState& state );
	void syncDom( const ShellState& state );
	void syncModel( const ShellState& state );
	void selectNewGameTab( std::string_view tab );
	void moveWizard( int step );
	void renderLoadGame( const ShellState& state );
	void syncNewGameTabs();
	void setText( const char* id, std::string_view text );
	void setVisible( const char* id, bool visible );
	void setEnabled( const char* id, bool enabled );
	void focusInitial( std::string_view route );
	void detachRouteListeners();
	void detachModalListeners();
	void detachLoadRowListeners();
	struct ListenerBinding { Rml::Element* element{}; const char* event{}; Rml::EventListener* listener{}; };

	Rml::Context& context_;
        ModalDialog dialog_;
        std::string confirmationFocusId_;
	ShellController* controller_{};
	Rml::ElementDocument* appShell_{};
	Rml::ElementDocument* routeDocument_{};
	Rml::ElementDocument* confirmation_{};
	std::vector<std::unique_ptr<Callback>> callbacks_;
	std::vector<std::unique_ptr<Callback>> modalCallbacks_;
	std::vector<std::unique_ptr<SettingCallback>> settingCallbacks_;
	std::vector<std::unique_ptr<NewGameFieldCallback>> newGameFieldCallbacks_;
	std::vector<std::unique_ptr<NewGameTabCallback>> newGameTabCallbacks_;
	std::vector<std::unique_ptr<LoadRowCallback>> loadRowCallbacks_;
	std::vector<ListenerBinding> routeListeners_;
	std::vector<ListenerBinding> modalListeners_;
	std::vector<ListenerBinding> loadRowListeners_;
	Rml::DataModelHandle shellModel_;
	Rml::DataModelHandle settingsModel_;
	Rml::DataModelHandle newGameModel_;
	Rml::DataModelHandle loadGameModel_;
	Rml::String routeValue_;
	Rml::String versionValue_;
	Rml::String statusValue_;
	bool pendingValue_{};
	bool continueValue_{};
	bool pausedValue_{};
	int supportedSettingsCount_{};
	int newGameErrorCount_{};
	int kingdomCount_{};
	int saveCount_{};
	std::uint64_t lastFocusToken_{};
	std::vector<std::pair<std::string, std::unique_ptr<NumericEditor>>> numericEditors_;
	bool updatingDom_{};
	std::string activeNewGameTab_{ "welcome" };
	std::string mainMenuReturnFocus_;
	std::vector<std::string> renderedKingdomKeys_;
	std::string renderedSavesMarkup_;
	std::vector<std::string> renderedSaveKeys_;
	localization::UiText textCatalog_;
};

} // namespace ingnomia::ui::shell
