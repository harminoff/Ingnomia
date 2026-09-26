/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../src/game/neighbormanager.h"

// Stage 15 live probe: opens Diplomacy through the HUD on a copied save, walks the Send Mission wizard for the
// first discovered kingdom that allows a mission, and checks the game's mission list afterwards. Enabled with
// INGNOMIA_AUTOMATE_DIPLOMACY_STAGE15_LIVE=1; captures land in INGNOMIA_AUTOMATE_DIPLOMACY_STAGE15_DIR.
namespace stage15
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().diplomacyStage15Probe( action ) ); }
inline void step( const std::string& action ) { automationTrace( probe( action ) ); }
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
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_DIPLOMACY_STAGE15_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "diplomacy", path ), QString( "capture requested %1" ).arg( name ) );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_DIPLOMACY_STAGE15_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static int target = -1, missionsBefore = 0, undiscovered = -1;
	// The tutorial world has discovered no kingdom yet; discover one in memory (the save on disk is unchanged).
	at( 8600, [] {
		auto* c = Global::eventConnector;
		QMetaObject::invokeMethod( c, [c] {
			auto* g = c->game();
			const bool ok = g && g->nm() && !g->nm()->kingdoms().isEmpty();
			if ( ok ) g->nm()->discoverKingdom( g->nm()->kingdoms().first().id );
			trace( ok, "probe discovered the first neighboring kingdom in memory" );
		}, Qt::QueuedConnection );
	} );
	at( 9000, [] { trace( MainWindow::getInstance().activateHudElement( "hud_open_diplomacy" ), "Diplomacy opened from the HUD" ); } );
	at( 10500, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		const auto rows = field( s, "neighbors" ).split( ';', Qt::SkipEmptyParts );
		for ( int i = 0; i < rows.size(); ++i )
		{
			if ( undiscovered < 0 && rows[i].contains( "undiscovered" ) ) undiscovered = i;
			if ( target < 0 && rows[i].contains( '+' ) ) target = i;
		}
		missionsBefore = static_cast<int>( field( s, "missions" ).count( ';' ) );
		trace( target >= 0, QString( "a discovered kingdom allows a mission (row %1)" ).arg( target ) );
		step( "click:diplomacy_tab_neighbors" );
		if ( undiscovered >= 0 ) step( "click-row:diplomacy_neighbor_rows:" + std::to_string( undiscovered ) );
	} );
	at( 10900, [] { capture( "neighbor-undiscovered" ); } );
	at( 11300, [] { if ( target >= 0 ) step( "click-row:diplomacy_neighbor_rows:" + std::to_string( target ) ); } );
	at( 11700, [] { capture( "neighbor" ); } );
	at( 12100, [] { step( "click:mission_wizard_open" ); } );
	at( 12500, [] { const auto s = probe( "state" ); trace( field( s, "wizard" ) == "1", "Send Mission opened the wizard: draft " + field( s, "draft" ) ); capture( "wizard-mission" ); } );
	at( 12900, [] { step( "click:wizard_next" ); } );
	at( 13600, [] { const auto s = probe( "state" ); trace( field( s, "gnomes" ) != "0", "citizens who can go: " + field( s, "gnomes" ) + " draft " + field( s, "draft" ) ); capture( "wizard-citizen" ); } );
	at( 14000, [] { step( "click:wizard_next" ); } );
	at( 14400, [] { capture( "wizard-review" ); } );
	at( 14800, [] { step( "click:mission_start" ); } );
	at( 16000, [] {
		const auto s = probe( "state" );
		trace( field( s, "wizard" ) == "0" && field( s, "view" ) == "4", "Finish closed the wizard and showed Missions" );
		trace( static_cast<int>( field( s, "missions" ).count( ';' ) ) == missionsBefore + 1, "the game reports exactly one new mission: " + field( s, "missions" ) );
		step( "click-row:diplomacy_mission_rows:" + std::to_string( missionsBefore ) );
	} );
	at( 16600, [] { capture( "missions" ); } );
	at( 17000, [] { step( "click:diplomacy_close_button" ); } );
	at( 17600, [] { trace( field( probe( "state" ), "open" ) == "0", "Close closed the Diplomacy window" ); } );
	at( 18000, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage15
