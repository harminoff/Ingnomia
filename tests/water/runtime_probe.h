#pragma once

// Explicit opt-in World/renderer test; use a copied save and isolated data root.
#include "game/world.h"
#include "gfx/spritefactory.h"
#include "gfx/sprite.h"
#include <QElapsedTimer>
#include "generation_probe.h"
#include "shoreline_probe.h"

inline void scheduleWaterRuntimeProbe( QApplication& app, GameManager* manager )
{
	scheduleGeneratedWaterProbe( app, manager );
	scheduleWaterShorelineProbe( app, manager );
	const QString folder = qEnvironmentVariable( "INGNOMIA_WATER_PROBE" );
	if ( folder.isEmpty() ) return;
	QDir().mkpath( folder );
	auto report = [folder]( const QString& text ) {
		QFile file( folder + "/result.txt" );
		if ( file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) file.write( ( text + "\n" ).toUtf8() );
	};
	auto check = [report]( bool ok, const QString& text ) { report( ( ok ? "PASS " : "FAIL " ) + text ); };
	auto capture = [&app, folder]( int delay, const QString& name, int rotation = 0 ) {
		QTimer::singleShot( delay, &app, [folder, name, rotation]() {
			auto& window = MainWindow::getInstance();
			if ( rotation ) window.renderer()->rotate( rotation );
			window.renderer()->onCenterCameraPosition( Position( 48, 47, 90 ) );
			window.renderer()->setScale( 1.2f );
			qputenv( "INGNOMIA_UI_CAPTURE", ( folder + "/" + name + ".png" ).toUtf8() );
			qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
			window.armUiCapture();
		} );
	};
	auto volume = []( World* world ) {
		int mass = 0;
		for ( int z = 88; z <= 90; ++z ) for ( int y = 34; y <= 60; ++y ) for ( int x = 30; x <= 64; ++x )
		{ const auto& tile = world->getTile( x, y, z ); mass += tile.fluidLevel + tile.pressure; }
		return mass;
	};
	auto publish = [manager]() {
		manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo( manager->game()->w()->updatedTiles() );
	};
	QTimer::singleShot( 9000, manager, [manager, check, volume, publish]() {
		if ( !manager->game() || Global::dimX < 66 || Global::dimY < 66 || Global::dimZ < 95 )
		{ check( false, "loaded world must be at least 66x66x95" ); return; }
		manager->setPaused( true );
		auto* game = manager->game();
		auto* world = game->w();
		// The copied landscape may contain water above the fixture's elevation.
		// Remove those external sources so this test measures only its reservoir.
		unsigned int tileID = 0;
		for ( Tile& tile : world->world() )
		{
			if ( tile.fluidLevel || tile.pressure || ( tile.flags & TileFlag::TF_WATER ) ) world->addToUpdateList( tileID );
			tile.fluidLevel = 0;
			tile.pressure = 0;
			tile.flow = WF_NOFLOW;
			tile.flags -= TileFlag::TF_WATER + TileFlag::TF_AQUIFIER + TileFlag::TF_DEAQUIFIER;
			++tileID;
		}
		const auto floor = game->sf()->createSprite( "RoughFloor", { "Sand" } )->uID;
		const auto wall = game->sf()->createSprite( "RoughWall", { "Dirt" } )->uID;
		for ( int z = 88; z < Global::dimZ - 1; ++z ) for ( int y = 34; y <= 60; ++y ) for ( int x = 30; x <= 64; ++x )
		{
			Tile& tile = world->getTile( x, y, z );
			tile = Tile {};
			tile.flags = TileFlag::TF_SUNLIGHT;
			if ( z <= 90 )
			{
				tile.floorType = FT_SOLIDFLOOR;
				tile.floorMaterial = Global::dirtUID;
				tile.floorSpriteUID = floor;
				tile.flags += TileFlag::TF_WALKABLE;
			}
			const bool bank = ( x >= 39 && x <= 48 && ( y == 43 || y == 52 ) ) ||
				( y >= 43 && y <= 52 && ( x == 39 || x == 48 ) ) ||
				( x >= 48 && x <= 57 && ( y == 46 || y == 49 ) ) || ( x == 57 && y >= 46 && y <= 49 );
			if ( z < 90 || ( z == 90 && bank ) )
			{
				tile.wallType = static_cast<WallType>( WT_SOLIDWALL | WT_MOVEBLOCKING | WT_VIEWBLOCKING );
				tile.wallMaterial = Global::dirtUID;
				tile.wallSpriteUID = wall;
				tile.flags -= TileFlag::TF_WALKABLE;
			}
			if ( z == 90 && x >= 40 && x <= 47 && y >= 44 && y <= 51 ) tile.fluidLevel = 10;
			world->addToUpdateList( x, y, z );
		}
		world->initWater();
		world->processWaterFlow();
		check( volume( world ) == 640, "closed reservoir contains 640 units" );
		publish();
	} );
	capture( 10500, "01-contained" );
	QTimer::singleShot( 12000, manager, [manager, check, report, volume, publish]() {
		if ( !manager->game() ) return;
		auto* world = manager->game()->w();
		Position work( 49, 47, 90 );
		world->mineWall( Position( 48, 47, 90 ), work );
		world->mineWall( Position( 48, 48, 90 ), work );
		world->processWaterFlow();
		report( QString( "breach mass=%1 level=%2" ).arg( volume( world ) ).arg( world->getTile( 48, 47, 90 ).fluidLevel ) );
		check( world->getTile( 48, 47, 90 ).fluidLevel > 0 && volume( world ) == 640,
			"mineWall wakes sleeping water and wets breach on next tick" );
		publish();
	} );
	capture( 13000, "02-breach" );
	QTimer::singleShot( 14500, manager, [manager, check, report, volume, publish]() {
		if ( !manager->game() ) return;
		auto* world = manager->game()->w();
		QElapsedTimer timer; timer.start();
		for ( int tick = 0; tick < 80; ++tick ) world->processWaterFlow();
		report( QString( "80 World water steps: %1 ms" ).arg( timer.elapsed() ) );
		report( QString( "settled mass=%1 farLevel=%2" ).arg( volume( world ) ).arg( world->getTile( 56, 47, 90 ).fluidLevel ) );
		check( world->getTile( 56, 47, 90 ).fluidLevel > 0 && volume( world ) == 640,
			"water reaches far channel with finite volume 640" );
		int low = 10, high = 0;
		for ( int y = 44; y <= 51; ++y ) for ( int x = 40; x <= 56; ++x )
		{
			const int level = world->getTile( x, y, 90 ).fluidLevel;
			if ( level ) { low = qMin( low, level ); high = qMax( high, level ); }
		}
		check( high - low <= 1, "reservoir and channel share a level surface" );
		publish();
	} );
	capture( 16000, "03-settled" );
	capture( 18000, "04-rotation1", 1 );
	capture( 20000, "05-rotation2", 1 );
	capture( 22000, "06-rotation3", 1 );
	QTimer::singleShot( 24000, manager, [manager, check, volume, publish]() {
		if ( !manager->game() ) return;
		auto* world = manager->game()->w();
		world->changeFluidLevel( Position( 40, 44, 90 ), -2 );
		for ( int tick = 0; tick < 30; ++tick ) world->processWaterFlow();
		check( volume( world ) == 638 && world->getTile( 40, 44, 90 ).fluidLevel >= 7,
			"withdrawal wakes and refills from neighboring water" );
		Tile& lower = world->getTile( 56, 47, 89 );
		lower.wallType = WT_NOWALL;
		lower.wallSpriteUID = 0;
		world->removeFloor( Position( 56, 47, 90 ), Position( 55, 47, 90 ) );
		for ( int tick = 0; tick < 30; ++tick ) world->processWaterFlow();
		check( lower.fluidLevel == 10 && volume( world ) == 638, "removeFloor opens gravity drain and conserves volume" );
		world->initWater();
		for ( int tick = 0; tick < 30; ++tick ) world->processWaterFlow();
		check( volume( world ) == 638, "load initialization preserves moved volume" );
		publish();
	} );
	QTimer::singleShot( 27000, &app, []() {
		if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
	} );
}
