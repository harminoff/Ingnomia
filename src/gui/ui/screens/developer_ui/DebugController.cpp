/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "DebugController.h"

#include <algorithm>
#include <cctype>
namespace ingnomia::ui::debug
{
DebugController::DebugController( bool enabled, CommandPort& c, ViewPort& v ) :
	commands_( c ), view_( v )
{
	state_.enabled = enabled;
	notify( false );
}
void DebugController::notify( bool rendered )
{
	++state_.revision.value;
	++state_.counters.notifications;
	if ( rendered )
		++state_.counters.renderedNotifications;
	view_.stateChanged( state_ );
}
void DebugController::beginWorld( WorldEpoch w )
{
	const bool enabled = state_.enabled;
	state_             = {};
	state_.enabled     = enabled;
	state_.world       = w;
	notify();
}
void DebugController::endWorld()
{
	const bool enabled = state_.enabled;
	state_             = {};
	state_.enabled     = enabled;
	gnomesRevision_    = {};
	catalogRevision_   = {};
	notify();
}
void DebugController::open()
{
	if ( !state_.enabled || !state_.world )
		return;
	state_.open = true;
	notify();
	requestGnomes();
}
void DebugController::close()
{
	state_.open = false;
	notify();
}
void DebugController::setPage( Page p )
{
	if ( !state_.enabled )
		return;
	state_.page = p;
	notify();
}
void DebugController::setSearch( std::string q )
{
	state_.search = std::move( q );
	notify();
}
std::vector<NamedId> DebugController::visibleGnomes() const
{
	auto rows     = state_.gnomes;
	std::string q = state_.search;
	std::ranges::transform( q, q.begin(), []( unsigned char c )
							{ return (char)std::tolower( c ); } );
	std::erase_if( rows, [&]( const auto& r )
				   {auto n=r.name;std::ranges::transform(n,n.begin(),[](unsigned char c){return(char)std::tolower(c);});return !q.empty()&&n.find(q)==std::string::npos; } );
	return rows;
}
bool DebugController::accepts( WorldEpoch w, Revision r )
{
	if ( w != state_.world )
	{
		++state_.counters.staleEpochRejects;
		return false;
	}
	(void)r;
	return true;
}
bool DebugController::applyGnomes( WorldEpoch w, Revision r, std::vector<NamedId> rows )
{
	if ( !accepts( w, r ) )
		return false;
	if ( !r || r.value <= gnomesRevision_.value )
	{
		++state_.counters.staleRevisionRejects;
		return false;
	}
	state_.gnomes   = std::move( rows );
	gnomesRevision_ = r;
	++state_.counters.fullSnapshots;
	notify();
	return true;
}
bool DebugController::applyCatalog( WorldEpoch w, Revision r, ItemCatalog c )
{
	if ( !accepts( w, r ) )
		return false;
	if ( !r || r.value <= catalogRevision_.value )
	{
		++state_.counters.staleRevisionRejects;
		return false;
	}
	state_.catalog   = std::move( c );
	catalogRevision_ = r;
	++state_.counters.fullSnapshots;
	notify();
	return true;
}
void DebugController::selectGnome( std::uint32_t id )
{
	if ( std::ranges::any_of( state_.gnomes, [&]( const auto& r )
							  { return r.id == id; } ) )
	{
		state_.selectedGnome = id;
		notify();
	}
}
void DebugController::moveGnomeSelection( std::int32_t delta )
{
	const auto rows = visibleGnomes();
	if ( rows.empty() )
		return;
	const auto current   = state_.selectedGnome ? std::ranges::find_if( rows, [&]( const auto& r )
																		{ return r.id == *state_.selectedGnome; } )
												: rows.end();
	const auto index     = current == rows.end() ? 0 : static_cast<std::int64_t>( current - rows.begin() );
	const auto next      = std::clamp<std::int64_t>( index + delta, 0, static_cast<std::int64_t>( rows.size() - 1 ) );
	state_.selectedGnome = rows[static_cast<std::size_t>( next )].id;
	notify();
}
void DebugController::requestGnomes()
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::RequestGnomes } );
}
void DebugController::requestGroups()
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::RequestGroups } );
}
void DebugController::requestItems( std::string group )
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::RequestItems, 0, std::move( group ) } );
}
void DebugController::requestMaterials( std::string item )
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::RequestMaterials, 0, std::move( item ) } );
}
void DebugController::spawnCreature( std::string type )
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::SpawnCreature, 0, std::move( type ) } );
}
void DebugController::setNeed( std::uint32_t id, std::string need, float value )
{
	if ( state_.enabled && state_.world )
	{
		DebugAction a { ActionKind::SetNeed, id, std::move( need ) };
		a.value = value;
		commands_.dispatch( a );
	}
}
void DebugController::killGnome( std::uint32_t id )
{
	if ( state_.enabled && state_.world )
		commands_.dispatch( { ActionKind::KillGnome, id } );
}
void DebugController::spawnItem( std::string item, std::vector<std::string> materials, std::int32_t count, std::int32_t x, std::int32_t y, std::int32_t z )
{
	if ( state_.enabled && state_.world )
	{
		DebugAction a { ActionKind::SpawnItem };
		a.primary   = std::move( item );
		a.materials = std::move( materials );
		a.count     = count;
		a.x         = x;
		a.y         = y;
		a.z         = z;
		commands_.dispatch( a );
	}
}
void DebugController::setWindowSize( std::int32_t width, std::int32_t height )
{
	if ( state_.enabled )
	{
		DebugAction a { ActionKind::SetWindowSize };
		a.width  = width;
		a.height = height;
		commands_.dispatch( a );
	}
}
void DebugController::setNeedDecayMultiplier( float value )
{
	if ( state_.enabled && state_.world )
	{
		DebugAction a { ActionKind::SetNeedDecayMultiplier };
		a.value = value;
		commands_.dispatch( a );
	}
}
void DebugController::setDisableNeedDecay( std::string need, bool enabled )
{
	if ( state_.enabled && state_.world )
	{
		DebugAction a { ActionKind::SetDisableNeedDecay };
		a.primary = std::move( need );
		a.enabled = enabled;
		commands_.dispatch( a );
	}
}
void DebugController::instrumentDirty( std::size_t count, bool snapshot, bool patch )
{
	state_.counters.dirtyBindings += count;
	state_.counters.fullSnapshots += snapshot;
	state_.counters.rowPatches += patch;
	notify();
}
} // namespace ingnomia::ui::debug
