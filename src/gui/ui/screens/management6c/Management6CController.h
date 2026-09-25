/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "Management6CState.h"
#include <array>

namespace ingnomia::ui::management6c
{

enum class CommandStatus : std::uint8_t { Accepted, Rejected };
enum class DispatchOriginKind : std::uint8_t { Workbench, DestructiveConfirmation };

class DispatchOrigin
{
	public:
	DispatchOrigin() = default;
	[[nodiscard]] DispatchOriginKind kind() const noexcept { return kind_; }
	[[nodiscard]] std::optional<ModalInstanceId> modal() const noexcept { return modal_; }
	bool operator==( const DispatchOrigin& ) const = default;

	private:
	explicit DispatchOrigin( ModalInstanceId value ) :
		kind_( DispatchOriginKind::DestructiveConfirmation ), modal_( value ) {}
	DispatchOriginKind kind_{ DispatchOriginKind::Workbench };
	std::optional<ModalInstanceId> modal_;
	friend class Management6CController;
};

struct CommandResult
{
	CommandStatus status{ CommandStatus::Accepted };
	bool pending{};
	std::string error;
};

class CommandPort
{
public:
	virtual ~CommandPort() = default;
	virtual CommandResult dispatch( const UiActionEnvelope&, DispatchOrigin ) = 0;
};

class ViewPort
{
public:
	virtual ~ViewPort() = default;
	virtual void stateChanged( const Management6CState& ) = 0;
};

class Management6CController
{
public:
	Management6CController( CommandPort&, ViewPort& );
	void addViewPort( ViewPort& );
	void removeViewPort( ViewPort& );
	[[nodiscard]] const Management6CState& state() const noexcept { return state_; }

	void beginWorld( WorldEpoch );
	void endWorld();
    void activateViewForInput(View view);
	void open( View );
	void close();
	void closeMilitary();
	void closeDiplomacy();
	void refresh();
	void setMilitaryFilter( std::string );
	void setDiplomacyFilter( std::string );
	void setMilitarySort( Sort );
	void setDiplomacySort( Sort );
	void setMilitaryError( std::string );
	void setDiplomacyError( std::string );

	[[nodiscard]] std::vector<SquadRow> visibleSquads() const;
	[[nodiscard]] std::vector<MilitaryRoleRow> visibleRoles() const;
	[[nodiscard]] std::vector<NeighborRow> visibleNeighbors() const;
	[[nodiscard]] std::vector<MissionRow> visibleMissions() const;

	void selectSquad( SquadId );
	void selectRole( MilitaryRoleId );
	void selectMember( CreatureId );
	void selectPriority( CatalogId );
	void selectUniformSlot( UniformSlot );
	void selectNeighbor( NeighborId );
	void selectMission( MissionId );
	void selectNext();
	void selectPrevious();
	void moveMemberSelection( std::int32_t );
	void movePrioritySelection( std::int32_t );
	void moveUniformSelection( std::int32_t );
	void moveMissionGnomeSelection( std::int32_t );

	bool applyMilitary( Snapshot<MilitaryRoster> );
	bool applyPriorityPatch( PriorityPatch );
	bool applyRoles( Snapshot<std::vector<MilitaryRoleRow>> );
	bool applyMaterialOptions( MaterialOptionsPatch );
	bool applyNeighbors( Snapshot<std::vector<NeighborRow>> );
	bool applyAvailableGnomes( Snapshot<std::vector<AvailableGnomeRow>> );
	bool applyMissions( Snapshot<std::vector<MissionRow>> );
	bool applyMissionPatch( RowPatch<MissionRow> );

	void addSquad();
	void renameSelectedSquad( std::string );
	void moveSelectedSquad( MoveDirection );
	void requestRemoveSelectedSquad();
	void removeSelectedMember();
	void assignSelectedMemberToSelectedSquad();
	/// Moves a citizen to a named squad (from any squad, or from none).
	void assignMemberToSquad( CreatureId, SquadId );
	void moveSelectedMember( MoveDirection );
	void setSelectedAttitude( MilitaryAttitude );
	void moveSelectedPriority( MoveDirection );
	void addRole();
	void renameSelectedRole( std::string );
	void requestRemoveSelectedRole();
	void assignSelectedMemberToRole();
	void assignMemberRole( CreatureId, MilitaryRoleId );
	void setSelectedRoleCivilian( bool );
	void setSelectedUniform( CatalogId, std::optional<CatalogId> );
	void cancelDestructive();
	void confirmDestructive( ModalInstanceId );

	void setMissionType( MissionType );
	void setMissionAction( MissionAction );
	void selectMissionGnome( CreatureId );
	void startMission();
	[[nodiscard]] bool canStartDraftMission() const;
	void onActionFinished( RequestId, CommandResult );

private:
	bool dispatch( std::string_view, UiActionPayload, DispatchOrigin = {} );
	bool accepts( WorldEpoch, Revision, Revision ) const;
	void requestMilitaryResync();
	void requestDiplomacyResync();
	void requestMissionResync();
	void reconcileMilitarySelection( const std::optional<SquadId>& previousSquad,
		std::size_t previousSquadIndex, const std::optional<MilitaryRoleId>& previousRole,
		std::size_t previousRoleIndex );
	void reconcileDiplomacySelection( const std::optional<NeighborId>& previousNeighbor,
		std::size_t previousNeighborIndex, const std::optional<MissionId>& previousMission,
		std::size_t previousMissionIndex );
	void updateHiddenSelectionFlags();
	void configureMissionDraft();
	void requestAvailableGnomes();
	void notify();

	CommandPort& commands_;
	std::vector<ViewPort*> views_;
	Management6CState state_;
	std::array<std::string, 5> filtersByView_{};
	std::array<Sort, 5> sortsByView_{};
	std::uint64_t nextRequest_{ 1 };
	std::uint64_t nextModal_{ 1 };
};

} // namespace ingnomia::ui::management6c
