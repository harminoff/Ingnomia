/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CQtDataAdapter.h"

#include "../../../aggregatormilitary.h"
#include "../../../aggregatorneighbors.h"

#include <algorithm>

namespace ingnomia::ui::management6c
{
namespace
{
std::string text( const QString& value ) { return value.toStdString(); }

std::optional<MilitaryAttitude> attitude( MilAttitude value )
{
	switch( value )
	{
		case MilAttitude::FLEE: return MilitaryAttitude::Flee;
		case MilAttitude::DEFEND: return MilitaryAttitude::Defend;
		case MilAttitude::ATTACK: return MilitaryAttitude::Attack;
		case MilAttitude::HUNT: return MilitaryAttitude::Hunt;
	}
	return std::nullopt;
}

std::optional<UniformSlot> uniformSlot( const QString& value )
{
	if( value == "HeadArmor" ) return UniformSlot::HeadArmor;
	if( value == "ChestArmor" ) return UniformSlot::ChestArmor;
	if( value == "ArmArmor" ) return UniformSlot::ArmArmor;
	if( value == "HandArmor" ) return UniformSlot::HandArmor;
	if( value == "LegArmor" ) return UniformSlot::LegArmor;
	if( value == "FootArmor" ) return UniformSlot::FootArmor;
	if( value == "LeftHandHeld" ) return UniformSlot::LeftHandHeld;
	if( value == "RightHandHeld" ) return UniformSlot::RightHandHeld;
	if( value == "Back" ) return UniformSlot::Back;
	return std::nullopt;
}

std::optional<TargetPriorityRow> targetPriority( const GuiTargetPriority& value )
{
	const auto converted = attitude( value.attitude );
	if( !converted || value.id.isEmpty() ) return std::nullopt;
	return TargetPriorityRow{ CatalogId{ text( value.id ) }, text( value.name ), *converted };
}

SquadMemberRow member( const GuiSquadGnome& value )
{
	return { CreatureId{ value.id }, text( value.name ),
		value.roleID == 0 ? std::nullopt : std::optional{ MilitaryRoleId{ value.roleID } } };
}

std::optional<MissionType> missionType( ::MissionType value )
{
	switch( value )
	{
		case ::MissionType::NOMISSION: return MissionType::None;
		case ::MissionType::EXPLORE: return MissionType::Explore;
		case ::MissionType::SPY: return MissionType::Spy;
		case ::MissionType::EMISSARY: return MissionType::Emissary;
		case ::MissionType::RAID: return MissionType::Raid;
		case ::MissionType::SABOTAGE: return MissionType::Sabotage;
	}
	return std::nullopt;
}

std::optional<MissionAction> missionAction( ::MissionAction value )
{
	switch( value )
	{
		case ::MissionAction::NONE: return MissionAction::None;
		case ::MissionAction::IMPROVE: return MissionAction::Improve;
		case ::MissionAction::INSULT: return MissionAction::Insult;
		case ::MissionAction::INVITE_TRADER: return MissionAction::InviteTrader;
		case ::MissionAction::INVITE_AMBASSADOR: return MissionAction::InviteAmbassador;
	}
	return std::nullopt;
}

std::optional<MissionStep> missionStep( ::MissionStep value )
{
	switch( value )
	{
		case ::MissionStep::NONE: return MissionStep::None;
		case ::MissionStep::LEAVE_MAP: return MissionStep::LeaveMap;
		case ::MissionStep::TRAVEL: return MissionStep::Travel;
		case ::MissionStep::ACTION: return MissionStep::Action;
		case ::MissionStep::RETURN: return MissionStep::Return;
		case ::MissionStep::RETURNED: return MissionStep::Returned;
	}
	return std::nullopt;
}

std::optional<MissionRow> missionRow( const ::Mission& value )
{
	const auto type = missionType( value.type );
	const auto action = missionAction( value.action );
	const auto step = missionStep( value.step );
	if( value.id == 0 || !type || !action || !step ) return std::nullopt;
	MissionRow row;
	row.id = MissionId{ value.id };
	row.type = *type;
	row.action = *action;
	row.step = *step;
	row.target = NeighborId{ value.target };
	row.startTick = value.startTick;
	row.nextCheckTick = value.nextCheckTick;
	row.elapsedHours = value.time;
	row.participants.reserve( static_cast<std::size_t>( value.gnomes.size() ) );
	for( const auto id : value.gnomes ) if( id != 0 ) row.participants.push_back( CreatureId{ id } );
	if( value.result.contains( "Success" ) ) row.result.success = value.result.value( "Success" ).toBool();
	if( value.result.contains( "TotalTime" ) ) row.result.totalHours = value.result.value( "TotalTime" ).toInt();
	return row;
}
} // namespace

void Management6CQtDataAdapter::setWorld( WorldEpoch world )
{
	world_ = world;
	squadRevision_ = {};
	roleRevision_ = {};
	neighborRevision_ = {};
	availableGnomeRevision_ = {};
	missionRevision_ = {};
}

std::optional<Snapshot<MilitaryRoster>> Management6CQtDataAdapter::military( const QList<GuiSquad>& values )
{
	MilitaryRoster roster;
	for( const auto& value : values )
	{
		if( value.id == 0 )
		{
			for( const auto& gnome : value.gnomes ) if( gnome.id != 0 ) roster.unassigned.push_back( member( gnome ) );
			continue;
		}
		SquadRow row;
		row.id = SquadId{ value.id };
		row.name = text( value.name );
		row.canMoveUp = value.showLeftArrow;
		row.canMoveDown = value.showRightArrow;
		for( const auto& priority : value.priorities )
		{
			const auto converted = targetPriority( priority );
			if( !converted ) return std::nullopt;
			row.priorities.push_back( *converted );
		}
		for( const auto& gnome : value.gnomes ) if( gnome.id != 0 ) row.members.push_back( member( gnome ) );
		roster.squads.push_back( std::move( row ) );
	}
	return Snapshot<MilitaryRoster>{ world_, Revision{ ++squadRevision_.value }, std::move( roster ) };
}

std::optional<PriorityPatch> Management6CQtDataAdapter::priorities( std::uint32_t squad,
	const QList<GuiTargetPriority>& values )
{
	if( squad == 0 ) return std::nullopt;
	std::vector<TargetPriorityRow> rows;
	for( const auto& value : values )
	{
		const auto converted = targetPriority( value );
		if( !converted ) return std::nullopt;
		rows.push_back( *converted );
	}
	const auto base = squadRevision_;
	return PriorityPatch{ world_, base, Revision{ ++squadRevision_.value }, SquadId{ squad }, std::move( rows ) };
}

std::optional<Snapshot<std::vector<MilitaryRoleRow>>> Management6CQtDataAdapter::roles(
	const QList<GuiMilRole>& values )
{
	std::vector<MilitaryRoleRow> rows;
	rows.reserve( static_cast<std::size_t>( values.size() ) );
	for( const auto& value : values )
	{
		if( value.id == 0 ) return std::nullopt;
		MilitaryRoleRow row;
		row.id = MilitaryRoleId{ value.id };
		row.name = text( value.name );
		row.civilian = value.isCivilian;
		for( const auto& item : value.uniform )
		{
			const auto slot = uniformSlot( item.slotName );
			if( !slot || item.armorType.isEmpty() ) return std::nullopt;
			UniformSlotRow uniform;
			uniform.slot = *slot;
			uniform.name = text( item.slotName );
			uniform.type = CatalogId{ text( item.armorType ) };
			if( !item.material.isEmpty() ) uniform.material = CatalogId{ text( item.material ) };
			for( const auto& possible : item.possibleTypesForSlot )
				if( !possible.isEmpty() ) uniform.possibleTypes.emplace_back( text( possible ) );
			for( const auto& material : item.possibleMaterials )
				if( !material.isEmpty() ) uniform.possibleMaterials.emplace_back( text( material ) );
			row.uniform.push_back( std::move( uniform ) );
		}
		rows.push_back( std::move( row ) );
	}
	return Snapshot<std::vector<MilitaryRoleRow>>{ world_, Revision{ ++roleRevision_.value }, std::move( rows ) };
}

std::optional<MaterialOptionsPatch> Management6CQtDataAdapter::materials( std::uint32_t role,
	const QString& sourceSlot, const QStringList& values )
{
	const auto slot = uniformSlot( sourceSlot );
	if( role == 0 || !slot ) return std::nullopt;
	std::vector<CatalogId> rows;
	for( const auto& value : values ) if( !value.isEmpty() ) rows.emplace_back( text( value ) );
	const auto base = roleRevision_;
	return MaterialOptionsPatch{ world_, base, Revision{ ++roleRevision_.value }, MilitaryRoleId{ role }, *slot,
		std::move( rows ) };
}

Snapshot<std::vector<NeighborRow>> Management6CQtDataAdapter::neighbors( const QList<GuiNeighborInfo>& values )
{
	std::vector<NeighborRow> rows;
	rows.reserve( static_cast<std::size_t>( values.size() ) );
	for( const auto& value : values )
	{
		if( value.id == 0 ) continue;
		NeighborRow row;
		row.id = NeighborId{ value.id };
		row.discovered = value.discovered;
		if( row.discovered )
		{
			row.name = text( value.name );
			row.distance = text( value.distance );
			row.type = text( value.type );
			row.attitude = text( value.attitude );
			row.wealth = text( value.wealth );
			row.economy = text( value.economy );
			row.military = text( value.military );
			row.canSpy = value.spyMission;
			row.canSabotage = value.sabotageMission;
			row.canRaid = value.raidMission;
			row.canSendEmissary = value.diploMission;
		}
		rows.push_back( std::move( row ) );
	}
	return { world_, Revision{ ++neighborRevision_.value }, std::move( rows ) };
}

Snapshot<std::vector<AvailableGnomeRow>> Management6CQtDataAdapter::availableGnomes(
	const QList<GuiAvailableGnome>& values )
{
	std::vector<AvailableGnomeRow> rows;
	for( const auto& value : values ) if( value.id != 0 ) rows.push_back( { CreatureId{ value.id }, text( value.name ) } );
	return { world_, Revision{ ++availableGnomeRevision_.value }, std::move( rows ) };
}

std::optional<Snapshot<std::vector<MissionRow>>> Management6CQtDataAdapter::missions( const QList<::Mission>& values )
{
	std::vector<MissionRow> rows;
	rows.reserve( static_cast<std::size_t>( values.size() ) );
	for( const auto& value : values )
	{
		const auto converted = missionRow( value );
		if( !converted ) return std::nullopt;
		rows.push_back( *converted );
	}
	return Snapshot<std::vector<MissionRow>>{ world_, Revision{ ++missionRevision_.value }, std::move( rows ) };
}

std::optional<RowPatch<MissionRow>> Management6CQtDataAdapter::mission( const ::Mission& value )
{
	const auto converted = missionRow( value );
	if( !converted ) return std::nullopt;
	const auto base = missionRevision_;
	return RowPatch<MissionRow>{ world_, base, Revision{ ++missionRevision_.value }, *converted };
}

} // namespace ingnomia::ui::management6c

