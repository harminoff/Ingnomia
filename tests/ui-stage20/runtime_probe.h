/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 20 live probe (run with INGNOMIA_AUTOMATE_HIGH_CONTRAST=1 to force High Contrast): starts at the main menu,
// uses access keys through real Qt key events (Alt+L, Esc, Alt+C), captures the menu, Load Game, the HUD and
// Population in High Contrast, and checks that the Build palette remembers where it was moved.
// Enabled with INGNOMIA_AUTOMATE_STAGE20_LIVE=1; captures land in INGNOMIA_AUTOMATE_STAGE20_DIR.
namespace stage20
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString shell( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().shellStage18Probe( action ) ); }
inline QString hud( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().hudStage17Probe( action ) ); }
inline void step( const QString& r ) { trace( r.startsWith( "PASS" ), r ); }
inline QString field( const QString& state, const QString& key )
{
	const auto start = state.indexOf( " " + key + "=" );
	if ( start < 0 ) return {};
	const auto from = start + key.size() + 2;
	const auto end  = state.indexOf( ' ', from );
	return state.mid( from, end < 0 ? -1 : end - from );
}
inline void capture( const char* kind, const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE20_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( kind, path ), QString( "capture requested %1" ).arg( name ) );
}
inline std::string key( int qtKey, Qt::KeyboardModifiers mods = Qt::NoModifier ) { return "key:" + std::to_string( qtKey ) + ":" + std::to_string( int( mods ) ); }

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE20_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static QString moved;
	at( 5000, [] {
		trace( field( shell( "state" ), "route" ) == "shell.main_menu", "main menu shown" );
		capture( "game", "main-menu" );
	} );
	at( 5600, [] { step( hud( key( Qt::Key_L, Qt::AltModifier ) ) ); } );
	at( 6600, [] {
		trace( field( shell( "state" ), "route" ) == "shell.load_game", "Alt+L opened Load Game through the game window's key handling" );
		capture( "game", "load-game" );
	} );
	at( 7200, [] { step( hud( key( Qt::Key_Escape ) ) ); } );
	at( 8000, [] {
		const auto s = shell( "state" );
		trace( field( s, "route" ) == "shell.main_menu" && field( s, "focus" ) == "shell-load", "Esc closed Load Game and the focus returned to Load Game" );
		step( hud( key( Qt::Key_C, Qt::AltModifier ) ) );
	} );
	at( 18000, [] {
		trace( field( shell( "state" ), "route" ) == "game.hud", "Alt+C continued into the copied save" );
		capture( "game", "hud" );
	} );
	at( 18600, [] { trace( MainWindow::getInstance().activateHudElement( "hud_open_population" ), "Population opened" ); } );
	at( 20000, [] { capture( "population", "population" ); } );
	at( 21000, [] { step( hud( "click:hud_tool_build" ) ); } );
	at( 22400, [] {
		const auto r = hud( "build-move:60:40" );
		step( r );
		moved = r.section( "position=", 1 ).section( ' ', 0, 0 );
		capture( "build", "build" );
	} );
	at( 23000, [] { step( hud( "build|click:hud_build_close" ) ); } );
	at( 23800, [] {
		const auto x = hud( "config:UiWindow.orders.build.position.x" ).section( ' ', 1 );
		const auto y = hud( "config:UiWindow.orders.build.position.y" ).section( ' ', 1 );
		trace( moved == x + "," + y, "closing the Build palette remembered where it was moved: " + moved + " saved " + x + "," + y );
	} );
	at( 24200, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage20
