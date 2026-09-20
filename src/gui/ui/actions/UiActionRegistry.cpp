/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiActionRegistry.h"

#include "../state/UiRegistries.h"

#include <algorithm>
#include <type_traits>
#include <unordered_set>

namespace ingnomia::ui
{
namespace
{

template<class Payload>
constexpr ActionDefinition action( std::string_view id, ActionScope scope,
	bool requiresConfirmation = false, bool requiresExpectedRevision = false )
{
	return { id, scope, { actionPayloadIndex<Payload>(), 0 }, 1,
		requiresConfirmation, requiresExpectedRevision };
}

template<class First, class Second>
constexpr ActionDefinition actionEither( std::string_view id, ActionScope scope )
{
	return { id, scope, { actionPayloadIndex<First>(), actionPayloadIndex<Second>() }, 2, false, false };
}

constexpr std::array ACTIONS{
	action<NoPayload>( "app.exit", ActionScope::Application, true ),
	action<StartNewGamePayload>( "app.start_new_game", ActionScope::Application ),
	action<NoPayload>( "app.continue_last_game", ActionScope::Application ),
	action<NoPayload>( "app.start_tutorial", ActionScope::Application ),
	action<LoadGamePayload>( "app.load_game", ActionScope::Application ),
	action<NoPayload>( "app.save_game", ActionScope::World ),
	action<NoPayload>( "app.end_world", ActionScope::World, true ),
	action<RoutePayload>( "nav.open", ActionScope::Presentation ),
	actionEither<RoutePayload, NoPayload>( "nav.back", ActionScope::Presentation ),
	actionEither<RoutePayload, NoPayload>( "nav.close", ActionScope::Presentation ),
	action<SetPausedPayload>( "sim.set_paused", ActionScope::World ),
	action<SetSpeedPayload>( "sim.set_speed", ActionScope::World ),
	action<OverlayPayload>( "view.set_overlay", ActionScope::World ),
	action<CenterPayload>( "view.center_on", ActionScope::World ),
	action<NoPayload>( "tutorial.advance", ActionScope::World ),
	action<NoPayload>( "tutorial.skip", ActionScope::World ),
	action<NoPayload>( "tutorial.restart", ActionScope::World ),
	action<NoPayload>( "tutorial.toggle_hints", ActionScope::World ),
	action<NoPayload>( "tutorial.finish", ActionScope::World ),
	action<ChangeLevelPayload>( "view.change_level", ActionScope::World ),
	action<ActivateToolPayload>( "tool.activate", ActionScope::World ),
	action<NoPayload>( "tool.cancel", ActionScope::World ),
	action<NoPayload>( "tool.rotate", ActionScope::World ),
	action<ChooseBuildPayload>( "tool.choose_build", ActionScope::World ),
	action<SetBuildMaterialPayload>( "tool.set_material", ActionScope::Presentation ),
	action<SelectPayload>( "inspect.select", ActionScope::World ),
	action<NoPayload>( "inspect.clear", ActionScope::World ),
	action<TileContextPayload>( "tile.execute_context_action", ActionScope::World ),
	action<EventResponsePayload>( "event.respond", ActionScope::World ),
	action<WatchPayload>( "watch.set", ActionScope::World ),
	action<SetSettingDraftPayload>( "settings.set_draft", ActionScope::Presentation ),
	action<NoPayload>( "settings.apply", ActionScope::Presentation ),
	action<NoPayload>( "settings.revert", ActionScope::Presentation ),
	action<NoPayload>( "settings.reset", ActionScope::Presentation ),
	action<NoPayload>( "load.refresh", ActionScope::Presentation ),
	action<SelectKingdomPayload>( "load.select_kingdom", ActionScope::Presentation ),
	action<SetNewGameFieldPayload>( "new_game.set_field", ActionScope::Presentation ),
	action<NoPayload>( "new_game.randomize_name", ActionScope::Presentation ),
	action<NoPayload>( "new_game.randomize_seed", ActionScope::Presentation ),
	action<PresetTargetPayload>( "new_game.select_preset", ActionScope::Presentation ),
	action<PresetTargetPayload>( "new_game.delete_preset", ActionScope::Presentation, true ),
	action<SavePresetPayload>( "new_game.save_preset", ActionScope::Presentation ),
	action<SetSpeciesPayload>( "new_game.set_species", ActionScope::Presentation ),
	action<SetStartingItemPayload>( "new_game.set_starting_item", ActionScope::Presentation ),
	action<StartingItemTargetPayload>( "new_game.remove_starting_item", ActionScope::Presentation ),
	action<SetStartingAnimalPayload>( "new_game.set_starting_animal", ActionScope::Presentation ),
	action<StartingAnimalTargetPayload>( "new_game.remove_starting_animal", ActionScope::Presentation ),
	action<StockpileTargetPayload>( "stockpile.refresh", ActionScope::World ),
	action<SetStockpileBasicsPayload>( "stockpile.set_basics", ActionScope::World ),
	action<SetStockpileFilterPayload>( "stockpile.set_filter", ActionScope::World ),
	action<WorkshopTargetPayload>( "workshop.refresh", ActionScope::World ),
	action<SetWorkshopBasicsPayload>( "workshop.set_basics", ActionScope::World ),
	action<SetButcherOptionsPayload>( "workshop.set_butcher_options", ActionScope::World ),
	action<SetFisherOptionsPayload>( "workshop.set_fisher_options", ActionScope::World ),
	action<QueueCraftPayload>( "workshop.queue_craft", ActionScope::World ),
	action<SetCraftJobPayload>( "workshop.set_job", ActionScope::World ),
	action<MoveCraftJobPayload>( "workshop.move_job", ActionScope::World ),
	action<CraftJobTargetPayload>( "workshop.cancel_job", ActionScope::World ),
	action<WorkshopTargetPayload>( "trade.refresh", ActionScope::World ),
	action<WorkshopTargetPayload>( "trade.execute", ActionScope::World, true ),
	action<SetTradeOfferPayload>( "trade.set_offer_count", ActionScope::World ),
	action<AgricultureTargetPayload>( "agriculture.refresh", ActionScope::World ),
	action<SetAgricultureBasicsPayload>( "agriculture.set_basics", ActionScope::World ),
	action<SetAgricultureProductPayload>( "agriculture.select_product", ActionScope::World ),
	action<SetHarvestOptionsPayload>( "agriculture.set_harvest_options", ActionScope::World ),
	action<SetGroveOptionsPayload>( "agriculture.set_grove_options", ActionScope::World ),
	action<SetPastureCapPayload>( "agriculture.set_population_caps", ActionScope::World ),
	action<SetButcheringPayload>( "agriculture.set_butchering", ActionScope::World ),
	action<SetPastureFoodPayload>( "agriculture.set_food_allowed", ActionScope::World ),
	action<NoPayload>( "population.refresh", ActionScope::World ),
	action<SetSkillPayload>( "population.set_skill", ActionScope::World ),
	action<SetGnomeSkillsPayload>( "population.set_all_skills_for_gnome", ActionScope::World ),
	action<SetSkillForAllPayload>( "population.set_skill_for_all", ActionScope::World ),
	action<SetProfessionPayload>( "population.set_profession", ActionScope::World ),
	action<SetScheduleCellPayload>( "population.set_schedule_cell", ActionScope::World ),
	action<SetScheduleRowPayload>( "population.set_schedule_row", ActionScope::World ),
	action<SetScheduleColumnPayload>( "population.set_schedule_column", ActionScope::World ),
	action<NoPayload>( "profession.refresh", ActionScope::World ),
	action<CreateProfessionPayload>( "profession.create", ActionScope::World ),
	action<ProfessionTargetPayload>( "profession.delete", ActionScope::World, true ),
	action<UpdateProfessionPayload>( "profession.update", ActionScope::World ),
	action<NoPayload>( "inventory.refresh", ActionScope::World ),
	action<InventoryHistoryPayload>( "inventory.request_history", ActionScope::World ),
	action<NoPayload>( "military.refresh", ActionScope::World ),
	action<NoPayload>( "military.add_squad", ActionScope::World ),
	action<NoPayload>( "military.add_role", ActionScope::World ),
	action<SquadTargetPayload>( "military.remove_squad", ActionScope::World, true ),
	action<RenameSquadPayload>( "military.rename_squad", ActionScope::World ),
	action<MoveSquadPayload>( "military.move_squad", ActionScope::World ),
	action<GnomeTargetPayload>( "military.remove_gnome", ActionScope::World ),
	action<MoveGnomePayload>( "military.move_gnome", ActionScope::World ),
	action<AssignSquadPayload>( "military.assign_squad", ActionScope::World ),
	action<SetAttitudePayload>( "military.set_attitude", ActionScope::World ),
	action<MovePriorityPayload>( "military.move_priority", ActionScope::World ),
	action<RoleTargetPayload>( "military.remove_role", ActionScope::World, true ),
	action<RenameRolePayload>( "military.rename_role", ActionScope::World ),
	action<AssignRolePayload>( "military.assign_role", ActionScope::World ),
	action<SetRoleCivilianPayload>( "military.set_role_civilian", ActionScope::World ),
	action<SetUniformSlotPayload>( "military.set_uniform_slot", ActionScope::World ),
	action<NoPayload>( "diplomacy.refresh", ActionScope::World ),
	action<NoPayload>( "diplomacy.refresh_available_gnomes", ActionScope::World ),
	action<StartMissionPayload>( "diplomacy.start_mission", ActionScope::World ),
	action<SetRoomTenantPayload>( "room.set_tenant", ActionScope::World ),
	action<SetRoomAlarmPayload>( "room.set_alarm", ActionScope::World ),
	action<SetMechanismStatePayload>( "mechanism.set_active", ActionScope::World, false, true ),
	action<SetMechanismStatePayload>( "mechanism.set_inverted", ActionScope::World, false, true ),
	action<SetAutomatonRefuelPayload>( "automaton.set_refuel", ActionScope::World ),
	action<SetAutomatonCorePayload>( "automaton.set_core", ActionScope::World ),
};

bool safeRelativeKey( std::string_view key )
{
	if( key.empty() || key.front() == '/' || key.front() == '\\' || key.find( ':' ) != std::string_view::npos )
		return false;
	std::size_t start = 0;
	while( start <= key.size() )
	{
		const auto end = key.find_first_of( "/\\", start );
		const auto part = key.substr( start, end == std::string_view::npos ? key.size() - start : end - start );
		if( part.empty() || part == "." || part == ".." ) return false;
		if( end == std::string_view::npos ) break;
		start = end + 1;
	}
	return true;
}

bool validEntity( const EntityRef& entity, WorldEpoch activeWorld )
{
	return entity.world == activeWorld && entity.id != 0;
}

bool validCatalog( const CatalogId& id )
{
	return !id.value.empty();
}

bool validInventoryRow( const InventoryRowId& row )
{
	if( !validCatalog( row.category ) ) return false;
	if( row.depth >= InventoryDepth::Group && !validCatalog( row.group ) ) return false;
	if( row.depth >= InventoryDepth::Item && !validCatalog( row.item ) ) return false;
	return row.depth != InventoryDepth::Material || validCatalog( row.material );
}

bool validStockpileFilterRow( const StockpileFilterRowId& row )
{
	if( !row.stockpile || !validCatalog( row.category ) ) return false;
	if( row.depth >= FilterDepth::Group && !validCatalog( row.group ) ) return false;
	if( row.depth >= FilterDepth::Item && !validCatalog( row.item ) ) return false;
	return row.depth != FilterDepth::Material || validCatalog( row.material );
}

bool validAgricultureTarget( const AgricultureTarget& target )
{
	return static_cast<bool>( target.designation );
}

bool validNewGameDraft( const NewGameDraft& draft )
{
	std::unordered_set<std::string_view> fields;
	for( const auto& field : draft.fields )
	{
		if( field.field.value.empty() || !fields.emplace( field.field.value ).second ) return false;
	}
	return true;
}

template<class Payload>
bool basicPayloadValid( const Payload& payload, const ActionValidationContext& context )
{
	if constexpr( std::is_same_v<Payload, LoadGamePayload> )
		return safeRelativeKey( payload.slot.relativeKey );
	else if constexpr( std::is_same_v<Payload, RoutePayload> )
		return UiRegistries::findRoute( payload.route.value, context.developmentBuild ) != nullptr;
	else if constexpr( std::is_same_v<Payload, StartNewGamePayload> )
		return validNewGameDraft( payload.draft );
	else if constexpr( std::is_same_v<Payload, ChangeLevelPayload> )
		return ( !context.minimumLevel || payload.absoluteLevel >= *context.minimumLevel )
			&& ( !context.maximumLevel || payload.absoluteLevel <= *context.maximumLevel );
	else if constexpr( std::is_same_v<Payload, CenterPayload> )
		return !std::holds_alternative<EntityRef>( payload.target )
			|| validEntity( std::get<EntityRef>( payload.target ), context.activeWorld );
	else if constexpr( std::is_same_v<Payload, SelectPayload> )
		return validEntity( payload.target, context.activeWorld );
	else if constexpr( std::is_same_v<Payload, ActivateToolPayload> )
		return UiRegistries::hasTool( payload.tool.value )
			&& ( !payload.item || !payload.item->value.empty() )
			&& std::ranges::all_of( payload.materials, []( const CatalogId& id ) { return !id.value.empty(); } );
	else if constexpr( std::is_same_v<Payload, ChooseBuildPayload> )
		return !payload.item.value.empty()
			&& std::ranges::all_of( payload.materials, []( const CatalogId& id ) { return !id.value.empty(); } );
	else if constexpr( std::is_same_v<Payload, EventResponsePayload> )
		return static_cast<bool>( payload.prompt );
	else if constexpr( std::is_same_v<Payload, TileContextPayload> )
		return static_cast<bool>( payload.tile );
	else if constexpr( std::is_same_v<Payload, WatchPayload> )
		return validInventoryRow( payload.row );
	else if constexpr( std::is_same_v<Payload, SetSettingDraftPayload> )
		return static_cast<bool>( payload.setting );
	else if constexpr( std::is_same_v<Payload, SelectKingdomPayload> )
		return safeRelativeKey( payload.kingdom.relativeKey );
	else if constexpr( std::is_same_v<Payload, SetNewGameFieldPayload> )
		return static_cast<bool>( payload.field );
	else if constexpr( std::is_same_v<Payload, SavePresetPayload> )
		return payload.preset && validNewGameDraft( payload.draft );
	else if constexpr( std::is_same_v<Payload, SetSpeciesPayload> )
		return validCatalog( payload.species );
	else if constexpr( std::is_same_v<Payload, StartingItemTargetPayload> )
		return validCatalog( payload.item ) && validCatalog( payload.material );
	else if constexpr( std::is_same_v<Payload, StartingAnimalTargetPayload> )
		return validCatalog( payload.species );
	else if constexpr( std::is_same_v<Payload, SetScheduleCellPayload> )
		return payload.cell.creature && payload.cell.hour < 24
			&& payload.activity <= ScheduleActivity::Training;
	else if constexpr( std::is_same_v<Payload, SetScheduleRowPayload> )
		return payload.creature && payload.activity <= ScheduleActivity::Training;
	else if constexpr( std::is_same_v<Payload, SetScheduleColumnPayload> )
		return payload.hour < 24 && payload.activity <= ScheduleActivity::Training;
	else if constexpr( std::is_same_v<Payload, SetStartingItemPayload> )
		return validCatalog( payload.item ) && validCatalog( payload.material ) && payload.amount > 0;
	else if constexpr( std::is_same_v<Payload, SetStartingAnimalPayload> )
		return validCatalog( payload.species ) && payload.amount > 0;
	else if constexpr( std::is_same_v<Payload, SetBuildMaterialPayload> )
		return validCatalog( payload.material );
	else if constexpr( std::is_same_v<Payload, SetStockpileFilterPayload> )
		return validStockpileFilterRow( payload.row );
	else if constexpr( std::is_same_v<Payload, QueueCraftPayload> )
		return payload.workshop && validCatalog( payload.craft ) && payload.count > 0
			&& std::ranges::all_of( payload.materials, validCatalog );
	else if constexpr( std::is_same_v<Payload, SetCraftJobPayload> )
		return payload.workshop && payload.job && payload.count > 0;
	else if constexpr( std::is_same_v<Payload, MoveCraftJobPayload>
		|| std::is_same_v<Payload, CraftJobTargetPayload> )
		return payload.workshop && payload.job;
	else if constexpr( std::is_same_v<Payload, SetTradeOfferPayload> )
		return payload.workshop && validCatalog( payload.row.item ) && validCatalog( payload.row.materialOrGender );
	else if constexpr( std::is_same_v<Payload, AgricultureTargetPayload> )
		return validAgricultureTarget( payload.target );
	else if constexpr( std::is_same_v<Payload, SetAgricultureBasicsPayload>
		|| std::is_same_v<Payload, SetHarvestOptionsPayload> )
		return validAgricultureTarget( payload.target );
	else if constexpr( std::is_same_v<Payload, SetAgricultureProductPayload> )
		return validAgricultureTarget( payload.target ) && validCatalog( payload.product );
	else if constexpr( std::is_same_v<Payload, SetPastureFoodPayload> )
		return payload.pasture && validCatalog( payload.item ) && validCatalog( payload.material );
	else if constexpr( std::is_same_v<Payload, SetSkillPayload> )
		return payload.creature && validCatalog( payload.skill );
	else if constexpr( std::is_same_v<Payload, SetSkillForAllPayload> )
		return validCatalog( payload.skill );
	else if constexpr( std::is_same_v<Payload, SetProfessionPayload> )
		return payload.creature && payload.profession;
	else if constexpr( std::is_same_v<Payload, CreateProfessionPayload> )
		return !payload.name.empty();
	else if constexpr( std::is_same_v<Payload, UpdateProfessionPayload> )
		return payload.current && !payload.newName.empty()
			&& std::ranges::all_of( payload.skills, validCatalog );
	else if constexpr( std::is_same_v<Payload, InventoryHistoryPayload> )
		return validCatalog( payload.item ) && ( payload.material.value.empty() || validCatalog( payload.material ) );
	else if constexpr( std::is_same_v<Payload, RenameSquadPayload> )
		return payload.squad && !payload.name.empty();
	else if constexpr( std::is_same_v<Payload, RenameRolePayload> )
		return payload.role && !payload.name.empty();
	else if constexpr( std::is_same_v<Payload, SetAttitudePayload> )
		return payload.squad && validCatalog( payload.targetType )
			&& payload.attitude <= MilitaryAttitude::Hunt;
	else if constexpr( std::is_same_v<Payload, MovePriorityPayload> )
		return payload.squad && validCatalog( payload.targetType );
	else if constexpr( std::is_same_v<Payload, AssignRolePayload> )
		return payload.creature && payload.role;
	else if constexpr( std::is_same_v<Payload, AssignSquadPayload> )
		return payload.creature && payload.squad;
	else if constexpr( std::is_same_v<Payload, SetUniformSlotPayload> )
		return payload.role && validCatalog( payload.type )
			&& payload.slot <= UniformSlot::Back
			&& ( !payload.material || validCatalog( *payload.material ) );
	else if constexpr( std::is_same_v<Payload, StartMissionPayload> )
		return payload.neighbor && payload.creature
			&& payload.type >= MissionType::Explore && payload.type <= MissionType::Sabotage
			&& payload.action <= MissionAction::InviteAmbassador;
	else if constexpr( std::is_same_v<Payload, SetRoomTenantPayload> )
		return payload.room && ( !payload.tenant || static_cast<bool>( *payload.tenant ) );
	else if constexpr( std::is_same_v<Payload, SetAutomatonCorePayload> )
		return payload.automaton && validCatalog( payload.core );
	else if constexpr( requires { payload.workshop; } )
		return static_cast<bool>( payload.workshop );
	else if constexpr( requires { payload.stockpile; } )
		return static_cast<bool>( payload.stockpile );
	else if constexpr( requires { payload.creature; } )
		return static_cast<bool>( payload.creature );
	else if constexpr( requires { payload.squad; } )
		return static_cast<bool>( payload.squad );
	else if constexpr( requires { payload.role; } )
		return static_cast<bool>( payload.role );
	else if constexpr( requires { payload.mechanism; } )
		return static_cast<bool>( payload.mechanism );
	else if constexpr( requires { payload.automaton; } )
		return static_cast<bool>( payload.automaton );
	else if constexpr( requires { payload.room; } )
		return static_cast<bool>( payload.room );
	else if constexpr( requires { payload.grove; } )
		return static_cast<bool>( payload.grove );
	else if constexpr( requires { payload.pasture; } )
		return static_cast<bool>( payload.pasture );
	else if constexpr( requires { payload.profession; } )
		return static_cast<bool>( payload.profession );
	else if constexpr( requires { payload.preset; } )
		return static_cast<bool>( payload.preset );
	else if constexpr( requires { payload.setting; } )
		return static_cast<bool>( payload.setting );
	else if constexpr( requires { payload.field; } )
		return static_cast<bool>( payload.field );
	else if constexpr( requires { payload.target.designation; } )
		return static_cast<bool>( payload.target.designation );
	else
		return true;
}

} // namespace

bool ActionDefinition::acceptsPayload( std::size_t index ) const noexcept
{
	for( std::uint8_t position = 0; position < payloadCount; ++position )
	{
		if( payloadIndexes[position] == index ) return true;
	}
	return false;
}

std::span<const ActionDefinition> UiActionRegistry::actions() noexcept
{
	return ACTIONS;
}

const ActionDefinition* UiActionRegistry::find( std::string_view id ) noexcept
{
	for( const auto& definition : ACTIONS )
	{
		if( definition.id == id ) return &definition;
	}
	return nullptr;
}

ActionValidation UiActionRegistry::audit() noexcept
{
	std::unordered_set<std::string_view> ids;
	for( const auto& definition : ACTIONS )
	{
		if( definition.id.empty() || !ids.emplace( definition.id ).second )
			return { ActionValidationCode::UnknownAction, "action identifiers must be non-empty and unique" };
		if( definition.payloadCount == 0 || definition.payloadCount > definition.payloadIndexes.size() )
			return { ActionValidationCode::PayloadMismatch, "action payload registration count is invalid" };
		for( std::uint8_t index = 0; index < definition.payloadCount; ++index )
		{
			if( definition.payloadIndexes[index] >= std::variant_size_v<UiActionPayload> )
				return { ActionValidationCode::PayloadMismatch, "action payload registration is outside the closed variant" };
		}
	}
	return {};
}

ActionValidation UiActionRegistry::validate( const UiActionEnvelope& envelope,
	const ActionValidationContext& context ) const
{
	const auto* definition = find( envelope.id.value );
	if( !definition ) return { ActionValidationCode::UnknownAction, "action identifier is not registered" };
	if( !envelope.request ) return { ActionValidationCode::InvalidRequest, "request identifier must be non-zero" };
	if( !definition->acceptsPayload( envelope.payload.index() ) )
		return { ActionValidationCode::PayloadMismatch, "action payload does not match the registered closed variant" };

	if( context.topModal )
	{
		if( !context.sourceModal || *context.sourceModal != *context.topModal )
			return { ActionValidationCode::BlockedByModal, "only the top modal may dispatch while a modal is active" };
	}
	else if( context.sourceModal || context.topModalKind )
		return { ActionValidationCode::IllegalContext, "modal source is not the active top modal" };

	if( definition->requiresConfirmation
		&& ( !context.sourceModal || context.topModalKind != ModalKind::DestructiveConfirmation ) )
		return { ActionValidationCode::ConfirmationRequired,
			"destructive action must dispatch from the active typed confirmation modal" };
	if( envelope.id.value == "event.respond" && context.topModalKind != ModalKind::EventPrompt )
		return { ActionValidationCode::IllegalContext,
			"event response must dispatch from the active event prompt modal" };

	if( envelope.world && *envelope.world != context.activeWorld )
		return { ActionValidationCode::StaleWorld, "action world epoch is stale" };

	if( definition->scope == ActionScope::World )
	{
		if( !envelope.world ) return { ActionValidationCode::MissingWorld, "world action requires an epoch" };
		if( !context.activeWorld || !context.acceptsWorldActions )
			return { ActionValidationCode::WorldUnavailable, "active world does not currently accept actions" };
		if( context.primaryRoute.value != "game.hud" )
			return { ActionValidationCode::IllegalContext, "world action requires the game HUD primary route" };
	}

	if( definition->requiresExpectedRevision && !envelope.expectedRevision )
		return { ActionValidationCode::MissingExpectedRevision,
			"action requires a revision-matched desired-state command" };
	if( envelope.expectedRevision )
	{
		if( !context.authoritativeRevision || *envelope.expectedRevision != *context.authoritativeRevision )
			return { ActionValidationCode::StaleRevision, "expected revision no longer matches authoritative UI state" };
	}

	auto payloadValidation = validatePayload( envelope.payload, context );
	if( !payloadValidation.valid() ) return payloadValidation;
	return {};
}

ActionValidation UiActionRegistry::validatePayload( const UiActionPayload& payload,
	const ActionValidationContext& context )
{
	const bool valid = std::visit( [&]( const auto& value ) { return basicPayloadValid( value, context ); }, payload );
	return valid ? ActionValidation{} : ActionValidation{ ActionValidationCode::InvalidPayload,
		"typed payload contains a missing target, invalid range, unsafe relative key, or unknown registry identifier" };
}

} // namespace ingnomia::ui
