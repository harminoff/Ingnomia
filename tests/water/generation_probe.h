#pragma once

// Full-world regression probe. Unlike the reservoir fixture, this preserves
// every generated water cell, source, drain, plant and terrain column.
#include "game/newgamesettings.h"
#include "game/world.h"
#include <QElapsedTimer>
#include <memory>

inline void scheduleGeneratedWaterProbe( QApplication& app, GameManager* manager )
{
	const QString folder = qEnvironmentVariable( "INGNOMIA_WATER_GENERATION_PROBE" );
	if ( folder.isEmpty() || qEnvironmentVariable( "INGNOMIA_DATA_FOLDER" ).isEmpty() ) return;
	QDir().mkpath( folder );
	auto report = [folder]( const QString& text ) {
		QFile file( folder + "/result.txt" );
		if ( file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) file.write( ( text + "\n" ).toUtf8() );
	};
	auto check = [report]( bool ok, const QString& text ) { report( ( ok ? "PASS " : "FAIL " ) + text ); };
	auto capture = [&app, folder]( const QString& name ) {
		QTimer::singleShot( 500, &app, [folder, name]() {
			auto& window = MainWindow::getInstance();
			window.renderer()->onCenterCameraPosition( Position( Global::dimX / 2, Global::dimY / 2,
				qMin( Global::dimZ - 2, Global::newGameSettings->ground() + 6 ) ) );
			bool scaleOk = false;
			const float requestedScale = qEnvironmentVariable( "INGNOMIA_WATER_GENERATION_SCALE" ).toFloat( &scaleOk );
			window.renderer()->setScale( scaleOk ? requestedScale : 0.5f );
			qputenv( "INGNOMIA_UI_CAPTURE", ( folder + "/" + name + ".png" ).toUtf8() );
			qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
			window.armUiCapture();
		} );
	};
	auto mass = []( World* world ) {
		qint64 volume = 0;
		for ( const auto& tile : world->world() ) volume += tile.fluidLevel + tile.pressure;
		return volume;
	};
	QTimer::singleShot( 2500, manager, [manager, report, check, capture, mass]() {
		const QString seed = qEnvironmentVariable( "INGNOMIA_WATER_GENERATION_SEED", "1239626711" );
		Global::newGameSettings->setSeed( seed );
		Global::newGameSettings->setKingdomName( "WaterGenerationProbe" );
		manager->startNewGame();
		manager->setPaused( true );
		auto* world = manager->game()->w();
		// Rebuild the same active-water state used when loading a save.
		world->initWater();
		auto dryLand = std::make_shared<QVector<unsigned int>>();
		unsigned int id = 0;
		int wet = 0, highestWater = 0, lowestLand = Global::dimZ;
		for ( const auto& tile : world->world() )
		{
			const int z = id / ( Global::dimX * Global::dimY );
			if ( tile.fluidLevel || tile.pressure ) { ++wet; highestWater = qMax( highestWater, z ); }
			if ( ( tile.flags & TileFlag::TF_SUNLIGHT ) && ( tile.floorType & FT_SOLIDFLOOR ) &&
				!( tile.wallType & WT_MOVEBLOCKING ) && !tile.fluidLevel && !tile.pressure )
			{
				dryLand->append( id );
				lowestLand = qMin( lowestLand, z );
			}
			++id;
		}
		const qint64 initialMass = mass( world );
		report( QString( "seed=%1 dimensions=%2x%3x%4 ground=%5 ocean=%6 rivers=%7 flatness=%8" )
			.arg( seed ).arg( Global::dimX ).arg( Global::dimY ).arg( Global::dimZ )
			.arg( Global::newGameSettings->ground() ).arg( Global::newGameSettings->oceanSize() )
			.arg( Global::newGameSettings->rivers() ).arg( Global::newGameSettings->flatness() ) );
		report( QString( "initial mass=%1 wetCells=%2 highestWater=%3 lowestDryLand=%4 dryLandCells=%5" )
			.arg( initialMass ).arg( wet ).arg( highestWater ).arg( lowestLand ).arg( dryLand->size() ) );
		check( !dryLand->isEmpty(), "generated map contains dry land" );
		capture( "01-generated" );
		QTimer::singleShot( 2500, manager, [manager, check, report, capture, mass, dryLand, initialMass]() {
			manager->setPaused( true );
			auto* world = manager->game()->w();
			QElapsedTimer elapsed; elapsed.start();
			for ( int step = 0; step < 400; ++step ) world->processWaterFlow();
			check( mass( world ) == initialMass, "400 flow steps preserve all generated water volume" );
			for ( int step = 0; step < 200; ++step ) world->processWater();
			int flooded = 0;
			for ( const auto id : *dryLand ) if ( world->world()[id].fluidLevel || world->world()[id].pressure ) ++flooded;
			report( QString( "after 600 steps: mass=%1 floodedLandCells=%2 elapsedMs=%3" ).arg( mass( world ) ).arg( flooded ).arg( elapsed.elapsed() ) );
			check( flooded == 0, "generated water stays below previously dry land, including active river sources and drains" );
			manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo( world->updatedTiles() );
			capture( "02-simulated" );
			QTimer::singleShot( 2500, manager, [manager, check, report, capture, mass]() {
				auto* world = manager->game()->w();
				bool found = false;
				for ( int z = Global::dimZ - 3; z > 7 && !found; --z )
				for ( int y = 2; y < Global::dimY - 2 && !found; ++y )
				for ( int x = 2; x < Global::dimX - 2 && !found; ++x )
				{
					Position water( x, y, z );
					if ( world->getTile( water ).fluidLevel < 5 ) continue;
					for ( const auto bank : { water.eastOf(), water.westOf(), water.northOf(), water.southOf() } )
					{
						if ( !( world->getTile( bank ).wallType & WT_SOLIDWALL ) ) continue;
						const qint64 before = mass( world );
						world->mineWall( bank, water );
						for ( int step = 0; step < 30; ++step ) world->processWaterFlow();
						report( QString( "mined natural bank at %1,%2,%3: fluid=%4" ).arg( bank.x ).arg( bank.y ).arg( bank.z ).arg( world->getTile( bank ).fluidLevel ) );
						check( world->getTile( bank ).fluidLevel > 0 && mass( world ) == before, "mining a natural bank admits water and preserves volume" );
						found = true;
						break;
					}
				}
				check( found || mass( world ) == 0, "natural wet bank is available for breach regression" );
				manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo( world->updatedTiles() );
				capture( "03-natural-breach" );
				QTimer::singleShot( 2500, manager, [report]() {
					report( "COMPLETE" );
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
			} );
		} );
	} );
}
