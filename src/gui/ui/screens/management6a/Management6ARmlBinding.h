/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "Management6AController.h"
#include "../../localization/UiText.h"

#include <RmlUi/Core/EventListener.h>
#include <functional>
#include <memory>
#include <map>
#include <vector>

namespace Rml { class Context; class Element; class ElementDocument; class Event; }

namespace ingnomia::ui::management6a
{
class Management6ARmlBinding final : public ViewPort
{
public:
	using DocumentLoader = std::function<Rml::ElementDocument*( const char* )>;
	explicit Management6ARmlBinding( Rml::Context& );
	~Management6ARmlBinding() override;
	bool initialize( Management6AController& );
	bool reloadDocuments();
	void shutdown();
	void setDocumentLoader( DocumentLoader loader ) { documentLoader_ = std::move( loader ); }
	void setPresentationEnabled( bool enabled ) { presentationEnabled_ = enabled; }
	void setCloseHandler( std::function<void()> handler ) { closeHandler_ = std::move( handler ); }
	void stateChanged( const Management6AState& ) override;
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] bool setFormValueForProbe( std::string_view id, std::string_view value );
	/// @brief Sets the live Stockpile search for an opt-in production probe.
	[[nodiscard]] bool setStockpileSearchForProbe( std::string_view value );
	/// @brief Activates the first visible live filter row matching the requested state/depth.
	[[nodiscard]] bool activateFirstStockpileFilterForProbe( TriState, FilterDepth );
	/// @brief Activates a specific visible live item/material filter row for a copied-save probe.
	[[nodiscard]] bool activateStockpileFilterForProbe( std::string_view item, std::string_view material );
	/// @brief Sends an opt-in key event through the live visible Stockpile filter row.
	[[nodiscard]] bool dispatchStockpileFilterKeyForProbe( int keyIdentifier );
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	[[nodiscard]] Rml::ElementDocument* workshopDocument() const noexcept { return workshop_; }
	[[nodiscard]] Rml::ElementDocument* stockpileDocument() const noexcept { return stockpile_; }
	[[nodiscard]] Rml::ElementDocument* agricultureDocument() const noexcept { return agriculture_; }

private:
	class Callback final : public Rml::EventListener { public: explicit Callback(std::function<void(Rml::Event&)> fn):fn_(std::move(fn)){}void ProcessEvent(Rml::Event& event)override;private:std::function<void(Rml::Event&)>fn_;};
	Rml::Element* element( const char* ) const;
	void bind( const char*, const char*, std::function<void(Rml::Event&)>, bool capture = false );
	void bindStockpileTooltip( const char*, std::function<std::string()> );
	void showStockpileTooltip( const std::string&, Rml::Element* );
	void hideStockpileTooltip();
	void bindClick( const char* id, std::function<void()> fn ) { bind( id, "click", [fn=std::move(fn)](Rml::Event&){fn();} ); }
	void text( const char*, const std::string& );
	void visible( const char*, bool );
	void checked( const char*, bool );
	void enabled( const char*, bool );
	std::string formValue( const char* ) const;
	void formValue( const char*, const std::string& );
	std::int32_t priority( const char*, std::int32_t fallback, std::int32_t maximum ) const;
	std::int32_t normalizePriority( const char*, std::int32_t fallback, std::int32_t maximum );
	void renderWorkshop( const WorkshopState& );
	void workshopMarkup( const char*, const std::string& );
	std::map<std::string, std::string> workshopMarkup_;
	WorkshopId workshopSettingsId_;
	std::string workshopSettingsName_;
	std::int32_t workshopSettingsPriority_{-1};
	std::optional<CraftJobId> workshopEditingJob_;
	std::uint32_t workshopEditingCount_{};
	bool renderingWorkshop_{};
	void renderStockpile( const StockpileState& );
	void renderStockpileFilterViewport();
	void renderAgriculture( const AgricultureState& );
	void renderFarmPlanner( const AgricultureState& );
	Rml::Context& context_;
	Management6AController* controller_{};
	localization::UiText textCatalog_;
	Rml::ElementDocument* workshop_{};
	Rml::ElementDocument* stockpile_{};
	Rml::ElementDocument* agriculture_{};
	struct Listener { Rml::Element* target{}; std::string event; std::unique_ptr<Callback> callback; bool capture{}; };
	std::vector<Listener> listeners_;
	DocumentLoader documentLoader_;
	std::function<void()> closeHandler_;
	bool presentationEnabled_{ true };
	bool tradeConfirmationVisible_{};
	bool stockpileTemplateConfirmationVisible_{};
	ManagementView activeView_{ ManagementView::None };
	static constexpr std::size_t pageSize_ = 48;
	std::size_t workshopPage_{};
	std::size_t agriculturePage_{};
	std::string farmGridMarkup_, farmCatalogMarkup_, farmOrdersMarkup_;
	std::vector<std::string> stockpileFilterMarkup_;
	std::size_t stockpileFilterFirst_{ static_cast<std::size_t>( -1 ) };
	bool renderingStockpileFilters_{};
	bool normalizingPriority_{};
	bool syncingCheckbox_{};
	std::string stockpileCategoryMarkup_;
	std::optional<std::size_t> stockpileContentFilterMenuColumn_;
	std::optional<std::size_t> stockpileAllowFilterMenuColumn_;
};
} // namespace ingnomia::ui::management6a
