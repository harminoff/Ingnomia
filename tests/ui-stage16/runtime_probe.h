/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../src/game/gnome.h"
#include "../../src/game/gnomemanager.h"
#include "../../src/gui/aggregatortileinfo.h"

// Stage 16 live probe: on a copied save, inspects a tile holding two citizens, chooses the second one by its
// list row, walks the creature inspector's pages, changes the profession through the drop-down and restores it,
// then opens a citizen from Population > Properties (POP-05). Enabled with INGNOMIA_AUTOMATE_INSPECTOR_STAGE16_LIVE=1;
// captures land in INGNOMIA_AUTOMATE_INSPECTOR_STAGE16_DIR.
namespace stage16
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().inspectorStage16Probe( action ) ); }
inline void step( const std::string& action ) { const auto r = probe( action ); trace( r.startsWith( "PASS" ), r ); }
inline QString field( const QString& state, const QString& key )
{
	const auto start = state.indexOf( " " + key + "=" );
	if ( start < 0 ) return {};
	const auto from = start + key.size() + 2;
	const auto end  = state.indexOf( ' ', from );
	return state.mid( from, end < 0 ? -1 : end - from );
}
inline void capture( const char* target, const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_STAGE16_DIR" ) + "/" + name + ".png";
	step( std::string( "capture:" ) + target + ":" + path.toStdString() );
}
// The creature entry "<id>:<name>:<profession>:<page>:role=<n>" for one creature inspector.
inline QStringList creature( const QString& state, unsigned id )
{
	for ( const auto& entry : field( state, "creatures" ).split( ';', Qt::SkipEmptyParts ) )
		if ( entry.section( ':', 0, 0 ).toUInt() == id ) return entry.split( ':' );
	return {};
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_STAGE16_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static std::atomic_uint tileId { 0 }, second { 0 };
	static QString profession, chosen;
	// Two citizens on one tile: the tutorial citizens start apart, so the probe moves the second one onto the
	// first one's tile in memory (the save on disk is unchanged).
	at( 8600, [] {
		auto* c = Global::eventConnector;
		QMetaObject::invokeMethod( c, [c] {
			auto* g = c->game();
			const bool ok = g && g->gm() && g->gm()->gnomes().size() >= 2;
			if ( ok )
			{
				auto* first  = g->gm()->gnomes()[0];
				auto* other  = g->gm()->gnomes()[1];
				other->forceMove( first->getPos() );
				tileId = first->getPos().toInt();
				second = other->id();
			}
			trace( ok, QString( "probe placed two citizens on tile %1 (second citizen %2)" ).arg( tileId.load() ).arg( second.load() ) );
		}, Qt::QueuedConnection );
	} );
	at( 9000, [] { trace( MainWindow::getInstance().activateHudElement( "hud_tool_inspect" ), "Inspect tool opened the tile inspector" ); } );
	at( 9600, [] {
		auto* tiles = Global::eventConnector->aggregatorTileInfo();
		QMetaObject::invokeMethod( tiles, [tiles] { tiles->onShowTileInfo( tileId ); }, Qt::QueuedConnection );
	} );
	at( 10600, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		const auto creatures = field( s, "tile" ).section( "creatures=", 1 ).split( ',', Qt::SkipEmptyParts );
		trace( creatures.size() >= 2 && creatures.contains( QString::number( second.load() ) ), "the tile lists each creature as its own row: " + creatures.join( ',' ) );
		capture( "tile", "tile" );
	} );
	at( 11200, [] {
		const auto creatures = field( probe( "state" ), "tile" ).section( "creatures=", 1 ).split( ',', Qt::SkipEmptyParts );
		const auto index = std::max<qsizetype>( 0, creatures.indexOf( QString::number( second.load() ) ) );
		step( "tile|click:live_tile_creature_" + std::to_string( index ) );
	} );
	at( 11500, [] { capture( "tile", "tile-chosen" ); } );
	at( 11900, [] { step( "tile|click:live_tile_open_creature" ); } );
	at( 13400, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		const auto entry = creature( s, second );
		trace( entry.size() >= 4 && entry[3] == "creature_preview_camera_panel", "Inspect opened the chosen citizen on the General page" );
		profession = entry.value( 2 ).replace( '_', ' ' );
		capture( "creature", "general" );
	} );
	at( 13800, [] { step( "creature|click:creature_preview_nav_stats" ); } );
	at( 14200, [] { capture( "creature", "attributes" ); } );
	at( 14600, [] { step( "creature|click:creature_preview_nav_expertise" ); } );
	at( 15000, [] { capture( "creature", "skills" ); } );
	at( 15400, [] {
		// Choose a profession other than the current one, as a user would from the drop-down.
		for ( int index = 0; index < 3; ++index )
		{
			const auto r = probe( "creature|choose-index:creature_preview_profession:" + std::to_string( index ) );
			chosen = r.section( "value=", 1 );
			if ( chosen != profession ) { trace( r.startsWith( "PASS" ), r ); break; }
		}
	} );
	at( 17400, [] {
		const auto entry = creature( probe( "state" ), second );
		trace( entry.value( 2 ).replace( '_', ' ' ) == chosen && chosen != profession, "the game applied the chosen profession at once: " + profession + " -> " + chosen );
		step( "creature|choose:creature_preview_profession:" + profession.toStdString() );
	} );
	at( 19400, [] {
		const auto entry = creature( probe( "state" ), second );
		trace( entry.value( 2 ).replace( '_', ' ' ) == profession, "the original profession is restored: " + profession );
		step( "creature|click:creature_preview_nav_equipment" );
	} );
	at( 19800, [] { capture( "creature", "equipment" ); } );
	at( 20200, [] { step( "creature|click:creature_preview_nav_inventory" ); } );
	at( 20600, [] { capture( "creature", "inventory" ); } );
	at( 21000, [] { step( "creature|click:creature_preview_close" ); } );
	at( 21600, [] {
		trace( creature( probe( "state" ), second ).isEmpty(), "Close closed the creature inspector" );
		trace( MainWindow::getInstance().activateHudElement( "hud_tool_inspect" ), "Inspect tool closed the tile inspector" );
	} );
	// POP-05: Population > Properties opens the one creature inspector.
	at( 22000, [] { trace( MainWindow::getInstance().activateHudElement( "hud_open_population" ), "Population opened from the HUD" ); } );
	// Choose a citizen other than the one inspected above, so the new inspector is Properties' own doing.
	at( 23400, [] { step( "population|click-row:population_rows:0" ); } );
	at( 23600, [] { if ( field( probe( "state" ), "popSelected" ).toUInt() == second ) step( "population|click-row:population_rows:1" ); } );
	at( 23800, [] { step( "population|click:population_open_citizen" ); } );
	at( 25400, [] {
		const auto s = probe( "state" );
		automationTrace( s );
		trace( field( s, "populationDetail" ) == "0", "Population has no citizen detail page of its own" );
		trace( field( s, "popSelected" ).toUInt() != second && !creature( s, field( s, "popSelected" ).toUInt() ).isEmpty(), "Properties opened the creature inspector for citizen " + field( s, "popSelected" ) );
		capture( "creature", "population-properties" );
	} );
	at( 26000, [] { step( "creature|click:creature_preview_close" ); step( "population|click:population_close_button" ); } );
	at( 26600, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage16
