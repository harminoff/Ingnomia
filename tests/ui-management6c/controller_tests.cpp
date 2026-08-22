#include "gui/ui/screens/management6c/Management6CController.h"
#include "gui/ui/screens/management6c/Management6CDomWindow.h"

#include <cstdlib>
#include <iostream>

using namespace ingnomia::ui;
using namespace ingnomia::ui::management6c;

namespace
{
void check( bool value, const char* message )
{
	if( !value )
	{
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}

struct SentCommand
{
	UiActionEnvelope action;
	DispatchOrigin origin;
};

struct Port final : CommandPort
{
	std::vector<SentCommand> commands;
	CommandResult next{};
	CommandResult dispatch( const UiActionEnvelope& action, DispatchOrigin origin ) override
	{
		commands.push_back( { action, origin } );
		const auto result = next;
		next = {};
		return result;
	}
};

struct Screen final : ViewPort
{
	Management6CState state;
	std::size_t changes{};
	void stateChanged( const Management6CState& value ) override
	{
		state = value;
		++changes;
	}
};

TargetPriorityRow priority( const char* id, const char* name, MilitaryAttitude attitude = MilitaryAttitude::Defend )
{
	return { CatalogId{ id }, name, attitude };
}

SquadRow squad( std::uint32_t id, const char* name, bool up, bool down )
{
	return { SquadId{ id }, name, up, down,
		{ priority( "Goblin", "Goblin" ), priority( "Mant", "Mant", MilitaryAttitude::Attack ) },
		{ SquadMemberRow{ CreatureId{ id + 100 }, std::string{ name } + " member", MilitaryRoleId{ 71 } } } };
}

MilitaryRoleRow role( std::uint32_t id, const char* name )
{
	return { MilitaryRoleId{ id }, name, false,
		{ UniformSlotRow{ UniformSlot::HeadArmor, "Head armor", CatalogId{ "none" }, CatalogId{ "any" },
			{ CatalogId{ "none" }, CatalogId{ "plate" } }, {} },
		  UniformSlotRow{ UniformSlot::Back, "Back", CatalogId{ "none" }, CatalogId{ "any" },
			{ CatalogId{ "none" }, CatalogId{ "quiver" } }, { CatalogId{ "any" }, CatalogId{ "leather" } } } } };
}

NeighborRow neighbor( std::uint32_t id, const char* name, bool discovered, bool goblin )
{
	NeighborRow row;
	row.id = NeighborId{ id };
	row.discovered = discovered;
	if( discovered )
	{
		row.name = name;
		row.distance = "Two days";
		row.type = goblin ? "Goblin kingdom" : "Gnome kingdom";
		row.attitude = "Neutral";
		row.wealth = "Average";
		row.economy = "Mining";
		row.military = "Strong";
		row.canSendEmissary = true;
		row.canSpy = goblin;
		row.canRaid = goblin;
		row.canSabotage = goblin;
	}
	return row;
}

MissionRow mission( std::uint32_t id, MissionType type, MissionAction action, MissionStep step )
{
	MissionRow row;
	row.id = MissionId{ id };
	row.type = type;
	row.action = action;
	row.step = step;
	row.target = NeighborId{ 22 };
	row.participants = { CreatureId{ 501 } };
	row.elapsedHours = 12;
	return row;
}
} // namespace

int main()
{
	for( const auto rowCount : { 4096u, 8192u, 16384u, 32768u, 65536u, 131072u,
		262144u, 524288u, 1048576u, 2097152u, 4194304u } )
	{
		const auto first = boundedDomWindow( rowCount, std::size_t{ 0 }, 0, false );
		const auto middle = boundedDomWindow( rowCount, rowCount / 2, 0, false );
		const auto last = boundedDomWindow( rowCount, rowCount - 1, 0, false );
		const auto manual = boundedDomWindow( rowCount, std::size_t{ 0 }, rowCount - 10, true );
		check( first.size() <= MaximumDynamicRowsPerList && middle.size() <= MaximumDynamicRowsPerList
			&& last.size() <= MaximumDynamicRowsPerList && manual.size() <= MaximumDynamicRowsPerList,
			"synthetic large management lists remain DOM-window bounded" );
		check( middle.begin <= rowCount / 2 && middle.end > rowCount / 2
			&& last.end == rowCount && manual.end == rowCount, "large-list windows preserve selected and paged reachability" );
	}

	Port port;
	Screen screen;
	Management6CController controller( port, screen );
	controller.beginWorld( WorldEpoch{ 44 } );
	check( !controller.state().open, "management workbench starts closed" );
	controller.open( View::Squads );
	check( controller.state().open, "military route opens explicitly" );
	check( port.commands.back().action.id.value == "military.refresh", "military route requests a refresh" );
	check( controller.state().militaryLoad == LoadState::Loading, "military exposes loading state" );

	MilitaryRoster roster;
	roster.squads = { squad( 10, "Zulu Guard", false, true ), squad( 20, "Amber Watch", true, false ) };
	roster.unassigned = { SquadMemberRow{ CreatureId{ 501 }, "Mira", std::nullopt } };
	check( controller.applyMilitary( { WorldEpoch{ 44 }, Revision{ 1 }, roster } ), "military snapshot accepted" );
	check( controller.state().selectedSquad == SquadId{ 10 }, "first source row selected on initial snapshot" );
	controller.setMilitarySort( Sort::Name );
	check( controller.visibleSquads().front().id == SquadId{ 20 }, "visible squad sort is deterministic" );
	controller.selectSquad( SquadId{ 10 } );
	controller.setMilitaryFilter( "amber" );
	check( controller.state().selectedSquad == SquadId{ 10 }, "filter does not retarget stable squad selection" );
	check( controller.state().militarySelectionHidden, "filtered stable selection is reported hidden" );
	controller.setMilitaryFilter( "" );
	check( controller.state().selectedSquad == SquadId{ 10 }, "clearing filter restores the same stable selection" );

	MilitaryRoster removed = roster;
	removed.squads.erase( removed.squads.begin() );
	check( controller.applyMilitary( { WorldEpoch{ 44 }, Revision{ 2 }, removed } ), "replacement military snapshot accepted" );
	check( controller.state().selectedSquad == SquadId{ 20 }, "removed selection falls to nearest source row" );
	const auto beforeGap = port.commands.size();
	check( !controller.applyPriorityPatch( { WorldEpoch{ 44 }, Revision{ 1 }, Revision{ 3 }, SquadId{ 20 }, {} } ),
		"priority base gap rejected" );
	check( port.commands.size() == beforeGap + 1 && port.commands.back().action.id.value == "military.refresh",
		"priority gap queues a military resync" );
	check( !controller.applyPriorityPatch( { WorldEpoch{ 44 }, Revision{ 1 }, Revision{ 4 }, SquadId{ 20 }, {} } )
		&& port.commands.size() == beforeGap + 1, "stale military resync is deduplicated" );
	check( controller.applyMilitary( { WorldEpoch{ 44 }, Revision{ 5 }, removed } ), "military resync snapshot accepted" );
	check( controller.applyPriorityPatch( { WorldEpoch{ 44 }, Revision{ 5 }, Revision{ 6 }, SquadId{ 20 },
		{ priority( "Goblin", "Goblin", MilitaryAttitude::Hunt ) } } ), "priority subset patch accepted" );
	port.next = { CommandStatus::Accepted, true, {} };
	controller.addSquad();
	check( controller.state().pendingAction.has_value(), "queued mutation exposes pending request" );
	const auto pendingCommandCount = port.commands.size();
	controller.addSquad();
	check( port.commands.size() == pendingCommandCount,
		"duplicate mutation is blocked while authoritative confirmation is pending" );
	check( controller.applyMilitary( { WorldEpoch{ 44 }, Revision{ 7 }, removed } )
		&& !controller.state().pendingAction, "authoritative snapshot clears pending presentation state" );

	check( controller.applyRoles( { WorldEpoch{ 44 }, Revision{ 1 }, { role( 71, "Shieldbearer" ), role( 72, "Scout" ) } } ),
		"role snapshot accepted" );
	controller.selectRole( MilitaryRoleId{ 71 } );
	controller.selectUniformSlot( UniformSlot::Back );
	controller.setSelectedUniform( CatalogId{ "quiver" }, CatalogId{ "leather" } );
	check( port.commands.back().action.id.value == "military.set_uniform_slot", "uniform mutation uses typed action" );
	const auto& uniformPayload = std::get<SetUniformSlotPayload>( port.commands.back().action.payload );
	check( uniformPayload.slot == UniformSlot::Back && uniformPayload.type.value == "quiver",
		"uniform mutation preserves the exact authoritative slot" );
	const auto invalidUniformCount = port.commands.size();
	controller.setSelectedUniform( CatalogId{ "invented" }, CatalogId{ "leather" } );
	check( port.commands.size() == invalidUniformCount, "uniform type outside authoritative options is blocked" );

	controller.requestRemoveSelectedRole();
	check( controller.state().destructive && controller.state().destructive->kind == DestructiveKind::Role,
		"role delete opens typed confirmation state" );
	const auto confirmationModal = controller.state().destructive->modal;
	check( static_cast<bool>( confirmationModal ), "destructive blocker has a tracked modal instance" );
	controller.confirmDestructive( ModalInstanceId{ confirmationModal.value + 1 } );
	check( port.commands.back().action.id.value != "military.remove_role", "mismatched confirmation instance cannot dispatch" );
	controller.confirmDestructive( confirmationModal );
	check( port.commands.back().action.id.value == "military.remove_role"
		&& port.commands.back().origin.kind() == DispatchOriginKind::DestructiveConfirmation
		&& port.commands.back().origin.modal() == confirmationModal,
		"destructive action dispatches from the exact visible confirmation instance" );
	controller.selectMember( CreatureId{ 501 } );
	controller.moveMemberSelection( 1 );
	check( controller.state().selectedMember == CreatureId{ 501 }, "unassigned keyboard navigation remains in its own stable list" );
	const auto unassignedCommandCount = port.commands.size();
	controller.removeSelectedMember();
	controller.moveSelectedMember( MoveDirection::Down );
	check( port.commands.size() == unassignedCommandCount, "unassigned citizens cannot dispatch squad removal or movement" );
	controller.assignSelectedMemberToRole();
	check( std::get<AssignRolePayload>( port.commands.back().action.payload ).creature == CreatureId{ 501 },
		"role assignment uses stable creature identity" );

	controller.open( View::Neighbors );
	check( port.commands.back().action.id.value == "diplomacy.refresh", "diplomacy route requests authoritative rows" );
	const auto hidden = neighbor( 11, "must-not-cross", false, false );
	const auto known = neighbor( 22, "KÃ¶nigshÃ¶hle", true, true );
	check( !hidden.name && !hidden.distance && !hidden.wealth, "undiscovered fixture has no display data" );
	check( controller.applyNeighbors( { WorldEpoch{ 44 }, Revision{ 1 }, { hidden, known } } ), "neighbor snapshot accepted" );
	controller.selectNeighbor( NeighborId{ 11 } );
	check( controller.state().missionDraft.type == MissionType::None, "undiscovered target cannot form mission draft" );
	controller.setDiplomacyFilter( "must-not-cross" );
	check( controller.visibleNeighbors().empty(), "masked fields cannot participate in search" );
	controller.setDiplomacyFilter( "" );
	controller.selectNeighbor( NeighborId{ 22 } );
	check( port.commands.back().action.id.value == "diplomacy.refresh_available_gnomes",
		"selecting a discovered target requests eligible gnomes" );
	check( controller.applyAvailableGnomes( { WorldEpoch{ 44 }, Revision{ 1 },
		{ AvailableGnomeRow{ CreatureId{ 501 }, "Mira" }, AvailableGnomeRow{ CreatureId{ 502 }, "Nia" } } } ),
		"eligible gnome snapshot accepted" );
	check( controller.state().missionDraft.type == MissionType::Emissary
		&& controller.state().missionDraft.action == MissionAction::Improve,
		"draft defaults to a source-backed emissary combination" );
	controller.setMissionType( MissionType::Explore );
	check( controller.state().missionDraft.type == MissionType::Emissary, "unsupported targeted Explore combination rejected" );
	controller.setMissionType( MissionType::Spy );
	check( controller.state().missionDraft.type == MissionType::Spy
		&& controller.state().missionDraft.action == MissionAction::None, "spy uses the authoritative no-action combination" );
	controller.setMissionAction( MissionAction::Insult );
	check( controller.state().missionDraft.action == MissionAction::None, "non-emissary action combination rejected" );
	controller.startMission();
	const auto& spy = std::get<StartMissionPayload>( port.commands.back().action.payload );
	check( spy.type == MissionType::Spy && spy.action == MissionAction::None && spy.neighbor == NeighborId{ 22 }
		&& spy.creature == CreatureId{ 501 }, "mission dispatch preserves exhaustive typed combination and stable IDs" );
	controller.setMissionType( MissionType::Emissary );
	controller.setMissionAction( MissionAction::InviteTrader );
	controller.selectMissionGnome( CreatureId{ 502 } );
	check( controller.canStartDraftMission(), "valid emissary draft reports startable" );
	controller.startMission();
	const auto& emissary = std::get<StartMissionPayload>( port.commands.back().action.payload );
	check( emissary.type == MissionType::Emissary && emissary.action == MissionAction::InviteTrader,
		"emissary action combination remains exhaustive and named" );

	controller.open( View::Missions );
	auto outbound = mission( 31, MissionType::Emissary, MissionAction::InviteTrader, MissionStep::Travel );
	check( controller.applyMissions( { WorldEpoch{ 44 }, Revision{ 1 }, { outbound } } ), "mission snapshot accepted" );
	controller.selectMission( MissionId{ 31 } );
	auto returned = outbound;
	returned.step = MissionStep::Returned;
	returned.result.success = true;
	returned.result.totalHours = 48;
	check( controller.applyMissionPatch( { WorldEpoch{ 44 }, Revision{ 1 }, Revision{ 2 }, returned } ),
		"mission event patches by stable identity" );
	check( controller.state().selectedMission == MissionId{ 31 }
		&& controller.state().missions.front().result.success == true, "mission selection survives backed event update" );
	const auto missionGap = port.commands.size();
	check( !controller.applyMissionPatch( { WorldEpoch{ 44 }, Revision{ 1 }, Revision{ 3 }, returned } ),
		"mission revision gap rejected" );
	check( port.commands.size() == missionGap + 1 && port.commands.back().action.id.value == "diplomacy.refresh",
		"mission gap requests full authoritative resync" );
	check( !controller.applyNeighbors( { WorldEpoch{ 43 }, Revision{ 99 }, { known } } ), "old world neighbor data rejected" );

	controller.setMilitaryError( "ui.error.military_unavailable" );
	check( controller.state().militaryLoad == LoadState::Error, "military error state is explicit" );
	controller.close();
	check( !controller.state().open && !controller.state().destructive, "route close hides workbench and clears local modal" );
	controller.endWorld();
	const auto commandCount = port.commands.size();
	check( !controller.applyPriorityPatch( { {}, {}, Revision{ 1 }, SquadId{ 20 }, {} } )
		&& !controller.applyMaterialOptions( { {}, {}, Revision{ 1 }, MilitaryRoleId{ 71 }, UniformSlot::Back, {} } )
		&& !controller.applyMissionPatch( { {}, {}, Revision{ 1 }, returned } ),
		"late queued subset patches are rejected after world unload" );
	controller.refresh();
	controller.addSquad();
	controller.startMission();
	check( port.commands.size() == commandCount, "world unload blocks all management commands" );
	check( controller.state().roster.squads.empty() && controller.state().neighbors.empty()
		&& !controller.state().selectedSquad && !controller.state().selectedNeighbor,
		"world unload clears rows, selection, drafts, and pending state" );

	std::cout << "Management 6C epoch/revision/selection/military/diplomacy tests passed\n";
}
