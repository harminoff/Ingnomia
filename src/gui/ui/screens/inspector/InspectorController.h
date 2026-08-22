/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "InspectorState.h"

namespace ingnomia::ui::inspector
{
enum class CommandStatus : std::uint8_t { Accepted, Rejected };
struct CommandResult { CommandStatus status{ CommandStatus::Accepted }; bool pending{}; std::string error; };
class InspectorCommandPort { public: virtual ~InspectorCommandPort() = default; virtual CommandResult dispatch( const UiActionEnvelope& ) = 0; };
class InspectorViewPort { public: virtual ~InspectorViewPort() = default; virtual void stateChanged( const InspectorState& ) = 0; };

class InspectorController
{
public:
	InspectorController( InspectorCommandPort&, InspectorViewPort& );
	[[nodiscard]] const InspectorState& state() const noexcept { return state_; }
	void beginWorld( WorldEpoch );
	void endWorld();
	void showTile( TileInspectorState );
	void showCreature( CreatureInspectorState, std::optional<WorldPosition> position = {} );
	void setProfessionChoices( std::vector<std::string> );
	void setProfession( std::string );
	void showWorkshop( WorkshopInspectorState );
	void showStockpile( StockpileInspectorState );
	void showAgriculture( AgricultureInspectorState );
	void setSelectionAction( std::string action, bool canRotate = false );
	void setSelectionCursor( std::optional<WorldPosition> );
	void setSelectionAnchor( std::optional<WorldPosition> );
	void setSelectionSize( std::string );
	void setSelectionPointer( std::optional<PointerPosition> );
	void openCreature( CreatureId );
	void toggleCreatureDetails();
	void executeContext( TileContextAction );
	void toggleWorkshopSuspended();
	void toggleStockpileSuspended();
	void toggleAgricultureSuspended();
	void toggleAgriculturePrimaryOption();
	void refresh();
	void locate();
	void cancelSelection();
	void rotateSelection();
	void close();
	void back();
	void onActionFinished( RequestId, CommandResult );
private:
	bool dispatch( std::string_view, UiActionPayload );
	void select( EntityRef, InspectorKind );
	void notify();
	InspectorCommandPort& commands_;
	InspectorViewPort& view_;
	InspectorState state_;
	std::uint64_t nextRequest_{ 1 };
};
} // namespace ingnomia::ui::inspector
