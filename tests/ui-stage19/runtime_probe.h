/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 19 live probe. Normal mode starts at the main menu: Settings (four pages, a setting changed and restored,
// Close), Continue into the copied save, the Pause dialog over the map, Settings from Pause, Main Menu with its
// message box cancelled, and Resume. Missing-save mode (INGNOMIA_AUTOMATE_SHELL_STAGE19_MISSING=1) loads a save folder
// that does not exist and checks that the game shows the failure instead of crashing (NEW-004).
// Enabled with INGNOMIA_AUTOMATE_SHELL_STAGE19_LIVE=1; captures land in INGNOMIA_AUTOMATE_SHELL_STAGE19_DIR.
namespace stage19
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
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE19_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "game", path ), QString( "capture requested %1" ).arg( name ) );
}

inline void scheduleMissing( const std::function<void( int, std::function<void()> )>& at )
{
	at( 9000, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "route" ) == "shell.loading" && field( s, "loadingError" ) == "1" && field( s, "retry" ) == "1", "a missing save shows the failure with Retry and Cancel instead of crashing" );
		trace( field( s, "cursor" ) != QString::number( int( Qt::WaitCursor ) ), "the hourglass is gone once the game asks the player" );
		capture( "loading-failed" );
	} );
	at( 9600, [] { step( "click:shell-back" ); } );
	at( 10400, [] { trace( field( probe( "state" ), "route" ) == "shell.main_menu", "Cancel returns to the main menu" ); } );
	at( 10800, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE19_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE19_MISSING" ) == "1" ) { scheduleMissing( at ); return; }
	static QString wheelBefore;
	at( 5000, [] { step( "click:shell-settings" ); } );
	at( 5800, [] {
		const auto s = probe( "state" );
		trace( field( s, "route" ) == "shell.settings" && field( s, "settingsPage" ) == "settings-page-display" && field( s, "inGame" ) == "0", "Settings opens on Display over the desktop" );
		capture( "settings-display" );
	} );
	at( 6200, [] { step( "click:settings-tab-controls" ); } );
	at( 6600, [] { trace( field( probe( "state" ), "settingsPage" ) == "settings-page-controls", "the Controls tab shows its page" ); capture( "settings-controls" ); } );
	at( 7000, [] { step( "click:setting-wheel-level" ); } );
	at( 7600, [] { step( "click:setting-wheel-level" ); } );
	at( 8000, [] { step( "click:settings-tab-sound" ); } );
	at( 8400, [] { trace( field( probe( "state" ), "settingsPage" ) == "settings-page-sound", "the Sound tab shows its page" ); capture( "settings-sound" ); } );
	at( 8800, [] { step( "click:settings-tab-saving" ); } );
	at( 9200, [] { trace( field( probe( "state" ), "settingsPage" ) == "settings-page-saving", "the Saving tab shows its page" ); capture( "settings-saving" ); } );
	at( 9600, [] { step( "click:shell-back" ); } );
	at( 10200, [] {
		const auto s = probe( "state" );
		trace( field( s, "route" ) == "shell.main_menu" && field( s, "focus" ) == "shell-settings", "Close returns to the main menu with the focus on Settings" );
		step( "click:shell-continue" );
	} );
	at( 10260, [] { capture( "loading" ); } );
	at( 20000, [] { trace( field( probe( "state" ), "route" ) == "game.hud", "Continue opened the copied save" ); } );
	at( 20400, [] { step( "escape" ); } );
	at( 21400, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "route" ) == "game.pause" && field( s, "inGame" ) == "1" && field( s, "paused" ) == "1", "Esc in the game shows Pause over the paused map" );
		capture( "pause" );
	} );
	at( 21800, [] { step( "click:pause-settings" ); } );
	at( 22600, [] {
		const auto s = probe( "state" );
		trace( field( s, "route" ) == "game.settings" && field( s, "inGame" ) == "1", "Settings from Pause sits over the map" );
		capture( "settings-in-game" );
		step( "click:shell-back" );
	} );
	at( 23400, [] {
		trace( field( probe( "state" ), "route" ) == "game.pause", "Close returns to Pause" );
		step( "click:pause-menu" );
	} );
	at( 24000, [] {
		trace( field( probe( "state" ), "confirm" ) == "1", "Main Menu asks first in a message box" );
		capture( "leave-confirmation" );
	} );
	at( 24400, [] { step( "dialog:cancel" ); } );
	at( 24900, [] {
		const auto s = probe( "state" );
		trace( field( s, "confirm" ) == "0" && field( s, "route" ) == "game.pause", "Cancel keeps the kingdom open" );
		step( "escape" );
	} );
	at( 26000, [] { trace( field( probe( "state" ), "route" ) == "game.hud", "Esc on Pause resumes the game" ); } );
	at( 26400, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage19
