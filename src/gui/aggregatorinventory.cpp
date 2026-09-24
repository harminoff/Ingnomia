/*
	This file is part of Ingnomia https://github.com/rschurade/Ingnomia
    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/** @file aggregatorinventory.cpp
 *  @brief AggregatorInventory implementation: builds the inventory category tree, the build
 *         menu catalogue with preview icons, and the top-bar watch list. Also listens for
 *         item add/remove events so watched rows update live.
 */
#include "aggregatorinventory.h"

#include "../base/config.h"
#include "../base/db.h"
#include "../base/dbhelper.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/util.h"
#include "../game/game.h"
#include "../game/tutorialmanager.h"
#include "../game/inventory.h"
#include "../game/itemhistory.h"
#include "../game/stockpilemanager.h"
#include "../gui/strings.h"
#include <QFileInfo>


namespace
{
// A number of inventory items are represented in the game renderer by a
// composed sprite (or have a null BaseSprite row).  The inventory list needs
// one stable thumbnail, so use an authoritative component from the same DB
// tilesheet rather than leaving the cell blank.
const QHash<QString, QString> inventoryThumbnailFallbacks {
	{ "AlarmBell", "AlarmBellBase" },
	{ "AnimalCorpse", "Meat" },
	{ "Automaton", "AutomatonTorsoFR" },
	{ "BallPeenHammer", "HammerHead" },
	{ "Bed", "StrawBed" },
	{ "Bellows", "BellowsFL" },
	{ "Berries", "Blackberry" },
	{ "BigTorch", "BigTorchBase" },
	{ "Bookshelf", "BookshelfFR" },
	{ "Brazier", "BrazierBase" },
	{ "Corpse", "Meat" },
	{ "FancyBed", "FancyWoodBedFrameFR" },
	{ "FellingAxe", "FellingAxeHead" },
	{ "File", "KnifeBlade" },
	{ "Fruit", "Strawberry" },
	{ "GearBox", "GearBoxBaseItem" },
	{ "GoblinCorpse", "GoblinTorso" },
	{ "Hammer", "HammerHead" },
	{ "Knife", "KnifeBlade" },
	{ "Leaves", "PineTree" },
	{ "Tree", "PineTree" },
	{ "Lever", "LeverOffFR" },
	{ "Painting", "Painting1" },
	{ "Pickaxe", "PickaxeHead" },
	{ "Pump", "PumpBase" },
	{ "SteamEngine", "SteamEngineBoilerFR" },
	{ "Sword", "SwordBlade" },
	{ "Torch", "GroundTorchBase" },
	{ "Vegetable", "Carrot" },
	{ "VerticalAxle", "Axle" },
	{ "WallTorch", "WallTorchBaseFR" },
	{ "Warhammer", "WarhammerHead" }
};

QVariantMap findBaseSprite( const QString& spriteID )
{

	if ( spriteID.isEmpty() ) return {};
	const auto direct = DB::selectRows( "BaseSprites", spriteID );
	if ( !direct.isEmpty() ) return direct.front();

	// Materialised sprites (Chair, Table, etc.) resolve through the default
	// Wood mapping in the same way SpriteFactory resolves them in-game.
	const auto materialRows = DB::selectRows( "Sprites_ByMaterialTypes", "ID", spriteID );
	for ( const auto& row : materialRows )
	{
		if ( row.value( "MaterialType" ).toString() != "Wood" ) continue;
		const auto mapped = DB::selectRows( "BaseSprites", row.value( "Sprite" ).toString() );
		if ( !mapped.isEmpty() ) return mapped.front();
	}
	for ( const auto& row : materialRows )
	{
		const auto mapped = DB::selectRows( "BaseSprites", row.value( "Sprite" ).toString() );
		if ( !mapped.isEmpty() ) return mapped.front();
		const auto rotations = DB::selectRows( "Sprites_Rotations", row.value( "Sprite" ).toString() );
		for ( const auto& rotation : rotations )
		{
			const auto rotated = DB::selectRows( "BaseSprites", rotation.value( "BaseSprite" ).toString() );
			if ( !rotated.isEmpty() ) return rotated.front();
		}
	}

	const auto spriteRows = DB::selectRows( "Sprites", spriteID );
	if ( !spriteRows.isEmpty() )
	{
		const auto base = DB::selectRows( "BaseSprites", spriteRows.front().value( "BaseSprite" ).toString() );
		if ( !base.isEmpty() ) return base.front();
		const auto rotations = DB::selectRows( "Sprites_Rotations", spriteID );
		for ( const auto& rotation : rotations )
		{
			const auto rotated = DB::selectRows( "BaseSprites", rotation.value( "BaseSprite" ).toString() );
			if ( !rotated.isEmpty() ) return rotated.front();
		}
	}
	for ( const auto& row : DB::selectRows( "Sprites_Combine", "ID", spriteID ) )
	{
		const auto base = DB::selectRows( "BaseSprites", row.value( "BaseSprite" ).toString() );
		if ( !base.isEmpty() ) return base.front();
		const auto nested = row.value( "Sprite" ).toString();
		if ( !nested.isEmpty() )
		{
			const auto resolved = findBaseSprite( nested );
			if ( !resolved.isEmpty() ) return resolved;
		}
	}
	const auto fallback = inventoryThumbnailFallbacks.value( spriteID );
	if ( !fallback.isEmpty() )
	{
		const auto mapped = DB::selectRows( "BaseSprites", fallback );
		if ( !mapped.isEmpty() ) return mapped.front();
	}
	return {};
}

QVariantMap findMaterialBaseSprite( const QString& spriteID, const QString& materialID )
{
	if ( spriteID.isEmpty() || materialID.isEmpty() ) return findBaseSprite( spriteID );
	for ( const auto& row : DB::selectRows( "Sprites_ByMaterials", "ID", spriteID ) )
	{
		if ( row.value( "MaterialID" ).toString() != materialID ) continue;
		const auto base = DB::selectRows( "BaseSprites", row.value( "BaseSprite" ).toString() );
		if ( !base.isEmpty() ) return base.front();
		const auto nested = row.value( "Sprite" ).toString();
		if ( !nested.isEmpty() ) return findBaseSprite( nested );
	}
	const auto materialType = DB::select( "Type", "Materials", materialID ).toString();
	for ( const auto& row : DB::selectRows( "Sprites_ByMaterialTypes", "ID", spriteID ) )
	{
		if ( row.value( "MaterialType" ).toString() != materialType ) continue;
		const auto base = DB::selectRows( "BaseSprites", row.value( "BaseSprite" ).toString() );
		if ( !base.isEmpty() ) return base.front();
		const auto nested = row.value( "Sprite" ).toString();
		if ( !nested.isEmpty() ) return findBaseSprite( nested );
	}
	return findBaseSprite( spriteID );
}
QPair<int, int> sheetDimensions( const QString& sheet )
{
	const auto name = sheet.toLower();
	if ( name == "furniture.png" ) return { 512, 576 };
	if ( name == "workshops.png" ) return { 416, 540 };
	if ( name == "terrain.png" || name == "default.png" ) return { 1024, 1152 };
	if ( name == "multitrees.png" ) return { 640, 576 };
	if ( name == "plants.png" ) return { 1024, 1152 };
	if ( name == "seasonalgrass.png" ) return { 416, 1116 };
	if ( name == "animals.png" ) return { 512, 576 };
	if ( name == "automatons.png" ) return { 256, 108 };
	if ( name == "goblin.png" ) return { 480, 288 };
	if ( name == "mushrooms.png" ) return { 1056, 108 };
	if ( name == "mushroom_biome_grass.png" ) return { 256, 288 };
	if ( name == "windmill.png" ) return { 256, 324 };
	if ( name == "food_drink_ingredients.png" ) return { 384, 144 };
	if ( name == "weapons_armour.png" ) return { 384, 288 };
	if ( name == "traps_mechanism.png" ) return { 576, 432 };
	if ( name == "gnomes.png" ) return { 768, 540 };
	return {};
}
bool assignInventorySprite( QString& spriteSheet, int& spriteX, int& spriteY, int& spriteWidth, int& spriteHeight, int& spriteSheetWidth, int& spriteSheetHeight, const QVariantMap& base )
{
	if ( base.isEmpty() ) return false;
	const auto sheet = base.value( "Tilesheet" ).toString().toLower();
	const auto rect = base.value( "SourceRectangle" ).toString().split( " ", Qt::SkipEmptyParts );
	if ( rect.size() != 4 ) return false;
	bool ok = false;
	const int x = rect[0].toInt( &ok ); if ( !ok || x < 0 ) return false;
	const int y = rect[1].toInt( &ok ); if ( !ok || y < 0 ) return false;
	const int width = rect[2].toInt( &ok ); if ( !ok || width <= 0 ) return false;
	const int height = rect[3].toInt( &ok ); if ( !ok || height <= 0 ) return false;
	const auto dimensions = sheetDimensions( sheet );
	if ( dimensions.first <= 0 || dimensions.second <= 0 || x + width > dimensions.first || y + height > dimensions.second ) return false;
	const bool hasCroppedTga = sheet == "default.png" || sheet == "furniture.png" || sheet == "workshops.png" || sheet == "terrain.png" || sheet == "plants.png" || sheet == "food_drink_ingredients.png" || sheet == "weapons_armour.png" || sheet == "windmill.png" || sheet == "traps_mechanism.png" || sheet == "automatons.png" || sheet == "gnomes.png" || sheet == "animals.png" || sheet == "mushroom_biome_grass.png" || sheet == "goblin.png" || sheet == "mushrooms.png" || sheet == "multitrees.png" || sheet == "seasonalgrass.png";
	if ( !hasCroppedTga ) return false;
	constexpr int inventoryThumbnailSize = 40;
	spriteSheet = "inventory_" + base.value( "ID" ).toString() + ".tga";
	spriteX = 0;
	spriteY = 0;
	spriteWidth = inventoryThumbnailSize;
	spriteHeight = inventoryThumbnailSize;
	spriteSheetWidth = inventoryThumbnailSize;
	spriteSheetHeight = inventoryThumbnailSize;
	return true;
}
}

/// @brief Constructs the AggregatorInventory and seeds the BuildSelection → string/BuildItemType
///        lookup maps used to route build-menu requests.
/// @param parent Qt parent object.
AggregatorInventory::AggregatorInventory( QObject* parent ) :
	QObject( parent )
{
	qRegisterMetaType<GuiInventoryHistoryPoint>();
	qRegisterMetaType<QList<GuiInventoryHistoryPoint>>();

	m_buildSelection2String.insert( BuildSelection::Workshop, "Workshop" );
	m_buildSelection2String.insert( BuildSelection::Wall, "Wall" );
	m_buildSelection2String.insert( BuildSelection::Floor, "Floor" );
	m_buildSelection2String.insert( BuildSelection::Stairs, "Stairs" );
	m_buildSelection2String.insert( BuildSelection::Ramps, "Ramp" );
	m_buildSelection2String.insert( BuildSelection::Containers, "Containers" );
	m_buildSelection2String.insert( BuildSelection::Fence, "Fence" );
	m_buildSelection2String.insert( BuildSelection::Furniture, "Furniture" );
	m_buildSelection2String.insert( BuildSelection::Utility, "Utility" );

	m_buildSelection2buildItem.insert( BuildSelection::Workshop, BuildItemType::Workshop );

	m_buildSelection2buildItem.insert( BuildSelection::Wall, BuildItemType::Terrain );
	m_buildSelection2buildItem.insert( BuildSelection::Floor, BuildItemType::Terrain );
	m_buildSelection2buildItem.insert( BuildSelection::Stairs, BuildItemType::Terrain );
	m_buildSelection2buildItem.insert( BuildSelection::Ramps, BuildItemType::Terrain );
	m_buildSelection2buildItem.insert( BuildSelection::Fence, BuildItemType::Terrain );

	m_buildSelection2buildItem.insert( BuildSelection::Containers, BuildItemType::Item );
	m_buildSelection2buildItem.insert( BuildSelection::Furniture, BuildItemType::Item );
	m_buildSelection2buildItem.insert( BuildSelection::Utility, BuildItemType::Item );
}

/// @brief Destructor.
AggregatorInventory::~AggregatorInventory()
{
}

/// @brief Binds the aggregator to a Game instance, pre-caches the item → group and
///        item → category lookups, and restores the watch list from GameState by dispatching
///        each saved watched entry to the correct updateWatchedItem() overload.
/// @param game Game to bind to.
void AggregatorInventory::init( Game* game )
{
	QObject::disconnect( stockpileContentConnection_ );
	QObject::disconnect( stockpileDeletedConnection_ );
	g = game;
	stockpileContentConnection_ = QObject::connect( g->spm(), &StockpileManager::signalStockpileContentChanged,
		this, [this]( unsigned int ) { emit signalInventoryChanged(); } );
	stockpileDeletedConnection_ = QObject::connect( g->spm(), &StockpileManager::signalStockpileDeleted,
		this, [this]( unsigned int ) { emit signalInventoryChanged(); } );

	for ( const auto& cat : g->inv()->categories() )
	{
		for ( const auto& group : g->inv()->groups( cat ) )
		{
			for ( const auto& item : g->inv()->items( cat, group ) )
			{
				m_itemToCategoryCache.insert( item, cat );
				m_itemToGroupCache.insert( item, group );
			}
		}
	}
	m_watchedItems.clear();
	// Can't use iterator due to 2nd reference being created in updateWatchedItem
	for( size_t i = 0; i < GameState::watchedItemList.size(); ++i )
	{
		// Reference is valid until first updateWatchedItem only
		const auto& gwi = GameState::watchedItemList[i];
		QString key = gwi.category + gwi.group + gwi.item + gwi.material;
		m_watchedItems.insert( key );

		if( m_watchedItems.contains( gwi.category ) && gwi.category == key )
		{
			updateWatchedItem( gwi.category );
		}
		else if( m_watchedItems.contains( gwi.category + gwi.group ) && gwi.category + gwi.group == key  )
		{
			updateWatchedItem( gwi.category, gwi.group );
		}
		else if( m_watchedItems.contains( gwi.category + gwi.group + gwi.item ) && gwi.category + gwi.group + gwi.item == key )
		{
			updateWatchedItem( gwi.category, gwi.group, gwi.item );
		}
		else //if( m_watchedItems.contains( gwi.category + gwi.group + gwi.item + gwi.material ) && gwi.category + gwi.group + gwi.item + gwi.material == key )
		{
			updateWatchedItem( gwi.category, gwi.group, gwi.item, gwi.material );
		}
	}
}

/// @brief Walks the inventory category/group/item/material tree and emits a populated
///        GuiInventoryCategory list with per-level totals and watch flags.
void AggregatorInventory::onRequestCategories()
{
	if( !g ) return;
	QHash<QString, QList<GuiItemIngredient>> ingredientsByCraft;
	for ( const auto& row : DB::selectRows( "Crafts_Components" ) )
	{
		const auto item = row.value( "ItemID" ).toString();
		if ( item.isEmpty() ) continue;
		ingredientsByCraft[row.value( "ID" ).toString()].append( {
			item, S::s( "$ItemName_" + item ), row.value( "AllowedMaterial" ).toString(),
			row.value( "AllowedMaterialType" ).toString(), row.value( "Amount" ).toInt() } );
	}
	QHash<QString, QString> workshopByCraft;
	for ( const auto& row : DB::selectRows( "Workshops" ) )
		for ( const auto& craft : row.value( "Crafts" ).toString().split( '|', Qt::SkipEmptyParts ) )
			workshopByCraft.insert( craft, row.value( "ID" ).toString() );
	QHash<QString, QList<GuiItemRecipe>> madeBy, usedIn;
	for ( const auto& row : DB::selectRows( "Crafts" ) )
	{
		const auto id = row.value( "ID" ).toString();
		const auto item = row.value( "ItemID" ).toString();
		if ( item.isEmpty() ) continue;
		GuiItemRecipe recipe { id, item, S::s( "$ItemName_" + item ), workshopByCraft.value( id ),
			row.value( "SkillID" ).toString(), row.value( "ResultMaterial" ).toString(), row.value( "ResultMaterialTypes" ).toString(),
			row.value( "ConversionMaterial" ).toString(), qMax( 1, row.value( "Amount" ).toInt() ), ingredientsByCraft.value( id ) };
		madeBy[item].append( recipe );
		QSet<QString> usedItems;
		for ( const auto& ingredient : recipe.ingredients )
			if ( !usedItems.contains( ingredient.itemID ) )
			{
				usedIn[ingredient.itemID].append( recipe );
				usedItems.insert( ingredient.itemID );
			}
	}
	const auto locationKey = []( const QString& item, const QString& material ) { return item + QChar( 0x1f ) + material; };
	QHash<QString, QList<GuiItemStockpile>> locations;
	for ( const auto stockpileID : g->spm()->allStockpiles() )
	{
		auto* stockpile = g->spm()->getStockpile( stockpileID );
		if ( !stockpile ) continue;
		QHash<QString, int> counts;
		for ( const auto* field : stockpile->getFields() )
			if ( field ) for ( const auto itemID : field->items )
				if ( g->inv()->itemExists( itemID ) )
				{
					const auto item = g->inv()->itemSID( itemID );
					++counts[locationKey( item, {} )];
					const auto material = g->inv()->materialSID( itemID );
					if ( !material.isEmpty() ) ++counts[locationKey( item, material )];
				}
		for ( auto it = counts.cbegin(); it != counts.cend(); ++it )
			locations[it.key()].append( { stockpileID, stockpile->name(), it.value() } );
	}
	m_categories.clear();
	for ( const auto& cat : g->inv()->categories() )
	{
		GuiInventoryCategory gic;
		gic.id = cat;
		gic.name = S::s( "$CategoryName_" + cat );
		gic.watched = m_watchedItems.contains( cat );

		for ( const auto& group : g->inv()->groups( cat ) )
		{
			GuiInventoryGroup gig;
			gig.id = group;
			gig.name = S::s( "$GroupName_" + group );
			gig.cat = cat;
			gig.watched = m_watchedItems.contains( cat + group );

			for ( const auto& item : g->inv()->items( cat, group ) )
			{
				GuiInventoryItem gii;
				gii.id = item;
				gii.name = S::s( "$ItemName_" + item );
				gii.cat = cat;
				gii.group = group;
				gii.watched = m_watchedItems.contains( cat + group + item );
				gii.madeBy = madeBy.value( item );
				gii.usedIn = usedIn.value( item );
				gii.locations = locations.value( locationKey( item, {} ) );
				setInventoryItemSprite( gii );

				for ( const auto& mat : g->inv()->materials( cat, group, item ) )
				{
					auto result   = g->inv()->itemCountDetailed( item, mat );
					//if( result.total > 0 )
					{
						GuiInventoryMaterial gim;
						gim.id = mat;
						gim.name = S::s( "$MaterialName_" + mat );
						gim.cat = cat;
						gim.group = group;
						gim.item = item;
						gim.watched = m_watchedItems.contains( cat + group + item + mat );
						const auto materialType = DB::select( "Type", "Materials", mat ).toString();
						const auto allowsType = [&]( const QString& types )
							{ return types.isEmpty() || materialType.isEmpty() || types.split( '|', Qt::SkipEmptyParts ).contains( materialType ); };
						for ( const auto& recipe : gii.madeBy )
						{
							if ( !recipe.resultMaterial.isEmpty() && recipe.resultMaterial != mat && !recipe.resultMaterial.startsWith( '$' ) && recipe.resultMaterial != "RandomMetal" ) continue;
							if ( !recipe.conversionMaterial.isEmpty() && recipe.conversionMaterial != mat && !recipe.conversionMaterial.startsWith( '$' ) ) continue;
							if ( !allowsType( recipe.resultMaterialTypes ) ) continue;
							gim.madeBy.append( recipe );
						}
						for ( const auto& recipe : gii.usedIn )
							for ( const auto& ingredient : recipe.ingredients )
								if ( ingredient.itemID == item && ( ingredient.allowedMaterial.isEmpty() || ingredient.allowedMaterial == mat ) && allowsType( ingredient.allowedMaterialType ) )
								{ gim.usedIn.append( recipe ); break; }
						gim.locations = locations.value( locationKey( item, mat ) );
						gim.countTotal = result.total;
						gim.countInJob = result.inJob;
						gim.countInStockpiles = result.inStockpile;
						gim.countEquipped = result.equipped;
						gim.countConstructed = result.constructed;
						gim.countLoose = result.loose;
						gim.totalValue = result.totalValue;
						setInventoryMaterialSprite( gim, item );
						if ( gim.spriteSheet.isEmpty() )
						{
							gim.spriteSheet = gii.spriteSheet;
							gim.spriteX = gii.spriteX;
							gim.spriteY = gii.spriteY;
							gim.spriteWidth = gii.spriteWidth;
							gim.spriteHeight = gii.spriteHeight;
							gim.spriteSheetWidth = gii.spriteSheetWidth;
							gim.spriteSheetHeight = gii.spriteSheetHeight;
						}

						gii.countTotal += result.total;
						gii.countInStockpiles += result.inStockpile;

						gii.materials.append( gim );
					}
				}
				gig.countTotal += gii.countTotal;
				gig.countInStockpiles += gii.countInStockpiles;

				gig.items.append( gii );
			}

			gic.countTotal += gig.countTotal;
			gic.countInStockpiles += gig.countInStockpiles;

			gic.groups.append( gig );
		}

		m_categories.append( gic );
	}

	emit signalInventoryCategories( m_categories );
}

void AggregatorInventory::onRequestHistory( QString itemSID, QString materialSID, int dayCount )
{
	if ( !g || itemSID.isEmpty() )
		return;
	if ( itemSID == QStringLiteral( "RawWood" ) && g->tutorial() )
		g->tutorial()->observeFact( TutorialFact::OpenInventory );

	const auto history = g->ih()->getHistory( itemSID );
	QString key = materialSID.isEmpty() ? QStringLiteral( "all" ) : materialSID;
	if ( !history.contains( key ) )
		key = QStringLiteral( "all" );
	const auto values = history.value( key );
	const int begin = dayCount > 0 ? qMax( 0, values.size() - dayCount ) : 0;
	QList<GuiInventoryHistoryPoint> points;
	points.reserve( values.size() - begin );
	for ( int i = begin; i < values.size(); ++i )
	{
		const auto& value = values.at( i );
		points.push_back( GuiInventoryHistoryPoint { i, value.total, value.plus, value.minus } );
	}
	signalInventoryHistory( itemSID, materialSID, points );
}

void AggregatorInventory::setInventoryItemSprite( GuiInventoryItem& item )
{
	assignInventorySprite( item.spriteSheet, item.spriteX, item.spriteY, item.spriteWidth, item.spriteHeight, item.spriteSheetWidth, item.spriteSheetHeight, findBaseSprite( DBH::spriteID( item.id ) ) );
	item.spriteSheet = inventoryIcon( item.id );
	if ( !item.spriteSheet.isEmpty() ) item.spriteWidth = item.spriteHeight = item.spriteSheetWidth = item.spriteSheetHeight = 40;
}

void AggregatorInventory::setInventoryMaterialSprite( GuiInventoryMaterial& material, const QString& itemID )
{
	assignInventorySprite( material.spriteSheet, material.spriteX, material.spriteY, material.spriteWidth, material.spriteHeight, material.spriteSheetWidth, material.spriteSheetHeight, findMaterialBaseSprite( DBH::spriteID( itemID ), material.id ) );
	material.spriteSheet = inventoryIcon( itemID, material.id );
	if ( !material.spriteSheet.isEmpty() ) material.spriteWidth = material.spriteHeight = material.spriteSheetWidth = material.spriteSheetHeight = 40;
}

QString AggregatorInventory::inventoryIcon( const QString& itemID, const QString& materialID )
{
	const auto filename = "catalog_" + itemID + "__" + materialID + ".tga";
	if ( QFileInfo::exists( Global::cfg->get( "dataPath" ).toString() + "/tilesheet/" + filename ) ) return filename;
	const auto base = findMaterialBaseSprite( DBH::spriteID( itemID ), materialID );
	GuiInventoryMaterial fallback;
	assignInventorySprite( fallback.spriteSheet, fallback.spriteX, fallback.spriteY, fallback.spriteWidth, fallback.spriteHeight, fallback.spriteSheetWidth, fallback.spriteSheetHeight, base );
	return fallback.spriteSheet;
}
/// @brief Collects the buildable entries for the given BuildSelection / category combination
///        (Constructions / Workshops / Containers / Items DB rows), produces preview icons
///        and required-item lists, and emits signalBuildItems.
/// @param buildSelection High-level build category (Wall, Workshop, Furniture, …).
/// @param category       Sub-category (e.g. workshop tab name or construction category).
void AggregatorInventory::onRequestBuildItems( BuildSelection buildSelection, QString category )
{
	if( !g ) return;
	m_buildItems.clear();
	if ( m_buildSelection2String.contains( buildSelection ) )
	{
		QList<QVariantMap> rows;
		QString prefix = "$ConstructionName_";
		switch( buildSelection )
		{
			case BuildSelection::Wall:
			case BuildSelection::Floor:
			case BuildSelection::Stairs:
			case BuildSelection::Ramps:
			case BuildSelection::Fence:
				rows = DB::selectRows( "Constructions", "Type", m_buildSelection2String.value( buildSelection ) );
				// These construction types are valid build targets but do not have
				// dedicated toolbar buttons. Keep them with their closest terrain
				// family instead of silently dropping them from the catalog.
				if ( buildSelection == BuildSelection::Wall )
					rows += DB::selectRows( "Constructions", "Type", "WallFloor" );
				else if ( buildSelection == BuildSelection::Ramps )
					rows += DB::selectRows( "Constructions", "Type", "RampCorner" );
				break;
			case BuildSelection::Workshop:
				rows = DB::selectRows( "Workshops" );
				prefix = "$WorkshopName_";
				break;
			case BuildSelection::Containers:
				rows = DB::selectRows( "Containers", "Buildable", "1" );
				prefix = "$ItemName_";
				break;
			case BuildSelection::Furniture:
				rows = DB::selectRows( "Items", "Category", "Furniture" );
				prefix = "$ItemName_";
				break;
			case BuildSelection::Utility:
				rows = DB::selectRows( "Items", "Category", "Utility" );
				prefix = "$ItemName_";
				break;
		}

		//qDebug() << "Type:" << m_buildSelection2String.value( buildSelection )<< "Category" << category << rows.size();
		for ( auto row : rows )
		{
			GuiBuildItem gbi;
			gbi.id   = row.value( "ID" ).toString();
			gbi.name = S::s( prefix + row.value( "ID" ).toString() );
			if ( buildSelection == BuildSelection::Workshop ) gbi.type = row.value( "Tab" ).toString();
			else if ( buildSelection == BuildSelection::Furniture || buildSelection == BuildSelection::Utility ) gbi.type = row.value( "ItemGroup" ).toString();
			else if ( buildSelection == BuildSelection::Containers ) gbi.type = QStringLiteral( "Containers" );
			else gbi.type = row.value( "Category" ).toString();
			if ( gbi.type.isEmpty() ) gbi.type = QStringLiteral( "Other" );
			if ( !category.isEmpty() && gbi.type.compare( category, Qt::CaseInsensitive ) != 0 ) continue;
			gbi.biType = m_buildSelection2buildItem.value( buildSelection );

			setBuildItemSprite( gbi, buildSelection );
			setBuildItemValues( gbi, buildSelection );

			m_buildItems.append( gbi );
		}
	}
	emit signalBuildItems( m_buildItems );
}

void AggregatorInventory::setBuildItemSprite( GuiBuildItem& gbi, BuildSelection selection )
{
	QString spriteID;
	switch ( selection )
	{
		case BuildSelection::Wall:
		case BuildSelection::Floor:
		case BuildSelection::Stairs:
		case BuildSelection::Ramps:
		case BuildSelection::Fence:
		{
			const auto rows = DB::selectRows( "Constructions_Sprites", "ID", gbi.id );
			for ( const auto& row : rows )
			{
				if ( !row.value( "SpriteID" ).toString().isEmpty() )
				{
					spriteID = row.value( "SpriteID" ).toString();
					break;
				}
			}
		}
		break;
		case BuildSelection::Workshop:
			spriteID = DB::select( "Icon", "Workshops", gbi.id ).toString();
			if ( spriteID.isEmpty() )
			{
				for ( const auto& row : DB::selectRows( "Workshops_Components", gbi.id ) )
				{
					if ( !row.value( "SpriteID" ).toString().isEmpty() )
					{
						spriteID = row.value( "SpriteID" ).toString();
						break;
					}
				}
			}
			break;
		case BuildSelection::Containers:
		{
			const auto rows = DB::selectRows( "Containers_Tiles", gbi.id );
			for ( const auto& row : rows )
			{
				if ( !row.value( "SpriteID" ).toString().isEmpty() )
				{
					spriteID = row.value( "SpriteID" ).toString();
					break;
				}
			}
			if ( spriteID.isEmpty() ) spriteID = DBH::spriteID( gbi.id );
		}
		break;
		case BuildSelection::Furniture:
		case BuildSelection::Utility:
			spriteID = DBH::spriteID( gbi.id );
			break;
	}

	const auto base = findBaseSprite( spriteID );
	if ( base.isEmpty() ) return;
	const auto rect = base.value( "SourceRectangle" ).toString().split( " ", Qt::SkipEmptyParts );
	if ( rect.size() != 4 ) return;
	bool ok = false;
	const int x = rect[0].toInt( &ok ); if ( !ok ) return;
	const int y = rect[1].toInt( &ok ); if ( !ok ) return;
	const int width = rect[2].toInt( &ok ); if ( !ok || width <= 0 ) return;
	const int height = rect[3].toInt( &ok ); if ( !ok || height <= 0 ) return;
	const auto dimensions = sheetDimensions( base.value( "Tilesheet" ).toString() );
	if ( dimensions.first <= 0 || dimensions.second <= 0 ) return;
	const auto pngSheet = base.value( "Tilesheet" ).toString();
	const auto sheetName = pngSheet.toLower();
	// RmlUi's pinned GL3 backend deliberately supports uncompressed TGA only.
	// These four sheets cover the construction palette; other world-only sheets
	// safely retain the text/glyph fallback until a preview is requested for them.
	if ( sheetName != "default.png" && sheetName != "furniture.png" && sheetName != "workshops.png" && sheetName != "terrain.png" ) return;
	// Per-entry crops keep the RmlUi document small and avoid relying on
	// renderer-specific background-position support. The files are generated
	// from these same DB rectangles and retain the original pixel art.
	gbi.spriteSheet = "build_" + base.value( "ID" ).toString() + ".tga";
	gbi.spriteX = 0;
	gbi.spriteY = 0;
	gbi.spriteWidth = width;
	gbi.spriteHeight = height;
	gbi.spriteSheetWidth = width;
	gbi.spriteSheetHeight = height;
}

/// @brief Populates a GuiBuildItem with required components and a PNG-encoded preview icon,
///        using the correct component table and icon builder for the given selection type.
/// @param gbi       Build item to populate (modified in place).
/// @param selection High-level build category used to pick the right component/icon path.
void AggregatorInventory::setBuildItemValues( GuiBuildItem& gbi, BuildSelection selection )
{
	if( !g ) return;
	auto type = m_buildSelection2buildItem.value( selection );
	switch ( type )
	{
		case BuildItemType::Workshop:
		{
			for ( auto row : DB::selectRows( "Workshops_Components", gbi.id ) )
			{
				if ( !row.value( "ItemID" ).toString().isEmpty() )
				{
					GuiBuildRequiredItem gbri;
					gbri.itemID = row.value( "ItemID" ).toString();
					gbri.amount = row.value( "Amount" ).toInt();
					setAvailableMats( gbri );
					gbi.requiredItems.append( gbri );
				}
			}

		}
		break;
		case BuildItemType::Terrain:
		{
			for ( auto row : DB::selectRows( "Constructions_Components", gbi.id ) )
			{
					GuiBuildRequiredItem gbri;
					gbri.itemID = row.value( "ItemID" ).toString();
					gbri.amount = row.value( "Amount" ).toInt();
					setAvailableMats( gbri );
					gbi.requiredItems.append( gbri );

			}

		}
		break;
		case BuildItemType::Item:
		{
			auto rows = DB::selectRows( "Constructions_Components", gbi.id );

			if( rows.size() )
			{
				for ( auto row : rows )
				{
					GuiBuildRequiredItem gbri;
					gbri.itemID = row.value( "ItemID" ).toString();
					gbri.amount = row.value( "Amount" ).toInt();
					setAvailableMats( gbri );
					gbi.requiredItems.append( gbri );
				}
			}
			else
			{
				GuiBuildRequiredItem gbri;
				gbri.itemID = gbi.id;
				gbri.amount = 1;
				setAvailableMats( gbri );
				gbi.requiredItems.append( gbri );
			}


		}
		break;
	}
}

/// @brief Fills the availableMats list on @p gbri with (material, count) pairs for the
///        component item, always placing the "any" wildcard first.
/// @param gbri Required component to populate.
void AggregatorInventory::setAvailableMats( GuiBuildRequiredItem& gbri )
{
	if( !g ) return;
	auto mats = g->inv()->materialCountsForItem( gbri.itemID );

	gbri.availableMats.append( { "any", mats["any"] } );
	for ( auto key : mats.keys() )
	{
		if ( key != "any" )
		{
			gbri.availableMats.append( { key, mats[key] } );
		}
	}
}

/// @brief Adds or removes @p gwi from the persistent watch list (GameState::watchedItemList)
///        and refreshes the corresponding count by dispatching to the appropriate
///        updateWatchedItem() overload.
/// @param active True to add to the watch list, false to remove.
/// @param gwi    Watch list entry (category/group/item/material path).
void AggregatorInventory::onSetActive( bool active, const GuiWatchedItem& gwi )
{
	QString key = gwi.category + gwi.group + gwi.item + gwi.material;
	if( active )
	{
		m_watchedItems.insert( key );
		GameState::watchedItemList.append( gwi );
	}
	else
	{
		m_watchedItems.remove( key );
		for( int i = 0; i < GameState::watchedItemList.size(); ++i )
		{
			auto hwi = GameState::watchedItemList[i];
			if( gwi.category == hwi.category && gwi.group == hwi.group && gwi.item == hwi.item && gwi.material == hwi.material )
			{
				GameState::watchedItemList.removeAt( i );
				break;
			}
		}
	}

	if( m_watchedItems.contains( gwi.category ) && gwi.category == key )
	{
		updateWatchedItem( gwi.category );
	}
	else if( m_watchedItems.contains( gwi.category + gwi.group ) && gwi.category + gwi.group == key  )
	{
		updateWatchedItem( gwi.category, gwi.group );
	}
	else if( m_watchedItems.contains( gwi.category + gwi.group + gwi.item ) && gwi.category + gwi.group + gwi.item == key )
	{
		updateWatchedItem( gwi.category, gwi.group, gwi.item );
	}
	else //if( m_watchedItems.contains( gwi.category + gwi.group + gwi.item + gwi.material ) && gwi.category + gwi.group + gwi.item + gwi.material == key )
	{
		updateWatchedItem( gwi.category, gwi.group, gwi.item, gwi.material );
	}


	//onRequestCategories();
}


/// @brief Live-update hook invoked when an item is added to the world. Refreshes any watched
///        rows whose path contains this (item, material) pair.
/// @param itemSID     Item string ID.
/// @param materialSID Material string ID.
void AggregatorInventory::onAddItem( QString itemSID, QString materialSID )
{
	QString cat = m_itemToCategoryCache.value( itemSID );
	QString group = m_itemToGroupCache.value( itemSID );

	if( m_watchedItems.contains( cat ) )
	{
		updateWatchedItem( cat );
	}
	if( m_watchedItems.contains( cat + group ) )
	{
		updateWatchedItem( cat, group );
	}
	if( m_watchedItems.contains( cat + group + itemSID ) )
	{
		updateWatchedItem( cat, group, itemSID );
	}
	if( m_watchedItems.contains( cat + group + itemSID + materialSID ) )
	{
		updateWatchedItem( cat, group, itemSID, materialSID );
	}
	emit signalInventoryChanged();
}

/// @brief Live-update hook invoked when an item is removed from the world. Refreshes any
///        watched rows whose path contains this (item, material) pair.
/// @param itemSID     Item string ID.
/// @param materialSID Material string ID.
void AggregatorInventory::onRemoveItem( QString itemSID, QString materialSID )
{
	QString cat = m_itemToCategoryCache.value( itemSID );
	QString group = m_itemToGroupCache.value( itemSID );

	if( m_watchedItems.contains( cat ) )
	{
		updateWatchedItem( cat );
	}
	if( m_watchedItems.contains( cat + group ) )
	{
		updateWatchedItem( cat, group );
	}
	if( m_watchedItems.contains( cat + group + itemSID ) )
	{
		updateWatchedItem( cat, group, itemSID );
	}
	if( m_watchedItems.contains( cat + group + itemSID + materialSID ) )
	{
		updateWatchedItem( cat, group, itemSID, materialSID );
	}
	emit signalInventoryChanged();
}

/// @brief Recomputes the count for a category-level watch entry and emits signalWatchList.
/// @param cat Category ID whose watch entry to refresh.
void AggregatorInventory::updateWatchedItem( QString cat )
{
	for( auto& gwi : GameState::watchedItemList )
	{
		if( gwi.category == cat && gwi.group.isEmpty() && gwi.item.isEmpty() && gwi.material.isEmpty() )
		{
			gwi.count = 0;
			for ( const auto& group : g->inv()->groups( cat ) )
			{
				for ( const auto& item : g->inv()->items( cat, group ) )
				{
					for ( const auto& mat : g->inv()->materials( cat, group, item ) )
					{
						gwi.count += g->inv()->itemCount( item, mat );
					}
				}
			}
			gwi.guiString = S::s( "$CategoryName_" + cat ) + ": " + QString::number( gwi.count );
			break;
		}
	}
	emit signalWatchList( GameState::watchedItemList );
}

/// @brief Recomputes the count for a group-level watch entry and emits signalWatchList.
/// @param cat   Category ID.
/// @param group Group ID within @p cat.
void AggregatorInventory::updateWatchedItem( QString cat, QString group )
{
	for( auto& gwi : GameState::watchedItemList )
	{
		if( gwi.category == cat && gwi.group == group && gwi.item.isEmpty() && gwi.material.isEmpty() )
		{
			gwi.count = 0;

			for ( const auto& item : g->inv()->items( cat, group ) )
			{
				for ( const auto& mat : g->inv()->materials( cat, group, item ) )
				{
					gwi.count += g->inv()->itemCount( item, mat );
				}
			}
			gwi.guiString = S::s( "$GroupName_" + group ) + ": " + QString::number( gwi.count );

			break;
		}
	}
	emit signalWatchList( GameState::watchedItemList );
}

/// @brief Recomputes the count for an item-level watch entry and emits signalWatchList.
/// @param cat   Category ID.
/// @param group Group ID.
/// @param item  Item ID.
void AggregatorInventory::updateWatchedItem( QString cat, QString group, QString item )
{
	for( auto& gwi : GameState::watchedItemList )
	{
		if( gwi.category == cat && gwi.group == group && gwi.item == item && gwi.material.isEmpty() )
		{
			gwi.count = 0;
			for ( const auto& mat : g->inv()->materials( cat, group, item ) )
			{
				gwi.count += g->inv()->itemCount( item, mat );
			}
			gwi.guiString = S::s( "$ItemName_" + item ) + ": " + QString::number( gwi.count );
			break;
		}
	}
	emit signalWatchList( GameState::watchedItemList );
}

/// @brief Recomputes the count for a fully qualified (item, material) watch entry and emits
///        signalWatchList.
/// @param cat   Category ID.
/// @param group Group ID.
/// @param item  Item ID.
/// @param mat   Material ID.
void AggregatorInventory::updateWatchedItem( QString cat, QString group, QString item, QString mat )
{
	for( auto& gwi : GameState::watchedItemList )
	{
		if( gwi.category == cat && gwi.group == group && gwi.item == item && gwi.material == mat )
		{
			gwi.count = g->inv()->itemCount( item, mat );
			gwi.guiString = S::s( "$MaterialName_" + mat ) + " " + S::s( "$ItemName_" + item ) + ": " + QString::number( gwi.count );
			break;
		}
	}
	emit signalWatchList( GameState::watchedItemList );
}

/// @brief Full refresh: rebuilds the category tree and re-emits the watch list.
void AggregatorInventory::update()
{
	onRequestCategories();

	emit signalWatchList( GameState::watchedItemList );
}
