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

WaterFlow oppositeFlow( WaterFlow flow )
{
	switch ( flow )
	{
		case WF_NORTH: return WF_SOUTH;
		case WF_SOUTH: return WF_NORTH;
		case WF_EAST: return WF_WEST;
		case WF_WEST: return WF_EAST;
		case WF_UP: return WF_DOWN;
		case WF_DOWN: return WF_UP;
		default: return WF_NOFLOW;
	}
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
	QHash<unsigned int, int> remaining;
	QHash<unsigned int, int> delta;
	QHash<unsigned int, WaterFlow> flowFlags;
	QSet<unsigned int> sourceSet;
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
		remaining.insert( id, mass.value( id ) );
		sourceSet.insert( id );
		active.append( id );
	}

	std::sort( active.begin(), active.end() );

	auto passable = [&]( unsigned int id ) {
		return id != invalidTile && id < worldTileCount && cells.contains( id ) && !cells[id].boundary && !cells[id].moveBlocking;
	};

	auto ensureMass = [&]( unsigned int id ) {
		if ( id == invalidTile || id >= worldTileCount || !cells.contains( id ) )
			return 0;
		return mass.value( id );
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

		ensureMass( destination );
		const int available = remaining.value( source );
		const int currentDestinationMass = mass.value( destination ) + delta.value( destination );
		const int destinationRoom = qMax( 0, capacity - currentDestinationMass );
		const int amount = qMin( requested, qMin( available, destinationRoom ) );
		if ( amount <= 0 )
			return 0;

		remaining[source] = available - amount;
		delta[source] = delta.value( source ) - amount;
		delta[destination] = delta.value( destination ) + amount;
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

	auto processHorizontalPair = [&]( unsigned int source, unsigned int neighbor, WaterFlow direction ) {
		if ( !passable( neighbor ) )
			return;
		const bool neighborIsSource = sourceSet.contains( neighbor );
		if ( neighborIsSource && source > neighbor )
			return;

		ensureMass( neighbor );
		if ( remaining.value( source ) > mass.value( neighbor ) + 1 )
		{
			addTransfer( source, neighbor, maxTransfer, direction );
		}
		else if ( neighborIsSource && remaining.value( neighbor ) > mass.value( source ) + 1 )
		{
			addTransfer( neighbor, source, maxTransfer, oppositeFlow( direction ) );
		}
	};

	for ( const unsigned int id : active )
	{
		const Neighbors n = neighbors( id, dimX, dimY, dimZ );
		processHorizontalPair( id, n.north, WF_NORTH );
		processHorizontalPair( id, n.south, WF_SOUTH );
		processHorizontalPair( id, n.east, WF_EAST );
		processHorizontalPair( id, n.west, WF_WEST );

		if ( remaining.value( id ) > capacity && passable( n.above ) && !cells[n.above].solidFloor )
			addTransfer( id, n.above, maxTransfer, WF_UP );
	}

	for ( const unsigned int id : result.touched )
	{
		if ( id >= worldTileCount )
			continue;
		const int finalMass = qBound( 0, mass.value( id ) + delta.value( id ), maxStoredMass );
		result.mass.insert( id, finalMass );
		if ( finalMass > 0 && passable( id ) )
			result.flow.insert( id, flowFlags.value( id, WF_NOFLOW ) );
		else
			result.mass[id] = 0;
	}

	return result;
}
