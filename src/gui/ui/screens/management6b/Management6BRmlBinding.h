/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "Management6BController.h"
#include "Management6BText.h"
#include "../../runtime/ModalDialog.h"
#include <RmlUi/Core/EventListener.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
namespace Rml { class Context; class Element; class ElementDocument; class Event; }
namespace ingnomia::ui::management6b
{
class Management6BRmlBinding final : public ViewPort
{
public:
	using DocumentLoader=std::function<Rml::ElementDocument*( const char* )>;
	using RouteCloseHandler=std::function<void(RouteId,FocusToken)>;
	explicit Management6BRmlBinding( Rml::Context& );
	~Management6BRmlBinding() override;
	bool initialize( Management6BController& );
	bool reloadDocuments();
	void shutdown();
	void setDocumentLoader( DocumentLoader loader ){ documentLoader_=std::move(loader); }
	void setPresentationEnabled( bool enabled ) { presentationEnabled_ = enabled; }
	void setWindowSurface(bool secondary) { secondarySurface_ = secondary; }
	void setStockpileOpenHandler(std::function<void(unsigned int)> handler) { stockpileOpenHandler_ = std::move(handler); }
	void setCitizenSelectedHandler(std::function<void()> handler) { citizenSelectedHandler_ = std::move(handler); }
	void stateChanged( const Management6BState& ) override;
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] bool activateFirstDataElement( std::string_view kind );
	[[nodiscard]] bool focusInventoryRowsForProbe();
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	[[nodiscard]] Rml::ElementDocument* populationDocument() const noexcept { return population_; }
	[[nodiscard]] Rml::ElementDocument* inventoryDocument() const noexcept { return inventory_; }
	bool openPopulation(FocusToken);bool openInventory(FocusToken);void closePopulation();void closeInventory();void closeRoute();void setRouteCloseHandler(RouteCloseHandler handler){routeCloseHandler_=std::move(handler);}
private:
	class Callback final:public Rml::EventListener{public:explicit Callback(std::function<void(Rml::Event&)>f):fn_(std::move(f)){}void ProcessEvent(Rml::Event&e)override{fn_(e);}private:std::function<void(Rml::Event&)>fn_;};
	void bind(Rml::ElementDocument*,const char*,std::function<void()>);void bindEvent(Rml::ElementDocument*,const char*,const char*,std::function<void(Rml::Event&)>);void text(Rml::ElementDocument*,const char*,const std::string&);void rml(Rml::ElementDocument*,const char*,const std::string&);void visible(Rml::ElementDocument*,const char*,bool);void focusPopulationRow();void focusInventoryRow();void focusScheduleCell();void renderInventoryViewport();
	void reviewDraft(std::function<void()> next);
    void reviewProfessionDelete();
    void reviewSkillScope(bool active);
    void reviewScheduleScope( ScheduleActivity );
    std::string scheduleScopeText( const Management6BState&, const ScheduleScope& ) const;
    ModalDialog dialog_;
    Rml::Context& context_;Management6BController* controller_{};Rml::ElementDocument* population_{},*inventory_{};
	localization::UiText textCatalog_{management6BText()};
	struct Listener{Rml::Element*target{};std::string event;std::unique_ptr<Callback>callback;};std::vector<Listener>listeners_;
	std::optional<RouteId> activeRoute_;FocusToken returnFocus_{};FocusToken populationFocus_{}, inventoryFocus_{};RouteCloseHandler routeCloseHandler_;DocumentLoader documentLoader_;std::optional<bool> secondarySurface_;bool presentationEnabled_{true};
	std::function<void(unsigned int)> stockpileOpenHandler_;
	std::function<void()> citizenSelectedHandler_;
	// Inventory rows are virtualized; their markup carries no watch or selection state, which is set in place,
	// so a click never rebuilds the row it lands on.
	void syncInventoryRows();
	std::unordered_map<std::string, std::string> renderedInventoryDetailMarkup_;
	std::vector<std::string> inventoryRowMarkup_;
	std::vector<InventoryRow> inventoryRows_;
	std::vector<InventoryRowId> inventoryRowIds_;
	std::array<std::string,6> reportOptions_;
	std::size_t inventoryViewportFirst_ { static_cast<std::size_t>( -1 ) };
	float inventoryRowHeight_ {};
	bool renderingInventoryViewport_ {}, renderingInventory_ {};
	std::string inventoryCategoryOptions_, inventoryFilterKey_;
	int inventoryDetailPane_ {};
	std::optional<unsigned int> inventoryDetailStockpile_;
	std::string inventoryDetailProduct_;
	std::optional<InventoryRowId> renderedInventoryDetail_;
	std::optional<View> renderedPopulationView_;
	std::string renderedProfessionOptions_;
	bool renderingPopulation_ {};
};
} // namespace ingnomia::ui::management6b
