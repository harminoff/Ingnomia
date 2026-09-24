/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "HudController.h"
#include "../../localization/UiText.h"
#include <RmlUi/Core/EventListener.h>
#include <memory>
#include <functional>
#include <vector>
namespace Rml { class Context; class Element; class ElementDocument; class Event; }
namespace ingnomia::ui::hud
{
class HudRmlBinding final : public HudViewPort
{
public:
	enum class Presentation { Full, OrdersTools };
	enum class ToolPanel { Mine, Build, Agriculture, Designations, Jobs };
	using WorkbenchHandler=std::function<void(FocusToken)>;
	using OrdersToolsHandler=std::function<void(std::string_view)>;
	using CloseHandler=std::function<void()>;
	using DocumentLoader=std::function<Rml::ElementDocument*(const char*)>;
	using PauseHandler=std::function<void()>;
	explicit HudRmlBinding( Rml::Context& context, Presentation presentation = Presentation::Full,
		ToolPanel toolPanel = ToolPanel::Mine );
	~HudRmlBinding() override;
	bool initialize( HudController& controller );
	bool reloadDocument();
	[[nodiscard]] bool activateElement( std::string_view id );
	[[nodiscard]] Rml::ElementDocument* document() const noexcept { return document_; }
	void shutdown();
	void stateChanged( const HudState& state ) override;
	void setWorkbenchHandlers(WorkbenchHandler population,WorkbenchHandler inventory,WorkbenchHandler military,WorkbenchHandler diplomacy);
	void setOrdersToolsHandler( OrdersToolsHandler handler );
	void setToolPanel( ToolPanel panel );
	void setCloseHandler( CloseHandler handler );
	void setDocumentLoader( DocumentLoader loader );
	void setPauseHandler(PauseHandler pause);
	void setInspectionHandler( CloseHandler handler ) { inspect_ = std::move(handler); }
	void setInspectionActive( bool active );
	void restoreWorkbenchFocus(FocusToken);
private:
	class Callback final : public Rml::EventListener { public: Callback( std::function<void()> fn ):fn_(std::move(fn)){} Callback( std::function<void(Rml::Event&)> fn ):eventFn_(std::move(fn)){} void ProcessEvent(Rml::Event&) override; private: std::function<void()> fn_; std::function<void(Rml::Event&)> eventFn_; };
	void bind( const char* id, std::function<void()> callback );
	void bindEvent( const char* id, const char* event, std::function<void( Rml::Event& )> callback );
	void text( const char* id, const std::string& value );
	void visible( const char* id, bool value );
	void toggleActionMenu( std::string_view menu );
	void backToSidebar();
	void bindSidebarTooltip( const char* id, const char* textKey );
	void showSidebarTooltip( const char* textKey, Rml::Element* source );
	void hideSidebarTooltip();
	void selectMineMode( std::string_view tool );
	void selectBuildCategory( const char* elementId, BuildSelection selection, std::string_view category );
	void backToBuildCategories();
	Rml::Context& context_;
	Presentation presentation_{ Presentation::Full };
	ToolPanel toolPanel_{ ToolPanel::Mine };
	HudController* controller_{};
	Rml::ElementDocument* document_{};
	std::vector<std::unique_ptr<Callback>> callbacks_;
	std::vector<std::pair<Rml::Element*, std::string>> listenerTargets_;
	localization::UiText textCatalog_;
	WorkbenchHandler openPopulation_,openInventory_,openMilitary_,openDiplomacy_;
	OrdersToolsHandler openOrdersTools_;
	CloseHandler close_, inspect_;
	bool inspectionActive_{};
	DocumentLoader documentLoader_;
	PauseHandler openPause_;
	bool mineMenuOpen_{};
	bool agricultureMenuOpen_{};
	bool designationMenuOpen_{};
	bool jobsMenuOpen_{};
	bool buildMenuOpen_{};
	bool kingdomPanelOpen_{};
	std::string selectedBuild_;
	std::string selectedBuildCategory_;
	std::string selectedBuildType_;
	bool buildCategoryPage_{ true };
	std::vector<std::string> renderedBuildTypes_;
	std::vector<BuildCatalogRow> renderedBuildCatalog_;
	std::string renderedSelectedBuild_;
	std::string renderedSelectedBuildType_;
	bool updatingBuildCatalog_{};
	std::optional<PromptInstanceId> focusedPrompt_;
};
}
