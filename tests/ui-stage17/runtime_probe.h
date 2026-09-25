/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 17 live probe: on a copied save, walks the game window's toolbar, drop-down menus and status bar, arms and
// cancels a tool, toggles a View setting, opens the Build window and arms Build from it, and answers an information
// message box. Enabled with INGNOMIA_AUTOMATE_HUD_STAGE17_LIVE=1; captures land in INGNOMIA_AUTOMATE_HUD_STAGE17_DIR.
namespace stage17
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().hudStage17Probe( action ) ); }
inline void step( const std::string& action ) { const auto r = probe( action ); trace( r.startsWith( "PASS" ), r ); }
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
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_STAGE17_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( kind, path ), QString( "capture requested %1" ).arg( name ) );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_STAGE17_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static QString buildAction;
	at( 9000, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "status" ) == "Ready" && field( s, "level" ) == "Level_" + field( s, "gameLevel" ), "status bar shows Ready and the level being viewed: " + field( s, "level" ) );
		trace( field( s, "cancelEnabled" ) == "0", "Cancel Tool is unavailable while no tool is armed" );
		capture( "game", "hud" );
	} );
	at( 9600, [] { step( "hover:hud_open_population" ); } );
	at( 9900, [] {
		const auto s = probe( "state" );
		trace( field( s, "status" ).startsWith( "Opens_the_citizens" ), "status bar describes the control under the pointer: " + field( s, "status" ) );
		step( "hover:hud_tool_cancel" );
	} );
	at( 10200, [] {
		trace( field( probe( "state" ), "status" ).contains( "Unavailable_because_no_tool" ), "an unavailable control says why" );
		step( "unhover" );
		step( "click:hud_tool_mine" );
	} );
	at( 10700, [] {
		const auto s = probe( "state" );
		trace( field( s, "menus" ) == "hud_mine_menu,", "Mine opens its drop-down menu" );
		capture( "game", "mine-menu" );
	} );
	at( 11300, [] { step( "click:hud_mine_walls" ); } );
	at( 11800, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "tool" ) == "mine" && field( s, "selected" ).contains( "hud_tool_mine," ) && field( s, "menus" ).isEmpty(), "Mine Walls arms the tool, closes the menu and sets the Mine button" );
		trace( field( s, "activeTool" ) == "Mine_Walls" && field( s, "status" ).contains( "Esc_cancels" ), "status bar names the mode and how to leave it" );
		trace( field( s, "cursor" ) == QString::number( int( Qt::CrossCursor ) ), "the pointer over the map shows the armed tool" );
		trace( field( s, "cancelEnabled" ) == "1", "Cancel Tool is available while a tool is armed" );
		capture( "game", "armed" );
	} );
	at( 12400, [] { step( "click:hud_tool_cancel" ); } );
	at( 12900, [] {
		const auto s = probe( "state" );
		trace( field( s, "tool" ) == "-" && !field( s, "selected" ).contains( "hud_tool_mine," ) && field( s, "cursor" ) == QString::number( int( Qt::ArrowCursor ) ), "Cancel Tool leaves the mode and restores the pointer" );
		step( "click:hud_tool_view" );
	} );
	at( 13400, [] {
		trace( field( probe( "state" ), "menus" ) == "hud_view_menu,", "View opens its menu" );
		capture( "game", "view-menu" );
	} );
	at( 13900, [] { step( "click:hud_overlay_jobs" ); } );
	at( 14500, [] {
		const auto s = probe( "state" );
		trace( field( s, "overlayJobs" ) == "1" && field( s, "menus" ).isEmpty(), "the Jobs setting turned on and the menu closed" );
		step( "click:hud_tool_view" );
		step( "click:hud_overlay_jobs" );
	} );
	at( 15100, [] {
		trace( field( probe( "state" ), "overlayJobs" ) == "0", "the Jobs setting turned off again" );
		step( "click:hud_speed_fast" );
	} );
	at( 15600, [] {
		const auto s = probe( "state" );
		trace( field( s, "selected" ).contains( "hud_speed_fast," ) && !field( s, "selected" ).contains( "hud_speed_normal," ), "Fast Speed is set and Normal Speed is not" );
		step( "click:hud_speed_normal" );
		step( "click:hud_tool_build" );
	} );
	at( 17000, [] {
		const auto s = probe( "state" );
		trace( field( s, "build" ) == "1", "Build opens the Build window" );
		step( "build|click:hud_build_furniture" );
	} );
	at( 18500, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "buildRows" ).toInt() > 0 && field( s, "buildRow" ).startsWith( "hud_build_" ), "the Furniture list shows items with one chosen: " + field( s, "buildRow" ) );
		buildAction = "hud_build_action_Build_" + field( s, "buildRow" ).mid( 10 );
		capture( "build", "build" );
	} );
	at( 19100, [] { step( "build|click:" + buildAction.toStdString() ); } );
	at( 19800, [] {
		const auto s = probe( "state" );
		trace( field( s, "tool" ) == "build" && field( s, "selected" ).contains( "hud_tool_build," ), "Build arms placement and the toolbar shows the mode" );
		capture( "game", "build-armed" );
	} );
	at( 20400, [] { step( "click:hud_tool_cancel" ); step( "build|click:hud_build_close" ); } );
	at( 21000, [] {
		const auto s = probe( "state" );
		trace( field( s, "tool" ) == "-" && field( s, "build" ) == "0", "Cancel Tool and Close leave Build" );
		step( "prompt" );
	} );
	at( 21600, [] {
		trace( field( probe( "state" ), "prompt" ) == "1", "an event shows one message box" );
		capture( "game", "message-box" );
	} );
	at( 22200, [] { step( "click:hud_event_ack" ); } );
	at( 22700, [] { trace( field( probe( "state" ), "prompt" ) == "0", "OK closes the message box" ); } );
	at( 23200, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage17
