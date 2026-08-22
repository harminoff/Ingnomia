/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "DebugState.h"
namespace ingnomia::ui::debug
{
class CommandPort
{
public:
	virtual ~CommandPort()                      = default;
	virtual bool dispatch( const DebugAction& ) = 0;
};
class ViewPort
{
public:
	virtual ~ViewPort()                            = default;
	virtual void stateChanged( const DebugState& ) = 0;
};
class DebugController
{
public:
	DebugController( bool, CommandPort&, ViewPort& );
	const DebugState& state() const
	{
		return state_;
	}
	void beginWorld( WorldEpoch );
	void endWorld();
	void open();
	void close();
	void setPage( Page );
	void setSearch( std::string );
	std::vector<NamedId> visibleGnomes() const;
	bool applyGnomes( WorldEpoch, Revision, std::vector<NamedId> );
	bool applyCatalog( WorldEpoch, Revision, ItemCatalog );
	void selectGnome( std::uint32_t );
	void moveGnomeSelection( std::int32_t );
	void requestGnomes();
	void requestGroups();
	void requestItems( std::string );
	void requestMaterials( std::string );
	void spawnCreature( std::string );
	void setNeed( std::uint32_t, std::string, float );
	void killGnome( std::uint32_t );
	void spawnItem( std::string, std::vector<std::string>, std::int32_t, std::int32_t, std::int32_t, std::int32_t );
	void setWindowSize( std::int32_t, std::int32_t );
	void setNeedDecayMultiplier( float );
	void setDisableNeedDecay( std::string, bool );
	void instrumentDirty( std::size_t, bool, bool );

private:
	void notify( bool = true );
	bool accepts( WorldEpoch, Revision );
	CommandPort& commands_;
	ViewPort& view_;
	DebugState state_;
	Revision gnomesRevision_, catalogRevision_;
};
} // namespace ingnomia::ui::debug
