/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "Management6BController.h"
#include "Management6BText.h"
#include <RmlUi/Core/EventListener.h>
#include <functional>
#include <memory>
#include <vector>
namespace Rml { class Context; class Element; class ElementDocument; class Event; }
namespace ingnomia::ui::management6b
{
class Management6BRmlBinding final : public ViewPort
{
public:
	using RouteCloseHandler=std::function<void(RouteId,FocusToken)>;
	explicit Management6BRmlBinding( Rml::Context& );
	~Management6BRmlBinding() override;
	bool initialize( Management6BController& );
	void shutdown();
	void stateChanged( const Management6BState& ) override;
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] bool activateFirstDataElement( std::string_view kind );
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	[[nodiscard]] Rml::ElementDocument* populationDocument() const noexcept { return population_; }
	[[nodiscard]] Rml::ElementDocument* inventoryDocument() const noexcept { return inventory_; }
	bool openPopulation(FocusToken);bool openInventory(FocusToken);void closePopulation();void closeInventory();void closeRoute();void setRouteCloseHandler(RouteCloseHandler handler){routeCloseHandler_=std::move(handler);}
private:
	class Callback final:public Rml::EventListener{public:explicit Callback(std::function<void(Rml::Event&)>f):fn_(std::move(f)){}void ProcessEvent(Rml::Event&e)override{fn_(e);}private:std::function<void(Rml::Event&)>fn_;};
	void bind(Rml::ElementDocument*,const char*,std::function<void()>);void bindEvent(Rml::ElementDocument*,const char*,const char*,std::function<void(Rml::Event&)>);void text(Rml::ElementDocument*,const char*,const std::string&);void rml(Rml::ElementDocument*,const char*,const std::string&);void visible(Rml::ElementDocument*,const char*,bool);void focusPopulationRow();void focusInventoryRow();void focusScheduleCell();
	Rml::Context& context_;Management6BController* controller_{};Rml::ElementDocument* population_{},*inventory_{};
	localization::UiText textCatalog_{management6BText()};
	struct Listener{Rml::Element*target{};std::string event;std::unique_ptr<Callback>callback;};std::vector<Listener>listeners_;
	std::optional<RouteId> activeRoute_;FocusToken returnFocus_{};FocusToken populationFocus_{}, inventoryFocus_{};RouteCloseHandler routeCloseHandler_;
};
} // namespace ingnomia::ui::management6b
