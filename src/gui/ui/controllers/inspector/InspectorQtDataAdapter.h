/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/inspector/InspectorState.h"

struct GuiTileInfo;
struct GuiCreatureInfo;
struct GuiWorkshopInfo;
struct GuiStockpileInfo;
struct GuiFarmInfo;
struct GuiPastureInfo;
struct GuiGroveInfo;
class QString;

namespace ingnomia::ui::inspector
{
class InspectorQtDataAdapter
{
public:
	static TileInspectorState tile( const GuiTileInfo& );
	static CreatureInspectorState creature( const GuiCreatureInfo& );
	static WorkshopInspectorState workshop( const GuiWorkshopInfo& );
	static StockpileInspectorState stockpile( const GuiStockpileInfo& );
	static AgricultureInspectorState farm( const GuiFarmInfo& );
	static AgricultureInspectorState pasture( const GuiPastureInfo& );
	static AgricultureInspectorState grove( const GuiGroveInfo& );
	static std::optional<WorldPosition> position( const QString& );
};
} // namespace ingnomia::ui::inspector
