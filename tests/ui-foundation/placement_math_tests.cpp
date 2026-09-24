#include "gui/isometricplacement.h"

#include <array>

namespace
{

std::array<int, 2> rotateXy( int x, int y, int width, int height, int rotation )
{
	switch ( rotation )
	{
		case 1: return { height - y - 1, x };
		case 2: return { width - x - 1, height - y - 1 };
		case 3: return { y, width - x - 1 };
		default: return { x, y };
	}
}

} // namespace

int main()
{
	constexpr double originX = 360.0;
	constexpr double originY = -440.0;
	constexpr int viewLevel = 20;
	constexpr int width = 40;
	constexpr int height = 32;

	for ( const int rotation : { 0, 1, 2, 3 } )
	{
		const int renderedWidth = rotation == 1 || rotation == 3 ? height : width;
		const int renderedHeight = rotation == 1 || rotation == 3 ? width : height;
		for ( const int z : { 20, 19, 18, 15 } )
		for ( const double surfaceHeight : { 12.0, 28.0 } )
		{
			for ( int x = 4; x < 20; ++x )
			{
				for ( int y = 5; y < 25; ++y )
				{
					const auto rendered = rotateXy( x, y, width, height, rotation );
					const double tileCenterX = originX + 16.0 * ( rendered[0] - rendered[1] ) + 16.0;
					const double tileCenterY = originY - surfaceHeight + 8.0 * ( rendered[0] + rendered[1] ) + 20.0 * ( viewLevel - z );
					for ( const auto offset : { std::array<int, 2>{ 0, 0 }, std::array<int, 2>{ 8, 0 }, std::array<int, 2>{ -8, 0 }, std::array<int, 2>{ 0, 4 }, std::array<int, 2>{ 0, -4 }, std::array<int, 2>{ 4, 2 }, std::array<int, 2>{ -4, -2 } } )
					{
						const auto picked = ingnomia::ui::nearestIsometricTile(
							tileCenterX + offset[0], tileCenterY + offset[1], originX, originY,
							viewLevel - z, renderedWidth, renderedHeight, surfaceHeight );
						if ( picked.x != rendered[0] || picked.y != rendered[1] )
							return 1;
					}
				}
			}
		}
	}
	// Regression: a cube's top and its hidden floor project one diagonal tile
	// apart. The visible surface, not the lower floor, determines the click.
	const auto underground = ingnomia::ui::nearestIsometricTile(64,156,0,0,0,100,100,28);
	const auto floor = ingnomia::ui::nearestIsometricTile(64,156,0,0,0,100,100,12);
	if (underground.x != 13 || underground.y != 10 || floor.x != 12 || floor.y != 9)
		return 2;
	// Cross a diamond edge before reaching the adjacent centre, at several
	// zooms. The inverse lattice coordinates cross their half-integer boundary.
	for (const double scale : {0.5, 1.0, 1.5, 3.0})
	for (const double surface : {12.0,28.0})
	for (const double step : {0.49,0.51})
	{
		const double mx = (16 + 16 * step) * scale;
		const double my = (160 - surface + 8 * step) * scale;
		const auto pick = ingnomia::ui::nearestIsometricTile(mx/scale,my/scale,0,0,0,100,100,surface);
		if (pick.x != (step < 0.5 ? 10 : 11) || pick.y != 10) return 3;
	}
	// A below-level item remains at its real Z while its drawn top is lifted
	// onto the unchanged marker. Check screen alignment at different depths
	// and zooms, including the formerly empty gap between the marker and item.
	for (const bool wallTop : {false,true})
	for (const int componentZ : {-1,-2})
	for (const int depth : {0,1,5,12})
	for (const double scale : {0.5,1.0,1.5,3.0})
	{
		const double marker = ((wallTop ? 28.0 : 12.0) - depth*20.0)*scale;
		const double item = (28.0 - (depth-componentZ)*20.0
			+ ingnomia::ui::excavationPreviewLift(wallTop,componentZ))*scale;
		if (std::abs(marker-item) > 0.0001) return 4;
	}
	return 0;
}
