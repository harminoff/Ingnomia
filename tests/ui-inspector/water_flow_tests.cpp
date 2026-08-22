#include "game/waterflowsolver.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <cstdlib>
#include <iostream>

namespace
{
unsigned int id( int x, int y, int z, int dimX, int dimY )
{
	return static_cast<unsigned int>( x + y * dimX + z * dimX * dimY );
}

void require( bool condition, const char* message )
{
	if ( condition )
		return;
	std::cerr << "water_flow_tests: " << message << '\n';
	std::exit( EXIT_FAILURE );
}

int totalMass( const QHash<unsigned int, int>& mass )
{
	int total = 0;
	for ( const int value : mass )
		total += value;
	return total;
}
}

int main( int argc, char** argv )
{
	QCoreApplication app( argc, argv );
	constexpr int dimX = 4;
	constexpr int dimY = 4;
	constexpr int dimZ = 4;
	const unsigned int source = id( 1, 1, 2, dimX, dimY );
	const unsigned int below = id( 1, 1, 1, dimX, dimY );
	const unsigned int boundary = id( 0, 1, 2, dimX, dimY );

	QHash<unsigned int, WaterFlowCell> cells;
	for ( int z = 0; z < dimZ; ++z )
	{
		for ( int y = 0; y < dimY; ++y )
		{
			for ( int x = 0; x < dimX; ++x )
			{
				WaterFlowCell& cell = cells[id( x, y, z, dimX, dimY )];
				cell.boundary = x == 0 || y == 0 || z == 0 || x == dimX - 1 || y == dimY - 1 || z == dimZ - 1;
			}
		}
	}
	// Keep the first assertion focused on gravity; the other two horizontal
	// neighbours are interior cells in this small fixture.
	cells[id( 2, 1, 2, dimX, dimY )].moveBlocking = true;
	cells[id( 1, 2, 2, dimX, dimY )].moveBlocking = true;

	// Downward flow is mass-conserving and never escapes the map boundary.
	cells[source].mass = 6;
	QSet<unsigned int> active { source };
	const WaterFlowResult downward = solveWaterFlow( cells, dimX, dimY, dimZ, active );
	require( totalMass( downward.mass ) == 6, "downward flow must conserve mass" );
	require( downward.mass.value( source ) == 5, "source should lose one unit per edge per tick" );
	require( downward.mass.value( below ) == 1, "receiver should gain one downward unit" );
	require( downward.mass.value( boundary ) == 0, "boundary cells must not receive water" );
	require( downward.nextActive.contains( below ), "newly wetted cells must remain active" );

	// A blocking wall is a physical water boundary.
	cells[source].mass = 6;
	cells[below].mass = 0;
	cells[below].moveBlocking = true;
	const WaterFlowResult blocked = solveWaterFlow( cells, dimX, dimY, dimZ, active );
	require( totalMass( blocked.mass ) == 6, "blocked flow must conserve mass" );
	require( blocked.mass.value( source ) == 6, "water must remain behind a blocking wall" );
	require( blocked.mass.value( below ) == 0, "blocking wall must not receive water" );
	require( blocked.nextActive.isEmpty(), "settled water should leave the active frontier" );
	cells[below].moveBlocking = false;

	// A level basin sleeps, then a newly opened side redistributes and settles.
	const unsigned int chamber = id( 2, 1, 2, dimX, dimY );
	cells[source].solidFloor = true;
	cells[chamber].solidFloor = true;
	cells[chamber].moveBlocking = false;
	cells[id( 1, 2, 2, dimX, dimY )].moveBlocking = true;
	cells[id( 2, 2, 2, dimX, dimY )].moveBlocking = true;
	cells[source].mass = 5;
	cells[chamber].mass = 5;
	QSet<unsigned int> basin { source, chamber };
	const WaterFlowResult settled = solveWaterFlow( cells, dimX, dimY, dimZ, basin );
	require( settled.nextActive.isEmpty(), "an equal basin should leave the active frontier" );

	cells[source].mass = 10;
	cells[chamber].mass = 0;
	QSet<unsigned int> flowing = basin;
	for ( int tick = 0; tick < 12 && !flowing.isEmpty(); ++tick )
	{
		const WaterFlowResult step = solveWaterFlow( cells, dimX, dimY, dimZ, flowing );
		for ( auto it = step.mass.cbegin(); it != step.mass.cend(); ++it )
			cells[it.key()].mass = it.value();
		require( cells[source].mass + cells[chamber].mass == 10, "horizontal free-flow must conserve finite mass" );
		flowing = step.nextActive;
	}
	require( cells[source].mass == 5 && cells[chamber].mass == 5, "breached basin should equalize" );
	require( flowing.isEmpty(), "equalized breach should settle back to sleep" );

	QSet<unsigned int> reverse;
	reverse.insert( chamber );
	reverse.insert( source );
	cells[source].mass = 8;
	cells[chamber].mass = 2;
	const WaterFlowResult deterministicA = solveWaterFlow( cells, dimX, dimY, dimZ, basin );
	const WaterFlowResult deterministicB = solveWaterFlow( cells, dimX, dimY, dimZ, reverse );
	require( deterministicA.mass == deterministicB.mass && deterministicA.flow == deterministicB.flow,
		"frontier insertion order must not change the result" );

	// Pressure is the overflow channel and rises one unit at a time.
	cells.clear();
	for ( int z = 0; z < dimZ; ++z )
	{
		for ( int y = 0; y < dimY; ++y )
		{
			for ( int x = 0; x < dimX; ++x )
			{
				WaterFlowCell& cell = cells[id( x, y, z, dimX, dimY )];
				cell.boundary = x == 0 || y == 0 || z == 0 || x == dimX - 1 || y == dimY - 1 || z == dimZ - 1;
			}
		}
	}
	cells[id( 2, 1, 2, dimX, dimY )].moveBlocking = true;
	cells[id( 1, 2, 2, dimX, dimY )].moveBlocking = true;
	cells[source].mass = 11;
	cells[source].solidFloor = true;
	const WaterFlowResult pressure = solveWaterFlow( cells, dimX, dimY, dimZ, active );
	const unsigned int above = id( 1, 1, 3, dimX, dimY );
	// z=3 is the outer boundary, so the pressure must remain contained.
	require( totalMass( pressure.mass ) == 11, "pressure must conserve mass at map edges" );
	require( pressure.mass.value( source ) == 11, "pressure must not leak through a map boundary" );
	require( pressure.mass.value( above ) == 0, "map boundary must contain upward pressure" );

	// Re-running the same snapshot must produce equivalent state.
	const WaterFlowResult repeat = solveWaterFlow( cells, dimX, dimY, dimZ, active );
	require( repeat.mass == pressure.mass, "solver must be deterministic" );
	require( repeat.flow == pressure.flow, "flow flags must be deterministic" );

	// A default-sized world still visits only the explicitly supplied active halo.
	QHash<unsigned int, WaterFlowCell> sparse;
	constexpr int largeX = 100;
	constexpr int largeY = 100;
	constexpr int largeZ = 130;
	const unsigned int sparseSource = id( 50, 50, 65, largeX, largeY );
	sparse.insert( sparseSource, WaterFlowCell { 10, false, false, false } );
	sparse.insert( sparseSource - 1, WaterFlowCell {} );
	sparse.insert( sparseSource + 1, WaterFlowCell {} );
	sparse.insert( sparseSource - largeX, WaterFlowCell {} );
	sparse.insert( sparseSource + largeX, WaterFlowCell {} );
	sparse.insert( sparseSource - largeX * largeY, WaterFlowCell {} );
	sparse.insert( sparseSource + largeX * largeY, WaterFlowCell {} );
	const WaterFlowResult bounded = solveWaterFlow( sparse, largeX, largeY, largeZ, { sparseSource } );
	require( bounded.visitedCellCount == 7, "solver work must be bounded by active cells plus halo" );

	std::cout << "water_flow_tests: all checks passed\n";
	return EXIT_SUCCESS;
}
