/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 13 live probe: drives the schedule grid on a copied save and compares the game's schedule with the
// stated scope. Enabled with INGNOMIA_AUTOMATE_SCHEDULE_STAGE13_LIVE=1; captures land in
// INGNOMIA_AUTOMATE_SCHEDULE_STAGE13_DIR. Reuses the Stage 12 probe helpers (population hook).
namespace stage13
{
inline QString probe( const std::string& action ) { return stage12::probe( action ); }
inline void step( const std::string& action ) { automationTrace( probe( action ) ); }
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_SCHEDULE_STAGE13_DIR" ) + "/" + name + ".png";
	stage12::trace( MainWindow::getInstance().requestManagementCaptureForProbe( "population", path ), QString( "capture requested %1" ).arg( name ) );
}
inline void chooseActivity( const char* id )
{
	// An option button is chosen by the user checking it (the change event carries the choice).
	stage12::click( id );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SCHEDULE_STAGE13_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static QString hour5, row1, hour9;
	at( 9000, [] { stage12::trace( MainWindow::getInstance().activateHudElement( "hud_open_population" ), "Population opened from the HUD" ); } );
	at( 10400, [] { stage12::click( "population_tab_schedules" ); } );
	at( 11000, [] { hour5 = probe( "schedule-hour:5" ); row1 = probe( "schedule-row:1" ); hour9 = probe( "schedule-hour:9" ); automationTrace( "hour5=" + hour5 + " row1=" + row1 + " hour9=" + hour9 ); capture( "grid" ); } );
	// One cell: row 0, hour 5 -> Sleep. Only that cell changes.
	at( 11600, [] { chooseActivity( "schedule_set_sleep" ); step( "click-cell:0:5" ); step( "schedule-scope" ); } );
	at( 12000, [] { capture( "cell-scope" ); stage12::click( "schedule_apply" ); } );
	at( 13400, [] {
		const auto now = probe( "schedule-hour:5" );
		auto expected = hour5;
		expected[0] = 'S';
		stage12::trace( now == expected, "one cell changed: hour 5 was " + hour5 + " now " + now );
	} );
	// Citizen's day: row heading of citizen 1 -> Eat for 24 hours.
	at( 13800, [] { chooseActivity( "schedule_set_eat" ); step( "click-row:schedule_names:1" ); step( "schedule-scope" ); } );
	at( 14200, [] { capture( "day-scope" ); stage12::click( "schedule_apply" ); } );
	at( 15600, [] { const auto now = probe( "schedule-row:1" ); stage12::trace( now == QString( 24, 'E' ), "citizen's day set to Eat: " + now ); } );
	// Hour for all: column heading 9 -> Train, reviewed in a message box first.
	at( 16000, [] { chooseActivity( "schedule_set_training" ); step( "click-row:schedule_hours:9" ); } );
	at( 16400, [] { stage12::click( "schedule_apply" ); } );
	at( 16900, [] { capture( "hour-review" ); } );
	at( 17300, [] { step( "dialog:accept" ); } );
	at( 18700, [] {
		const auto now = probe( "schedule-hour:9" );
		stage12::trace( !now.isEmpty() && now == QString( now.size(), 'T' ), "hour 9 set to Train for all: was " + hour9 + " now " + now );
	} );
	// Keyboard: End reaches hour 23 and the headings follow the scroll.
	at( 19100, [] { step( "click-cell:2:0" ); step( "key:end" ); step( "schedule-scope" ); } );
	at( 19500, [] { const auto s = probe( "schedule-scope" ); stage12::trace( s.contains( "active=23" ), "End reached hour 23: " + s ); capture( "keyboard-end" ); } );
	at( 20200, [] { stage12::click( "population_close_button" ); } );
	at( 21000, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage13
