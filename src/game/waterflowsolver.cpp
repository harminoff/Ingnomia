/*
    This file is part of Ingnomia.
    See the project license for details.
*/
/** @file waterflowsolver.cpp
 *  @brief Implementation of the deterministic water-flow step.
 */

#include "waterflowsolver.h"

#include <algorithm>
#include <limits>

namespace
{
constexpr unsigned int invalidTile = std::numeric_limits<unsigned int>::max();

unsigned int tileID( int x, int y, int z, int dimX, int dimY, int dimZ )
{
	if ( x < 0 || y < 0 || z < 0 || x >= dimX || y >= dimY || z >= dimZ )
		return invalidTile;
	return static_cast<unsigned int>( x + y * dimX + z * dimX * dimY );
}

struct Neighbors
{
	unsigned int above = invalidTile;
	unsigned int below = invalidTile;
	unsigned int north = invalidTile;
	unsigned int south = invalidTile;
	unsigned int east = invalidTile;
	unsigned int west = invalidTile;
};

Neighbors neighbors( unsigned int id, int dimX, int dimY, int dimZ )
{
	const int layer = dimX * dimY;
	const int z = static_cast<int>( id ) / layer;
	const int remainder = static_cast<int>( id ) % layer;
	const int y = remainder / dimX;
	const int x = remainder % dimX;
	return {
		tileID( x, y, z + 1, dimX, dimY, dimZ ),
		tileID( x, y, z - 1, dimX, dimY, dimZ ),
		tileID( x, y - 1, z, dimX, dimY, dimZ ),
		tileID( x, y + 1, z, dimX, dimY, dimZ ),
		tileID( x + 1, y, z, dimX, dimY, dimZ ),
		tileID( x - 1, y, z, dimX, dimY, dimZ )
	};
}

}

WaterFlowResult solveWaterFlow( const QHash<unsigned int, WaterFlowCell>& cells,
	int dimX,
	int dimY,
	int dimZ,
	const QSet<unsigned int>& activeWater,
	const WaterFlowConfig& config )
{
	WaterFlowResult result;
	if ( dimX <= 0 || dimY <= 0 || dimZ <= 0 || cells.isEmpty() )
		return result;

	const unsigned int worldTileCount = static_cast<unsigned int>( dimX * dimY * dimZ );
	const int capacity = qMax( 0, config.surfaceCapacity );
	const int maxStoredMass = qMax( capacity, config.maxStoredMass );
	const int maxTransfer = qMax( 0, config.maxTransferPerEdge );

	QHash<unsigned int, int> mass;
	QHash<unsigned int, WaterFlow> flowFlags;
	QVector<unsigned int> active;
	active.reserve( activeWater.size() );
	result.visitedCellCount = cells.size();

	for ( auto it = cells.cbegin(); it != cells.cend(); ++it )
	{
		mass.insert( it.key(), qBound( 0, it.value().mass, maxStoredMass ) );
	}

	for ( const unsigned int id : activeWater )
	{
		if ( id >= worldTileCount || !cells.contains( id ) )
			continue;
		const WaterFlowCell& cell = cells[id];
		result.touched.insert( id );
		if ( cell.boundary || cell.moveBlocking || mass.value( id ) == 0 )
		{
			result.mass.insert( id, 0 );
			continue;
		}
		active.append( id );
	}

	std::sort( active.begin(), active.end() );

	auto passable = [&]( unsigned int id ) {
		return id != invalidTile && id < worldTileCount && cells.contains( id ) && !cells[id].boundary && !cells[id].moveBlocking;
	};

	auto activateAround = [&]( unsigned int id ) {
		if ( passable( id ) )
			result.nextActive.insert( id );
		const Neighbors n = neighbors( id, dimX, dimY, dimZ );
		for ( const unsigned int neighbor : { n.above, n.below, n.north, n.south, n.east, n.west } )
		{
			if ( passable( neighbor ) )
				result.nextActive.insert( neighbor );
		}
	};

	auto addTransfer = [&]( unsigned int source, unsigned int destination, int requested, WaterFlow direction ) {
		if ( !passable( source ) || !passable( destination ) || requested <= 0 )
			return 0;

		const int available = mass.value( source );
		const int currentDestinationMass = mass.value( destination );
		const int destinationRoom = qMax( 0, capacity - currentDestinationMass );
		const int amount = qMin( requested, qMin( available, destinationRoom ) );
		if ( amount <= 0 )
			return 0;

		mass[source] -= amount;
		mass[destination] += amount;
		flowFlags[source] = flowFlags.value( source, WF_NOFLOW ) + direction;
		result.touched.insert( source );
		result.touched.insert( destination );
		activateAround( source );
		activateAround( destination );
		return amount;
	};

	// Gravity gets the first claim on free capacity.  Solid floors contain a
	// cell even when its neighbours below are empty.
	for ( const unsigned int id : active )
	{
		const Neighbors n = neighbors( id, dimX, dimY, dimZ );
		if ( !cells[id].solidFloor && passable( n.below ) )
			addTransfer( id, n.below, maxTransfer, WF_DOWN );
	}

	// A local integer gradient stops at 10,9,8,...,1,0 forever. Relax the
	// connected surface instead, with a dry frontier only one cell wide. This
	// levels a body without scanning the map or spreading through a dry wall.
	QSet<unsigned int> wet;
	QSet<unsigned int> candidates;
	for ( auto it = mass.cbegin(); it != mass.cend(); ++it )
	{
		if ( it.value() <= 0 || cells[it.key()].mass <= 0 || !passable( it.key() ) )
			continue;
		wet.insert( it.key() );
		candidates.insert( it.key() );
		const Neighbors n = neighbors( it.key(), dimX, dimY, dimZ );
		for ( const auto adjacent : { n.north, n.east, n.south, n.west } )
			if ( passable( adjacent ) ) candidates.insert( adjacent );
	}
	QVector<unsigned int> ordered = candidates.values().toVector();
	std::sort( ordered.begin(), ordered.end() );
	QSet<unsigned int> visited;
	for ( const auto seed : ordered )
	{
		if ( visited.contains( seed ) ) continue;
		QVector<unsigned int> body { seed };
		visited.insert( seed );
		qint64 total = 0;
		for ( qsizetype cursor = 0; cursor < body.size(); ++cursor )
		{
			const auto id = body[cursor];
			total += mass.value( id );
			const Neighbors n = neighbors( id, dimX, dimY, dimZ );
			for ( const auto adjacent : { n.north, n.east, n.south, n.west } )
			{
				// Do not walk through chains of dry candidates. A newly wetted
				// frontier becomes a source on the next fixed simulation tick.
				if ( candidates.contains( adjacent ) && !visited.contains( adjacent ) &&
					( wet.contains( id ) || wet.contains( adjacent ) ) )
				{
					visited.insert( adjacent );
					body.append( adjacent );
				}
			}
		}
		if ( total == 0 ) continue;
		auto draining = [&]( unsigned int id ) {
			const auto below = neighbors( id, dimX, dimY, dimZ ).below;
			return !cells[id].solidFloor && passable( below ) && mass.value( below ) < capacity;
		};
		// Keep indivisible remainders where they are so a settled puddle does
		// not shuffle every tick. Give a spillway first claim on that last unit:
		// even 1/10-deep water must drain when its containing floor is removed.
		std::sort( body.begin(), body.end(), [&]( unsigned int a, unsigned int b ) {
			if ( draining( a ) != draining( b ) ) return draining( a );
			if ( mass.value( a ) != mass.value( b ) ) return mass.value( a ) > mass.value( b );
			return a < b;
		} );
		const int level = static_cast<int>( total / body.size() );
		const int remainder = static_cast<int>( total % body.size() );
		QHash<unsigned int, int> target;
		QVector<unsigned int> donors;
		QVector<unsigned int> receivers;
		for ( qsizetype i = 0; i < body.size(); ++i )
		{
			const auto id = body[i];
			target[id] = level + ( i < remainder ? 1 : 0 );
			if ( mass.value( id ) > target[id] ) donors.append( id );
			if ( mass.value( id ) < target[id] ) receivers.append( id );
		}
		qsizetype receiverIndex = 0;
		for ( const auto donor : donors )
		{
			int available = qMin( maxTransfer, mass.value( donor ) - target[donor] );
			while ( available > 0 && receiverIndex < receivers.size() )
			{
				const auto receiver = receivers[receiverIndex];
				const int amount = qMin( available, target[receiver] - mass.value( receiver ) );
				mass[donor] -= amount;
				mass[receiver] += amount;
				available -= amount;
				result.touched.insert( donor );
				result.touched.insert( receiver );
				activateAround( donor );
				activateAround( receiver );
				const int dx = static_cast<int>( receiver % dimX ) - static_cast<int>( donor % dimX );
				const int dy = static_cast<int>( receiver / dimX % dimY ) - static_cast<int>( donor / dimX % dimY );
				WaterFlow direction = WF_NOFLOW;
				if ( dx != 0 ) direction += dx > 0 ? WF_EAST : WF_WEST;
				if ( dy != 0 ) direction += dy > 0 ? WF_SOUTH : WF_NORTH;
				flowFlags[donor] = flowFlags.value( donor, WF_NOFLOW ) + direction;
				if ( mass.value( receiver ) == target[receiver] ) ++receiverIndex;
			}
		}
	}

	for ( const auto id : active )
	{
		const Neighbors n = neighbors( id, dimX, dimY, dimZ );
		if ( mass.value( id ) > capacity && passable( n.above ) && !cells[n.above].solidFloor )
			addTransfer( id, n.above, qMin( maxTransfer, mass.value( id ) - capacity ), WF_UP );
	}

	for ( const unsigned int id : result.touched )
	{
		if ( id >= worldTileCount )
			continue;
		const int finalMass = qBound( 0, mass.value( id ), maxStoredMass );
		result.mass.insert( id, finalMass );
		if ( finalMass > 0 && passable( id ) )
			result.flow.insert( id, flowFlags.value( id, WF_NOFLOW ) );
		else
			result.mass[id] = 0;
	}

	return result;
}
