/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

// Stage 14 live probe: drives the Military sheet on a copied save through production RmlUi elements and
// checks the game's authoritative roster after each command. Enabled with
// INGNOMIA_AUTOMATE_MILITARY_STAGE14_LIVE=1; captures land in INGNOMIA_AUTOMATE_MILITARY_STAGE14_DIR.
namespace stage14
{
inline void trace( bool ok, const QString& message ) { automationTrace( QString( ok ? "PASS " : "FAIL " ) + message ); }
inline QString probe( const std::string& action ) { return QString::fromStdString( MainWindow::getInstance().militaryStage14Probe( action ) ); }
inline void step( const std::string& action ) { automationTrace( probe( action ) ); }
inline QString field( const QString& state, const QString& key )
{
	const auto start = state.indexOf( " " + key + "=" );
	if ( start < 0 ) return {};
	const auto from = start + key.size() + 2;
	const auto end  = state.indexOf( ' ', from );
	return state.mid( from, end < 0 ? -1 : end - from );
}
inline int squadCount() { return static_cast<int>( field( probe( "state" ), "squads" ).count( ';' ) ); }
inline void capture( const char* name )
{
	const auto path = qEnvironmentVariable( "INGNOMIA_AUTOMATE_MILITARY_STAGE14_DIR" ) + "/" + name + ".png";
	trace( MainWindow::getInstance().requestManagementCaptureForProbe( "military", path ), QString( "capture requested %1" ).arg( name ) );
}

inline void schedule( QApplication& app )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_MILITARY_STAGE14_LIVE" ) != "1" ) return;
	const auto at = [&app]( int delay, std::function<void()> fn ) { QTimer::singleShot( delay, Qt::PreciseTimer, &app, std::move( fn ) ); };
	static int squadsBefore = 0;
	static QString citizen;
	at( 9000, [] { trace( MainWindow::getInstance().activateHudElement( "hud_open_military" ), "Military opened from the HUD" ); } );
	at( 10500, [] { automationTrace( probe( "state" ) ); squadsBefore = squadCount(); capture( "squads" ); step( "click:squad_add" ); } );
	at( 11500, [] { step( "click:squad_add" ); } );
	at( 12500, [] {
		trace( squadCount() == squadsBefore + 2, QString( "New created two squads: %1 -> %2" ).arg( squadsBefore ).arg( squadCount() ) );
		step( "click-row:military_squad_rows:" + std::to_string( squadCount() - 2 ) );
		step( "type:squad_name_input:Stage 14 A" );
		step( "click:squad_rename" );
	} );
	at( 13500, [] { const auto s = probe( "state" ); trace( s.contains( "Stage_14_A[" ), "Rename changed only the selected squad: " + field( s, "squads" ) ); capture( "squads-renamed" ); } );
	// Members: add an unassigned citizen to the renamed squad, then move them to the other new squad by name.
	at( 14000, [] { step( "click:military_tab_members" ); } );
	at( 14500, [] { citizen = field( probe( "state" ), "unassigned" ).section( ',', 0, 0 ); step( "click-row:military_unassigned_rows:0" ); } );
	at( 14900, [] { capture( "members-selected" ); step( "click:member_assign_squad" ); } );
	at( 16000, [] { const auto s = probe( "state" ); trace( !citizen.isEmpty() && s.contains( "Stage_14_A[" + citizen + "," ), "Add put " + citizen + " in Stage 14 A: " + field( s, "squads" ) ); } );
	at( 16400, [] { step( "click-row:military_member_rows:0" ); step( "choose-index:military_member_role:0" ); } );
	at( 17400, [] { step( "choose-index:military_member_destination:" + std::to_string( squadCount() - 1 - 1 ) ); } );
	at( 17800, [] { capture( "members" ); step( "click:member_move_to" ); } );
	at( 18900, [] { const auto s = probe( "state" ); trace( !s.contains( "Stage_14_A[" + citizen + "," ), "Move took " + citizen + " out of Stage 14 A: " + field( s, "squads" ) ); } );
	// Roles: a new role made civilian with the check box.
	at( 19300, [] { step( "click:military_tab_roles" ); step( "click:role_add" ); } );
	at( 20400, [] { step( "click-row:military_role_rows:" + std::to_string( field( probe( "state" ), "roles" ).count( ';' ) - 1 ) ); } );
	at( 20800, [] { step( "click:role_civilian" ); } );
	at( 21900, [] { const auto s = probe( "state" ); trace( s.contains( "(civ);" ), "Civilian check box set the new role civilian: " + field( s, "roles" ) ); capture( "roles" ); } );
	at( 22400, [] { step( "click:military_tab_uniforms" ); step( "click-row:military_uniform_rows:0" ); } );
	at( 22900, [] { capture( "uniforms" ); } );
	// Targets: set a response, then move the target; the move keeps the response.
	at( 23400, [] { step( "click:military_tab_priorities" ); step( "click-row:military_priority_rows:1" ); } );
	at( 23900, [] { step( "click:attitude_hunt" ); } );
	at( 25000, [] {
		const auto s = probe( "state" );
		trace( s.contains( "Stage_14_A[]{CrystalSnake=1,Yak=3," ), "Hunt set the response of the second target only: " + field( s, "squads" ) );
		capture( "targets" );
		step( "click:priority_move_up" );
	} );
	at( 26100, [] { const auto s = probe( "state" ); trace( s.contains( "Stage_14_A[]{Yak=3,CrystalSnake=1," ), "Move Up reordered and kept the response: " + field( s, "squads" ) ); } );
	// Delete the renamed squad through the message box.
	at( 26500, [] { step( "click:military_tab_squads" ); step( "click-row:military_squad_rows:" + std::to_string( squadCount() - 2 ) ); step( "click:squad_remove" ); } );
	at( 27000, [] { capture( "squad-delete-review" ); } );
	at( 27400, [] { step( "dialog:accept" ); } );
	at( 28500, [] { const auto s = probe( "state" ); trace( !s.contains( "Stage_14_A[" ) && squadCount() == squadsBefore + 1, "Delete removed Stage 14 A only: " + field( s, "squads" ) ); step( "click:military_close_button" ); } );
	at( 29300, [] { trace( field( probe( "state" ), "open" ) == "0", "Close closed the Military window" ); } );
	at( 30000, [] { if ( Global::eventConnector ) QMetaObject::invokeMethod( Global::eventConnector, "onExit", Qt::QueuedConnection ); } );
}
} // namespace stage14
