/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/management6b/Management6BState.h"

#include <QStringList>

struct GuiPopulationInfo;
struct GuiGnomeInfo;
struct GuiScheduleInfo;
struct GuiGnomeScheduleInfo;
struct GuiInventoryCategory;
struct GuiCreatureInfo;
struct GuiSkillInfo;
struct GuiInventoryHistoryPoint;

namespace ingnomia::ui::management6b
{
class Management6BQtDataAdapter
{
public:
	void setWorld( WorldEpoch );
	Snapshot<std::vector<PopulationRow>> population( const GuiPopulationInfo& );
	RowPatch<PopulationRow> populationPatch( const GuiGnomeInfo& );
	Snapshot<std::vector<ProfessionRow>> professions( const QStringList& );
	std::vector<SkillCatalogRow> skillCatalog( const QList<GuiSkillInfo>& ) const;
	std::pair<ProfessionId, std::vector<CatalogId>> professionSkills( const QString&, const QList<GuiSkillInfo>& ) const;
	Snapshot<std::vector<ScheduleRow>> schedules( const GuiScheduleInfo& );
	RowPatch<ScheduleRow> schedulePatch( const GuiGnomeScheduleInfo& );
	Snapshot<std::vector<InventoryRow>> inventory( const QList<GuiInventoryCategory>& );
	Snapshot<CreatureDetail> creature( const GuiCreatureInfo& );
	std::vector<InventoryHistoryPoint> inventoryHistory( const QList<GuiInventoryHistoryPoint>& ) const;
private:
	WorldEpoch world_;
	Revision populationRevision_, professionRevision_, scheduleRevision_, inventoryRevision_, creatureRevision_;
};
} // namespace ingnomia::ui::management6b
