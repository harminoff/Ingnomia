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
/** @file farm.cpp
 *  @brief Farm plot implementation: crop management, tilling, planting, harvesting,
 *         and auto-harvest threshold logic.
 */
#include "farm.h"

#include "../base/config.h"
#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/util.h"
#include "../game/game.h"
#include "../game/gnomemanager.h"
#include "../game/inventory.h"
#include "../game/jobmanager.h"
#include "../game/plant.h"
#include "../game/world.h"

#include <QDebug>
#include <algorithm>

/** @brief Constructs farm properties by deserializing from a variant map.
 *  @param in Variant map containing plant type, seed item, harvest settings, and auto-harvest thresholds. */
FarmProperties::FarmProperties( QVariantMap& in )
{
	plantType = in.value( "PlantType" ).toString();
	seedItem  = in.value( "SeedItemID" ).toString();
	onlySeed  = in.value( "OnlySeed" ).toBool();
	harvest   = in.value( "Harvest" ).toBool();
	auto vjpl = in.value( "JobPriorities" ).toList();
	for ( auto vjp : vjpl )
	{
		jobPriorities.append( vjp.value<quint8>() );
	}

	autoHarvestSeed  = in.value( "AutoHarvestSeed" ).toBool();
	autoHarvestItem1 = in.value( "AutoHarvestItem1" ).toBool();
	autoHarvestItem2 = in.value( "AutoHarvestItem2" ).toBool();

	autoHarvestSeedMin  = in.value( "AutoHarvestSeedMin" ).toUInt();
	autoHarvestSeedMax  = in.value( "AutoHarvestSeedMax" ).toUInt();
	autoHarvestItem1Min = in.value( "AutoHarvestItem1Min" ).toUInt();
	autoHarvestItem1Max = in.value( "AutoHarvestItem1Max" ).toUInt();
	autoHarvestItem2Min = in.value( "AutoHarvestItem2Min" ).toUInt();
	autoHarvestItem2Max = in.value( "AutoHarvestItem2Max" ).toUInt();
}

/** @brief Serializes farm properties into a variant map.
 *  @param out Output variant map. */
void FarmProperties::serialize( QVariantMap& out ) const
{
	out.insert( "Type", "farm" );
	out.insert( "PlantType", plantType );
	out.insert( "SeedItemID", seedItem );
	out.insert( "OnlySeed", onlySeed );
	out.insert( "Harvest", harvest );
	QVariantList jpl;
	for ( auto jp : jobPriorities )
	{
		jpl.append( jp );
	}
	out.insert( "JobPriorities", jpl );

	out.insert( "AutoHarvestSeed", autoHarvestSeed );
	out.insert( "AutoHarvestItem1", autoHarvestItem1 );
	out.insert( "AutoHarvestItem2", autoHarvestItem2 );

	out.insert( "AutoHarvestSeedMin", autoHarvestSeedMin );
	out.insert( "AutoHarvestSeedMax", autoHarvestSeedMax );
	out.insert( "AutoHarvestItem1Min", autoHarvestItem1Min );
	out.insert( "AutoHarvestItem1Max", autoHarvestItem1Max );
	out.insert( "AutoHarvestItem2Min", autoHarvestItem2Min );
	out.insert( "AutoHarvestItem2Max", autoHarvestItem2Max );
}

/** @brief Constructs a new farm from a set of designated tiles.
 *  @param tiles List of position/validity pairs for the farm fields.
 *  @param game Pointer to the owning Game instance. */
Farm::Farm( QList<QPair<Position, bool>> tiles, Game* game ) :
	WorldObject( game )
{
	m_name = "Farm";

	m_properties.jobPriorities.clear();
	m_properties.jobPriorities.push_back( FarmJobs::Harvest );
	m_properties.jobPriorities.push_back( FarmJobs::PlantPlant );
	m_properties.jobPriorities.push_back( FarmJobs::Till );

	for ( auto p : tiles )
	{
		if ( p.second )
		{
			m_fields.insert( p.first.toInt(), FarmField(p.first, nullptr));
		}
	}

	if (!m_fields.empty())
	{
		m_properties.firstPos = m_fields.first().pos;
	}
}

/** @brief Constructs a farm from serialized save data.
 *  @param vals Variant map containing saved farm state.
 *  @param game Pointer to the owning Game instance. */
Farm::Farm( QVariantMap vals, Game* game ) :
	WorldObject( vals, game ),
	m_properties( vals )
{
	QVariantList vfl = vals.value( "Fields" ).toList();
	for ( auto vf : vfl )
	{
		FarmField grofi;
		auto gfm = vf.toMap();
		grofi.pos       = Position( gfm.value( "Pos" ).toString() );
		grofi.crop = gfm.value( "Crop" ).toString();
		grofi.pendingCrop = gfm.value( "PendingCrop" ).toString();
		grofi.pendingOrder = gfm.value( "PendingOrder" ).toUInt();
		for ( const auto& value : gfm.value( "CropOrders" ).toList() )
		{
			const auto order = value.toMap();
			FarmCropOrder cropOrder { order.value( "Id" ).toUInt(), order.value( "Crop" ).toString(), order.value( "Remaining", 1 ).toInt(), order.value( "Repeat" ).toBool() };
			if ( !cropOrder.crop.isEmpty() )
			{
				grofi.orders.append( cropOrder );
				m_nextCropOrderId = std::max( m_nextCropOrderId, cropOrder.id + 1 );
			}
		}
		if( gfm.contains( "Job" ) )
		{
			grofi.job = g->jm()->getJob( gfm.value( "Job" ).toUInt() );
		}
		m_fields.insert( grofi.pos.toInt(), std::move(grofi));
	}

	if (!m_fields.empty())
	{
		m_properties.firstPos = m_fields.first().pos;
	}

	QVariantList vjl = vals.value( "Jobs" ).toList();
	for ( auto vj : vjl )
	{
		QSharedPointer<Job> job( new Job( vj.toMap() ) );
	}
}

/** @brief Serializes the farm state including fields and active jobs.
 *  @return QVariant containing the serialized farm data. */
QVariant Farm::serialize() const
{
	QVariantMap out;
	WorldObject::serialize( out );
	m_properties.serialize( out );

	QVariantList tiles;
	for ( const auto& field : m_fields )
	{
		QVariantMap entry;
		entry.insert( "Pos", field.pos.toString() );
		entry.insert( "Crop", field.crop );
		entry.insert( "PendingCrop", field.pendingCrop );
		entry.insert( "PendingOrder", field.pendingOrder );
		QVariantList orders;
		for ( const auto& order : field.orders )
			orders.append( QVariantMap { { "Id", order.id }, { "Crop", order.crop }, { "Remaining", order.remaining }, { "Repeat", order.repeat } } );
		entry.insert( "CropOrders", orders );
		if( field.job )
		{
			QSharedPointer<Job> spJob = field.job.toStrongRef();
			entry.insert( "Job", spJob->id() );
		}
		tiles.append( entry );
	}
	out.insert( "Fields", tiles );
	return out;
}

/** @brief Destructor. */
Farm::~Farm()
{

}

/** @brief Adds a tile to the farm and sets the TF_FARM flag on the world tile.
 *  @param pos Position of the tile to add. */
void Farm::addTile( const Position & pos )
{
	m_fields.insert( pos.toInt(), FarmField(pos, nullptr) );
	g->w()->setTileFlag( pos, TileFlag::TF_FARM );
}

/** @brief Per-tick farm update: for each field without an active job, checks for harvestable
 *         plants, untilled soil, or tilled soil needing planting, and creates appropriate jobs.
 *  @param tick Current game tick. */
void Farm::onTick( quint64 tick )
{
	if ( !m_active )
		return;

	for( auto& gf : m_fields )
	{
		if ( !gf.pendingCrop.isEmpty() && !gf.job )
		{
			const auto plant = g->w()->plants().constFind( gf.pos.toInt() );
			if ( plant != g->w()->plants().cend() && plant->isPlant() && plant->plantID() == gf.pendingCrop )
			{
				for ( auto order = gf.orders.begin(); order != gf.orders.end(); ++order )
				{
					if ( order->id != gf.pendingOrder ) continue;
					if ( order->repeat )
					{
						const auto completed = *order;
						gf.orders.erase( order );
						gf.orders.append( completed );
					}
					else if ( --order->remaining <= 0 ) gf.orders.erase( order );
					break;
				}
			}
			gf.pendingCrop.clear();
			gf.pendingOrder = 0;
		}
		if( !gf.job )
		{
			Tile& tile = g->w()->getTile( gf.pos );

			if ( g->w()->plants().contains( gf.pos.toInt() ) )
			{
				Plant& plant = g->w()->plants()[gf.pos.toInt()];
				if ( !plant.isPlant() )
				{
					continue;
				}
				if ( plant.harvestable() )
				{
					//harvest
					unsigned int jobID = g->jm()->addJob( "Harvest", gf.pos, 0, true );
					auto job = g->jm()->getJob( jobID );
					if( job )
					{
						//job->setPrio();
						gf.job = job;
					}
				}
			}
			else
			{
				if ( !( tile.flags & TileFlag::TF_TILLED ) )
				{
					//till
					unsigned int jobID = g->jm()->addJob( "Till", gf.pos, 0, true );
					auto job = g->jm()->getJob( jobID );
					if( job )
					{
						//job->setPrio();
						gf.job = job;
					}
				}
				if ( tile.flags & TileFlag::TF_TILLED )
				{
					const QString crop = gf.orders.isEmpty() ? ( gf.crop.isEmpty() ? m_properties.plantType : gf.crop ) : gf.orders.first().crop;
					if ( crop.isEmpty() ) continue;
					const QString seed = DB::select( "SeedItemID", "Plants", crop ).toString();
					if ( seed.isEmpty() ) continue;
					auto item = g->inv()->getClosestItem( gf.pos, true, seed, crop );
					if ( item == 0 )
					{
						continue;
					}
					unsigned int jobID = g->jm()->addJob( "PlantFarm", gf.pos, "Plant", { crop }, 0, true );
					auto job = g->jm()->getJob( jobID );
					if( job )
					{
						job->addRequiredItem( 1, seed, crop, QStringList() );
						gf.pendingCrop = crop;
						gf.pendingOrder = gf.orders.isEmpty() ? 0 : gf.orders.first().id;
						//job->setPrio();
						gf.job = job;
					}
				}
			}
		}
	}

	//updateAutoFarmer();
}

/** @brief Checks auto-harvest thresholds for seeds and harvest items, and enables or
 *         disables harvesting based on min/max inventory counts. */
void Farm::updateAutoFarmer()
{
	QString seedMaterialID = DB::select( "Material", "Plants", m_properties.plantType ).toString();

	unsigned int countSeed = g->inv()->itemCount( m_properties.seedItem, seedMaterialID );

	auto hl = DB::selectRows( "Plants_OnHarvest_HarvestedItem", m_properties.plantType );

	QString item1ID;
	QString material1ID;
	QString item2ID;
	QString material2ID;
	for ( auto hi : hl )
	{
		item1ID = hi.value( "ItemID" ).toString();
		if ( item1ID != m_properties.seedItem )
		{
			material1ID = hi.value( "MaterialID" ).toString();
			break;
		}
	}
	unsigned int countItem1 = 0;
	unsigned int countItem2 = 0;
	if ( !item1ID.isEmpty() )
	{
		countItem1 = g->inv()->itemCount( item1ID, material1ID );
	}

	for ( auto hi : hl )
	{
		item2ID = hi.value( "ItemID" ).toString();
		if ( item2ID != m_properties.seedItem && item2ID != item1ID )
		{
			material2ID = hi.value( "MaterialID" ).toString();
			break;
		}
	}
	if ( !item2ID.isEmpty() )
	{
		countItem2 = g->inv()->itemCount( item2ID, material2ID );
	}
	bool harvestOn  = false;
	bool harvestOff = false;

	if ( m_properties.autoHarvestSeed )
	{
		unsigned int count = g->inv()->itemCount( m_properties.seedItem, seedMaterialID );
		unsigned int min   = m_properties.autoHarvestSeedMin;
		unsigned int max   = m_properties.autoHarvestSeedMax;
		if ( count < min )
		{
			harvestOn = true;
		}
		if ( count > max )
		{
			harvestOff = true;
		}
	}
	if ( m_properties.autoHarvestItem1 && !item1ID.isEmpty() )
	{
		unsigned int count = g->inv()->itemCount( item1ID, material1ID );
		unsigned int min   = m_properties.autoHarvestItem1Min;
		unsigned int max   = m_properties.autoHarvestItem1Max;
		if ( count < min )
		{
			harvestOn = true;
		}
		if ( count > max )
		{
			harvestOff = true;
		}
	}
	if ( m_properties.autoHarvestItem2 && !item2ID.isEmpty() )
	{
		unsigned int count = g->inv()->itemCount( item2ID, material2ID );
		unsigned int min   = m_properties.autoHarvestItem2Min;
		unsigned int max   = m_properties.autoHarvestItem2Max;
		if ( count < min )
		{
			harvestOn = true;
		}
		if ( count > max )
		{
			harvestOff = true;
		}
	}

	if ( harvestOn )
	{
		m_properties.harvest = true;
	}
	else
	{
		if ( harvestOff )
		{
			m_properties.harvest = false;
		}
	}
}

/** @brief Returns whether this farm can be safely deleted.
 *  @return Always true. */
bool Farm::canDelete()
{
	return true; //m_jobsOut.isEmpty();
}

/** @brief Removes a tile from the farm, cancels any active job on it, and clears the TF_FARM flag.
 *  @param pos Position of the tile to remove.
 *  @return True if the farm is now empty (last tile removed). */
bool Farm::removeTile( const Position & pos )
{
	auto id = pos.toInt();
	if( m_fields.contains( id ) )
	{
		auto gf = m_fields.value( id );
		if( gf.job )
		{
			//qDebug() << "farm field still has a job";
			g->jm()->deleteJobAt( pos );
		}
	}
	m_fields.remove( id );
	g->w()->clearTileFlag( pos, TileFlag::TF_FARM );

	if (!m_fields.empty())
	{
		m_properties.firstPos = m_fields.first().pos;
	}
	else
	{
		m_properties.firstPos = Position();
	}

	// if last tile deleted return true
	return m_fields.empty();
}

/** @brief Retrieves farm statistics: total plots, tilled count, planted count, and crops ready to harvest.
 *  @param numPlots Output: total number of farm fields.
 *  @param tilled Output: number of tilled fields.
 *  @param planted Output: number of fields with growing plants.
 *  @param cropReady Output: number of fields with harvestable crops. */
void Farm::getInfo( int& numPlots, int& tilled, int& planted, int& cropReady )
{
	numPlots  = m_fields.size();
	tilled    = 0;
	planted   = 0;
	cropReady = 0;
	for ( const auto& gf : m_fields )
	{

		Tile& tile = g->w()->getTile( gf.pos );
		if ( tile.flags & TileFlag::TF_TILLED )
		{
			++tilled;
		}
		if ( g->w()->plants().contains( gf.pos.toInt() ) )
		{
			Plant& plant = g->w()->plants()[gf.pos.toInt()];

			if ( plant.isPlant() )
			{
				++planted;
				if ( plant.harvestable() )
				{
					++cropReady;
				}
			}
		}
	}
}

/** @brief Returns the total number of tiles in this farm.
 *  @return Tile count. */
int Farm::countTiles()
{
	return m_fields.size();
}

/** @brief Sets the crop type for this farm and looks up the corresponding seed item ID.
 *  @param plantID Plant type SID from the Plants DB table. */
void Farm::setPlantType( QString plantID )
{
	m_properties.plantType = plantID;

	QString seedItemID    = DB::select( "SeedItemID", "Plants", plantID ).toString();
	m_properties.seedItem = seedItemID;
}

/** @brief Enables or disables harvesting on this farm.
 *  @param harvest True to enable harvest jobs. */
void Farm::setHarvest( bool harvest )
{
	m_properties.harvest = harvest;
}

bool Farm::setPlotCrop( const QList<Position>& plots, const QString& crop )
{
	if ( !crop.isEmpty() && ( DB::select( "Type", "Plants", crop ).toString() != "Plant"
		|| DB::select( "SeedItemID", "Plants", crop ).toString().isEmpty() ) ) return false;
	bool changed = false;
	for ( const auto& plot : plots )
	{
		auto field = m_fields.find( plot.toInt() );
		if ( field == m_fields.end() || field->crop == crop ) continue;
		field->crop = crop;
		changed = true;
	}
	return changed;
}

bool Farm::queuePlotCrop( const QList<Position>& plots, const QString& crop, int count, bool repeat )
{
	if ( count < 1 || count > 9999 || DB::select( "Type", "Plants", crop ).toString() != "Plant"
		|| DB::select( "SeedItemID", "Plants", crop ).toString().isEmpty() ) return false;
	bool changed = false;
	for ( const auto& plot : plots )
	{
		auto field = m_fields.find( plot.toInt() );
		if ( field == m_fields.end() ) continue;
		field->orders.append( FarmCropOrder { m_nextCropOrderId++, crop, count, repeat } );
		changed = true;
	}
	return changed;
}

bool Farm::hasAssignedCrop() const
{
	if ( !m_properties.plantType.isEmpty() ) return true;
	return std::any_of( m_fields.cbegin(), m_fields.cend(), []( const FarmField& field ) { return !field.crop.isEmpty(); } );
}

bool Farm::hasQueuedCropOrder() const
{
	return std::any_of( m_fields.cbegin(), m_fields.cend(), []( const FarmField& field ) { return !field.orders.isEmpty(); } );
}

bool Farm::removePlotOrder( Position plot, unsigned int orderId )
{
	auto field = m_fields.find( plot.toInt() );
	if ( field == m_fields.end() ) return false;
	for ( auto order = field->orders.begin(); order != field->orders.end(); ++order )
		if ( order->id == orderId ) { field->orders.erase( order ); return true; }
	return false;
}

bool Farm::movePlotOrder( Position plot, unsigned int orderId, bool earlier )
{
	auto field = m_fields.find( plot.toInt() );
	if ( field == m_fields.end() ) return false;
	for ( int index = 0; index < field->orders.size(); ++index )
	{
		if ( field->orders[index].id != orderId ) continue;
		const int destination = index + ( earlier ? -1 : 1 );
		if ( destination < 0 || destination >= field->orders.size() ) return false;
		field->orders.swapItemsAt( index, destination );
		return true;
	}
	return false;
}
