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
inline QString frame( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().windowFrameProbe( action ) ); }
inline void frameStep( const std::string& action ) { const auto r = frame( action ); trace( r.startsWith( "PASS" ), r ); }
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
	// The Build palette's window menu holds Move and Close (PDF p.113); typing C closes the palette.
	at( 20200, [] { step( "build|activate" ); } );
	at( 20400, [] {
		step( "click:hud_tool_cancel" );
		const auto menu = probe( "build|menu" );
		trace( menu.contains( "menu open move+m always_on_top+t close+c onTop=0" ), "Alt+Space opens the palette's window menu with Move, Always on Top and Close: " + menu );
		const auto typed = probe( "build|menu-key:t" );
		trace( typed.contains( "onTop=1" ), "Always on Top keeps the palette above its peers: " + typed );
		const auto again = probe( "build|menu" );
		trace( again.contains( "always_on_top+t*" ), "the menu checks Always on Top while it is set: " + again );
		step( "build|menu-key:t" );
		// What's This? from the secondary button on a control (PDF p.286-287).
		const auto right = probe( "build|right:hud_build_furniture" );
		trace( right.contains( "menu=1" ), "the secondary button on a palette control offers What's This?: " + right );
		const auto shown = probe( "build|menu-key:w" );
		trace( shown.contains( "help=open" ), "choosing What's This? explains the control: " + shown );
	} );
	at( 20700, [] {
		step( "build|help" );
		step( "build|menu" );
		step( "build|menu-key:c" );
	} );
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
	// What's This? in the game window (PDF p.285-286): the toolbar button starts the mode and the Help pointer; the next
	// click explains the item in a pop-up and ends the mode; the click after that only closes the pop-up.
	at( 22750, [] {
		const auto s = frame( "whats-this" );
		trace( s.contains( "mode=1 cursor=" + QString::number( int( Qt::WhatsThisCursor ) ) ), "What's This? starts the mode with the Help pointer: " + s );
	} );
	at( 22850, [] {
		const auto s = frame( "whats-click:hud_pause" );
		trace( s.contains( "mode=0" ) && !s.contains( "popup=-" ), "clicking Pause in the mode explains it: " + s );
		capture( "game", "whats-this" );
	} );
	at( 23050, [] {
		const auto s = frame( "whats-click:hud_pause" );
		trace( s.contains( "popup=-" ) && s.contains( "mode=0" ), "the next click only closes the pop-up: " + s );
	} );
	// Primary window frame (PDF p.311-314): 4 px sizing border, 18 px caption, 16 x 14 caption buttons with Close
	// 2 px apart, client area below a 1 px line; maximize fills the work area without the border; double-clicking
	// the caption restores; Minimize minimizes; Close closes the game.
	at( 23200, [] {
		const auto s = frame( "state" );
		automationTrace( s );
		const QRegularExpression sep( "[,x]" );
		const auto cap = field( s, "caption" ).split( sep ), mn = field( s, "min" ).split( sep ), mx = field( s, "max" ).split( sep );
		const auto cl = field( s, "close" ).split( sep ), ic = field( s, "icon" ).split( sep ), ctx = field( s, "context" ).split( 'x' );
		const int u = cap.value( 3 ).toInt() / 18;
		const int w = ctx.value( 0 ).toInt(), h = ctx.value( 1 ).toInt();
		trace( field( s, "visible" ) == "1" && field( s, "maximized" ) == "0" && u >= 1 && cap.value( 3 ).toInt() == 18 * u, "the window has a Windows 98 caption: " + field( s, "caption" ) );
		trace( cap.value( 0 ).toInt() == 4 * u && cap.value( 1 ).toInt() == 4 * u && cap.value( 2 ).toInt() == w - 8 * u, "the caption sits inside the 4 px sizing border" );
		trace( ic.value( 2 ).toInt() == 16 * u && ic.value( 3 ).toInt() == 16 * u && field( s, "title" ).endsWith( "_-_Ingnomia" ) && field( s, "title" ).size() > 11,
			"the caption shows the small icon and names the open kingdom, then the game (PDF p.93): " + field( s, "title" ) );
		trace( mn.value( 2 ).toInt() == 16 * u && mn.value( 3 ).toInt() == 14 * u && mx.value( 0 ).toInt() == mn.value( 0 ).toInt() + 16 * u
				&& cl.value( 0 ).toInt() == mx.value( 0 ).toInt() + 18 * u && cl.value( 0 ).toInt() + 16 * u == w - 6 * u && mn.value( 1 ).toInt() == 6 * u,
			"Minimize and Maximize sit together and Close stands 2 px apart: " + field( s, "min" ) + " " + field( s, "max" ) + " " + field( s, "close" ) );
		trace( field( s, "hud" ) == QString( "%1,%2,%3,%4" ).arg( 4 * u ).arg( 23 * u ).arg( w - 8 * u ).arg( h - 27 * u ), "the game window's contents lie in the client area: " + field( s, "hud" ) );
		trace( field( s, "maxTip" ) == "Maximize", "the Maximize button's ToolTip names it" );
		{
			// Window menu (PDF p.95, p.113): every title bar command, Restore unavailable in a normal window.
			const auto menu = frame( "menu" );
			trace( menu.contains( "menu open restore-r move+m size+s minimize+n maximize+x close+c" ), "Alt+Space opens the window menu: " + menu );
		}
		capture( "game", "window-menu" );
		{
			// Size grip at the far corner of the status bar, reaching the client area's lower right corner (PDF p.100).
			const auto g = field( s, "grip" ).split( sep );
			trace( g.size() == 4 && g[2].toInt() == 12 * u && g[3].toInt() == 12 * u && g[0].toInt() + g[2].toInt() == w - 4 * u && g[1].toInt() + g[3].toInt() == h - 4 * u,
				"the status bar shows a size grip in the window's lower right corner: " + field( s, "grip" ) );
		}
	} );
	at( 23600, [] { frameStep( "menu-key:x" ); } );
	at( 24000, [] {
		const auto s = frame( "state" );
		automationTrace( s );
		const auto ctx = field( s, "context" ).split( 'x' );
		const auto cap = field( s, "caption" ).split( QRegularExpression( "[,x]" ) );
		const int u = cap.value( 3 ).toInt() / 18;
		trace( field( s, "maximized" ) == "1" && field( s, "fillsWork" ) == "1", "Maximize fills the work area and leaves the taskbar uncovered" );
		trace( field( s, "frameMax" ) == "1" && cap.value( 0 ) == "0" && cap.value( 1 ) == "0" && field( s, "hud" ) == QString( "0,%1,%2,%3" ).arg( 19 * u ).arg( ctx.value( 0 ) ).arg( ctx.value( 1 ).toInt() - 19 * u ),
			"a maximized window has no sizing border: " + field( s, "hud" ) );
		trace( field( s, "maxTip" ) == "Restore", "the button becomes Restore" );
		trace( field( s, "grip" ) == "-", "a maximized window hides its size grip" );
		capture( "game", "maximized" );
	} );
	at( 24400, [] {
		const auto menu = frame( "menu" );
		trace( menu.contains( "menu open restore+r move-m size-s minimize+n maximize-x close+c" ), "a maximized window's menu offers Restore, not Move, Size or Maximize: " + menu );
		frameStep( "menu-key:r" );
	} );
	at( 25000, [] {
		const auto s = frame( "state" );
		trace( field( s, "maximized" ) == "0" && field( s, "maxTip" ) == "Maximize", "Restore on the window menu restores the window" );
		frameStep( "dblclick" );
	} );
	at( 25600, [] {
		trace( field( frame( "state" ), "maximized" ) == "1", "double-clicking the caption maximizes the window" );
		frameStep( "dblclick" );
	} );
	at( 26200, [] {
		const auto s = frame( "state" );
		trace( field( s, "maximized" ) == "0" && field( s, "frameMax" ) == "0" && field( s, "maxTip" ) == "Maximize", "double-clicking the caption again restores it" );
		frameStep( "click:frame_minimize" );
	} );
	at( 26900, [] {
		trace( field( frame( "state" ), "minimized" ) == "1", "Minimize minimizes the window" );
		frameStep( "restore" );
	} );
	at( 27600, [] {
		const auto s = frame( "state" );
		trace( field( s, "minimized" ) == "0" && field( s, "visible" ) == "1", "the window comes back with its frame" );
		frameStep( "click:frame_close" );
	} );
	// Close ends the run; this fallback only fires if it did not.
	at( 30000, [] {
		trace( false, "Close did not close the game" );
		if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection );
	} );
}
} // namespace stage17
