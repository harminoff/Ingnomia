/*
    This file is part of Ingnomia https://github.com/rschurade/Ingnomia
    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.
*/
/** @file isometricplacement.h
 *  @brief Shader-matched screen-to-tile math for the placement cursor.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace ingnomia::ui
{

struct IsometricTilePick
{
	int x = 0;
	int y = 0;
};

// Stairs/ramps have a top-face anchor at height 28 in their atlas sprite.
// Lift the buried ghost to the selected surface for presentation only. Its
// Position and the excavation job still refer to the original depth.
inline constexpr float excavationPreviewLift( bool wallTop, int componentZOffset )
{
	return ( wallTop ? 28.f : 12.f ) - ( 28.f + componentZOffset * 20.f );
}

/// @brief Finds the rendered tile whose projected centre is nearest the pointer.
/// @param renderedMouseX Mouse X in the renderer's unscaled coordinate space.
/// @param renderedMouseY Mouse Y in the renderer's unscaled coordinate space.
/// @param screenOriginX Screen-space X origin used by the orthographic camera.
/// @param screenOriginY Screen-space Y origin used by the orthographic camera.
/// @param zDifference Number of levels below the visible top level.
/// @param renderedWidth Rendered map width after camera rotation.
/// @param renderedHeight Rendered map height after camera rotation.
/// @return Bounded rendered-map tile coordinates.
inline IsometricTilePick nearestIsometricTile(
	double renderedMouseX,
	double renderedMouseY,
	double screenOriginX,
	double screenOriginY,
	int zDifference,
	int renderedWidth,
	int renderedHeight,
	double surfaceHeight = 12.0 )
{
	if ( renderedWidth <= 0 || renderedHeight <= 0 )
		return {};

	// Invert the visible diamond, NOT the bounding quad. SpritePixmap pads the
	// 32x36 terrain sprites by 16 rows in a 32x64 atlas slot. The floor diamond
	// centre is atlas row 40 (height 12); a wall's top is row 24 (height 28).
	// world_v.glsl projects an atlas row r at height (64-r)-12:
	// x = 16 * (tileX - tileY) + 16
	// y = -8 * (tileX + tileY) - 20 * zDifference + surfaceHeight
	const double tileDeltaX = ( renderedMouseX - screenOriginX - 16.0 ) / 16.0;
	const double tileDeltaY = ( renderedMouseY - ( screenOriginY - surfaceHeight ) - zDifference * 20.0 ) / 8.0;
	const double renderedX = ( tileDeltaX + tileDeltaY ) * 0.5;
	const double renderedY = ( tileDeltaY - tileDeltaX ) * 0.5;

	IsometricTilePick result;
	double bestDistance = std::numeric_limits<double>::max();
	const int candidateX[2] = { static_cast<int>( std::floor( renderedX ) ), static_cast<int>( std::ceil( renderedX ) ) };
	const int candidateY[2] = { static_cast<int>( std::floor( renderedY ) ), static_cast<int>( std::ceil( renderedY ) ) };
	for ( const int candidateTileX : candidateX )
	{
		for ( const int candidateTileY : candidateY )
		{
			const int boundedX = std::max( 0, std::min( renderedWidth - 1, candidateTileX ) );
			const int boundedY = std::max( 0, std::min( renderedHeight - 1, candidateTileY ) );
			const double distance = std::abs( ( boundedX - boundedY ) - tileDeltaX )
				+ std::abs( ( boundedX + boundedY ) - tileDeltaY );
			if ( distance < bestDistance )
			{
				bestDistance = distance;
				result.x = boundedX;
				result.y = boundedY;
			}
		}
	}
	return result;
}

} // namespace ingnomia::ui
