#pragma once

// Opt-in rendering fixture: two-deep water, a concave shore, an island,
// foreground banks, and exposed map-facing sides. Does not advance simulation.
#include "game/world.h"
#include "gfx/spritefactory.h"
#include "gfx/sprite.h"

inline void scheduleWaterShorelineProbe( QApplication& app, GameManager* manager )
{
	const QString folder = qEnvironmentVariable( "INGNOMIA_WATER_SHORELINE_PROBE" );
	if ( folder.isEmpty() ) return;
	QDir().mkpath( folder );
	QTimer::singleShot( 9000, manager, [manager, folder]() {
		if ( !manager->game() || Global::dimX < 66 || Global::dimY < 66 || Global::dimZ < 95 ) return;
		manager->setPaused( true );
		auto* game = manager->game();
		auto* world = game->w();
		const auto sand = game->sf()->createSprite( "RoughFloor", { "Sand" } )->uID;
		const auto dirt = game->sf()->createSprite( "RoughFloor", { "Dirt" } )->uID;
		const auto wall = game->sf()->createSprite( "RoughWall", { "Dirt" } )->uID;
		bool levelOk = false;
		const int requestedLevel = qEnvironmentVariable( "INGNOMIA_WATER_SHORELINE_LEVEL" ).toInt( &levelOk );
		const int surfaceLevel = levelOk ? qBound( 1, requestedLevel, 10 ) : 10;
		const auto flow = static_cast<WaterFlow>( qBound( 0, qEnvironmentVariableIntValue( "INGNOMIA_WATER_SHORELINE_FLOW" ), 15 ) );
		for ( int z = 87; z < Global::dimZ; ++z )
		for ( int y = 33; y <= 64; ++y ) for ( int x = 30; x <= 63; ++x )
		{
			Tile& tile = world->getTile( x, y, z );
			tile = Tile {};
			tile.flags = TileFlag::TF_SUNLIGHT;
			const bool pool = x >= 40 && x <= 50 && y >= 42 && y <= 53 &&
				!( x >= 47 && y >= 49 ) && !( x == 43 && y == 46 );
			if ( z <= ( pool ? 88 : 90 ) )
			{
				tile.wallType = static_cast<WallType>( WT_SOLIDWALL | WT_MOVEBLOCKING | WT_VIEWBLOCKING );
				tile.wallSpriteUID = wall;
				tile.wallMaterial = Global::dirtUID;
				tile.floorType = FT_SOLIDFLOOR;
				tile.floorSpriteUID = dirt;
			}
			if ( z == ( pool ? 89 : 91 ) )
			{
				tile.floorType = FT_SOLIDFLOOR;
				tile.floorSpriteUID = pool ? sand : dirt;
				tile.floorMaterial = Global::dirtUID;
				tile.flags += TileFlag::TF_WALKABLE;
			}
			if ( pool && ( z == 89 || z == 90 ) )
			{
				tile.fluidLevel = z == 90 ? surfaceLevel : 10;
				tile.flags += TileFlag::TF_WATER;
				tile.flow = flow;
			}
			world->addToUpdateList( x, y, z );
		}
		manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo( world->updatedTiles() );
		QFile result( folder + "/result.txt" );
		if ( result.open( QIODevice::WriteOnly ) ) result.write( "Concave bank and island fixture ready; simulation paused.\n" );
	} );
	const bool animation = qEnvironmentVariableIntValue( "INGNOMIA_WATER_APPEARANCE_PROBE" ) != 0;
	if ( animation )
	{
		QTimer::singleShot( 11000, &app, []() {
			auto& window = MainWindow::getInstance();
			window.onUiSetViewLevel( 91 );
			window.renderer()->onCenterCameraPosition( Position( 46, 48, 91 ) );
			window.renderer()->setScale( 2.5f );
		} );
		// A full six-second advection cycle, using real framebuffers at a fixed
		// camera. Geometry and simulation stay fixed so motion is shader-only.
		for ( int frame = 0; frame <= 60; ++frame )
		{
			QTimer::singleShot( 12000 + frame * 100, &app, [folder, frame]() {
				qputenv( "INGNOMIA_UI_CAPTURE", ( folder + QString( "/frame-%1.png" ).arg( frame, 3, 10, QChar( '0' ) ) ).toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
			} );
		}
	}
	for ( int rotation = 0; rotation < 4; ++rotation )
	{
		QTimer::singleShot( ( animation ? 21000 : 11500 ) + rotation * 2000, &app, [folder, rotation]() {
			auto& window = MainWindow::getInstance();
			if ( rotation ) window.renderer()->rotate( 1 );
			window.onUiSetViewLevel( 91 );
			Position center( 46, 48, 91 );
			switch ( rotation )
			{
				case 1: center = Position( Global::dimY - 49, 46, 91 ); break;
				case 2: center = Position( Global::dimX - 47, Global::dimY - 49, 91 ); break;
				case 3: center = Position( 48, Global::dimX - 47, 91 ); break;
			}
			window.renderer()->onCenterCameraPosition( center );
			window.renderer()->setScale( 2.5f );
			qputenv( "INGNOMIA_UI_CAPTURE", ( folder + "/rotation-" + QString::number( rotation ) + ".png" ).toUtf8() );
			qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
			window.armUiCapture();
		} );
	}
	QTimer::singleShot( animation ? 30000 : 20000, &app, []() {
		if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
	} );
}
