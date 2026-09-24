/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CRmlBinding.h"
#include "../ManagementTooltip.h"
#include "../../localization/RmlText.h"

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

	bind( military_, "military_close", [this]{ closeRoute(); } );
	bind( military_, "military_tab_squads", [this]{ controller_->open( View::Squads ); } );
	bind( military_, "military_tab_roles", [this]{ controller_->open( View::Roles ); } );
	bind( military_, "military_tab_priorities", [this]{ controller_->open( View::Priorities ); } );
	bind( military_, "military_tab_neighbors", [this]{ controller_->open( View::Neighbors ); } );
		bind( military_, "military_tab_missions", [this]{ controller_->open( View::Missions ); } );
		bind( military_, "military_views_toggle", [this]{
			if( auto* shell = military_->GetElementById( "military_shell" ) )
			{
				const bool open = !shell->IsClassSet( "is-rail-open" );
				shell->SetClass( "is-rail-open", open );
				if( auto* toggle = military_->GetElementById( "military_views_toggle" ) ) toggle->SetAttribute( "aria-expanded", open ? "true" : "false" );
			}
		} );
		bind( diplomacy_, "diplomacy_views_toggle", [this]{
			if( auto* shell = diplomacy_->GetElementById( "diplomacy_shell" ) )
			{
				const bool open = !shell->IsClassSet( "is-rail-open" );
				shell->SetClass( "is-rail-open", open );
				if( auto* toggle = diplomacy_->GetElementById( "diplomacy_views_toggle" ) ) toggle->SetAttribute( "aria-expanded", open ? "true" : "false" );
			}
		} );
	bind( military_, "military_refresh", [this]{ controller_->refresh(); } );
	bind( military_, "military_error_retry", [this]{ controller_->refresh(); } );
	bind( military_, "military_sort_source", [this]{ controller_->setMilitarySort( controller_->state().militarySort == Sort::SourceOrder ? Sort::Name : Sort::SourceOrder ); } );
	bind( military_, "military_sort_name", [this]{ controller_->setMilitarySort( Sort::Name ); } );
	bind( military_, "military_previous", [this]{ controller_->selectPrevious(); focusCurrentRow(); } );
	bind( military_, "military_next", [this]{ controller_->selectNext(); focusCurrentRow(); } );
	bindEvent( military_, "military_search", "change", [this]( Rml::Event& event ) {
		if( auto* element = event.GetCurrentElement() )
			controller_->setMilitaryFilter( element->GetAttribute<Rml::String>( "value", "" ) );
	} );
	bindEvent( military_, "military_squad_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-squad" ), "data-squad" ) )
			{ showDetails( true ); controller_->selectSquad( SquadId{ *id } ); }
	} );
	bindEvent( military_, "military_squad_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( military_, "military_role_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-role" ), "data-role" ) )
			{ showDetails( true ); controller_->selectRole( MilitaryRoleId{ *id } ); }
	} );
	bindEvent( military_, "military_role_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( military_, "military_member_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-creature" ), "data-creature" ) )
			controller_->selectMember( CreatureId{ *id } );
	} );
	bindEvent( military_, "military_member_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveMemberSelection( value ); } ) ) focusSelectedMember();
	} );
	bindEvent( military_, "military_unassigned_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-creature" ), "data-creature" ) )
			controller_->selectMember( CreatureId{ *id } );
	} );
	bindEvent( military_, "military_unassigned_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveMemberSelection( value ); } ) ) focusSelectedMember();
	} );
	bindEvent( military_, "military_priority_rows", "click", [this]( Rml::Event& event ) {
		if( auto* element = attributeTarget( event, "data-priority" ) )
			controller_->selectPriority( CatalogId{ element->GetAttribute<Rml::String>( "data-priority", "" ) } );
	} );
	bindEvent( military_, "military_priority_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->movePrioritySelection( value ); } ) ) focusSelectedPriority();
	} );
	bindEvent( military_, "military_uniform_rows", "click", [this]( Rml::Event& event ) {
		if( auto* element = attributeTarget( event, "data-uniform-slot" ) )
			if( const auto slot = uniformSlot( element->GetAttribute<Rml::String>( "data-uniform-slot", "" ) ) )
				controller_->selectUniformSlot( *slot );
	} );
	bindEvent( military_, "military_uniform_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveUniformSelection( value ); } ) ) focusSelectedUniform();
	} );
	bindEvent( military_, "military_uniform_type_rows", "click", [this]( Rml::Event& event ) {
		auto* element = attributeTarget( event, "data-uniform-type" );
		if( !element || !controller_->state().selectedRole || !controller_->state().selectedUniformSlot ) return;
		const auto role = std::ranges::find_if( controller_->state().roles, [&]( const MilitaryRoleRow& row ){ return row.id == *controller_->state().selectedRole; } );
		if( role == controller_->state().roles.end() ) return;
		const auto slot = std::ranges::find_if( role->uniform, [&]( const UniformSlotRow& row ){ return row.slot == *controller_->state().selectedUniformSlot; } );
		if( slot != role->uniform.end() ) controller_->setSelectedUniform(
			CatalogId{ element->GetAttribute<Rml::String>( "data-uniform-type", "" ) }, slot->material );
	} );
	bindEvent( military_, "military_uniform_material_rows", "click", [this]( Rml::Event& event ) {
		auto* element = attributeTarget( event, "data-uniform-material" );
		if( !element || !controller_->state().selectedRole || !controller_->state().selectedUniformSlot ) return;
		const auto role = std::ranges::find_if( controller_->state().roles, [&]( const MilitaryRoleRow& row ){ return row.id == *controller_->state().selectedRole; } );
		if( role == controller_->state().roles.end() ) return;
		const auto slot = std::ranges::find_if( role->uniform, [&]( const UniformSlotRow& row ){ return row.slot == *controller_->state().selectedUniformSlot; } );
		if( slot != role->uniform.end() ) controller_->setSelectedUniform( slot->type,
			CatalogId{ element->GetAttribute<Rml::String>( "data-uniform-material", "" ) } );
	} );

	bind( military_, "military_list_tools", [this]{ filtersOpen_ = !filtersOpen_; stateChanged( controller_->state() ); } );
    bind( diplomacy_, "diplomacy_list_tools", [this]{ filtersOpen_ = !filtersOpen_; stateChanged( controller_->state() ); } );
    bind( military_, "military_back", [this]{ showDetails( false ); } );
    bind( diplomacy_, "diplomacy_back", [this]{ showDetails( false ); } );
    bind( diplomacy_, "diplomacy_empty_neighbors", [this]{ controller_->open( View::Neighbors ); } );
    bind( diplomacy_, "diplomacy_new_mission", [this]{ controller_->open( View::Neighbors ); } );
    bind( diplomacy_, "diplomacy_view_missions", [this]{ controller_->open( View::Missions ); } );
    bindEvent( military_, "military_member_role_choices", "change", [this]( Rml::Event& event ) {
        const auto role = unsignedValue( event.GetParameter<Rml::String>( "value", "" ) );
        if( role && controller_->state().selectedMember )
            controller_->assignMemberRole( *controller_->state().selectedMember, MilitaryRoleId{ *role } );
    } );
    bindEvent( military_, "military_uniform_type_rows", "change", [this]( Rml::Event& event ) {
        const auto type = event.GetParameter<Rml::String>( "value", "" );
        if( !type.empty() ) controller_->setSelectedUniform( CatalogId{ type }, CatalogId{ "any" } );
    } );
    bindEvent( military_, "military_uniform_material_rows", "change", [this]( Rml::Event& event ) {
        const auto material = event.GetParameter<Rml::String>( "value", "" );
        const auto& state = controller_->state();
        const auto role = std::ranges::find_if( state.roles, [&]( const auto& row ){ return state.selectedRole == row.id; } );
        if( material.empty() || role == state.roles.end() ) return;
        const auto slot = std::ranges::find_if( role->uniform, [&]( const auto& row ){ return state.selectedUniformSlot == row.slot; } );
        if( slot != role->uniform.end() ) controller_->setSelectedUniform( slot->type, CatalogId{ material } );
    } );
	bind( military_, "squad_add", [this]{ controller_->addSquad(); } );
	bind( military_, "squad_move_up", [this]{ controller_->moveSelectedSquad( MoveDirection::Up ); } );
	bind( military_, "squad_move_down", [this]{ controller_->moveSelectedSquad( MoveDirection::Down ); } );
	bind( military_, "squad_remove", [this]{ controller_->requestRemoveSelectedSquad(); } );
	bind( military_, "squad_rename", [this]{ if( auto* element = military_->GetElementById( "squad_name_input" ) )
		controller_->renameSelectedSquad( element->GetAttribute<Rml::String>( "value", "" ) ); } );
	bind( military_, "member_move_up", [this]{ controller_->moveSelectedMember( MoveDirection::Up ); } );
	bind( military_, "member_move_down", [this]{ controller_->moveSelectedMember( MoveDirection::Down ); } );
	bind( military_, "member_remove", [this]{ controller_->removeSelectedMember(); } );
	bind( military_, "member_assign_squad", [this]{ controller_->assignSelectedMemberToSelectedSquad(); } );
	bind( military_, "role_add", [this]{ controller_->addRole(); } );
	bind( military_, "role_assign_member", [this]{ controller_->assignSelectedMemberToRole(); } );
	bind( military_, "role_remove", [this]{ controller_->requestRemoveSelectedRole(); } );
	bind( military_, "role_rename", [this]{ if( auto* element = military_->GetElementById( "role_name_input" ) )
		controller_->renameSelectedRole( element->GetAttribute<Rml::String>( "value", "" ) ); } );
	bind( military_, "role_toggle_civilian", [this]{
		if( !controller_->state().selectedRole ) return;
		const auto role = std::ranges::find_if( controller_->state().roles, [&]( const MilitaryRoleRow& row ){ return row.id == *controller_->state().selectedRole; } );
		if( role != controller_->state().roles.end() ) controller_->setSelectedRoleCivilian( !role->civilian );
	} );
	bind( military_, "priority_move_up", [this]{ controller_->moveSelectedPriority( MoveDirection::Up ); } );
	bind( military_, "priority_move_down", [this]{ controller_->moveSelectedPriority( MoveDirection::Down ); } );
	bind( military_, "attitude_flee", [this]{ controller_->setSelectedAttitude( MilitaryAttitude::Flee ); } );
	bind( military_, "attitude_defend", [this]{ controller_->setSelectedAttitude( MilitaryAttitude::Defend ); } );
	bind( military_, "attitude_attack", [this]{ controller_->setSelectedAttitude( MilitaryAttitude::Attack ); } );
	bind( military_, "attitude_hunt", [this]{ controller_->setSelectedAttitude( MilitaryAttitude::Hunt ); } );
	bind( military_, "military_confirm_cancel", [this]{ controller_->cancelDestructive(); } );
	bind( military_, "military_confirm_accept", [this]{
		if( controller_->state().destructive ) controller_->confirmDestructive( controller_->state().destructive->modal );
	} );
	bindEvent( military_, "military_confirm_layer", "keydown", [this]( Rml::Event& event ) {
		const auto key = static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) );
		if( key == Rml::Input::KI_ESCAPE ) controller_->cancelDestructive();
		else if( key == Rml::Input::KI_TAB )
		{
			const auto modifiers = event.GetParameter<int>( "key_modifier_state", 0 );
			const char* id = ( modifiers & Rml::Input::KM_SHIFT ) ? "military_confirm_cancel" : "military_confirm_accept";
			if( auto* element = military_->GetElementById( id ) ) element->Focus();
		}
		else return;
		event.StopPropagation();
	} );

	bind( diplomacy_, "diplomacy_close", [this]{ closeRoute(); } );
	bind( diplomacy_, "diplomacy_tab_squads", [this]{ controller_->open( View::Squads ); } );
	bind( diplomacy_, "diplomacy_tab_roles", [this]{ controller_->open( View::Roles ); } );
	bind( diplomacy_, "diplomacy_tab_priorities", [this]{ controller_->open( View::Priorities ); } );
	bind( diplomacy_, "diplomacy_tab_neighbors", [this]{ controller_->open( View::Neighbors ); } );
	bind( diplomacy_, "diplomacy_tab_missions", [this]{ controller_->open( View::Missions ); } );
	bind( diplomacy_, "diplomacy_refresh", [this]{ controller_->refresh(); } );
	bind( diplomacy_, "diplomacy_error_retry", [this]{ controller_->refresh(); } );
	bind( diplomacy_, "diplomacy_sort_source", [this]{ controller_->setDiplomacySort( controller_->state().diplomacySort == Sort::SourceOrder ? Sort::Name : Sort::SourceOrder ); } );
	bind( diplomacy_, "diplomacy_sort_name", [this]{ controller_->setDiplomacySort( Sort::Name ); } );
	bind( diplomacy_, "diplomacy_previous", [this]{ controller_->selectPrevious(); focusCurrentRow(); } );
	bind( diplomacy_, "diplomacy_next", [this]{ controller_->selectNext(); focusCurrentRow(); } );
	bindEvent( diplomacy_, "diplomacy_search", "change", [this]( Rml::Event& event ) {
		if( auto* element = event.GetCurrentElement() )
			controller_->setDiplomacyFilter( element->GetAttribute<Rml::String>( "value", "" ) );
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-neighbor" ), "data-neighbor" ) )
			{ showDetails( true ); controller_->selectNeighbor( NeighborId{ *id } ); }
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( diplomacy_, "diplomacy_mission_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-mission" ), "data-mission" ) )
			{ showDetails( true ); controller_->selectMission( MissionId{ *id } ); }
	} );
	bindEvent( diplomacy_, "diplomacy_mission_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( diplomacy_, "diplomacy_gnome_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-gnome" ), "data-gnome" ) )
			controller_->selectMissionGnome( CreatureId{ *id } );
	} );
	bindEvent( diplomacy_, "diplomacy_gnome_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ controller_->moveMissionGnomeSelection( value ); } ) ) focusSelectedGnome();
	} );
	bind( diplomacy_, "mission_type_spy", [this]{ controller_->setMissionType( MissionType::Spy ); } );
	bind( diplomacy_, "mission_type_emissary", [this]{ controller_->setMissionType( MissionType::Emissary ); } );
	bind( diplomacy_, "mission_type_raid", [this]{ controller_->setMissionType( MissionType::Raid ); } );
	bind( diplomacy_, "mission_type_sabotage", [this]{ controller_->setMissionType( MissionType::Sabotage ); } );
	bind( diplomacy_, "mission_action_improve", [this]{ controller_->setMissionAction( MissionAction::Improve ); } );
	bind( diplomacy_, "mission_action_insult", [this]{ controller_->setMissionAction( MissionAction::Insult ); } );
	bind( diplomacy_, "mission_action_trader", [this]{ controller_->setMissionAction( MissionAction::InviteTrader ); } );
	bind( diplomacy_, "mission_action_ambassador", [this]{ controller_->setMissionAction( MissionAction::InviteAmbassador ); } );
	bind( diplomacy_, "mission_start", [this]{ controller_->startMission(); } );

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
	bindTooltips( military_, "military_tooltip", { "military_views_toggle", "military_tab_squads",
		"military_tab_roles", "military_tab_priorities" } );
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
	text( military_, "military_views_toggle", tr( "management.navigation.views" ) + ": " + tr( state.view == View::Roles ? "management.military.roles_uniforms" : state.view == View::Priorities ? "management.military.target_priorities" : "management.military.squads" ) );
	selected( military_, "military_tab_squads", state.view == View::Squads );
	selected( military_, "military_tab_roles", state.view == View::Roles );
	selected( military_, "military_tab_priorities", state.view == View::Priorities );
	selected( military_, "military_tab_neighbors", state.view == View::Neighbors );
	selected( military_, "military_tab_missions", state.view == View::Missions );
	const bool hasRows = !state.roster.squads.empty() || !state.roster.unassigned.empty() || !state.roles.empty();
	visible( military_, "military_loading", ( state.militaryLoad == LoadState::Loading || state.militaryLoad == LoadState::Stale ) && !hasRows );
	visible( military_, "military_empty", false );
	visible( military_, "military_error", state.militaryLoad == LoadState::Error );
	visible( military_, "military_main", ( hasRows || state.militaryLoad == LoadState::Empty ) && state.militaryLoad != LoadState::Error );
	visible( military_, "squad_add", state.view != View::Roles );
	visible( military_, "role_add", state.view == View::Roles );
	inputValue( military_, "military_search", state.militaryFilter );
	text( military_, "military_sort_source", tr( state.militarySort == Sort::Name ? "management.flow.sort_name" : "management.flow.sort_original" ) );
	if( auto* search = military_->GetElementById( "military_search" ) ) search->SetAttribute( "placeholder", tr( state.view == View::Roles ? "management.flow.filter_roles" : "management.flow.filter_squads" ) );
	visible( military_, "military_squad_rows", state.view != View::Roles );
	visible( military_, "military_role_rows", state.view == View::Roles );
	visible( military_, "military_squad_detail", state.view == View::Squads );
	visible( military_, "military_role_detail", state.view == View::Roles );
	visible( military_, "military_priority_detail", state.view == View::Priorities );

	std::string squadRows;
	const auto visibleSquadRows = controller_->visibleSquads();
	const auto squadWindow = windowFor( RowSurface::Squads, visibleSquadRows.size(),
		selectedIndex( visibleSquadRows, state.selectedSquad, []( const SquadRow& row ){ return row.id; } ),
		state.selectedSquad ? std::to_string( state.selectedSquad->value ) : std::string{} );
	for( std::size_t index = squadWindow.begin; index < squadWindow.end; ++index )
	{
		const auto& row = visibleSquadRows[index];
		squadRows += "<button id='military_squad_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedSquad == row.id ) squadRows += " is-selected";
		squadRows += "' data-squad='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + std::to_string( row.members.size() )
            + ( row.members.size() == 1 ? " member" : " members" ) + "</span></button>";
	}
	appendWindowControls( squadRows, RowSurface::Squads, squadWindow, visibleSquadRows.size() );
	rml( military_, "military_squad_rows", squadRows.empty() ? "<div class='c-state-panel'>No squads match the filter.</div>" : squadRows );

	std::string roleRows;
	const auto visibleRoleRows = controller_->visibleRoles();
	const auto roleWindow = windowFor( RowSurface::Roles, visibleRoleRows.size(),
		selectedIndex( visibleRoleRows, state.selectedRole, []( const MilitaryRoleRow& row ){ return row.id; } ),
		state.selectedRole ? std::to_string( state.selectedRole->value ) : std::string{} );
	for( std::size_t index = roleWindow.begin; index < roleWindow.end; ++index )
	{
		const auto& row = visibleRoleRows[index];
		roleRows += "<button id='military_role_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedRole == row.id ) roleRows += " is-selected";
		roleRows += "' data-role='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + ( row.civilian ? "Civilian" : "Combat" ) + "</span></button>";
	}
	appendWindowControls( roleRows, RowSurface::Roles, roleWindow, visibleRoleRows.size() );
	rml( military_, "military_role_rows", roleRows.empty() ? "<div class='c-state-panel'>No roles match the filter.</div>" : roleRows );

    visible( military_, "military_selection_status", state.militarySelectionHidden );
    text( military_, "military_selection_status", tr( "management.flow.selected_hidden" ) );
    const auto roleName = [&]( const std::optional<MilitaryRoleId>& id ) {
        if( !id ) return tr( "management.flow.no_role" );
        const auto found = std::ranges::find_if( state.roles, [&]( const MilitaryRoleRow& row ){ return row.id == *id; } );
        return found == state.roles.end() ? tr( "management.flow.unknown_role" ) : found->name;
    };

	const auto squad = state.selectedSquad ? std::ranges::find_if( state.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state.selectedSquad; } ) : state.roster.squads.end();
	const bool hasSquad = squad != state.roster.squads.end();
	text( military_, "military_squad_title", hasSquad ? squad->name : "No squad selected" );
	inputValue( military_, "squad_name_input", hasSquad ? squad->name : "" );
	enabled( military_, "squad_rename", hasSquad );
	enabled( military_, "squad_remove", hasSquad );
	enabled( military_, "squad_move_up", hasSquad && squad->canMoveUp );
	visible( military_, "squad_move_up", hasSquad && squad->canMoveUp );
	enabled( military_, "squad_move_down", hasSquad && squad->canMoveDown );
	visible( military_, "squad_move_down", hasSquad && squad->canMoveDown );
	std::string members;
	const std::vector<SquadMemberRow> noMembers;
	const auto& memberRows = hasSquad ? squad->members : noMembers;
	const auto memberWindow = windowFor( RowSurface::Members, memberRows.size(),
		selectedIndex( memberRows, state.selectedMember, []( const SquadMemberRow& row ){ return row.id; } ),
		state.selectedMember ? std::to_string( state.selectedMember->value ) : std::string{} );
	for( std::size_t index = memberWindow.begin; index < memberWindow.end; ++index )
	{
		const auto& row = memberRows[index];
		members += "<button id='military_member_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedMember == row.id ) members += " is-selected";
		members += "' data-creature='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + esc( roleName( row.role ) ) + "</span></button>";
	}
	appendWindowControls( members, RowSurface::Members, memberWindow, memberRows.size() );
	rml( military_, "military_member_rows", members.empty() ? "<div class='c-state-panel'>No squad members.</div>" : members );
	std::string unassigned;
	const auto unassignedWindow = windowFor( RowSurface::Unassigned, state.roster.unassigned.size(),
		selectedIndex( state.roster.unassigned, state.selectedMember, []( const SquadMemberRow& row ){ return row.id; } ),
		state.selectedMember ? std::to_string( state.selectedMember->value ) : std::string{} );
	for( std::size_t index = unassignedWindow.begin; index < unassignedWindow.end; ++index )
	{
		const auto& row = state.roster.unassigned[index];
		unassigned += "<button id='military_unassigned_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedMember == row.id ) unassigned += " is-selected";
		unassigned += "' data-creature='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + esc( roleName( row.role ) ) + "</span></button>";
	}
	appendWindowControls( unassigned, RowSurface::Unassigned, unassignedWindow, state.roster.unassigned.size() );
	rml( military_, "military_unassigned_rows", unassigned.empty() ? "<div class='c-state-panel'>No unassigned citizens.</div>" : unassigned );
	const bool selectedMemberAssigned = state.selectedMember && std::ranges::any_of( state.roster.squads,
		[&]( const SquadRow& value ){ return std::ranges::any_of( value.members,
			[&]( const SquadMemberRow& member ){ return member.id == *state.selectedMember; } ); } );
	const auto sourceSquad = std::ranges::find_if( state.roster.squads, [&]( const SquadRow& row ) {
        return state.selectedMember && std::ranges::any_of( row.members, [&]( const auto& member ){ return member.id == *state.selectedMember; } );
    } );
    const auto sourceIndex = static_cast<std::size_t>( std::distance( state.roster.squads.begin(), sourceSquad ) );
    const bool previousSquad = sourceSquad != state.roster.squads.end() && sourceIndex > 0;
    const bool nextSquad = sourceSquad != state.roster.squads.end() && sourceIndex + 1 < state.roster.squads.size();
    visible( military_, "member_move_up", previousSquad );
    visible( military_, "member_move_down", nextSquad );
    enabled( military_, "member_move_up", previousSquad );
    enabled( military_, "member_move_down", nextSquad );
    if( previousSquad ) text( military_, "member_move_up", "Move to " + state.roster.squads[sourceIndex - 1].name );
    if( nextSquad ) text( military_, "member_move_down", "Move to " + state.roster.squads[sourceIndex + 1].name );
    text( military_, "member_assign_squad", hasSquad ? "Assign to " + squad->name : "Assign to squad" );
	enabled( military_, "member_remove", selectedMemberAssigned );
    visible( military_, "member_remove", selectedMemberAssigned );
	const bool selectedMemberInDestination = hasSquad && state.selectedMember && std::ranges::any_of( squad->members,
		[&]( const SquadMemberRow& member ){ return member.id == *state.selectedMember; } );
	enabled( military_, "member_assign_squad", hasSquad && state.selectedMember && !selectedMemberInDestination );
    visible( military_, "member_assign_squad", hasSquad && state.selectedMember && !selectedMemberInDestination );

	std::string priorities;
	const std::vector<TargetPriorityRow> noPriorities;
	const auto& priorityRows = hasSquad ? squad->priorities : noPriorities;
	const auto priorityWindow = windowFor( RowSurface::Priorities, priorityRows.size(),
		selectedIndex( priorityRows, state.selectedPriority, []( const TargetPriorityRow& row ){ return row.targetType; } ),
		state.selectedPriority ? state.selectedPriority->value : std::string{} );
	for( std::size_t index = priorityWindow.begin; index < priorityWindow.end; ++index )
	{
		const auto& row = priorityRows[index];
		priorities += "<button id='military_priority_" + esc( row.targetType.value ) + "' class='m6c-row c-list__row";
		if( state.selectedPriority == row.targetType ) priorities += " is-selected";
		priorities += "' data-priority='" + esc( row.targetType.value ) + "'><span class='c-list__primary'>" + std::to_string( index + 1 ) + ". " + esc( row.name )
			+ "</span><span class='c-list__meta'>" + attitudeName( row.attitude ) + "</span></button>";
	}
	appendWindowControls( priorities, RowSurface::Priorities, priorityWindow, priorityRows.size() );
	rml( military_, "military_priority_rows", priorities.empty() ? "<div class='c-state-panel'>No target priorities.</div>" : priorities );
	text( military_, "military_priority_title", hasSquad ? squad->name + " target priorities" : "No squad selected" );
	const auto currentPriorityIndex = selectedIndex( priorityRows, state.selectedPriority, []( const TargetPriorityRow& row ){ return row.targetType; } );
    enabled( military_, "priority_move_up", currentPriorityIndex && *currentPriorityIndex > 0 );
    enabled( military_, "priority_move_down", currentPriorityIndex && *currentPriorityIndex + 1 < priorityRows.size() );
    text( military_, "military_priority_selection", currentPriorityIndex ? priorityRows[*currentPriorityIndex].name : "Select a target" );
	MilitaryAttitude currentAttitude = MilitaryAttitude::Flee;
	bool hasPriority = false;
	if( hasSquad && state.selectedPriority )
	{
		const auto row = std::ranges::find_if( squad->priorities, [&]( const TargetPriorityRow& value ){ return value.targetType == *state.selectedPriority; } );
		if( row != squad->priorities.end() ) { currentAttitude = row->attitude; hasPriority = true; }
	}
	const char* helpKey = currentAttitude == MilitaryAttitude::Flee ? "management.flow.flee_help"
        : currentAttitude == MilitaryAttitude::Defend ? "management.flow.defend_help"
        : currentAttitude == MilitaryAttitude::Attack ? "management.flow.attack_help" : "management.flow.hunt_help";
    text( military_, "military_attitude_help", hasPriority ? tr( helpKey ) : "" );
	selected( military_, "attitude_flee", hasPriority && currentAttitude == MilitaryAttitude::Flee );
	selected( military_, "attitude_defend", hasPriority && currentAttitude == MilitaryAttitude::Defend );
	selected( military_, "attitude_attack", hasPriority && currentAttitude == MilitaryAttitude::Attack );
	selected( military_, "attitude_hunt", hasPriority && currentAttitude == MilitaryAttitude::Hunt );
	for( const char* id : { "attitude_flee", "attitude_defend", "attitude_attack", "attitude_hunt" } ) enabled( military_, id, hasPriority );

	const auto role = state.selectedRole ? std::ranges::find_if( state.roles,
		[&]( const MilitaryRoleRow& row ){ return row.id == *state.selectedRole; } ) : state.roles.end();
	const bool hasRole = role != state.roles.end();
	text( military_, "military_role_title", hasRole ? role->name + ( role->civilian ? " (civilian)" : " (combat)" ) : "No role selected" );
	inputValue( military_, "role_name_input", hasRole ? role->name : "" );
	enabled( military_, "role_rename", hasRole );
	enabled( military_, "role_remove", hasRole );
	enabled( military_, "role_toggle_civilian", hasRole );
    text( military_, "role_toggle_civilian", tr( hasRole && role->civilian ? "management.flow.civilian_on" : "management.flow.civilian_off" ) );
    selected( military_, "role_toggle_civilian", hasRole && role->civilian );
    const auto* selectedMember = state.selectedMember ? memberById( state, *state.selectedMember ) : nullptr;
    text( military_, "role_member_assignment_status", selectedMember ? selectedMember->name : tr( "management.flow.choose_citizen" ) );
    std::vector<std::pair<std::string, std::string>> memberRoles;
    if( selectedMember ) for( const auto& row : state.roles ) memberRoles.emplace_back( std::to_string( row.id.value ), row.name );
    rml( military_, "military_member_role_choices", choices( RowSurface::MemberRoles, memberRoles,
        selectedMember && selectedMember->role ? std::to_string( selectedMember->role->value ) : "", "military_member_role_choice" ) );

	std::string uniforms;
	const std::vector<UniformSlotRow> noUniforms;
	const auto& uniformRows = hasRole ? role->uniform : noUniforms;
	std::optional<std::size_t> selectedUniformIndex;
	if( state.selectedUniformSlot )
	{
		const auto found = std::ranges::find_if( uniformRows,
			[&]( const UniformSlotRow& row ){ return row.slot == *state.selectedUniformSlot; } );
		if( found != uniformRows.end() ) selectedUniformIndex = static_cast<std::size_t>( std::distance( uniformRows.begin(), found ) );
	}
	const auto uniformWindow = windowFor( RowSurface::UniformSlots, uniformRows.size(), selectedUniformIndex,
		state.selectedUniformSlot ? uniformSlotId( *state.selectedUniformSlot ) : std::string{} );
	for( std::size_t index = uniformWindow.begin; index < uniformWindow.end; ++index )
	{
		const auto& row = uniformRows[index];
		uniforms += "<button id='military_uniform_" + std::string{ uniformSlotId( row.slot ) } + "' class='m6c-row m6c-uniform-row c-list__row";
		if( state.selectedUniformSlot == row.slot ) uniforms += " is-selected";
		uniforms += "' data-uniform-slot='" + std::string{ uniformSlotId( row.slot ) } + "'><span class='c-list__primary'>" + esc( tr( ( "management.uniform." + std::string{ uniformSlotId( row.slot ) } ).c_str() ) )
			+ "</span><span class='c-list__meta'>" + esc( catalogName( row.type.value ) ) + "</span><span class='c-list__meta'>" + esc( catalogName( row.material ? row.material->value : "any" ) ) + "</span></button>";
	}
	appendWindowControls( uniforms, RowSurface::UniformSlots, uniformWindow, uniformRows.size() );
	rml( military_, "military_uniform_rows", uniforms.empty() ? "<div class='c-state-panel'>No uniform slots reported.</div>" : uniforms );
	const UniformSlotRow* selectedSlot = nullptr;
	if( hasRole && state.selectedUniformSlot )
	{
		const auto found = std::ranges::find_if( role->uniform, [&]( const UniformSlotRow& row ){ return row.slot == *state.selectedUniformSlot; } );
		if( found != role->uniform.end() ) selectedSlot = &*found;
	}
    text( military_, "military_uniform_title", selectedSlot
        ? tr( ( "management.uniform." + std::string{ uniformSlotId( selectedSlot->slot ) } ).c_str() ) : "Select a uniform slot" );
    std::vector<std::pair<std::string, std::string>> types, materials;
    if( selectedSlot )
    {
        for( const auto& type : selectedSlot->possibleTypes ) types.emplace_back( type.value, catalogName( type.value ) );
        for( const auto& material : selectedSlot->possibleMaterials ) materials.emplace_back( material.value, catalogName( material.value ) );
    }
    rml( military_, "military_uniform_type_rows", choices( RowSurface::UniformTypes, types,
        selectedSlot ? selectedSlot->type.value : "", "military_uniform_type_choice" ) );
    rml( military_, "military_uniform_material_rows", choices( RowSurface::UniformMaterials, materials,
        selectedSlot && selectedSlot->material ? selectedSlot->material->value : "any", "military_uniform_material_choice" ) );

	const bool confirm = state.destructive.has_value();
	visible( military_, "military_confirm_layer", confirm );
	if( confirm )
	{
		lastDestructiveKind_ = state.destructive->kind;
		text( military_, "military_confirm_text", "Remove " + state.destructive->displayName + "? This cannot be undone." );
		if( !confirmationVisible_ ) if( auto* element = military_->GetElementById( "military_confirm_cancel" ) ) element->Focus();
	}
	else if( confirmationVisible_ && state.open )
	{
		const char* id = lastDestructiveKind_ == DestructiveKind::Squad ? "squad_remove" : "role_remove";
		if( auto* element = military_->GetElementById( id ) ) element->Focus();
	}
	confirmationVisible_ = confirm;
	visible( military_, "military_footer", state.pendingAction || !state.status.empty() || state.militaryLoad == LoadState::Stale );
	text( military_, "military_status", state.pendingAction
		? "Applying change"
		: state.status.empty() && state.militaryLoad == LoadState::Stale ? "Refreshing military data" : state.status );
}

void Management6CRmlBinding::renderDiplomacy( const Management6CState& state )
{
	text( diplomacy_, "diplomacy_views_toggle", tr( "management.navigation.views" ) + ": " + tr( state.view == View::Missions ? "management.diplomacy.mission_activity" : "management.diplomacy.neighbors" ) );
	selected( diplomacy_, "diplomacy_tab_neighbors", state.view == View::Neighbors );
	selected( diplomacy_, "diplomacy_tab_missions", state.view == View::Missions );
	selected( diplomacy_, "diplomacy_tab_squads", state.view == View::Squads );
	selected( diplomacy_, "diplomacy_tab_roles", state.view == View::Roles );
	selected( diplomacy_, "diplomacy_tab_priorities", state.view == View::Priorities );
	const bool hasNeighbors = !state.neighbors.empty();
	const bool hasMissions = !state.missions.empty();
	const auto load = state.view == View::Neighbors ? state.diplomacyLoad : state.missionLoad;
	const bool hasRows = state.view == View::Neighbors ? hasNeighbors : hasMissions;
	visible( diplomacy_, "diplomacy_loading", ( load == LoadState::Loading || load == LoadState::Stale ) && !hasRows );
	visible( diplomacy_, "diplomacy_empty", load == LoadState::Empty );
    text( diplomacy_, "diplomacy_empty_title", tr( state.view == View::Missions ? "management.flow.no_missions" : "management.flow.no_neighbors" ) );
    text( diplomacy_, "diplomacy_empty_detail", tr( state.view == View::Missions ? "management.flow.no_missions_help" : "management.flow.no_neighbors_help" ) );
    visible( diplomacy_, "diplomacy_empty_neighbors", state.view == View::Missions );
    inputValue( diplomacy_, "diplomacy_search", state.diplomacyFilter );
    text( diplomacy_, "diplomacy_sort_source", tr( state.diplomacySort == Sort::SourceOrder ? "management.flow.sort_original"
        : state.view == View::Missions ? "management.flow.sort_type" : "management.flow.sort_name" ) );
    if( auto* search = diplomacy_->GetElementById( "diplomacy_search" ) ) search->SetAttribute( "placeholder", tr( state.view == View::Missions ? "management.flow.filter_missions" : "management.flow.filter_neighbors" ) );
	visible( diplomacy_, "diplomacy_error", load == LoadState::Error );
	visible( diplomacy_, "diplomacy_main", hasRows && load != LoadState::Error );
	visible( diplomacy_, "diplomacy_neighbor_rows", state.view == View::Neighbors );
	visible( diplomacy_, "diplomacy_mission_rows", state.view == View::Missions );
	visible( diplomacy_, "diplomacy_neighbor_detail", state.view == View::Neighbors );
	visible( diplomacy_, "diplomacy_mission_detail", state.view == View::Missions );

	std::string neighbors;
	const auto visibleNeighborRows = controller_->visibleNeighbors();
	const auto neighborWindow = windowFor( RowSurface::Neighbors, visibleNeighborRows.size(),
		selectedIndex( visibleNeighborRows, state.selectedNeighbor, []( const NeighborRow& row ){ return row.id; } ),
		state.selectedNeighbor ? std::to_string( state.selectedNeighbor->value ) : std::string{} );
	for( std::size_t index = neighborWindow.begin; index < neighborWindow.end; ++index )
	{
		const auto& row = visibleNeighborRows[index];
		neighbors += "<button id='diplomacy_neighbor_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedNeighbor == row.id ) neighbors += " is-selected";
		neighbors += "' data-neighbor='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>"
			+ esc( row.discovered && row.name ? *row.name : "Undiscovered neighbor" ) + "</span><span class='c-list__meta'>"
			+ ( row.discovered ? "Discovered" : "Details unavailable" ) + "</span></button>";
	}
	appendWindowControls( neighbors, RowSurface::Neighbors, neighborWindow, visibleNeighborRows.size() );
	rml( diplomacy_, "diplomacy_neighbor_rows", neighbors.empty() ? "<div class='c-state-panel'>No neighbors match the filter.</div>" : neighbors );
	std::string missions;
	const auto visibleMissionRows = controller_->visibleMissions();
	const auto missionWindow = windowFor( RowSurface::Missions, visibleMissionRows.size(),
		selectedIndex( visibleMissionRows, state.selectedMission, []( const MissionRow& row ){ return row.id; } ),
		state.selectedMission ? std::to_string( state.selectedMission->value ) : std::string{} );
	for( std::size_t index = missionWindow.begin; index < missionWindow.end; ++index )
	{
		const auto& row = visibleMissionRows[index];
		missions += "<button id='diplomacy_mission_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.selectedMission == row.id ) missions += " is-selected";
		missions += "' data-mission='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + std::string{ missionTypeName( row.type ) } + " - " + missionStepName( row.step )
			+ "</span><span class='c-list__meta'>" + esc( destinationName( state, row ) ) + "</span></button>";
	}
	appendWindowControls( missions, RowSurface::Missions, missionWindow, visibleMissionRows.size() );
	rml( diplomacy_, "diplomacy_mission_rows", missions.empty() ? "<div class='c-state-panel'>No missions match the filter.</div>" : missions );
    visible( diplomacy_, "diplomacy_selection_status", state.diplomacySelectionHidden );
    text( diplomacy_, "diplomacy_selection_status", tr( "management.flow.selected_hidden" ) );

	const auto neighbor = state.selectedNeighbor ? std::ranges::find_if( state.neighbors,
		[&]( const NeighborRow& row ){ return row.id == *state.selectedNeighbor; } ) : state.neighbors.end();
	const bool hasNeighbor = neighbor != state.neighbors.end();
	const bool discovered = hasNeighbor && neighbor->discovered;
	text( diplomacy_, "neighbor_name", discovered && neighbor->name ? *neighbor->name : hasNeighbor ? "Undiscovered neighbor" : "No neighbor selected" );
	visible( diplomacy_, "neighbor_discovered_detail", discovered );
	visible( diplomacy_, "neighbor_undiscovered_detail", hasNeighbor && !discovered );
	if( discovered )
	{
		text( diplomacy_, "neighbor_distance", neighbor->distance.value_or( "Not reported" ) );
		text( diplomacy_, "neighbor_type", neighbor->type.value_or( "Not reported" ) );
		text( diplomacy_, "neighbor_attitude", neighbor->attitude.value_or( "Not reported" ) );
		text( diplomacy_, "neighbor_wealth", neighbor->wealth.value_or( "Not reported" ) );
		text( diplomacy_, "neighbor_economy", neighbor->economy.value_or( "Not reported" ) );
		text( diplomacy_, "neighbor_military", neighbor->military.value_or( "Not reported" ) );
	}
	const bool anyMission = discovered && ( neighbor->canSpy || neighbor->canSendEmissary || neighbor->canRaid || neighbor->canSabotage );
	visible( diplomacy_, "mission_builder", anyMission );
	visible( diplomacy_, "mission_type_spy", discovered && neighbor->canSpy );
	visible( diplomacy_, "mission_type_emissary", discovered && neighbor->canSendEmissary );
	visible( diplomacy_, "mission_type_raid", discovered && neighbor->canRaid );
	visible( diplomacy_, "mission_type_sabotage", discovered && neighbor->canSabotage );
	selected( diplomacy_, "mission_type_spy", state.missionDraft.type == MissionType::Spy );
	selected( diplomacy_, "mission_type_emissary", state.missionDraft.type == MissionType::Emissary );
	selected( diplomacy_, "mission_type_raid", state.missionDraft.type == MissionType::Raid );
	selected( diplomacy_, "mission_type_sabotage", state.missionDraft.type == MissionType::Sabotage );
	const bool emissary = state.missionDraft.type == MissionType::Emissary;
	visible( diplomacy_, "mission_action_group", emissary );
	selected( diplomacy_, "mission_action_improve", state.missionDraft.action == MissionAction::Improve );
	selected( diplomacy_, "mission_action_insult", state.missionDraft.action == MissionAction::Insult );
	selected( diplomacy_, "mission_action_trader", state.missionDraft.action == MissionAction::InviteTrader );
	selected( diplomacy_, "mission_action_ambassador", state.missionDraft.action == MissionAction::InviteAmbassador );
	std::string gnomes;
	const auto gnomeWindow = windowFor( RowSurface::Gnomes, state.availableGnomes.size(),
		selectedIndex( state.availableGnomes, state.missionDraft.creature, []( const AvailableGnomeRow& row ){ return row.id; } ),
		state.missionDraft.creature ? std::to_string( state.missionDraft.creature->value ) : std::string{} );
	for( std::size_t index = gnomeWindow.begin; index < gnomeWindow.end; ++index )
	{
		const auto& row = state.availableGnomes[index];
		gnomes += "<button id='diplomacy_gnome_" + std::to_string( row.id.value ) + "' class='m6c-row c-list__row";
		if( state.missionDraft.creature == row.id ) gnomes += " is-selected";
		gnomes += "' data-gnome='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + esc( row.name ) + "</span></button>";
	}
	appendWindowControls( gnomes, RowSurface::Gnomes, gnomeWindow, state.availableGnomes.size() );
	const bool eligibilityLoading = state.availableGnomeLoad == LoadState::Loading || state.availableGnomeLoad == LoadState::Idle;
    rml( diplomacy_, "diplomacy_gnome_rows", gnomes.empty()
        ? "<div class='c-state-panel'>" + esc( tr( eligibilityLoading ? "management.flow.eligible_loading" : state.availableGnomeLoad == LoadState::Error ? "management.flow.eligible_error" : "management.flow.eligible_empty" ) ) + "</div>" : gnomes );
	enabled( diplomacy_, "mission_start", controller_->canStartDraftMission() );

	const auto mission = state.selectedMission ? std::ranges::find_if( state.missions,
		[&]( const MissionRow& row ){ return row.id == *state.selectedMission; } ) : state.missions.end();
	if( mission != state.missions.end() )
	{
		text( diplomacy_, "mission_title", missionTypeName( mission->type ) );
		text( diplomacy_, "mission_id", "Available" );
		text( diplomacy_, "mission_action", missionActionName( mission->action ) );
		text( diplomacy_, "mission_step", missionStepName( mission->step ) );
		text( diplomacy_, "mission_target", destinationName( state, *mission ) );
		std::string participants;
		for( const auto& creature : mission->participants )
        {
            if( !participants.empty() ) participants += ", ";
            const auto* member = memberById( state, creature );
            participants += member ? member->name : "Unknown citizen (" + std::to_string( creature.value ) + ")";
        }
		text( diplomacy_, "mission_participants", participants.empty() ? "None reported" : participants );
		text( diplomacy_, "mission_timing", mission->step == MissionStep::Returned
            ? ( mission->result.totalHours ? std::to_string( *mission->result.totalHours ) + " hours total" : "Duration not reported" )
            : "Elapsed " + std::to_string( mission->elapsedHours ) + " hours" );
        visible( diplomacy_, "mission_action_field", mission->action != MissionAction::None );
        visible( diplomacy_, "mission_result_field", mission->result.success.has_value() );
		std::string result = "Not reported";
		if( mission->result.success ) result = *mission->result.success ? "Success" : "Failure";

		text( diplomacy_, "mission_result", result );
	}
	visible( diplomacy_, "diplomacy_footer", state.pendingAction || !state.status.empty() || load == LoadState::Stale );
	text( diplomacy_, "diplomacy_status", state.pendingAction
		? "Applying change"
		: state.status.empty() && load == LoadState::Stale ? "Refreshing diplomacy data" : state.status );
}

} // namespace ingnomia::ui::management6c
