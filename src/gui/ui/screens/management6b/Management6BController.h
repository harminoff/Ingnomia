/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "Management6BState.h"

namespace ingnomia::ui::management6b
{
enum class CommandStatus : std::uint8_t
{
	Accepted,
	Rejected
};
struct CommandResult
{
	CommandStatus status { CommandStatus::Accepted };
	bool pending {};
	std::string error;
};
class CommandPort
{
public:
	virtual ~CommandPort()                                    = default;
	virtual CommandResult dispatch( const UiActionEnvelope& ) = 0;
};
class ViewPort
{
public:
	virtual ~ViewPort()                                   = default;
	virtual void stateChanged( const Management6BState& ) = 0;
};

class Management6BController
{
public:
	Management6BController( CommandPort&, ViewPort& );
	void addViewPort( ViewPort& );
	void removeViewPort( ViewPort& );
	[[nodiscard]] const Management6BState& state() const noexcept
	{
		return state_;
	}
	void beginWorld( WorldEpoch );
	void endWorld();
	void open( View );
	void openPopulation()
	{
		open( View::Citizens );
	}
	void openInventory()
	{
		open( View::Inventory );
	}
	void close();
	void closePopulation();
	void closeInventory();
	void refresh();
	void inventoryChanged();
	void setPopulationFilter( std::string );
	void setInventoryFilter( std::string );
	void setInventoryOwnedOnly( bool );
	void setInventoryCategory( std::string );
	void toggleInventoryExpanded( InventoryRowId );
	[[nodiscard]] bool inventoryExpanded( const InventoryRowId& ) const;
	void setPopulationSort( Sort );
	void setInventorySort( Sort );
	[[nodiscard]] std::vector<PopulationRow> visiblePopulation() const;
	[[nodiscard]] std::vector<InventoryRow> visibleInventory() const;
	[[nodiscard]] std::vector<PopulationRow> populationPage() const;
	[[nodiscard]] std::vector<InventoryRow> inventoryPage() const;
	void changePopulationPage( std::int32_t );
	void changeInventoryPage( std::int32_t );
	void selectCreature( CreatureId );
	void selectInventory( InventoryRowId );
	void movePopulationSelection( std::int32_t );
	void moveInventorySelection( std::int32_t );
	void selectScheduleCell( ScheduleCellId );
	void moveScheduleFocus( std::int32_t hourDelta, std::int32_t rowDelta );
	bool applyPopulation( Snapshot<std::vector<PopulationRow>> );
	bool applyPopulationPatch( RowPatch<PopulationRow> );
	bool applyProfessions( Snapshot<std::vector<ProfessionRow>> );
	bool applyProfessionSkills( WorldEpoch, ProfessionId, std::vector<CatalogId> );
	bool applySchedules( Snapshot<std::vector<ScheduleRow>> );
	bool applySchedulePatch( RowPatch<ScheduleRow> );
	bool applyInventory( Snapshot<std::vector<InventoryRow>> );
	bool applyInventoryPatch( RowPatch<InventoryRow> );
	bool applyCreature( Snapshot<CreatureDetail> );
	void setSkill( CreatureId, CatalogId, bool );
	void setAllSkills( CreatureId, bool );
	void setSkillForAll( CatalogId, bool );
	void setProfession( CreatureId, ProfessionId );
	void updateProfession( ProfessionId, std::string, std::vector<CatalogId> );
	void setScheduleCell( CreatureId, std::uint8_t, ScheduleActivity );
	void setScheduleRow( CreatureId, ScheduleActivity );
	void setScheduleColumn( std::uint8_t, ScheduleActivity );
	void activateScheduleCell( ScheduleActivity );
	void setWatched( InventoryRowId, bool );
	void toggleSelectedWatch();
	void requestSelectedInventoryHistory();
	bool applyInventoryHistory( WorldEpoch, InventoryRowId, std::vector<InventoryHistoryPoint> );
	void onActionFinished( RequestId, CommandResult );

private:
	bool dispatch( std::string_view, UiActionPayload );
	bool accepts( WorldEpoch, Revision incoming, Revision current ) const;
	void requestPopulationRefresh();
	void requestInventoryRefresh();
	void collapseInventorySections();
	void reconcileSelection();
	void notify();
	CommandPort& commands_;
	std::vector<ViewPort*> views_;
	Management6BState state_;
	bool inventoryExpansionInitialized_ {};
	std::uint64_t nextRequest_ { 1 };
};
} // namespace ingnomia::ui::management6b
