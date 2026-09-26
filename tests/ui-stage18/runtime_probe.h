/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 18 live probe: starts at the main menu (no automatic load), walks the Custom Game wizard and cancels it,
// checks that the focus returns to Custom Game, then opens a copied save from the Load Game dialog by double-click.
// Enabled with INGNOMIA_AUTOMATE_SHELL_STAGE18_LIVE=1; captures land in INGNOMIA_AUTOMATE_SHELL_STAGE18_DIR.
namespace stage18
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().shellStage18Probe( action ) ); }
inline void step( const std::string& action ) { const auto r = probe( action ); trace( r.startsWith( "PASS" ), r ); }
inline QString field( const QString& state, const QString& key )
{
	const auto start = state.indexOf( " " + key + "=" );
	if ( start < 0 ) return {};
	const auto from = start + key.size() + 2;
	const auto end  = state.indexOf( ' ', from );
	return state.mid( from, end < 0 ? -1 : end - from );
}
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE18_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "game", path ), QString( "capture requested %1" ).arg( name ) );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE18_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	at( 5000, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "route" ) == "shell.main_menu", "the main menu is shown" );
		trace( field( s, "focus" ) == "shell-continue", "Continue, the default command, has the focus: " + field( s, "focus" ) );
		trace( field( s, "pending" ) == "0", "no request is left pending at start" );
		capture( "main-menu" );
	} );
	at( 5600, [] { step( "click:shell-new-setup" ); } );
	at( 6400, [] {
		const auto s = probe( "state" );
		trace( field( s, "page" ) == "new-panel-welcome" && field( s, "backEnabled" ) == "0" && field( s, "next" ) == "1" && field( s, "finish" ) == "0", "the wizard opens on Welcome with Back unavailable" );
		capture( "wizard-welcome" );
	} );
	at( 6800, [] { step( "click:new-next" ); } );
	at( 7200, [] { trace( field( probe( "state" ), "page" ) == "new-panel-world", "Next shows World" ); capture( "wizard-world" ); } );
	at( 7600, [] { step( "click:new-next" ); } );
	at( 8000, [] { trace( field( probe( "state" ), "page" ) == "new-panel-settlement", "Next shows Settlement" ); capture( "wizard-settlement" ); } );
	at( 8400, [] { step( "click:new-next" ); } );
	at( 8800, [] { trace( field( probe( "state" ), "page" ) == "new-panel-terrain", "Next shows Terrain and Life" ); capture( "wizard-terrain" ); } );
	at( 9200, [] { step( "click:new-next" ); } );
	at( 9600, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "page" ) == "new-panel-review" && field( s, "finish" ) == "1" && field( s, "next" ) == "0" && field( s, "finishEnabled" ) == "1", "Completion shows an available Finish in place of Next" );
		capture( "wizard-completion" );
	} );
	at( 10000, [] { step( "click:new-back" ); } );
	at( 10400, [] {
		trace( field( probe( "state" ), "page" ) == "new-panel-terrain", "Back returns to the previous page" );
		step( "click:shell-back" );
	} );
	at( 11000, [] {
		const auto s = probe( "state" );
		trace( field( s, "route" ) == "shell.main_menu" && field( s, "focus" ) == "shell-new-setup", "Cancel returns to the main menu with the focus on Custom Game (NEW-002)" );
		step( "click:shell-load" );
	} );
	at( 12200, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "route" ) == "shell.load_game" && field( s, "kingdoms" ).toInt() >= 1, "Load Game lists the kingdoms under Look in" );
		step( "choose-index:load-kingdoms:0" );
	} );
	at( 13400, [] {
		const auto s = probe( "state" );
		trace( field( s, "saves" ).toInt() >= 1, "choosing a kingdom lists its saves: " + field( s, "saves" ) );
		step( "click-row:load-saves:0" );
	} );
	at( 14000, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "selectedSave" ) == "1" && field( s, "fileName" ) != "-", "a click selects the save and names it: " + field( s, "fileName" ) );
		trace( field( s, "openEnabled" ) == "1", "Open is available for a compatible save" );
		capture( "load-game" );
	} );
	at( 14600, [] { step( "dblclick:save-row-0" ); } );
	at( 22600, [] { trace( field( probe( "state" ), "route" ) == "game.hud", "a double-click opened the save" ); } );
	at( 23100, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage18
