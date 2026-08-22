/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CRmlBinding.h"
#include "../../localization/RmlText.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>

#include <algorithm>
#include <charconv>
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
	military_ = context_.LoadDocument( "windows/military_manager.rml" );
	diplomacy_ = context_.LoadDocument( "windows/diplomacy_missions.rml" );
	if( !military_ || !diplomacy_ ) { shutdown(); return false; }
	localization::applyRmlText( *military_, textCatalog_ );
	localization::applyRmlText( *diplomacy_, textCatalog_ );
	military_->Hide();
	diplomacy_->Hide();

	bind( military_, "military_close", [this]{ closeMilitary(); } );
	bind( military_, "military_tab_squads", [this]{ controller_->open( View::Squads ); } );
	bind( military_, "military_tab_roles", [this]{ controller_->open( View::Roles ); } );
	bind( military_, "military_tab_priorities", [this]{ controller_->open( View::Priorities ); } );
	bind( military_, "military_refresh", [this]{ controller_->refresh(); } );
	bind( military_, "military_error_retry", [this]{ controller_->refresh(); } );
	bind( military_, "military_sort_source", [this]{ controller_->setMilitarySort( Sort::SourceOrder ); } );
	bind( military_, "military_sort_name", [this]{ controller_->setMilitarySort( Sort::Name ); } );
	bind( military_, "military_previous", [this]{ controller_->selectPrevious(); focusCurrentRow(); } );
	bind( military_, "military_next", [this]{ controller_->selectNext(); focusCurrentRow(); } );
	bindEvent( military_, "military_search", "change", [this]( Rml::Event& event ) {
		if( auto* element = event.GetCurrentElement() )
			controller_->setMilitaryFilter( element->GetAttribute<Rml::String>( "value", "" ) );
	} );
	bindEvent( military_, "military_squad_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-squad" ), "data-squad" ) )
			controller_->selectSquad( SquadId{ *id } );
	} );
	bindEvent( military_, "military_squad_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( military_, "military_role_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-role" ), "data-role" ) )
			controller_->selectRole( MilitaryRoleId{ *id } );
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

	bind( military_, "squad_add", [this]{ controller_->addSquad(); } );
	bind( military_, "squad_move_up", [this]{ controller_->moveSelectedSquad( MoveDirection::Up ); } );
	bind( military_, "squad_move_down", [this]{ controller_->moveSelectedSquad( MoveDirection::Down ); } );
	bind( military_, "squad_remove", [this]{ controller_->requestRemoveSelectedSquad(); } );
	bind( military_, "squad_rename", [this]{ if( auto* element = military_->GetElementById( "squad_name_input" ) )
		controller_->renameSelectedSquad( element->GetAttribute<Rml::String>( "value", "" ) ); } );
	bind( military_, "member_move_up", [this]{ controller_->moveSelectedMember( MoveDirection::Up ); } );
	bind( military_, "member_move_down", [this]{ controller_->moveSelectedMember( MoveDirection::Down ); } );
	bind( military_, "member_remove", [this]{ controller_->removeSelectedMember(); } );
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

	bind( diplomacy_, "diplomacy_close", [this]{ closeDiplomacy(); } );
	bind( diplomacy_, "diplomacy_tab_neighbors", [this]{ controller_->open( View::Neighbors ); } );
	bind( diplomacy_, "diplomacy_tab_missions", [this]{ controller_->open( View::Missions ); } );
	bind( diplomacy_, "diplomacy_refresh", [this]{ controller_->refresh(); } );
	bind( diplomacy_, "diplomacy_error_retry", [this]{ controller_->refresh(); } );
	bind( diplomacy_, "diplomacy_sort_source", [this]{ controller_->setDiplomacySort( Sort::SourceOrder ); } );
	bind( diplomacy_, "diplomacy_sort_name", [this]{ controller_->setDiplomacySort( Sort::Name ); } );
	bind( diplomacy_, "diplomacy_previous", [this]{ controller_->selectPrevious(); focusCurrentRow(); } );
	bind( diplomacy_, "diplomacy_next", [this]{ controller_->selectNext(); focusCurrentRow(); } );
	bindEvent( diplomacy_, "diplomacy_search", "change", [this]( Rml::Event& event ) {
		if( auto* element = event.GetCurrentElement() )
			controller_->setDiplomacyFilter( element->GetAttribute<Rml::String>( "value", "" ) );
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-neighbor" ), "data-neighbor" ) )
			controller_->selectNeighbor( NeighborId{ *id } );
	} );
	bindEvent( diplomacy_, "diplomacy_neighbor_rows", "keydown", [this]( Rml::Event& event ) {
		if( moveKey( event, [this]( std::int32_t value ){ value < 0 ? controller_->selectPrevious() : controller_->selectNext(); } ) ) focusCurrentRow();
	} );
	bindEvent( diplomacy_, "diplomacy_mission_rows", "click", [this]( Rml::Event& event ) {
		if( const auto id = unsignedAttribute( attributeTarget( event, "data-mission" ), "data-mission" ) )
			controller_->selectMission( MissionId{ *id } );
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
		{ diplomacy_, "diplomacy_neighbor_rows" }, { diplomacy_, "diplomacy_mission_rows" },
		{ diplomacy_, "diplomacy_gnome_rows" } } )
		bindEvent( target.first, target.second, "click", [this]( Rml::Event& event ){ (void)handleWindowPage( event ); } );

	stateChanged( controller.state() );
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
		auto callback = std::make_unique<Callback>( std::move( function ) );
		element->AddEventListener( event, callback.get() );
		listeners_.push_back( { element, event, std::move( callback ) } );
	}
}

void Management6CRmlBinding::syncDocuments( const Management6CState& state )
{
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
		// Set the computed display explicitly as well. Dynamic SetInnerRML on a
		// sibling can invalidate the class-only style pass in older RmlUi builds;
		// explicit display keeps mutually-exclusive detail panes from leaking
		// their contents into the active route.
		element->SetProperty( "display", value ? "block" : "none" );
	}
}

void Management6CRmlBinding::selected( Rml::ElementDocument* document, const char* id, bool value )
{
	if( document ) if( auto* element = document->GetElementById( id ) ) element->SetClass( "is-selected", value );
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
	if( document ) if( auto* element = document->GetElementById( id ) ) element->SetAttribute( "value", value );
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

void Management6CRmlBinding::stateChanged( const Management6CState& state )
{
	if( !military_ || !diplomacy_ || !controller_ ) return;
	visible( military_, "military_root", state.open && isMilitaryView( state.view ) );
	visible( diplomacy_, "diplomacy_root", state.open && isDiplomacyView( state.view ) );
	renderMilitary( state );
	renderDiplomacy( state );
	syncDocuments( state );
}

void Management6CRmlBinding::renderMilitary( const Management6CState& state )
{
	selected( military_, "military_tab_squads", state.view == View::Squads );
	selected( military_, "military_tab_roles", state.view == View::Roles );
	selected( military_, "military_tab_priorities", state.view == View::Priorities );
	const bool hasRows = !state.roster.squads.empty() || !state.roster.unassigned.empty() || !state.roles.empty();
	visible( military_, "military_loading", ( state.militaryLoad == LoadState::Loading || state.militaryLoad == LoadState::Stale ) && !hasRows );
	visible( military_, "military_empty", state.militaryLoad == LoadState::Empty );
	visible( military_, "military_error", state.militaryLoad == LoadState::Error );
	visible( military_, "military_main", hasRows && state.militaryLoad != LoadState::Error );
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
			+ "</span><span class='c-list__meta'>" + std::to_string( row.members.size() ) + " members</span></button>";
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

	std::string selection = "Selection: ";
	if( state.view == View::Roles ) selection += state.selectedRole ? "role " + std::to_string( state.selectedRole->value ) : "none";
	else selection += state.selectedSquad ? "squad " + std::to_string( state.selectedSquad->value ) : "none";
	if( state.militarySelectionHidden ) selection += " (preserved, hidden by filter)";
	text( military_, "military_selection_status", selection );
	if( auto* element = military_->GetElementById( "military_selection_status" ) ) element->SetClass( "m6c-hidden-selection", state.militarySelectionHidden );

	const auto squad = state.selectedSquad ? std::ranges::find_if( state.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state.selectedSquad; } ) : state.roster.squads.end();
	const bool hasSquad = squad != state.roster.squads.end();
	text( military_, "military_squad_title", hasSquad ? squad->name : "No squad selected" );
	inputValue( military_, "squad_name_input", hasSquad ? squad->name : "" );
	enabled( military_, "squad_rename", hasSquad );
	enabled( military_, "squad_remove", hasSquad );
	enabled( military_, "squad_move_up", hasSquad && squad->canMoveUp );
	enabled( military_, "squad_move_down", hasSquad && squad->canMoveDown );
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
			+ "</span><span class='c-list__meta'>Role " + ( row.role ? std::to_string( row.role->value ) : "unassigned" ) + "</span></button>";
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
			+ "</span><span class='c-list__meta'>No squad</span></button>";
	}
	appendWindowControls( unassigned, RowSurface::Unassigned, unassignedWindow, state.roster.unassigned.size() );
	rml( military_, "military_unassigned_rows", unassigned.empty() ? "<div class='c-state-panel'>No unassigned citizens.</div>" : unassigned );
	const bool selectedMemberAssigned = state.selectedMember && std::ranges::any_of( state.roster.squads,
		[&]( const SquadRow& value ){ return std::ranges::any_of( value.members,
			[&]( const SquadMemberRow& member ){ return member.id == *state.selectedMember; } ); } );
	enabled( military_, "member_move_up", selectedMemberAssigned );
	enabled( military_, "member_move_down", selectedMemberAssigned );
	enabled( military_, "member_remove", selectedMemberAssigned );

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
		priorities += "' data-priority='" + esc( row.targetType.value ) + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + attitudeName( row.attitude ) + "</span></button>";
	}
	appendWindowControls( priorities, RowSurface::Priorities, priorityWindow, priorityRows.size() );
	rml( military_, "military_priority_rows", priorities.empty() ? "<div class='c-state-panel'>No target priorities.</div>" : priorities );
	text( military_, "military_priority_title", hasSquad ? squad->name + " target priorities" : "No squad selected" );
	enabled( military_, "priority_move_up", state.selectedPriority.has_value() );
	enabled( military_, "priority_move_down", state.selectedPriority.has_value() );
	MilitaryAttitude currentAttitude = MilitaryAttitude::Flee;
	bool hasPriority = false;
	if( hasSquad && state.selectedPriority )
	{
		const auto row = std::ranges::find_if( squad->priorities, [&]( const TargetPriorityRow& value ){ return value.targetType == *state.selectedPriority; } );
		if( row != squad->priorities.end() ) { currentAttitude = row->attitude; hasPriority = true; }
	}
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
	std::string selectedMemberName;
	if( state.selectedMember )
	{
		for( const auto& value : state.roster.squads )
		{
			const auto found = std::ranges::find_if( value.members,
				[&]( const SquadMemberRow& member ){ return member.id == *state.selectedMember; } );
			if( found != value.members.end() ) { selectedMemberName = found->name; break; }
		}
		if( selectedMemberName.empty() )
		{
			const auto found = std::ranges::find_if( state.roster.unassigned,
				[&]( const SquadMemberRow& member ){ return member.id == *state.selectedMember; } );
			if( found != state.roster.unassigned.end() ) selectedMemberName = found->name;
		}
	}
	text( military_, "role_member_assignment_status", selectedMemberName.empty()
		? "No citizen selected. Choose one from the Squads tab first."
		: "Selected: " + selectedMemberName );
	enabled( military_, "role_assign_member", hasRole && !selectedMemberName.empty() );
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
		uniforms += "<button id='military_uniform_" + std::string{ uniformSlotId( row.slot ) } + "' class='m6c-row c-list__row";
		if( state.selectedUniformSlot == row.slot ) uniforms += " is-selected";
		uniforms += "' data-uniform-slot='" + std::string{ uniformSlotId( row.slot ) } + "'><span class='c-list__primary'>" + esc( row.name )
			+ "</span><span class='c-list__meta'>" + esc( row.type.value ) + " / " + esc( row.material ? row.material->value : "any" ) + "</span></button>";
	}
	appendWindowControls( uniforms, RowSurface::UniformSlots, uniformWindow, uniformRows.size() );
	rml( military_, "military_uniform_rows", uniforms.empty() ? "<div class='c-state-panel'>No uniform slots reported.</div>" : uniforms );
	const UniformSlotRow* selectedSlot = nullptr;
	if( hasRole && state.selectedUniformSlot )
	{
		const auto found = std::ranges::find_if( role->uniform, [&]( const UniformSlotRow& row ){ return row.slot == *state.selectedUniformSlot; } );
		if( found != role->uniform.end() ) selectedSlot = &*found;
	}
	std::string types;
	std::string materials;
	if( selectedSlot )
	{
		const auto typeWindow = windowFor( RowSurface::UniformTypes, selectedSlot->possibleTypes.size(),
			selectedIndex( selectedSlot->possibleTypes, std::optional{ selectedSlot->type }, []( const CatalogId& value ){ return value; } ),
			selectedSlot->type.value );
		for( std::size_t index = typeWindow.begin; index < typeWindow.end; ++index )
		{
			const auto& type = selectedSlot->possibleTypes[index];
			types += "<button class='m6c-row c-list__row" + std::string{ type == selectedSlot->type ? " is-selected" : "" }
				+ "' data-uniform-type='" + esc( type.value ) + "'>" + esc( type.value ) + "</button>";
		}
		appendWindowControls( types, RowSurface::UniformTypes, typeWindow, selectedSlot->possibleTypes.size() );
		const auto materialWindow = windowFor( RowSurface::UniformMaterials, selectedSlot->possibleMaterials.size(),
			selectedIndex( selectedSlot->possibleMaterials, selectedSlot->material, []( const CatalogId& value ){ return value; } ),
			selectedSlot->material ? selectedSlot->material->value : std::string{} );
		for( std::size_t index = materialWindow.begin; index < materialWindow.end; ++index )
		{
			const auto& material = selectedSlot->possibleMaterials[index];
			materials += "<button class='m6c-row c-list__row" + std::string{ selectedSlot->material == material ? " is-selected" : "" }
				+ "' data-uniform-material='" + esc( material.value ) + "'>" + esc( material.value ) + "</button>";
		}
		appendWindowControls( materials, RowSurface::UniformMaterials, materialWindow, selectedSlot->possibleMaterials.size() );
	}
	rml( military_, "military_uniform_type_rows", types.empty() ? "<div class='c-state-panel'>No legal types reported.</div>" : types );
	rml( military_, "military_uniform_material_rows", materials.empty() ? "<div class='c-state-panel'>Choose a type to request materials.</div>" : materials );

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
	text( military_, "military_revision", "Squads r" + std::to_string( state.squadRevision.value ) + " / Roles r" + std::to_string( state.roleRevision.value ) );
	text( military_, "military_status", state.pendingAction
		? "Applying request " + std::to_string( state.pendingAction->value )
		: state.status.empty() && state.militaryLoad == LoadState::Stale ? "Refreshing stale military data" : state.status );
}

void Management6CRmlBinding::renderDiplomacy( const Management6CState& state )
{
	selected( diplomacy_, "diplomacy_tab_neighbors", state.view == View::Neighbors );
	selected( diplomacy_, "diplomacy_tab_missions", state.view == View::Missions );
	const bool hasNeighbors = !state.neighbors.empty();
	const bool hasMissions = !state.missions.empty();
	const auto load = state.view == View::Neighbors ? state.diplomacyLoad : state.missionLoad;
	const bool hasRows = state.view == View::Neighbors ? hasNeighbors : hasMissions;
	visible( diplomacy_, "diplomacy_loading", ( load == LoadState::Loading || load == LoadState::Stale ) && !hasRows );
	visible( diplomacy_, "diplomacy_empty", load == LoadState::Empty );
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
			+ esc( row.discovered && row.name ? *row.name : "Undiscovered neighbor" ) + "</span><span class='c-list__meta'>ID "
			+ std::to_string( row.id.value ) + "</span></button>";
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
		missions += "' data-mission='" + std::to_string( row.id.value ) + "'><span class='c-list__primary'>" + std::string{ missionTypeName( row.type ) }
			+ "</span><span class='c-list__meta'>" + missionStepName( row.step ) + "</span></button>";
	}
	appendWindowControls( missions, RowSurface::Missions, missionWindow, visibleMissionRows.size() );
	rml( diplomacy_, "diplomacy_mission_rows", missions.empty() ? "<div class='c-state-panel'>No missions match the filter.</div>" : missions );
	std::string selection = "Selection: ";
	if( state.view == View::Neighbors ) selection += state.selectedNeighbor ? "neighbor " + std::to_string( state.selectedNeighbor->value ) : "none";
	else selection += state.selectedMission ? "mission " + std::to_string( state.selectedMission->value ) : "none";
	if( state.diplomacySelectionHidden ) selection += " (preserved, hidden by filter)";
	text( diplomacy_, "diplomacy_selection_status", selection );
	if( auto* element = diplomacy_->GetElementById( "diplomacy_selection_status" ) ) element->SetClass( "m6c-hidden-selection", state.diplomacySelectionHidden );

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
	enabled( diplomacy_, "mission_type_spy", discovered && neighbor->canSpy );
	enabled( diplomacy_, "mission_type_emissary", discovered && neighbor->canSendEmissary );
	enabled( diplomacy_, "mission_type_raid", discovered && neighbor->canRaid );
	enabled( diplomacy_, "mission_type_sabotage", discovered && neighbor->canSabotage );
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
		gnomes += "' data-gnome='" + std::to_string( row.id.value ) + "'>" + esc( row.name ) + "</button>";
	}
	appendWindowControls( gnomes, RowSurface::Gnomes, gnomeWindow, state.availableGnomes.size() );
	rml( diplomacy_, "diplomacy_gnome_rows", gnomes.empty() ? "<div class='c-state-panel'>No eligible gnomes are available.</div>" : gnomes );
	enabled( diplomacy_, "mission_start", controller_->canStartDraftMission() );

	const auto mission = state.selectedMission ? std::ranges::find_if( state.missions,
		[&]( const MissionRow& row ){ return row.id == *state.selectedMission; } ) : state.missions.end();
	if( mission != state.missions.end() )
	{
		text( diplomacy_, "mission_title", missionTypeName( mission->type ) );
		text( diplomacy_, "mission_id", std::to_string( mission->id.value ) );
		text( diplomacy_, "mission_action", missionActionName( mission->action ) );
		text( diplomacy_, "mission_step", missionStepName( mission->step ) );
		text( diplomacy_, "mission_target", mission->target ? std::to_string( mission->target.value ) : "Not reported" );
		std::string participants;
		for( const auto& creature : mission->participants ) { if( !participants.empty() ) participants += ", "; participants += std::to_string( creature.value ); }
		text( diplomacy_, "mission_participants", participants.empty() ? "None reported" : participants );
		text( diplomacy_, "mission_timing", "Elapsed " + std::to_string( mission->elapsedHours ) + " hours; next check tick " + std::to_string( mission->nextCheckTick ) );
		std::string result = "Not reported";
		if( mission->result.success ) result = *mission->result.success ? "Success" : "Failure";
		if( mission->result.totalHours ) result += "; total " + std::to_string( *mission->result.totalHours ) + " hours";
		text( diplomacy_, "mission_result", result );
	}
	text( diplomacy_, "diplomacy_revision", "Neighbors r" + std::to_string( state.neighborRevision.value ) + " / Missions r" + std::to_string( state.missionRevision.value ) );
	text( diplomacy_, "diplomacy_status", state.pendingAction
		? "Applying request " + std::to_string( state.pendingAction->value )
		: state.status.empty() && load == LoadState::Stale ? "Refreshing stale diplomacy data" : state.status );
}

} // namespace ingnomia::ui::management6c
