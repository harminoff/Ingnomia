/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/management6a/Management6AState.h"

#include <QList>

struct GuiWorkshopInfo;
struct GuiTradeItem;
struct GuiStockpileInfo;
struct GuiFarmInfo;
struct GuiPastureInfo;
struct GuiGroveInfo;
struct GuiPlant;
struct GuiAnimal;

namespace ingnomia::ui::management6a
{
class Management6AQtDataAdapter
{
public:
	static WorkshopSnapshot workshop( const GuiWorkshopInfo& );
	static TradeRow trade( const GuiTradeItem&, TradeParty );
	static StockpileSnapshot stockpile( const GuiStockpileInfo& );
	static AgricultureSnapshot farm( const GuiFarmInfo& );
	static AgricultureSnapshot pasture( const GuiPastureInfo& );
	static AgricultureSnapshot grove( const GuiGroveInfo& );
	static std::vector<AgricultureCatalogRow> plants( const QList<GuiPlant>& );
	static std::vector<AgricultureCatalogRow> animals( const QList<GuiAnimal>& );
};
} // namespace ingnomia::ui::management6a
