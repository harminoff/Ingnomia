/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorQtDataAdapter.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorcreatureinfo.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatortileinfo.h"
#include "../../../aggregatorworkshop.h"
#include "../../../../base/global.h"

namespace ingnomia::ui::inspector
{
namespace { std::string text(const QString& v){return v.toStdString();} }
std::optional<WorldPosition> InspectorQtDataAdapter::position(const QString& value){const auto p=value.split(' ',Qt::SkipEmptyParts);if(p.size()!=3)return{};bool a,b,c;const int x=p[0].toInt(&a),y=p[1].toInt(&b),z=p[2].toInt(&c);if(!a||!b||!c)return{};return WorldPosition{x,y,z};}
TileInspectorState InspectorQtDataAdapter::tile(const GuiTileInfo& in)
{
	TileInspectorState out;out.id=TileId{in.tileID};const Position p(in.tileID);out.position={p.x,p.y,p.z};out.wall=text(in.wall);out.floor=text(in.floor);out.embedded=text(in.embedded);out.plant=text(in.plant);out.water=text(in.water);out.construction=text(in.constructed);
	for(const auto&i:in.items)out.items.push_back({text(i.text),text(i.material),i.count});
	for(const auto&c:in.creatures)out.creatures.push_back({CreatureId{c.id},text(c.text),EntityKind::Creature});
	out.jobName=text(in.jobName);out.jobWorker=text(in.jobWorker);out.jobPriority=text(in.jobPriority);out.requiredSkill=text(in.requiredSkill);out.requiredTool=text(in.requiredTool);out.requiredToolAvailable=text(in.requiredToolAvailable);out.workPositions=text(in.workPositions);for(const auto&i:in.requiredItems)out.requiredItems.push_back({text(i.text),text(i.material),i.count});out.hasJob=!in.jobName.isEmpty();out.canRaisePriority=in.canRaisePriority;out.canLowerPriority=in.canLowerPriority;
	if(in.designationID)out.designation=DesignationId{in.designationID};out.designationName=text(in.designationName);
	if(in.designationFlag==TileFlag::TF_ROOM)out.roomSummary="Beds "+text(in.beds)+(in.isEnclosed?" | enclosed":" | open")+(in.hasRoof?" | roofed":" | no roof");
	if(in.mechInfo.itemID)out.mechanismSummary=text(in.mechInfo.name)+" | "+(in.mechInfo.active?"active":"inactive")+(in.mechInfo.isInvertable?(in.mechInfo.inverted?" | inverted":" | normal"):"");
	out.plantIsTree=in.plantIsTree;out.plantIsHarvestable=in.plantIsHarvestable;out.canMine=!in.wall.isEmpty()&&!out.hasJob;out.canRemoveFloor=!in.floor.isEmpty()&&in.wall.isEmpty()&&in.plant.isEmpty()&&!out.hasJob;out.canHarvest=!in.plant.isEmpty()&&in.plantIsHarvestable&&!out.hasJob;out.canFell=!in.plant.isEmpty()&&in.plantIsTree&&!out.hasJob;out.canRemovePlant=!in.plant.isEmpty()&&!in.plantIsTree&&!out.hasJob;
	out.canManage=in.designationID&&(in.designationFlag==TileFlag::TF_WORKSHOP||in.designationFlag==TileFlag::TF_STOCKPILE||in.designationFlag==TileFlag::TF_FARM||in.designationFlag==TileFlag::TF_PASTURE||in.designationFlag==TileFlag::TF_GROVE);
	return out;
}
CreatureInspectorState InspectorQtDataAdapter::creature(const GuiCreatureInfo& i)
{
	CreatureInspectorState out;
	out.id = CreatureId{ i.id };
	out.name = text( i.name );
	out.profession = text( i.profession );
	out.activity = text( i.activity );
	out.strength = i.str;
	out.dexterity = i.dex;
	out.constitution = i.con;
	out.intelligence = i.intel;
	out.wisdom = i.wis;
	out.charisma = i.cha;
	out.hunger = i.hunger;
	out.thirst = i.thirst;
	out.sleep = i.sleep;
	out.happiness = i.happiness;
	out.needsReported = i.needsReported;
	out.inventoryReported = i.inventoryReported;
	for ( const auto& skill : i.skills )
		out.skills.push_back( { text( skill.name ), "level " + std::to_string( skill.level ) + " | " + ( skill.active ? "active" : "inactive" ), 0 } );
	auto slot = [&out]( const char* name, const EquipmentItem& item )
	{
		if( !item.itemID ) return;
		std::string detail = text( item.item );
		if( !item.material.isEmpty() ) detail += " | " + text( item.material );
		out.equipment.push_back( { name, std::move( detail ), 0 } );
	};
	slot( "Head", i.equipment.head );
	slot( "Chest", i.equipment.chest );
	slot( "Arms", i.equipment.arm );
	slot( "Hands", i.equipment.hand );
	slot( "Legs", i.equipment.leg );
	slot( "Feet", i.equipment.foot );
	slot( "Left hand", i.equipment.leftHandHeld );
	slot( "Right hand", i.equipment.rightHandHeld );
	slot( "Back", i.equipment.back );
	for( const auto& item : i.inventory ) out.inventory.push_back( { text( item ), {}, 0 } );
	return out;
}
WorkshopInspectorState InspectorQtDataAdapter::workshop(const GuiWorkshopInfo&i){return {WorkshopId{i.workshopID},text(i.name),text(i.gui),i.priority,i.maxPriority,i.suspended,i.acceptGenerated,i.autoCraftMissing,i.linkStockpile,i.butcherCorpses,i.butcherExcess,i.catchFish,i.processFish,static_cast<std::uint32_t>(i.products.size()),static_cast<std::uint32_t>(i.jobList.size())};}
StockpileInspectorState InspectorQtDataAdapter::stockpile(const GuiStockpileInfo&i){StockpileInspectorState o{StockpileId{i.stockpileID},text(i.name),i.priority,i.maxPriority,i.capacity,i.itemCount,i.reserved,i.suspended,i.pullFromOthers,i.allowPullFromHere,{}};for(const auto&r:i.summary)o.contents.push_back({text(r.itemName),text(r.materialName),static_cast<std::uint32_t>(qMax(0,r.count))});return o;}
AgricultureInspectorState InspectorQtDataAdapter::farm(const GuiFarmInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Farm,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.plantType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.planted=i.planted;o.ready=i.cropReady;o.suspended=i.suspended;o.harvest=i.harvest;return o;}
AgricultureInspectorState InspectorQtDataAdapter::pasture(const GuiPastureInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Pasture,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.animalType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.male=i.numMale;o.female=i.numFemale;o.total=i.total;o.maxMale=i.maxMale;o.maxFemale=i.maxFemale;o.foodCurrent=i.foodCurrent;o.foodMax=i.foodMax;o.hayCurrent=i.hayCurrent;o.hayMax=i.hayMax;o.suspended=i.suspended;o.harvest=i.harvest;o.harvestHay=i.harvestHay;o.tame=i.tame;return o;}
AgricultureInspectorState InspectorQtDataAdapter::grove(const GuiGroveInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Grove,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.treeType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.planted=i.planted;o.ready=i.cropReady;o.suspended=i.suspended;o.pick=i.pickFruits;o.plant=i.plantTrees;o.fell=i.fellTrees;return o;}
} // namespace ingnomia::ui::inspector
