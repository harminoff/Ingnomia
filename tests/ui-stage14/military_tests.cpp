/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 14: military as a Windows 98 property sheet: squad, citizen, role, destination, then act.
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include "gui/ui/screens/management6c/Management6CRmlBinding.h"
using namespace ingnomia::ui;
int checks = 0;
void check( bool value, const char* label )
{
	++checks;
	if ( !value )
	{
		std::cerr << "FAIL " << label << std::endl;
		std::exit( 1 );
	}
}
struct Files : Rml::FileInterface
{
	std::filesystem::path root;
	Rml::FileHandle Open( const Rml::String& name ) override
	{
		auto path = std::filesystem::path( name );
		if ( !path.has_root_name() ) path = root / path.relative_path();
		return reinterpret_cast<Rml::FileHandle>( std::fopen( path.string().c_str(), "rb" ) );
	}
	void Close( Rml::FileHandle f ) override { std::fclose( reinterpret_cast<FILE*>( f ) ); }
	size_t Read( void* p, size_t size, Rml::FileHandle f ) override { return std::fread( p, 1, size, reinterpret_cast<FILE*>( f ) ); }
	bool Seek( Rml::FileHandle f, long n, int from ) override { return std::fseek( reinterpret_cast<FILE*>( f ), n, from ) == 0; }
	size_t Tell( Rml::FileHandle f ) override { return static_cast<size_t>( std::ftell( reinterpret_cast<FILE*>( f ) ) ); }
};
struct Renderer : Rml::RenderInterface
{
	Rml::CompiledGeometryHandle CompileGeometry( Rml::Span<const Rml::Vertex>, Rml::Span<const int> ) override { return 1; }
	void RenderGeometry( Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle ) override {}
	void ReleaseGeometry( Rml::CompiledGeometryHandle ) override {}
	Rml::TextureHandle LoadTexture( Rml::Vector2i&, const Rml::String& ) override { return 0; }
	Rml::TextureHandle GenerateTexture( Rml::Span<const Rml::byte>, Rml::Vector2i ) override { return 1; }
	void ReleaseTexture( Rml::TextureHandle ) override {}
	void EnableScissorRegion( bool ) override {}
	void SetScissorRegion( Rml::Rectanglei ) override {}
};
void update( Rml::Context& c )
{
	c.Update();
	if ( connected_tabs::reconcile( c ) ) c.Update();
}
using namespace management6c;
struct Port final : CommandPort
{
	std::vector<UiActionEnvelope> sent;
	CommandResult dispatch( const UiActionEnvelope& a, DispatchOrigin ) override { sent.push_back( a ); return {}; }
	int count( const char* id ) { return static_cast<int>( std::count_if( sent.begin(), sent.end(), [&]( auto& a ) { return a.id.value == id; } ) ); }
};
TargetPriorityRow target( const char* id, MilitaryAttitude a ) { return { CatalogId { id }, id, a }; }

int main( int argc, char** argv )
{
	check( argc == 2, "assets" );
	Files files;
	files.root = argv[1];
	Renderer renderer;
	Rml::SystemInterface system;
	Rml::SetFileInterface( &files );
	Rml::SetRenderInterface( &renderer );
	Rml::SetSystemInterface( &system );
	check( Rml::Initialise(), "initialize" );
	ClassicFocusInstancer focus;
	Rml::Factory::RegisterDecoratorInstancer( "win98-focus", &focus );
	Rml::LoadFontFace( "fonts/LatoLatin-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Bold.ttf" );
	auto* c = Rml::CreateContext( "stage14", { 384, 380 } );
	{
		Port port;
		Management6CRmlBinding binding( *c );
		binding.setWindowSurface( false );
		Management6CController controller( port, binding );
		check( binding.initialize( controller ), "binding loads" );
		bool closed = false;
		binding.setRouteCloseHandler( [&]( RouteId, FocusToken ) { closed = true; } );
		controller.beginWorld( WorldEpoch { 14 } );
		check( binding.openMilitary( View::Squads, FocusToken { 1 } ), "military opens" );
		auto* doc   = binding.militaryDocument();
		auto el     = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click  = [&]( const std::string& id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); update( *c ); };
		auto off    = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner  = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto value  = [&]( const char* id, const std::string& v ) { auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( el( id ) ); e->SetValue( v ); e->DispatchEvent( "change", Rml::Dictionary {} ); update( *c ); };
		auto choose = [&]( const char* id ) { click( id ); };
		auto dialog = [&]( const char* id ) -> Rml::Element* {
			for ( int i = 0; i < c->GetNumDocuments(); ++i )
				if ( auto* e = c->GetDocument( i )->GetElementById( id ); e && c->GetDocument( i ) != doc && c->GetDocument( i )->IsVisible() ) return e;
			return nullptr;
		};
		auto answer = [&]( const char* id ) { auto* e = dialog( id ); check( e != nullptr, id ); e->Click(); update( *c ); };
		auto options = [&]( const char* id ) { std::string out; auto* e = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( el( id ) ); for ( int i = 0; e && i < e->GetNumOptions(); ++i ) out += e->GetOption( i )->GetAttribute<Rml::String>( "value", "" ) + ","; return out; };

		MilitaryRoster roster;
		roster.squads   = { SquadRow { SquadId { 10 }, "Alpha", false, true, { target( "Goblin", MilitaryAttitude::Flee ), target( "Wolf", MilitaryAttitude::Attack ), target( "Bear", MilitaryAttitude::Defend ) }, { SquadMemberRow { CreatureId { 501 }, "Ada", MilitaryRoleId { 71 } } } },
			SquadRow { SquadId { 20 }, "Bravo", true, false, {}, { SquadMemberRow { CreatureId { 502 }, "Bert", std::nullopt } } } };
		roster.unassigned = { SquadMemberRow { CreatureId { 503 }, "Cara", std::nullopt } };
		check( controller.applyMilitary( { WorldEpoch { 14 }, Revision { 1 }, roster } ), "roster" );
		std::vector<MilitaryRoleRow> roles {
			MilitaryRoleRow { MilitaryRoleId { 71 }, "Soldier", false, { UniformSlotRow { UniformSlot::ChestArmor, "Chest", CatalogId { "Armor" }, CatalogId { "Iron" }, { CatalogId { "Armor" }, CatalogId { "Cloth" } }, { CatalogId { "Iron" }, CatalogId { "Copper" } } } } },
			MilitaryRoleRow { MilitaryRoleId { 72 }, "Scout", true, {} } };
		check( controller.applyRoles( { WorldEpoch { 14 }, Revision { 1 }, roles } ), "roles" );
		update( *c );

		// ---------------------------------------------------------------- Sheet
		check( el( "military_tabs" )->IsClassSet( "c-connected-tabs" ) && el( "military_workbench" )->IsClassSet( "w98-sheet" ), "connected tabs on a property sheet" );
		check( !el( "military_tab_neighbors" )->IsVisible( true ) && !el( "military_tab_missions" )->IsVisible( true ), "diplomacy pages are not military tabs" );
		check( doc->GetElementById( "military_views_toggle" ) == nullptr && doc->GetElementById( "military_rail" ) == nullptr, "no side rail" );
		check( el( "military_close_button" )->IsVisible( true ), "Close is the commit button" );
		{
			auto* caption = el( "military_workbench" )->GetFirstChild();
			check( caption->IsClassSet( "w98-caption" ) && caption->IsVisible( true ) && el( "military_tabs" )->GetAbsoluteTop() >= caption->GetAbsoluteTop() + caption->GetOffsetHeight(), "caption stays visible above the tabs" );
		}

		// ---------------------------------------------------------------- Squads
		check( inner( "military_squad_rows" ).find( "1. Alpha" ) != std::string::npos && inner( "military_squad_rows" ).find( "2. Bravo" ) != std::string::npos, "squads are numbered in order" );
		check( controller.state().selectedSquad == SquadId { 10 } && off( "squad_move_up" ) && !off( "squad_move_down" ), "first squad cannot move up" );
		auto n = port.sent.size();
		click( "military_squad_20" );
		check( port.sent.size() == n && controller.state().selectedSquad == SquadId { 20 } && !off( "squad_move_up" ) && off( "squad_move_down" ), "selecting a squad changes nothing" );
		click( "squad_move_up" );
		check( port.sent.back().id.value == "military.move_squad" && std::get<MoveSquadPayload>( port.sent.back().payload ).squad == SquadId { 20 }, "Move Up targets the selected squad" );
		controller.onActionFinished( port.sent.back().request, {} );
		rmlui_dynamic_cast<Rml::ElementFormControl*>( el( "squad_name_input" ) )->SetValue( "Bravo Guard" );
		click( "squad_rename" );
		check( std::get<RenameSquadPayload>( port.sent.back().payload ).name == "Bravo Guard" && std::get<RenameSquadPayload>( port.sent.back().payload ).squad == SquadId { 20 }, "Rename targets only the selected squad" );
		controller.onActionFinished( port.sent.back().request, {} );
		n = port.sent.size();
		click( "squad_remove" );
		check( dialog( "confirm-title" ) && std::string( dialog( "confirm-title" )->GetInnerRML() ) == "Bravo" && std::string( dialog( "confirm-detail" )->GetInnerRML() ).find( "Deleting Bravo removes the squad" ) != std::string::npos, "Delete asks in a message box named after the squad" );
		answer( "confirm-cancel" );
		check( port.count( "military.remove_squad" ) == 0 && !controller.state().destructive, "Cancel keeps the squad" );

		// ---------------------------------------------------------------- Members
		click( "military_tab_members" );
		check( el( "military_members_page" )->IsVisible( true ) && !el( "military_squads_page" )->IsVisible( true ), "Members page" );
		value( "military_member_squad", "10" );
		check( controller.state().selectedSquad == SquadId { 10 } && inner( "military_members_label" ) == "Members of Alpha:", "squad drop-down names the list" );
		n = port.sent.size();
		click( "military_unassigned_503" );
		check( port.sent.size() == n && controller.state().selectedMember == CreatureId { 503 }, "selecting a citizen never transfers" );
		check( !off( "member_assign_squad" ) && off( "member_remove" ), "Add available for a citizen outside the squad" );
		click( "member_assign_squad" );
		check( std::get<AssignSquadPayload>( port.sent.back().payload ).creature == CreatureId { 503 } && std::get<AssignSquadPayload>( port.sent.back().payload ).squad == SquadId { 10 }, "Add assigns the selected citizen to the named squad" );
		controller.onActionFinished( port.sent.back().request, {} );
		click( "military_member_501" );
		check( inner( "role_member_assignment_status" ) == "Ada (Alpha)" && options( "military_member_destination" ) == "20,", "Move to lists only other squads" );
		click( "member_move_to" );
		check( std::get<AssignSquadPayload>( port.sent.back().payload ).creature == CreatureId { 501 } && std::get<AssignSquadPayload>( port.sent.back().payload ).squad == SquadId { 20 }, "Move sends the citizen to the chosen squad" );
		controller.onActionFinished( port.sent.back().request, {} );
		value( "military_member_role", "72" );
		check( port.sent.back().id.value == "military.assign_role" && std::get<AssignRolePayload>( port.sent.back().payload ).creature == CreatureId { 501 } && std::get<AssignRolePayload>( port.sent.back().payload ).role == MilitaryRoleId { 72 }, "Role drop-down assigns the role to that citizen" );
		controller.onActionFinished( port.sent.back().request, {} );
		click( "member_remove" );
		check( port.sent.back().id.value == "military.remove_gnome" && std::get<GnomeTargetPayload>( port.sent.back().payload ).creature == CreatureId { 501 }, "Remove takes the selected citizen out of the squad" );
		controller.onActionFinished( port.sent.back().request, {} );

		// ---------------------------------------------------------------- Roles
		click( "military_tab_roles" );
		click( "military_role_72" );
		check( el( "role_civilian" )->HasAttribute( "checked" ), "Civilian check box shows the role's state" );
		auto n0 = port.sent.size();
		click( "role_civilian" );
		check( port.sent.size() == n0 + 1, "one click sends one change" );
		check( port.sent.back().id.value == "military.set_role_civilian" && !std::get<SetRoleCivilianPayload>( port.sent.back().payload ).civilian && std::get<SetRoleCivilianPayload>( port.sent.back().payload ).role == MilitaryRoleId { 72 }, "Civilian check box changes only that role" );
		controller.onActionFinished( port.sent.back().request, {} );
		click( "military_role_71" );
		check( inner( "role_usage" ) == "1 citizen has this role.", "role usage counts citizens" );
		click( "role_remove" );
		check( std::string( dialog( "confirm-title" )->GetInnerRML() ) == "Soldier", "role delete is named" );
		answer( "confirm-cancel" );

		// ---------------------------------------------------------------- Uniforms
		click( "military_tab_uniforms" );
		check( rmlui_dynamic_cast<Rml::ElementFormControl*>( el( "military_uniform_role" ) )->GetValue() == "71", "uniform role follows the selected role" );
		click( "military_uniform_ChestArmor" );
		check( options( "military_uniform_type" ) == "Armor,Cloth," && options( "military_uniform_material" ) == "Iron,Copper,", "only legal choices are offered" );
		check( inner( "military_uniform_scope" ).find( "every citizen with the Soldier role (1 citizen)" ) != std::string::npos, "uniform scope names the role and its citizens" );
		value( "military_uniform_material", "Copper" );
		{
			const auto p = std::get<SetUniformSlotPayload>( port.sent.back().payload );
			check( p.role == MilitaryRoleId { 71 } && p.slot == UniformSlot::ChestArmor && p.type == CatalogId { "Armor" } && p.material == CatalogId { "Copper" }, "material change targets the role's slot" );
		}
		controller.onActionFinished( port.sent.back().request, {} );

		// ---------------------------------------------------------------- Targets
		click( "military_tab_priorities" );
		value( "military_priority_squad", "10" );
		click( "military_priority_Wolf" );
		check( el( "attitude_attack" )->HasAttribute( "checked" ) && !el( "attitude_flee" )->HasAttribute( "checked" ) && inner( "military_priority_selection" ) == "Response to Wolf", "response shows the selected target's value" );
		n = port.sent.size();
		click( "priority_move_up" );
		check( port.sent.size() == n + 1 && port.sent.back().id.value == "military.move_priority" && std::get<MovePriorityPayload>( port.sent.back().payload ).targetType == CatalogId { "Wolf" }, "Move Up reorders without changing the response" );
		controller.onActionFinished( port.sent.back().request, {} );
		choose( "attitude_hunt" );
		{
			const auto p = std::get<SetAttitudePayload>( port.sent.back().payload );
			check( port.sent.back().id.value == "military.set_attitude" && p.squad == SquadId { 10 } && p.targetType == CatalogId { "Wolf" } && p.attitude == MilitaryAttitude::Hunt, "response option sets the value, not an attack order" );
		}
		controller.onActionFinished( port.sent.back().request, {} );
		click( "military_priority_Bear" );
		check( off( "priority_move_down" ) && !off( "priority_move_up" ), "last target cannot move down" );

		// ---------------------------------------------------------------- Fit
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			for ( const char* tab : { "military_tab_squads", "military_tab_members", "military_tab_roles", "military_tab_uniforms", "military_tab_priorities" } )
			{
				click( tab );
				const char* pages[] = { "military_squads_page", "military_members_page", "military_roles_page", "military_uniforms_page", "military_priority_page" };
				for ( auto* id : pages )
				{
					auto* page = el( id );
					if ( !page->IsVisible( true ) ) continue;
					auto* close = el( "military_close_button" );
					if ( page->GetScrollHeight() > page->GetClientHeight() + 1.f || close->GetAbsoluteTop() + close->GetOffsetHeight() > 380.f * k + 1.f || el( "military_tabs" )->GetOffsetWidth() > 384.f * k )
					{
						std::cerr << "clipped " << id << " scale=" << scale << " page=" << page->GetScrollHeight() << "/" << page->GetClientHeight() << " close=" << close->GetAbsoluteTop() + close->GetOffsetHeight() << std::endl;
						check( false, "page, tabs and Close stay inside the sheet" );
					}
					++checks;
				}
			}
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 384, 380 } );
		click( "military_close_button" );
		check( closed, "Close closes the window" );
		binding.shutdown();
	}
	Rml::RemoveContext( "stage14" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
