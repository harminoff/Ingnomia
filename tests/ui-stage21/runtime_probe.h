/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 21 live probe for the Windows 98 Inventory window and Item Properties. Enabled with
// INGNOMIA_AUTOMATE_STAGE21_INVENTORY=1 on a loaded save; captures land in INGNOMIA_AUTOMATE_STAGE21_DIR.
namespace stage21
{
inline void trace( const QString& r ) { automationTrace( r ); }
inline QString inv( const char* action ) { return QString::fromStdString( MainWindow::getInstance().inventoryStage21Probe( action ) ); }
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE21_DIR" ) + "/" + name + ".png";
	const bool ok = MainWindow::getInstance().requestManagementCaptureForProbe( "inventory", path );
	automationTrace( QString( ok ? "PASS " : "FAIL " ) + "capture requested " + name );
}
inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE21_INVENTORY" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	at( 9000, [] { trace( QString( MainWindow::getInstance().activateHudElement( "hud_open_inventory" ) ? "PASS " : "FAIL " ) + "Inventory opened from the toolbar" ); } );
	at( 10500, [] { trace( inv( "state" ) ); capture( "list" ); } );
	at( 11200, [] { trace( inv( "find:wood" ) ); } );
	at( 11800, [] { trace( inv( "select-first" ) ); capture( "find" ); } );
	at( 12400, [] { trace( inv( "open" ) ); } );
	at( 13000, [] { trace( inv( "state" ) ); capture( "item-general" ); } );
	at( 13600, [] { trace( inv( "tab:1" ) ); } );
	at( 14100, [] { capture( "item-stockpiles" ); } );
	at( 14600, [] { trace( inv( "tab:2" ) ); } );
	at( 15100, [] { capture( "item-recipes" ); } );
	at( 15600, [] { trace( inv( "tab:3" ) ); } );
	at( 16100, [] { capture( "item-history" ); } );
	at( 16600, [] { trace( inv( "tab:0" ) ); trace( inv( "watch" ) ); } );
	at( 17400, [] { trace( inv( "watch" ) ); trace( inv( "close-detail" ) ); } );
	at( 18000, [] { trace( inv( "state" ) ); capture( "list-after" ); } );
	at( 18800, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage21
