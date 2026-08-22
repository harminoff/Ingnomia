/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../state/UiNavigation.h"
#include "../accessibility/UiAccessibility.h"
#include <functional>
#include <unordered_map>

namespace ingnomia::ui::navigation
{
enum class Workbench : std::uint8_t { Population, Inventory, Military, Diplomacy };
struct WorkbenchPorts
{
	std::function<bool(FocusToken)> openPopulation, openInventory, openMilitary, openDiplomacy;
	std::function<void()> closePopulationInventory, closeMilitaryDiplomacy;
	std::function<void()> closePopulation, closeInventory, closeMilitary, closeDiplomacy;
	std::function<void(FocusToken)> restoreFocus;
};
class WorkbenchCoordinator final
{
public:
	explicit WorkbenchCoordinator( WorkbenchPorts, bool developmentBuild = false );
	[[nodiscard]] bool enterGame();
	void leaveGame();
	[[nodiscard]] bool open( Workbench, FocusToken );
	[[nodiscard]] bool close( const RouteId&, FocusToken );
	[[nodiscard]] bool closeActive();
	[[nodiscard]] bool handleEscape( bool composing, bool modal, bool modalDismissible, bool overlay, bool activeTool );
	[[nodiscard]] const NavigationState& state() const noexcept { return router_.state(); }
private:
	[[nodiscard]] static RouteId route( Workbench );
	[[nodiscard]] bool invokeOpen( Workbench, FocusToken );
	WorkbenchPorts ports_; UiRouter router_; std::optional<FocusToken> returnFocus_;
	std::unordered_map<std::string, FocusToken> returnFocusByRoute_;
	bool switching_{};
};
}
