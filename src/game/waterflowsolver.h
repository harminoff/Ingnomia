/*
    This file is part of Ingnomia.
    See the project license for details.
*/
/** @file waterflowsolver.h
 *  @brief Deterministic, mass-conserving water-flow step independent of World.
 */

#pragma once

#include "../base/tile.h"

#include <QHash>
#include <QSet>
#include <QVector>

/// @brief The simulation state required by one water cell.
///
/// `mass` is fluidLevel + pressure.  Keeping the solver in mass units avoids
/// the old underflow/overflow class of bugs while World still stores the
/// legacy surface/pressure pair for save compatibility.
struct WaterFlowCell
{
	int mass = 0;
	bool moveBlocking = false;
	bool solidFloor = false;
	bool boundary = false;
};

/// @brief Tunable, gameplay-facing limits for one fixed simulation step.
struct WaterFlowConfig
{
	int surfaceCapacity = 10;
	int maxStoredMass = 265;
	int maxTransferPerEdge = 1;
};

/// @brief Result of one deterministic water-flow step.
struct WaterFlowResult
{
	QHash<unsigned int, int> mass;
	QHash<unsigned int, WaterFlow> flow;
	QSet<unsigned int> touched;
	QSet<unsigned int> nextActive;
	int visitedCellCount = 0;
};

/// @brief Simulates one fixed-tick water step over a flat X/Y/Z grid.
///
/// The input is a stable snapshot.  Downward flow is resolved first, then
/// horizontal gradients are processed once per pair, and finally pressure can
/// move upward.  No transfer can exceed receiver capacity or source mass.
WaterFlowResult solveWaterFlow( const QHash<unsigned int, WaterFlowCell>& cells,
	int dimX,
	int dimY,
	int dimZ,
	const QSet<unsigned int>& activeWater,
	const WaterFlowConfig& config = {} );
