/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "LegacyToolActionAdapter.h"
#include <array>
namespace ingnomia::ui::hud
{
std::optional<std::string_view> LegacyToolActionAdapter::action( const ToolId& tool ) noexcept
{
	using Pair = std::pair<std::string_view, std::string_view>;
	static constexpr std::array<Pair, 31> values{{
		{ "inspect", "" }, { "mine", "Mine" }, { "explorative_mine", "ExplorativeMine" },
		{ "remove_floor", "RemoveFloor" }, { "dig_stairs_down", "DigStairsDown" },
		{ "mine_stairs_up", "MineStairsUp" }, { "dig_ramp_down", "DigRampDown" }, { "dig_hole", "DigHole" },
		{ "fell_tree", "FellTree" }, { "plant_tree", "PlantTree" }, { "harvest_tree", "HarvestTree" }, { "forage", "Forage" },
		{ "remove_plant", "RemovePlant" }, { "create_stockpile", "CreateStockpile" },
		{ "create_farm", "CreateFarm" }, { "create_grove", "CreateGrove" }, { "create_pasture", "CreatePasture" },
		{ "create_personal_room", "CreateRoom" }, { "create_dormitory", "CreateDorm" },
		{ "create_dining_hall", "CreateDining" }, { "create_hospital", "CreateHospital" },
		{ "create_forbidden_area", "CreateNoPass" }, { "create_guard_area", "CreateGuardArea" }, { "remove_designation", "RemoveDesignation" },
		{ "suspend_job", "SuspendJob" }, { "resume_job", "ResumeJob" }, { "cancel_job", "CancelJob" },
		{ "raise_job_priority", "RaisePrio" }, { "lower_job_priority", "LowerPrio" }, { "deconstruct", "Deconstruct" }, { "build", "" }
	}};
	for( const auto& [id, legacy] : values ) if( id == tool.value ) return legacy;
	return std::nullopt;
}
}
