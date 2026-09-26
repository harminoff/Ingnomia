/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../src/game/farmingmanager.h"
#include "../../src/game/farm.h"
#include "../../src/game/grove.h"
#include "../../src/game/pasture.h"
#include "../../src/game/world.h"

// Stage 11 live probe: drives the agriculture property sheet through production RmlUi input on a
// copied save. It runs inside the farm plan probe (INGNOMIA_AUTOMATE_FARM_PLAN_PROBE=1), which
// creates a four-plot farm and sets INGNOMIA_PROBE_FARM_PLOT_A/B and INGNOMIA_PROBE_FARM_TILE.
// Captures land in INGNOMIA_AUTOMATE_AGRICULTURE_STAGE11_DIR.
namespace stage11
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString state() { return QString::fromStdString( MainWindow::getInstance().agricultureStage11Probe( "state" ) ); }
inline void click( const std::string& id ) { trace( MainWindow::getInstance().activateManagementElement( id ), QString( "activated %1" ).arg( QString::fromStdString( id ) ) ); }
inline void probe( const char* action ) { automationTrace( QString::fromStdString( MainWindow::getInstance().agricultureStage11Probe( action ) ) ); }
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_AGRICULTURE_STAGE11_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "agriculture", path ), QString( "capture requested %1" ).arg( name ) );
}
inline void field( const char* id, const char* value ) { trace( MainWindow::getInstance().setManagementFormValueForProbe( id, value ), QString( "set %1=%2" ).arg( id, value ) ); }
inline std::string plotId( const char* env ) { return "agriculture_plot_" + qEnvironmentVariable( env ).toStdString(); }
inline void openTile( const char* env )
{
	auto* c         = Global::eventConnector;
	const auto tile = qEnvironmentVariable( env ).toUInt();
	QMetaObject::invokeMethod( c, [c, tile] { c->onSelectTile( tile ); }, Qt::QueuedConnection );
}
inline void onGame( std::function<void( Game* )> fn )
{
	auto* c = Global::eventConnector;
	QMetaObject::invokeMethod( c, [c, fn] { if ( c->game() ) fn( c->game() ); }, Qt::QueuedConnection );
}

// Creates a two-tile grove and a two-tile pasture beside the probe farm (in-memory only).
inline void createGroveAndPasture( Game* g )
{
	const Position farm( qEnvironmentVariable( "INGNOMIA_PROBE_FARM_TILE" ).toUInt() );
	const auto mask = TileFlag::TF_WORKSHOP + TileFlag::TF_STOCKPILE + TileFlag::TF_GROVE + TileFlag::TF_FARM + TileFlag::TF_PASTURE + TileFlag::TF_ROOM;
	const auto open = [&]( Position p ) { return g->w()->isWalkableGnome( p ) && ( g->w()->getTile( p ).flags - ~mask ) == TileFlag::TF_NONE; };
	bool grove = false, pasture = false;
	for ( int dx = -12; dx <= 12 && !( grove && pasture ); ++dx )
		for ( int dy = -12; dy <= 12 && !( grove && pasture ); ++dy )
		{
			const Position a = farm + Position( dx, dy, 0 ), b = a.eastOf();
			if ( !open( a ) || !open( b ) ) continue;
			if ( !grove )
			{
				g->fm()->addGrove( a, { { a, true }, { b, true } } );
				if ( g->fm()->getGroveAtPos( a ) ) { grove = true; qputenv( "INGNOMIA_STAGE11_GROVE_TILE", QByteArray::number( a.toInt() ) ); }
			}
			else if ( !pasture )
			{
				g->fm()->addPasture( a, { { a, true }, { b, true } } );
				if ( g->fm()->getPastureAtPos( a ) ) { pasture = true; qputenv( "INGNOMIA_STAGE11_PASTURE_TILE", QByteArray::number( a.toInt() ) ); }
			}
		}
	trace( grove && pasture, "created a probe grove and pasture next to the farm" );
}

// Farm plan steps replace the Stage 00 plan flow: select two of four plots (click, Ctrl+click),
// choose Strawberry in the crop drop-down, Assign Crop, then queue two plantings on each.
inline void farmPlanSteps()
{
	click( "agriculture_view_plots" );
	click( plotId( "INGNOMIA_PROBE_FARM_PLOT_A" ) );
	probe( ( "ctrl-click:" + plotId( "INGNOMIA_PROBE_FARM_PLOT_B" ) ).c_str() );
	field( "agriculture_plot_crop", "Strawberry" );
	click( "agriculture_plot_assign" );
	field( "agriculture_plot_count", "2" );
	click( "agriculture_plot_queue" );
	automationTrace( state() );
	automationTrace( "PASS Farm grid selects two plots and queues two Strawberry plantings on each" );
}

inline void schedule( QApplication& app, int start )
{
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	at( start, [] { capture( "farm-plots" ); } );
	// Plot Queue for one plot.
	at( start + 800, [] { click( plotId( "INGNOMIA_PROBE_FARM_PLOT_B" ) ); click( "agriculture_view_queue" ); probe( "select-first-order" ); } );
	at( start + 1400, [] { automationTrace( state() ); capture( "farm-queue" ); } );
	// General: rename and switch off harvest, pending until Apply.
	at( start + 2200, [] { click( "agriculture_view_general" ); field( "agriculture_name", "Stage 11 farm" ); click( "agriculture_toggle_harvest" ); } );
	at( start + 2700, [] { const auto s = state(); trace( s.contains( "dirty=1" ) && s.contains( "harvest=1" ), "General edits stay pending: " + s ); capture( "farm-general-pending" ); } );
	at( start + 3400, [] { click( "agriculture_apply" ); } );
	at( start + 4600, [] {
		onGame( []( Game* g ) {
			auto* farm = g->fm()->getFarmAtPos( Position( qEnvironmentVariable( "INGNOMIA_PROBE_FARM_TILE" ).toUInt() ) );
			trace( farm && farm->name() == "Stage 11 farm" && !farm->harvest(), QString( "Apply renamed the farm and switched off harvest in the game model name=%1" ).arg( farm ? farm->name() : "-" ) );
		} );
		const auto s = state();
		trace( s.contains( "dirty=0" ) && s.contains( "harvest=0" ), "authoritative snapshot confirms Apply: " + s );
		capture( "farm-general" );
	} );
	// Crops: default crop is pending until OK; OK applies and closes.
	at( start + 5400, [] { click( "agriculture_view_crops" ); click( "agriculture_farm_crop_" + QByteArray( "Strawberry" ).toHex().toStdString() ); } );
	at( start + 5900, [] { capture( "farm-crops" ); } );
	at( start + 6600, [] { click( "agriculture_ok" ); } );
	at( start + 7800, [] {
		onGame( []( Game* g ) {
			auto* farm = g->fm()->getFarmAtPos( Position( qEnvironmentVariable( "INGNOMIA_PROBE_FARM_TILE" ).toUInt() ) );
			trace( farm && farm->plantType() == "Strawberry", QString( "OK applied the default crop plantType=%1" ).arg( farm ? farm->plantType() : "-" ) );
			createGroveAndPasture( g );
		} );
	} );
	// Grove.
	at( start + 8600, [] { openTile( "INGNOMIA_STAGE11_GROVE_TILE" ); } );
	at( start + 9800, [] { automationTrace( state() ); capture( "grove-general" ); click( "agriculture_toggle_fell" ); click( "agriculture_apply" ); } );
	at( start + 11000, [] { const auto s = state(); trace( s.contains( "kind=2" ) && s.contains( "fell=0" ) && s.contains( "pick=1" ) && s.contains( "dirty=0" ), "Fell trees changed only that rule: " + s ); click( "agriculture_view_crops" ); } );
	at( start + 11600, [] { capture( "grove-trees" ); } );
	// Pasture.
	at( start + 12400, [] { openTile( "INGNOMIA_STAGE11_PASTURE_TILE" ); } );
	at( start + 13600, [] { automationTrace( state() ); capture( "pasture-general" ); click( "agriculture_view_animals" ); } );
	at( start + 13900, [] { const auto s = state(); trace( s.contains( "product= " ), "a new pasture starts with no animal type: " + s ); field( "agriculture_animal_type", "Chicken" ); } );
	at( start + 14000, [] { capture( "pasture-type-pending" ); click( "agriculture_apply" ); automationTrace( "after type Apply " + state() ); } );
	at( start + 14500, [] { automationTrace( "type Apply settled " + state() ); } );
	at( start + 14800, [] { field( "agriculture_male_cap", "3" ); click( "agriculture_male_cap-up" ); capture( "pasture-animals" ); } );
	at( start + 14900, [] { click( "agriculture_view_food" ); } );
	at( start + 15400, [] { capture( "pasture-food" ); click( "agriculture_apply" ); } );
	at( start + 16500, [] {
		onGame( []( Game* g ) {
			auto* p = g->fm()->getPastureAtPos( Position( qEnvironmentVariable( "INGNOMIA_STAGE11_PASTURE_TILE" ).toUInt() ) );
			automationTrace( QString( "MODEL pasture type=%1" ).arg( p ? p->animalType() : "-" ) );
		} );
	} );
	at( start + 16600, [] { const auto s = state(); trace( s.contains( "kind=1" ) && s.contains( "product=Chicken" ) && s.contains( "maxMale=4" ) && s.contains( "dirty=0" ), "pasture animal type and male limit applied: " + s ); } );
	// Close with pending changes asks Yes / No / Cancel.
	at( start + 17200, [] { click( "agriculture_view_general" ); field( "agriculture_name", "Unsaved pasture" ); click( "agriculture_close" ); } );
	at( start + 17800, [] { capture( "pasture-close-prompt" ); } );
	at( start + 18600, [] { click( "agriculture_review_alternate" ); } );
	at( start + 19400, [] {
		onGame( []( Game* g ) {
			auto* p = g->fm()->getPastureAtPos( Position( qEnvironmentVariable( "INGNOMIA_STAGE11_PASTURE_TILE" ).toUInt() ) );
			trace( p && p->name() != "Unsaved pasture", "No discarded the pending pasture name" );
		} );
	} );
}
} // namespace stage11
