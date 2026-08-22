/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../screens/management6c/Management6CState.h"

#include <QList>

struct GuiSquad;
struct GuiTargetPriority;
struct GuiMilRole;
struct GuiNeighborInfo;
struct GuiAvailableGnome;
struct Mission;

namespace ingnomia::ui::management6c
{

class Management6CQtDataAdapter
{
public:
	void setWorld( WorldEpoch );
	std::optional<Snapshot<MilitaryRoster>> military( const QList<GuiSquad>& );
	std::optional<PriorityPatch> priorities( std::uint32_t, const QList<GuiTargetPriority>& );
	std::optional<Snapshot<std::vector<MilitaryRoleRow>>> roles( const QList<GuiMilRole>& );
	std::optional<MaterialOptionsPatch> materials( std::uint32_t, const QString&, const QStringList& );
	Snapshot<std::vector<NeighborRow>> neighbors( const QList<GuiNeighborInfo>& );
	Snapshot<std::vector<AvailableGnomeRow>> availableGnomes( const QList<GuiAvailableGnome>& );
	std::optional<Snapshot<std::vector<MissionRow>>> missions( const QList<Mission>& );
	std::optional<RowPatch<MissionRow>> mission( const Mission& );

private:
	WorldEpoch world_;
	Revision squadRevision_, roleRevision_, neighborRevision_, availableGnomeRevision_, missionRevision_;
};

} // namespace ingnomia::ui::management6c

