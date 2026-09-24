/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorQtDataAdapter.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorcreatureinfo.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatortileinfo.h"
#include "../../../aggregatorworkshop.h"
#include "../../../../base/global.h"
#include "../../../../base/db.h"
#include "../../../../base/dbhelper.h"

#include <algorithm>

namespace ingnomia::ui::inspector
{
namespace
{
std::string text( const QString& value ) { return value.toUtf8().toStdString(); }
CatalogId catalog( const QString& value ) { return CatalogId{ text( value ) }; }

QVariantMap equipmentBaseSprite( const QString& item )
{
	const auto spriteID = DBH::spriteID( item );
	if( spriteID.isEmpty() ) return {};
	const auto direct = DB::selectRows( "BaseSprites", spriteID );
	if( !direct.isEmpty() ) return direct.front();
	for( const auto& row : DB::selectRows( "Sprites_ByMaterialTypes", "ID", spriteID ) )
	{
		const auto mapped = DB::selectRows( "BaseSprites", row.value( "Sprite" ).toString() );
		if( !mapped.isEmpty() ) return mapped.front();
	}
	const auto spriteRows = DB::selectRows( "Sprites", spriteID );
	if( !spriteRows.isEmpty() )
	{
		const auto mapped = DB::selectRows( "BaseSprites", spriteRows.front().value( "BaseSprite" ).toString() );
		if( !mapped.isEmpty() ) return mapped.front();
	}
	return {};
}

std::string equipmentIcon( const EquipmentItem& item )
{
	if( !item.itemID || item.item.isEmpty() ) return {};
	const auto base = equipmentBaseSprite( item.item );
	const auto baseID = base.value( "ID" ).toString();
	return baseID.isEmpty() ? std::string{} : "../tilesheet/inventory_" + text( baseID ) + ".tga";
}

std::string itemIcon( const QString& item )
{
	if( item.isEmpty() ) return {};
	const auto base = equipmentBaseSprite( item );
	const auto baseID = base.value( "ID" ).toString();
	return baseID.isEmpty() ? std::string{} : "../tilesheet/inventory_" + text( baseID ) + ".tga";
}

std::vector<EquipmentTypeChoice> equipmentChoices( const QString& slot )
{
	std::vector<EquipmentTypeChoice> result;
	result.push_back( { CatalogId{ "none" }, { CatalogId{ "any" } } } );
	for( const auto& row : DB::selectRows( "Uniform_Slots", slot ) )
	{
		const auto type = row.value( "Type" ).toString();
		if( type.isEmpty() || std::any_of( result.begin(), result.end(), [&type]( const EquipmentTypeChoice& value ) { return value.type.value == text( type ); } ) ) continue;
		EquipmentTypeChoice choice;
		choice.type = catalog( type );
		choice.materials.push_back( CatalogId{ "any" } );
		const auto materialTypes = DB::select2( "MaterialType", "Uniform_Slots", "Type", type );
		if( !materialTypes.isEmpty() )
			for( const auto& material : DB::select2( "ID", "Materials", "Type", materialTypes.front().toString() ) )
				choice.materials.push_back( catalog( material.toString() ) );
		result.push_back( std::move( choice ) );
	}
	return result;
}
}
std::optional<WorldPosition> InspectorQtDataAdapter::position(const QString& value){const auto p=value.split(' ',Qt::SkipEmptyParts);if(p.size()!=3)return{};bool a,b,c;const int x=p[0].toInt(&a),y=p[1].toInt(&b),z=p[2].toInt(&c);if(!a||!b||!c)return{};return WorldPosition{x,y,z};}
TileInspectorState InspectorQtDataAdapter::tile(const GuiTileInfo& in)
{
	TileInspectorState out;out.id=TileId{in.tileID};const Position p(in.tileID);out.position={p.x,p.y,p.z};out.wall=text(in.wall);out.floor=text(in.floor);out.embedded=text(in.embedded);out.plant=text(in.plant);out.water=text(in.water);out.construction=text(in.constructed);
	for(const auto&i:in.items)out.items.push_back({text(i.text),text(i.material),i.count});
	for(const auto&c:in.creatures)out.creatures.push_back({CreatureId{c.id},text(c.text),EntityKind::Creature});
	out.jobName=text(in.jobName);out.jobWorker=text(in.jobWorker);out.jobPriority=text(in.jobPriority);out.requiredSkill=text(in.requiredSkill);out.requiredTool=text(in.requiredTool);out.requiredToolAvailable=text(in.requiredToolAvailable);out.workPositions=text(in.workPositions);for(const auto&i:in.requiredItems)out.requiredItems.push_back({text(i.text),text(i.material),i.count,i.available});out.hasJob=!in.jobName.isEmpty();out.canRaisePriority=in.canRaisePriority;out.canLowerPriority=in.canLowerPriority;
	if(in.designationID)out.designation=DesignationId{in.designationID};out.designationName=text(in.designationName);
	if(in.designationFlag==TileFlag::TF_ROOM)out.roomSummary="Beds "+text(in.beds)+(in.isEnclosed?" | enclosed":" | open")+(in.hasRoof?" | roofed":" | no roof");
	if(in.mechInfo.itemID)out.mechanismSummary=text(in.mechInfo.name)+" | "+(in.mechInfo.active?"active":"inactive")+(in.mechInfo.isInvertable?(in.mechInfo.inverted?" | inverted":" | normal"):"");
	out.plantIsTree=in.plantIsTree;out.plantIsHarvestable=in.plantIsHarvestable;out.canMine=!in.wall.isEmpty()&&!out.hasJob;out.canRemoveFloor=!in.floor.isEmpty()&&in.wall.isEmpty()&&in.plant.isEmpty()&&!out.hasJob;out.canHarvest=!in.plant.isEmpty()&&in.plantIsHarvestable&&!out.hasJob;out.canFell=!in.plant.isEmpty()&&in.plantIsTree&&!out.hasJob;out.canRemovePlant=!in.plant.isEmpty()&&!in.plantIsTree&&!out.hasJob;
	out.canDeleteStockpile=in.designationID && in.designationFlag==TileFlag::TF_STOCKPILE;
	out.canManage=in.designationID&&(in.designationFlag==TileFlag::TF_WORKSHOP||in.designationFlag==TileFlag::TF_STOCKPILE||in.designationFlag==TileFlag::TF_FARM||in.designationFlag==TileFlag::TF_PASTURE||in.designationFlag==TileFlag::TF_GROVE);
	return out;
}
CreatureInspectorState InspectorQtDataAdapter::creature(const GuiCreatureInfo& i)
{
	CreatureInspectorState out;
	out.id = CreatureId{ i.id };
	out.name = text( i.name );
	out.profession = text( i.profession );
	out.professionReported = i.professionReported;
	out.activity = text( i.activity );
	out.strength = i.str;
	out.dexterity = i.dex;
	out.constitution = i.con;
	out.intelligence = i.intel;
	out.wisdom = i.wis;
	out.charisma = i.cha;
	out.attributesReported = i.attributesReported;
	out.hunger = i.hunger;
	out.thirst = i.thirst;
	out.sleep = i.sleep;
	out.happiness = i.happiness;
	out.needsReported = i.needsReported;
	out.inventoryReported = i.inventoryReported;
	for ( const auto& skill : i.skills )
		out.skills.push_back( { text( skill.name ), "level " + std::to_string( skill.level ) + " | " + ( skill.active ? "active" : "inactive" ), 0 } );
	out.skillsReported = i.skillsReported;
	out.equipmentReported = i.equipmentReported;
	out.equipmentRole = MilitaryRoleId{ i.roleID };
	out.equipmentRoleName = text( i.roleName );
	auto slot = [&out, &i]( UniformSlot id, const char* key, const char* name, const EquipmentItem& item )
	{
		if( item.itemID )
		{
			std::string detail = text( item.item );
			if( !item.material.isEmpty() ) detail += " | " + text( item.material );
			out.equipment.push_back( { name, std::move( detail ), 0 } );
		}
		EquipmentSlotState value;
		value.slot = id;
		value.label = name;
		value.item = text( item.item );
		value.material = text( item.material );
		value.icon = equipmentIcon( item );
		const auto desired = i.uniform.parts.value( QString::fromLatin1( key ) );
		value.desiredType = catalog( desired.type.isEmpty() ? QStringLiteral( "none" ) : desired.type );
		value.desiredMaterial = catalog( desired.material.isEmpty() ? QStringLiteral( "any" ) : desired.material );
		value.choices = equipmentChoices( QString::fromLatin1( key ) );
		out.equipmentSlots.push_back( std::move( value ) );
	};
	slot( UniformSlot::HeadArmor, "HeadArmor", "Head", i.equipment.head );
	slot( UniformSlot::ChestArmor, "ChestArmor", "Chest", i.equipment.chest );
	slot( UniformSlot::ArmArmor, "ArmArmor", "Arms", i.equipment.arm );
	slot( UniformSlot::HandArmor, "HandArmor", "Hands", i.equipment.hand );
	slot( UniformSlot::LegArmor, "LegArmor", "Legs", i.equipment.leg );
	slot( UniformSlot::FootArmor, "FootArmor", "Feet", i.equipment.foot );
	slot( UniformSlot::LeftHandHeld, "LeftHandHeld", "Left hand", i.equipment.leftHandHeld );
	slot( UniformSlot::RightHandHeld, "RightHandHeld", "Right hand", i.equipment.rightHandHeld );
	slot( UniformSlot::Back, "Back", "Back", i.equipment.back );
	for( const auto& item : i.inventory ) out.inventory.push_back( { text( item ), {}, 0 } );
	return out;
}
WorkshopInspectorState InspectorQtDataAdapter::workshop(const GuiWorkshopInfo&i){return {WorkshopId{i.workshopID},text(i.name),text(i.gui),i.priority,i.maxPriority,i.suspended,i.acceptGenerated,i.autoCraftMissing,i.linkStockpile,i.butcherCorpses,i.butcherExcess,i.catchFish,i.processFish,static_cast<std::uint32_t>(i.products.size()),static_cast<std::uint32_t>(i.jobList.size())};}
StockpileInspectorState InspectorQtDataAdapter::stockpile(const GuiStockpileInfo&i){StockpileInspectorState o{StockpileId{i.stockpileID},text(i.name),i.priority,i.maxPriority,i.capacity,i.itemCount,i.reserved,i.suspended,i.pullFromOthers,i.allowPullFromHere,{}};for(const auto&r:i.summary)o.contents.push_back({text(r.itemName),text(r.materialName),static_cast<std::uint32_t>(qMax(0,r.count)),false,itemIcon(r.itemSID)});return o;}
AgricultureInspectorState InspectorQtDataAdapter::farm(const GuiFarmInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Farm,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.plantType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.planted=i.planted;o.ready=i.cropReady;o.suspended=i.suspended;o.harvest=i.harvest;return o;}
AgricultureInspectorState InspectorQtDataAdapter::pasture(const GuiPastureInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Pasture,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.animalType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.male=i.numMale;o.female=i.numFemale;o.total=i.total;o.maxMale=i.maxMale;o.maxFemale=i.maxFemale;o.foodCurrent=i.foodCurrent;o.foodMax=i.foodMax;o.hayCurrent=i.hayCurrent;o.hayMax=i.hayMax;o.suspended=i.suspended;o.harvest=i.harvest;o.harvestHay=i.harvestHay;o.tame=i.tame;return o;}
AgricultureInspectorState InspectorQtDataAdapter::grove(const GuiGroveInfo&i){AgricultureInspectorState o;o.target={AgricultureKind::Grove,DesignationId{i.ID}};o.name=text(i.name);o.product=text(i.treeType);o.priority=i.priority;o.maxPriority=i.maxPriority;o.plots=i.numPlots;o.planted=i.planted;o.ready=i.cropReady;o.suspended=i.suspended;o.pick=i.pickFruits;o.plant=i.plantTrees;o.fell=i.fellTrees;return o;}
} // namespace ingnomia::ui::inspector
