/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 12 live probe: opens Population through the HUD on a copied save and drives the property sheet
// through production RmlUi elements. Enabled with INGNOMIA_AUTOMATE_POPULATION_STAGE12_LIVE=1 plus
// INGNOMIA_DATA_FOLDER / INGNOMIA_AUTOMATE_LOAD_PATH; captures land in INGNOMIA_AUTOMATE_POPULATION_STAGE12_DIR.
namespace stage12
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().populationStage12Probe( action ) ); }
inline void step( const std::string& action ) { automationTrace( probe( action ) ); }
inline void click( const char* id ) { trace( MainWindow::getInstance().activateManagementElement( id ), QString( "activated %1" ).arg( id ) ); }
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_POPULATION_STAGE12_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "population", path ), QString( "capture requested %1" ).arg( name ) );
}
inline QString field( const QString& state, const QString& key )
{
	const auto start = state.indexOf( " " + key + "=" );
	if ( start < 0 ) return {};
	const auto from = start + key.size() + 2;
	const auto end  = state.indexOf( ' ', from );
	return state.mid( from, end < 0 ? -1 : end - from );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_POPULATION_STAGE12_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static QString skillBefore;
	at( 9000, [] { trace( MainWindow::getInstance().activateHudElement( "hud_open_population" ), "Population opened from the HUD" ); } );
	// Citizens: sorted list view; clicking the sorted heading reverses the order and keeps the selection.
	at( 10500, [] { step( "click-row:population_rows:1" ); } );
	at( 11000, [] { const auto s = probe( "state" ); automationTrace( s ); capture( "citizens" ); } );
	at( 11300, [] { click( "population_sort_name" ); } );
	at( 11600, [] {
		const auto s = probe( "state" );
		trace( field( s, "desc" ) == "1", "Name heading reversed the order: " + field( s, "order" ) );
		capture( "citizens-descending" );
	} );
	// Skills: toggle one citizen's check box, confirm authoritatively, then restore it.
	at( 12400, [] { click( "population_tab_skills" ); } );
	at( 13000, [] { skillBefore = field( probe( "state" ), "skillState" ); automationTrace( "skills before " + skillBefore ); capture( "skills" ); step( "click-row:skill_citizen_rows:0" ); } );
	at( 14600, [] {
		const auto now = field( probe( "state" ), "skillState" );
		trace( !skillBefore.isEmpty() && now != skillBefore && now.section( ';', 1 ) == skillBefore.section( ';', 1 ), "the game changed only the first citizen's skill: " + now );
		step( "click-row:skill_citizen_rows:0" );
	} );
	at( 16000, [] { trace( field( probe( "state" ), "skillState" ) == skillBefore, "second click restored the skill" ); click( "skill_disable_all" ); } );
	at( 16500, [] { capture( "skills-bulk-review" ); } );
	at( 16900, [] { step( "dialog:cancel" ); } );
	// Professions: New, add a skill, Save; the game reports the new profession with one skill; then Delete.
	at( 17200, [] { click( "population_tab_professions" ); click( "profession_create" ); } );
	at( 18400, [] {
		const auto s = probe( "state" );
		trace( field( s, "profession" ) == "New" && s.contains( "New Profession(0)" ), "New created and selected New Profession: " + field( s, "professions" ) );
		step( "click-row:profession_available_skills:0" );
		click( "profession_skill_add" );
	} );
	at( 18900, [] { capture( "professions-dirty" ); click( "profession_save" ); } );
	at( 20400, [] {
		const auto s = probe( "state" );
		trace( s.contains( "New Profession(1)" ) && field( s, "dirty" ) == "0", "Save stored one skill for New Profession: " + field( s, "professions" ) );
		capture( "professions" );
		click( "profession_delete" );
	} );
	at( 20900, [] { capture( "profession-delete-review" ); } );
	at( 21300, [] { step( "dialog:accept" ); } );
	at( 22200, [] { const auto s = probe( "state" ); trace( !s.contains( "New Profession(" ), "Delete removed New Profession: " + field( s, "professions" ) ); } );
	// Schedules page (grid redesign in Stage 13).
	at( 22600, [] { click( "population_tab_schedules" ); } );
	at( 23200, [] { capture( "schedules" ); } );
	at( 23600, [] { click( "population_close_button" ); } );
	at( 24000, [] { trace( field( probe( "state" ), "open" ) == "0", "Close closed the Population window" ); } );
	at( 24800, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage12
