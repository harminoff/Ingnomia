/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiRegistries.h"

#include <array>
#include <unordered_set>

namespace ingnomia::ui
{
namespace
{

constexpr std::array ROUTES{
	RouteDefinition{ "shell.main_menu", RouteKind::Primary, "doc.main_menu" },
	RouteDefinition{ "shell.new_game", RouteKind::Primary, "doc.new_game" },
	RouteDefinition{ "shell.load_game", RouteKind::Primary, "doc.load_game" },
	RouteDefinition{ "shell.settings", RouteKind::Primary, "doc.settings" },
	RouteDefinition{ "shell.loading", RouteKind::Primary, "doc.loading" },
	RouteDefinition{ "game.hud", RouteKind::Primary, "doc.game_hud" },
	RouteDefinition{ "game.pause", RouteKind::Overlay, "doc.pause_menu" },
	RouteDefinition{ "game.settings", RouteKind::Overlay, "doc.settings" },
	RouteDefinition{ "workbench.population", RouteKind::Workbench, "doc.population_manager" },
	RouteDefinition{ "workbench.inventory", RouteKind::Workbench, "doc.inventory_browser" },
	RouteDefinition{ "workbench.military", RouteKind::Workbench, "doc.military_manager" },
	RouteDefinition{ "workbench.diplomacy", RouteKind::Workbench, "doc.diplomacy_missions" },
	RouteDefinition{ "panel.tile", RouteKind::Dock, "doc.tile_inspector" },
	RouteDefinition{ "panel.creature", RouteKind::Dock, "doc.creature_inspector" },
	RouteDefinition{ "panel.stockpile", RouteKind::Dock, "doc.stockpile_manager" },
	RouteDefinition{ "panel.workshop", RouteKind::Dock, "doc.workshop_manager" },
	RouteDefinition{ "panel.agriculture", RouteKind::Dock, "doc.agriculture_manager" },
#if defined(INGNOMIA_DEVELOPER_UI)
	RouteDefinition{ "debug.panel", RouteKind::Dock, "doc.debug_panel", true },
#endif
};

constexpr std::array DOCUMENTS{
	DocumentDefinition{ "doc.app_shell", "documents/app_shell.rml" },
	DocumentDefinition{ "doc.main_menu", "screens/main_menu.rml" },
	DocumentDefinition{ "doc.new_game", "screens/new_game.rml" },
	DocumentDefinition{ "doc.load_game", "screens/load_game.rml" },
	DocumentDefinition{ "doc.loading", "screens/loading.rml" },
	DocumentDefinition{ "doc.settings", "screens/settings.rml" },
	DocumentDefinition{ "doc.pause_menu", "screens/pause_menu.rml" },
	DocumentDefinition{ "doc.game_hud", "screens/game_hud.rml" },
	DocumentDefinition{ "doc.action_bar", "screens/game_hud.rml" },
	DocumentDefinition{ "doc.selection_status", "screens/game_hud.rml" },
	DocumentDefinition{ "doc.tile_inspector", "screens/inspector.rml" },
	DocumentDefinition{ "doc.creature_inspector", "screens/inspector.rml" },
	DocumentDefinition{ "doc.agriculture_manager", "panels/agriculture_manager.rml" },
	DocumentDefinition{ "doc.stockpile_manager", "windows/stockpile_manager.rml" },
	DocumentDefinition{ "doc.workshop_manager", "windows/workshop_manager.rml" },
	DocumentDefinition{ "doc.population_manager", "windows/population_manager.rml" },
	DocumentDefinition{ "doc.inventory_browser", "windows/inventory_browser.rml" },
	DocumentDefinition{ "doc.military_manager", "windows/military_manager.rml" },
	DocumentDefinition{ "doc.diplomacy_missions", "windows/diplomacy_missions.rml" },
	DocumentDefinition{ "doc.event_prompt", "screens/game_hud.rml" },
	DocumentDefinition{ "doc.confirm_destructive", "modals/confirm_destructive.rml" },
	DocumentDefinition{ "doc.error_fallback", {}, true },
#if defined(INGNOMIA_DEVELOPER_UI)
	DocumentDefinition{ "doc.debug_panel", "developer_ui/debug_panel.rml", false, true },
#endif
};

constexpr std::array MODELS{
	NamedDefinition{ "ui_shell" },
	NamedDefinition{ "ui_modal" },
	NamedDefinition{ "ui_hud" },
	NamedDefinition{ "ui_tools" },
	NamedDefinition{ "ui_inspector" },
	NamedDefinition{ "ui_stockpile" },
	NamedDefinition{ "ui_workshop" },
	NamedDefinition{ "ui_agriculture" },
	NamedDefinition{ "ui_population" },
	NamedDefinition{ "ui_inventory" },
	NamedDefinition{ "ui_military" },
	NamedDefinition{ "ui_diplomacy" },
	NamedDefinition{ "ui_settings" },
	NamedDefinition{ "ui_new_game" },
	NamedDefinition{ "ui_load_game" },
#if defined(INGNOMIA_DEVELOPER_UI)
	NamedDefinition{ "ui_debug", true },
#endif
};

constexpr std::array TOOLS{
	NamedDefinition{ "inspect" },
	NamedDefinition{ "mine" },
	NamedDefinition{ "explorative_mine" },
	NamedDefinition{ "remove_floor" },
	NamedDefinition{ "dig_stairs_down" },
	NamedDefinition{ "mine_stairs_up" },
	NamedDefinition{ "dig_ramp_down" },
	NamedDefinition{ "dig_hole" },
	NamedDefinition{ "fell_tree" },
	NamedDefinition{ "plant_tree" },
	NamedDefinition{ "harvest_tree" },
	NamedDefinition{ "forage" },
	NamedDefinition{ "remove_plant" },
	NamedDefinition{ "create_stockpile" },
	NamedDefinition{ "create_farm" },
	NamedDefinition{ "create_grove" },
	NamedDefinition{ "create_pasture" },
	NamedDefinition{ "create_personal_room" },
	NamedDefinition{ "create_dormitory" },
	NamedDefinition{ "create_dining_hall" },
	NamedDefinition{ "create_hospital" },
	NamedDefinition{ "create_forbidden_area" },
	NamedDefinition{ "create_guard_area" },
	NamedDefinition{ "remove_designation" },
	NamedDefinition{ "suspend_job" },
	NamedDefinition{ "resume_job" },
	NamedDefinition{ "cancel_job" },
	NamedDefinition{ "raise_job_priority" },
	NamedDefinition{ "lower_job_priority" },
	NamedDefinition{ "deconstruct" },
	NamedDefinition{ "build" },
};

template<class Range>
bool uniqueIds( const Range& range )
{
	std::unordered_set<std::string_view> ids;
	for( const auto& entry : range )
	{
		if( entry.id.empty() || !ids.emplace( entry.id ).second ) return false;
	}
	return true;
}

} // namespace

std::span<const RouteDefinition> UiRegistries::routes() noexcept
{
	return ROUTES;
}

std::span<const DocumentDefinition> UiRegistries::documents() noexcept
{
	return DOCUMENTS;
}

std::span<const NamedDefinition> UiRegistries::modelNames() noexcept
{
	return MODELS;
}

std::span<const NamedDefinition> UiRegistries::tools() noexcept
{
	return TOOLS;
}

const RouteDefinition* UiRegistries::findRoute( std::string_view id, bool developmentBuild ) noexcept
{
	for( const auto& route : ROUTES )
	{
		if( route.id == id && ( developmentBuild || !route.developmentOnly ) ) return &route;
	}
	return nullptr;
}

const DocumentDefinition* UiRegistries::findDocument( std::string_view id, bool developmentBuild ) noexcept
{
	for( const auto& document : DOCUMENTS )
	{
		if( document.id == id && ( developmentBuild || !document.developmentOnly ) ) return &document;
	}
	return nullptr;
}

bool UiRegistries::hasModel( std::string_view id, bool developmentBuild ) noexcept
{
	for( const auto& model : MODELS )
	{
		if( model.id == id && ( developmentBuild || !model.developmentOnly ) ) return true;
	}
	return false;
}

bool UiRegistries::hasTool( std::string_view id ) noexcept
{
	for( const auto& tool : TOOLS )
	{
		if( tool.id == id ) return true;
	}
	return false;
}

RegistryAudit UiRegistries::audit() noexcept
{
	if( !uniqueIds( ROUTES ) ) return { false, "route identifiers must be non-empty and unique" };
	if( !uniqueIds( DOCUMENTS ) ) return { false, "document identifiers must be non-empty and unique" };
	if( !uniqueIds( MODELS ) ) return { false, "model identifiers must be non-empty and unique" };
	if( !uniqueIds( TOOLS ) ) return { false, "tool identifiers must be non-empty and unique" };

	for( const auto& route : ROUTES )
	{
		bool found = false;
		for( const auto& document : DOCUMENTS )
		{
			if( document.id == route.documentId )
			{
				found = true;
				if( route.developmentOnly != document.developmentOnly )
					return { false, "route and document development visibility must agree" };
				break;
			}
		}
		if( !found ) return { false, "every route must reference a registered document" };
	}

	return { true, {} };
}

} // namespace ingnomia::ui
