/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AQtDataAdapter.h"

#include "../../../../base/db.h"
#include "../../../../base/dbhelper.h"
#include "../../../../base/config.h"
#include "../../../../base/global.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatorinventory.h"
#include "../../../strings.h"
#include "../../../aggregatorworkshop.h"

#include <algorithm>
#include <QFileInfo>
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
	out.canLinkStockpile = in.canLinkStockpile;
	for(const auto& row : in.stockpiles) out.stockpiles.push_back({StockpileId{row.id},text(row.name),row.linked});
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
	const auto iconFor = []( const QString& item, const QString& material = QString {} ) -> StockpileFilterRow::Icon
	{
		return { text( AggregatorInventory::inventoryIcon( item, material ) ), 40, 40 };
	};
	const auto alphabetized = []( auto values )
	{
		std::stable_sort( values.begin(), values.end(), []( const QString& a, const QString& b )
			{
				const auto displayA = QString::fromUtf8( displayFilterLabel( a ).c_str() );
				const auto displayB = QString::fromUtf8( displayFilterLabel( b ).c_str() );
				const auto foldedOrder = QString::compare( displayA, displayB, Qt::CaseInsensitive );
				return foldedOrder == 0 ? a < b : foldedOrder < 0;
			} );
		return values;
	};
	for ( const auto& category : alphabetized( filter.categories() ) )
	{
		int categoryOn = 0, categoryTotal = 0;
		std::vector<StockpileFilterRow> descendants;
		for ( const auto& group : alphabetized( filter.groups( category ) ) )
		{
			int groupOn = 0, groupTotal = 0;
			std::vector<StockpileFilterRow> groupRows;
			for ( const auto& item : alphabetized( filter.items( category, group ) ) )
			{
				int itemOn = 0, itemTotal = 0;
				const auto& itemIcon = iconFor( item );
				std::vector<StockpileFilterRow> materialRows;
				for ( const auto& material : alphabetized( filter.materials( category, group, item ) ) )
				{
					const bool active = filter.getCheckState( category, group, item, material );
					itemOn += active ? 1 : 0;
					++itemTotal;
					materialRows.push_back( { { out.id, catalog( category ), catalog( group ), catalog( item ), catalog( material ), FilterDepth::Material }, text( S::s( "$MaterialName_" + material ) ), active ? TriState::On : TriState::Off, iconFor( item, material ) } );
				}
				groupOn += itemOn;
				groupTotal += itemTotal;
				groupRows.push_back( { { out.id, catalog( category ), catalog( group ), catalog( item ), {}, FilterDepth::Item }, text( S::s( "$ItemName_" + item ) ), triState( itemOn, itemTotal ), itemIcon } );
				groupRows.insert( groupRows.end(), materialRows.begin(), materialRows.end() );
			}
			categoryOn += groupOn;
			categoryTotal += groupTotal;
			descendants.push_back( { { out.id, catalog( category ), catalog( group ), {}, {}, FilterDepth::Group }, text( S::s( "$GroupName_" + group ) ), triState( groupOn, groupTotal ) } );
			descendants.insert( descendants.end(), groupRows.begin(), groupRows.end() );
		}
		out.filters.push_back( { { out.id, catalog( category ), {}, {}, {}, FilterDepth::Category }, text( S::s( "$CategoryName_" + category ) ), triState( categoryOn, categoryTotal ) } );
		out.filters.insert( out.filters.end(), descendants.begin(), descendants.end() );
	}
	std::map<std::pair<std::string, std::string>, const ItemsSummary*> stored;
	for ( const auto& value : in.summary )
		if ( value.count > 0 ) stored[{ text( value.itemSID ), text( value.materialSID ) }] = &value;
	std::vector<StockpileContentRow> leaves;
	for ( const auto& filterRow : out.filters )
	{
		if ( filterRow.id.depth != FilterDepth::Material ) continue;
		const auto found = stored.find( { filterRow.id.item.value, filterRow.id.material.value } );
		if ( found == stored.end() ) continue;
		const auto& value = *found->second;
		leaves.push_back( { { filterRow.id.category, filterRow.id.group, filterRow.id.item, filterRow.id.material, FilterDepth::Material }, displayFilterLabel( value.materialName ), static_cast<std::uint32_t>( value.count ), static_cast<std::uint32_t>( std::max( 0, value.total ) ), filterRow.icon } );
	}
	const auto descendant = []( const StockpileContentRow& leaf, const StockpileFilterRow& parent )
	{
		if ( leaf.id.category != parent.id.category ) return false;
		if ( parent.id.depth >= FilterDepth::Group && leaf.id.group != parent.id.group ) return false;
		if ( parent.id.depth >= FilterDepth::Item && leaf.id.item != parent.id.item ) return false;
		return parent.id.depth != FilterDepth::Material || leaf.id.material == parent.id.material;
	};
	for ( const auto& filterRow : out.filters )
	{
		if ( filterRow.id.depth == FilterDepth::Material )
		{
			const auto leaf = std::find_if( leaves.begin(), leaves.end(), [&]( const auto& row ) { return row.id.category == filterRow.id.category && row.id.group == filterRow.id.group && row.id.item == filterRow.id.item && row.id.material == filterRow.id.material; } );
			if ( leaf != leaves.end() ) out.contents.push_back( *leaf );
			continue;
		}
		std::uint32_t stockpiled = 0, total = 0;
		for ( const auto& leaf : leaves )
			if ( descendant( leaf, filterRow ) ) { stockpiled += leaf.stockpiled; total += leaf.total; }
		if ( stockpiled > 0 )
			out.contents.push_back( { { filterRow.id.category, filterRow.id.group, filterRow.id.item, {}, filterRow.id.depth }, filterRow.label, stockpiled, total, filterRow.icon } );
	}
	for ( const auto& name : in.templateNames ) out.templateNames.push_back( text( name ) );
	return out;
}

AgricultureSnapshot Management6AQtDataAdapter::farm( const GuiFarmInfo& in )
{
	AgricultureSnapshot out;
	out.target      = { AgricultureKind::Farm, { in.ID } };
	out.name        = text( in.name );
	out.product     = catalog( in.plantType );
	out.productName = text( in.product.name );
	out.productSeeds = in.product.seedCount;
	out.productItems = in.product.itemCount;
	out.productPlants = in.product.plantCount;
	out.priority    = in.priority;
	out.maxPriority = in.maxPriority;
	out.plots       = in.numPlots;
	out.tilled      = in.tilled;
	out.planted     = in.planted;
	out.ready       = in.cropReady;
	out.suspended   = in.suspended;
	out.harvest     = in.harvest;
	for ( const auto& field : in.fields )
	{
		FarmPlotRow row;
		row.position = { field.x, field.y, field.z };
		row.assignedCrop = catalog( field.assignedCrop );
		row.plantedCrop = catalog( field.plantedCrop );
		row.tilled = field.tilled; row.planted = field.planted; row.ready = field.ready; row.busy = field.busy;
		for ( const auto& order : field.orders ) row.orders.push_back( { order.id, catalog( order.crop ), order.remaining, order.repeat } );
		out.fields.push_back( std::move( row ) );
	}
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
	{
		QString icon;
		if ( Global::cfg )
		{
			const QString sheetRoot = Global::cfg->get( "dataPath" ).toString() + "/tilesheet/";
			const QString plantIcon = "filter_" + v.plantID + ".tga";
			const QString seedIcon = "catalog_" + v.seedID + "__" + v.materialID + ".tga";
			if ( QFileInfo::exists( sheetRoot + plantIcon ) ) icon = plantIcon;
			else if ( QFileInfo::exists( sheetRoot + seedIcon ) ) icon = seedIcon;
		}
		out.push_back( { catalog( v.plantID ), text( v.name ), static_cast<std::uint32_t>( std::max( 0, v.seedCount ) ), static_cast<std::uint32_t>( std::max( 0, v.plantCount ) ), static_cast<std::uint32_t>( std::max( 0, v.itemCount ) ), text( icon ) } );
	}
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
