/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CRmlBinding.h"
#include "../ManagementTooltip.h"
#include "../../localization/RmlText.h"
#include "../../runtime/ConnectedTabs.h"
#include "../../runtime/SelectOptions.h"
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <initializer_list>
#include <sstream>
#include <utility>

namespace ingnomia::ui::management6c
{
namespace
{
std::string esc( const std::string& value ) { return Rml::StringUtilities::EncodeRml( value ); }

Rml::Element* attributeTarget( Rml::Event& event, const char* attribute )
{
	for( auto* element = event.GetTargetElement(); element && element != event.GetCurrentElement();
		element = element->GetParentNode() )
		if( element->HasAttribute( attribute ) ) return element;
	return nullptr;
}

std::optional<std::uint32_t> unsignedAttribute( Rml::Element* element, const char* attribute )
{
	if( !element ) return std::nullopt;
	const auto value = element->GetAttribute<Rml::String>( attribute, "" );
	std::uint32_t parsed{};
	const auto result = std::from_chars( value.data(), value.data() + value.size(), parsed );
	if( result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0 ) return std::nullopt;
	return parsed;
}

std::optional<std::size_t> sizeAttribute( Rml::Element* element, const char* attribute )
{
	if( !element ) return std::nullopt;
	const auto value = element->GetAttribute<Rml::String>( attribute, "" );
	std::size_t parsed{};
	const auto result = std::from_chars( value.data(), value.data() + value.size(), parsed );
	if( result.ec != std::errc{} || result.ptr != value.data() + value.size() ) return std::nullopt;
	return parsed;
}

template<class Row, class Id, class Projection>
std::optional<std::size_t> selectedIndex( const std::vector<Row>& rows, const std::optional<Id>& selected,
	Projection projection )
{
	if( !selected ) return std::nullopt;
	const auto found = std::ranges::find_if( rows, [&]( const Row& row ){ return projection( row ) == *selected; } );
	if( found == rows.end() ) return std::nullopt;
	return static_cast<std::size_t>( std::distance( rows.begin(), found ) );
}

std::optional<UniformSlot> uniformSlot( std::string_view value )
{
	if( value == "HeadArmor" ) return UniformSlot::HeadArmor;
	if( value == "ChestArmor" ) return UniformSlot::ChestArmor;
	if( value == "ArmArmor" ) return UniformSlot::ArmArmor;
	if( value == "HandArmor" ) return UniformSlot::HandArmor;
	if( value == "LegArmor" ) return UniformSlot::LegArmor;
	if( value == "FootArmor" ) return UniformSlot::FootArmor;
	if( value == "LeftHandHeld" ) return UniformSlot::LeftHandHeld;
	if( value == "RightHandHeld" ) return UniformSlot::RightHandHeld;
	if( value == "Back" ) return UniformSlot::Back;
	return std::nullopt;
}

const char* uniformSlotId( UniformSlot value )
{
	switch( value )
	{
		case UniformSlot::HeadArmor: return "HeadArmor";
		case UniformSlot::ChestArmor: return "ChestArmor";
		case UniformSlot::ArmArmor: return "ArmArmor";
		case UniformSlot::HandArmor: return "HandArmor";
		case UniformSlot::LegArmor: return "LegArmor";
		case UniformSlot::FootArmor: return "FootArmor";
		case UniformSlot::LeftHandHeld: return "LeftHandHeld";
		case UniformSlot::RightHandHeld: return "RightHandHeld";
		case UniformSlot::Back: return "Back";
	}
	return "ChestArmor";
}

std::optional<std::uint32_t> unsignedValue( const std::string& value )
{
    std::uint32_t parsed{};
    const auto result = std::from_chars( value.data(), value.data() + value.size(), parsed );
    if( result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0 ) return std::nullopt;
    return parsed;
}

std::string catalogName( const std::string& value )
{
    std::string result;
    for( const unsigned char c : value )
    {
        if( c == '_' ) result += ' ';
        else
        {
            if( !result.empty() && std::isupper( c ) && std::islower( static_cast<unsigned char>( result.back() ) ) ) result += ' ';
            result += static_cast<char>( c );
        }
    }
    if( !result.empty() ) result.front() = static_cast<char>( std::toupper( static_cast<unsigned char>( result.front() ) ) );
    return result;
}

const SquadMemberRow* memberById( const Management6CState& state, CreatureId id )
{
    for( const auto& squad : state.roster.squads )
        for( const auto& member : squad.members ) if( member.id == id ) return &member;
    for( const auto& member : state.roster.unassigned ) if( member.id == id ) return &member;
    return nullptr;
}

std::string destinationName( const Management6CState& state, const MissionRow& mission )
{
    if( mission.type == MissionType::Explore ) return "Surrounding lands";
    const auto neighbor = std::ranges::find_if( state.neighbors,
        [&]( const NeighborRow& row ){ return row.id == mission.target; } );
    if( neighbor == state.neighbors.end() ) return "Unknown destination";
    return neighbor->discovered && neighbor->name ? *neighbor->name : "Undiscovered neighbor";
}

const char* attitudeName( MilitaryAttitude value )
{
	switch( value )
	{
		case MilitaryAttitude::Flee: return "Flee";
		case MilitaryAttitude::Defend: return "Defend";
		case MilitaryAttitude::Attack: return "Attack";
		case MilitaryAttitude::Hunt: return "Hunt";
	}
	return "Unknown";
}

const char* missionTypeName( MissionType value )
{
	switch( value )
	{
		case MissionType::None: return "None";
		case MissionType::Explore: return "Explore";
		case MissionType::Spy: return "Spy";
		case MissionType::Emissary: return "Emissary";
		case MissionType::Raid: return "Raid";
		case MissionType::Sabotage: return "Sabotage";
	}
	return "Unknown";
}

const char* missionActionName( MissionAction value )
{
	switch( value )
	{
		case MissionAction::None: return "No sub-action";
		case MissionAction::Improve: return "Improve relations";
		case MissionAction::Insult: return "Insult";
		case MissionAction::InviteTrader: return "Invite trader";
		case MissionAction::InviteAmbassador: return "Invite ambassador";
	}
	return "Unknown";
}

const char* missionStepName( MissionStep value )
{
	switch( value )
	{
		case MissionStep::None: return "Not started";
		case MissionStep::LeaveMap: return "Leaving the map";
		case MissionStep::Travel: return "Travelling";
		case MissionStep::Action: return "Performing action";
		case MissionStep::Return: return "Returning";
		case MissionStep::Returned: return "Returned";
	}
	return "Unknown";
}

bool isMilitaryView( View view )
{
	return view == View::Squads || view == View::Roles || view == View::Priorities;
}

bool isDiplomacyView( View view ) { return view == View::Neighbors || view == View::Missions; }

bool moveKey( Rml::Event& event, std::function<void( std::int32_t )> move )
{
	const auto key = static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) );
	if( key == Rml::Input::KI_UP ) move( -1 );
	else if( key == Rml::Input::KI_DOWN ) move( 1 );
	else return false;
	event.StopPropagation();
	return true;
}
} // namespace

Management6CRmlBinding::Management6CRmlBinding( Rml::Context& context ) : context_( context ) {}
Management6CRmlBinding::~Management6CRmlBinding() { shutdown(); }

const char* Management6CRmlBinding::rowSurfaceName( RowSurface value )
{
	switch( value )
	{
		case RowSurface::Squads: return "squads";
		case RowSurface::Roles: return "roles";
		case RowSurface::Members: return "members";
		case RowSurface::Unassigned: return "unassigned";
		case RowSurface::Priorities: return "priorities";
		case RowSurface::UniformSlots: return "uniform-slots";
		case RowSurface::UniformTypes: return "uniform-types";
		case RowSurface::UniformMaterials: return "uniform-materials";
		case RowSurface::Neighbors: return "neighbors";
		case RowSurface::Missions: return "missions";
		case RowSurface::Gnomes: return "gnomes";
		case RowSurface::MemberRoles: return "member-roles";
		case RowSurface::Count: break;
	}
	return "invalid";
}

std::optional<Management6CRmlBinding::RowSurface> Management6CRmlBinding::rowSurface( std::string_view value )
{
	for( std::size_t index = 0; index < static_cast<std::size_t>( RowSurface::Count ); ++index )
	{
		const auto surface = static_cast<RowSurface>( index );
		if( value == rowSurfaceName( surface ) ) return surface;
	}
	return std::nullopt;
}

DomWindow Management6CRmlBinding::windowFor( RowSurface surface, std::size_t count,
	std::optional<std::size_t> selected, std::string selectedKey )
{
	auto& state = windows_[static_cast<std::size_t>( surface )];
	if( state.selectedKey != selectedKey )
	{
		state.selectedKey = std::move( selectedKey );
		state.manualPage = false;
	}
	const auto result = boundedDomWindow( count, selected, state.begin, state.manualPage );
	state.begin = result.begin;
	return result;
}

bool Management6CRmlBinding::handleWindowPage( Rml::Event& event )
{
	auto* element = attributeTarget( event, "data-window" );
	if( !element ) return false;
	const auto surface = rowSurface( element->GetAttribute<Rml::String>( "data-window", "" ) );
	const auto begin = sizeAttribute( element, "data-window-start" );
	if( !surface || !begin ) return false;
	auto& state = windows_[static_cast<std::size_t>( *surface )];
	state.begin = *begin;
	state.manualPage = true;
	if( controller_ ) stateChanged( controller_->state() );
	event.StopPropagation();
	return true;
}

std::string Management6CRmlBinding::tr( const char* key ) const
{
    return textCatalog_.format( LocalizationKey{ key } );
}

void Management6CRmlBinding::showDetails( bool show )
{
    detailOpen_ = show;
    if( !controller_ ) return;
    stateChanged( controller_->state() );
    if( !show ) focusCurrentRow();
}

std::string Management6CRmlBinding::choices( RowSurface surface,
    const std::vector<std::pair<std::string, std::string>>& values, const std::string& current, const char* id )
{
    const auto found = std::ranges::find_if( values, [&]( const auto& value ){ return value.first == current; } );
    const auto selected = found == values.end() ? std::nullopt
        : std::optional{ static_cast<std::size_t>( std::distance( values.begin(), found ) ) };
    const auto window = windowFor( surface, values.size(), selected, current );
    std::string result = "<div class='m6c-choice-control'><select id='" + std::string{ id } + "'" + ( values.empty() ? " disabled" : "" ) + ">";
    if( !selected || *selected < window.begin || *selected >= window.end )
        result += "<option value='' selected>" + esc( found != values.end() ? found->second : current.empty() ? tr( "management.flow.none" ) : catalogName( current ) ) + "</option>";
    for( std::size_t i = window.begin; i < window.end; ++i )
        result += "<option value='" + esc( values[i].first ) + "'" + ( values[i].first == current ? " selected" : "" ) + ">" + esc( values[i].second ) + "</option>";
    result += "</select><span class='m6c-select-caret'>v</span></div>";
    appendWindowControls( result, surface, window, values.size() );
    return result;
}

void Management6CRmlBinding::appendWindowControls( std::string& output, RowSurface surface,
	const DomWindow& window, std::size_t count )
{
	if( window.hasPrevious() )
	{
		const auto previous = window.begin > MaximumDynamicRowsPerList
			? window.begin - MaximumDynamicRowsPerList : 0;
		output += "<button id='m6c_page_" + std::string{ rowSurfaceName( surface ) }
			+ "_previous' class='m6c-page-row' data-window='" + std::string{ rowSurfaceName( surface ) }
			+ "' data-window-start='" + std::to_string( previous ) + "'>Previous rows</button>";
	}
	if( window.hasNext( count ) )
		output += "<button id='m6c_page_" + std::string{ rowSurfaceName( surface ) }
			+ "_next' class='m6c-page-row' data-window='" + std::string{ rowSurfaceName( surface ) }
			+ "' data-window-start='" + std::to_string( window.end ) + "'>Next rows</button>";
}

bool Management6CRmlBinding::initialize( Management6CController& controller )
{
	shutdown();
	controller_ = &controller;
	const auto load = [this]( const char* path )
		{ return documentLoader_ ? documentLoader_( path ) : context_.LoadDocument( path ); };
	military_ = load( "windows/military_manager.rml" );
	diplomacy_ = load( "windows/diplomacy_missions.rml" );
	if( !military_ || !diplomacy_ ) { shutdown(); return false; }
	localization::applyRmlText( *military_, textCatalog_ );
	localization::applyRmlText( *diplomacy_, textCatalog_ );
	if ( secondarySurface_ )
	{
		// Native windows own their own tabs; unrelated workbenches open from
		// the HUD and must not replace this window's document.
		for ( const char* id : { "military_tab_neighbors", "military_tab_missions" } ) visible( military_, id, false );
		for ( const char* id : { "diplomacy_tab_squads", "diplomacy_tab_roles", "diplomacy_tab_priorities" } ) visible( diplomacy_, id, false );
	}
	military_->Hide();
	diplomacy_->Hide();

	// ---- Military property sheet: five pages over the controller's three views; commands act at once.
	dialog_.setDocumentPath( "modals/win98_message_box.rml" );
	bind( military_, "military_close", [this]{ closeRoute(); } );
	bind( military_, "military_close_button", [this]{ closeRoute(); } );
	const auto page = [this]( View view, MilitaryPage value ) { return [this, view, value] { militaryPage_ = value; controller_->open( view ); stateChanged( controller_->state() ); }; };
	bind( military_, "military_tab_squads", page( View::Squads, MilitaryPage::Squads ) );
	bind( military_, "military_tab_members", page( View::Squads, MilitaryPage::Members ) );
	bind( military_, "military_tab_roles", page( View::Roles, MilitaryPage::Roles ) );
	bind( military_, "military_tab_uniforms", page( View::Roles, MilitaryPage::Uniforms ) );
	bind( military_, "military_tab_priorities", page( View::Priorities, MilitaryPage::Targets ) );
	bind( military_, "military_tab_neighbors", [this]{ controller_->open( View::Neighbors ); } );
	bind( military_, "military_tab_missions", [this]{ controller_->open( View::Missions ); } );
	bind( diplomacy_, "diplomacy_views_toggle", [this]{
		if( auto* shell = diplomacy_->GetElementById( "diplomacy_shell" ) )
		{
			const bool open = !shell->IsClassSet( "is-rail-open" );
			shell->SetClass( "is-rail-open", open );
			if( auto* toggle = diplomacy_->GetElementById( "diplomacy_views_toggle" ) ) toggle->SetAttribute( "aria-expanded", open ? "true" : "false" );
		}
	} );
	bind( military_, "military_error_retry", [this]{ controller_->refresh(); } );
	bindEvent( military_, "military_workbench", "keydown", [this]( Rml::Event& event ) {
		const auto key  = event.GetParameter<int>( "key_identifier", 0 );
		const bool ctrl = event.GetParameter<int>( "ctrl_key", 0 ) != 0, shift = event.GetParameter<int>( "shift_key", 0 ) != 0;
		if( key == Rml::Input::KI_F5 ) controller_->refresh();
		else if( key == Rml::Input::KI_TAB && ctrl )
		{
			// Ctrl+Tab / Ctrl+Shift+Tab switch pages (PDF p.147).
			const std::array pages { MilitaryPage::Squads, MilitaryPage::Members, MilitaryPage::Roles, MilitaryPage::Uniforms, MilitaryPage::Targets };
			const auto at = std::ranges::find( pages, militaryPage_ ) - pages.begin();
			militaryPage_ = pages[static_cast<std::size_t>( ( at + ( shift ? 4 : 1 ) ) % 5 )];
			controller_->open( militaryPage_ == MilitaryPage::Targets ? View::Priorities : militaryPage_ == MilitaryPage::Roles || militaryPage_ == MilitaryPage::Uniforms ? View::Roles : View::Squads );
			stateChanged( controller_->state() );
		}
		else return;
		event.StopPropagation();
	} );
	bindEvent( military_, "military_squad_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-squad" ), "data-squad" ) ) controller_->selectSquad( SquadId{ *id } );
	} );
	bindEvent( military_, "military_squad_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( military_, "military_role_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-role" ), "data-role" ) ) controller_->selectRole( MilitaryRoleId{ *id } );
	} );
	bindEvent( military_, "military_role_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	for( const char* list : { "military_member_rows", "military_unassigned_rows" } )
	{
		bindEvent( military_, list, "click", [this]( Rml::Event& event ) {
			if( const auto id = unsignedAttribute( attributeTarget( event, "data-creature" ), "data-creature" ) ) controller_->selectMember( CreatureId{ *id } );
		} );
		bindEvent( military_, list, "keydown", [this]( Rml::Event& event ) {
			if( moveKey( event, [this]( std::int32_t value ){ controller_->moveMemberSelection( value ); } ) ) focusSelectedMember();
		} );
	}
	bindEvent( military_, "military_priority_rows", "click", [this]( Rml::Event& event ) {
		if( auto* element = attributeTarget( event, "data-priority" ) )
			controller_->selectPriority( CatalogId{ element->GetAttribute<Rml::String>( "data-priority", "" ) } );
	} );
	bindEvent( military_, "military_priority_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->movePrioritySelection( value ); } ) ) focusSelectedPriority();
	} );
	bindEvent( military_, "military_uniform_rows", "click", [this]( Rml::Event& event ) {
		if( auto* element = attributeTarget( event, "data-uniform-slot" ) )
			if( const auto slot = uniformSlot( element->GetAttribute<Rml::String>( "data-uniform-slot", "" ) ) ) controller_->selectUniformSlot( *slot );
	} );
	bindEvent( military_, "military_uniform_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveUniformSelection( value ); } ) ) focusSelectedUniform();
	} );
	// Drop-down lists: a user choice (change event) selects a squad/role or edits the named record at once.
	const auto choice = [this]( const char* id, std::function<void( const std::string& )> apply ) {
		bindEvent( military_, id, "change", [this, apply]( Rml::Event& event ) {
			if( rendering_ ) return;
			auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() );
			const auto value = control ? control->GetValue() : Rml::String();
			if( !value.empty() ) apply( value );
		} );
	};
	const auto squadChoice = [this]( const std::string& v ) { if( const auto id = unsignedValue( v ) ) if( controller_->state().selectedSquad != SquadId{ *id } ) controller_->selectSquad( SquadId{ *id } ); };
	choice( "military_member_squad", squadChoice );
	choice( "military_priority_squad", squadChoice );
	choice( "military_uniform_role", [this]( const std::string& v ) { if( const auto id = unsignedValue( v ) ) if( controller_->state().selectedRole != MilitaryRoleId{ *id } ) controller_->selectRole( MilitaryRoleId{ *id } ); } );
	choice( "military_member_role", [this]( const std::string& v ) {
		const auto& s = controller_->state();
		const auto* member = s.selectedMember ? memberById( s, *s.selectedMember ) : nullptr;
		if( const auto id = unsignedValue( v ); id && member && member->role != MilitaryRoleId{ *id } ) controller_->assignMemberRole( member->id, MilitaryRoleId{ *id } );
	} );
	const auto selectedSlot = [this]() -> const UniformSlotRow* {
		const auto& s = controller_->state();
		const auto role = std::ranges::find_if( s.roles, [&]( const auto& row ){ return s.selectedRole == row.id; } );
		if( role == s.roles.end() ) return nullptr;
		const auto slot = std::ranges::find_if( role->uniform, [&]( const auto& row ){ return s.selectedUniformSlot == row.slot; } );
		return slot == role->uniform.end() ? nullptr : &*slot;
	};
	choice( "military_uniform_type", [this, selectedSlot]( const std::string& v ) { if( auto* slot = selectedSlot(); slot && slot->type.value != v ) controller_->setSelectedUniform( CatalogId{ v }, CatalogId{ "any" } ); } );
	choice( "military_uniform_material", [this, selectedSlot]( const std::string& v ) { if( auto* slot = selectedSlot(); slot && ( !slot->material || slot->material->value != v ) ) controller_->setSelectedUniform( slot->type, CatalogId{ v } ); } );
	bind( military_, "squad_add", [this]{ controller_->addSquad(); } );
	bind( military_, "squad_move_up", [this]{ controller_->moveSelectedSquad( MoveDirection::Up ); } );
	bind( military_, "squad_move_down", [this]{ controller_->moveSelectedSquad( MoveDirection::Down ); } );
	bind( military_, "squad_remove", [this]{ controller_->requestRemoveSelectedSquad(); } );
	bind( military_, "squad_rename", [this]{ if( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( military_->GetElementById( "squad_name_input" ) ) ) controller_->renameSelectedSquad( e->GetValue() ); } );
	bind( military_, "member_remove", [this]{ controller_->removeSelectedMember(); } );
	bind( military_, "member_assign_squad", [this]{ controller_->assignSelectedMemberToSelectedSquad(); } );
	bind( military_, "member_move_to", [this]{
		auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( military_->GetElementById( "military_member_destination" ) );
		const auto& s = controller_->state();
		if( const auto id = e ? unsignedValue( e->GetValue() ) : std::nullopt; id && s.selectedMember ) controller_->assignMemberToSquad( *s.selectedMember, SquadId{ *id } );
	} );
	bind( military_, "role_add", [this]{ controller_->addRole(); } );
	bind( military_, "role_remove", [this]{ controller_->requestRemoveSelectedRole(); } );
	bind( military_, "role_rename", [this]{ if( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( military_->GetElementById( "role_name_input" ) ) ) controller_->renameSelectedRole( e->GetValue() ); } );
	// Civilian is a persistent check box; it changes the selected role at once.
	bindEvent( military_, "role_civilian", "change", [this]( Rml::Event& event ) {
		if( rendering_ ) return;
		controller_->setSelectedRoleCivilian( event.GetCurrentElement()->HasAttribute( "checked" ) );
	} );
	bind( military_, "priority_move_up", [this]{ controller_->moveSelectedPriority( MoveDirection::Up ); } );
	bind( military_, "priority_move_down", [this]{ controller_->moveSelectedPriority( MoveDirection::Down ); } );
	// Response option buttons: one exclusive value for the selected target; never an order to attack.
	for( const auto& [id, attitude] : { std::pair{ "attitude_flee", MilitaryAttitude::Flee }, std::pair{ "attitude_defend", MilitaryAttitude::Defend },
		std::pair{ "attitude_attack", MilitaryAttitude::Attack }, std::pair{ "attitude_hunt", MilitaryAttitude::Hunt } } )
		bindEvent( military_, id, "change", [this, attitude = attitude]( Rml::Event& event ) {
			if( !rendering_ && event.GetCurrentElement()->HasAttribute( "checked" ) ) controller_->setSelectedAttitude( attitude );
		} );

	// ---- Diplomacy property sheet (Neighbors, Missions; Close only) and the Send Mission wizard.
	bind( diplomacy_, "diplomacy_close", [this]{ if( wizardPage_ ) closeWizard(); else closeRoute(); } );
	bind( diplomacy_, "diplomacy_close_button", [this]{ closeRoute(); } );
	bind( diplomacy_, "diplomacy_tab_neighbors", [this]{ controller_->open( View::Neighbors ); } );
	bind( diplomacy_, "diplomacy_tab_missions", [this]{ controller_->open( View::Missions ); } );
	bind( diplomacy_, "diplomacy_tab_squads", [this]{ controller_->open( View::Squads ); } );
	bind( diplomacy_, "diplomacy_tab_roles", [this]{ controller_->open( View::Roles ); } );
	bind( diplomacy_, "diplomacy_tab_priorities", [this]{ controller_->open( View::Priorities ); } );
	bind( diplomacy_, "diplomacy_error_retry", [this]{ controller_->refresh(); } );
	bindEvent( diplomacy_, "diplomacy_workbench", "keydown", [this]( Rml::Event& event ) {
		const auto key  = event.GetParameter<int>( "key_identifier", 0 );
		const bool ctrl = event.GetParameter<int>( "ctrl_key", 0 ) != 0;
		if( wizardPage_ )
		{
			// The wizard is modal: Esc is Cancel, Enter is the default button (Next, or Finish on Review).
			if( key == Rml::Input::KI_ESCAPE ) closeWizard();
			else if( key == Rml::Input::KI_RETURN ) { if( wizardPage_ == 3 ) finishWizard(); else wizardNext(); }
			else return;
		}
		else if( key == Rml::Input::KI_F5 ) controller_->refresh();
		else if( key == Rml::Input::KI_TAB && ctrl ) controller_->open( controller_->state().diplomacyView == View::Missions ? View::Neighbors : View::Missions );
		else return;
		event.StopPropagation();
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-neighbor" ), "data-neighbor" ) ) controller_->selectNeighbor( NeighborId{ *id } );
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( diplomacy_, "diplomacy_mission_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-mission" ), "data-mission" ) ) controller_->selectMission( MissionId{ *id } );
	} );
	bindEvent( diplomacy_, "diplomacy_mission_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( diplomacy_, "diplomacy_gnome_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-gnome" ), "data-gnome" ) ) controller_->selectMissionGnome( CreatureId{ *id } );
	} );
	bindEvent( diplomacy_, "diplomacy_gnome_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveMissionGnomeSelection( value ); } ) ) focusSelectedGnome();
	} );
	// Mission type and task are exclusive values (option buttons); choosing one never starts a mission.
	for( const auto& [id, type] : { std::pair{ "mission_type_spy", MissionType::Spy }, std::pair{ "mission_type_emissary", MissionType::Emissary },
		std::pair{ "mission_type_raid", MissionType::Raid }, std::pair{ "mission_type_sabotage", MissionType::Sabotage } } )
		bindEvent( diplomacy_, id, "change", [this, type = type]( Rml::Event& event ) { if( event.GetCurrentElement()->HasAttribute( "checked" ) ) controller_->setMissionType( type ); } );
	for( const auto& [id, action] : { std::pair{ "mission_action_improve", MissionAction::Improve }, std::pair{ "mission_action_insult", MissionAction::Insult },
		std::pair{ "mission_action_trader", MissionAction::InviteTrader }, std::pair{ "mission_action_ambassador", MissionAction::InviteAmbassador } } )
		bindEvent( diplomacy_, id, "change", [this, action = action]( Rml::Event& event ) { if( event.GetCurrentElement()->HasAttribute( "checked" ) ) controller_->setMissionAction( action ); } );
	bind( diplomacy_, "mission_wizard_open", [this]{ openWizard(); } );
	bind( diplomacy_, "wizard_back", [this]{ if( wizardPage_ > 1 ) { --wizardPage_; stateChanged( controller_->state() ); } } );
	bind( diplomacy_, "wizard_next", [this]{ wizardNext(); } );
	bind( diplomacy_, "wizard_cancel", [this]{ closeWizard(); } );
	bind( diplomacy_, "wizard_close", [this]{ closeWizard(); } );
	bind( diplomacy_, "mission_start", [this]{ finishWizard(); } );

	for( const auto& target : std::initializer_list<std::pair<Rml::ElementDocument*, const char*>>{
		{ military_, "military_squad_rows" }, { military_, "military_role_rows" },
		{ military_, "military_member_rows" }, { military_, "military_unassigned_rows" },
		{ military_, "military_priority_rows" }, { military_, "military_uniform_rows" },
		{ military_, "military_uniform_type_rows" }, { military_, "military_uniform_material_rows" },
		{ military_, "military_member_role_choices" },
		{ diplomacy_, "diplomacy_neighbor_rows" }, { diplomacy_, "diplomacy_mission_rows" },
		{ diplomacy_, "diplomacy_gnome_rows" } } )
		bindEvent( target.first, target.second, "click", [this]( Rml::Event& event ){ (void)handleWindowPage( event ); } );
	auto bindTooltips = [this]( Rml::ElementDocument* document, const char* tooltipId,
		std::initializer_list<const char*> ids )
	{
		for ( const char* id : ids )
		{
			if ( auto* target = document->GetElementById( id ) ) target->SetAttribute( "aria-describedby", tooltipId );
			for ( const char* event : { "mouseover", "focus" } )
				bindEvent( document, id, event, [this, document, tooltipId]( Rml::Event& e ) {
					showManagementTooltip( document, context_, tooltipId, e.GetCurrentElement() );
				} );
			for ( const char* event : { "mouseout", "blur" } )
				bindEvent( document, id, event, [document, tooltipId]( Rml::Event& ) {
					hideManagementTooltip( document, tooltipId );
				} );
		}
	};
	bindTooltips( military_, "military_tooltip", { "military_close" } );
	bindTooltips( diplomacy_, "diplomacy_tooltip", { "diplomacy_views_toggle", "diplomacy_tab_neighbors",
		"diplomacy_tab_missions" } );

	stateChanged( controller.state() );
	return true;
}

bool Management6CRmlBinding::reloadDocuments()
{
	auto* controller = controller_;
	if ( !controller ) return false;
	const auto activeRoute = activeRoute_;
	const auto returnFocus = returnFocus_;
	const auto militaryFocus = militaryFocus_;
	const auto diplomacyFocus = diplomacyFocus_;
	const bool detailOpen = detailOpen_;
	const bool filtersOpen = filtersOpen_;
	const auto windows = windows_;
	shutdown();
	if ( !initialize( *controller ) ) return false;
	activeRoute_ = activeRoute;
	returnFocus_ = returnFocus;
	militaryFocus_ = militaryFocus;
	diplomacyFocus_ = diplomacyFocus;
	detailOpen_ = detailOpen;
	filtersOpen_ = filtersOpen;
	windows_ = windows;
	renderedView_.reset();
	renderedRml_.clear();
	stateChanged( controller->state() );
	return true;
}

void Management6CRmlBinding::shutdown()
{
	dialog_.close( false );
	renderedMilitaryOptions_.clear();
	shownDestructive_ = {};
	for( auto& listener : listeners_ )
		if( listener.target ) listener.target->RemoveEventListener( listener.event, listener.callback.get() );
	listeners_.clear();
	shown_ = nullptr;
	activeRoute_.reset();
	returnFocus_ = {};
	confirmationVisible_ = false;
	rendering_ = false;
	detailOpen_ = false;
	renderedView_.reset();
	renderedWorld_ = {};
	windows_ = {};
	renderedRml_.clear();
	dynamicRenderCount_ = 0;
	if( military_ ) { context_.UnloadDocument( military_ ); military_ = nullptr; }
	if( diplomacy_ ) { context_.UnloadDocument( diplomacy_ ); diplomacy_ = nullptr; }
	controller_ = nullptr;
}

bool Management6CRmlBinding::openMilitary( View view, FocusToken returnFocus )
{
	if( !controller_ || !isMilitaryView( view ) || !returnFocus ) return false;
	activeRoute_ = RouteId{ "workbench.military" };
	returnFocus_ = returnFocus;
	militaryFocus_ = returnFocus;
	controller_->open( view );
	return true;
}

bool Management6CRmlBinding::openDiplomacy( View view, FocusToken returnFocus )
{
	if( !controller_ || !isDiplomacyView( view ) || !returnFocus ) return false;
	activeRoute_ = RouteId{ "workbench.diplomacy" };
	returnFocus_ = returnFocus;
	diplomacyFocus_ = returnFocus;
	controller_->open( view );
	return true;
}

void Management6CRmlBinding::closeMilitary()
{
	if( !controller_ || !controller_->state().militaryOpen ) return;
	controller_->closeMilitary();
	if( routeCloseHandler_ ) routeCloseHandler_( RouteId{ "workbench.military" }, militaryFocus_ );
	militaryFocus_ = {};
}

void Management6CRmlBinding::closeDiplomacy()
{
	if( !controller_ || !controller_->state().diplomacyOpen ) return;
	controller_->closeDiplomacy();
	if( routeCloseHandler_ ) routeCloseHandler_( RouteId{ "workbench.diplomacy" }, diplomacyFocus_ );
	diplomacyFocus_ = {};
}

void Management6CRmlBinding::closeRoute()
{
	if( !controller_ || !controller_->state().open ) return;
	const auto route = activeRoute_;
	const auto returnFocus = returnFocus_;
	controller_->close();
	activeRoute_.reset();
	returnFocus_ = {};
	if( route && routeCloseHandler_ ) routeCloseHandler_( *route, returnFocus );
}

void Management6CRmlBinding::bind( Rml::ElementDocument* document, const char* id, std::function<void()> function )
{
	bindEvent( document, id, "click", [function = std::move( function )]( Rml::Event& event ) {
		event.StopPropagation();
		function();
	} );
}

void Management6CRmlBinding::bindEvent( Rml::ElementDocument* document, const char* id, const char* event,
	std::function<void( Rml::Event& )> function )
{
	if( !document ) return;
	if( auto* element = document->GetElementById( id ) )
	{
		auto callback = std::make_unique<Callback>( [this, document, function = std::move( function )]( Rml::Event& value ) {
            if(rendering_) return;
            controller_->activateViewForInput(document == diplomacy_ ? controller_->state().diplomacyView : controller_->state().militaryView);
            function( value );
		} );
		element->AddEventListener( event, callback.get() );
		listeners_.push_back( { element, event, std::move( callback ) } );
	}
}

void Management6CRmlBinding::syncDocuments( const Management6CState& state )
{
	if ( !presentationEnabled_ )
	{
		military_->Hide();
		diplomacy_->Hide();
		return;
	}
	if( state.militaryOpen ) military_->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto );
	else military_->Hide();
	if( state.diplomacyOpen ) diplomacy_->Show( Rml::ModalFlag::None, Rml::FocusFlag::Auto );
	else diplomacy_->Hide();
	shown_ = isMilitaryView( state.view ) ? military_ : diplomacy_;
}

void Management6CRmlBinding::text( Rml::ElementDocument* document, const char* id, const std::string& value )
{
	if( document ) if( auto* element = document->GetElementById( id ) ) element->SetInnerRML( esc( value ) );
}

void Management6CRmlBinding::rml( Rml::ElementDocument* document, const char* id, const std::string& value )
{
	if( !document ) return;
	auto [cached, inserted] = renderedRml_.try_emplace( id, value );
	if( !inserted && cached->second == value ) return;
	cached->second = value;
	if( auto* element = document->GetElementById( id ) )
	{
		element->SetInnerRML( value );
		++dynamicRenderCount_;
	}
}

void Management6CRmlBinding::visible( Rml::ElementDocument* document, const char* id, bool value )
{
	if( document ) if( auto* element = document->GetElementById( id ) )
	{
		element->SetClass( "is-hidden", !value );
        // Hide explicitly, but let RCSS restore flex/block and responsive display.
        if( value ) element->RemoveProperty( "display" );
        else element->SetProperty( "display", "none" );
	}
}

void Management6CRmlBinding::selected( Rml::ElementDocument* document, const char* id, bool value )
{
	if( document ) if( auto* element = document->GetElementById( id ) )
	{
		element->SetClass( "is-selected", value );
		element->SetAttribute( "aria-selected", value ? "true" : "false" );
	}
}

void Management6CRmlBinding::enabled( Rml::ElementDocument* document, const char* id, bool value )
{
	if( !document ) return;
	if( auto* element = document->GetElementById( id ) )
	{
		if( value ) element->RemoveAttribute( "disabled" );
		else element->SetAttribute( "disabled", "" );
	}
}

void Management6CRmlBinding::inputValue( Rml::ElementDocument* document, const char* id, const std::string& value )
{
	if( document ) if( auto* element = document->GetElementById( id ) )
		if( ( context_.GetFocusElement() != element || std::string_view{ id }.ends_with( "_search" ) ) && element->GetAttribute<Rml::String>( "value", "" ) != value ) element->SetAttribute( "value", value );
}

void Management6CRmlBinding::focusCurrentRow()
{
	if( !controller_ ) return;
	const auto& state = controller_->state();
	std::string id;
	Rml::ElementDocument* document = nullptr;
	if( state.view == View::Squads || state.view == View::Priorities ) { document = military_; if( state.selectedSquad ) id = "military_squad_" + std::to_string( state.selectedSquad->value ); }
	else if( state.view == View::Roles ) { document = military_; if( state.selectedRole ) id = "military_role_" + std::to_string( state.selectedRole->value ); }
	else if( state.view == View::Neighbors ) { document = diplomacy_; if( state.selectedNeighbor ) id = "diplomacy_neighbor_" + std::to_string( state.selectedNeighbor->value ); }
	else { document = diplomacy_; if( state.selectedMission ) id = "diplomacy_mission_" + std::to_string( state.selectedMission->value ); }
	if( document && !id.empty() ) if( auto* element = document->GetElementById( id ) ) element->Focus();
}

void Management6CRmlBinding::focusSelectedMember()
{
	if( military_ && controller_ && controller_->state().selectedMember )
	{
		const auto suffix = std::to_string( controller_->state().selectedMember->value );
		if( auto* element = military_->GetElementById( "military_member_" + suffix ) ) element->Focus();
		else if( auto* unassignedElement = military_->GetElementById( "military_unassigned_" + suffix ) )
			unassignedElement->Focus();
	}
}

void Management6CRmlBinding::focusSelectedPriority()
{
	if( military_ && controller_ && controller_->state().selectedPriority )
		if( auto* element = military_->GetElementById( "military_priority_" + controller_->state().selectedPriority->value ) ) element->Focus();
}

void Management6CRmlBinding::focusSelectedUniform()
{
	if( military_ && controller_ && controller_->state().selectedUniformSlot )
		if( auto* element = military_->GetElementById( "military_uniform_" + std::string{ uniformSlotId( *controller_->state().selectedUniformSlot ) } ) ) element->Focus();
}

void Management6CRmlBinding::focusSelectedGnome()
{
	if( diplomacy_ && controller_ && controller_->state().missionDraft.creature )
		if( auto* element = diplomacy_->GetElementById( "diplomacy_gnome_" + std::to_string( controller_->state().missionDraft.creature->value ) ) ) element->Focus();
}

bool Management6CRmlBinding::activateElement( std::string_view id )
{
	for( auto* document : { military_, diplomacy_ } )
		if( document ) if( auto* element = document->GetElementById( std::string{ id } ) )
		{
			element->DispatchEvent( "click", Rml::Dictionary{} );
			return true;
		}
	return false;
}
bool Management6CRmlBinding::activateFirstDataElement( std::string_view kind )
{
	if ( !controller_ )
		return false;
	if ( kind == "military_squad" )
	{
		const auto rows = controller_->visibleSquads();
		if ( rows.empty() )
			return false;
		return activateElement( "military_squad_" + std::to_string( rows.front().id.value ) );
	}
	if ( kind == "military_role" )
	{
		const auto rows = controller_->visibleRoles();
		if ( rows.empty() )
			return false;
		return activateElement( "military_role_" + std::to_string( rows.front().id.value ) );
	}
	if ( kind == "diplomacy_neighbor" )
	{
		const auto rows = controller_->visibleNeighbors();
		if ( rows.empty() )
			return false;
		return activateElement( "diplomacy_neighbor_" + std::to_string( rows.front().id.value ) );
	}
	if ( kind == "diplomacy_mission" )
	{
		const auto rows = controller_->visibleMissions();
		if ( rows.empty() )
			return false;
		return activateElement( "diplomacy_mission_" + std::to_string( rows.front().id.value ) );
	}
	if ( kind == "diplomacy_gnome" && !controller_->state().availableGnomes.empty() )
		return activateElement( "diplomacy_gnome_" + std::to_string( controller_->state().availableGnomes.front().id.value ) );
	return false;
}

void Management6CRmlBinding::stateChanged( const Management6CState& source )
{
    auto state = source;
    if (secondarySurface_) {
        state.view = *secondarySurface_ ? source.diplomacyView : source.militaryView;
        state.open = *secondarySurface_ ? source.diplomacyOpen : source.militaryOpen;
        state.militaryOpen = !*secondarySurface_ && source.militaryOpen;
        state.diplomacyOpen = *secondarySurface_ && source.diplomacyOpen;
    }
	if( !military_ || !diplomacy_ || !controller_ ) return;
    rendering_ = true;
    if( renderedWorld_ != state.world || renderedView_ != state.view || !state.open ) {
        detailOpen_ = false; filtersOpen_ = false;
		hideManagementTooltip( military_, "military_tooltip" );
		hideManagementTooltip( diplomacy_, "diplomacy_tooltip" );
        if( auto* shell = military_->GetElementById( "military_shell" ) ) shell->SetClass( "is-rail-open", false );
        if( auto* toggle = military_->GetElementById( "military_views_toggle" ) ) toggle->SetAttribute( "aria-expanded", "false" );
        if( auto* shell = diplomacy_->GetElementById( "diplomacy_shell" ) ) shell->SetClass( "is-rail-open", false );
        if( auto* toggle = diplomacy_->GetElementById( "diplomacy_views_toggle" ) ) toggle->SetAttribute( "aria-expanded", "false" );
    }
    renderedWorld_ = state.world;
    renderedView_ = state.view;
    for( auto* document : { military_, diplomacy_ } )
    {
        const bool military = document == military_;
        if( auto* body = document->GetElementById( military ? "military_body" : "diplomacy_body" ) ) { body->SetClass( "m6c-detail-open", detailOpen_ ); body->SetClass( "m6c-filters-open", filtersOpen_ ); }
        if( auto* main = document->GetElementById( military ? "military_main" : "diplomacy_main" ) ) main->SetClass( "m6c-show-detail", detailOpen_ );
    }
	visible( military_, "military_root", state.open && isMilitaryView( state.view ) );
	visible( diplomacy_, "diplomacy_root", state.open && isDiplomacyView( state.view ) );
	renderMilitary( state );
	renderDiplomacy( state );
	syncDocuments( state );
	rendering_ = false;
}

void Management6CRmlBinding::renderMilitary( const Management6CState& state )
{
	// The controller has three views; the sheet shows five pages (Squads/Members over Squads, Roles/Uniforms over Roles).
	if( state.view == View::Priorities ) militaryPage_ = MilitaryPage::Targets;
	else if( state.view == View::Roles && militaryPage_ != MilitaryPage::Uniforms ) militaryPage_ = MilitaryPage::Roles;
	else if( state.view == View::Squads && militaryPage_ != MilitaryPage::Members ) militaryPage_ = MilitaryPage::Squads;
	const auto p = militaryPage_;
	for( const auto& [tab, pane, value] : std::array{
		std::tuple{ "military_tab_squads", "military_squads_page", MilitaryPage::Squads }, std::tuple{ "military_tab_members", "military_members_page", MilitaryPage::Members },
		std::tuple{ "military_tab_roles", "military_roles_page", MilitaryPage::Roles }, std::tuple{ "military_tab_uniforms", "military_uniforms_page", MilitaryPage::Uniforms },
		std::tuple{ "military_tab_priorities", "military_priority_page", MilitaryPage::Targets } } )
	{
		selected( military_, tab, p == value );
		visible( military_, pane, p == value && state.militaryLoad != LoadState::Error );
	}
	if( auto* tabs = military_->GetElementById( "military_tabs" ) )
		connected_tabs::select( *tabs, military_->GetElementById( p == MilitaryPage::Members ? "military_tab_members" : p == MilitaryPage::Roles ? "military_tab_roles"
			: p == MilitaryPage::Uniforms ? "military_tab_uniforms" : p == MilitaryPage::Targets ? "military_tab_priorities" : "military_tab_squads" ) );
	const bool hasRows = !state.roster.squads.empty() || !state.roster.unassigned.empty() || !state.roles.empty();
	visible( military_, "military_loading", ( state.militaryLoad == LoadState::Loading || state.militaryLoad == LoadState::Stale ) && !hasRows );
	visible( military_, "military_error", state.militaryLoad == LoadState::Error );
	const auto roleName = [&]( const std::optional<MilitaryRoleId>& id ) {
		if( !id ) return std::string( "No role" );
		const auto found = std::ranges::find_if( state.roles, [&]( const MilitaryRoleRow& row ){ return row.id == *id; } );
		return found == state.roles.end() ? std::string( "Unknown role" ) : found->name;
	};
	const auto options = [&]( const char* id, const std::vector<std::pair<std::string, std::string>>& values, const std::string& current ) {
		// Drop-down lists are rebuilt only when their values change; a blank entry keeps an empty value honest.
		const bool blank = current.empty() || std::ranges::none_of( values, [&]( const auto& v ){ return v.first == current; } );
		std::string markup = blank ? "|" : "";
		for( const auto& [value, label] : values ) markup += value + "=" + label + "\n";
		auto& previous = renderedMilitaryOptions_[id];
		if( auto* e = military_->GetElementById( id ) )
		{
			if( markup != previous ) { previous = markup; setSelectOptions( e, values, blank ); }
			if( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( e ) ) if( control->GetValue() != current ) control->SetValue( current );
		}
	};
	std::vector<std::pair<std::string, std::string>> squadOptions, roleOptions;
	for( const auto& row : state.roster.squads ) squadOptions.emplace_back( std::to_string( row.id.value ), row.name );
	for( const auto& row : state.roles ) roleOptions.emplace_back( std::to_string( row.id.value ), row.name );
	const auto selectedSquadValue = state.selectedSquad ? std::to_string( state.selectedSquad->value ) : std::string();
	const auto selectedRoleValue  = state.selectedRole ? std::to_string( state.selectedRole->value ) : std::string();

	// ---- Squads page.
	std::string squadRows;
	const auto& squads = state.roster.squads;
	const auto squadWindow = windowFor( RowSurface::Squads, squads.size(), selectedIndex( squads, state.selectedSquad, []( const SquadRow& row ){ return row.id; } ), selectedSquadValue );
	for( std::size_t index = squadWindow.begin; index < squadWindow.end; ++index )
	{
		const auto& row = squads[index];
		squadRows += "<button id='military_squad_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedSquad == row.id ? " is-selected" : "" ) + "' data-squad='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + std::to_string( index + 1 ) + ". " + esc( row.name ) + "</span><span class='w98-cell w98-cell--num w98-w-state'>" + std::to_string( row.members.size() ) + ( row.members.size() == 1 ? " member" : " members" ) + "</span></button>";
	}
	appendWindowControls( squadRows, RowSurface::Squads, squadWindow, squads.size() );
	rml( military_, "military_squad_rows", squadRows.empty() ? std::string( "<div class='w98-list-empty'>There are no squads. Click New to add one.</div>" ) : squadRows );
	const auto squad = state.selectedSquad ? std::ranges::find_if( squads, [&]( const SquadRow& row ){ return row.id == *state.selectedSquad; } ) : squads.end();
	const bool hasSquad = squad != squads.end();
	inputValue( military_, "squad_name_input", hasSquad ? squad->name : "" );
	enabled( military_, "squad_name_input", hasSquad );
	enabled( military_, "squad_rename", hasSquad );
	enabled( military_, "squad_remove", hasSquad );
	enabled( military_, "squad_move_up", hasSquad && squad->canMoveUp );
	enabled( military_, "squad_move_down", hasSquad && squad->canMoveDown );

	// ---- Members page: two lists with the transfer commands between them, then the selected citizen.
	options( "military_member_squad", squadOptions, selectedSquadValue );
	options( "military_priority_squad", squadOptions, selectedSquadValue );
	text( military_, "military_members_label", hasSquad ? "Members of " + squad->name + ":" : std::string( "Members:" ) );
	std::string members;
	const std::vector<SquadMemberRow> noMembers;
	const auto& memberRows = hasSquad ? squad->members : noMembers;
	const auto memberWindow = windowFor( RowSurface::Members, memberRows.size(), selectedIndex( memberRows, state.selectedMember, []( const SquadMemberRow& row ){ return row.id; } ),
		state.selectedMember ? std::to_string( state.selectedMember->value ) : std::string{} );
	for( std::size_t index = memberWindow.begin; index < memberWindow.end; ++index )
	{
		const auto& row = memberRows[index];
		members += "<button id='military_member_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedMember == row.id ? " is-selected" : "" ) + "' data-creature='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + esc( row.name ) + " (" + esc( roleName( row.role ) ) + ")</span></button>";
	}
	appendWindowControls( members, RowSurface::Members, memberWindow, memberRows.size() );
	rml( military_, "military_member_rows", members.empty() ? std::string( "<div class='w98-list-empty'>" ) + ( hasSquad ? "No members." : "Choose a squad." ) + "</div>" : members );
	std::string unassigned;
	const auto unassignedWindow = windowFor( RowSurface::Unassigned, state.roster.unassigned.size(), selectedIndex( state.roster.unassigned, state.selectedMember, []( const SquadMemberRow& row ){ return row.id; } ),
		state.selectedMember ? std::to_string( state.selectedMember->value ) : std::string{} );
	for( std::size_t index = unassignedWindow.begin; index < unassignedWindow.end; ++index )
	{
		const auto& row = state.roster.unassigned[index];
		unassigned += "<button id='military_unassigned_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedMember == row.id ? " is-selected" : "" ) + "' data-creature='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + esc( row.name ) + "</span></button>";
	}
	appendWindowControls( unassigned, RowSurface::Unassigned, unassignedWindow, state.roster.unassigned.size() );
	rml( military_, "military_unassigned_rows", unassigned.empty() ? std::string( "<div class='w98-list-empty'>Everyone is in a squad.</div>" ) : unassigned );
	const auto* member = state.selectedMember ? memberById( state, *state.selectedMember ) : nullptr;
	const auto memberSquad = member ? std::ranges::find_if( squads, [&]( const SquadRow& row ){ return std::ranges::any_of( row.members, [&]( const auto& m ){ return m.id == member->id; } ); } ) : squads.end();
	const bool inSelectedSquad = member && hasSquad && memberSquad == squad;
	enabled( military_, "member_assign_squad", hasSquad && member && !inSelectedSquad );
	enabled( military_, "member_remove", member && memberSquad != squads.end() );
	text( military_, "role_member_assignment_status", member ? member->name + ( memberSquad != squads.end() ? " (" + memberSquad->name + ")" : std::string( " (not in a squad)" ) ) : std::string( "Selected citizen" ) );
	options( "military_member_role", member ? roleOptions : std::vector<std::pair<std::string, std::string>>{}, member && member->role ? std::to_string( member->role->value ) : std::string() );
	std::vector<std::pair<std::string, std::string>> destinations;
	if( member ) for( const auto& row : squads ) if( memberSquad == squads.end() || row.id != memberSquad->id ) destinations.emplace_back( std::to_string( row.id.value ), row.name );
	const auto previousDestination = [&] { auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( military_->GetElementById( "military_member_destination" ) ); return e ? std::string( e->GetValue() ) : std::string(); }();
	options( "military_member_destination", destinations, std::ranges::any_of( destinations, [&]( const auto& d ){ return d.first == previousDestination; } ) ? previousDestination : destinations.empty() ? std::string() : destinations.front().first );
	enabled( military_, "military_member_role", member != nullptr && !roleOptions.empty() );
	enabled( military_, "military_member_destination", !destinations.empty() );
	enabled( military_, "member_move_to", !destinations.empty() );

	// ---- Roles page.
	std::string roleRows;
	const auto roleWindow = windowFor( RowSurface::Roles, state.roles.size(), selectedIndex( state.roles, state.selectedRole, []( const MilitaryRoleRow& row ){ return row.id; } ), selectedRoleValue );
	for( std::size_t index = roleWindow.begin; index < roleWindow.end; ++index )
	{
		const auto& row = state.roles[index];
		roleRows += "<button id='military_role_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedRole == row.id ? " is-selected" : "" ) + "' data-role='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + esc( row.name ) + "</span><span class='w98-cell w98-w-state'>" + ( row.civilian ? "Civilian" : "Combat" ) + "</span></button>";
	}
	appendWindowControls( roleRows, RowSurface::Roles, roleWindow, state.roles.size() );
	rml( military_, "military_role_rows", roleRows.empty() ? std::string( "<div class='w98-list-empty'>There are no roles. Click New to add one.</div>" ) : roleRows );
	const auto role = state.selectedRole ? std::ranges::find_if( state.roles, [&]( const MilitaryRoleRow& row ){ return row.id == *state.selectedRole; } ) : state.roles.end();
	const bool hasRole = role != state.roles.end();
	inputValue( military_, "role_name_input", hasRole ? role->name : "" );
	enabled( military_, "role_name_input", hasRole );
	enabled( military_, "role_rename", hasRole );
	enabled( military_, "role_remove", hasRole );
	enabled( military_, "role_civilian", hasRole );
	if( auto* box = military_->GetElementById( "role_civilian" ) )
	{
		const bool on = hasRole && role->civilian;
		if( box->HasAttribute( "checked" ) != on ) { if( on ) box->SetAttribute( "checked", "checked" ); else box->RemoveAttribute( "checked" ); }
	}
	std::size_t users = 0;
	if( hasRole )
	{
		for( const auto& row : squads ) users += static_cast<std::size_t>( std::ranges::count_if( row.members, [&]( const auto& m ){ return m.role == role->id; } ) );
		users += static_cast<std::size_t>( std::ranges::count_if( state.roster.unassigned, [&]( const auto& m ){ return m.role == role->id; } ) );
	}
	text( military_, "role_usage", hasRole ? std::to_string( users ) + ( users == 1 ? " citizen has" : " citizens have" ) + " this role." : std::string() );

	// ---- Uniforms page: one list view and one editor for the selected slot; the scope is every user of the role.
	options( "military_uniform_role", roleOptions, selectedRoleValue );
	std::string uniforms;
	const std::vector<UniformSlotRow> noUniforms;
	const auto& uniformRows = hasRole ? role->uniform : noUniforms;
	std::optional<std::size_t> selectedUniformIndex;
	if( state.selectedUniformSlot )
		if( const auto found = std::ranges::find_if( uniformRows, [&]( const UniformSlotRow& row ){ return row.slot == *state.selectedUniformSlot; } ); found != uniformRows.end() )
			selectedUniformIndex = static_cast<std::size_t>( std::distance( uniformRows.begin(), found ) );
	const auto uniformWindow = windowFor( RowSurface::UniformSlots, uniformRows.size(), selectedUniformIndex, state.selectedUniformSlot ? uniformSlotId( *state.selectedUniformSlot ) : std::string{} );
	for( std::size_t index = uniformWindow.begin; index < uniformWindow.end; ++index )
	{
		const auto& row = uniformRows[index];
		uniforms += "<button id='military_uniform_" + std::string{ uniformSlotId( row.slot ) } + "' class='w98-list-item" + std::string( state.selectedUniformSlot == row.slot ? " is-selected" : "" ) + "' data-uniform-slot='" + std::string{ uniformSlotId( row.slot ) }
			+ "'><span class='w98-cell w98-w-slot'>" + esc( tr( ( "management.uniform." + std::string{ uniformSlotId( row.slot ) } ).c_str() ) ) + "</span><span class='w98-cell w98-cell--grow'>" + esc( catalogName( row.type.value ) )
			+ "</span><span class='w98-cell w98-w-profession'>" + esc( catalogName( row.material ? row.material->value : "any" ) ) + "</span></button>";
	}
	appendWindowControls( uniforms, RowSurface::UniformSlots, uniformWindow, uniformRows.size() );
	rml( military_, "military_uniform_rows", uniforms.empty() ? std::string( "<div class='w98-list-empty'>" ) + ( hasRole ? "No uniform slots reported." : "Choose a role." ) + "</div>" : uniforms );
	const UniformSlotRow* slot = selectedUniformIndex ? &uniformRows[*selectedUniformIndex] : nullptr;
	text( military_, "military_uniform_title", slot ? tr( ( "management.uniform." + std::string{ uniformSlotId( slot->slot ) } ).c_str() ) : std::string( "Selected slot" ) );
	std::vector<std::pair<std::string, std::string>> types, materials;
	if( slot )
	{
		// Only the game's legal choices are offered.
		for( const auto& type : slot->possibleTypes ) types.emplace_back( type.value, catalogName( type.value ) );
		for( const auto& material : slot->possibleMaterials ) materials.emplace_back( material.value, catalogName( material.value ) );
	}
	options( "military_uniform_type", types, slot ? slot->type.value : std::string() );
	options( "military_uniform_material", materials, slot && slot->material ? slot->material->value : std::string() );
	enabled( military_, "military_uniform_type", slot && !types.empty() );
	enabled( military_, "military_uniform_material", slot && !materials.empty() );
	text( military_, "military_uniform_scope", hasRole ? "Changes apply at once to every citizen with the " + role->name + " role (" + std::to_string( users ) + ( users == 1 ? " citizen)." : " citizens)." ) : std::string() );

	// ---- Targets page: ordering beside the list; the response is a separate exclusive choice.
	std::string priorities;
	const std::vector<TargetPriorityRow> noPriorities;
	const auto& priorityRows = hasSquad ? squad->priorities : noPriorities;
	const auto priorityWindow = windowFor( RowSurface::Priorities, priorityRows.size(), selectedIndex( priorityRows, state.selectedPriority, []( const TargetPriorityRow& row ){ return row.targetType; } ),
		state.selectedPriority ? state.selectedPriority->value : std::string{} );
	for( std::size_t index = priorityWindow.begin; index < priorityWindow.end; ++index )
	{
		const auto& row = priorityRows[index];
		priorities += "<button id='military_priority_" + esc( row.targetType.value ) + "' class='w98-list-item" + std::string( state.selectedPriority == row.targetType ? " is-selected" : "" ) + "' data-priority='" + esc( row.targetType.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + std::to_string( index + 1 ) + ". " + esc( row.name ) + "</span><span class='w98-cell w98-w-state'>" + attitudeName( row.attitude ) + "</span></button>";
	}
	appendWindowControls( priorities, RowSurface::Priorities, priorityWindow, priorityRows.size() );
	rml( military_, "military_priority_rows", priorities.empty() ? std::string( "<div class='w98-list-empty'>" ) + ( hasSquad ? "No targets." : "Choose a squad." ) + "</div>" : priorities );
	const auto currentPriorityIndex = selectedIndex( priorityRows, state.selectedPriority, []( const TargetPriorityRow& row ){ return row.targetType; } );
	enabled( military_, "priority_move_up", currentPriorityIndex && *currentPriorityIndex > 0 );
	enabled( military_, "priority_move_down", currentPriorityIndex && *currentPriorityIndex + 1 < priorityRows.size() );
	const bool hasPriority = currentPriorityIndex.has_value();
	text( military_, "military_priority_selection", hasPriority ? "Response to " + priorityRows[*currentPriorityIndex].name : std::string( "Response" ) );
	for( const auto& [id, attitude] : { std::pair{ "attitude_flee", MilitaryAttitude::Flee }, std::pair{ "attitude_defend", MilitaryAttitude::Defend }, std::pair{ "attitude_attack", MilitaryAttitude::Attack }, std::pair{ "attitude_hunt", MilitaryAttitude::Hunt } } )
	{
		enabled( military_, id, hasPriority );
		if( auto* radio = military_->GetElementById( id ) )
		{
			const bool on = hasPriority && priorityRows[*currentPriorityIndex].attitude == attitude;
			if( radio->HasAttribute( "checked" ) != on ) { if( on ) radio->SetAttribute( "checked", "checked" ); else radio->RemoveAttribute( "checked" ); }
		}
	}

	// ---- Deletion reviews use the Windows 98 message box: the title names the squad or role.
	if( state.destructive && !dialog_.active() && shownDestructive_ != state.destructive->modal )
	{
		shownDestructive_ = state.destructive->modal;
		const auto request = *state.destructive;
		const bool isSquad = request.kind == DestructiveKind::Squad;
		dialog_.show( request.displayName,
			"Deleting " + request.displayName + ( isSquad ? " removes the squad and its target list." : " removes the role and its uniform." ) + " You cannot undo this.",
			"Delete", "Cancel",
			[this, modal = request.modal] { controller_->confirmDestructive( modal ); },
			[this] { controller_->cancelDestructive(); } );
	}
	if( !state.destructive ) shownDestructive_ = {};
	text( military_, "military_status", state.pendingAction ? "Applying change..." : state.status.empty() && state.militaryLoad == LoadState::Stale ? "Refreshing military data..." : state.status );
}

void Management6CRmlBinding::openWizard()
{
	if( !controller_ || !controller_->state().selectedNeighbor ) return;
	wizardNeighbor_ = controller_->state().selectedNeighbor;
	wizardPage_     = 1;
	wizardNote_.clear();
	stateChanged( controller_->state() );
	if( auto* first = diplomacy_->GetElementById( "wizard_next" ) ) first->Focus();
}
void Management6CRmlBinding::closeWizard()
{
	wizardPage_ = 0;
	wizardNeighbor_.reset();
	wizardNote_.clear();
	if( controller_ ) stateChanged( controller_->state() );
	if( auto* opener = diplomacy_ ? diplomacy_->GetElementById( "mission_wizard_open" ) : nullptr ) opener->Focus();
}
void Management6CRmlBinding::wizardNext()
{
	if( !controller_ || wizardPage_ == 0 || wizardPage_ >= 3 ) return;
	const auto& draft = controller_->state().missionDraft;
	if( wizardPage_ == 1 && draft.type == MissionType::None ) return;
	if( wizardPage_ == 2 && !draft.creature ) return;
	++wizardPage_;
	if( wizardPage_ == 3 ) reviewedDraft_ = draft; // the reviewed mission; Finish sends exactly this
	stateChanged( controller_->state() );
}
void Management6CRmlBinding::finishWizard()
{
	if( !controller_ || wizardPage_ != 3 ) return;
	const auto& s = controller_->state();
	// Revalidate: the kingdom, mission, task and citizen must still be the reviewed ones and still legal.
	if( s.selectedNeighbor != wizardNeighbor_ || s.missionDraft != reviewedDraft_ || !controller_->canStartDraftMission() )
	{
		wizardNote_ = "The mission or the kingdom changed since you reviewed it. Check the pages again, then click Finish.";
		wizardPage_ = 1;
		stateChanged( s );
		return;
	}
	controller_->startMission();
	closeWizard();
	controller_->open( View::Missions );
}
void Management6CRmlBinding::renderDiplomacy( const Management6CState& state )
{
	const auto page = state.view == View::Missions ? View::Missions : View::Neighbors;
	if( auto* tabs = diplomacy_->GetElementById( "diplomacy_tabs" ) )
		connected_tabs::select( *tabs, diplomacy_->GetElementById( page == View::Missions ? "diplomacy_tab_missions" : "diplomacy_tab_neighbors" ) );
	const auto load = page == View::Neighbors ? state.diplomacyLoad : state.missionLoad;
	const bool hasRows = page == View::Neighbors ? !state.neighbors.empty() : !state.missions.empty();
	visible( diplomacy_, "diplomacy_loading", ( load == LoadState::Loading || load == LoadState::Stale ) && !hasRows );
	visible( diplomacy_, "diplomacy_error", load == LoadState::Error );
	visible( diplomacy_, "diplomacy_empty", load == LoadState::Empty );
	text( diplomacy_, "diplomacy_empty", page == View::Missions ? "No missions have been sent. Choose a kingdom on the Neighbors page and click Send Mission." : "No neighboring kingdoms are known yet." );
	visible( diplomacy_, "diplomacy_neighbors_page", page == View::Neighbors && load != LoadState::Error );
	visible( diplomacy_, "diplomacy_missions_page", page == View::Missions && load != LoadState::Error );
	// Unknown facts are the word "Unknown", never zero or a placeholder (07-win98-design-reference section 3.5).
	const std::string unknown = "Unknown";

	// ---- Neighbors page.
	std::string neighbors;
	const auto& neighborRows = state.neighbors;
	const auto neighborWindow = windowFor( RowSurface::Neighbors, neighborRows.size(), selectedIndex( neighborRows, state.selectedNeighbor, []( const NeighborRow& row ){ return row.id; } ),
		state.selectedNeighbor ? std::to_string( state.selectedNeighbor->value ) : std::string{} );
	for( std::size_t index = neighborWindow.begin; index < neighborWindow.end; ++index )
	{
		const auto& row = neighborRows[index];
		neighbors += "<button id='diplomacy_neighbor_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedNeighbor == row.id ? " is-selected" : "" ) + "' data-neighbor='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + esc( row.discovered && row.name ? *row.name : "Undiscovered kingdom" ) + "</span></button>";
	}
	appendWindowControls( neighbors, RowSurface::Neighbors, neighborWindow, neighborRows.size() );
	rml( diplomacy_, "diplomacy_neighbor_rows", neighbors.empty() ? std::string( "<div class='w98-list-empty'>None known.</div>" ) : neighbors );
	const auto neighbor = state.selectedNeighbor ? std::ranges::find_if( neighborRows, [&]( const NeighborRow& row ){ return row.id == *state.selectedNeighbor; } ) : neighborRows.end();
	const bool hasNeighbor = neighbor != neighborRows.end();
	const bool discovered = hasNeighbor && neighbor->discovered;
	text( diplomacy_, "neighbor_name", discovered && neighbor->name ? *neighbor->name : hasNeighbor ? std::string( "Undiscovered kingdom" ) : std::string( "Kingdom" ) );
	visible( diplomacy_, "neighbor_undiscovered_detail", hasNeighbor && !discovered );
	const auto fact = [&]( const std::optional<std::string>& value ) { return discovered && value ? *value : unknown; };
	text( diplomacy_, "neighbor_distance", hasNeighbor ? fact( neighbor->distance ) : "" );
	text( diplomacy_, "neighbor_type", hasNeighbor ? fact( neighbor->type ) : "" );
	text( diplomacy_, "neighbor_attitude", hasNeighbor ? fact( neighbor->attitude ) : "" );
	text( diplomacy_, "neighbor_wealth", hasNeighbor ? fact( neighbor->wealth ) : "" );
	text( diplomacy_, "neighbor_economy", hasNeighbor ? fact( neighbor->economy ) : "" );
	text( diplomacy_, "neighbor_military", hasNeighbor ? fact( neighbor->military ) : "" );
	const bool anyMission = discovered && ( neighbor->canSpy || neighbor->canSendEmissary || neighbor->canRaid || neighbor->canSabotage );
	enabled( diplomacy_, "mission_wizard_open", anyMission && !state.pendingAction );
	text( diplomacy_, "mission_unavailable", !hasNeighbor ? "Choose a kingdom." : !discovered ? "" : !anyMission ? "No mission can be sent to this kingdom now." : "" );

	// ---- Missions page: list view and the selected mission's reported facts.
	std::string missions;
	const auto& missionRows = state.missions;
	const auto missionWindow = windowFor( RowSurface::Missions, missionRows.size(), selectedIndex( missionRows, state.selectedMission, []( const MissionRow& row ){ return row.id; } ),
		state.selectedMission ? std::to_string( state.selectedMission->value ) : std::string{} );
	for( std::size_t index = missionWindow.begin; index < missionWindow.end; ++index )
	{
		const auto& row = missionRows[index];
		const std::string status = row.step == MissionStep::Returned ? ( row.result.success ? ( *row.result.success ? "Succeeded" : "Did not succeed" ) : "Returned" ) : missionStepName( row.step );
		missions += "<button id='diplomacy_mission_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( state.selectedMission == row.id ? " is-selected" : "" ) + "' data-mission='" + std::to_string( row.id.value )
			+ "'><span class='w98-cell w98-w-profession'>" + missionTypeName( row.type ) + "</span><span class='w98-cell w98-cell--grow'>" + esc( destinationName( state, row ) ) + "</span><span class='w98-cell w98-w-profession'>" + esc( status ) + "</span></button>";
	}
	appendWindowControls( missions, RowSurface::Missions, missionWindow, missionRows.size() );
	rml( diplomacy_, "diplomacy_mission_rows", missions.empty() ? std::string( "<div class='w98-list-empty'>No missions.</div>" ) : missions );
	const auto mission = state.selectedMission ? std::ranges::find_if( missionRows, [&]( const MissionRow& row ){ return row.id == *state.selectedMission; } ) : missionRows.end();
	const bool hasMission = mission != missionRows.end();
	text( diplomacy_, "mission_title", hasMission ? std::string( missionTypeName( mission->type ) ) + " to " + destinationName( state, *mission ) : std::string( "Mission" ) );
	text( diplomacy_, "mission_action", hasMission ? ( mission->action != MissionAction::None ? std::string( missionActionName( mission->action ) ) : std::string( "None" ) ) : "" );
	text( diplomacy_, "mission_step", hasMission ? std::string( missionStepName( mission->step ) ) : "" );
	std::string participants;
	if( hasMission )
		for( const auto& creature : mission->participants )
		{
			if( !participants.empty() ) participants += ", ";
			const auto* member = memberById( state, creature );
			participants += member ? member->name : "Citizen " + std::to_string( creature.value );
		}
	text( diplomacy_, "mission_participants", hasMission ? ( participants.empty() ? unknown : participants ) : "" );
	text( diplomacy_, "mission_timing", !hasMission ? std::string() : mission->step == MissionStep::Returned
		? ( mission->result.totalHours ? std::to_string( *mission->result.totalHours ) + " hours in total" : unknown )
		: std::to_string( mission->elapsedHours ) + " hours so far" );
	text( diplomacy_, "mission_result", !hasMission ? std::string() : mission->result.success ? ( *mission->result.success ? std::string( "Succeeded" ) : std::string( "Did not succeed" ) )
		: mission->step == MissionStep::Returned ? unknown : std::string( "Not reported yet" ) );

	// ---- Send Mission wizard (simple wizard: Mission, Citizen, Review).
	if( wizardPage_ && ( !state.selectedNeighbor || state.selectedNeighbor != wizardNeighbor_ || !discovered ) ) wizardPage_ = 0; // the kingdom went away
	visible( diplomacy_, "mission_wizard", wizardPage_ != 0 );
	if( wizardPage_ )
	{
		const auto kingdom = neighbor->name.value_or( "the kingdom" );
		text( diplomacy_, "mission_wizard_title", "Send Mission to " + kingdom );
		const char* headings[] = { "", "Mission", "Citizen", "Review" };
		text( diplomacy_, "wizard_heading", headings[wizardPage_] );
		text( diplomacy_, "wizard_subtitle", wizardPage_ == 1 ? "Which mission do you want to send to " + kingdom + "?"
			: wizardPage_ == 2 ? std::string( "Which citizen do you want to send?" ) : std::string( "Check the mission, then click Finish to send it." ) );
		visible( diplomacy_, "wizard_page_mission", wizardPage_ == 1 );
		visible( diplomacy_, "wizard_page_citizen", wizardPage_ == 2 );
		visible( diplomacy_, "wizard_page_review", wizardPage_ == 3 );
		const auto& draft = state.missionDraft;
		const auto radio = [&]( const char* id, bool on, bool available ) {
			enabled( diplomacy_, id, available );
			if( auto* e = diplomacy_->GetElementById( id ) )
				if( e->HasAttribute( "checked" ) != on ) { if( on ) e->SetAttribute( "checked", "checked" ); else e->RemoveAttribute( "checked" ); }
		};
		radio( "mission_type_emissary", draft.type == MissionType::Emissary, neighbor->canSendEmissary );
		radio( "mission_type_spy", draft.type == MissionType::Spy, neighbor->canSpy );
		radio( "mission_type_raid", draft.type == MissionType::Raid, neighbor->canRaid );
		radio( "mission_type_sabotage", draft.type == MissionType::Sabotage, neighbor->canSabotage );
		const bool emissary = draft.type == MissionType::Emissary;
		visible( diplomacy_, "mission_action_group", emissary );
		radio( "mission_action_improve", draft.action == MissionAction::Improve, emissary );
		radio( "mission_action_insult", draft.action == MissionAction::Insult, emissary );
		radio( "mission_action_trader", draft.action == MissionAction::InviteTrader, emissary );
		radio( "mission_action_ambassador", draft.action == MissionAction::InviteAmbassador, emissary );
		text( diplomacy_, "mission_types_note", !wizardNote_.empty() ? wizardNote_ : "Missions this kingdom does not allow are unavailable." );
		std::string gnomes;
		const auto gnomeWindow = windowFor( RowSurface::Gnomes, state.availableGnomes.size(), selectedIndex( state.availableGnomes, draft.creature, []( const AvailableGnomeRow& row ){ return row.id; } ),
			draft.creature ? std::to_string( draft.creature->value ) : std::string{} );
		for( std::size_t index = gnomeWindow.begin; index < gnomeWindow.end; ++index )
		{
			const auto& row = state.availableGnomes[index];
			gnomes += "<button id='diplomacy_gnome_" + std::to_string( row.id.value ) + "' class='w98-list-item" + std::string( draft.creature == row.id ? " is-selected" : "" ) + "' data-gnome='" + std::to_string( row.id.value )
				+ "'><span class='w98-cell w98-cell--grow'>" + esc( row.name ) + "</span></button>";
		}
		appendWindowControls( gnomes, RowSurface::Gnomes, gnomeWindow, state.availableGnomes.size() );
		const bool loading = state.availableGnomeLoad == LoadState::Loading || state.availableGnomeLoad == LoadState::Idle;
		rml( diplomacy_, "diplomacy_gnome_rows", gnomes.empty() ? "<div class='w98-list-empty'>" + esc( loading ? std::string( "Finding citizens who can go..." ) : state.availableGnomeLoad == LoadState::Error ? std::string( "The list of citizens could not be loaded." ) : std::string( "No citizen can go now." ) ) + "</div>" : gnomes );
		const auto* citizen = draft.creature ? [&]() -> const AvailableGnomeRow* { const auto f = std::ranges::find_if( state.availableGnomes, [&]( const auto& r ){ return r.id == *draft.creature; } ); return f == state.availableGnomes.end() ? nullptr : &*f; }() : nullptr;
		text( diplomacy_, "review_destination", kingdom );
		text( diplomacy_, "review_mission", std::string( missionTypeName( draft.type ) ) + ( draft.action != MissionAction::None ? std::string( ": " ) + missionActionName( draft.action ) : std::string() ) );
		text( diplomacy_, "review_citizen", citizen ? citizen->name : unknown );
		text( diplomacy_, "review_note", controller_->canStartDraftMission() ? "Finish sends this mission once." : "This mission cannot be sent now. Go back and change it." );
		enabled( diplomacy_, "wizard_back", wizardPage_ > 1 );
		visible( diplomacy_, "wizard_next", wizardPage_ < 3 );
		enabled( diplomacy_, "wizard_next", wizardPage_ == 1 ? draft.type != MissionType::None : draft.creature.has_value() );
		visible( diplomacy_, "mission_start", wizardPage_ == 3 );
		enabled( diplomacy_, "mission_start", wizardPage_ == 3 && controller_->canStartDraftMission() );
	}
	text( diplomacy_, "diplomacy_status", state.pendingAction ? "Sending..." : state.status.empty() && load == LoadState::Stale ? "Refreshing diplomacy data..." : state.status );
}

} // namespace ingnomia::ui::management6c
