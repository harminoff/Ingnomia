/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../state/UiFoundationTypes.h"
#include <map>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ingnomia::ui::localization
{
struct TextArgument { std::string name; std::string value; };
enum class MissingKeyPolicy : std::uint8_t { Mark, UseKey };

class UiText final
{
public:
	UiText() : UiText( englishEntries() ) {}
	explicit UiText( std::map<std::string,std::string,std::less<>> entries, MissingKeyPolicy missing = MissingKeyPolicy::Mark )
		: entries_( std::move( entries ) ), missing_( missing ) {}
	[[nodiscard]] std::string format( LocalizationKey key, std::span<const TextArgument> arguments = {} ) const
	{
		auto found = entries_.find( key.value );
		std::string result = found == entries_.end()
			? ( missing_ == MissingKeyPolicy::UseKey ? key.value : "⟦" + key.value + "⟧" )
			: found->second;
		for( const auto& argument : arguments )
		{
			const auto token = "{" + argument.name + "}";
			std::size_t at = 0;
			while( ( at = result.find( token, at ) ) != std::string::npos )
			{
				result.replace( at, token.size(), argument.value );
				at += argument.value.size();
			}
		}
		return result;
	}
	[[nodiscard]] std::string format( LocalizationKey key, std::initializer_list<TextArgument> arguments ) const { return format(key,std::span<const TextArgument>(arguments.begin(),arguments.size())); }
	[[nodiscard]] bool contains( LocalizationKey key ) const { return entries_.contains( key.value ); }
	[[nodiscard]] static UiText english() { return UiText( englishEntries() ); }
	[[nodiscard]] static UiText longStringFixture()
	{
		auto entries = englishEntries();
		for( auto& [key, value] : entries )
			value = "⟦ " + value + " — extended localization fixture with UTF-8: Ångström, Жук, 日本語, مرحبا ⟧";
		return UiText( std::move( entries ) );
	}
private:
	[[nodiscard]] static std::map<std::string,std::string,std::less<>> englishEntries()
	{
		return {
			{"hud.build.unavailable","Unavailable: missing materials"},{"hud.build.available","Available"},{"hud.build.cannot_build","Cannot build"},{"ui.error.build_materials_unavailable","Cannot place: required materials are unavailable"},
			{"common.close","Close"},{"common.locate","Locate"},{"common.refresh","Refresh"},{"common.cancel","Cancel"},{"common.search","Search"},
			{"common.loading","Loading…"},{"common.no_results","No matching results."},{"common.previous_rows","Previous rows"},{"common.next_rows","Next rows"},
			{"workshop.title","Workshop"},{"workshop.craft_catalog","Craft catalog"},{"workshop.production_queue","Production queue"},
			{"stockpile.title","Stockpile"},{"agriculture.title","Agriculture"},{"trade.confirm.title","Complete this trade?"},
			{"status.updating","Updating…"},{"status.refreshing","Refreshing…"},{"error.missing_key","Missing text: {key}"},
			{"hud.gnomes","Gnomes {count}"},{"hud.animals","Animals {count}"},{"hud.items","Items {count}"},{"hud.date","Day {day} / Year {year}"},{"hud.level","Z {level}"},{"hud.pause","Pause"},{"hud.resume","Resume"},{"hud.inspect","Inspect"},{"hud.selection","{width}×{height}×{depth} | valid {valid} / invalid {invalid}"},{"hud.rotate_hint","R rotate"},
			{"hud.speed.normal","Normal"},{"hud.speed.fast","Fast"},{"hud.overlay.designations","Designations"},{"hud.overlay.jobs","Jobs"},{"hud.overlay.walls","Walls"},{"hud.overlay.axles","Mechanics"},{"hud.tip.inventory","Open inventory and resources"},{"hud.tip.military","Manage squads, roles, uniforms, and target priorities"},{"hud.tip.population","View gnomes, work assignments, and schedules"},{"hud.tip.missions","View neighbors and current missions"},{"hud.tip.build","Choose and place construction, furniture, and workshops"},{"hud.tip.deconstruct","Remove a constructed object or terrain piece"},{"hud.tip.mine","Choose mining and excavation orders"},{"hud.tip.agriculture","Cut, plant, harvest, forage, and remove plants"},{"hud.tip.designations","Mark stockpiles, farms, rooms, zones, and other areas"},{"hud.tip.jobs","Suspend, resume, cancel, or reprioritize jobs"},{"hud.tip.overlay_designations","Show or hide designation markers"},{"hud.tip.overlay_jobs","Show or hide job markers"},{"hud.tip.overlay_walls","Show lower wall faces for easier map viewing"},{"hud.tip.mechanics","Show powered mechanical axles and other mechanism overlays"},{"hud.tool.mine","Mine"},{"hud.tool.build","Build"},{"hud.tool.explore_mine","Explore mine"},{"hud.tool.fell","Fell"},{"hud.tool.harvest","Harvest"},{"hud.tool.stockpile","Stockpile"},{"hud.tool.farm","Farm"},{"hud.tool.cancel","Cancel"},{"hud.tool.rotate","Rotate"},{"hud.build.title","Build"},{"hud.build.categories","Categories"},{"hud.build.types","Types"},{"hud.build.category_hint","Choose a category"},{"hud.build.catalog_title","Construction pieces"},{"hud.build.catalog_hint","Select a piece to place"},{"hud.build.no_items","No buildable pieces in this category."},{"hud.build.action_build","Build"},{"hud.build.furniture","Furniture"},{"hud.build.workshop","Workshops"},{"hud.build.containers","Containers"},{"hud.build.utility","Utility"},{"hud.build.wall","Walls"},{"hud.build.floor","Floors"},{"hud.build.stairs","Stairs"},{"hud.build.ramps","Ramps"},{"hud.build.fence","Fences"},{"hud.hint.select","LMB select / drag"},{"hud.hint.cancel","RMB/Esc cancel"},{"hud.event.continue","Continue"},{"common.yes_title","Yes"},{"common.no_title","No"},
			{"management.priority_summary","Priority {priority} / {maximum}"},{"management.active","Active"},{"management.suspended","Suspended"},{"common.yes","yes"},{"common.no","no"},{"common.on","on"},{"common.off","off"},
			{"management.stockpile_summary","{items} stored | {reserved} incoming | {status}"},{"management.agriculture_summary","Priority {priority} / {maximum} | plots {plots} | planted {planted} | ready {ready}"},
			{"inspector.selection","Selection"},{"inspector.tile","Tile"},{"inspector.position","X {x}  Y {y}  Z {z}"},{"inspector.static.tile_remove_floor","Remove floor"},{"inspector.static.tile_fell","Fell tree"},{"inspector.static.tile_remove_plant","Remove plant"},
			{"inspector.static.skills","Skills"},{"inspector.static.no_skills","No skills reported"},{"inspector.static.equipment","Equipment"},{"inspector.static.no_equipment","No worn items"},{"inspector.static.inventory","Inventory"},{"inspector.static.no_inventory","No carried items"},{"inspector.static.profession_choices","Set profession"},{"inspector.static.no_profession_choices","No profession choices available"},{"inspector.static.no_activity","Activity is not reported for this creature"},{"inspector.static.not_reported","Not reported"},
			{"ui.confirm.end_world.title","Return to main menu?"},{"ui.confirm.end_world.detail","Unsaved progress in the current kingdom may be lost."},{"ui.confirm.exit.title","Exit Ingnomia?"},{"ui.confirm.exit.detail","Unsaved progress may be lost."},{"ui.confirm.load_other_world.detail","Loading another world ends the current session."},{"ui.error.action_rejected","The action was rejected."},{"ui.status.saving_game","Saving game…"},{"ui.status.game_saved","Game saved."},{"ui.error.save_failed","The game could not be saved."},{"ui.error.save_list_unavailable","The save list could not be read."},
			#include "UiTextStaticEntries.inc"
		};
	}
	std::map<std::string,std::string,std::less<>> entries_;
	MissingKeyPolicy missing_{ MissingKeyPolicy::Mark };
};
}
