/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AQtDataAdapter.h"

#include "../../../../base/db.h"
#include "../../../../base/dbhelper.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatorworkshop.h"

#include <algorithm>
#include <map>

namespace ingnomia::ui::management6a
{
namespace
{
std::string text( const QString& value )
{
	return value.toUtf8().toStdString();
}
std::string displayFilterLabel( QString value )
{
	value.replace( '_', ' ' );
	value.replace( '-', ' ' );
	for ( int index = value.size() - 1; index > 0; --index )
	{
		const auto previous = value.at( index - 1 );
		const auto current = value.at( index );
		const auto next = index + 1 < value.size() ? value.at( index + 1 ) : QChar {};
		const bool lowerToUpper = previous.isLower() && current.isUpper();
		const bool acronymToWord = previous.isUpper() && current.isUpper() && next.isLower();
		const bool letterToNumber = previous.isLetter() && current.isDigit();
		const bool numberToLetter = previous.isDigit() && current.isLetter();
		if ( ( lowerToUpper || acronymToWord || letterToNumber || numberToLetter ) && previous != ' ' && current != ' ' )
			value.insert( index, ' ' );
	}
	return text( value );
}
CatalogId catalog( const QString& value )
{
	return CatalogId { text( value ) };
}
CraftRepeatMode mode( CraftMode value )
{
	return value == CraftMode::CraftNumber ? CraftRepeatMode::Once : value == CraftMode::CraftTo ? CraftRepeatMode::Maintain
																								 : CraftRepeatMode::Repeat;
}
TriState triState( int on, int total )
{
	return on == 0 ? TriState::Off : on == total ? TriState::On
																		 : TriState::Mixed;
}
QVariantMap findFilterBaseSprite( const QString& spriteID )
{
	if ( spriteID.isEmpty() )
		return {};
	const auto direct = DB::selectRows( "BaseSprites", spriteID );
	if ( !direct.isEmpty() )
		return direct.front();
	const auto materialRows = DB::selectRows( "Sprites_ByMaterialTypes", "ID", spriteID );
	for ( const auto& row : materialRows )
	{
		if ( row.value( "MaterialType" ).toString() != "Wood" )
			continue;
		const auto mapped = DB::selectRows( "BaseSprites", row.value( "Sprite" ).toString() );
		if ( !mapped.isEmpty() )
			return mapped.front();
	}
	for ( const auto& row : materialRows )
	{
		const auto mapped = DB::selectRows( "BaseSprites", row.value( "Sprite" ).toString() );
		if ( !mapped.isEmpty() )
			return mapped.front();
		const auto rotations = DB::selectRows( "Sprites_Rotations", row.value( "Sprite" ).toString() );
		for ( const auto& rotation : rotations )
		{
			const auto rotated = DB::selectRows( "BaseSprites", rotation.value( "BaseSprite" ).toString() );
			if ( !rotated.isEmpty() )
				return rotated.front();
		}
	}
	const auto spriteRows = DB::selectRows( "Sprites", spriteID );
	if ( !spriteRows.isEmpty() )
	{
		const auto base = DB::selectRows( "BaseSprites", spriteRows.front().value( "BaseSprite" ).toString() );
		if ( !base.isEmpty() )
			return base.front();
	}
	return {};
}
StockpileFilterRow::Icon filterIcon( const QString& itemID )
{
	const auto base = findFilterBaseSprite( DBH::spriteID( itemID ) );
	if ( base.isEmpty() )
		return {};
	const auto baseID = base.value( "ID" ).toString();
	if ( baseID.isEmpty() )
		return {};
	const auto rect = base.value( "SourceRectangle" ).toString().split( " ", Qt::SkipEmptyParts );
	if ( rect.size() != 4 )
		return {};
	bool ok = false;
	const auto width = rect[2].toInt( &ok );
	if ( !ok || width <= 0 )
		return {};
	const auto height = rect[3].toInt( &ok );
	if ( !ok || height <= 0 )
		return {};
	const auto sheet = base.value( "Tilesheet" ).toString().toLower();
	const bool hasCroppedTga = sheet == "default.png" || sheet == "furniture.png" || sheet == "workshops.png" || sheet == "terrain.png" || sheet == "plants.png" || sheet == "food_drink_ingredients.png" || sheet == "weapons_armour.png" || sheet == "windmill.png" || sheet == "traps_mechanism.png" || sheet == "automatons.png" || sheet == "gnomes.png" || sheet == "animals.png" || sheet == "mushroom_biome_grass.png" || sheet == "goblin.png" || sheet == "mushrooms.png" || sheet == "multitrees.png" || sheet == "seasonalgrass.png";
	if ( !hasCroppedTga )
		return {};
	// The stockpile list uses normalized alpha-trimmed thumbnails. The regular
	// build_ crops intentionally retain their DB rectangles for other UIs, but
	// those rectangles can contain almost entirely transparent padding for
	// equipment sprites (making the visible art look like a tiny speck).
	return { "filter_" + baseID.toUtf8().toStdString() + ".tga", 24, 24 };
}
} // namespace

WorkshopSnapshot Management6AQtDataAdapter::workshop( const GuiWorkshopInfo& in )
{
	WorkshopSnapshot out;
	out.id               = { in.workshopID };
	out.name             = text( in.name );
	out.subtype          = text( in.gui );
	out.priority         = in.priority;
	out.maxPriority      = in.maxPriority;
	out.suspended        = in.suspended;
	out.acceptGenerated  = in.acceptGenerated;
	out.autoCraftMissing = in.autoCraftMissing;
	out.connectStockpile = in.linkStockpile;
	out.butcherCorpses   = in.butcherCorpses;
	out.butcherExcess    = in.butcherExcess;
	out.catchFish        = in.catchFish;
	out.processFish      = in.processFish;
	for ( const auto& product : in.products )
	{
		WorkshopProductRow row;
		row.id = catalog( product.sid );
		for ( const auto& component : product.components )
		{
			WorkshopComponentRow c;
			c.item                = catalog( component.sid );
			c.amount              = static_cast<std::uint32_t>( std::max( 0, component.amount ) );
			c.requireSameMaterial = component.requireSame;
			for ( const auto& material : component.materials )
				c.materials.push_back( { catalog( material.sid ), static_cast<std::uint32_t>( std::max( 0, material.amount ) ) } );
			row.components.push_back( std::move( c ) );
		}
		out.products.push_back( std::move( row ) );
	}
	for ( const auto& job : in.jobList )
	{
		CraftQueueRow row;
		row.id             = { job.id };
		row.craft          = catalog( job.craftID );
		row.item           = catalog( job.itemSID );
		row.mode           = mode( job.mode );
		row.count          = static_cast<std::uint32_t>( std::max( 0, job.numItemsToCraft ) );
		row.alreadyCrafted = static_cast<std::uint32_t>( std::max( 0, job.alreadyCrafted ) );
		row.suspended      = job.paused;
		row.moveBack       = job.moveToBackWhenDone;
		for ( const auto& required : job.requiredItems )
			row.materials.push_back( catalog( required.materialSID ) );
		out.queue.push_back( std::move( row ) );
	}
	return out;
}
TradeRow Management6AQtDataAdapter::trade( const GuiTradeItem& in, TradeParty party )
{
	return { { party, catalog( in.itemSID ), catalog( in.materialSIDorGender ), in.quality }, text( in.name ), static_cast<std::uint32_t>( std::max( 0, in.count ) ), static_cast<std::uint32_t>( std::max( 0, in.reserved ) ), in.value };
}

StockpileSnapshot Management6AQtDataAdapter::stockpile( const GuiStockpileInfo& in )
{
	StockpileSnapshot out;
	out.id                = { in.stockpileID };
	out.name              = text( in.name );
	out.priority          = in.priority;
	out.maxPriority       = in.maxPriority;
	out.suspended         = in.suspended;
	out.pullFromOthers    = in.pullFromOthers;
	out.allowPullFromHere = in.allowPullFromHere;
	out.capacity          = in.capacity;
	out.itemCount         = in.itemCount;
	out.reserved          = in.reserved;
	auto filter           = in.filter;
	static std::map<std::string, StockpileFilterRow::Icon> itemIcons;
	const auto iconFor = [&]( const QString& item ) -> const StockpileFilterRow::Icon&
	{
		const auto key = text( item );
		if ( const auto found = itemIcons.find( key ); found != itemIcons.end() )
			return found->second;
		return itemIcons.emplace( key, filterIcon( item ) ).first->second;
	};
	for ( const auto& category : filter.categories() )
	{
		int categoryOn = 0, categoryTotal = 0;
		std::vector<StockpileFilterRow> descendants;
		for ( const auto& group : filter.groups( category ) )
		{
			int groupOn = 0, groupTotal = 0;
			std::vector<StockpileFilterRow> groupRows;
			for ( const auto& item : filter.items( category, group ) )
			{
				int itemOn = 0, itemTotal = 0;
				const auto& itemIcon = iconFor( item );
				std::vector<StockpileFilterRow> materialRows;
				for ( const auto& material : filter.materials( category, group, item ) )
				{
					const bool active = filter.getCheckState( category, group, item, material );
					itemOn += active ? 1 : 0;
					++itemTotal;
					materialRows.push_back( { { out.id, catalog( category ), catalog( group ), catalog( item ), catalog( material ), FilterDepth::Material }, displayFilterLabel( material ), active ? TriState::On : TriState::Off, itemIcon } );
				}
				groupOn += itemOn;
				groupTotal += itemTotal;
				groupRows.push_back( { { out.id, catalog( category ), catalog( group ), catalog( item ), {}, FilterDepth::Item }, displayFilterLabel( item ), triState( itemOn, itemTotal ), itemIcon } );
				groupRows.insert( groupRows.end(), materialRows.begin(), materialRows.end() );
			}
			categoryOn += groupOn;
			categoryTotal += groupTotal;
			descendants.push_back( { { out.id, catalog( category ), catalog( group ), {}, {}, FilterDepth::Group }, displayFilterLabel( group ), triState( groupOn, groupTotal ) } );
			descendants.insert( descendants.end(), groupRows.begin(), groupRows.end() );
		}
		out.filters.push_back( { { out.id, catalog( category ), {}, {}, {}, FilterDepth::Category }, displayFilterLabel( category ), triState( categoryOn, categoryTotal ) } );
		out.filters.insert( out.filters.end(), descendants.begin(), descendants.end() );
	}
	const auto& active = filter.getActive();
	// Contents come from physical stockpile fields. Keep blocked rows in the
	// projection; the allow-list is policy, not a destructive inventory view.
	for ( const auto& value : in.summary )
		if ( value.count > 0 )
			out.contents.push_back( { { catalog( value.itemSID ), catalog( value.materialSID ) }, text( value.itemName ), text( value.materialName ), static_cast<std::uint32_t>( value.count ), active.contains( { value.itemSID, value.materialSID } ) } );
	return out;
}

AgricultureSnapshot Management6AQtDataAdapter::farm( const GuiFarmInfo& in )
{
	AgricultureSnapshot out;
	out.target      = { AgricultureKind::Farm, { in.ID } };
	out.name        = text( in.name );
	out.product     = catalog( in.plantType );
	out.priority    = in.priority;
	out.maxPriority = in.maxPriority;
	out.plots       = in.numPlots;
	out.tilled      = in.tilled;
	out.planted     = in.planted;
	out.ready       = in.cropReady;
	out.suspended   = in.suspended;
	out.harvest     = in.harvest;
	return out;
}
AgricultureSnapshot Management6AQtDataAdapter::pasture( const GuiPastureInfo& in )
{
	AgricultureSnapshot out;
	out.target      = { AgricultureKind::Pasture, { in.ID } };
	out.name        = text( in.name );
	out.product     = catalog( in.animalType );
	out.priority    = in.priority;
	out.maxPriority = in.maxPriority;
	out.plots       = in.numPlots;
	out.male        = in.numMale;
	out.female      = in.numFemale;
	out.total       = in.total;
	out.capacity    = in.maxNumber;
	out.maxMale     = in.maxMale;
	out.maxFemale   = in.maxFemale;
	out.foodCurrent = in.foodCurrent;
	out.foodMax     = in.foodMax;
	out.hayCurrent  = in.hayCurrent;
	out.hayMax      = in.hayMax;
	out.suspended   = in.suspended;
	out.harvest     = in.harvest;
	out.harvestHay  = in.harvestHay;
	out.tame        = in.tame;
	for ( const auto& animal : in.animals )
		out.animals.push_back( { { animal.id }, text( animal.name ), catalog( animal.animalID ), animal.gender == ::Gender::MALE ? Gender::Male : Gender::Female, animal.isYoung, animal.toButcher } );
	for ( const auto& food : in.food )
		out.foods.push_back( { catalog( food.itemSID ), catalog( food.materialSID ), text( food.name ), food.checked } );
	return out;
}
AgricultureSnapshot Management6AQtDataAdapter::grove( const GuiGroveInfo& in )
{
	AgricultureSnapshot out;
	out.target      = { AgricultureKind::Grove, { in.ID } };
	out.name        = text( in.name );
	out.product     = catalog( in.treeType );
	out.priority    = in.priority;
	out.maxPriority = in.maxPriority;
	out.plots       = in.numPlots;
	out.planted     = in.planted;
	out.ready       = in.cropReady;
	out.suspended   = in.suspended;
	out.pick        = in.pickFruits;
	out.plant       = in.plantTrees;
	out.fell        = in.fellTrees;
	return out;
}
std::vector<AgricultureCatalogRow> Management6AQtDataAdapter::plants( const QList<GuiPlant>& in )
{
	std::vector<AgricultureCatalogRow> out;
	for ( const auto& v : in )
		out.push_back( { catalog( v.plantID ), text( v.name ), static_cast<std::uint32_t>( std::max( 0, v.seedCount ) ), static_cast<std::uint32_t>( std::max( 0, v.plantCount ) ), static_cast<std::uint32_t>( std::max( 0, v.itemCount ) ) } );
	return out;
}
std::vector<AgricultureCatalogRow> Management6AQtDataAdapter::animals( const QList<GuiAnimal>& in )
{
	std::vector<AgricultureCatalogRow> out;
	for ( const auto& v : in )
		out.push_back( { catalog( v.animalID ), text( v.name ), static_cast<std::uint32_t>( std::max( 0, v.totalcount ) ), 0, 0 } );
	return out;
}
} // namespace ingnomia::ui::management6a
