/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CQtCommandPort.h"

#include "../../../aggregatormilitary.h"
#include "../../../aggregatorneighbors.h"
#include "../../../eventconnector.h"
#include "../../actions/UiActionRegistry.h"

#include <QMetaObject>

namespace ingnomia::ui::management6c
{
namespace
{
std::optional<::MilAttitude> domainAttitude( MilitaryAttitude value )
{
	switch( value )
	{
		case MilitaryAttitude::Flee: return ::MilAttitude::FLEE;
		case MilitaryAttitude::Defend: return ::MilAttitude::DEFEND;
		case MilitaryAttitude::Attack: return ::MilAttitude::ATTACK;
		case MilitaryAttitude::Hunt: return ::MilAttitude::HUNT;
	}
	return std::nullopt;
}

std::optional<QString> domainSlot( UniformSlot value )
{
	switch( value )
	{
		case UniformSlot::HeadArmor: return "HeadArmor";
		case UniformSlot::ChestArmor: return "ChestArmor";
		case UniformSlot::ArmArmor: return "ArmArmor";
		case UniformSlot::HandArmor: return "HandArmor";
		case UniformSlot::LegArmor: return "LegArmor";
		case UniformSlot::FootArmor: return "FootArmor";
		case UniformSlot::LeftHandHeld: return "LeftHandHeld";
		case UniformSlot::RightHandHeld: return "RightHandHeld";
		case UniformSlot::Back: return "Back";
	}
	return std::nullopt;
}

std::optional<::MissionType> domainMissionType( MissionType value )
{
	switch( value )
	{
		case MissionType::None: return std::nullopt;
		case MissionType::Explore: return ::MissionType::EXPLORE;
		case MissionType::Spy: return ::MissionType::SPY;
		case MissionType::Emissary: return ::MissionType::EMISSARY;
		case MissionType::Raid: return ::MissionType::RAID;
		case MissionType::Sabotage: return ::MissionType::SABOTAGE;
	}
	return std::nullopt;
}

std::optional<::MissionAction> domainMissionAction( MissionAction value )
{
	switch( value )
	{
		case MissionAction::None: return ::MissionAction::NONE;
		case MissionAction::Improve: return ::MissionAction::IMPROVE;
		case MissionAction::Insult: return ::MissionAction::INSULT;
		case MissionAction::InviteTrader: return ::MissionAction::INVITE_TRADER;
		case MissionAction::InviteAmbassador: return ::MissionAction::INVITE_AMBASSADOR;
	}
	return std::nullopt;
}

bool validMissionCombination( MissionType type, MissionAction action )
{
	if( type == MissionType::Emissary )
		return action == MissionAction::Improve || action == MissionAction::Insult
			|| action == MissionAction::InviteTrader || action == MissionAction::InviteAmbassador;
	return ( type == MissionType::Spy || type == MissionType::Raid || type == MissionType::Sabotage )
		&& action == MissionAction::None;
}
} // namespace

Management6CQtCommandPort::Management6CQtCommandPort( EventConnector* connector ) : connector_( connector ) {}

CommandResult Management6CQtCommandPort::reject( const char* error ) const
{
	return { CommandStatus::Rejected, false, error };
}

CommandResult Management6CQtCommandPort::queue( std::function<void()> command ) const
{
	if( !connector_ || !QMetaObject::invokeMethod( connector_, std::move( command ), Qt::QueuedConnection ) )
		return reject( "ui.error.bridge_queue_failed" );
	return { CommandStatus::Accepted, true, {} };
}

CommandResult Management6CQtCommandPort::dispatch( const UiActionEnvelope& action, DispatchOrigin origin )
{
	if( !connector_ ) return reject( "ui.error.bridge_unavailable" );
	ActionValidationContext context;
	context.activeWorld = world_;
	context.acceptsWorldActions = accepts_;
	context.primaryRoute = RouteId{ "game.hud" };
	if( origin.kind() == DispatchOriginKind::DestructiveConfirmation )
	{
		if( !origin.modal() ) return reject( "ui.error.confirmation_origin_invalid" );
		context.topModal = *origin.modal();
		context.sourceModal = *origin.modal();
		context.topModalKind = ModalKind::DestructiveConfirmation;
	}
	else if( origin.modal() ) return reject( "ui.error.confirmation_origin_invalid" );
	const auto validation = UiActionRegistry{}.validate( action, context );
	if( !validation.valid() )
		return reject( validation.code == ActionValidationCode::ConfirmationRequired
			? "ui.error.confirmation_required" : "ui.error.invalid_or_stale_action" );

	QPointer<AggregatorMilitary> military = connector_->aggregatorMilitary();
	QPointer<AggregatorNeighbors> diplomacy = connector_->aggregatorNeighbors();
	if( action.id.value.starts_with( "military." ) && !military ) return reject( "ui.error.military_bridge_unavailable" );
	if( action.id.value.starts_with( "diplomacy." ) && !diplomacy ) return reject( "ui.error.diplomacy_bridge_unavailable" );

	if( action.id.value == "military.refresh" )
		return queue( [military]{ if( military ) { military->onRequestMilitary(); military->onRequestRoles(); } } );
	if( action.id.value == "military.add_squad" )
		return queue( [military]{ if( military ) military->onAddSquad(); } );
	if( action.id.value == "military.remove_squad" )
	{
		const auto* payload = std::get_if<SquadTargetPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, id = payload->squad.value]{ if( military ) military->onRemoveSquad( id ); } );
	}
	if( action.id.value == "military.rename_squad" )
	{
		const auto* payload = std::get_if<RenameSquadPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { military->onRenameSquad( value.squad.value,
			QString::fromStdString( value.name ) ); military->onRequestMilitary(); } } );
	}
	if( action.id.value == "military.move_squad" )
	{
		const auto* payload = std::get_if<MoveSquadPayload>( &action.payload );
		if( !payload || ( payload->direction != MoveDirection::Up && payload->direction != MoveDirection::Down ) )
			return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { if( value.direction == MoveDirection::Up )
			military->onMoveSquadLeft( value.squad.value ); else military->onMoveSquadRight( value.squad.value ); } } );
	}
	if( action.id.value == "military.remove_gnome" )
	{
		const auto* payload = std::get_if<GnomeTargetPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, id = payload->creature.value]{ if( military ) military->onRemoveGnomeFromSquad( id ); } );
	}
	if( action.id.value == "military.move_gnome" )
	{
		const auto* payload = std::get_if<MoveGnomePayload>( &action.payload );
		if( !payload || ( payload->direction != MoveDirection::Up && payload->direction != MoveDirection::Down ) )
			return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { if( value.direction == MoveDirection::Up )
			military->onMoveGnomeLeft( value.creature.value ); else military->onMoveGnomeRight( value.creature.value ); } } );
	}
	if( action.id.value == "military.assign_squad" )
	{
		const auto* payload = std::get_if<AssignSquadPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military )
			military->onAssignGnomeToSquad( value.creature.value, value.squad.value ); } );
	}
	if( action.id.value == "military.set_attitude" )
	{
		const auto* payload = std::get_if<SetAttitudePayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		const auto attitude = domainAttitude( payload->attitude );
		if( !attitude ) return reject( "ui.error.military_attitude_unavailable" );
		return queue( [military, value = *payload, attitude = *attitude]{ if( military ) { military->onSetAttitude(
			value.squad.value, QString::fromStdString( value.targetType.value ), attitude ); military->onRequestMilitary(); } } );
	}
	if( action.id.value == "military.move_priority" )
	{
		const auto* payload = std::get_if<MovePriorityPayload>( &action.payload );
		if( !payload || ( payload->direction != MoveDirection::Up && payload->direction != MoveDirection::Down ) )
			return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { if( value.direction == MoveDirection::Up )
			military->onMovePrioUp( value.squad.value, QString::fromStdString( value.targetType.value ) );
			else military->onMovePrioDown( value.squad.value, QString::fromStdString( value.targetType.value ) ); } } );
	}
	if( action.id.value == "military.add_role" )
		return queue( [military]{ if( military ) military->onAddRole(); } );
	if( action.id.value == "military.remove_role" )
	{
		const auto* payload = std::get_if<RoleTargetPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, id = payload->role.value]{ if( military ) military->onRemoveRole( id ); } );
	}
	if( action.id.value == "military.rename_role" )
	{
		const auto* payload = std::get_if<RenameRolePayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { military->onRenameRole( value.role.value,
			QString::fromStdString( value.name ) ); military->onRequestRoles(); } } );
	}
	if( action.id.value == "military.assign_role" )
	{
		const auto* payload = std::get_if<AssignRolePayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { military->onSetRole( value.creature.value,
			value.role.value ); military->onRequestMilitary(); } } );
	}
	if( action.id.value == "military.set_role_civilian" )
	{
		const auto* payload = std::get_if<SetRoleCivilianPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queue( [military, value = *payload]{ if( military ) { military->onSetRoleCivilian( value.role.value,
			value.civilian ); military->onRequestRoles(); } } );
	}
	if( action.id.value == "military.set_uniform_slot" )
	{
		const auto* payload = std::get_if<SetUniformSlotPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		const auto slot = domainSlot( payload->slot );
		if( !slot ) return reject( "ui.error.uniform_slot_unavailable" );
		const auto material = payload->material ? QString::fromStdString( payload->material->value ) : QString{ "any" };
		return queue( [military, value = *payload, slot = *slot, material]{ if( military ) { military->onSetArmorType(
			value.role.value, slot, QString::fromStdString( value.type.value ), material ); military->onRequestRoles(); } } );
	}

	if( action.id.value == "diplomacy.refresh" )
		return queue( [diplomacy, military]{
            // Participant names come from the existing authoritative citizen roster,
            // including citizens who are currently away on missions.
            if( military ) military->onRequestMilitary();
            if( diplomacy ) { diplomacy->onRequestNeighborsUpdate(); diplomacy->onRequestMissions(); }
        } );
	if( action.id.value == "diplomacy.refresh_available_gnomes" )
		return queue( [diplomacy]{ if( diplomacy ) diplomacy->onRequestAvailableGnomes(); } );
	if( action.id.value == "diplomacy.start_mission" )
	{
		const auto* payload = std::get_if<StartMissionPayload>( &action.payload );
		if( !payload || !validMissionCombination( payload->type, payload->action ) )
			return reject( "ui.error.mission_combination_unavailable" );
		const auto type = domainMissionType( payload->type );
		const auto missionAction = domainMissionAction( payload->action );
		if( !type || !missionAction ) return reject( "ui.error.mission_combination_unavailable" );
		return queue( [diplomacy, value = *payload, type = *type, missionAction = *missionAction]{ if( diplomacy )
			diplomacy->onStartMission( type, missionAction, value.neighbor.value, value.creature.value ); } );
	}
	return reject( "ui.error.action_unavailable" );
}

} // namespace ingnomia::ui::management6c
