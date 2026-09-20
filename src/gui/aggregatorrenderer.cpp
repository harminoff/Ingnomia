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
/** @file aggregatorrenderer.cpp
 *  @brief AggregatorRenderer implementation: batches tile sprite data, collects creature
 *         sprites and thought bubbles, and packs axle power info for the renderer.
 */
#include "aggregatorrenderer.h"

#include "../base/global.h"
#include "../base/gamestate.h"
#include "../game/game.h"
#include "../game/creature.h"
#include "../game/creaturemanager.h"
#include "../game/job.h"
#include "../game/jobmanager.h"
#include "../game/gnomemanager.h"
#include "../game/mechanismmanager.h"
#include "../game/world.h"
#include "../gfx/sprite.h"
#include "../gfx/spritefactory.h"

/// @brief Constructs the AggregatorRenderer and registers payload metatypes so the signals
///        can be delivered via queued connections across threads.
/// @param parent Qt parent object.
AggregatorRenderer::AggregatorRenderer( QObject* parent ) :
	QObject( parent )
{
	qRegisterMetaType<TileDataUpdateInfo>();
	qRegisterMetaType<ThoughtBubbleInfo>();
	qRegisterMetaType<AxleDataInfo>();
}

/// @brief Binds the aggregator to a Game instance.
/// @param game Game to bind to.
void AggregatorRenderer::init( Game* game )
{
	g = game;
}

/// @brief Builds a TileDataUpdate packet for a single world tile, packing floor/wall/item
///        sprite UIDs (with rotation and wall-flag bits), job-overlay sprites, and light/
///        fluid/vegetation levels.
/// @param tileID Integer tile key (Position::toInt()).
/// @return TileDataUpdate ready to be pushed into a batch.
TileDataUpdate AggregatorRenderer::aggregateTile( unsigned int tileID ) const
{
	if( !g ) return TileDataUpdate();
	const auto& tile = g->w()->world()[tileID];
	TileData td;
	if ( tile.floorSpriteUID )
	{
		unsigned int rot  = ( tile.floorRotation % 4 ) * ROT_BIT;
		td.floorSpriteUID = tile.floorSpriteUID + rot;
	}

	if ( tile.wallSpriteUID )
	{
		unsigned int rot = ( tile.wallRotation % 4 ) * ROT_BIT;
		td.wallSpriteUID = tile.wallSpriteUID + rot;
		if ( tile.wallType & WallType::WT_SOLIDWALL )
		{
			td.wallSpriteUID += WALL_BIT;
		}
	}

	if ( tile.itemSpriteUID )
	{
		td.itemSpriteUID = tile.itemSpriteUID;
	}

	if ( g->w()->hasJob( Position( tileID ) ) )
	{
		QVariantMap job      = g->w()->jobSprite( Position( tileID ) );
		QVariantMap floorJob = job.value( "Floor" ).toMap();
		QVariantMap wallJob  = job.value( "Wall" ).toMap();

		if ( !floorJob.isEmpty() )
		{
			unsigned int suid    = floorJob.value( "SpriteUID" ).toUInt();
			unsigned int rot     = floorJob.value( "Rot" ).toUInt() * ROT_BIT;
			td.jobSpriteFloorUID = suid + rot;
		}
		if ( !wallJob.isEmpty() )
		{
			unsigned int suid   = wallJob.value( "SpriteUID" ).toUInt();
			unsigned int rot    = wallJob.value( "Rot" ).toUInt() * ROT_BIT;
			td.jobSpriteWallUID = suid + rot;
		}
	}

	td.flags  = (quint64)tile.flags;
	td.flags2 = (quint64)tile.flags >> 32;
	if ( tile.wallType & WT_MOVEBLOCKING ) td.flags2 |= TileData::WaterBlocking;
	if ( tile.floorType & FT_SOLIDFLOOR ) td.flags2 |= TileData::WaterFloor;

	td.lightLevel      = qMin( tile.lightLevel, (unsigned char)20 );
	td.fluidLevel      = qMin( tile.fluidLevel, (unsigned char)10 );
	td.vegetationLevel = qMin( tile.vegetationLevel, (unsigned char)100 );
	td.waterFlow       = static_cast<unsigned char>( tile.flow );

	return TileDataUpdate { tileID, td };
}

namespace
{
/// @brief Adds the render-only creature motion fields to a tile update. Long jumps are
///        deliberately not interpolated because they are teleports, not walking steps.
void applyCreatureRenderData( TileDataUpdate& update, const CreatureRenderData& creature )
{
	update.tile.creatureSpriteUID = creature.spriteUID;

	const Position currentPosition( update.id );
	const Position delta = creature.previousPosition - currentPosition;
	const bool isAdjacentStep = qAbs( delta.x ) <= 1 && qAbs( delta.y ) <= 1 && delta.z == 0;
	if ( isAdjacentStep )
	{
		update.tile.creatureOffsetX = delta.x;
		update.tile.creatureOffsetY = delta.y;
		update.tile.creatureOffsetZ = delta.z;
		update.tile.creatureMotionTick = static_cast<quint32>( creature.motionTick );
		update.tile.creatureMotionDurationTicks = creature.motionDurationTicks;
	}
}
} // namespace

/// @brief Walks every live gnome (including dead-on-map gnomes and specials), automaton,
///        animal, and monster to compute a tileID → creature sprite map. Rotation is
///        encoded in the high bits of the sprite UID (facing * ROT_BIT), while the previous
///        position is retained for render-only interpolation.
/// @return Hash mapping tile UIDs to creature sprite and previous render position data.
QHash<unsigned int, CreatureRenderData> AggregatorRenderer::collectCreatures( quint64 simulationTick,
	QHash<unsigned int, CreatureCameraTarget>& cameraTargets )
{
	if( !g ) return QHash<unsigned int, CreatureRenderData>();
	QHash<unsigned int, CreatureRenderData> creatures;
	QHash<unsigned int, Position> currentCreaturePositions;
	cameraTargets.clear();

	const auto registerCameraTarget = [&]( unsigned int creatureID, const Position& currentPosition )
	{
		if ( cameraTargets.contains( creatureID ) ) return;

		const Position previousPosition = m_previousCreaturePositions.value( creatureID, currentPosition );
		if ( !m_lastCreatureMotionTicks.contains( creatureID ) )
			m_lastCreatureMotionTicks.insert( creatureID, simulationTick );

		if ( !m_creatureMotionSegments.contains( creatureID ) )
		{
			m_creatureMotionSegments.insert( creatureID, CreatureCameraTarget {
				currentPosition, currentPosition, simulationTick, 1
			} );
		}

		if ( previousPosition != currentPosition )
		{
			const quint64 previousMotionTick = m_lastCreatureMotionTicks.value( creatureID, simulationTick );
			const quint64 elapsedTicks = simulationTick > previousMotionTick ? simulationTick - previousMotionTick : 1;
			const Position delta = previousPosition - currentPosition;
			const bool isAdjacentStep = qAbs( delta.x ) <= 1 && qAbs( delta.y ) <= 1 && delta.z == 0;
			// OpenRCT2 keeps an entity's pre/post tick pair alive for the complete
			// interpolation interval. Do the same here: unrelated tile updates must
			// not replace an active camera path with current/current and snap the
			// viewport to its destination. Long jumps remain instant teleports.
			const auto duration = static_cast<quint32>( qBound<quint64>( 1, elapsedTicks, 12 ) );
			m_creatureMotionSegments[creatureID] = CreatureCameraTarget {
				currentPosition,
				isAdjacentStep ? previousPosition : currentPosition,
				simulationTick,
				isAdjacentStep ? duration : 1
			};
			m_lastCreatureMotionTicks[creatureID] = simulationTick;
		}

		currentCreaturePositions[creatureID] = currentPosition;
		cameraTargets.insert( creatureID, m_creatureMotionSegments.value( creatureID ) );
	};

	const auto addCreature = [&]( unsigned int creatureID, const Position& currentPosition,
			unsigned int spriteID, const Position& renderPosition )
	{
		registerCameraTarget( creatureID, currentPosition );
		const auto cameraTarget = cameraTargets.value( creatureID );
		const Position renderOffset = renderPosition - currentPosition;
		creatures[renderPosition.toInt()] = CreatureRenderData {
			spriteID,
			cameraTarget.previousPosition + renderOffset,
			cameraTarget.motionTick,
			cameraTarget.motionDurationTicks
		};
	};

	Sprite* sprite    = nullptr;

	//TODO remove when we create gnome corpses
	for ( auto gn : g->gm()->deadGnomes() )
	{
		const Position position = gn->getPos();
		{
			unsigned int spriteID = 0;
			sprite                = g->sf()->getCreatureSprite( gn->id(), spriteID );
			if ( sprite )
			{
				spriteID += gn->facing() * ROT_BIT;
			}
			addCreature( gn->id(), position, spriteID, position );
		}
	}

	for ( auto gn : g->gm()->gnomes() )
	{
		if ( !gn->goneOffMap() )
		{
			const Position position = gn->getPos();
			{
				unsigned int spriteID = 0;
				sprite                = g->sf()->getCreatureSprite( gn->id(), spriteID );
				if ( sprite )
				{
					spriteID += gn->facing() * ROT_BIT;
				}
				addCreature( gn->id(), position, spriteID, position );
			}
		}
	}
	for ( auto gn : g->gm()->specialGnomes() )
	{
		const Position position = gn->getPos();
		{
			unsigned int spriteID = 0;
			sprite                = g->sf()->getCreatureSprite( gn->id(), spriteID );
			if ( sprite )
			{
				spriteID += gn->facing() * ROT_BIT;
			}
			addCreature( gn->id(), position, spriteID, position );
		}
	}

	for ( auto a : g->gm()->automatons() )
	{
		const Position position = a->getPos();
		{
			unsigned int spriteID = 0;
			sprite                = g->sf()->getCreatureSprite( a->id(), spriteID );
			if ( sprite )
			{
				spriteID += a->facing() * ROT_BIT;
			}
			addCreature( a->id(), position, spriteID, position );
		}
	}

	const auto& creatureList = g->cm()->creatures();

	for( const auto& creature : creatureList )
	{
		switch( creature->type() )
		{
			case CreatureType::ANIMAL:
			{
				auto a = dynamic_cast<Animal*>( creature );
				const Position position = a->getPos();
				const unsigned int positionID = position.toInt();
				if ( !a->isDead() )
				{
					registerCameraTarget( a->id(), position );
					currentCreaturePositions[a->id()] = position;
					if ( !creatures.contains( positionID ) )
					{
						if ( a->isMulti() )
						{
							auto sprites = a->multiSprites();
							for ( auto def : sprites )
							{
								addCreature( a->id(), position, def.second, def.first );
							}
						}
						else
						{
							Sprite* sprite        = g->sf()->getSprite( a->spriteUID() );
							unsigned int spriteID = 0;
							if ( sprite )
							{
								spriteID = sprite->uID;
								spriteID += a->facing() * ROT_BIT;
							}
							addCreature( a->id(), position, spriteID, position );
						}
					}
				}
			}
			break;
			case CreatureType::MONSTER:
			{
				auto m = dynamic_cast<Monster*>( creature );
				const Position position = m->getPos();
				const unsigned int positionID = position.toInt();
				if ( !m->isDead() )
				{
					registerCameraTarget( m->id(), position );
					currentCreaturePositions[m->id()] = position;
					if ( !creatures.contains( positionID ) )
					{
						unsigned int spriteID = 0;
						sprite                = g->sf()->getCreatureSprite( m->id(), spriteID );
						if ( sprite )
						{
							spriteID += m->facing() * ROT_BIT;
						}
						addCreature( m->id(), position, spriteID, position );
					}
				}
			}
		}
	}
	
	for ( auto it = m_creatureMotionSegments.begin(); it != m_creatureMotionSegments.end(); )
	{
		if ( !currentCreaturePositions.contains( it.key() ) )
		{
			m_lastCreatureMotionTicks.remove( it.key() );
			it = m_creatureMotionSegments.erase( it );
		}
		else
		{
			++it;
		}
	}
	m_previousCreaturePositions = currentCreaturePositions;
	return creatures;
}

/// @brief Emits a full-world tile sprite refresh: iterates every tile in the world, fills in
///        creature sprites, and pushes batches of up to 65536 tiles through signalTileUpdates.
void AggregatorRenderer::onAllTileInfo()
{
	if( !g ) return;
	// Bake tile updates
	QHash<unsigned int, CreatureCameraTarget> cameraTargets;
	auto creatures = collectCreatures( GameState::tick, cameraTargets );
	for ( auto tile = creatures.keyBegin(); tile != creatures.keyEnd(); ++tile )
	{
		//tiles.insert(*tile);
	}
	constexpr size_t batchSize = 1 << 16;
	TileDataUpdateInfo tileUpdates;
	tileUpdates.simulationTick = m_latestSimulationTick ? m_latestSimulationTick : GameState::tick;
	tileUpdates.creatureCameraTargets = cameraTargets;
	tileUpdates.updates.reserve( batchSize );
	const unsigned int worldSize = (unsigned int)g->w()->world().size();
	for ( unsigned int tileUID = 0; tileUID < worldSize; ++tileUID )
	{
		auto update = aggregateTile( tileUID );

		const auto creatureSprite = creatures.find( tileUID );
		if ( creatureSprite != creatures.end() )
		{
			applyCreatureRenderData( update, creatureSprite.value() );
		}

		tileUpdates.updates.push_back( update );

		if ( tileUpdates.updates.size() >= batchSize )
		{
			{
				emit signalTileUpdates( tileUpdates );
				tileUpdates.updates.clear();
			}
			tileUpdates.updates.reserve( batchSize );
		}
	}
	if ( !tileUpdates.updates.empty() )
	{
		emit signalTileUpdates( tileUpdates );
	}
}

/// @brief Emits a partial tile sprite refresh for just the tiles in @p changeSet, and also
///        triggers axle-data and thought-bubble updates as a side effect. Used for every
///        per-frame update after the initial world load.
/// @param changeSet Set of tile UIDs whose state changed this frame.
void AggregatorRenderer::onUpdateAnyTileInfo( const QSet<unsigned int>& changeSet )
{
	if( !g ) return;
	// Bake tile updates
	QHash<unsigned int, CreatureCameraTarget> cameraTargets;
	auto creatures = collectCreatures( GameState::tick, cameraTargets );
	for ( auto tile = creatures.keyBegin(); tile != creatures.keyEnd(); ++tile )
	{
		//tiles.insert(*tile);
	}
	constexpr size_t batchSize = 1 << 16;
	TileDataUpdateInfo tileUpdates;
	tileUpdates.simulationTick = m_latestSimulationTick ? m_latestSimulationTick : GameState::tick;
	tileUpdates.creatureCameraTargets = cameraTargets;
	tileUpdates.updates.reserve( batchSize );
	for ( auto tileUID : changeSet )
	{
		auto update = aggregateTile( tileUID );

		const auto creatureSprite = creatures.find( tileUID );
		if ( creatureSprite != creatures.end() )
		{
			applyCreatureRenderData( update, creatureSprite.value() );
		}

		tileUpdates.updates.push_back( update );

		if ( tileUpdates.updates.size() >= batchSize )
		{
			{
				emit signalTileUpdates( tileUpdates );
				tileUpdates.updates.clear();
			}
			tileUpdates.updates.reserve( batchSize );
		}
	}
	if ( !tileUpdates.updates.empty() )
	{
		emit signalTileUpdates( tileUpdates );
	}

	if ( g->mcm()->axlesChanged() )
	{
		onAxleDataUpdate();
	}
	onThoughtBubbleUpdate();
}

/// @brief Collects all active thought bubbles from gnomes and animals and emits them to the
///        renderer via signalThoughtBubbles.
void AggregatorRenderer::onThoughtBubbleUpdate()
{
	if( !g ) return;
	ThoughtBubbleInfo info;
	for ( const auto& gn : g->gm()->gnomes() )
	{
		QString thoughtBubble = gn->thoughtBubble();
		if ( !thoughtBubble.isEmpty() )
		{
			info.thoughtBubbles.push_back( { gn->getPos(), g->sf()->thoughtBubbleID( thoughtBubble ) } );
		}
	}

	for ( const auto& gn : g->cm()->animals() )
	{
		QString thoughtBubble = gn->thoughtBubble();
		if ( !thoughtBubble.isEmpty() )
		{
			info.thoughtBubbles.push_back( { gn->getPos(), g->sf()->thoughtBubbleID( thoughtBubble ) } );
		}
	}

	// PENDING means the workshop blueprint's required items could not yet be
	// claimed. Keep the warning attached to the authoritative construction job.
	const auto pendingResourcesSprite = g->sf()->thoughtBubbleID( "NeedsResources" );
	for ( auto it = g->jm()->allJobs().cbegin(); it != g->jm()->allJobs().cend(); ++it )
	{
		const auto& job = it.value();
		if ( job && job->type() == "BuildWorkshop" && job->phase() == JobPhase::PENDING )
		{
			info.thoughtBubbles.push_back( { job->pos(), pendingResourcesSprite } );
		}
	}
	emit signalThoughtBubbles( info );
}

/// @brief Emits the current axle power/rotation state for all axles to the renderer.
void AggregatorRenderer::onAxleDataUpdate()
{
	if( !g ) return;
	AxleDataInfo data;
	data.data = g->mcm()->axleData();
	emit signalAxleData( data );
}

/// @brief Relays a camera-center request (e.g. "jump to gnome") to the renderer.
/// @param location Target world position.
void AggregatorRenderer::onCenterCamera( const Position& location )
{
	if ( !g ) return;
	emit signalCenterCamera( location );
}

/// @brief Notifies the renderer that world dimensions or other global parameters changed
///        (e.g. after loading a new save) so it can reallocate GPU buffers.
void AggregatorRenderer::onWorldParametersChanged()
{
	if( !g ) return;
	m_previousCreaturePositions.clear();
	m_lastCreatureMotionTicks.clear();
	m_creatureMotionSegments.clear();
	m_latestSimulationTick = 0;
	emit signalWorldParametersChanged();
}

/// @brief Relays every authoritative fixed simulation tick to the GUI renderer.
/// Tile updates are intentionally not used as the clock: ticks with no dirty
/// terrain still need to advance creature interpolation smoothly.
void AggregatorRenderer::onSimulationTick( quint64 tick )
{
	m_latestSimulationTick = tick;
	emit signalSimulationTick( tick );
}
