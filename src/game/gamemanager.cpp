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
/** @file gamemanager.cpp
 *  @brief Game lifecycle management: new game creation, save/load, pause, speed control.
 */
#include "gamemanager.h"

#include "../base/config.h"
#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/io.h"
#include "../base/pathfinder.h"
#include "../base/util.h"
#include "../base/selection.h"

#include "../game/game.h"
#include "../game/mechanismmanager.h"
#include "../game/militarymanager.h"
#include "../game/newgamesettings.h"
#include "../game/world.h"
#include "../gfx/spritefactory.h"
#include "../gui/eventconnector.h"
#include "../gui/mainwindow.h"
#include "../gui/mainwindowrenderer.h"
#include "../gui/strings.h"

#include "../game/inventory.h"
#include "../game/creaturemanager.h"
#include "../game/eventmanager.h"
#include "../game/farmingmanager.h"
#include "../game/fluidmanager.h"
#include "../game/gnomemanager.h"
#include "../game/mechanismmanager.h"
#include "../game/roommanager.h"
#include "../game/soundmanager.h"
#include "../game/stockpilemanager.h"
#include "../game/workshopmanager.h"


#include "../gui/aggregatoragri.h"
#include "../gui/aggregatorcreatureinfo.h"
#include "../gui/aggregatordebug.h"
#include "../gui/aggregatorinventory.h"
#include "../gui/aggregatorpopulation.h"
#include "../gui/aggregatorrenderer.h"
#include "../gui/aggregatorstockpile.h"
#include "../gui/aggregatortileinfo.h"
#include "../gui/aggregatorworkshop.h"
#include "../gui/aggregatorneighbors.h"
#include "../gui/aggregatormilitary.h"
#include "../gui/aggregatorsettings.h"
#include "../gui/aggregatorloadgame.h"
#include "../gui/aggregatorselection.h"
#include "../gui/aggregatorsound.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
void lifecycleTrace( const QString& message )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_LIFECYCLE_TRACE_PATH" );
	if ( path.isEmpty() ) return;
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) return;
	QTextStream stream( &file );
	stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs ) << " " << message << "\n";
}
}

/** @brief Constructs the GameManager, initializes event connector, utility, and new game settings.
 *  @param parent The parent QObject.
 */
GameManager::GameManager( QObject* parent ) :
	QObject( parent )
{
	qRegisterMetaType<GameSpeed>();

	m_eventConnector = new EventConnector( this );
	Global::eventConnector = m_eventConnector;
	// Keep the simulation heartbeat acknowledged while the Qt/RmlUi frontend is
	// active.  The legacy frontend used to provide this acknowledgement from
	// its view layer; after that layer was removed, the loop stopped after 20
	// heartbeats even though Game::m_paused had already become false.  The
	// EventConnector lives with GameManager on the game thread, so a direct
	// self-connection is safe and preserves the existing signal for observers.
	connect( m_eventConnector, &EventConnector::signalHeartbeat,
		m_eventConnector, &EventConnector::onHeartbeatResponse, Qt::DirectConnection );
	Global::util = new Util( nullptr );

	Global::newGameSettings = new NewGameSettings( this );

	GameState::init();

}

/** @brief Destructor. Deletes the active game instance if one exists. */
GameManager::~GameManager()
{
	if ( m_game )
	{
		delete m_game;
	}
}

/** @brief Returns the event connector used for GUI signal routing.
 *  @return Pointer to the EventConnector instance.
 */
EventConnector* GameManager::eventConnector()
{
	return m_eventConnector;
}

/** @brief Shows or hides the main menu, pausing the game when shown.
 *  @param value True to show menu, false to hide it.
 */
void GameManager::setShowMainMenu( bool value )
{
	m_eventConnector->emitPause( true );
	m_eventConnector->emitInMenu( value );

	if( m_game )
	{
		m_game->setPaused( true );
	}
}

/** @brief Ends the current game session, cleaning up the game instance and resetting globals. */
void GameManager::endCurrentGame()
{
	lifecycleTrace( "endCurrentGame begin" );
	// Put every GUI projection back into menu mode before destroying the world.
	// This is intentionally emitted synchronously by the MainWindow connection so
	// queued RmlUi/aggregator updates cannot observe a half-destroyed Game.
	m_eventConnector->emitInMenu( true );
	lifecycleTrace( "endCurrentGame menu entered" );
	m_eventConnector->emitStopGame();
	lifecycleTrace( "endCurrentGame stop emitted" );

	if ( m_game )
	{
		// Stop the heartbeat timer explicitly and clear the connector's raw pointer
		// before QObject destruction.  The old path left m_game dangling, causing a
		// subsequent load to double-delete it and allowing late commands to target
		// the retired world.
		m_game->stop();
		lifecycleTrace( "endCurrentGame game stopped" );
		m_eventConnector->setGamePtr( nullptr );
		lifecycleTrace( "endCurrentGame connector cleared" );
		delete m_game;
		lifecycleTrace( "endCurrentGame game deleted" );
		m_game = nullptr;
		Global::sel = nullptr;
		Global::util = new Util( nullptr );
	}
	lifecycleTrace( "endCurrentGame complete" );
}

/** @brief Starts a new game: saves settings, creates the world, and resumes. */
void GameManager::startNewGame()
{
	qDebug() << "GameManger: New game";
	lifecycleTrace( "startNewGame begin" );
	m_eventConnector->emitWorldTransitionStarted( true );

	// create new random kingdom name

	// save current settings for fast create new game
	Global::newGameSettings->save();
	lifecycleTrace( "startNewGame settings saved" );

	// check if folder exists, set new save folder name if yes
	createNewGame();
	lifecycleTrace( "startNewGame createNewGame complete" );

	m_eventConnector->sendResume();
	lifecycleTrace( "startNewGame resume sent" );
}

void GameManager::startTutorial()
{
	qDebug() << "GameManager: Interactive tutorial";
	lifecycleTrace( "startTutorial begin" );
	m_eventConnector->emitWorldTransitionStarted( true );
	auto* settings = new NewGameSettings( this );
	settings->setKingdomName( "Tutorial Valley" );
	settings->setSeed( "ingnomia-tutorial-v1" );
	settings->setWorldSize( 64 );
	settings->setZLevels( 100 );
	settings->setGround( 70 );
	settings->setFlatness( 20 );
	settings->setOceanSize( 0 );
	settings->setRivers( 0 );
	settings->setRiverSize( 0 );
	settings->setNumGnomes( 5 );
	settings->setStartZone( 8 );
	settings->setTreeDensity( 0 );
	settings->setPlantDensity( 0 );
	settings->setNumWildAnimals( 0 );
	settings->setPeaceful( true );
	settings->addStartingItem( "Pickaxe", "Pine", "Pine", 2 );
	settings->addStartingItem( "FellingAxe", "Pine", "Pine", 1 );
	settings->addStartingItem( "RawWood", "Pine", QString(), 32 );
	settings->addStartingItem( "RawStone", "Granite", QString(), 16 );
	settings->addStartingItem( "Bed", "Pine", "Pine", 5 );
	settings->addStartingItem( "Chair", "Pine", QString(), 1 );
	settings->addStartingItem( "Table", "Pine", QString(), 1 );
	settings->addStartingItem( "Knife", "Granite", QString(), 1 );
	settings->addStartingItem( "Bread", "Wheat", QString(), 8 );
	settings->addStartingItem( "Grain", "Wheat", QString(), 16 );
	settings->addStartingItem( "Seed", "Strawberry", QString(), 16 );
	createNewGame( GameStartMode::InteractiveTutorial, settings );
	settings->deleteLater();
	// The first lesson is intentionally presented paused.  The player can use
	// the normal pause control to begin, while the normal new-game path still
	// resumes immediately.
	if( !m_game || !m_game->tutorial() || !m_game->tutorial()->active() )
		m_eventConnector->sendResume();
	else
		m_eventConnector->emitPause( true );
	lifecycleTrace( "startTutorial complete" );
}

/** @brief Placeholder for new game setup logic (checking save folder existence). */
void GameManager::setUpNewGame()
{
	// check if folder exists, set new save folder name if yes
}

/** @brief Loads the most recently saved game, sorted by modification time. */
void GameManager::continueLastGame()
{
	//get last save
	QString folder = IO::getDataFolder() + "/save/";

	QDir dir( folder );
	dir.setFilter( QDir::Dirs | QDir::NoDotAndDotDot );
	dir.setSorting( QDir::Time );
	if ( !dir.entryList().isEmpty() )
	{
		auto kingdomDir = dir.entryList().first();

		folder = IO::getDataFolder() + "/save/" + kingdomDir + "/";
		QDir dir2( folder );
		dir2.setFilter( QDir::Dirs | QDir::NoDotAndDotDot );
		dir2.setSorting( QDir::Time );
		if ( !dir2.entryList().isEmpty() )
		{
			auto gameDir = dir2.entryList().first();

			if ( IO::saveCompatible( folder + gameDir + "/" ) )
			{
				loadGame( folder + gameDir + "/" );
				return;
			}
		}
	}
	m_eventConnector->sendLoadGameDone( false );
}

/** @brief Resets global state and reinitializes GameState and translation strings. */
void GameManager::init()
{
	lifecycleTrace( "init begin" );
	m_eventConnector->emitStopGame();

	if ( m_game )
	{
		delete m_game;
	}
	// reset everything and initialize components;
	Global::reset();
	lifecycleTrace( "init global reset complete" );

	GameState::init();
	lifecycleTrace( "init gamestate complete" );

	if ( !S::gi().init() )
	{
		qDebug() << "Failed to init translation.";
		abort();
	}
	lifecycleTrace( "init complete" );
}

/** @brief Loads a saved game from the specified folder.
 *  @param folder Path to the save directory.
 */
void GameManager::loadGame( QString folder )
{
	m_eventConnector->emitWorldTransitionStarted( false );
	init();

	m_game = new Game( this );
	m_eventConnector->setGamePtr( m_game );

	IO io( m_game, this) ;
	connect( &io, &IO::signalStatus, this, &GameManager::onGeneratorMessage );
	if ( io.load( folder ) )
	{
		m_game->tutorial()->deserialize( GameState::tutorial );
		Global::util = new Util( m_game );
		Global::sel = new Selection( m_game );

		postCreationInit();
		m_eventConnector->sendLoadGameDone( true );
		m_eventConnector->emitWorldTransitionFinished( true );
	}
	else
	{
		qDebug() << "failed to load";
		m_eventConnector->sendLoadGameDone( false );
		m_eventConnector->emitWorldTransitionFinished( false );
	}
}

/** @brief Creates a new game: initializes state, generates the world, and sets up globals. */
void GameManager::createNewGame( GameStartMode mode, NewGameSettings* settings )
{
	lifecycleTrace( "createNewGame begin" );
	init();
	lifecycleTrace( "createNewGame after init" );
	m_game = new Game( this );
	m_eventConnector->setGamePtr( m_game );
	lifecycleTrace( "createNewGame game allocated" );
	m_game->generateWorld( settings ? settings : Global::newGameSettings, mode );
	lifecycleTrace( "createNewGame generateWorld complete" );

	Global::util = new Util( m_game );
	Global::sel = new Selection( m_game );

	GameState::peaceful = ( settings ? settings : Global::newGameSettings )->isPeaceful();

	postCreationInit();
	lifecycleTrace( "createNewGame postCreationInit complete" );
	m_eventConnector->emitWorldTransitionFinished( true );
}


/** @brief Post-creation initialization: connects signals between game subsystems and GUI aggregators. */
void GameManager::postCreationInit()
{
	lifecycleTrace( "postCreationInit begin" );
	m_game->mil()->init();

	m_eventConnector->aggregatorAgri()->init( m_game );
	m_eventConnector->aggregatorCreatureInfo()->init( m_game );
	m_eventConnector->aggregatorInventory()->init( m_game );
	m_eventConnector->aggregatorMilitary()->init( m_game );
	m_eventConnector->aggregatorNeighbors()->init( m_game );
	m_eventConnector->aggregatorPopulation()->init( m_game );
	m_eventConnector->aggregatorRenderer()->init( m_game );
	m_eventConnector->aggregatorStockpile()->init( m_game );
	m_eventConnector->aggregatorTileInfo()->init( m_game );
	m_eventConnector->aggregatorWorkshop()->init( m_game );
	m_eventConnector->aggregatorSound()->init( m_game );
	m_eventConnector->aggregatorDebug()->init( m_game );

	connect( m_game->fm(), &FarmingManager::signalFarmChanged, m_eventConnector->aggregatorAgri(), &AggregatorAgri::onUpdateFarm, Qt::QueuedConnection );
	connect( m_game->fm(), &FarmingManager::signalPastureChanged, m_eventConnector->aggregatorAgri(), &AggregatorAgri::onUpdatePasture, Qt::QueuedConnection );
	connect( m_eventConnector->aggregatorDebug(), &AggregatorDebug::signalTriggerEvent, m_game->em(), &EventManager::onDebugEvent );
	connect( m_game->spm(), &StockpileManager::signalStockpileAdded, m_eventConnector->aggregatorStockpile(), &AggregatorStockpile::onOpenStockpileInfo, Qt::QueuedConnection );
	connect( m_game->spm(), &StockpileManager::signalStockpileContentChanged, m_eventConnector->aggregatorStockpile(), &AggregatorStockpile::onUpdateStockpileContent, Qt::QueuedConnection );
	connect( m_game->wsm(), &WorkshopManager::signalJobListChanged, m_eventConnector->aggregatorWorkshop(), &AggregatorWorkshop::onCraftListChanged, Qt::QueuedConnection );

	connect( m_game->sm(), &SoundManager::signalPlayEffect, m_eventConnector->aggregatorSound(), &AggregatorSound::onPlayEffect, Qt::QueuedConnection );

	connect( m_game->em(), &EventManager::signalUpdateMission, m_eventConnector->aggregatorNeighbors(), &AggregatorNeighbors::onUpdateMission, Qt::QueuedConnection );
	connect( m_game->em(), &EventManager::signalCenterCamera, m_eventConnector->aggregatorRenderer(), &AggregatorRenderer::onCenterCamera, Qt::QueuedConnection );

	connect( m_game->inv(), &Inventory::signalAddItem, m_eventConnector->aggregatorInventory(), &AggregatorInventory::onAddItem, Qt::QueuedConnection );
	connect( m_game->inv(), &Inventory::signalRemoveItem, m_eventConnector->aggregatorInventory(), &AggregatorInventory::onRemoveItem, Qt::QueuedConnection );

	connect( m_game, &Game::signalTimeAndDate, m_eventConnector, &EventConnector::onTimeAndDate );
	connect( m_game, &Game::signalKingdomInfo, m_eventConnector, &EventConnector::onKingdomInfo );
	connect( m_game, &Game::signalHudSettlement, m_eventConnector, &EventConnector::onHudSettlement );
	connect( m_game, &Game::signalHudClock, m_eventConnector, &EventConnector::onHudClock );
	connect( m_game, &Game::signalHeartbeat, m_eventConnector, &EventConnector::onHeartbeat );
	connect( m_game->tutorial(), &TutorialManager::signalSnapshot, m_eventConnector, &EventConnector::onTutorialSnapshot, Qt::QueuedConnection );
	m_eventConnector->onTutorialSnapshot( m_game->tutorial()->snapshot() );
	if( m_game->startMode() == GameStartMode::InteractiveTutorial )
		m_eventConnector->aggregatorRenderer()->onCenterCamera( GameState::origin );


	Global::util->initAllowedInContainer();
	m_eventConnector->onViewLevel( GameState::viewLevel );
	// The GUI consumes initialCameraTarget in its queued view initialization.  Keep
	// it available until that restore has completed so a fresh world cannot fall
	// back to the previous game's origin camera.
	m_eventConnector->emitInitView();
	m_eventConnector->emitInMenu( false );

	connect( m_eventConnector, &EventConnector::stopGame, m_eventConnector->aggregatorRenderer(), &AggregatorRenderer::onWorldParametersChanged );
	connect( m_eventConnector, &EventConnector::startGame, m_game, &Game::start );

	connect( m_eventConnector, &EventConnector::signalCameraPosition, m_eventConnector->aggregatorSound(), &AggregatorSound::onCameraPosition, Qt::QueuedConnection );


	qRegisterMetaType<QSet<unsigned int>>();
	connect( m_game, &Game::signalSimulationTick, m_eventConnector->aggregatorRenderer(), &AggregatorRenderer::onSimulationTick, Qt::QueuedConnection );
	connect( m_game, &Game::signalUpdateTileInfo,  m_eventConnector->aggregatorTileInfo(), &AggregatorTileInfo::onUpdateAnyTileInfo );
	connect( m_game, &Game::signalUpdateStockpile, m_eventConnector->aggregatorStockpile(), &AggregatorStockpile::onUpdateAfterTick );
	connect( m_game, &Game::signalUpdateTileInfo,  m_eventConnector->aggregatorRenderer(), &AggregatorRenderer::onUpdateAnyTileInfo );
	connect( m_game, &Game::signalTimeAndDate,     m_eventConnector, &EventConnector::onTimeAndDate );
	connect( m_game, &Game::signalHudSettlement,  m_eventConnector, &EventConnector::onHudSettlement );
	connect( m_game, &Game::signalHudClock,       m_eventConnector, &EventConnector::onHudClock );
	m_game->sendTime();

	connect( Global::sel, &Selection::signalActionChanged, m_eventConnector->aggregatorSelection(), &AggregatorSelection::onActionChanged, Qt::QueuedConnection );
	connect( Global::sel, &Selection::signalFirstClick, m_eventConnector->aggregatorSelection(), &AggregatorSelection::onUpdateFirstClick, Qt::QueuedConnection );
	connect( Global::sel, &Selection::signalSize, m_eventConnector->aggregatorSelection(), &AggregatorSelection::onUpdateSize, Qt::QueuedConnection );
	Global::sel->updateGui();
	lifecycleTrace( "postCreationInit complete" );

	m_eventConnector->aggregatorInventory()->update();

	m_eventConnector->emitPause( m_game->paused() );
	m_eventConnector->emitStartGame();
}

/** @brief Handles status messages from the world generator (logs them).
 *  @param message The status message to log.
 */
void GameManager::onGeneratorMessage( QString message )
{
	qDebug() << message;
	m_eventConnector->emitWorldTransitionProgress( message );
}

/** @brief Saves the current game to disk, pausing while saving and resuming afterward.
 *  @return True when the newly-created save contains the authoritative world and game files.
 */
bool GameManager::saveGame()
{
	if( m_game )
	{
		bool paused = m_game->paused();
		m_game->setPaused( true );
		IO io( m_game, this );
		const QString folder = io.save();
		m_game->setPaused( paused );

		m_eventConnector->sendResume();
		return !folder.isEmpty() && QFile::exists( folder + "world.dat" ) && QFile::exists( folder + "game.json" );
	}
	return false;
}

/** @brief Returns the current game speed.
 *  @return The current GameSpeed, or Normal if no game is active.
 */
GameSpeed GameManager::gameSpeed()
{
	if( m_game )
	{
		return m_game->gameSpeed();
	}
	return GameSpeed::Normal;
}
/** @brief Sets the game speed and notifies the GUI.
 *  @param speed The desired GameSpeed.
 */
void GameManager::setGameSpeed( GameSpeed speed )
{
	if( m_game )
	{
		if( m_game->gameSpeed() != speed )
		{
			m_game->setGameSpeed( speed );
			m_eventConnector->emitGameSpeed( m_game->gameSpeed() );
		}
	}
}

/** @brief Returns whether the game is currently paused.
 *  @return True if paused or no game is active.
 */
bool GameManager::paused()
{
	if( m_game )
	{
		return m_game->paused();
	}
	return true;
}

/** @brief Pauses or unpauses the game and notifies the GUI.
 *  @param value True to pause, false to unpause.
 */
void GameManager::setPaused( bool value )
{
	lifecycleTrace( QString( "setPaused request=%1 before=%2" ).arg( value ? "true" : "false", paused() ? "true" : "false" ) );
	if( m_game )
	{
		if( m_game->paused() != value )
		{
			m_game->setPaused( value );
			lifecycleTrace( QString( "setPaused applied=%1" ).arg( value ? "true" : "false" ) );
		}
		else
		{
			lifecycleTrace( "setPaused no_change" );
		}
		// Always refresh the GUI projection.  This also repairs a stale HUD
		// label if an earlier lifecycle signal was missed while the state itself
		// was already at the requested value.
		m_eventConnector->emitPause( m_game->paused() );
	}
	else
	{
		lifecycleTrace( "setPaused ignored_no_game" );
	}
}
/** @brief Forwards a heartbeat response value to the game (used for tick synchronization).
 *  @param value The heartbeat response value.
 */
void GameManager::setHeartbeatResponse( int value )
{
	if( m_game )
	{
		m_game->setHeartbeatResponse( value );
	}
}

/** @brief Returns a pointer to the active Game instance.
 *  @return Pointer to the Game, or nullptr if no game is active.
 */
Game* GameManager::game()
{
	return m_game;
}
