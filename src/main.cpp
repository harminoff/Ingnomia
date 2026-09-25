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
#include "base/config.h"
#include "base/db.h"
#include "base/io.h"
#include "base/crashhandler.h"
#include "base/global.h"

#include "game/gamemanager.h"
#include "game/game.h"
#include "game/eventmanager.h"
#include "game/stockpile.h"
#include "game/stockpilemanager.h"

#include "gui/mainwindow.h"
#include "gui/mainwindowrenderer.h"
#include "gui/eventconnector.h"
#include "gui/aggregatorcreatureinfo.h"
#include "gui/aggregatorselection.h"
#include "gui/aggregatorrenderer.h"
#include "gui/aggregatortileinfo.h"
#include "gui/aggregatordebug.h"
#include "gui/aggregatorsettings.h"
#include "gui/strings.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileIconProvider>
#include <QStandardPaths>
#include <QColorSpace>
#include <QSurfaceFormat>
#include <QWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMutex>
#include <QtWidgets/QApplication>

#include <RmlUi/Core/Input.h>

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "version.h"
#include "../tests/water/runtime_probe.h"
#include "../tests/lighting/runtime_probe.h"
#include "../tests/placement/runtime_probe.h"

QTextStream* out = 0;
bool verbose     = false;

void automationTrace( const QString& message )
{
	static QMutex traceMutex;
	QMutexLocker locker( &traceMutex );
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_TRACE_PATH" );
	if ( path.isEmpty() ) return;
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) return;
	QTextStream stream( &file );
	stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs ) << " " << message << "\n";
}

#include "../tests/ui-stage10/runtime_probe.h"
#include "../tests/ui-stage11/runtime_probe.h"
#include "../tests/ui-stage12/runtime_probe.h"
#include "../tests/ui-stage13/runtime_probe.h"
#include "../tests/ui-stage14/runtime_probe.h"
#include "../tests/ui-stage15/runtime_probe.h"
#include "../tests/ui-stage16/runtime_probe.h"
#include "../tests/ui-stage17/runtime_probe.h"
#include "../tests/ui-stage18/runtime_probe.h"
#include "../tests/ui-stage19/runtime_probe.h"
#include "../tests/ui-stage20/runtime_probe.h"
#include "../tests/ui-stage21/runtime_probe.h"
#include "../tests/ui-management6a/runtime_probe.h"
#include "../tests/ui-inspector/live_tile_probe.h"

void scheduleUiFixture( QApplication& app, const QString& fixture, const QString& capturePath, bool hold )
{
	if ( fixture.isEmpty() ) return;
	auto* timer = new QTimer( &app );
	timer->setInterval( 50 );
	QObject::connect( timer, &QTimer::timeout, &app,
		[timer, appPtr = &app, fixture, capturePath, hold, attempts = 0]() mutable {
			++attempts;
				auto& window = MainWindow::getInstance();
			bool shown = false;
				if ( fixture.compare( "components", Qt::CaseInsensitive ) == 0 ) shown = window.showComponentFixture();
				else if ( fixture.compare( "shell_confirmation", Qt::CaseInsensitive ) == 0 ) shown = window.shellRouteForProbe() == "shell.main_menu";
				else if ( fixture.compare( "inventory", Qt::CaseInsensitive ) == 0 ) shown = window.showInventoryFixture();
			else if ( fixture.compare( "blueprint", Qt::CaseInsensitive ) == 0 ) shown = window.showInspectorBlueprintFixture();
			else if ( fixture.compare( "stockpile", Qt::CaseInsensitive ) == 0 ) shown = window.showInspectorStockpileFixture();
			else if ( fixture.compare( "workshop_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementWorkshopFixture();
			else if ( fixture.compare( "stockpile_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementStockpileFixture();
			else if ( fixture.compare( "farm_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementFarmFixture();
			else if ( fixture.compare( "creature_profile", Qt::CaseInsensitive ) == 0
				|| fixture.compare( "creature_equipment", Qt::CaseInsensitive ) == 0 ) shown = window.showInspectorCreatureFixture();
			else
			{
				timer->stop();
				timer->deleteLater();
				automationTrace( QStringLiteral( "ui_fixture unsupported=%1" ).arg( fixture ) );
				return;
			}
			if ( !shown && attempts < 200 ) return;
			timer->stop();
			timer->deleteLater();
			automationTrace( QStringLiteral( "ui_fixture_ready name=%1 shown=%2 attempts=%3 wait_ms=%4" )
				.arg( fixture ).arg( shown ? "true" : "false" ).arg( attempts ).arg( attempts * 50 ) );
			if ( !shown )
			{
				if ( !hold && Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
					return;
				}
			if ( fixture.compare( "shell_confirmation", Qt::CaseInsensitive ) == 0 )
			{
				QObject::connect( appPtr, &QCoreApplication::aboutToQuit, appPtr, [] {
					automationTrace( "shell_confirmation about_to_quit=true" );
				} );
				std::string focused;
				const bool opened = window.dispatchShellClickForProbe( "shell-exit", &focused );
				automationTrace( QStringLiteral( "shell_confirmation first_open=%1 focus=%2 route=%3" )
					.arg( opened ? "true" : "false", QString::fromStdString( focused ), QString::fromStdString( window.shellRouteForProbe() ) ) );
				QTimer::singleShot( 500, appPtr, [] {
					auto& shell = MainWindow::getInstance();
					std::string cancelFocus;
					const bool cancelled = shell.dispatchShellClickForProbe( "confirm-cancel", &cancelFocus );
					const auto route = shell.shellRouteForProbe();
					automationTrace( QStringLiteral( "shell_confirmation cancel=%1 focus=%2 route=%3" )
						.arg( cancelled ? "true" : "false", QString::fromStdString( cancelFocus ), QString::fromStdString( route ) ) );
					if ( !cancelled || route != "shell.main_menu" ) return;
					QTimer::singleShot( 300, qApp, [] {
						auto& shell = MainWindow::getInstance();
						std::string focus;
						const bool reopened = shell.dispatchShellClickForProbe( "shell-exit", &focus );
						automationTrace( QStringLiteral( "shell_confirmation second_open=%1 focus=%2 route=%3" )
							.arg( reopened ? "true" : "false", QString::fromStdString( focus ), QString::fromStdString( shell.shellRouteForProbe() ) ) );
						if ( !reopened ) return;
						QTimer::singleShot( 300, qApp, [] {
							std::string acceptFocus;
						const bool accepted = MainWindow::getInstance().dispatchShellClickForProbe( "confirm-accept", &acceptFocus );
						automationTrace( QStringLiteral( "shell_confirmation accept=%1 focus=%2 action=app.exit" )
							.arg( accepted ? "true" : "false", QString::fromStdString( acceptFocus ) ) );
							if ( !accepted && Global::eventConnector )
								QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
						} );
					} );
				} );
				QTimer::singleShot( 3000, appPtr, [] {
					automationTrace( "shell_confirmation fail_safe_exit=true" );
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
			}
			if ( fixture.compare( "components", Qt::CaseInsensitive ) == 0 )
			{
				std::string detail;
				const bool generatedParts = window.verifyComponentFixture( &detail );
				automationTrace( QStringLiteral( "ui_fixture_components_generated passed=%1 %2" )
					.arg( generatedParts ? "true" : "false", QString::fromStdString( detail ) ) );
				if ( !generatedParts ) qCritical() << "Generated component fixture inspection failed:" << QString::fromStdString( detail );
			}

				if ( fixture.compare( "inventory", Qt::CaseInsensitive ) == 0
				&& qEnvironmentVariable( "INGNOMIA_AUTOMATE_FILTER_COMBO_FIXTURE" ) == "1" )
			{
				QElapsedTimer comboTimer;
				comboTimer.start();
				const bool combo = window.activateManagementElement( "inventory_filter_item_toggle" );
				const bool first = combo && window.activateManagementElement( "inventory_filter_item_options_option_1" );
				const bool second = first && window.activateManagementElement( "inventory_filter_item_options_option_2" );
				automationTrace( QStringLiteral( "ui_fixture_inventory_combo open=%1 first=%2 second=%3 elapsed_ms=%4" )
					.arg( combo ? "true" : "false", first ? "true" : "false", second ? "true" : "false" ).arg( comboTimer.elapsed() ) );
			}
			else if ( fixture.compare( "creature_profile", Qt::CaseInsensitive ) == 0 )
			{
				const bool expertise = window.activateInspectorElement( "creature_preview_nav_expertise" );
				const bool dropdown = window.activateInspectorElement( "creature_preview_profession_toggle" );
				automationTrace( QStringLiteral( "ui_fixture_creature_profile expertise=%1 dropdown=%2" )
					.arg( expertise ? "true" : "false" ).arg( dropdown ? "true" : "false" ) );
			}
			else if ( fixture.compare( "creature_equipment", Qt::CaseInsensitive ) == 0 )
			{
				const bool equipment = window.activateInspectorElement( "creature_preview_nav_equipment" );
				const bool slot = window.activateInspectorElement( "creature_equipment_slot_head" );
				automationTrace( QStringLiteral( "ui_fixture_creature_equipment equipment=%1 slot=%2" )
					.arg( equipment ? "true" : "false" ).arg( slot ? "true" : "false" ) );
			}
			else if ( fixture.compare( "stockpile", Qt::CaseInsensitive ) == 0 )
			{
				std::string detail;
				const bool inlineGeometry = window.verifyInspectorStockpileItemGeometry( &detail );
				automationTrace( QStringLiteral( "ui_fixture_stockpile_geometry inline=%1 %2" )
					.arg( inlineGeometry ? "true" : "false", QString::fromStdString( detail ) ) );
				if ( !inlineGeometry ) qCritical() << "Stockpile fixture item icon geometry failed:" << QString::fromStdString( detail );
			}
			else if ( fixture.compare( "workshop_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementWorkshopFixture();
			else if ( fixture.compare( "stockpile_manager", Qt::CaseInsensitive ) == 0
				&& qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_SEARCH_FIXTURE" ) == "1" )
			{
				const bool allow = window.activateManagementElement( "stockpile_view_allow" );
				bool sequence = allow;
				QElapsedTimer searchTimer;
				searchTimer.start();
				for ( const auto* value : { "axe", "ax", "a", "", "wood", "" } )
					sequence = window.setManagementStockpileSearchForProbe( value ) && sequence;
				automationTrace( QStringLiteral( "ui_fixture_stockpile_search allow=%1 sequence=%2 elapsed_ms=%3" )
					.arg( allow ? "true" : "false", sequence ? "true" : "false" ).arg( searchTimer.elapsed() ) );
			}
			else if ( fixture.compare( "workshop_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementWorkshopFixture();
			else if ( fixture.compare( "workshop_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementWorkshopFixture();
			else if ( fixture.compare( "stockpile_manager", Qt::CaseInsensitive ) == 0 )
			{
				const auto templateFixture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_TEMPLATE_FIXTURE" ).trimmed().toLower();
				if ( !templateFixture.isEmpty() )
				{
					const bool allow = window.activateManagementElement( "stockpile_view_allow" );
					bool interaction = false;
					if ( templateFixture == "menu" )
						interaction = window.activateManagementElement( "stockpile_template_toggle" );
					else if ( templateFixture == "overwrite" )
						interaction = window.setManagementFormValueForProbe( "stockpile_template_name", "Food only" )
							&& window.activateManagementElement( "stockpile_template_save" );
					automationTrace( QStringLiteral( "ui_fixture_stockpile_template mode=%1 allow=%2 interaction=%3" )
						.arg( templateFixture, allow ? "true" : "false", interaction ? "true" : "false" ) );
				}
			}

			const auto fixtureElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_UI_FIXTURE_ELEMENT" );
			if ( !fixtureElement.isEmpty() )
			{
				const bool activated = window.activateManagementElement( fixtureElement.toStdString() );
				automationTrace( QStringLiteral( "ui_fixture_element id=%1 activated=%2" ).arg( fixtureElement ).arg( activated ? "true" : "false" ) );
			}
			if ( !capturePath.isEmpty() )
			{
				qputenv( "INGNOMIA_UI_CAPTURE", capturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				window.armUiCapture();
				automationTrace( QStringLiteral( "ui_fixture_capture armed=true path=%1" ).arg( capturePath ) );
			}
			const auto detachedCapturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_UI_FIXTURE_DETACHED_CAPTURE_PATH" );
			if ( fixture.compare( "inventory", Qt::CaseInsensitive ) == 0 && !detachedCapturePath.isEmpty() )
			{
				const bool armed = window.requestManagementCaptureForProbe( "inventory", detachedCapturePath );
				automationTrace( QStringLiteral( "ui_fixture_detached_capture kind=inventory armed=%1 path=%2" )
					.arg( armed ? "true" : "false", detachedCapturePath ) );
			}
			else if ( fixture.compare( "workshop_manager", Qt::CaseInsensitive ) == 0 ) shown = window.showManagementWorkshopFixture();
			else if ( fixture.compare( "stockpile_manager", Qt::CaseInsensitive ) == 0 && !detachedCapturePath.isEmpty() )
			{
				const bool armed = window.requestManagementCaptureForProbe( "stockpile", detachedCapturePath );
				automationTrace( QStringLiteral( "ui_fixture_detached_capture kind=stockpile armed=%1 path=%2" )
					.arg( armed ? "true" : "false", detachedCapturePath ) );
			}
			if ( !hold )
				QTimer::singleShot( capturePath.isEmpty() ? 250 : 1000, qApp, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
		} );
	timer->start();
}

void clearLog()
{
	QString folder   = IO::getDataFolder();
	bool ok          = true;
	QString fileName = "log.txt";
	if ( QDir( folder ).exists() )
	{
		fileName = folder + "/" + fileName;
	}

	QFile file( fileName );
	file.open( QIODevice::WriteOnly );
	file.close();
}

QPointer<QFile> openLog()
{
	QString folder   = IO::getDataFolder();
	bool ok          = true;
	QString fileName = "log.txt";
	if ( QDir( folder ).exists() )
	{
		fileName = folder + "/" + fileName;
	}

	QPointer<QFile> outFile(new QFile( fileName ));
	if(outFile->open( QIODevice::WriteOnly | QIODevice::Append ))
	{
		return outFile;
	}
	else
	{
		return nullptr;
	}
}

void logOutput( QtMsgType type, const QMessageLogContext& context, const QString& message )
{
	if ( message.startsWith( "libpng warning:" ) )
		return;

	QString filedate  = QDateTime::currentDateTime().toString( "yyyy.MM.dd hh:mm:ss:zzz" );
#ifdef _WIN32
	if ( IsDebuggerPresent() )
#else
	if (verbose)
#endif // _WIN32
	{
		QString debugdate = QDateTime::currentDateTime().toString( "hh:mm:ss:zzz" );

		switch ( type )
		{
			case QtDebugMsg:
				debugdate += " [D]";
				break;
			case QtInfoMsg:
				debugdate += " [I]";
				break;
			case QtWarningMsg:
				debugdate += " [W]";
				break;
			case QtCriticalMsg:
				debugdate += " [C]";
				break;
			case QtFatalMsg:
				debugdate += " [F]";
				break;
		}
		QString text    = debugdate + " " + message + "\n";
		std::string str = text.toStdString();

#ifdef _WIN32
		OutputDebugStringA( str.c_str() );
#else
		std::cerr << str;
#endif
	}
	static QPointer<QFile> outFile = openLog();
	static std::mutex guard;

	if ( outFile )
	{
		std::lock_guard<std::mutex> lock( guard );
		QTextStream ts( outFile );
		ts << filedate << " " << message << Qt::endl;
	}
}

int main( int argc, char* argv[] )
{
	setupCrashHandler();
	clearLog();
	qInstallMessageHandler( &logOutput );
	qInfo() << PROJECT_NAME << "version" << PROJECT_VERSION;
#ifdef GIT_REPO
	qInfo() << "Built from" << GIT_REPO << GIT_REF << "(" << GIT_SHA << ")"
			<< "build" << BUILD_ID;
#endif // GIT_REPO

	// Disable use of ANGLE, as it supports OpenGL 3.x at most
	QCoreApplication::setAttribute( Qt::AA_ShareOpenGLContexts );
	// Enable fractional DPI support (e.g. 150%)
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy( Qt::HighDpiScaleFactorRoundingPolicy::PassThrough );

	// Set the default surface format before QApplication constructs the global shared
	// GL context — otherwise Qt warns that a later setDefaultFormat may cause sharing issues.
	{
		QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
		defaultFormat.setRenderableType( QSurfaceFormat::OpenGL );
		defaultFormat.setSwapBehavior( QSurfaceFormat::TripleBuffer );
		defaultFormat.setColorSpace( QColorSpace::SRgb );
		defaultFormat.setDepthBufferSize( 16 );
		// 0 = unthrottled, 1 = vysnc full FPS, 2 = vsync half FPS
		defaultFormat.setSwapInterval( 0 );
		defaultFormat.setVersion( 4, 3 );
		defaultFormat.setProfile( QSurfaceFormat::CoreProfile );
		defaultFormat.setOption( QSurfaceFormat::DebugContext );
		QSurfaceFormat::setDefaultFormat( defaultFormat );
	}

	QApplication a( argc, argv );
	QCoreApplication::addLibraryPath( QCoreApplication::applicationDirPath() );
	QCoreApplication::setOrganizationDomain( PROJECT_HOMEPAGE_URL );
	QCoreApplication::setOrganizationName( "Roest" );
	QCoreApplication::setApplicationName( PROJECT_NAME );
	QCoreApplication::setApplicationVersion( PROJECT_VERSION );

	Global::cfg = new Config;

	if ( !Global::cfg->valid() )
	{
		qDebug() << "Failed to init Config.";
		abort();
	}

	DB::init();
	DB::initStructs();

	if ( !S::gi().init() )
	{
		qDebug() << "Failed to init translation.";
		abort();
	}

	Global::cfg->set( "CurrentVersion", PROJECT_VERSION );

	QStringList args = a.arguments();

	for ( int i = 1; i < args.size(); ++i )
	{
		if ( args.at( i ) == "-h" || args.at( i ) == "?" )
		{
			qDebug() << "Command line options:";
			qDebug() << "-h : displays this message";
			qDebug() << "-v : toggles verbose mode, warning: this will spam your console with messages";
			qDebug() << "---";
		}
		if ( args.at( i ) == "-v" )
		{
			verbose = true;
		}
		if ( args.at( i ) == "-ds" )
		{
			Global::debugSound = true;
		}
	}

	int width  = qMax( 1200, Global::cfg->get( "WindowWidth" ).toInt() );
	int height = qMax( 675, Global::cfg->get( "WindowHeight" ).toInt() );

	GameManager* gm = new GameManager;
	QThread gameThread;
	gameThread.start();
	gm->moveToThread( &gameThread );


	//MainWindow w;
	MainWindow w;
	
	w.setIcon( QIcon( QCoreApplication::applicationDirPath() + "/content/icon.png" ) );
	w.resize( width, height );
	w.setPosition( Global::cfg->get( "WindowPosX" ).toInt(), Global::cfg->get( "WindowPosY" ).toInt() );
	w.show();
	scheduleWaterRuntimeProbe( a, gm );
	scheduleLightingProbe( a, gm );
	schedulePlacementProbe( a, gm );
	scheduleLiveTileProbe( a, gm );
	// Test-only path: exercise the normal saved-game command from inside the Qt
	// event loop when desktop input injection is unavailable. Normal launches are
	// unchanged unless this explicit environment variable is present. The path
	// variant removes ambiguity about which save QDir's time ordering selected.
	const QString automatedLoadPath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_LOAD_PATH" );
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_LOAD" ) == "1" || !automatedLoadPath.isEmpty() )
	{
		QTimer::singleShot( 2500, &a, [connector = Global::eventConnector, automatedLoadPath]() {
			if ( !connector )
			{
				qWarning() << "Automated saved-game load skipped: EventConnector unavailable";
				return;
			}
			if ( automatedLoadPath.isEmpty() )
			{
				automationTrace( QStringLiteral( "continue_last_game dispatched=true" ) );
				qInfo() << "Automated saved-game load dispatching continueLastGame";
				QMetaObject::invokeMethod( connector, "onContinueLastGame", Qt::QueuedConnection );
			}
			else
			{
				automationTrace( QStringLiteral( "explicit_load dispatched=true path=%1" ).arg( automatedLoadPath ) );
				qInfo() << "Automated saved-game load dispatching explicit path" << automatedLoadPath;
				// Keep the diagnostic on the same queued game-thread path as the
				// production Load button. Constructing Game as a child of the
				// game-thread GameManager from the GUI thread leaves timers and QObject
				// teardown with mismatched affinity and makes the later save/reload
				// probe meaningless.
				QMetaObject::invokeMethod( connector, "onLoadGame", Qt::QueuedConnection,
					Q_ARG( QString, automatedLoadPath ) );
			}
			// Optional visual-test seam: many older saves open on an empty surface
			// z-level. Select a requested level only for the explicit diagnostic run,
			// leaving normal launches and saved camera state untouched.
			bool viewLevelOk = false;
			const int requestedViewLevel = qEnvironmentVariable( "INGNOMIA_AUTOMATE_VIEW_LEVEL" ).toInt( &viewLevelOk );
			if( viewLevelOk )
			{
				qInfo() << "Automated saved-game selecting view level" << requestedViewLevel;
				MainWindow::getInstance().onUiSetViewLevel( requestedViewLevel );
			}
			if( qEnvironmentVariable( "INGNOMIA_AUTOMATE_CENTER_VIEW" ) == "1" )
			{
				qInfo() << "Automated saved-game centering view";
				auto& window = MainWindow::getInstance();
				if( window.renderer() )
				{
					window.renderer()->setMove( 0.0f, 0.0f );
					window.renderer()->setScale( 1.0f );
				}
			}
			const auto centerPosition = qEnvironmentVariable( "INGNOMIA_AUTOMATE_CENTER_POSITION" ).split( ' ', Qt::SkipEmptyParts );
			if( centerPosition.size() == 3 )
			{
				bool xOk = false, yOk = false, zOk = false;
				const Position target( centerPosition[0].toInt( &xOk ), centerPosition[1].toInt( &yOk ), centerPosition[2].toInt( &zOk ) );
				if( xOk && yOk && zOk && Global::eventConnector )
				{
					QMetaObject::invokeMethod( Global::eventConnector, [target]() {
						if( Global::eventConnector ) Global::eventConnector->aggregatorRenderer()->onCenterCamera( target );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "center_position dispatched=true target=%1 %2 %3" ).arg( target.x ).arg( target.y ).arg( target.z ) );
				}
				else automationTrace( QStringLiteral( "center_position dispatched=false invalid" ) );
			}
		} );
	}
	// Opt-in route watcher for the transient production Loading surface. Polling
	// starts shortly before the explicit-load seam so the route is captured even
	// when a small tutorial world leaves Loading between ordinary capture timers.
	const QString automatedLoadingCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_LOADING_CAPTURE_PATH" );
	if ( !automatedLoadingCapture.isEmpty() )
	{
		auto* loadingWatcher = new QTimer( &a );
		loadingWatcher->setInterval( 8 );
		QObject::connect( loadingWatcher, &QTimer::timeout, &a,
			[loadingWatcher, automatedLoadingCapture, attempts = 0]() mutable {
				++attempts;
				const auto route = MainWindow::getInstance().shellRouteForProbe();
				if ( route == "shell.loading" )
				{
					qputenv( "INGNOMIA_UI_CAPTURE", automatedLoadingCapture.toUtf8() );
					qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
					MainWindow::getInstance().armUiCapture();
					automationTrace( QStringLiteral( "loading_route_capture armed=true route=%1 path=%2" )
						.arg( QString::fromStdString( route ), automatedLoadingCapture ) );
					loadingWatcher->stop();
					loadingWatcher->deleteLater();
				}
				else if ( route == "game.hud" || attempts >= 1500 )
				{
					automationTrace( QStringLiteral( "loading_route_capture armed=false route=%1 attempts=%2" )
						.arg( QString::fromStdString( route ) ).arg( attempts ) );
					loadingWatcher->stop();
					loadingWatcher->deleteLater();
				}
			} );
		QTimer::singleShot( 2300, &a, [loadingWatcher] { loadingWatcher->start(); } );
		if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_LOADING_CAPTURE_EXIT" ) == "1" )
			QTimer::singleShot( 15000, &a, [] {
				if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
	}
	const QString automatedInspectorTile = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_TILE_ID" );
	const QString automatedInspectorCreature = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_CREATURE_ID" );
	const QString automatedInspectorSecondCreature = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_SECOND_CREATURE_ID" );
	const QString automatedInspectorElementAll = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT_ALL" );
	const QString automatedSelectCreature = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SELECT_CREATURE_ID" );
	const QString automatedSelectCreatureCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SELECT_CREATURE_CAPTURE_PATH" );
	const QString automatedInspectorProfession = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_PROFESSION" );
	const QString automatedInspectorElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT" );
	const QString automatedInspectorSecondElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_SECOND_ELEMENT" );
	const QString automatedInspectorSecondTile = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_SECOND_TILE_ID" );
	const QString automatedInspectorGlCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_GL_CAPTURE_PATH" );
	const QString automatedUiFixture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_UI_FIXTURE" );
	const QString automatedUiFixtureCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_UI_FIXTURE_CAPTURE_PATH" );
	const bool holdAutomatedUiFixture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_UI_FIXTURE_HOLD" ) == "1";
	const bool automatedInspectorReopen = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_REOPEN" ) == "1";
	if ( qEnvironmentVariableIsSet( "INGNOMIA_UI_CAPTURE" ) && qEnvironmentVariableIsSet( "INGNOMIA_AUTOMATE_TRACE_PATH" ) )
		automationTrace( QStringLiteral( "ui_capture_requested path=%1" ).arg( qEnvironmentVariable( "INGNOMIA_UI_CAPTURE" ) ) );
	bool automatedInspectorRaiseCountOk = false;
	const int automatedInspectorRaiseCount = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_RAISE_COUNT" ).toInt( &automatedInspectorRaiseCountOk );
	if ( !automatedInspectorTile.isEmpty() && ( !automatedInspectorProfession.isEmpty() || !automatedInspectorElement.isEmpty() ) )
	{
		bool tileOk = false;
		const auto tileId = automatedInspectorTile.toUInt( &tileOk );
		if ( tileOk )
		{
			QTimer::singleShot( 9000, &a, [tileId]() {
				if ( !Global::eventConnector ) return;
				QMetaObject::invokeMethod( Global::eventConnector, [tileId]() {
					if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileId );
				}, Qt::QueuedConnection );
				automationTrace( QStringLiteral( "inspector_tile dispatched=true id=%1" ).arg( tileId ) );
			} );
			QTimer::singleShot( 11000, &a, [automatedInspectorElement]() {
				if ( !automatedInspectorElement.isEmpty() )
				{
					const bool activated = MainWindow::getInstance().activateInspectorElement( automatedInspectorElement.toStdString() );
					automationTrace( QStringLiteral( "inspector_element requested=%1 activated=%2" ).arg( automatedInspectorElement ).arg( activated ? "true" : "false" ) );
					return;
				}
				const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_open_creature" );
				automationTrace( QStringLiteral( "inspector_creature activated=%1" ).arg( activated ? "true" : "false" ) );
			} );
			if ( automatedInspectorRaiseCountOk && automatedInspectorRaiseCount > 0 )
			{
				for ( int index = 0; index < automatedInspectorRaiseCount; ++index )
				{
					QTimer::singleShot( 13000 + index * 500, &a, [index]() {
						const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_raise_job" );
						automationTrace( QStringLiteral( "inspector_raise index=%1 activated=%2" ).arg( index + 1 ).arg( activated ? "true" : "false" ) );
					} );
				}
			}
			if ( !automatedInspectorGlCapture.isEmpty() )
			{
				const int captureDelay = !automatedInspectorSecondTile.isEmpty() ? 16500 : ( automatedInspectorRaiseCountOk && automatedInspectorRaiseCount > 0 ? 13000 + automatedInspectorRaiseCount * 500 + 1500 : 11500 );
				QTimer::singleShot( captureDelay, &a, [automatedInspectorGlCapture]() {
					qputenv( "INGNOMIA_UI_CAPTURE", automatedInspectorGlCapture.toUtf8() );
					qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
					automationTrace( QStringLiteral( "inspector_gl_capture armed=true path=%1" ).arg( automatedInspectorGlCapture ) );
				} );
			}
			if ( !automatedInspectorProfession.isEmpty() )
			{
				QTimer::singleShot( 14000, &a, [automatedInspectorProfession]() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "inspector_profession_choice_0" );
					automationTrace( QStringLiteral( "inspector_profession requested=%1 activated=%2" ).arg( automatedInspectorProfession ).arg( activated ? "true" : "false" ) );
				} );
			}
			if ( !automatedInspectorSecondElement.isEmpty() )
			{
				QTimer::singleShot( 13000, &a, [automatedInspectorSecondElement]() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( automatedInspectorSecondElement.toStdString() );
					automationTrace( QStringLiteral( "inspector_element_secondary requested=%1 activated=%2" ).arg( automatedInspectorSecondElement ).arg( activated ? "true" : "false" ) );
				} );
			}
			if ( !automatedInspectorSecondTile.isEmpty() )
			{
				bool secondTileOk = false;
				const auto secondTileId = automatedInspectorSecondTile.toUInt( &secondTileOk );
				if ( secondTileOk )
				{
					QTimer::singleShot( 13000, &a, [secondTileId]() {
						if ( !Global::eventConnector ) return;
						QMetaObject::invokeMethod( Global::eventConnector, [secondTileId]() {
							if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( secondTileId );
						}, Qt::QueuedConnection );
						automationTrace( QStringLiteral( "inspector_tile_secondary dispatched=true id=%1" ).arg( secondTileId ) );
					} );
					QTimer::singleShot( 15000, &a, []() {
						const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_open_creature" );
						automationTrace( QStringLiteral( "inspector_creature_secondary activated=%1" ).arg( activated ? "true" : "false" ) );
					} );
				}
			}
		}
	else automationTrace( QStringLiteral( "inspector_tile dispatched=false invalid" ) );
	}
	if ( !automatedInspectorCreature.isEmpty() )
	{
		bool creatureOk = false;
		const auto creatureId = automatedInspectorCreature.toUInt( &creatureOk );
		if ( creatureOk )
		{
			QTimer::singleShot( 9000, &a, [creatureId]() {
				if ( !Global::eventConnector ) return;
				Global::eventConnector->aggregatorCreatureInfo()->onRequestCreatureUpdate( creatureId );
				Global::eventConnector->aggregatorCreatureInfo()->onRequestProfessionList();
				automationTrace( QStringLiteral( "inspector_creature dispatched=true id=%1" ).arg( creatureId ) );
			} );
		}
		else automationTrace( QStringLiteral( "inspector_creature dispatched=false invalid" ) );
	}
	if ( !automatedInspectorSecondCreature.isEmpty() )
	{
		bool creatureOk = false;
		const auto creatureId = automatedInspectorSecondCreature.toUInt( &creatureOk );
		if ( creatureOk )
		{
			QTimer::singleShot( 13000, &a, [creatureId]() {
				if ( !Global::eventConnector ) return;
				Global::eventConnector->aggregatorCreatureInfo()->onRequestCreatureUpdate( creatureId );
				automationTrace( QStringLiteral( "inspector_creature_secondary dispatched=true id=%1" ).arg( creatureId ) );
			} );
		}
		else automationTrace( QStringLiteral( "inspector_creature_secondary dispatched=false invalid" ) );
	}
	if ( !automatedInspectorElementAll.isEmpty() )
		QTimer::singleShot( 15000, &a, [automatedInspectorElementAll]() {
			const auto activated = MainWindow::getInstance().activateInspectorElementInAllWindows( automatedInspectorElementAll.toStdString() );
			automationTrace( QStringLiteral( "inspector_element_all requested=%1 activated=%2" ).arg( automatedInspectorElementAll ).arg( activated ) );
		} );
	// Exercise the same signal path as a real tile click without desktop input
	// injection. This deliberately emits AggregatorSelection::signalSelectCreature;
	// the production queued connections then perform the normal creature lookup,
	// profession refresh, and detached-window open sequence.
	if ( !automatedSelectCreature.isEmpty() )
	{
		bool creatureOk = false;
		const auto creatureId = automatedSelectCreature.toUInt( &creatureOk );
		if ( creatureOk )
		{
			QTimer::singleShot( 9000, &a, [creatureId]() {
				if ( !Global::eventConnector || !Global::eventConnector->aggregatorSelection() ) return;
				auto* selection = Global::eventConnector->aggregatorSelection();
				QMetaObject::invokeMethod( selection, [selection, creatureId]()
					{ selection->signalSelectCreature( creatureId ); }, Qt::QueuedConnection );
				automationTrace( QStringLiteral( "selection_creature dispatched=true id=%1" ).arg( creatureId ) );
			} );
		}
		else automationTrace( QStringLiteral( "selection_creature dispatched=false invalid" ) );
	}
	if ( !automatedSelectCreature.isEmpty() )
		QTimer::singleShot( 16000, &a, []() {
			if ( Global::eventConnector )
				QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	if ( !automatedSelectCreatureCapture.isEmpty() )
		QTimer::singleShot( 12000, &a, [automatedSelectCreatureCapture]() {
			qputenv( "INGNOMIA_UI_CAPTURE", automatedSelectCreatureCapture.toUtf8() );
			qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
			MainWindow::getInstance().armUiCapture();
			automationTrace( QStringLiteral( "selection_creature_capture armed=true path=%1" ).arg( automatedSelectCreatureCapture ) );
		} );
	scheduleUiFixture( a, automatedUiFixture, automatedUiFixtureCapture, holdAutomatedUiFixture );
	if ( automatedInspectorReopen )
	{
		QTimer::singleShot( 9000, &a, []()
		{
			const bool shown = MainWindow::getInstance().showInspectorCreatureFixture();
			automationTrace( QStringLiteral( "inspector_reopen_initial shown=%1" ).arg( shown ? "true" : "false" ) );
		} );
		QTimer::singleShot( 10500, &a, []()
		{
			const bool activated = MainWindow::getInstance().activateInspectorElement( "creature_preview_nav_expertise" );
			automationTrace( QStringLiteral( "inspector_reopen_expand activated=%1" ).arg( activated ? "true" : "false" ) );
		} );
		QTimer::singleShot( 12000, &a, []()
		{
			const bool activated = MainWindow::getInstance().activateInspectorElement( "creature_preview_close" );
			automationTrace( QStringLiteral( "inspector_reopen_close activated=%1" ).arg( activated ? "true" : "false" ) );
		} );
		QTimer::singleShot( 13500, &a, []()
		{
			const bool shown = MainWindow::getInstance().showInspectorCreatureFixture();
			automationTrace( QStringLiteral( "inspector_reopen_second shown=%1" ).arg( shown ? "true" : "false" ) );
		} );
		QTimer::singleShot( 17500, &a, []()
		{
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	// Opt-in generic UI surface probe. This keeps the capture path reusable for
	// every shell/HUD surface without adding test-only buttons or changing the
	// normal input route. The MCP supplies the element id and capture path.
	const QString automatedShellElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_ELEMENT" );
	const QString automatedShellSecondElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_SECOND_ELEMENT" );
	const QString automatedShellCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_CAPTURE_PATH" );
	const QString automatedHudElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_ELEMENT" );
	const QString automatedHudCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_CAPTURE_PATH" );
	bool automatedShellDelayOk = false;
	const int automatedShellDelay = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_ELEMENT_DELAY_MS", "1500" ).toInt( &automatedShellDelayOk );
	const int shellElementDelay = automatedShellDelayOk ? qMax( 250, automatedShellDelay ) : 1500;
	bool automatedShellSecondDelayOk = false;
	const int automatedShellSecondDelay = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_SECOND_ELEMENT_DELAY_MS", "4500" ).toInt( &automatedShellSecondDelayOk );
	const int shellSecondElementDelay = automatedShellSecondDelayOk ? qMax( shellElementDelay + 250, automatedShellSecondDelay ) : shellElementDelay + 3000;
	if ( !automatedShellElement.isEmpty() )
	{
		QTimer::singleShot( shellElementDelay, &a, [automatedShellElement]() {
			auto& window = MainWindow::getInstance();
			const auto before = window.shellRouteForProbe();
			std::string focusedTarget;
			const bool activated = window.dispatchShellClickForProbe( automatedShellElement.toStdString(), &focusedTarget );
			const auto after = window.shellRouteForProbe();
			const auto focusAfter = window.shellFocusedElementForProbe();
			automationTrace( QStringLiteral( "ui_shell_element requested=%1 click_dispatched=%2 route_before=%3 route_after=%4 focused_target=%5 focus_after=%6" )
				.arg( automatedShellElement, activated ? "true" : "false", QString::fromStdString( before ), QString::fromStdString( after ),
					QString::fromStdString( focusedTarget ), QString::fromStdString( focusAfter ) ) );
		} );
        if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_TABS" ) == "1" )
            QTimer::singleShot( shellElementDelay + 500, &a, []() {
                automationTrace( QStringLiteral("ui_shell_tabs %1").arg(
                    QString::fromStdString( MainWindow::getInstance().verifyShellTabsForProbe() ) ) );
            } );
		if ( !automatedShellSecondElement.isEmpty() )
		{
			QTimer::singleShot( shellSecondElementDelay, &a, [automatedShellSecondElement]() {
				auto& window = MainWindow::getInstance();
				const auto before = window.shellRouteForProbe();
				std::string focusedTarget;
				const bool activated = window.dispatchShellClickForProbe( automatedShellSecondElement.toStdString(), &focusedTarget );
				const auto after = window.shellRouteForProbe();
				const auto focusAfter = window.shellFocusedElementForProbe();
				automationTrace( QStringLiteral( "ui_shell_second_element requested=%1 click_dispatched=%2 route_before=%3 route_after=%4 focused_target=%5 focus_after=%6" )
					.arg( automatedShellSecondElement, activated ? "true" : "false", QString::fromStdString( before ), QString::fromStdString( after ),
						QString::fromStdString( focusedTarget ), QString::fromStdString( focusAfter ) ) );
			} );
		}
		if ( !automatedShellCapture.isEmpty() )
		{
			const int captureDelay = ( automatedShellSecondElement.isEmpty() ? shellElementDelay : shellSecondElementDelay ) + 1000;
			QTimer::singleShot( captureDelay, &a, [automatedShellCapture]() {
				qputenv( "INGNOMIA_UI_CAPTURE", automatedShellCapture.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "ui_shell_capture armed=true path=%1" ).arg( automatedShellCapture ) );
			} );
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_EXIT" ) == "1" )
				QTimer::singleShot( captureDelay + 1500, &a, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
		}
	}
	bool automatedHudDelayOk = false;
	const int automatedHudDelay = qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_ELEMENT_DELAY_MS", "7000" ).toInt( &automatedHudDelayOk );
	const int hudElementDelay = automatedHudDelayOk ? qMax( 250, automatedHudDelay ) : 7000;
	if ( !automatedHudElement.isEmpty() )
	{
		QTimer::singleShot( hudElementDelay, &a, [automatedHudElement]() {
			const bool activated = MainWindow::getInstance().activateHudElement( automatedHudElement.toStdString() );
			automationTrace( QStringLiteral( "ui_hud_element requested=%1 activated=%2" ).arg( automatedHudElement, activated ? "true" : "false" ) );
		} );
		if ( !automatedHudCapture.isEmpty() )
		{
			QTimer::singleShot( hudElementDelay + 1000, &a, [automatedHudCapture]() {
				qputenv( "INGNOMIA_UI_CAPTURE", automatedHudCapture.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "ui_hud_capture armed=true path=%1" ).arg( automatedHudCapture ) );
			} );
		}
	}
	const QString automatedManagementElement = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_ELEMENT" );
	const QString automatedManagementDetachedCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_DETACHED_CAPTURE_PATH" );
	scheduleWorkshopOrderProbe( a );
	scheduleFarmOpenProbe( a );
	stage12::schedule( a );
	stage13::schedule( a );
	stage14::schedule( a );
	stage15::schedule( a );
	stage16::schedule( a );
	stage17::schedule( a );
	stage18::schedule( a );
	stage19::schedule( a );
	stage20::schedule( a );
	stage21::schedule( a );
	const QString automatedManagementGlCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_GL_CAPTURE_PATH" );
	const QString automatedManagementMixedFilter = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_MIXED_FILTER" );
	const QString automatedManagementMixedFilterSearch = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_MIXED_FILTER_SEARCH" );
	const QString automatedManagementMixedFilterCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_MIXED_FILTER_CAPTURE_PATH" );
	const QString automatedManagementStockpileKeyboard = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_STOCKPILE_KEYBOARD" );
	const QString automatedManagementStockpileKeyboardCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MANAGEMENT_STOCKPILE_KEYBOARD_CAPTURE_PATH" );
	const QString automatedStockpileEmpty = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY" );
	const QString automatedStockpileEmptyCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY_CAPTURE_PATH" );
	if ( !automatedManagementElement.isEmpty() && ( !automatedLoadPath.isEmpty() || !automatedInspectorTile.isEmpty() || automatedStockpileEmpty.compare( "1", Qt::CaseInsensitive ) == 0 ) )
	{
		QTimer::singleShot( 13500, &a, [automatedManagementElement]() {
			const bool activated = MainWindow::getInstance().activateManagementElement( automatedManagementElement.toStdString() );
			automationTrace( QStringLiteral( "management_element requested=%1 activated=%2" ).arg( automatedManagementElement ).arg( activated ? "true" : "false" ) );
		} );
		if ( !automatedManagementDetachedCapture.isEmpty() )
		{
			QTimer::singleShot( 15000, &a, [automatedManagementElement, automatedManagementDetachedCapture]() {
				const auto kind = automatedManagementElement.startsWith( "population_" ) || automatedManagementElement.startsWith( "skill_" ) || automatedManagementElement.startsWith( "profession_" ) || automatedManagementElement.startsWith( "schedule_" ) ? "population" : automatedManagementElement.startsWith( "stockpile_" ) ? "stockpile" : automatedManagementElement.startsWith( "diplomacy_" ) ? "diplomacy" : "military";
				const bool armed = MainWindow::getInstance().requestManagementCaptureForProbe( kind, automatedManagementDetachedCapture );
				automationTrace( QStringLiteral( "management_detached_capture kind=%1 armed=%2 path=%3" ).arg( kind, armed ? "true" : "false" ).arg( automatedManagementDetachedCapture ) );
			} );
		}
		if ( !automatedManagementGlCapture.isEmpty() )
		{
			QTimer::singleShot( 15000, &a, [automatedManagementGlCapture]() {
				qputenv( "INGNOMIA_UI_CAPTURE", automatedManagementGlCapture.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "management_gl_capture armed=true path=%1" ).arg( automatedManagementGlCapture ) );
			} );
		}
		QTimer::singleShot( 18000, &a, []() {
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	if ( automatedManagementMixedFilter.compare( "1", Qt::CaseInsensitive ) == 0 && !automatedInspectorTile.isEmpty() )
	{
		const auto search = automatedManagementMixedFilterSearch.isEmpty() ? QStringLiteral( "AppleWood" ) : automatedManagementMixedFilterSearch;
		QTimer::singleShot( 13500, &a, [search]() {
			const bool set = MainWindow::getInstance().setManagementStockpileSearchForProbe( search.toStdString() );
			automationTrace( QStringLiteral( "management_mixed_filter_search value=%1 set=%2" ).arg( search, set ? "true" : "false" ) );
		} );
		QTimer::singleShot( 14500, &a, []() {
			const bool selected = MainWindow::getInstance().activateManagementStockpileMaterialForProbe( "RawWood", "AppleWood" );
			const bool toggled = selected && MainWindow::getInstance().activateManagementElement( "stockpile_toggle_filter" )
				&& MainWindow::getInstance().activateManagementElement( "stockpile_apply" );
			automationTrace( QStringLiteral( "management_mixed_filter_material selected=%1 toggled=%2" ).arg( selected ? "true" : "false", toggled ? "true" : "false" ) );
		} );
		QTimer::singleShot( 16500, &a, []() {
			const bool set = MainWindow::getInstance().setManagementStockpileSearchForProbe( "RawWood" );
			automationTrace( QStringLiteral( "management_mixed_filter_parent_search value=RawWood set=%1" ).arg( set ? "true" : "false" ) );
		} );
		QTimer::singleShot( 18500, &a, []() {
			const bool selected = MainWindow::getInstance().selectFirstManagementMixedStockpileFilterForProbe();
			automationTrace( QStringLiteral( "management_mixed_filter_parent selected=%1" ).arg( selected ? "true" : "false" ) );
		} );
		if ( !automatedManagementMixedFilterCapture.isEmpty() )
		{
			QTimer::singleShot( 20000, &a, [automatedManagementMixedFilterCapture]() {
				qputenv( "INGNOMIA_UI_CAPTURE", automatedManagementMixedFilterCapture.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "management_mixed_filter_capture armed=true path=%1" ).arg( automatedManagementMixedFilterCapture ) );
			} );
		}
		QTimer::singleShot( 22500, &a, []() {
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	if ( automatedManagementStockpileKeyboard.compare( "1", Qt::CaseInsensitive ) == 0 && !automatedInspectorTile.isEmpty() )
	{
		QTimer::singleShot( 13500, &a, []() {
			const bool set = MainWindow::getInstance().setManagementStockpileSearchForProbe( "AppleWood" );
			automationTrace( QStringLiteral( "management_stockpile_keyboard_search value=AppleWood set=%1" ).arg( set ? "true" : "false" ) );
		} );
		QTimer::singleShot( 14500, &a, []() {
			const bool selected = MainWindow::getInstance().activateManagementStockpileMaterialForProbe( "RawWood", "AppleWood" );
			const bool toggled = selected && MainWindow::getInstance().activateManagementElement( "stockpile_toggle_filter" )
				&& MainWindow::getInstance().activateManagementElement( "stockpile_apply" );
			automationTrace( QStringLiteral( "management_stockpile_keyboard_material selected=%1 toggled=%2" ).arg( selected ? "true" : "false", toggled ? "true" : "false" ) );
		} );
		QTimer::singleShot( 16500, &a, []() {
			const bool set = MainWindow::getInstance().setManagementStockpileSearchForProbe( "RawWood" );
			automationTrace( QStringLiteral( "management_stockpile_keyboard_parent_search value=RawWood set=%1" ).arg( set ? "true" : "false" ) );
		} );
		QTimer::singleShot( 18500, &a, []() {
			const bool selected = MainWindow::getInstance().selectFirstManagementMixedStockpileFilterForProbe();
			automationTrace( QStringLiteral( "management_stockpile_keyboard_parent selected=%1" ).arg( selected ? "true" : "false" ) );
		} );
		QTimer::singleShot( 19500, &a, []() {
			const bool moved = MainWindow::getInstance().dispatchManagementStockpileFilterKeyForProbe( static_cast<int>( Rml::Input::KI_DOWN ) );
			automationTrace( QStringLiteral( "management_stockpile_keyboard_down dispatched=%1" ).arg( moved ? "true" : "false" ) );
		} );
		QTimer::singleShot( 20000, &a, []() {
			const bool toggled = MainWindow::getInstance().dispatchManagementStockpileFilterKeyForProbe( static_cast<int>( Rml::Input::KI_SPACE ) );
			automationTrace( QStringLiteral( "management_stockpile_keyboard_space dispatched=%1" ).arg( toggled ? "true" : "false" ) );
		} );
		if ( !automatedManagementStockpileKeyboardCapture.isEmpty() )
		{
			QTimer::singleShot( 21500, &a, [automatedManagementStockpileKeyboardCapture]() {
				qputenv( "INGNOMIA_UI_CAPTURE", automatedManagementStockpileKeyboardCapture.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "management_stockpile_keyboard_capture armed=true path=%1" ).arg( automatedManagementStockpileKeyboardCapture ) );
			} );
		}
		QTimer::singleShot( 24000, &a, []() {
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	if ( automatedStockpileEmpty.compare( "1", Qt::CaseInsensitive ) == 0 && !automatedLoadPath.isEmpty() )
	{
		const auto targetParts = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY_TARGET", "50 50 1" ).split( ' ', Qt::SkipEmptyParts );
		bool targetOk = targetParts.size() == 3;
		int targetX = 0;
		int targetY = 0;
		int targetZ = 0;
		if ( targetOk )
		{
			targetOk = false;
			targetX = targetParts[0].toInt( &targetOk );
			bool yOk = false;
			bool zOk = false;
			targetY = targetParts[1].toInt( &yOk );
			targetZ = targetParts[2].toInt( &zOk );
			targetOk = targetOk && yOk && zOk;
		}
		if ( !targetOk )
		{
			automationTrace( QStringLiteral( "stockpile_empty invalid_target=%1" ).arg( qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY_TARGET" ) ) );
		}
		else
		{
			const Position target( targetX, targetY, targetZ );
			QTimer::singleShot( 7000, &a, [target]() {
				auto& window = MainWindow::getInstance();
				if ( window.renderer() ) window.renderer()->onCenterCameraPosition( target );
				automationTrace( QStringLiteral( "stockpile_empty_camera target=%1" ).arg( target.toString() ) );
			} );
			QTimer::singleShot( 8000, &a, []() {
				const bool activated = MainWindow::getInstance().activateHudElement( "hud_tool_stockpile" );
				automationTrace( QStringLiteral( "stockpile_empty_tool activated=%1" ).arg( activated ? "true" : "false" ) );
			} );
			const auto clickMap = []( const char* traceName ) {
				auto& window = MainWindow::getInstance();
				bool xOk = false;
				bool yOk = false;
				const double clickX = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY_CLICK_X" ).toDouble( &xOk );
				const double clickY = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_EMPTY_CLICK_Y" ).toDouble( &yOk );
				const QPointF local( xOk ? clickX : window.width() / 2.0, yOk ? clickY : window.height() / 2.0 );
				const QPointF global = QPointF( window.position() ) + local;
				QMouseEvent press( QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
				QMouseEvent release( QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
				QCoreApplication::sendEvent( &window, &press );
				QCoreApplication::sendEvent( &window, &release );
				automationTrace( QStringLiteral( "%1 accepted=%2" ).arg( traceName, ( press.isAccepted() && release.isAccepted() ) ? "true" : "false" ) );
			};
			QTimer::singleShot( 9500, &a, [clickMap]() { clickMap( "stockpile_empty_first_click" ); } );
			QTimer::singleShot( 10500, &a, [clickMap]() { clickMap( "stockpile_empty_second_click" ); } );
			QTimer::singleShot( 12500, &a, []() {
				if ( !Global::eventConnector ) return;
				QMetaObject::invokeMethod( Global::eventConnector, []() {
					if ( !Global::eventConnector || !Global::eventConnector->game() ) return;
					auto* stockpile = Global::eventConnector->game()->spm()->getLastAddedStockpile();
					if ( !stockpile || stockpile->getFields().isEmpty() )
					{
						automationTrace( QStringLiteral( "stockpile_empty_open dispatched=false no_valid_field" ) );
						return;
					}
					const auto stockpileID = stockpile->id();
					const auto tileID = stockpile->getFields().firstKey();
					Global::eventConnector->aggregatorStockpile()->onOpenStockpileInfo( stockpileID );
					automationTrace( QStringLiteral( "stockpile_empty_created id=%1 tile=%2 fields=%3 contents=%4" )
						.arg( stockpileID ).arg( tileID ).arg( stockpile->getFields().size() ).arg( stockpile->itemCount( tileID ) ) );
				}, Qt::QueuedConnection );
			} );
			if ( !automatedStockpileEmptyCapture.isEmpty() )
			{
				QTimer::singleShot( 15500, &a, [automatedStockpileEmptyCapture]() {
					qputenv( "INGNOMIA_UI_CAPTURE", automatedStockpileEmptyCapture.toUtf8() );
					qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
					MainWindow::getInstance().armUiCapture();
					automationTrace( QStringLiteral( "stockpile_empty_capture armed=true path=%1" ).arg( automatedStockpileEmptyCapture ) );
				} );
			}
			QTimer::singleShot( 19000, &a, []() {
				if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
		}
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_NEW_GAME" ) == "1" )
	{
		QTimer::singleShot( 2500, &a, []() {
			if ( !Global::eventConnector )
			{
				qWarning() << "Automated new-game dispatch skipped: EventConnector unavailable";
				return;
			}
			qInfo() << "Automated new-game dispatching app.start_new_game";
			QMetaObject::invokeMethod( Global::eventConnector, "onStartNewGame", Qt::QueuedConnection );
		} );
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_TUTORIAL" ) == "1" )
	{
		QTimer::singleShot( 2500, &a, []() {
			if ( Global::eventConnector )
			{
				QMetaObject::invokeMethod( Global::eventConnector, "onStartTutorial", Qt::QueuedConnection );
				automationTrace( "tutorial_start dispatched=true" );
			}
			else automationTrace( "tutorial_start dispatched=false" );
		} );
	}
	// Opt-in persistence probe. The caller supplies a copied save folder and
	// data root through the environment; save() therefore creates a new slot
	// only inside that isolated root before the world is ended and reloaded.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE" ) == "1" )
	{
		const QString persistenceMutation = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" );
		const bool persistenceInspectorPriority = persistenceMutation.compare( "inspector_priority", Qt::CaseInsensitive ) == 0;
		const bool persistenceStockpilePriority = persistenceMutation.compare( "stockpile_priority", Qt::CaseInsensitive ) == 0;
		const bool persistenceStockpileFilter = persistenceMutation.compare( "stockpile_filter", Qt::CaseInsensitive ) == 0;
		const bool persistenceStockpilePull = persistenceMutation.compare( "stockpile_pull", Qt::CaseInsensitive ) == 0;
		const bool persistenceStockpileAllowPull = persistenceMutation.compare( "stockpile_allow_pull", Qt::CaseInsensitive ) == 0;
		const bool persistencePopulationProfession = persistenceMutation.compare( "population_profession", Qt::CaseInsensitive ) == 0;
		const bool persistenceExtended = persistenceStockpilePriority || persistenceStockpileFilter || persistenceStockpilePull || persistenceStockpileAllowPull || persistencePopulationProfession;
		const int persistenceSaveDelay = persistenceInspectorPriority ? 15000 : persistenceExtended ? 17000 : 8500;
		const int persistenceEndDelay = persistenceInspectorPriority ? 19000 : persistenceExtended ? 21000 : 12500;
		const int persistenceReloadDelay = persistenceInspectorPriority ? 23000 : persistenceExtended ? 25000 : 16000;
		if ( persistencePopulationProfession )
		{
			QTimer::singleShot( 10000, &a, []() {
				const bool requested = MainWindow::getInstance().createPopulationProfessionForProbe( "CodexProbeProfession" );
				automationTrace( QStringLiteral( "persistence_population_profession_create requested=%1" ).arg( requested ? "true" : "false" ) );
			} );
			QTimer::singleShot( 14500, &a, []() {
				const bool exists = MainWindow::getInstance().populationHasProfessionForProbe( "CodexProbeProfession" );
				automationTrace( QStringLiteral( "persistence_population_profession_before_save exists=%1" ).arg( exists ? "true" : "false" ) );
			} );
			QTimer::singleShot( 27500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_population" );
				automationTrace( QStringLiteral( "persistence_population_profession_reopen opened=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 30000, &a, []() {
				const bool exists = MainWindow::getInstance().populationHasProfessionForProbe( "CodexProbeProfession" );
				automationTrace( QStringLiteral( "persistence_population_profession_after_reload exists=%1" ).arg( exists ? "true" : "false" ) );
			} );
			QTimer::singleShot( 32000, &a, []() {
				if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
		}

		QTimer::singleShot( persistenceSaveDelay, &a, []() {
			if ( !Global::eventConnector ) return;
			automationTrace( QStringLiteral( "persistence_save dispatched=true" ) );
			QMetaObject::invokeMethod( Global::eventConnector, "onSaveGame", Qt::QueuedConnection );
		} );
		QTimer::singleShot( persistenceEndDelay, &a, []() {
			if ( !Global::eventConnector ) return;
			automationTrace( QStringLiteral( "persistence_end dispatched=true" ) );
			QMetaObject::invokeMethod( Global::eventConnector, "onEndGame", Qt::QueuedConnection );
		} );
		QTimer::singleShot( persistenceReloadDelay, &a, []() {
			if ( !Global::eventConnector ) return;
			const auto slot = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_SLOT" );
			if ( slot.isEmpty() )
			{
				automationTrace( QStringLiteral( "persistence_reload dispatched=false missing_slot" ) );
				return;
			}
			automationTrace( QStringLiteral( "persistence_reload dispatched=true path=%1" ).arg( slot ) );
			QMetaObject::invokeMethod( Global::eventConnector, "onLoadGame", Qt::QueuedConnection,
				Q_ARG( QString, slot ) );
		} );
		if ( persistenceInspectorPriority )
		{
			bool tileOk = false;
			const auto tileID = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_INSPECTOR_TILE_ID" ).toUInt( &tileOk );
			if ( tileOk )
			{
				// Freeze the live world before creating the job so its serialized priority
				// cannot be consumed by the simulation before the save boundary.
				QTimer::singleShot( 8500, &a, []() {
					const bool paused = MainWindow::getInstance().activateHudElement( "hud_pause" );
					automationTrace( QStringLiteral( "persistence_priority_pause dispatched=%1" ).arg( paused ? "true" : "false" ) );
				} );
				QTimer::singleShot( 9000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_priority_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				QTimer::singleShot( 11000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_mine" );
					automationTrace( QStringLiteral( "persistence_priority_mine activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 13000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_raise_job" );
					automationTrace( QStringLiteral( "persistence_priority_raise activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 31500, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_priority_reload_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				const QString capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_INSPECTOR_CAPTURE_PATH" );
				if ( !capturePath.isEmpty() )
				{
					QTimer::singleShot( 33500, &a, [capturePath]() {
						qputenv( "INGNOMIA_UI_CAPTURE", capturePath.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						automationTrace( QStringLiteral( "persistence_priority_gl_capture armed=true path=%1" ).arg( capturePath ) );
					} );
				}
				QTimer::singleShot( 37000, &a, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
			}
			else
				automationTrace( QStringLiteral( "persistence_priority_tile dispatched=false invalid" ) );
		}
		else if ( persistenceStockpilePriority )
		{
			bool tileOk = false;
			const auto tileID = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_TILE_ID" ).toUInt( &tileOk );
			if ( tileOk )
			{
				QTimer::singleShot( 8500, &a, []() {
					const bool paused = MainWindow::getInstance().activateHudElement( "hud_pause" );
					automationTrace( QStringLiteral( "persistence_stockpile_pause dispatched=%1" ).arg( paused ? "true" : "false" ) );
				} );
				QTimer::singleShot( 9000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_stockpile_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				QTimer::singleShot( 11000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_manage" );
					automationTrace( QStringLiteral( "persistence_stockpile_manage activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 13500, &a, []() {
					const auto desired = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_PRIORITY" ).trimmed();
					const bool set = MainWindow::getInstance().setManagementFormValueForProbe( "stockpile_priority", desired.toStdString() );
					const bool applied = MainWindow::getInstance().activateManagementElement( "stockpile_apply" );
					automationTrace( QStringLiteral( "persistence_stockpile_apply value=%1 set=%2 applied=%3" ).arg( desired, set ? "true" : "false", applied ? "true" : "false" ) );
				} );
				const auto beforeCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_BEFORE_CAPTURE_PATH" );
				if ( !beforeCapture.isEmpty() )
				{
					QTimer::singleShot( 15500, &a, [beforeCapture]() {
						qputenv( "INGNOMIA_UI_CAPTURE", beforeCapture.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						MainWindow::getInstance().armUiCapture();
						automationTrace( QStringLiteral( "persistence_stockpile_before_capture armed=true path=%1" ).arg( beforeCapture ) );
					} );
				}
				QTimer::singleShot( 31000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_stockpile_reload_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				QTimer::singleShot( 37000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_manage" );
					automationTrace( QStringLiteral( "persistence_stockpile_reload_manage activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 38000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					const bool queued = QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->onManageCommand( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_stockpile_reload_direct_manage queued=%1" ).arg( queued ? "true" : "false" ) );
				} );
				const auto preDirectCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_PRE_DIRECT_CAPTURE_PATH" );
				if ( !preDirectCapture.isEmpty() )
				{
					QTimer::singleShot( 37500, &a, [preDirectCapture]() {
						qputenv( "INGNOMIA_UI_CAPTURE", preDirectCapture.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						MainWindow::getInstance().armUiCapture();
						automationTrace( QStringLiteral( "persistence_stockpile_pre_direct_capture armed=true path=%1" ).arg( preDirectCapture ) );
					} );
				}
				const auto afterCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_AFTER_CAPTURE_PATH" );
				if ( !afterCapture.isEmpty() )
				{
					QTimer::singleShot( 41000, &a, [afterCapture]() {
						qputenv( "INGNOMIA_UI_CAPTURE", afterCapture.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						MainWindow::getInstance().armUiCapture();
						automationTrace( QStringLiteral( "persistence_stockpile_after_capture armed=true path=%1" ).arg( afterCapture ) );
					} );
				}
				QTimer::singleShot( 44000, &a, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
			}
			else
				automationTrace( QStringLiteral( "persistence_stockpile_tile dispatched=false invalid" ) );
		}
		else if ( persistenceStockpileFilter || persistenceStockpilePull || persistenceStockpileAllowPull )
		{
			bool tileOk = false;
			const auto tileID = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_TILE_ID" ).toUInt( &tileOk );
			const char* mutationElement = persistenceStockpileAllowPull ? "stockpile_toggle_allow_pull" : persistenceStockpilePull ? "stockpile_toggle_pull" : "stockpile_toggle_filter";
			if ( tileOk )
			{
				QTimer::singleShot( 8500, &a, []() {
					const bool paused = MainWindow::getInstance().activateHudElement( "hud_pause" );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation_pause dispatched=%1" ).arg( paused ? "true" : "false" ) );
				} );
				QTimer::singleShot( 9000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				QTimer::singleShot( 11000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_manage" );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation_manage activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 13500, &a, [mutationElement]() {
					// The sheet keeps the change pending until Apply.
					const bool activated = MainWindow::getInstance().activateManagementElement( mutationElement )
						&& MainWindow::getInstance().activateManagementElement( "stockpile_apply" );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation element=%1 activated=%2" ).arg( mutationElement, activated ? "true" : "false" ) );
				} );
				const auto beforeCapture = qEnvironmentVariable( persistenceStockpileAllowPull ? "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_ALLOW_PULL_BEFORE_CAPTURE_PATH" : persistenceStockpilePull ? "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_PULL_BEFORE_CAPTURE_PATH" : "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_FILTER_BEFORE_CAPTURE_PATH" );
				if ( !beforeCapture.isEmpty() )
				{
					QTimer::singleShot( 15500, &a, [beforeCapture]() {
						qputenv( "INGNOMIA_UI_CAPTURE", beforeCapture.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						MainWindow::getInstance().armUiCapture();
						automationTrace( QStringLiteral( "persistence_stockpile_mutation_before_capture armed=true path=%1" ).arg( beforeCapture ) );
					} );
				}
				QTimer::singleShot( 31000, &a, [tileID]() {
					if ( !Global::eventConnector ) return;
					QMetaObject::invokeMethod( Global::eventConnector, [tileID]() {
						if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->onShowTileInfo( tileID );
					}, Qt::QueuedConnection );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation_reload_tile dispatched=true id=%1" ).arg( tileID ) );
				} );
				QTimer::singleShot( 37000, &a, []() {
					const bool activated = MainWindow::getInstance().activateInspectorElement( "tile_manage" );
					automationTrace( QStringLiteral( "persistence_stockpile_mutation_reload_manage activated=%1" ).arg( activated ? "true" : "false" ) );
				} );
				const auto afterCapture = qEnvironmentVariable( persistenceStockpileAllowPull ? "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_ALLOW_PULL_AFTER_CAPTURE_PATH" : persistenceStockpilePull ? "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_PULL_AFTER_CAPTURE_PATH" : "INGNOMIA_AUTOMATE_PERSISTENCE_STOCKPILE_FILTER_AFTER_CAPTURE_PATH" );
				if ( !afterCapture.isEmpty() )
				{
					QTimer::singleShot( 41000, &a, [afterCapture]() {
						qputenv( "INGNOMIA_UI_CAPTURE", afterCapture.toUtf8() );
						qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
						MainWindow::getInstance().armUiCapture();
						automationTrace( QStringLiteral( "persistence_stockpile_mutation_after_capture armed=true path=%1" ).arg( afterCapture ) );
					} );
				}
				QTimer::singleShot( 44000, &a, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
			}
			else
				automationTrace( QStringLiteral( "persistence_stockpile_mutation_tile dispatched=false invalid" ) );
		}
		// Optional mutation before the save: exercise the real inventory watch
		// action, then verify the watched row survives the world replacement.
		if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" ).compare( "inventory", Qt::CaseInsensitive ) == 0 )
		{
			QTimer::singleShot( 6500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_inventory" );
				automationTrace( QStringLiteral( "persistence_inventory_open dispatched=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 7600, &a, []() {
				const bool focused = MainWindow::getInstance().focusInventoryRowsForProbe();
				const bool selected = focused && MainWindow::getInstance().dispatchInventoryKeyForProbe( Qt::Key_Down );
				const auto before = MainWindow::getInstance().inventoryWatchStatusForProbe();
				const bool sent = selected && MainWindow::getInstance().dispatchInventoryKeyForProbe( Qt::Key_Space );
				automationTrace( QStringLiteral( "persistence_inventory_watch focus=%1 selected=%2 space_sent=%3 before=%4 immediate=%5" )
					.arg( focused ? "true" : "false", selected ? "true" : "false", sent ? "true" : "false",
						QString::fromStdString( before ), QString::fromStdString( MainWindow::getInstance().inventoryWatchStatusForProbe() ) ) );
			} );
		}
		else if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" ).compare( "population", Qt::CaseInsensitive ) == 0 )
		{
			QTimer::singleShot( 6500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_population" );
				automationTrace( QStringLiteral( "persistence_population_open dispatched=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 7400, &a, []() {
				const bool tab = MainWindow::getInstance().activateManagementElement( "population_tab_schedules" );
				const bool selected = MainWindow::getInstance().activateFirstManagementElement( "schedule" );
				const bool changed = MainWindow::getInstance().activateManagementElement( "schedule_set_eat" );
				automationTrace( QStringLiteral( "persistence_population_schedule tab=%1 selected=%2 changed=%3" ).arg( tab ? "true" : "false", selected ? "true" : "false", changed ? "true" : "false" ) );
			} );
		}
		else if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" ).compare( "military", Qt::CaseInsensitive ) == 0 )
		{
			QTimer::singleShot( 6500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_military" );
				automationTrace( QStringLiteral( "persistence_military_open dispatched=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 7400, &a, []() {
				const bool tab = MainWindow::getInstance().activateManagementElement( "military_tab_roles" );
				const bool selected = MainWindow::getInstance().activateFirstManagementElement( "military_role" );
				const bool added = MainWindow::getInstance().activateManagementElement( "role_add" );
				automationTrace( QStringLiteral( "persistence_military_role tab=%1 selected=%2 added=%3" ).arg( tab ? "true" : "false", selected ? "true" : "false", added ? "true" : "false" ) );
			} );
		}
		else if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" ).compare( "inventory_history", Qt::CaseInsensitive ) == 0 )
		{
			QTimer::singleShot( 6500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_inventory" );
				automationTrace( QStringLiteral( "persistence_inventory_history_open dispatched=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 7600, &a, []() {
				const bool selected = MainWindow::getInstance().activateFirstManagementElement( "inventory" );
				const bool requested = MainWindow::getInstance().activateManagementElement( "inventory_request_history" );
				automationTrace( QStringLiteral( "persistence_inventory_history_request selected=%1 dispatched=%2" ).arg( selected ? "true" : "false", requested ? "true" : "false" ) );
			} );
			QTimer::singleShot( 20500, &a, []() {
				const bool opened = MainWindow::getInstance().activateHudElement( "hud_open_inventory" );
				automationTrace( QStringLiteral( "persistence_inventory_history_reopen dispatched=%1" ).arg( opened ? "true" : "false" ) );
			} );
			QTimer::singleShot( 27500, &a, []() {
				const bool requested = MainWindow::getInstance().requestInventoryHistoryProbe();
				automationTrace( QStringLiteral( "persistence_inventory_history_after_reload requested=%1" ).arg( requested ? "true" : "false" ) );
			} );
			QTimer::singleShot( 29000, &a, []() {
				const auto status = MainWindow::getInstance().inventoryHistoryStatus();
				automationTrace( QStringLiteral( "persistence_inventory_history_status_after_reload=%1" ).arg( QString::fromStdString( status ) ) );
			} );
			QTimer::singleShot( 31000, &a, []() {
				if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
		}
	}
	// Opt-in production route smoke test.  This uses the same closed shell
	// listener/controller path as a real Settings click, then returns through
	// the route's Back control.  Normal launches are unchanged.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_SETTINGS" ) == "1" )
	{
		QTimer::singleShot( 1500, &a, []() {
			const bool opened = MainWindow::getInstance().activateShellElement( "shell-settings" );
			automationTrace( QStringLiteral( "shell_settings dispatched=%1" ).arg( opened ? "true" : "false" ) );
			qInfo() << "Automated shell Settings route dispatched:" << opened;
		} );
		QTimer::singleShot( 3500, &a, []() {
			const bool returned = MainWindow::getInstance().activateShellElement( "shell-back" );
			automationTrace( QStringLiteral( "shell_settings_back dispatched=%1" ).arg( returned ? "true" : "false" ) );
			qInfo() << "Automated shell Settings Back dispatched:" << returned;
		} );
	}
	// Opt-in production settings-input probe. This dispatches the same RmlUi
	// Change events delivered by native range/checkbox controls, then captures
	// the live Settings document and records the persisted config values.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SETTINGS_UI" ) == "1" )
	{
		QTimer::singleShot( 1500, &a, []() {
			const bool opened = MainWindow::getInstance().activateShellElement( "shell-settings" );
			automationTrace( QStringLiteral( "settings_ui_open dispatched=%1" ).arg( opened ? "true" : "false" ) );
		} );
		QTimer::singleShot( 2600, &a, []() {
			const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-ui-scale", 125.0f, false );
			automationTrace( QStringLiteral( "settings_ui_scale_change dispatched=%1 value=125" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 3200, &a, []() {
			const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-minimum-light", 40.0f, false );
			automationTrace( QStringLiteral( "settings_ui_light_change dispatched=%1 value=40" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 3800, &a, []() {
			const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-keyboard-speed", 140.0f, false );
			automationTrace( QStringLiteral( "settings_ui_keyboard_change dispatched=%1 value=140" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 4400, &a, []() {
			const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-wheel-level", 0.0f, true );
			automationTrace( QStringLiteral( "settings_ui_wheel_change dispatched=%1 checked=true" ).arg( dispatched ? "true" : "false" ) );
		} );
		if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SETTINGS_NEW_UI" ) == "1" )
		{
			QTimer::singleShot( 4700, &a, []() {
				const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-master-volume", 30.0f, false );
				automationTrace( QStringLiteral( "settings_ui_volume_change dispatched=%1 value=30" ).arg( dispatched ? "true" : "false" ) );
			} );
			QTimer::singleShot( 4900, &a, []() {
				const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-autosave-interval", 7.0f, false );
				automationTrace( QStringLiteral( "settings_ui_autosave_interval_change dispatched=%1 value=7" ).arg( dispatched ? "true" : "false" ) );
			} );
			QTimer::singleShot( 5100, &a, []() {
				const bool dispatched = MainWindow::getInstance().dispatchShellSettingChangeForProbe( "setting-autosave-continue", 0.0f, true );
				automationTrace( QStringLiteral( "settings_ui_autosave_continue_change dispatched=%1 checked=true" ).arg( dispatched ? "true" : "false" ) );
			} );
		}
		QTimer::singleShot( 5600, &a, []() {
			if ( Global::cfg )
			{
				automationTrace( QStringLiteral( "settings_ui_config uiscale=%1 lightMin=%2 keyboardMoveSpeed=%3 toggleMouseWheel=%4" )
					.arg( Global::cfg->get( "uiscale" ).toFloat(), 0, 'f', 2 )
					.arg( Global::cfg->get( "lightMin" ).toFloat(), 0, 'f', 2 )
					.arg( Global::cfg->get( "keyboardMoveSpeed" ).toInt() )
					.arg( Global::cfg->get( "toggleMouseWheel" ).toBool() ? "true" : "false" ) );
				if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SETTINGS_NEW_UI" ) == "1" )
					automationTrace( QStringLiteral( "settings_ui_new_config volume=%1 autosaveInterval=%2 autosaveContinue=%3" )
						.arg( Global::cfg->get( "AudioMasterVolume" ).toFloat(), 0, 'f', 2 )
						.arg( Global::cfg->get( "AutoSaveInterval" ).toInt() )
						.arg( Global::cfg->get( "AutoSaveContinue" ).toBool() ? "true" : "false" ) );
			}
			const auto capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SETTINGS_UI_CAPTURE_PATH" );
			if ( !capturePath.isEmpty() )
			{
				qputenv( "INGNOMIA_UI_CAPTURE", capturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				automationTrace( QStringLiteral( "settings_ui_capture armed=true path=%1" ).arg( capturePath ) );
			}
		} );
		QTimer::singleShot( 8000, &a, []() {
			if ( Global::eventConnector )
			{
				automationTrace( QStringLiteral( "settings_ui_exit queued=true" ) );
				QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			}
		} );
	}
	// Opt-in settings persistence probe. The values are written through the
	// authoritative AggregatorSettings slots, then the process exits through the
	// normal EventConnector signal so a second process can inspect the config.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SETTINGS" ) == "1" )
	{
		QTimer::singleShot( 6500, &a, []() {
			auto* settings = Global::eventConnector ? Global::eventConnector->aggregatorSettings() : nullptr;
			if ( !settings ) return;
			const bool queued = QMetaObject::invokeMethod( settings, [settings]() {
				settings->onSetUIScale( 1.25f );
				settings->onSetKeyboardSpeed( 140 );
				settings->onSetLightMin( 40 );
				settings->onSetToggleMouseWheel( true );
			}, Qt::QueuedConnection );
			automationTrace( QStringLiteral( "settings_mutation queued=%1" ).arg( queued ? "true" : "false" ) );
		} );
		QTimer::singleShot( 8000, &a, []() {
			if ( !Global::eventConnector ) return;
			automationTrace( QStringLiteral( "settings_refresh queued=true" ) );
			QMetaObject::invokeMethod( Global::eventConnector->aggregatorSettings(), &AggregatorSettings::onRequestSettings, Qt::QueuedConnection );
		} );
		QTimer::singleShot( 10000, &a, []() {
			if ( Global::eventConnector )
			{
				automationTrace( QStringLiteral( "settings_exit queued=true" ) );
				QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			}
		} );
	}
	// Diagnostic-only pause path. This exercises the same queued EventConnector
	// slot used by the legacy keyboard/toolbar pause gesture, without requiring
	// desktop input injection. Normal launches are unchanged.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PAUSE_TOGGLE" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_pause" );
			qInfo() << "Automated pause toggle dispatched through HUD RmlUi listener:" << dispatched;
		} );
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PAUSE_KEY" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			QKeyEvent key( QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral( " " ) );
			QCoreApplication::sendEvent( &MainWindow::getInstance(), &key );
			qInfo() << "Automated Space pause toggle delivered to MainWindow:" << key.isAccepted();
		} );
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PAUSE_MOUSE" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			auto& window = MainWindow::getInstance();
			const QPointF local( 505.0, 22.0 );
			const QPointF global = QPointF( window.position() ) + local;
			QMouseEvent press( QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
			QMouseEvent release( QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
			QCoreApplication::sendEvent( &window, &press );
			QCoreApplication::sendEvent( &window, &release );
			qInfo() << "Automated HUD pause mouse click delivered:" << press.isAccepted() << release.isAccepted();
		} );
	}
	// Opt-in pause-route capture. The HUD Pause button only changes simulation
	// state; Escape follows MainWindow's real shell-navigation path into the
	// registered game.pause route. This seam is inert unless explicitly enabled.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PAUSE_MENU" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			QKeyEvent key( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
			QCoreApplication::sendEvent( &MainWindow::getInstance(), &key );
			automationTrace( QStringLiteral( "pause_menu_escape accepted=%1" ).arg( key.isAccepted() ? "true" : "false" ) );
		} );
		const auto capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_PAUSE_MENU_CAPTURE_PATH" );
		if ( !capturePath.isEmpty() )
		{
			QTimer::singleShot( 8500, &a, [capturePath]() {
				qputenv( "INGNOMIA_UI_CAPTURE", capturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "pause_menu_capture armed=true path=%1" ).arg( capturePath ) );
			} );
		}
		QTimer::singleShot( 11000, &a, []() {
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	// Opt-in event-prompt production probe. It asks the authoritative debug
	// event producer for a DB-backed migration query or acknowledge-only invasion
	// message, then exercises the live HUD modal. No normal launch or game event
	// source is changed by this seam.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_PROMPT" ) == "1" )
	{
		const bool acknowledgeOnly = qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_PROMPT_KIND" ) == "ack";
		const bool queueProbe = qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_PROMPT_QUEUE" ) == "1";
		QTimer::singleShot( 9000, &a, [gm, acknowledgeOnly, queueProbe]() {
			const bool queued = QMetaObject::invokeMethod( gm, [gm, acknowledgeOnly, queueProbe]() {
				if ( auto* game = gm->game() )
				{
					if ( auto* eventManager = game->em() )
					{
						automationTrace( QStringLiteral( "event_prompt_debug executing=true" ) );
						auto handoff = [&]( EventType type, const QVariantMap& debugArgs, bool acknowledge ) {
							eventManager->onDebugEvent( type, debugArgs );
							const auto eventList = eventManager->serialize().value( "EventList" ).toList();
							if ( eventList.isEmpty() || !Global::eventConnector ) return;
							const auto event = eventList.constLast().toMap();
							const auto data = event.value( "Data" ).toMap();
							const auto message = data.value( acknowledge ? "OnSuccess" : "Init" ).toMap();
							QString body = message.value( "Message" ).toString();
							body.replace( "$Num", QString::number( data.value( "Amount" ).toInt() ) );
							const unsigned int eventId = acknowledge ? 0u : event.value( "ID" ).toUInt();
							Global::eventConnector->onEvent( eventId, message.value( "Title" ).toString(), body,
								message.value( "Pause" ).toBool(), !acknowledge );
							automationTrace( QStringLiteral( "event_prompt_db_handoff=true kind=%1 event_id=%2 title=%3" )
								.arg( acknowledge ? QStringLiteral( "ack" ) : QStringLiteral( "yesno" ) )
								.arg( eventId ).arg( message.value( "Title" ).toString() ) );
						};
						if ( queueProbe )
						{
							handoff( EventType::MIGRATION, QVariantMap{}, false );
							QVariantMap invasionArgs;
							invasionArgs.insert( "Amount", 1 );
							invasionArgs.insert( "Type", "Goblin" );
							handoff( EventType::INVASION, invasionArgs, true );
						}
						else if ( acknowledgeOnly )
						{
							QVariantMap invasionArgs;
							invasionArgs.insert( "Amount", 1 );
							invasionArgs.insert( "Type", "Goblin" );
							handoff( EventType::INVASION, invasionArgs, true );
						}
						else
							handoff( EventType::MIGRATION, QVariantMap{}, false );
						return;
					}
				}
				automationTrace( QStringLiteral( "event_prompt_debug unavailable=true" ) );
			}, Qt::QueuedConnection );
			automationTrace( QStringLiteral( "event_prompt_source queued=%1 kind=%2" )
				.arg( queued ? QStringLiteral( "true" ) : QStringLiteral( "false" ) )
				.arg( queueProbe ? QStringLiteral( "fifo" ) : ( acknowledgeOnly ? QStringLiteral( "ack" ) : QStringLiteral( "yesno" ) ) ) );
		} );
		const auto capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_PROMPT_CAPTURE_PATH" );
		if ( !capturePath.isEmpty() )
		{
			QTimer::singleShot( 11500, &a, [capturePath]() {
				qputenv( "INGNOMIA_UI_CAPTURE", capturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				automationTrace( QStringLiteral( "event_prompt_gl_capture armed=true path=%1" ).arg( capturePath ) );
			} );
		}
		QTimer::singleShot( 13500, &a, [acknowledgeOnly, queueProbe]() {
			const char* responseElement = acknowledgeOnly && !queueProbe ? "hud_event_ack" : "hud_event_yes";
			const bool activated = MainWindow::getInstance().activateHudElement( responseElement );
			automationTrace( QStringLiteral( "event_prompt_response element=%1 activated=%2" )
				.arg( responseElement ).arg( activated ? "true" : "false" ) );
		} );
		const auto secondCapturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_QUEUE_SECOND_CAPTURE_PATH" );
		if ( queueProbe && !secondCapturePath.isEmpty() )
		{
			QTimer::singleShot( 14500, &a, [secondCapturePath]() {
				qputenv( "INGNOMIA_UI_CAPTURE", secondCapturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "event_prompt_queue_second_capture armed=true path=%1" ).arg( secondCapturePath ) );
			} );
		}
		if ( queueProbe )
		{
			QTimer::singleShot( 16000, &a, []() {
				const bool activated = MainWindow::getInstance().activateHudElement( "hud_event_ack" );
				automationTrace( QStringLiteral( "event_prompt_queue_response element=hud_event_ack activated=%1" )
					.arg( activated ? "true" : "false" ) );
			} );
		}
		const auto afterResponseCapturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_EVENT_PROMPT_AFTER_RESPONSE_CAPTURE_PATH" );
		if ( !afterResponseCapturePath.isEmpty() )
		{
			QTimer::singleShot( queueProbe ? 17500 : 15000, &a, [afterResponseCapturePath]() {
				qputenv( "INGNOMIA_UI_CAPTURE", afterResponseCapturePath.toUtf8() );
				qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
				MainWindow::getInstance().armUiCapture();
				automationTrace( QStringLiteral( "event_prompt_after_response_capture armed=true path=%1" ).arg( afterResponseCapturePath ) );
			} );
		}
		QTimer::singleShot( queueProbe ? 20500 : 18000, &a, []() {
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_MINE_MENU" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_tool_mine" );
			automationTrace( QStringLiteral( "hud_mine_menu dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
			qInfo() << "Automated Mine menu open dispatched through HUD RmlUi listener:" << dispatched;
		} );
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_MENU" ) == "1" )
	{
		// Saved-world probes may need more than the normal menu delay to finish
		// loading a large world.  Keep the default unchanged, while allowing an
		// explicit delayed production probe to exercise the real loaded-world
		// catalog and placement path instead of racing initialization.
		const int buildDelay = qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_DELAY_MS" ).toInt();
		const int buildBase = buildDelay > 0 ? buildDelay : 7000;
		if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_CENTER_VIEW" ) == "1" )
		{
			QTimer::singleShot( qMax( 0, buildBase - 500 ), &a, []() {
				bool levelOk = false;
				const int level = qEnvironmentVariable( "INGNOMIA_AUTOMATE_VIEW_LEVEL" ).toInt( &levelOk );
				if ( levelOk ) MainWindow::getInstance().onUiSetViewLevel( level );
				auto& window = MainWindow::getInstance();
				if ( window.renderer() )
				{
					window.renderer()->setMove( 0.0f, 0.0f );
					window.renderer()->setScale( 1.0f );
				}
				automationTrace( QStringLiteral( "hud_build_view_reset level=%1" ).arg( levelOk ? QString::number( level ) : QStringLiteral( "default" ) ) );
			} );
		}
		QTimer::singleShot( buildBase, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_tool_build" );
			automationTrace( QStringLiteral( "hud_build_menu dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
			qInfo() << "Automated Build menu open dispatched through HUD RmlUi listener:" << dispatched;
		} );
		QTimer::singleShot( buildBase + 1500, &a, []() {
			const bool terrainActions = qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_TERRAIN_ACTIONS" ) == "1";
			const auto id = terrainActions ? "hud_build_wall" : "hud_build_workshop";
			const bool dispatched = MainWindow::getInstance().activateHudElement( id );
			automationTrace( QString::fromLatin1( terrainActions ? "hud_build_terrain_category dispatched=%1" : "hud_build_workshop dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
			qInfo() << "Automated Build category dispatched through HUD RmlUi listener:" << dispatched;
		} );
		QTimer::singleShot( buildBase + 3000, &a, []() {
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_TERRAIN_ACTIONS" ) == "1" )
			{
				const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_action_FillHole_Palisade" );
				automationTrace( QStringLiteral( "hud_build_fill_hole dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
				return;
			}
			// Crude is a real Workshops row in the Wood category.  This exercises
			// the same delegated data-build listener used by a physical click;
			// the catalog is populated asynchronously, so a false result is
			// retained in the trace instead of being inferred as success.
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_Crude" );
			automationTrace( QStringLiteral( "hud_build_crude dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
			qInfo() << "Automated Build Crude selection dispatched through HUD RmlUi listener:" << dispatched;
		} );
		QTimer::singleShot( buildBase + 4500, &a, []() {
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_TERRAIN_ACTIONS" ) == "1" )
			{
				const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_action_Replace_Palisade" );
				automationTrace( QStringLiteral( "hud_build_replace dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
				return;
			}
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_tool_rotate" );
			automationTrace( QStringLiteral( "hud_build_rotate dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
			qInfo() << "Automated Build rotation dispatched through HUD RmlUi listener:" << dispatched;
		} );
		QTimer::singleShot( buildBase + 5500, &a, []() {
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_TERRAIN_ACTIONS" ) == "1" )
			{
				const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_action_Build_Palisade" );
				automationTrace( QStringLiteral( "hud_build_build dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
				automationTrace( QStringLiteral( "hud_build_terrain_actions_ready=true" ) );
				return;
			}
			auto& window = MainWindow::getInstance();
			QList<QPointF> points;
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_CLICK_SWEEP" ) == "1" )
			{
				for ( const double y : { 560.0, 640.0, 720.0, 800.0, 880.0 } )
					for ( const double x : { 760.0, 920.0, 1080.0, 1240.0, 1400.0 } ) points.push_back( { x, y } );
			}
			else
			{
				bool xOk = false;
				bool yOk = false;
				const double x = qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_CLICK_X" ).toDouble( &xOk );
				const double y = qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_CLICK_Y" ).toDouble( &yOk );
				points.push_back( { xOk ? x : 900.0, yOk ? y : 450.0 } );
			}
			bool accepted = false;
			for ( const auto& local : points )
			{
				const QPointF global = QPointF( window.position() ) + local;
				// Prime RmlUi's pointer-owner state before the synthetic click. A
				// physical user moves over the map before pressing; without this
				// event the first automated click can still be classified as UI input
				// and never exercise the placement-preview path.
				QMouseEvent move( QEvent::MouseMove, local, local, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier );
				QMouseEvent press( QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
				QMouseEvent release( QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
				QCoreApplication::sendEvent( &window, &move );
				QCoreApplication::sendEvent( &window, &press );
				QCoreApplication::sendEvent( &window, &release );
				accepted = accepted || ( press.isAccepted() && release.isAccepted() );
			}
			automationTrace( QStringLiteral( "hud_build_map_click points=%1 accepted=%2" ).arg( points.size() ).arg( accepted ? "true" : "false" ) );
			qInfo() << "Automated Build map placement clicks delivered:" << points.size() << accepted;
		} );
		if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_COMMIT" ) == "1" )
		{
			QTimer::singleShot( buildBase + 7500, &a, []() {
				if ( !Global::eventConnector ) return;
				automationTrace( QStringLiteral( "hud_build_save dispatched=true" ) );
				QMetaObject::invokeMethod( Global::eventConnector, "onSaveGame", Qt::QueuedConnection );
				qInfo() << "Automated Build placement save dispatched through EventConnector";
			} );
			QTimer::singleShot( buildBase + 12500, &a, []() {
				if ( !Global::eventConnector ) return;
				automationTrace( QStringLiteral( "hud_build_exit dispatched=true" ) );
				QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
		}
		else
		{
			QTimer::singleShot( buildBase + 7500, &a, []() {
				const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_tool_cancel" );
				automationTrace( QStringLiteral( "hud_build_cancel dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
				qInfo() << "Automated Build cancel dispatched through HUD RmlUi listener:" << dispatched;
			} );
		}
	}
	// Opt-in real-save build error probe. It selects the furniture catalog's
	// Chair entry, which is unavailable in a material-starved save, and records
	// the typed status instead of attempting a placement. Normal launches are
	// unchanged unless this diagnostic variable is explicitly enabled.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_BUILD_INVALID" ) == "1" )
	{
		QTimer::singleShot( 7000, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_tool_build" );
			automationTrace( QStringLiteral( "hud_invalid_build_menu dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 8500, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_furniture" );
			automationTrace( QStringLiteral( "hud_invalid_build_category dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 10500, &a, []() {
			const bool dispatched = MainWindow::getInstance().activateHudElement( "hud_build_Chair" );
			automationTrace( QStringLiteral( "hud_invalid_build_item dispatched=%1" ).arg( dispatched ? "true" : "false" ) );
		} );
		QTimer::singleShot( 12000, &a, []() {
			const auto status = MainWindow::getInstance().hudStatus();
			automationTrace( QStringLiteral( "hud_invalid_build_status=%1" ).arg( QString::fromStdString( status ) ) );
			if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
		} );
	}
	// Opt-in live-save workbench smoke.  The HUD launcher is the production
	// entry point for each management surface; keeping the choice in an
	// environment variable lets the review matrix run one route per process
	// without adding release-visible controls or fixture-only shortcuts.
	const auto automatedWorkbench = qEnvironmentVariable( "INGNOMIA_AUTOMATE_WORKBENCH" ).trimmed().toLower();
	const int configuredInventoryProbeDelay = qEnvironmentVariableIntValue( "INGNOMIA_AUTOMATE_INVENTORY_DELAY_MS" );
	const int inventoryProbeDelay = configuredInventoryProbeDelay > 0 ? configuredInventoryProbeDelay : 0;
	if ( !automatedWorkbench.isEmpty() )
	{
		QTimer::singleShot( 7000 + ( automatedWorkbench == "inventory" ? inventoryProbeDelay : 0 ), &a, [automatedWorkbench]() {
			QString element;
			if ( automatedWorkbench == "population" ) element = QStringLiteral( "hud_open_population" );
			else if ( automatedWorkbench == "inventory" ) element = QStringLiteral( "hud_open_inventory" );
			else if ( automatedWorkbench == "military" ) element = QStringLiteral( "hud_open_military" );
			else if ( automatedWorkbench == "diplomacy" ) element = QStringLiteral( "hud_open_diplomacy" );
			if ( element.isEmpty() )
			{
				automationTrace( QStringLiteral( "workbench_%1 invalid=true" ).arg( automatedWorkbench ) );
				return;
			}
			const bool dispatched = MainWindow::getInstance().activateHudElement( element.toStdString() );
			automationTrace( QStringLiteral( "workbench_%1 dispatched=%2" ).arg( automatedWorkbench, dispatched ? "true" : "false" ) );
			qInfo() << "Automated live workbench launcher dispatched:" << automatedWorkbench << dispatched;
		} );
		QTimer::singleShot( 9000 + ( automatedWorkbench == "inventory" ? inventoryProbeDelay : 0 ), &a, [automatedWorkbench]() {
			const auto activate = []( std::string_view id ) {
				const bool result = MainWindow::getInstance().activateManagementElement( id );
				automationTrace( QStringLiteral( "management_element_%1 dispatched=%2" ).arg( QString::fromUtf8( id.data(), static_cast<qsizetype>( id.size() ) ), result ? "true" : "false" ) );
				return result;
			};
			if ( automatedWorkbench == "population" )
			{
				activate( "population_tab_schedules" );
				QTimer::singleShot( 1500, qApp, []() {
					const bool selected = MainWindow::getInstance().activateFirstManagementElement( "schedule" );
					automationTrace( QStringLiteral( "management_first_schedule dispatched=%1" ).arg( selected ? "true" : "false" ) );
				} );
				QTimer::singleShot( 1900, qApp, []() {
					const bool changed = MainWindow::getInstance().activateManagementElement( "schedule_set_eat" );
					automationTrace( QStringLiteral( "management_schedule_set_eat dispatched=%1" ).arg( changed ? "true" : "false" ) );
				} );
			}
			else if ( automatedWorkbench == "inventory" )
			{
				activate( "inventory_sort_total" );
				if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_WATCH" ) == "1" )
				{
					QTimer::singleShot( 1000, qApp, []() {
						const bool focused = MainWindow::getInstance().focusInventoryRowsForProbe();
						const bool moved = focused && MainWindow::getInstance().dispatchInventoryKeyForProbe( Qt::Key_Down );
						automationTrace( QStringLiteral( "inventory_watch_focus=%1 down_sent=%2 selected=%3" )
							.arg( focused ? "true" : "false", moved ? "true" : "false",
								QString::fromStdString( MainWindow::getInstance().inventoryWatchStatusForProbe() ) ) );
					} );
					QTimer::singleShot( 1600, qApp, []() {
						const auto before = MainWindow::getInstance().inventoryWatchStatusForProbe();
						const bool sent = MainWindow::getInstance().dispatchInventoryKeyForProbe( Qt::Key_Space );
						automationTrace( QStringLiteral( "inventory_watch_space_sent=%1 before=%2 immediate=%3" )
							.arg( sent ? "true" : "false", QString::fromStdString( before ),
								QString::fromStdString( MainWindow::getInstance().inventoryWatchStatusForProbe() ) ) );
					} );
					QTimer::singleShot( 2600, qApp, []() {
						const auto status = MainWindow::getInstance().inventoryWatchStatusForProbe();
						automationTrace( QStringLiteral( "inventory_watch_snapshot=%1" ).arg( QString::fromStdString( status ) ) );
						const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_WATCH_CAPTURE_PATH" );
						if ( !path.isEmpty() )
						{
							const bool armed = MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
							automationTrace( QStringLiteral( "inventory_watch_capture_armed=%1 path=%2" ).arg( armed ? "true" : "false", path ) );
						}
					} );
					QTimer::singleShot( 3400, qApp, []() {
						if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
					} );
					return;
				}
				QTimer::singleShot( 1000, qApp, []() {
					const bool selected = MainWindow::getInstance().activateFirstManagementElement( "inventory" );
					automationTrace( QStringLiteral( "management_first_inventory dispatched=%1" ).arg( selected ? "true" : "false" ) );
				} );
			}
			else if ( automatedWorkbench == "military" )
			{
				activate( "military_tab_roles" );
				QTimer::singleShot( 1000, qApp, []() {
					const bool selected = MainWindow::getInstance().activateFirstManagementElement( "military_role" );
					automationTrace( QStringLiteral( "management_first_military_role dispatched=%1" ).arg( selected ? "true" : "false" ) );
					const bool added = MainWindow::getInstance().activateManagementElement( "role_add" );
					automationTrace( QStringLiteral( "management_military_role_add dispatched=%1" ).arg( added ? "true" : "false" ) );
				} );
			}
			else if ( automatedWorkbench == "diplomacy" )
			{
				activate( "diplomacy_tab_missions" );
				activate( "diplomacy_tab_neighbors" );
				QTimer::singleShot( 1000, qApp, []() {
					const bool selected = MainWindow::getInstance().activateFirstManagementElement( "diplomacy_neighbor" );
					automationTrace( QStringLiteral( "management_first_diplomacy_neighbor dispatched=%1" ).arg( selected ? "true" : "false" ) );
				} );
			}
		} );
	}
	// Opt-in authoritative inventory-history probe. The workbench must be opened
	// through the same HUD route as a normal player; only the final row selection
	// and request are automated for evidence collection.
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_HISTORY" ) == "1" )
	{
		QTimer::singleShot( 10500 + inventoryProbeDelay, &a, []() {
			const bool requested = MainWindow::getInstance().requestInventoryHistoryProbe();
			automationTrace( QStringLiteral( "inventory_history_request dispatched=%1" ).arg( requested ? "true" : "false" ) );
		} );
		QTimer::singleShot( 13000 + inventoryProbeDelay, &a, []() {
			const auto status = MainWindow::getInstance().inventoryHistoryStatus();
			automationTrace( QStringLiteral( "inventory_history_status=%1" ).arg( QString::fromStdString( status ) ) );
			const auto detailCapture = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_DETAIL_CAPTURE_PATH" );
			if ( !detailCapture.isEmpty() )
			{
				const bool armed = MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", detailCapture );
				automationTrace( QStringLiteral( "inventory_detail_capture armed=%1 path=%2" ).arg( armed ? "true" : "false", detailCapture ) );
			}
			const auto capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_CAPTURE_PATH" );
			if ( !capturePath.isEmpty() && QGuiApplication::primaryScreen() )
				QGuiApplication::primaryScreen()->grabWindow( MainWindow::getInstance().winId() ).save( capturePath );
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_SCROLL_PROBE" ) == "1" )
			{
				QTimer::singleShot( 400, qApp, []() {
					const int recipes = MainWindow::getInstance().openLongestInventoryProductsForProbe();
					automationTrace( QStringLiteral( "inventory_scroll_recipes=%1 item=%2" ).arg( recipes ).arg( QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 1000, qApp, []() {
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_SCROLL_CAPTURE_PATH" );
					if ( !path.isEmpty() ) MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
					automationTrace( QStringLiteral( "inventory_scroll_before=%1" ).arg( QString::fromStdString( MainWindow::getInstance().inventoryProductScrollStatusForProbe() ) ) );
				} );
				QTimer::singleShot( 1500, qApp, []() {
					const bool scrolled = MainWindow::getInstance().scrollInventoryProductsForProbe();
					automationTrace( QStringLiteral( "inventory_scroll_wheel sent=%1" ).arg( scrolled ? "true" : "false" ) );
				} );
				QTimer::singleShot( 2200, qApp, []() {
					automationTrace( QStringLiteral( "inventory_scroll_after=%1" ).arg( QString::fromStdString( MainWindow::getInstance().inventoryProductScrollStatusForProbe() ) ) );
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_SCROLL_AFTER_CAPTURE_PATH" );
					if ( !path.isEmpty() ) MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
				} );
				QTimer::singleShot( 2900, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "product_last" );
					automationTrace( QStringLiteral( "inventory_scroll_product_click=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 3400, qApp, []() {
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_PRODUCT_CAPTURE_PATH" );
					if ( !path.isEmpty() ) MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
				} );
				QTimer::singleShot( 3900, qApp, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
				return;
			}
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_POINTER_PROBE" ) == "1" )
			{
				QTimer::singleShot( 400, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "product" );
					automationTrace( QStringLiteral( "inventory_pointer_product clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 1000, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "back" );
					automationTrace( QStringLiteral( "inventory_pointer_back_item clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 1600, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "back" );
					automationTrace( QStringLiteral( "inventory_pointer_back_previous clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 1900, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "back" );
					automationTrace( QStringLiteral( "inventory_pointer_back_list clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_BACK_CAPTURE_PATH" );
					if ( !path.isEmpty() ) MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
				} );
				QTimer::singleShot( 2500, qApp, []() {
					const bool opened = MainWindow::getInstance().requestInventoryHistoryProbe();
					automationTrace( QStringLiteral( "inventory_pointer_reopen opened=%1 item=%2" ).arg( opened ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 3200, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "ingredient" );
					automationTrace( QStringLiteral( "inventory_pointer_ingredient clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 3800, qApp, []() {
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "back" );
					automationTrace( QStringLiteral( "inventory_pointer_back_from_ingredient clicked=%1 item=%2" ).arg( clicked ? "true" : "false", QString::fromStdString( MainWindow::getInstance().inventoryDetailItemForProbe() ) ) );
				} );
				QTimer::singleShot( 4400, qApp, []() {
                    // Some raw materials have no ingredient link; reopen the original target.
                    MainWindow::getInstance().requestInventoryHistoryProbe();
					const bool clicked = MainWindow::getInstance().clickInventoryDetailForProbe( "stockpile" );
					automationTrace( QStringLiteral( "inventory_pointer_stockpile clicked=%1" ).arg( clicked ? "true" : "false" ) );
				} );
				QTimer::singleShot( 5400, qApp, []() {
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_STOCKPILE_CAPTURE_PATH" );
					if ( !path.isEmpty() ) MainWindow::getInstance().requestManagementCaptureForProbe( "stockpile", path );
				} );
				QTimer::singleShot( 6400, qApp, []() {
					if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
				} );
				return;
			}
			const auto stockpileID = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_STOCKPILE_LINK_ID" );
			if ( !stockpileID.isEmpty() )
			{
				QTimer::singleShot( 400, qApp, [stockpileID]() {
					const bool activated = MainWindow::getInstance().activateManagementElement( ( "inventory_stockpile_" + stockpileID ).toStdString() );
					automationTrace( QStringLiteral( "inventory_stockpile_link id=%1 activated=%2" ).arg( stockpileID, activated ? "true" : "false" ) );
				} );
				QTimer::singleShot( 1400, qApp, []() {
					const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INVENTORY_STOCKPILE_CAPTURE_PATH" );
					if ( !path.isEmpty() )
					{
						const bool armed = MainWindow::getInstance().requestManagementCaptureForProbe( "stockpile", path );
						automationTrace( QStringLiteral( "inventory_stockpile_capture armed=%1 path=%2" ).arg( armed ? "true" : "false", path ) );
					}
				} );
			}
			QTimer::singleShot( !stockpileID.isEmpty() ? 2500 : detailCapture.isEmpty() ? 0 : 1000, qApp, []() {
				if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
			} );
		} );
	}
	if( Global::cfg->get( "fullscreen" ).toBool() )
	{
		w.onFullScreen( true );
	}

	auto ret = a.exec();

	// Stop the simulation on its owning thread before shutting the thread down.
	// Terminating a live Game/QTimer thread left the saved-world exit path with
	// an intermittent 0xC0000409 process failure and could leave the renderer
	// observing a half-retired Game.  A blocking queued stop gives Game the same
	// cleanup path as a normal world replacement; quit is retained as the normal
	// QThread shutdown, with terminate only as a last-resort timeout fallback.
	if ( gm && gameThread.isRunning() )
	{
		QMetaObject::invokeMethod( gm, &GameManager::endCurrentGame, Qt::BlockingQueuedConnection );
		// Release OpenAL on its owning thread before DLL/process teardown.
		if ( auto* connector = Global::eventConnector )
			QMetaObject::invokeMethod( connector, [connector] { connector->shutdownAudio(); }, Qt::BlockingQueuedConnection );
		gameThread.quit();
		if ( !gameThread.wait( 5000 ) )
		{
			gameThread.terminate();
			gameThread.wait();
		}
	}

	return ret;
}

#ifdef _WIN32
INT WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow )
{
	return main( 0, nullptr );
}

extern "C"
{
	// Request use of dedicated GPUs for NVidia/AMD/iGPU mixed setups
	__declspec( dllexport ) DWORD NvOptimusEnablement                  = 1;
	__declspec( dllexport ) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif // _WIN32

