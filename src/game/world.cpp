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

/** @file world.cpp
 *  @brief Implementation of the World class: initialization, sprite setters, tile flags,
 *         grass/water simulation, plant/creature management, mining, ramp creation,
 *         lighting, sunlight propagation, and tile update tracking.
 */

#include "world.h"
#include "waterflowsolver.h"

#include "../base/config.h"
#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/position.h"
#include "../base/util.h"
#include "../base/vptr.h"
#include "../game/game.h"
#include "../game/creaturemanager.h"
#include "../game/farmingmanager.h"
#include "../game/gnomemanager.h"
#include "../game/inventory.h"
#include "../game/mechanismmanager.h"
#include "../game/plant.h"
#include "../game/roommanager.h"
#include "../game/stockpilemanager.h"
#include "../game/workshopmanager.h"
#include "../gfx/sprite.h"
#include "../gfx/spritefactory.h"
#include "../gui/eventconnector.h"

#include <QDebug>
#include <QHash>
#include <QJsonDocument>
#include <QVector3D>

#include <algorithm>
#include <limits>
#include <random>
#include <time.h>

namespace
{
bool isWaterBoundary( int x, int y, int z, int dimX, int dimY, int dimZ )
{
	// The outermost ring is the retaining wall. The inner ring (x/y == 1)
	// is valid playable terrain and is where the world generator places edge
	// rivers and ocean water.
	return x == 0 || x == dimX - 1 || y == 0 || y == dimY - 1 || z == 0 || z == dimZ - 1;
}

bool isWaterBoundary( const Position& pos, int dimX, int dimY, int dimZ )
{
	return isWaterBoundary( pos.x, pos.y, pos.z, dimX, dimY, dimZ );
}

bool isWaterBoundary( unsigned int tileID, int dimX, int dimY, int dimZ )
{
	if ( tileID >= static_cast<unsigned int>( dimX * dimY * dimZ ) )
	{
		return true;
	}
	const unsigned int pitchZ = static_cast<unsigned int>( dimX * dimY );
	const unsigned int z      = tileID / pitchZ;
	const unsigned int plane  = tileID % pitchZ;
	const unsigned int y      = plane / static_cast<unsigned int>( dimX );
	const unsigned int x      = plane % static_cast<unsigned int>( dimX );
	return isWaterBoundary( static_cast<int>( x ), static_cast<int>( y ), static_cast<int>( z ), dimX, dimY, dimZ );
}
}

/**
 * @brief Constructs the World with the given dimensions, initializing construction lookup maps.
 * @param dimX World width in tiles.
 * @param dimY World depth in tiles.
 * @param dimZ World height in z-levels.
 * @param game Pointer to the owning Game instance.
 */
World::World( int dimX, int dimY, int dimZ, Game* game ) :
	g( game ),
	m_dimX( dimX ),
	m_dimY( dimY ),
	m_dimZ( dimZ ),
	m_regionMap( this )
{
	m_constructionSID2ENUM.insert( "Wall", CID_WALL );
	m_constructionSID2ENUM.insert( "FancyWall", CID_FANCYWALL );
	m_constructionSID2ENUM.insert( "Fence", CID_FENCE );
	m_constructionSID2ENUM.insert( "Floor", CID_FLOOR );
	m_constructionSID2ENUM.insert( "FancyFloor", CID_FANCYFLOOR );
	m_constructionSID2ENUM.insert( "WallFloor", CID_WALLFLOOR );
	m_constructionSID2ENUM.insert( "Stairs", CID_STAIRS );
	m_constructionSID2ENUM.insert( "Ramp", CID_RAMP );
	m_constructionSID2ENUM.insert( "RampCorner", CID_RAMPCORNER );
	m_constructionSID2ENUM.insert( "Item", CID_ITEM );
	m_constructionSID2ENUM.insert( "Workshop", CID_WORKSHOP );

	m_constrItemSID2ENUM.insert( "Containers", CI_STORAGE );
	m_constrItemSID2ENUM.insert( "Furniture", CI_FURNITURE );
	m_constrItemSID2ENUM.insert( "Lights", CI_LIGHT );
	m_constrItemSID2ENUM.insert( "Doors", CI_DOOR );
	m_constrItemSID2ENUM.insert( "AlarmBell", CI_ALARMBELL );
	m_constrItemSID2ENUM.insert( "Farm", CI_FARMUTIL );
	m_constrItemSID2ENUM.insert( "Mechanism", CI_MECHANISM );
	m_constrItemSID2ENUM.insert( "Hydraulics", CI_HYDRAULICS );
}

/**
 * @brief Destructor.
 */
World::~World()
{
}

/**
 * @brief Initializes the world after generation: sets dimensions, initializes light map, water, and grass.
 */
void World::init()
{
	qDebug() << "World::init";

	m_dimX = Global::dimX;
	m_dimY = Global::dimY;
	m_dimZ = Global::dimZ;

	m_lightMap.init();
	initWater();
	initGrassUpdateList();
}

/**
 * @brief Scans all tiles to find water, aquifiers, and deaquifiers, initializing the water tracking set.
 */
void World::initWater()
{
	m_water.clear();
	m_activeWater.clear();
	m_aquifiers.clear();
	m_deaquifiers.clear();

	for ( int z = m_dimZ - 2; z >= 0; --z )
	{
		for ( int y = 0; y < m_dimY; ++y )
		{
			for ( int x = 0; x < m_dimX; ++x )
			{
				Tile& here = getTile( x, y, z );
				if ( isWaterBoundary( x, y, z, m_dimX, m_dimY, m_dimZ ) )
				{
					// The outer rows and bottom level form a virtual retaining wall.
					here.fluidLevel = 0;
					here.pressure   = 0;
					here.flow       = WF_NOFLOW;
					here.flags -= TileFlag::TF_WATER;
					continue;
				}

				const bool markedWater = (bool)( here.flags & TileFlag::TF_WATER );
				if ( markedWater && here.fluidLevel == 0 && here.pressure == 0 && !(bool)( here.wallType & WT_MOVEBLOCKING ) )
				{
					// Older generated worlds stored the water-surface marker without
					// storing fluid mass. Treat that marker as authored full water when
					// the world is initialized, rather than waking an empty cell.
					here.fluidLevel = 10;
				}

				if ( here.flags & TileFlag::TF_AQUIFIER )
				{
					addAquifier( Position( x, y, z ) );
				}
				if ( here.flags & TileFlag::TF_DEAQUIFIER )
				{
					addDeaquifier( Position( x, y, z ) );
				}

				if ( here.fluidLevel > 0 || here.pressure > 0 )
				{
					if ( (bool)( here.wallType & ( WT_SOLIDWALL | WT_MOVEBLOCKING ) ) )
					{
						here.fluidLevel = 0;
						here.pressure   = 0;
						here.flow       = WF_NOFLOW;
						here.flags -= TileFlag::TF_WATER;
					}
					else
					{
						// Fluid mass is the canonical water state. Older saves and
						// generator revisions could persist the level without the
						// derived TF_WATER marker, which made the inspector report
						// water while the renderer culled the cell.
						here.flags += TileFlag::TF_WATER;
						Position pos( x, y, z );
						m_water.insert( pos.toInt() );
						// Existing generated/save water is already an authored basin.
						// Keep it settled on load; aquifiers and later topology changes
						// explicitly wake the simulation when new flow is needed.
						here.flow = WF_NOFLOW;
					}
				}
				else if ( !(bool)( here.flags & TileFlag::TF_AQUIFIER ) )
				{
					// Do not let a stale save flag reintroduce a zero-mass water cell.
					here.pressure = 0;
					here.flow     = WF_NOFLOW;
					here.flags -= TileFlag::TF_WATER;
				}
			}
		}
	}
}

/**
 * @brief Post-load initialization: re-initializes region map, light map, grass, and water from loaded tile data.
 */
void World::afterLoad()
{
	qDebug() << "World::afterLoad";
	m_dimX = Global::dimX;
	m_dimY = Global::dimY;
	m_dimZ = Global::dimZ;

	m_regionMap.initRegions();
	m_lightMap.init();
	initGrassUpdateList();
	initWater();
}

/**
 * @brief Sets the floor sprite UID for the tile at the given coordinates and marks it for rendering update.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param spriteUID Sprite UID to assign to the floor.
 */
void World::setFloorSprite( unsigned short x, unsigned short y, unsigned short z, const unsigned int spriteUID )
{
	getTile( x, y, z ).floorSpriteUID = spriteUID;
	addToUpdateList( x, y, z );
}

/**
 * @brief Sets the floor sprite UID for the tile at the given position and marks it for rendering update.
 * @param pos World position.
 * @param spriteUID Sprite UID to assign to the floor.
 */
void World::setFloorSprite( Position pos, unsigned int spriteUID )
{
	getTile( pos ).floorSpriteUID = spriteUID;
	addToUpdateList( pos );
}

/**
 * @brief Sets the wall sprite UID and rotation for the tile at the given coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param spriteUID Sprite UID to assign to the wall.
 * @param rotation Wall rotation (0-3).
 */
void World::setWallSprite( unsigned short x, unsigned short y, unsigned short z, unsigned int spriteUID, unsigned char rotation )
{
	const Position pos( x, y, z );
	unsigned int UID           = pos.toInt();
	m_world[UID].wallSpriteUID = spriteUID;
	m_world[UID].wallRotation  = rotation;
	addToUpdateList( UID );
}

/**
 * @brief Sets the wall sprite UID and rotation for the tile at the given position.
 * @param pos World position.
 * @param spriteUID Sprite UID to assign to the wall.
 * @param rotation Wall rotation (0-3).
 */
void World::setWallSprite( Position pos, unsigned int spriteUID, unsigned char rotation )
{
	unsigned int UID           = pos.toInt();
	m_world[UID].wallSpriteUID = spriteUID;
	m_world[UID].wallRotation  = rotation;
	addToUpdateList( UID );
}

/**
 * @brief Sets the wall sprite UID by flat tile ID (no update list notification).
 * @param tileID Flat tile index.
 * @param spriteUID Sprite UID to assign.
 */
void World::setWallSprite( unsigned int tileID, unsigned int spriteUID )
{
	m_world[tileID].wallSpriteUID = spriteUID;
}

/**
 * @brief Sets the item sprite UID for the tile at the given coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param spriteUID Sprite UID to assign to the item layer.
 * @param rotation Item rotation (currently unused).
 */
void World::setItemSprite( unsigned short x, unsigned short y, unsigned short z, unsigned int spriteUID, unsigned char rotation )
{
	const Position pos( x, y, z );
	unsigned int UID           = pos.toInt();
	m_world[UID].itemSpriteUID = spriteUID;
	//m_world[UID].wallRotation = rotation;
	addToUpdateList( UID );
}

/**
 * @brief Sets the item sprite UID for the tile at the given position.
 * @param pos World position.
 * @param spriteUID Sprite UID to assign to the item layer.
 * @param rotation Item rotation (currently unused).
 */
void World::setItemSprite( Position pos, unsigned int spriteUID, unsigned char rotation )
{
	unsigned int UID           = pos.toInt();
	m_world[UID].itemSpriteUID = spriteUID;
	//m_world[UID].wallRotation = rotation;
	addToUpdateList( UID );
}

/**
 * @brief Sets the wall sprite UID by flat tile ID (note: despite the method name, sets wallSpriteUID).
 * @param tileID Flat tile index.
 * @param spriteUID Sprite UID to assign.
 */
void World::setFloorSprite( unsigned int tileID, unsigned int spriteUID )
{
	m_world[tileID].wallSpriteUID = spriteUID;
	addToUpdateList( tileID );
}

/**
 * @brief Sets a job sprite on a tile by flat tile ID (delegates to Position-based overload).
 * @param tileID Flat tile index.
 * @param spriteUID Sprite UID for the job indicator.
 * @param rotation Sprite rotation.
 * @param floor True if this is a floor job, false for wall.
 * @param jobID ID of the associated job.
 * @param busy True if the job is currently being worked.
 */
void World::setJobSprite( unsigned int tileID, unsigned int spriteUID, unsigned char rotation, bool floor, unsigned int jobID, bool busy )
{
	Position pos( tileID );
	setJobSprite( pos, spriteUID, rotation, floor, jobID, busy );
}

/**
 * @brief Sets a job sprite on a tile, updating job flags and the job sprites map.
 * @param pos World position.
 * @param spriteUID Sprite UID for the job indicator.
 * @param rotation Sprite rotation.
 * @param floor True if this is a floor job, false for wall.
 * @param jobID ID of the associated job.
 * @param busy True if the job is currently being worked.
 */
void World::setJobSprite( Position pos, unsigned int spriteUID, unsigned char rotation, bool floor, unsigned int jobID, bool busy )
{
	Tile& tile = getTile( pos );
	setTileFlag( pos, floor ? TileFlag::TF_JOB_FLOOR : TileFlag::TF_JOB_WALL );
	if ( busy )
	{
		if ( floor )
			setTileFlag( pos, TileFlag::TF_JOB_BUSY_FLOOR );
		else
			setTileFlag( pos, TileFlag::TF_JOB_BUSY_WALL );
	}
	else
	{
		if ( floor )
			clearTileFlag( pos, TileFlag::TF_JOB_BUSY_FLOOR );
		else
			clearTileFlag( pos, TileFlag::TF_JOB_BUSY_WALL );
	}

	QVariantMap s;
	s.insert( "Pos", pos.toString() );
	s.insert( "Rot", rotation );
	s.insert( "JobID", jobID );
	s.insert( "SpriteUID", spriteUID );

	if ( !m_jobSprites.contains( pos.toInt() ) )
	{
		QVariantMap entry;
		m_jobSprites.insert( pos.toInt(), entry );
	}
	if ( floor )
	{
		m_jobSprites[pos.toInt()].insert( "Floor", s );
	}
	else
	{
		m_jobSprites[pos.toInt()].insert( "Wall", s );
	}

	addToUpdateList( pos.toInt() );
}

/**
 * @brief Removes a job sprite from a tile (floor or wall layer) and clears the associated job flags.
 * @param pos World position.
 * @param floor True to clear the floor job sprite, false for wall.
 */
void World::clearJobSprite( Position pos, bool floor )
{
	if ( m_jobSprites.contains( pos.toInt() ) )
	{
		if ( floor )
		{
			m_jobSprites[pos.toInt()].remove( "Floor" );
			clearTileFlag( pos, TileFlag::TF_JOB_FLOOR + TileFlag::TF_JOB_BUSY_FLOOR );
		}
		else
		{
			m_jobSprites[pos.toInt()].remove( "Wall" );
			clearTileFlag( pos, TileFlag::TF_JOB_WALL + TileFlag::TF_JOB_BUSY_WALL );
		}
	}
	if ( m_jobSprites[pos.toInt()].isEmpty() )
	{
		m_jobSprites.remove( pos.toInt() );
	}
	addToUpdateList( pos.toInt() );
}

/**
 * @brief Sets tile flags on the tile at the given coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param flag TileFlag bits to add.
 */
void World::setTileFlag( unsigned short x, unsigned short y, unsigned short z, TileFlag flag )
{
	Position pos( x, y, z );
	setTileFlag( pos, flag );
}

/**
 * @brief Adds tile flags to the tile at the given position, updating region map if walkability changes.
 * @param pos World position.
 * @param flag TileFlag bits to add.
 */
void World::setTileFlag( Position pos, TileFlag flag )
{
	unsigned int tid = pos.toInt();
	Tile& tile       = getTile( tid );
	tile.flags += flag;

	if ( flag & TileFlag::TF_WALKABLE )
	{
		m_regionMap.updatePosition( pos );
	}
	addToUpdateList( pos.toInt() );
}

/**
 * @brief Clears tile flags from the tile at the given position, removing designations if walkability is lost.
 * @param pos World position.
 * @param flag TileFlag bits to remove.
 */
void World::clearTileFlag( Position pos, TileFlag flag )
{
	unsigned int tid = pos.toInt();
	Tile& tile       = getTile( tid );
	tile.flags -= flag;

	if ( flag & TileFlag::TF_WALKABLE )
	{
		m_regionMap.updatePosition( pos );

		g->fm()->removeTile( pos, true, true, false );
		g->spm()->removeTile( pos );
		g->rm()->removeTile( pos );
	}
	addToUpdateList( pos.toInt() );
}

/**
 * @brief Recalculates the fence sprite at the given position based on adjacent fence connections.
 * @param pos World position of the fence to update.
 */
void World::updateFenceSprite( Position pos )
{
	QVariantMap constr;
	if ( m_wallConstructions.contains( pos.toInt() ) )
	{
		constr = m_wallConstructions.value( pos.toInt() );
	}
	else
	{
		return;
	}
	QString suffix          = "Rot";
	QString constructionSID = constr.value( "ConstructionID" ).toString();

	if ( m_wallConstructions.contains( pos.northOf().toInt() ) )
	{
		QVariantMap nc = m_wallConstructions.value( pos.northOf().toInt() );
		if ( nc.value( "ConstructionID" ).toString() == constructionSID )
		{
			suffix += "N";
		}
	}

	if ( m_wallConstructions.contains( pos.eastOf().toInt() ) )
	{
		QVariantMap nc = m_wallConstructions.value( pos.eastOf().toInt() );
		if ( nc.value( "ConstructionID" ).toString() == constructionSID )
		{
			suffix += "E";
		}
	}
	if ( m_wallConstructions.contains( pos.southOf().toInt() ) )
	{
		QVariantMap nc = m_wallConstructions.value( pos.southOf().toInt() );
		if ( nc.value( "ConstructionID" ).toString() == constructionSID )
		{
			suffix += "S";
		}
	}
	if ( m_wallConstructions.contains( pos.westOf().toInt() ) )
	{
		QVariantMap nc = m_wallConstructions.value( pos.westOf().toInt() );
		if ( nc.value( "ConstructionID" ).toString() == constructionSID )
		{
			suffix += "W";
		}
	}

	auto spl = DB::selectRows( "Constructions_Sprites", "ID", constructionSID );
	for ( auto sp : spl )
	{
		Position offset;
		offset = Position( sp.value( "Offset" ).toString() );
		Position constrPos( pos + offset );
		unsigned int tid       = constrPos.toInt();
		Tile& tile             = getTile( tid );
		QString materialID     = DBH::materialSID( tile.wallMaterial );
		QString spriteSID      = sp.value( "SpriteID" ).toString() + suffix;
		unsigned int spriteUID = g->sf()->createSprite( spriteSID, { materialID } )->uID;

		tile.wallSpriteUID = spriteUID;
	}
	addToUpdateList( pos );
}

/**
 * @brief Recalculates the pipe sprite at the given position based on adjacent pipe connections.
 * @param pos World position of the pipe to update.
 */
void World::updatePipeSprite( Position pos )
{
	QVariantMap constr;
	if ( m_wallConstructions.contains( pos.toInt() ) )
	{
		constr = m_wallConstructions.value( pos.toInt() );
	}
	else
	{
		return;
	}
	QString suffix = "Rot";

	unsigned int itemUID = constr.value( "Item" ).toUInt();
	QString itemSID      = g->inv()->itemSID( itemUID );

	if ( itemSID.startsWith( "Pump" ) )
	{
		return;
	}

	if ( getTileFlag( pos.northOf() ) & TileFlag::TF_PIPE )
	{
		suffix += "N";
	}
	if ( getTileFlag( pos.eastOf() ) & TileFlag::TF_PIPE )
	{
		suffix += "E";
	}
	if ( getTileFlag( pos.southOf() ) & TileFlag::TF_PIPE )
	{
		suffix += "S";
	}
	if ( getTileFlag( pos.westOf() ) & TileFlag::TF_PIPE )
	{
		suffix += "W";
	}

	auto spl = DB::selectRows( "Constructions_Sprites", "ID", itemSID );
	for ( auto sp : spl )
	{
		Position offset;
		offset = Position( sp.value( "Offset" ).toString() );
		Position constrPos( pos + offset );
		unsigned int tid       = constrPos.toInt();
		Tile& tile             = getTile( tid );
		QString materialID     = DBH::materialSID( tile.wallMaterial );
		QString spriteSID      = sp.value( "SpriteID" ).toString() + suffix;
		unsigned int spriteUID = g->sf()->createSprite( spriteSID, { materialID } )->uID;

		tile.wallSpriteUID = spriteUID;
	}
	addToUpdateList( pos );
}

/**
 * @brief Restores a sprite UID onto a tile from saved data (either floor or wall layer).
 * @param vals Map containing "Pos", "IsFloor", and "UID" keys.
 */
void World::addLoadedSprites( QVariantMap vals )
{
	Tile& tile = getTile( Position( vals.value( "Pos" ) ) );

	if ( vals.value( "IsFloor" ).toBool() )
	{
		tile.floorSpriteUID = vals.value( "UID" ).toUInt();
	}
	else
	{
		tile.wallSpriteUID = vals.value( "UID" ).toUInt();
	}
	//Util::string2Tile( tile, vals.value( "OldTileVals" ).toString() );
}

/**
 * @brief Moves all items off a tile if it has become blocked (wall or unwalkable).
 * @param pos Position to check for items.
 * @param to Destination position to move items to.
 */
void World::expelTileItems( Position pos, Position& to )
{
	Tile& tile = getTile( pos );
	//do we even have to move anything?
	PositionEntry pe;
	if ( g->inv()->getObjectsAtPosition( pos, pe ) )
	{
		//check if tile is now blocked for items and creatures
		if ( tile.wallType & WallType::WT_MOVEBLOCKING || !isWalkable( pos ) )
		{
			for ( auto i : pe )
			{
				g->inv()->moveItemToPos( i, to );
			}
		}
	}
}

/**
 * @brief Force-moves all gnomes and animals off a tile if it has become blocked.
 * @param pos Position to check for creatures.
 * @param to Destination position to move creatures to.
 */
void World::expelTileInhabitants( Position pos, Position& to )
{
	//qDebug() << "expel from " << pos.toString();
	Tile& tile = getTile( pos );
	// check if someone is on the tile
	if ( m_creaturePositions.contains( pos.toInt() ) )
	{
		//check if tile is now blocked for items and creatures
		if ( (bool)( tile.wallType & WallType::WT_MOVEBLOCKING ) || !isWalkable( pos ) )
		{
			g->gm()->forceMoveGnomes( pos, to );
			g->cm()->forceMoveAnimals( pos, to );
		}
	}
}

/**
 * @brief Plants a tree at the given position and sets its wall sprite.
 * @param pos World position.
 * @param type Tree type string ID from the Plants database.
 * @param fullyGrown If true, the tree starts fully grown.
 */
void World::plantTree( Position pos, QString type, bool fullyGrown )
{
	Plant plant_( pos, type, fullyGrown, g );
	m_plants.insert( pos.toInt(), plant_ );

	getTile( pos ).wallSpriteUID = m_plants[pos.toInt()].getSprite()->uID;
	addToUpdateList( pos );
}

/**
 * @brief Plants a mushroom at the given position and sets its wall sprite and transparency flag.
 * @param pos World position.
 * @param type Mushroom type string ID from the Plants database.
 * @param fullyGrown If true, the mushroom starts fully grown.
 */
void World::plantMushroom( Position pos, QString type, bool fullyGrown )
{
	Plant plant_( pos, type, fullyGrown, g );
	m_plants.insert( pos.toInt(), plant_ );

	Tile& tile = getTile( pos );
	if( plant_.hasAlpha() )
	{
		setTileFlag( pos, TileFlag::TF_TRANSPARENT );
	}
	tile.wallSpriteUID = m_plants[pos.toInt()].getSprite()->uID;
	addToUpdateList( pos );
}

/**
 * @brief Plants a crop/plant at the given position, matching the base item's material to a plant type.
 * @param pos World position.
 * @param baseItem Inventory UID of the seed/base item whose material determines the plant type.
 */
void World::plant( Position pos, unsigned int baseItem )
{
	QStringList plants = DB::ids( "Plants", "Type", "Plant" );
	for ( auto plant : plants )
	{
		if ( DB::select( "Material", "Plants", plant ).toString() == g->inv()->materialSID( baseItem ) )
		{
			Plant plant_( pos, plant, false, g );
			m_plants.insert( pos.toInt(), plant_ );
			getTile( pos ).wallSpriteUID = m_plants[pos.toInt()].getSprite()->uID;
			return;
		}
	}
}

/**
 * @brief Adds an already-constructed Plant to the plants map.
 * @param plant The Plant object to insert (keyed by its position).
 */
void World::addPlant( Plant plant )
{
	Position pos = plant.getPos();
	m_plants.insert( pos.toInt(), plant );
	//getTile( pos ).wallSpriteUID = m_plants[pos.toInt()].getSprite();
}

/**
 * @brief Removes the plant at the given position, clearing its wall sprite.
 * @param pos World position of the plant to remove.
 */
void World::removePlant( Position pos )
{
	if ( m_plants.contains( pos.toInt() ) )
	{
		getTile( pos ).wallType      = WallType::WT_NOWALL;
		getTile( pos ).wallSpriteUID = 0;
		m_plants.remove( pos.toInt() );
		getTile( pos ).wallSpriteUID = 0;
		addToUpdateList( pos );
	}
}

/**
 * @brief Removes a plant from the plants map by its Plant object.
 * @param plant The Plant to remove (looked up by position).
 */
void World::removePlant( Plant plant )
{
	m_plants.remove( plant.getPos().toInt() );
}

/**
 * @brief Reduces the growth level of the plant at the given position by one step.
 * @param pos World position of the plant.
 * @return True if the plant was fully reduced and removed, false otherwise.
 */
bool World::reduceOneGrowLevel( Position pos )
{
	if ( m_plants.contains( pos.toInt() ) )
	{
		if ( m_plants[pos.toInt()].reduceOneGrowLevel() )
		{
			removePlant( pos );
			return true;
		}
	}
	return false;
}

/**
 * @brief Unregisters a creature from its current position in the creature positions map.
 * @param pos World position the creature is leaving.
 * @param creatureID Unique ID of the creature to remove.
 */
void World::removeCreatureFromPosition( Position pos, unsigned int creatureID )
{
	if ( m_creaturePositions.contains( pos.toInt() ) )
	{
		if ( m_creaturePositions[pos.toInt()].size() == 1 )
		{
			m_creaturePositions.remove( pos.toInt() );
			g->mcm()->updateCreaturesAtPos( pos, 0 );
		}
		else
		{
			QList<unsigned int>& cl = m_creaturePositions[pos.toInt()];
			cl.removeAll( creatureID );
			g->mcm()->updateCreaturesAtPos( pos, m_creaturePositions[pos.toInt()].size() );
		}
		addToUpdateList( pos );
	}
}

/**
 * @brief Registers a creature at a new position in the creature positions map.
 * @param pos World position the creature is entering.
 * @param creatureID Unique ID of the creature to register.
 */
void World::insertCreatureAtPosition( Position pos, unsigned int creatureID )
{
	if ( m_creaturePositions.contains( pos.toInt() ) )
	{
		m_creaturePositions[pos.toInt()].push_back( creatureID );
		g->mcm()->updateCreaturesAtPos( pos, m_creaturePositions[pos.toInt()].size() );
	}
	else
	{
		QList<unsigned int> cl( { creatureID } );
		m_creaturePositions.insert( pos.toInt(), cl );
		g->mcm()->updateCreaturesAtPos( pos, 1 );
	}
	addToUpdateList( pos );
}

/**
 * @brief Tick-based grass growth simulation: randomly grows grass on candidate dirt tiles near existing grass.
 */
void World::processGrass()
{
	QList<Position> toRemove;
	unsigned short dirtUID = Global::dirtUID;
	for ( auto pi : m_grassCandidatePositions )
	{
		if ( m_grass.contains( pi ) )
		{
			toRemove.append( pi );
		}
		else
		{
			Position p( pi );

			if ( rand() % 300 > 298 )
			{
				Tile& tile = getTile( p );
				if ( tile.floorMaterial == dirtUID && tile.floorType & FT_SOLIDFLOOR && tile.flags & TileFlag::TF_SUNLIGHT )
				{
					if ( tile.wallType & WT_RAMP && tile.wallMaterial == dirtUID )
					{
						setTileFlag( p, TileFlag::TF_GRASS );
						createRamp( p );
						addToUpdateList( p );

						auto pa = p.aboveOf();
						addToUpdateList( pa );

						isGrassCandidate( pa.northOf() );
						isGrassCandidate( pa.eastOf() );
						isGrassCandidate( pa.southOf() );
						isGrassCandidate( pa.westOf() );
					}
					else
					{
						createGrass( p );
						addToUpdateList( p );
					}
				}
				toRemove.append( p );

				isGrassCandidate( p.northOf() );
				isGrassCandidate( p.eastOf() );
				isGrassCandidate( p.southOf() );
				isGrassCandidate( p.westOf() );
			}
		}
	}
	for ( auto p : toRemove )
	{
		m_grassCandidatePositions.remove( p.toInt() );
	}
}

/**
 * @brief Scans all tiles to build the initial grass set and candidate positions for grass growth.
 */
void World::initGrassUpdateList()
{
	m_grass.clear();
	m_grassCandidatePositions.clear();

	for ( int z = m_dimZ - 2; z >= 0; --z )
	{
		for ( int y = 0; y < m_dimY; ++y )
		{
			for ( int x = 0; x < m_dimX; ++x )
			{
				Tile& here = getTile( x, y, z );
				if ( (bool)( here.flags & TileFlag::TF_GRASS ) )
				{
					Position pos( x, y, z );
					m_grass.insert( pos );
				}
			}
		}
	}

	for ( auto p : m_grass )
	{
		isGrassCandidate( p.northOf() );
		isGrassCandidate( p.eastOf() );
		isGrassCandidate( p.southOf() );
		isGrassCandidate( p.westOf() );
	}
}

/**
 * @brief Evaluates whether a tile is a valid candidate for grass growth and adds it to the candidate set.
 * @param pos World position to evaluate.
 */
void World::isGrassCandidate( Position pos )
{
	Tile& tile = getTile( pos );

	if ( ( tile.floorType & FT_SOLIDFLOOR ) && ( tile.floorMaterial == Global::dirtUID ) && ( ( tile.wallType == WT_NOWALL ) || ( tile.wallType == WT_RAMP ) ) && !m_grass.contains( pos.toInt() ) )
	{
		m_grassCandidatePositions.insert( pos.toInt() );
	}
}

/**
 * @brief Public wrapper to add a position to the grass growth candidate set.
 * @param pos World position to evaluate as a grass candidate.
 */
void World::addGrassCandidate( Position pos )
{
	isGrassCandidate( pos );
}

/**
 * @brief Removes grass from a tile, resetting its vegetation level and floor sprite to bare dirt.
 * @param pos World position to remove grass from.
 */
void World::removeGrass( Position pos )
{
	clearTileFlag( pos, TileFlag::TF_GRASS );

	if ( m_grass.contains( pos.toInt() ) )
	{
		m_grass.remove( pos.toInt() );

		Tile& tile           = getTile( pos );
		tile.vegetationLevel = 0;
		QString materialSID  = DBH::materialSID( tile.floorMaterial );

		if ( tile.floorType & FT_SOLIDFLOOR && materialSID == "Dirt" )
		{
			//if( Global::debugMode ) qDebug() << "add grass candidate at " << pos.toString();
			tile.floorSpriteUID = g->sf()->createSprite( "RoughFloor", { "Dirt" } )->uID;
			m_grassCandidatePositions.insert( pos.toInt() );
		}
	}

	addToUpdateList( pos );
}

/**
 * @brief Creates grass on a tile, setting the grass sprite, flag, and full vegetation level.
 * @param pos World position to create grass on. Skips tilled tiles.
 */
void World::createGrass( Position pos )
{
	auto tf = getTileFlag( pos );
	if ( tf & TileFlag::TF_TILLED )
	{
		return;
	}

	Tile& tile          = getTile( pos );
	tile.floorSpriteUID = g->sf()->createSprite( "GrassWithDetail", { "Grass", "None" } )->uID;

	m_grass.insert( pos );
	setTileFlag( pos, TileFlag::TF_GRASS );
	tile.vegetationLevel = 100;
}

/**
 * @brief Adds water at a position with the given fluid level if not already tracked.
 * @param pos World position.
 * @param level Initial fluid level (1-10).
 */
void World::addWater( Position pos, unsigned char level )
{
	if ( isWaterBoundary( pos, m_dimX, m_dimY, m_dimZ ) )
	{
		return;
	}
	Tile& tile = getTile( pos );
	if ( tile.wallType & WallType::WT_MOVEBLOCKING )
	{
		return;
	}
	if ( !m_water.count( pos.toInt() ) )
	{
		m_water.insert( pos.toInt() );
		m_activeWater.insert( pos.toInt() );

		tile.fluidLevel = level;
		tile.flags += TileFlag::TF_WATER;
		addToUpdateList( pos );
	}
}

/**
 * @brief Changes the fluid level at a position by the given delta, handling pressure overflow.
 * @param pos World position.
 * @param diff Amount to add (positive) or subtract (negative) from the fluid level.
 */
void World::changeFluidLevel( Position pos, int diff )
{
	if ( isWaterBoundary( pos, m_dimX, m_dimY, m_dimZ ) )
	{
		return;
	}
	Tile& tile         = getTile( pos );
	if ( tile.wallType & WallType::WT_MOVEBLOCKING )
	{
		// A wall, tree, or other blocking object is a physical water boundary.
		// Clear any stale fluid that was present before the object was placed.
		if ( tile.fluidLevel > 0 || tile.pressure > 0 || ( tile.flags & TileFlag::TF_WATER ) )
		{
			tile.fluidLevel = 0;
			tile.pressure   = 0;
			tile.flow       = WF_NOFLOW;
			tile.flags     -= TileFlag::TF_WATER;
			m_water.erase( pos.toInt() );
			addToUpdateList( pos );
		}
		return;
	}
	constexpr int fluidCapacity = 10;
	constexpr int maxStoredMass = fluidCapacity + 255;
	const int effectiveLevel = qBound( 0, (int)tile.fluidLevel + (int)tile.pressure + diff, maxStoredMass );

	tile.fluidLevel = qMin( fluidCapacity, effectiveLevel );
	tile.pressure   = qBound( 0, effectiveLevel - fluidCapacity, 255 );

	if ( effectiveLevel > 0 )
	{
		m_water.insert( pos.toInt() );
		m_activeWater.insert( pos.toInt() );
		tile.flags += TileFlag::TF_WATER;
	}
	else
	{
		m_water.erase( pos.toInt() );
		m_activeWater.remove( pos.toInt() );
		tile.flow = WF_NOFLOW;
		tile.flags -= TileFlag::TF_WATER;
	}
	addToUpdateList( pos );
}

/**
 * @brief Registers an aquifer source tile that generates water each tick.
 * @param pos World position of the aquifer.
 */
void World::addAquifier( Position pos )
{
	if ( isWaterBoundary( pos, m_dimX, m_dimY, m_dimZ ) )
	{
		return;
	}
	if ( getTile( pos ).wallType & WallType::WT_MOVEBLOCKING )
	{
		return;
	}
	m_aquifiers.append( pos );
	m_water.insert( pos.toInt() );
	m_activeWater.insert( pos.toInt() );
	Tile& tile = getTile( pos );
	tile.flags += TileFlag::TF_WATER;
	tile.flags += TileFlag::TF_AQUIFIER;
	addToUpdateList( pos );
}

/**
 * @brief Registers a deaquifier drain tile that removes water each tick.
 * @param pos World position of the deaquifier.
 */
void World::addDeaquifier( Position pos )
{
	m_deaquifiers.append( pos );
	Tile& tile = getTile( pos );
	tile.flags += TileFlag::TF_DEAQUIFIER;
}

/**
 * @brief Per-tick water simulation: processes aquifiers and deaquifiers, then runs flow simulation.
 */
void World::processWater()
{
	// Batch updates
	QVector<unsigned int> waterUpdates;
	// Expecting to see every tile again
	waterUpdates.reserve( m_aquifiers.size() + m_deaquifiers.size() );

	// Add / remove 1 water per tick and aquifier / deaquifier
	for ( const auto& pos : m_aquifiers )
	{
		if ( isWaterBoundary( pos, m_dimX, m_dimY, m_dimZ ) )
		{
			continue;
		}
		Tile& tile = getTile( pos );
		if ( tile.wallType & WallType::WT_MOVEBLOCKING )
		{
			tile.fluidLevel = 0;
			tile.pressure   = 0;
			tile.flow       = WF_NOFLOW;
			tile.flags     -= TileFlag::TF_WATER;
			m_water.erase( pos.toInt() );
			m_activeWater.remove( pos.toInt() );
			continue;
		}
		if ( tile.pressure == 0 && tile.fluidLevel < 10)
		{
			tile.fluidLevel++;
			tile.flags += TileFlag::TF_WATER;
			waterUpdates.append( pos.toInt() );
		}
		m_water.insert( pos.toInt() );
		wakeWaterAround( pos );
	}
	for ( const auto& pos : m_deaquifiers )
	{
		Tile& tile = getTile( pos );
		if ( tile.fluidLevel > 0 )
		{
			if ( tile.pressure > 0 )
			{
				tile.pressure--;
			}
			else
			{
				tile.fluidLevel--;
			}
			if ( tile.fluidLevel == 0 )
			{
				tile.flow = WF_NOFLOW;
				tile.flags -= TileFlag::TF_WATER;
				m_water.erase( pos.toInt() );
			}
			wakeWaterAround( pos );
			waterUpdates.append( pos.toInt() );
		}
	}

	// Batch submit water tile updates
	addToUpdateList( waterUpdates );
	waterUpdates.clear();

	processWaterFlow();
}

namespace
{
constexpr unsigned int invalidWaterTile = std::numeric_limits<unsigned int>::max();
constexpr int fluidCapacity            = 10;
constexpr int maxStoredFluidMass       = fluidCapacity + 255;
constexpr int maxTransferPerEdge       = 1;

/**
 * @brief Bounds-safe neighbors for the flat world array.
 *
 * The old implementation used zero as a sentinel (which is a real tile ID)
 * and had two-tile off-by-one errors on every horizontal edge.  Water uses
 * the complete world volume, so it must not use Position::valid(), which
 * intentionally excludes the outer x/y rows for other read-only queries.
 */
struct Neighbors
{
	Neighbors( unsigned int tileID, int dimX, int dimY, int dimZ )
	{
		const unsigned int pitchZ = static_cast<unsigned int>( dimX * dimY );
		const unsigned int z      = tileID / pitchZ;
		const unsigned int plane  = tileID % pitchZ;
		const unsigned int y      = plane / static_cast<unsigned int>( dimX );
		const unsigned int x      = plane % static_cast<unsigned int>( dimX );

		auto makeID = [=]( int neighborX, int neighborY, int neighborZ ) {
			if ( neighborX < 0 || neighborX >= dimX || neighborY < 0 || neighborY >= dimY || neighborZ < 0 || neighborZ >= dimZ )
			{
				return invalidWaterTile;
			}
			return static_cast<unsigned int>( neighborX + dimX * neighborY + pitchZ * neighborZ );
		};

		north = makeID( x, y - 1, z );
		south = makeID( x, y + 1, z );
		east  = makeID( x + 1, y, z );
		west  = makeID( x - 1, y, z );
		above = makeID( x, y, z + 1 );
		below = makeID( x, y, z - 1 );
	}

	unsigned int above = invalidWaterTile;
	unsigned int below = invalidWaterTile;
	unsigned int north = invalidWaterTile;
	unsigned int south = invalidWaterTile;
	unsigned int east  = invalidWaterTile;
	unsigned int west  = invalidWaterTile;
};

constexpr WaterFlow oppositeFlow( WaterFlow flow )
{
	switch ( flow )
	{
		case WF_NORTH:
			return WF_SOUTH;
		case WF_SOUTH:
			return WF_NORTH;
		case WF_EAST:
			return WF_WEST;
		case WF_WEST:
			return WF_EAST;
		case WF_UP:
			return WF_DOWN;
		case WF_DOWN:
			return WF_UP;
		default:
			return WF_NOFLOW;
	}
}
}

/**
 * @brief Wakes water at a changed topology cell and its six direct neighbours.
 *
 * Dry cells are intentionally allowed in the active set: one of their wet
 * neighbours may need to flow into them after a wall or floor is removed.
 */
void World::wakeWaterAround( Position pos )
{
	const unsigned int worldTileCount = static_cast<unsigned int>( m_world.size() );
	const unsigned int tileID = pos.toInt();
	if ( tileID >= worldTileCount )
		return;

	const Neighbors adjacent( tileID, m_dimX, m_dimY, m_dimZ );
	for ( const unsigned int id : { tileID, adjacent.above, adjacent.below, adjacent.north, adjacent.south, adjacent.east, adjacent.west } )
	{
		if ( id != invalidWaterTile && id < worldTileCount && !isWaterBoundary( id, m_dimX, m_dimY, m_dimZ ) )
			m_activeWater.insert( id );
	}
}

/**
 * @brief Simulates water flow with a deterministic, mass-conserving grid pass.
 *
 * Each water cell is treated as a small storage cell with a capacity of ten
 * surface units.  A tick first moves one unit down when the receiving cell has
 * room, then equalizes horizontal gradients one cell-pair at a time.  All
 * transfers are planned against a snapshot and committed together, so a tile
 * cannot be drained multiple times just because several neighbors selected it
 * in the same frame.
 */
void World::processWaterFlowLegacy()
{
	QHash<unsigned int, int> mass;
	QHash<unsigned int, int> remaining;
	QHash<unsigned int, int> delta;
	QHash<unsigned int, WaterFlow> flowFlags;
	QSet<unsigned int> sourceSet;
	QSet<unsigned int> touched;
	QSet<unsigned int> invalidTracked;
	QVector<unsigned int> activeWater;

	const unsigned int worldTileCount = static_cast<unsigned int>( m_world.size() );
	activeWater.reserve( static_cast<int>( m_water.size() ) );

	auto storedMass = []( const Tile& tile ) {
		return qBound( 0, (int)tile.fluidLevel + (int)tile.pressure, maxStoredFluidMass );
	};

	// Take a stable snapshot.  A source is allowed to send only its mass at
	// the beginning of the tick; incoming mass becomes available next tick.
	for ( const unsigned int currentPos : m_water )
	{
		if ( currentPos >= worldTileCount )
		{
			invalidTracked.insert( currentPos );
			continue;
		}

		Tile& here = m_world[currentPos];
		touched.insert( currentPos );
		if ( isWaterBoundary( currentPos, m_dimX, m_dimY, m_dimZ ) || (bool)( here.wallType & WallType::WT_MOVEBLOCKING ) || storedMass( here ) == 0 )
		{
			here.flow       = WF_NOFLOW;
			here.pressure   = 0;
			here.fluidLevel = 0;
			here.flags -= TileFlag::TF_WATER;
			invalidTracked.insert( currentPos );
			continue;
		}

		const int currentMass = storedMass( here );
		mass.insert( currentPos, currentMass );
		remaining.insert( currentPos, currentMass );
		sourceSet.insert( currentPos );
		activeWater.append( currentPos );
	}

	for ( const unsigned int currentPos : invalidTracked )
	{
		m_water.erase( currentPos );
	}

	auto passable = [&]( unsigned int tileID ) {
		return tileID != invalidWaterTile && tileID < worldTileCount && !isWaterBoundary( tileID, m_dimX, m_dimY, m_dimZ ) && !(bool)( m_world[tileID].wallType & WallType::WT_MOVEBLOCKING );
	};

	auto ensureMass = [&]( unsigned int tileID ) {
		if ( tileID == invalidWaterTile || tileID >= worldTileCount )
		{
			return 0;
		}
		if ( !mass.contains( tileID ) )
		{
			const int tileMass = storedMass( m_world[tileID] );
			mass.insert( tileID, tileMass );
			remaining.insert( tileID, tileMass );
		}
		return mass.value( tileID );
	};

	auto addTransfer = [&]( unsigned int source, unsigned int destination, int requested, WaterFlow direction ) {
		if ( !passable( source ) || !passable( destination ) || requested <= 0 )
		{
			return 0;
		}

		ensureMass( destination );
		const int available = remaining.value( source, 0 );
		const int currentDestinationMass = mass.value( destination, 0 ) + delta.value( destination, 0 );
		const int destinationRoom = qMax( 0, fluidCapacity - currentDestinationMass );
		const int amount = qMin( requested, qMin( available, destinationRoom ) );
		if ( amount <= 0 )
		{
			return 0;
		}

		remaining[source] = available - amount;
		delta[source] = delta.value( source, 0 ) - amount;
		delta[destination] = delta.value( destination, 0 ) + amount;
		flowFlags[source] = flowFlags.value( source, WF_NOFLOW ) + direction;
		touched.insert( source );
		touched.insert( destination );
		return amount;
	};

	// Gravity is directional and gets first claim on available capacity.
	for ( const unsigned int currentPos : activeWater )
	{
		const Neighbors neighbors( currentPos, m_dimX, m_dimY, m_dimZ );
		const Tile& here = m_world[currentPos];
		if ( !(bool)( here.floorType & FloorType::FT_SOLIDFLOOR ) && passable( neighbors.below ) )
		{
			addTransfer( currentPos, neighbors.below, maxTransferPerEdge, WF_DOWN );
		}
	}

	auto processHorizontalPair = [&]( unsigned int source, unsigned int neighbor, WaterFlow direction ) {
		if ( !passable( neighbor ) )
		{
			return;
		}

		const bool neighborIsSource = sourceSet.contains( neighbor );
		// Every active-active pair is processed once, in flat-ID order.  This
		// removes the old random direction reversal and makes saves reproducible.
		if ( neighborIsSource && source > neighbor )
		{
			return;
		}

		ensureMass( neighbor );
		const int sourceMass = remaining.value( source, 0 );
		const int neighborMass = mass.value( neighbor, 0 );
		if ( sourceMass > neighborMass + 1 )
		{
			addTransfer( source, neighbor, maxTransferPerEdge, direction );
		}
		else if ( neighborIsSource && remaining.value( neighbor, 0 ) > mass.value( source, 0 ) + 1 )
		{
			addTransfer( neighbor, source, maxTransferPerEdge, oppositeFlow( direction ) );
		}
	};

	// Equalize the horizontal surface without ever exceeding receiver capacity.
	for ( const unsigned int currentPos : activeWater )
	{
		const Neighbors neighbors( currentPos, m_dimX, m_dimY, m_dimZ );
		processHorizontalPair( currentPos, neighbors.north, WF_NORTH );
		processHorizontalPair( currentPos, neighbors.south, WF_SOUTH );
		processHorizontalPair( currentPos, neighbors.east, WF_EAST );
		processHorizontalPair( currentPos, neighbors.west, WF_WEST );

		// Pressure is an overflow channel, not a reason to teleport an entire
		// column.  Let only one stored unit rise per tick when a cell is overfull.
		const unsigned int above = neighbors.above;
		if ( remaining.value( currentPos, 0 ) > fluidCapacity && passable( above ) && !(bool)( m_world[above].floorType & FloorType::FT_SOLIDFLOOR ) )
		{
			addTransfer( currentPos, above, maxTransferPerEdge, WF_UP );
		}
	}

	// Commit every touched cell from the same snapshot.  This is where the
	// previous flood-then-drain implementation could underflow fluidLevel or
	// apply several competing transfers to one tile.
	QVector<unsigned int> updateIDs = touched.values().toVector();
	std::sort( updateIDs.begin(), updateIDs.end() );
	QVector<unsigned int> waterUpdates;
	waterUpdates.reserve( updateIDs.size() );
	for ( const unsigned int tileID : updateIDs )
	{
		if ( tileID >= worldTileCount )
		{
			continue;
		}

		Tile& tile = m_world[tileID];
		const TileFlag oldFlags = tile.flags;
		const unsigned char oldFluid = tile.fluidLevel;
		const unsigned char oldPressure = tile.pressure;
		const WaterFlow oldFlow = tile.flow;
		const int finalMass = qBound( 0, mass.value( tileID, 0 ) + delta.value( tileID, 0 ), maxStoredFluidMass );

		if ( !passable( tileID ) || finalMass == 0 )
		{
			tile.fluidLevel = 0;
			tile.pressure   = 0;
			tile.flow       = WF_NOFLOW;
			tile.flags -= TileFlag::TF_WATER;
			m_water.erase( tileID );
		}
		else
		{
			tile.fluidLevel = qMin( fluidCapacity, finalMass );
			tile.pressure   = qBound( 0, finalMass - fluidCapacity, 255 );
			tile.flow       = flowFlags.value( tileID, WF_NOFLOW );
			tile.flags += TileFlag::TF_WATER;
			m_water.insert( tileID );
		}

		if ( oldFlags != tile.flags || oldFluid != tile.fluidLevel || oldPressure != tile.pressure || oldFlow != tile.flow )
		{
			waterUpdates.append( tileID );
		}
	}

	addToUpdateList( waterUpdates );
}

/**
 * @brief Applies the extracted deterministic water solver to World storage.
 *
 * The solver works entirely in mass units. This adapter deliberately keeps
 * the legacy fluidLevel/pressure split and TF_WATER/m_water bookkeeping so
 * existing saves and gameplay callers remain compatible.
 */
void World::processWaterFlow()
{
	const int worldTileCount = m_world.size();
	if ( worldTileCount <= 0 || m_activeWater.isEmpty() )
		return;

	constexpr int surfaceCapacity = 10;
	constexpr int maxStoredMass = surfaceCapacity + 255;
	QSet<unsigned int> sparseIDs = m_activeWater;
	for ( const unsigned int tileID : std::as_const( m_activeWater ) )
	{
		if ( tileID >= static_cast<unsigned int>( worldTileCount ) )
			continue;
		const Neighbors adjacent( tileID, m_dimX, m_dimY, m_dimZ );
		for ( const unsigned int neighbor : { adjacent.above, adjacent.below, adjacent.north, adjacent.south, adjacent.east, adjacent.west } )
		{
			if ( neighbor != invalidWaterTile && neighbor < static_cast<unsigned int>( worldTileCount ) )
				sparseIDs.insert( neighbor );
		}
	}

	QHash<unsigned int, WaterFlowCell> cells;
	cells.reserve( sparseIDs.size() );
	for ( const unsigned int tileID : std::as_const( sparseIDs ) )
	{
		const Tile& tile = m_world[static_cast<int>( tileID )];
		cells.insert( tileID, {
			qBound( 0, static_cast<int>( tile.fluidLevel ) + static_cast<int>( tile.pressure ), maxStoredMass ),
			static_cast<bool>( tile.wallType & WallType::WT_MOVEBLOCKING ),
			static_cast<bool>( tile.floorType & FloorType::FT_SOLIDFLOOR ),
			isWaterBoundary( tileID, m_dimX, m_dimY, m_dimZ )
		} );
	}

	const WaterFlowConfig config { surfaceCapacity, maxStoredMass, 1 };
	const WaterFlowResult flow = solveWaterFlow( cells, m_dimX, m_dimY, m_dimZ, m_activeWater, config );
	m_activeWater = flow.nextActive;

	QVector<unsigned int> touched = flow.touched.values().toVector();
	std::sort( touched.begin(), touched.end() );
	QVector<unsigned int> waterUpdates;
	waterUpdates.reserve( touched.size() );

	for ( const unsigned int tileID : touched )
	{
		if ( tileID >= static_cast<unsigned int>( worldTileCount ) )
			continue;

		Tile& tile = m_world[static_cast<int>( tileID )];
		const TileFlag oldFlags = tile.flags;
		const unsigned char oldFluid = tile.fluidLevel;
		const unsigned char oldPressure = tile.pressure;
		const WaterFlow oldFlow = tile.flow;
		const int finalMass = flow.mass.value( tileID, 0 );

		if ( finalMass <= 0 || cells[tileID].boundary || cells[tileID].moveBlocking )
		{
			tile.fluidLevel = 0;
			tile.pressure = 0;
			tile.flow = WF_NOFLOW;
			tile.flags -= TileFlag::TF_WATER;
			m_water.erase( tileID );
		}
		else
		{
			tile.fluidLevel = static_cast<unsigned char>( qMin( surfaceCapacity, finalMass ) );
			tile.pressure = static_cast<unsigned char>( qBound( 0, finalMass - surfaceCapacity, 255 ) );
			tile.flow = flow.flow.value( tileID, WF_NOFLOW );
			tile.flags += TileFlag::TF_WATER;
			m_water.insert( tileID );
		}

		if ( oldFlags != tile.flags || oldFluid != tile.fluidLevel || oldPressure != tile.pressure || oldFlow != tile.flow )
			waterUpdates.append( tileID );
	}

	addToUpdateList( waterUpdates );
}

/**
 * @brief Removes any designation (stockpile, farm, grove, pasture, room, or no-pass zone) from a tile.
 * @param pos World position to clear designations from.
 */
void World::removeDesignation( Position pos )
{
	Tile& tile = getTile( pos );
	if ( tile.flags & TileFlag::TF_STOCKPILE )
	{
		g->spm()->removeTile( pos );
		addToUpdateList( pos );
	}
	else if ( tile.flags & ( TileFlag::TF_GROVE + TileFlag::TF_FARM + TileFlag::TF_PASTURE ) )
	{
		g->fm()->removeTile( pos, true, true, true );
		addToUpdateList( pos );
	}
	else if ( tile.flags & TileFlag::TF_ROOM )
	{
		g->rm()->removeTile( pos );
		addToUpdateList( pos );
	}
	else if ( tile.flags & TileFlag::TF_NOPASS )
	{
		clearTileFlag( pos, TileFlag::TF_NOPASS );
		m_regionMap.updatePosition( pos );
		addToUpdateList( pos );
	}
}

/**
 * @brief Mines out a wall tile, clearing it, making it walkable, updating ramps and lights, and discovering neighbors.
 * @param pos Position of the wall to mine.
 * @param workPosition Work position (unused in current implementation).
 * @return Pair of (wall material UID, embedded material UID) that were in the mined tile.
 */
QPair<unsigned short, unsigned short> World::mineWall( Position pos, Position& workPosition )
{
	Tile& tile                 = getTile( pos );
	unsigned short materialInt = tile.wallMaterial;
	unsigned short embeddedInt = tile.embeddedMaterial;

	if ( tile.wallType == WallType::WT_RAMP )
	{
		Tile& tileAbove          = getTile( pos.aboveOf() );
		tileAbove.floorType      = FloorType::FT_NOFLOOR;
		tileAbove.floorMaterial  = 0;
		tileAbove.floorSpriteUID = 0;
		clearTileFlag( Position( pos.aboveOf() ), TileFlag::TF_WALKABLE );
		removeGrass( pos );

		if ( m_creaturePositions.contains(  pos.aboveOf().toInt() ) )
		{
			g->gm()->forceMoveGnomes( pos.aboveOf(), pos );
		}
		g->inv()->gravity( pos.aboveOf() );
	}

	tile.wallType         = WallType::WT_NOWALL;
	tile.wallMaterial     = 0;
	tile.embeddedMaterial = 0;
	tile.wallSpriteUID    = 0;
	tile.itemSpriteUID    = 0;
	tile.flags += TileFlag::TF_WALKABLE;

	m_regionMap.updatePosition( pos );
	updateLightsInRange( pos );

	updateRampAtPos( pos.northOf() );
	updateRampAtPos( pos.eastOf() );
	updateRampAtPos( pos.southOf() );
	updateRampAtPos( pos.westOf() );

	discover( pos );
	wakeWaterAround( pos );
	if ( tile.wallType == WallType::WT_NOWALL )
		wakeWaterAround( pos.aboveOf() );

	QString ncd = QString::number( pos.toInt() );
	ncd += ";";
	ncd += Global::util->tile2String( tile );

	return { materialInt, embeddedInt };
}

/**
 * @brief Removes a wall tile (similar to mineWall but without ramp updates or creature displacement).
 * @param pos Position of the wall to remove.
 * @param workPosition Work position (unused in current implementation).
 * @return Pair of (wall material UID, embedded material UID) that were in the removed tile.
 */
QPair<unsigned short, unsigned short> World::removeWall( Position pos, Position& workPosition )
{
	Tile& tile                 = getTile( pos );
	unsigned short materialInt = tile.wallMaterial;
	unsigned short embeddedInt = tile.embeddedMaterial;

	if ( tile.wallType == WallType::WT_RAMP )
	{
		Tile& tileAbove          = getTile( pos.aboveOf() );
		tileAbove.floorType      = FloorType::FT_NOFLOOR;
		tileAbove.floorMaterial  = 0;
		tileAbove.floorSpriteUID = 0;
		clearTileFlag( Position( pos.aboveOf() ), TileFlag::TF_WALKABLE );
		removeGrass( pos );
	}

	tile.wallType         = WallType::WT_NOWALL;
	tile.wallMaterial     = 0;
	tile.embeddedMaterial = 0;
	tile.wallSpriteUID    = 0;
	tile.itemSpriteUID    = 0;
	tile.flags += TileFlag::TF_WALKABLE;

	m_regionMap.updatePosition( pos );
	updateLightsInRange( pos );

	discover( pos );
	wakeWaterAround( pos );

	QString ncd = QString::number( pos.toInt() );
	ncd += ";";
	ncd += Global::util->tile2String( tile );

	return { materialInt, embeddedInt };
}

/**
 * @brief Discovers (reveals) the tile at the given position and its neighbors.
 * @param pos World position to discover.
 */
void World::discover( Position pos )
{
	discover( pos.x, pos.y, pos.z );
	addToUpdateList( pos );
}

/**
 * @brief Clears the TF_UNDISCOVERED flag from a 3x3 area around the given coordinates.
 * @param x X coordinate center.
 * @param y Y coordinate center.
 * @param z Z coordinate.
 */
void World::discover( int x, int y, int z )
{
	clearTileFlag( Position( x - 1, y - 1, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x - 1, y, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x - 1, y + 1, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x, y - 1, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x, y, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x, y + 1, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x + 1, y - 1, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x + 1, y, z ), TileFlag::TF_UNDISCOVERED );
	clearTileFlag( Position( x + 1, y + 1, z ), TileFlag::TF_UNDISCOVERED );
}

/**
 * @brief Removes a ramp at the given position, clearing the ramp wall and the ramp-top floor above.
 * @param pos Position of the ramp to remove.
 * @param workPosition Work position (unused in current implementation).
 * @return Wall material UID of the removed ramp.
 */
unsigned short World::removeRamp( Position pos, Position workPosition )
{
	Tile& tile      = getTile( pos );
	Tile& tileAbove = getTile( pos.aboveOf() );

	unsigned short materialInt = tile.wallMaterial;
	// delete current ramp
	tileAbove.floorType      = FloorType::FT_NOFLOOR;
	tileAbove.floorMaterial  = 0;
	tileAbove.floorSpriteUID = 0;

	tile.wallType      = WallType::WT_NOWALL;
	tile.wallMaterial  = 0;
	tile.wallSpriteUID = 0;

	updateLightsInRange( pos );

	removeGrass( pos );
	wakeWaterAround( pos );
	wakeWaterAround( pos.aboveOf() );

	return materialInt;
}

/**
 * @brief Recalculates or removes a ramp at the given position based on current neighbor wall state.
 * @param pos Position of the ramp to update.
 */
void World::updateRampAtPos( Position pos )
{
	Tile& tile = getTile( pos );
	if ( !( tile.wallType & WallType::WT_RAMP ) )
	{
		return;
	}
	//	qDebug() << "ramp at " << pos.toString();
	Tile& tileBelow = getTile( pos.belowOf() );
	Tile& tileAbove = getTile( pos.aboveOf() );

	bool north = getTile( pos.northOf() ).wallType & WallType::WT_ROUGH;
	bool south = getTile( pos.southOf() ).wallType & WallType::WT_ROUGH;
	bool east  = getTile( pos.eastOf() ).wallType & WallType::WT_ROUGH;
	bool west  = getTile( pos.westOf() ).wallType & WallType::WT_ROUGH;

	int sum = (int)north + (int)south + (int)east + (int)west;
	if ( sum > 2 || sum == 0 )
	{
		// delete current ramp
		tileAbove.floorType      = FloorType::FT_NOFLOOR;
		tileAbove.floorMaterial  = 0;
		tileAbove.floorSpriteUID = 0;

		tile.wallType      = WallType::WT_NOWALL;
		tile.wallMaterial  = 0;
		tile.wallSpriteUID = 0;

		updateWalkable( pos );
		updateWalkable( pos.aboveOf() );

		removeGrass( pos );
		wakeWaterAround( pos );
		wakeWaterAround( pos.aboveOf() );

		return;
	}

	QString mat = DBH::materialSID( tile.wallMaterial );

	tileAbove.floorType = FloorType::FT_RAMPTOP;

	tile.wallType = WallType::WT_RAMP;
	tile.flags += TileFlag::TF_WALKABLE;

	setRampSprites( tile, tileAbove, sum, north, east, south, west, mat );

	updateWalkable( pos );
	updateWalkable( pos.aboveOf() );
}

/**
 * @brief Removes the floor at the given position, displacing items and creatures, and updating sunlight.
 * @param pos Position of the floor to remove.
 * @param extractTo Position where displaced items and creatures are moved.
 * @return Floor material UID of the removed floor.
 */
unsigned short World::removeFloor( Position pos, Position extractTo )
{
	Tile& tile              = getTile( pos );
	tile.floorType          = FloorType::FT_NOFLOOR;
	tile.floorSpriteUID     = 0;
	unsigned short floorMat = tile.floorMaterial;
	tile.floorMaterial      = 0;
	clearTileFlag( pos, TileFlag::TF_WALKABLE );
	m_regionMap.updatePosition( pos );
	removeGrass( pos );
	removePlant( pos );
	//g->inv()->gravity( pos );

	if ( tile.flags & TileFlag::TF_SUNLIGHT )
	{
		updateSunlight( pos );
	}

	PositionEntry pe;
	if ( g->inv()->getObjectsAtPosition( pos, pe ) )
	{
		for ( auto i : pe )
		{
			g->inv()->moveItemToPos( i, extractTo );
		}
	}

	if ( m_creaturePositions.contains( pos.toInt() ) )
	{
		g->gm()->forceMoveGnomes( pos, extractTo );
	}
	
	discover( pos.belowOf() );
	wakeWaterAround( pos );
	wakeWaterAround( pos.belowOf() );

	addToUpdateList( pos );

	return floorMat;
}

/**
 * @brief Creates a natural ramp at x,y,z using the given terrain material (used during world generation).
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param mat Terrain material to use for the ramp.
 */
void World::createRamp( int x, int y, int z, TerrainMaterial mat )
{
	int offset        = z * m_dimX * m_dimY;
	Tile& tileBelow   = m_world[x + y * m_dimX + ( z - 1 ) * m_dimX * m_dimY];
	Tile& tileAbove   = m_world[x + y * m_dimX + ( z + 1 ) * m_dimX * m_dimY];
	Tile& tile        = m_world[x + y * m_dimX + offset];

	if ( tileAbove.floorType == FloorType::FT_SOLIDFLOOR )
		return;
	if ( tileBelow.wallType == WallType::WT_NOWALL )
		return;
	if ( tile.wallType != WallType::WT_NOWALL )
		return;

	unsigned short key = DBH::materialUID( mat.key );

	if ( ( tileBelow.wallType & WallType::WT_ROUGH ) && ( tile.wallType == WallType::WT_NOWALL ) && ( tile.floorType == FloorType::FT_NOFLOOR ) )
	{
		tile.floorType      = FloorType::FT_SOLIDFLOOR;
		tile.floorMaterial  = key;
		tile.floorSpriteUID = g->sf()->createSprite( mat.floor, { mat.key } )->uID;
	}

	bool north = m_world[x + ( y - 1 ) * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool south = m_world[x + ( y + 1 ) * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool east  = m_world[( x + 1 ) + y * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool west  = m_world[( x - 1 ) + y * m_dimX + offset].wallType & WallType::WT_ROUGH;

	int sum = (int)north + (int)south + (int)east + (int)west;
	if ( sum > 2 || sum == 0 )
		return;

	tile.wallType     = ( WallType )( WallType::WT_RAMP );
	tile.wallMaterial = key;
	tile.flags += TileFlag::TF_WALKABLE;
	m_regionMap.updatePosition( Position( x, y, z ) );

	tileAbove.floorType     = FloorType::FT_RAMPTOP;
	tileAbove.floorMaterial = key;

	setRampSprites( tile, tileAbove, sum, north, east, south, west, mat.key );
}

/**
 * @brief Creates a natural ramp at the given position, inferring material from neighbor tiles.
 * @param pos World position.
 */
void World::createRamp( Position pos )
{
	createRamp( pos.x, pos.y, pos.z );
}

/**
 * @brief Creates a natural ramp at x,y,z, inferring material from the wall below.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 */
void World::createRamp( int x, int y, int z )
{
	int offset      = z * m_dimX * m_dimY;
	Tile& tileBelow = m_world[x + y * m_dimX + ( z - 1 ) * m_dimX * m_dimY];
	Tile& tileAbove = m_world[x + y * m_dimX + ( z + 1 ) * m_dimX * m_dimY];
	Tile& tile      = m_world[x + y * m_dimX + offset];
	if ( tileAbove.floorType == FloorType::FT_SOLIDFLOOR )
	{
		return;
	}
	if ( tileBelow.wallType == WallType::WT_NOWALL )
	{
		return;
	}
	if ( tile.floorType != FloorType::FT_SOLIDFLOOR )
	{
		return;
	}
	//if( tile.wallType != WallType::WT_NOWALL ) return;
	if ( ( tileBelow.wallType & WallType::WT_ROUGH ) && ( tile.wallType == WallType::WT_NOWALL ) && ( tile.floorType == FloorType::FT_NOFLOOR ) )
	{
		tile.floorType      = FloorType::FT_SOLIDFLOOR;
		tile.floorMaterial  = tileBelow.wallMaterial;
		tile.floorSpriteUID = g->sf()->createSprite( DB::select( "FloorSprite", "TerrainMaterials", DBH::materialSID( tileBelow.wallMaterial ) ).toString(), { DBH::materialSID( tileBelow.wallMaterial ) } )->uID;
	}

	bool north = m_world[x + ( y - 1 ) * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool south = m_world[x + ( y + 1 ) * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool east  = m_world[( x + 1 ) + y * m_dimX + offset].wallType & WallType::WT_ROUGH;
	bool west  = m_world[( x - 1 ) + y * m_dimX + offset].wallType & WallType::WT_ROUGH;

	int sum = (int)north + (int)south + (int)east + (int)west;

	if ( sum > 2 || sum == 0 )
	{
		return;
	}
	unsigned int key  = 0;
	unsigned int nMat = m_world[x + ( y - 1 ) * m_dimX + offset].wallMaterial;
	unsigned int sMat = m_world[x + ( y + 1 ) * m_dimX + offset].wallMaterial;
	unsigned int eMat = m_world[( x + 1 ) + y * m_dimX + offset].wallMaterial;
	unsigned int wMat = m_world[( x - 1 ) + y * m_dimX + offset].wallMaterial;

	if ( sum == 1 )
	{
		key = nMat | sMat | eMat | wMat;
	}
	else
	{
		if ( north )
		{
			if ( nMat == sMat || nMat == eMat || nMat == wMat )
				key = nMat;
		}
		if ( south )
		{
			if ( sMat == eMat || sMat == wMat )
				key = sMat;
		}
		if ( east )
		{
			if ( eMat == wMat )
				key = eMat;
		}
	}

	if ( key == 0 )
	{
		return;
	}
	key            = tile.floorMaterial;
	QString matSID = DBH::materialSID( key );

	tile.wallType     = ( WallType )( WallType::WT_RAMP );
	tile.wallMaterial = key;
	tile.flags += TileFlag::TF_WALKABLE;
	m_regionMap.updatePosition( Position( x, y, z ) );

	tileAbove.floorType     = FloorType::FT_RAMPTOP;
	tileAbove.floorMaterial = key;
	
	setRampSprites( tile, tileAbove, sum, north, east, south, west, matSID );
}

/**
 * @brief Creates a ramp at the given position using the specified material string ID.
 * @param pos World position.
 * @param materialSID Material string ID for the ramp.
 */
void World::createRamp( Position pos, QString materialSID )
{
	createRamp( pos.x, pos.y, pos.z, materialSID );
}

/**
 * @brief Creates a ramp at x,y,z using the specified material string ID, checking against solid walls.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param materialSID Material string ID for the ramp.
 */
void World::createRamp( int x, int y, int z, QString materialSID )
{
	int offset      = z * m_dimX * m_dimY;
	Tile& tileBelow = m_world[x + y * m_dimX + ( z - 1 ) * m_dimX * m_dimY];
	Tile& tileAbove = m_world[x + y * m_dimX + ( z + 1 ) * m_dimX * m_dimY];
	Tile& tile      = m_world[x + y * m_dimX + offset];
	if ( tileAbove.floorType == FloorType::FT_SOLIDFLOOR )
		return;
	//if( tileBelow.wallType == WallType::WT_NOWALL ) return;
	if ( tile.floorType != FloorType::FT_SOLIDFLOOR )
		return;
	if ( tile.wallType != WallType::WT_NOWALL )
		return;
	if ( ( tileBelow.wallType & WallType::WT_ROUGH ) && ( tile.wallType == WallType::WT_NOWALL ) && ( tile.floorType == FloorType::FT_NOFLOOR ) )
	{
		tile.floorType      = FloorType::FT_SOLIDFLOOR;
		tile.floorMaterial  = tileBelow.wallMaterial;
		tile.floorSpriteUID = g->sf()->createSprite( DB::select( "FloorSprite", "TerrainMaterials", DBH::materialSID( tileBelow.wallMaterial ) ).toString(), { DBH::materialSID( tileBelow.wallMaterial ) } )->uID;
	}

	bool north = m_world[x + ( y - 1 ) * m_dimX + offset].wallType & WallType::WT_SOLIDWALL;
	bool south = m_world[x + ( y + 1 ) * m_dimX + offset].wallType & WallType::WT_SOLIDWALL;
	bool east  = m_world[( x + 1 ) + y * m_dimX + offset].wallType & WallType::WT_SOLIDWALL;
	bool west  = m_world[( x - 1 ) + y * m_dimX + offset].wallType & WallType::WT_SOLIDWALL;

	int sum = (int)north + (int)south + (int)east + (int)west;

	if ( sum > 2 || sum == 0 )
	{
		return;
	}
	unsigned int key = DBH::materialUID( materialSID );

	tile.wallType     = ( WallType )( WallType::WT_RAMP );
	tile.wallMaterial = key;
	tile.flags += TileFlag::TF_WALKABLE;
	removeGrass( Position( x, y, z ) );

	m_regionMap.updatePosition( Position( x, y, z ) );

	tileAbove.floorType     = FloorType::FT_RAMPTOP;
	tileAbove.floorMaterial = key;

	setRampSprites( tile, tileAbove, sum, north, east, south, west, materialSID );
}

/**
 * @brief Sets the appropriate ramp sprites and rotations based on which neighbor walls exist.
 * @param tile Reference to the ramp tile.
 * @param tileAbove Reference to the tile above (receives the ramp-top floor sprite).
 * @param sum Number of adjacent rough/solid walls (1 or 2).
 * @param north True if the north neighbor has a wall.
 * @param east True if the east neighbor has a wall.
 * @param south True if the south neighbor has a wall.
 * @param west True if the west neighbor has a wall.
 * @param materialSID Material string ID used to select the correct sprite variant.
 */
void World::setRampSprites( Tile& tile, Tile& tileAbove, int sum, bool north, bool east, bool south, bool west, QString materialSID )
{
	unsigned int ramp      = g->sf()->createSprite( "Ramp", { materialSID } )->uID;
	unsigned int top       = g->sf()->createSprite( "RampTop", { materialSID } )->uID;
	unsigned int corner    = g->sf()->createSprite( "CornerRamp", { materialSID } )->uID;
	unsigned int cornerTop = g->sf()->createSprite( "CornerRampTop", { materialSID } )->uID;
	unsigned int uRamp     = g->sf()->createSprite( "URamp", { materialSID } )->uID;
	unsigned int uRampTop  = g->sf()->createSprite( "URampTop", { materialSID } )->uID;

	if ( tile.flags & TileFlag::TF_GRASS )
	{
		tile.floorSpriteUID = g->sf()->createSprite( "RoughFloor", { "Dirt" } )->uID;
		ramp                = g->sf()->createSprite( "GrassSoilRamp", { "Dirt", "Grass" } )->uID;
		top                 = g->sf()->createSprite( "GrassSoilRampTop", { "Dirt", "Grass" } )->uID;
		corner              = g->sf()->createSprite( "GrassSoilCornerRamp", { "Dirt", "Grass" } )->uID;
		cornerTop           = g->sf()->createSprite( "GrassSoilCornerRampTop", { "Dirt", "Grass" } )->uID;
		uRamp               = g->sf()->createSprite( "GrassSoilURamp", { "Dirt", "Grass" } )->uID;
		uRampTop            = g->sf()->createSprite( "GrassSoilURampTop", { "Dirt", "Grass" } )->uID;
		if ( tile.flags & TileFlag::TF_SUNLIGHT )
		{
			tileAbove.flags += TileFlag::TF_SUNLIGHT;
		}
	}

	if ( tile.flags & TileFlag::TF_BIOME_MUSHROOM )
	{
		//floorSpriteUID = g->sf()->createSprite( "RoughFloor", { "Dirt" } )->uID;
		ramp      = g->sf()->createSprite( "MushroomGrassSoilRamp", { "Dirt", "Grass" } )->uID;
		top       = g->sf()->createSprite( "MushroomGrassSoilRampTop", { "Dirt", "Grass" } )->uID;
		corner    = g->sf()->createSprite( "MushroomGrassSoilCornerRamp", { "Dirt", "Grass" } )->uID;
		cornerTop = g->sf()->createSprite( "MushroomGrassSoilCornerRampTop", { "Dirt", "Grass" } )->uID;
		uRamp     = g->sf()->createSprite( "MushroomGrassSoilURamp", { "Dirt", "Grass" } )->uID;
		uRampTop  = g->sf()->createSprite( "MushroomGrassSoilURampTop", { "Dirt", "Grass" } )->uID;
	}

	if ( sum == 1 )
	{
		if ( north )
		{
			tile.wallSpriteUID       = ramp;
			tileAbove.floorSpriteUID = top;
			tile.wallRotation        = 1;
			tileAbove.floorRotation  = 1;
		}
		else if ( west )
		{
			tile.wallSpriteUID       = ramp;
			tileAbove.floorSpriteUID = top;
			tile.wallRotation        = 0;
			tileAbove.floorRotation  = 0;
		}
		else if ( east )
		{
			tile.wallSpriteUID       = ramp;
			tileAbove.floorSpriteUID = top;
			tile.wallRotation        = 2;
			tileAbove.floorRotation  = 2;
		}
		else // south
		{
			tile.wallSpriteUID       = ramp;
			tileAbove.floorSpriteUID = top;
			tile.wallRotation        = 3;
			tileAbove.floorRotation  = 3;
		}
	}
	else
	{
		if ( north && west )
		{
			tile.wallSpriteUID       = corner;
			tileAbove.floorSpriteUID = cornerTop;
			tile.wallRotation        = 0;
			tileAbove.floorRotation  = 0;
		}
		else if ( north && east )
		{
			tile.wallSpriteUID       = corner;
			tileAbove.floorSpriteUID = cornerTop;
			tile.wallRotation        = 1;
			tileAbove.floorRotation  = 1;
		}
		else if ( south && west )
		{
			tile.wallSpriteUID       = corner;
			tileAbove.floorSpriteUID = cornerTop;
			tile.wallRotation        = 3;
			tileAbove.floorRotation  = 3;
		}
		else if ( south && east )
		{
			tile.wallSpriteUID       = corner;
			tileAbove.floorSpriteUID = cornerTop;
			tile.wallRotation        = 2;
			tileAbove.floorRotation  = 2;
		}
		else if ( north && south )
		{
			tile.wallSpriteUID       = uRamp;
			tileAbove.floorSpriteUID = uRampTop;
			tile.wallRotation        = 0;
			tileAbove.floorRotation  = 0;
		}
		else if ( west && east )
		{
			tile.wallSpriteUID       = uRamp;
			tileAbove.floorSpriteUID = uRampTop;
			tile.wallRotation        = 1;
			tileAbove.floorRotation  = 1;
		}
	}
}

/**
 * @brief Creates outer corner ramp sprites where two adjacent ramps meet at a corner.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 */
void World::createRampOuterCorners( int x, int y, int z )
{
	int offset = z * m_dimX * m_dimY;
	Tile& tile = m_world[x + y * m_dimX + offset];

	if ( tile.wallType != WallType::WT_NOWALL )
		return;

	bool north = m_world[x + ( y - 1 ) * m_dimX + offset].wallType & WallType::WT_RAMP;
	bool south = m_world[x + ( y + 1 ) * m_dimX + offset].wallType & WallType::WT_RAMP;
	bool east  = m_world[( x + 1 ) + y * m_dimX + offset].wallType & WallType::WT_RAMP;
	bool west  = m_world[( x - 1 ) + y * m_dimX + offset].wallType & WallType::WT_RAMP;

	int sum = (int)north + (int)south + (int)east + (int)west;

	if ( sum > 2 || sum == 0 )
	{
		return;
	}

	unsigned int key = tile.floorMaterial;

	QString matSID = DBH::materialSID( key );

	unsigned int corner = g->sf()->createSprite( "OuterCornerRamp", { matSID } )->uID;

	if ( tile.flags & TileFlag::TF_GRASS )
	{
		matSID = "Grass";
		corner = g->sf()->createSprite( "GrassOuterCornerRamp", { matSID } )->uID;
	}

	tile.wallType     = ( WallType )( WallType::WT_RAMPCORNER );
	tile.wallMaterial = tile.floorMaterial;

	if ( north && west )
	{
		tile.wallSpriteUID = corner;
		tile.wallRotation  = 0;
	}
	if ( north && east )
	{
		tile.wallSpriteUID = corner;

		tile.wallRotation = 1;
	}
	if ( south && west )
	{
		tile.wallSpriteUID = corner;
		tile.wallRotation  = 3;
	}
	if ( south && east )
	{
		tile.wallSpriteUID = corner;
		tile.wallRotation  = 2;
	}
}

/**
 * @brief Marks a tile for rendering update by flat tile ID (thread-safe).
 * @param uID Flat tile index.
 */
void World::addToUpdateList( const unsigned int uID )
{
	QMutexLocker lock( &m_updateMutex );
	m_updatedTiles.insert( uID );
}

/**
 * @brief Marks a tile for rendering update by position (thread-safe).
 * @param pos World position.
 */
void World::addToUpdateList( Position pos )
{
	auto tileID = pos.toInt();

	QMutexLocker lock( &m_updateMutex );
	m_updatedTiles.insert( tileID );
}

/**
 * @brief Marks a tile for rendering update by x,y,z coordinates (thread-safe).
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 */
void World::addToUpdateList( const unsigned short x, const unsigned short y, const unsigned short z )
{
	auto tileID = Position( x, y, z ).toInt();

	QMutexLocker lock( &m_updateMutex );
	m_updatedTiles.insert( tileID );
}

/**
 * @brief Batch-marks multiple tiles for rendering update from a QVector (thread-safe).
 * @param ul Vector of flat tile indices to mark.
 */
void World::addToUpdateList( const QVector<unsigned int>& ul )
{
	QMutexLocker lock( &m_updateMutex );
	for ( auto tileID : ul )
	{
		m_updatedTiles.insert( tileID );
	}
}

/**
 * @brief Batch-marks multiple tiles for rendering update from a QSet (thread-safe).
 * @param ul Set of flat tile indices to mark.
 */
void World::addToUpdateList( const QSet<unsigned int>& ul )
{
	QMutexLocker lock( &m_updateMutex );
	m_updatedTiles += ul;
}

/**
 * @brief Sets the lock state of a door, toggling walkability flags for gnomes, monsters, and animals.
 * @param tileUID Flat tile index of the door position.
 * @param lockGnome True to block gnome passage.
 * @param lockMonster True to block monster passage.
 * @param lockAnimal True to block animal passage.
 */
void World::setDoorLocked( unsigned int tileUID, bool lockGnome, bool lockMonster, bool lockAnimal )
{
	Position pos( tileUID );

	if ( lockGnome )
	{
		clearTileFlag( pos, TileFlag::TF_WALKABLE );
	}
	else
	{
		setTileFlag( pos, TileFlag::TF_WALKABLE );
	}
	if ( lockMonster )
	{
		clearTileFlag( pos, TileFlag::TF_WALKABLEMONSTERS );
	}
	else
	{
		setTileFlag( pos, TileFlag::TF_WALKABLEMONSTERS );
	}
	if ( lockAnimal )
	{
		clearTileFlag( pos, TileFlag::TF_WALKABLEANIMALS );
	}
	else
	{
		setTileFlag( pos, TileFlag::TF_WALKABLEANIMALS );
	}
	m_regionMap.updateConnectedRegions( pos );
}

/**
 * @brief Adds a light intensity delta to the tile at the given position.
 * @param pos World position.
 * @param light Light intensity to add.
 */
void World::putLight( Position pos, int light )
{
	Tile& tile = getTile( pos );
	tile.lightLevel += light;
	addToUpdateList( pos );
}

/**
 * @brief Adds a light intensity delta to the tile at the given coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate.
 * @param light Light intensity to add.
 */
void World::putLight( const unsigned short x, const unsigned short y, const unsigned short z, int light )
{
	Tile& tile = getTile( x, y, z );
	tile.lightLevel += light;
	addToUpdateList( x, y, z );
}

/**
 * @brief Registers a new light source in the light map and propagates it through the tile array.
 * @param id Unique ID for the light source.
 * @param pos World position of the light.
 * @param intensity Light intensity value.
 */
void World::addLight( unsigned int id, Position pos, int intensity )
{
	QSet<unsigned int> ul;
	m_lightMap.addLight( ul, m_world, id, pos, intensity );
	addToUpdateList( ul );
}

/**
 * @brief Removes a light source from the light map and clears its contribution from tiles.
 * @param id Unique ID of the light source to remove.
 */
void World::removeLight( unsigned int id )
{
	QSet<unsigned int> ul;
	m_lightMap.removeLight( ul, m_world, id );
	addToUpdateList( ul );
}

/**
 * @brief Moves a light source to a new position by removing and re-adding it.
 * @param id Unique ID of the light source.
 * @param pos New world position.
 * @param intensity Light intensity at the new position.
 */
void World::moveLight( unsigned int id, Position pos, int intensity )
{
	QSet<unsigned int> ul;
	m_lightMap.removeLight( ul, m_world, id );
	m_lightMap.addLight( ul, m_world, id, pos, intensity );
	addToUpdateList( ul );
}

/**
 * @brief Recalculates all lights that may be affected by a change at the given position.
 * @param pos World position that changed (e.g., a wall was added or removed).
 */
void World::updateLightsInRange( Position pos )
{
	QSet<unsigned int> ul;
	m_lightMap.updateLight( ul, m_world, pos );
	addToUpdateList( ul );
}

/**
 * @brief Propagates sunlight downward through open floors starting at the given position.
 * @param pos Starting position to propagate sunlight from.
 */
void World::updateSunlight( Position pos )
{
	Tile& tile = getTile( pos );
	tile.flags += TileFlag::TF_SUNLIGHT;
	addToUpdateList( pos );
	if ( pos.z > 0 && tile.floorType == FT_NOFLOOR )
	{
		updateSunlight( pos.belowOf() );
	}
}

/**
 * @brief Checks whether the tile at the given position has direct sunlight.
 * @param pos World position.
 * @return True if the tile has the TF_SUNLIGHT flag.
 */
bool World::hasSunlight( Position pos )
{
	Tile& tile = getTile( pos );
	return tile.flags & TileFlag::TF_SUNLIGHT;
}

/**
 * @brief Checks whether the tile at the given position has grass.
 * @param pos World position.
 * @return True if the tile has the TF_GRASS flag.
 */
bool World::hasGrass( Position pos )
{
	Tile& tile = getTile( pos );
	return tile.flags & TileFlag::TF_GRASS;
}

/**
 * @brief Checks whether the tile has fully grown grass (level 100), is walkable, and has the grass flag.
 * @param pos World position.
 * @return True if the tile has maximum grass growth.
 */
bool World::hasMaxGrass( Position pos )
{
	Tile& tile = getTile( pos );
	return tile.flags & TileFlag::TF_GRASS && tile.flags & TileFlag::TF_WALKABLE && tile.vegetationLevel == 100; //TODO bad fix, making tiles not walkable should remove designations
}

/**
 * @brief Checks whether removing the floor at pos would trap a gnome on an isolated neighbor tile.
 * @param pos Position of the floor that would be removed.
 * @param workPos Worker's position (excluded from the neighbor check).
 * @return True if removing the floor would trap a gnome.
 */
bool World::checkTrapGnomeFloor( Position pos, Position workPos )
{
	// TODO just realized this function is garbage, needs to be fixed
	// check other 3 neighbors of pos
	Position checkPos = pos.northOf();
	if ( checkPos != workPos )
	{
		// checkPos only has one walkable neighbor which is the tile we want to remove
		if ( walkableNeighbors( checkPos ) == 1 )
		{
			// checkIf Gnome is on tile
			if ( isWalkable( pos.eastOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.westOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.southOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.northOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
		}
	}
	checkPos = pos.eastOf();
	if ( checkPos != workPos )
	{
		// checkPos only has one walkable neighbor which is the tile we want to remove
		if ( walkableNeighbors( checkPos ) == 1 )
		{
			// checkIf Gnome is on tile
			if ( isWalkable( pos.eastOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.westOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.southOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.northOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
		}
	}
	checkPos = pos.southOf();
	if ( checkPos != workPos )
	{
		// checkPos only has one walkable neighbor which is the tile we want to remove
		if ( walkableNeighbors( checkPos ) == 1 )
		{
			// checkIf Gnome is on tile
			if ( isWalkable( pos.eastOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.westOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.southOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.northOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
		}
	}
	checkPos = pos.westOf();
	if ( checkPos != workPos )
	{
		// checkPos only has one walkable neighbor which is the tile we want to remove
		if ( walkableNeighbors( checkPos ) == 1 )
		{
			// checkIf Gnome is on tile
			if ( isWalkable( pos.eastOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.westOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.southOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
			if ( isWalkable( pos.northOf() ) && g->gm()->gnomesAtPosition( pos.eastOf() ).size() > 0 )
				return true;
		}
	}

	return false;
}

/**
 * @brief Sets or clears the animation bit on the wall sprite UID at the given position.
 * @param pos World position.
 * @param anim True to enable the animation bit, false to disable it.
 */
void World::setWallSpriteAnim( Position pos, bool anim )
{
	Tile& tile = getTile( pos );
	if ( anim )
	{
		tile.wallSpriteUID |= 0x00040000;
	}
	else
	{
		tile.wallSpriteUID &= 0xFFFBFFFF;
	}
	addToUpdateList( pos );
}
