/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "WorkbenchCoordinator.h"
namespace ingnomia::ui::navigation
{
WorkbenchCoordinator::WorkbenchCoordinator(WorkbenchPorts ports,bool developmentBuild):ports_(std::move(ports)),router_(developmentBuild){}
bool WorkbenchCoordinator::enterGame(){return router_.open(RouteId{"game.hud"}).accepted();}
void WorkbenchCoordinator::leaveGame(){while(router_.state().workbench)(void)closeActive();(void)router_.open(RouteId{"shell.main_menu"});returnFocus_.reset();returnFocusByRoute_.clear();}
RouteId WorkbenchCoordinator::route(Workbench value){switch(value){case Workbench::Population:return RouteId{"workbench.population"};case Workbench::Inventory:return RouteId{"workbench.inventory"};case Workbench::Military:return RouteId{"workbench.military"};case Workbench::Diplomacy:return RouteId{"workbench.diplomacy"};}return{};}
bool WorkbenchCoordinator::invokeOpen(Workbench value,FocusToken focus){const auto&fn=value==Workbench::Population?ports_.openPopulation:value==Workbench::Inventory?ports_.openInventory:value==Workbench::Military?ports_.openMilitary:ports_.openDiplomacy;return fn&&fn(focus);}
bool WorkbenchCoordinator::open(Workbench value,FocusToken focus){
	if(!focus)return false;const auto target=route(value);if(router_.state().workbench==target)return closeActive();
	if(!router_.open(target).accepted()||!invokeOpen(value,focus)){ (void)router_.close(target); return false; }
	returnFocus_=focus;returnFocusByRoute_[target.value]=focus;return true;
}
bool WorkbenchCoordinator::close(const RouteId&routeValue,FocusToken focus){if(std::ranges::find(router_.state().workbenches,routeValue)==router_.state().workbenches.end())return false;if(!router_.close(routeValue).accepted())return false;returnFocusByRoute_.erase(routeValue.value);returnFocus_=router_.state().workbench&&returnFocusByRoute_.contains(router_.state().workbench->value)?std::optional<FocusToken>{returnFocusByRoute_.at(router_.state().workbench->value)}:std::nullopt;if(!switching_&&focus&&ports_.restoreFocus)ports_.restoreFocus(focus);return true;}
bool WorkbenchCoordinator::closeActive(){const auto active=router_.state().workbench;if(!active)return false;const auto focus=returnFocusByRoute_.contains(active->value)?returnFocusByRoute_.at(active->value):FocusToken{};if(active->value=="workbench.population"){if(ports_.closePopulation)ports_.closePopulation();else if(ports_.closePopulationInventory)ports_.closePopulationInventory();}else if(active->value=="workbench.inventory"){if(ports_.closeInventory)ports_.closeInventory();else if(ports_.closePopulationInventory)ports_.closePopulationInventory();}else if(active->value=="workbench.military"){if(ports_.closeMilitary)ports_.closeMilitary();else if(ports_.closeMilitaryDiplomacy)ports_.closeMilitaryDiplomacy();}else{if(ports_.closeDiplomacy)ports_.closeDiplomacy();else if(ports_.closeMilitaryDiplomacy)ports_.closeMilitaryDiplomacy();}if(router_.state().workbench==active)return close(*active,focus);return true;}
bool WorkbenchCoordinator::handleEscape(bool composing,bool modal,bool modalDismissible,bool overlay,bool activeTool){const auto layer=accessibility::escapeTarget({composing,modal,modalDismissible,overlay,router_.state().workbench.has_value(),activeTool});return layer==accessibility::EscapeLayer::Workbench&&closeActive();}
}
