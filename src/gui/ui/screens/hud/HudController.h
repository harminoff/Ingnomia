/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "HudState.h"
#include "../../../../base/enums.h"

namespace ingnomia::ui::hud
{
enum class CommandStatus : std::uint8_t { Accepted, Rejected };
struct CommandResult { CommandStatus status{ CommandStatus::Accepted }; bool pending{}; std::string error; };

class HudCommandPort
{
public:
	virtual ~HudCommandPort() = default;
	virtual CommandResult dispatch( const UiActionEnvelope& action ) = 0;
	virtual CommandResult requestBuildItems( BuildSelection, std::string_view ) { return { CommandStatus::Rejected, false, "ui.error.action_unavailable" }; }
	virtual void registerPrompt( PromptInstanceId, std::optional<EventResponseTargetId> ) {}
};

class HudViewPort
{
public:
	virtual ~HudViewPort() = default;
	virtual void stateChanged( const HudState& state ) = 0;
};

class HudController
{
public:
	HudController( HudCommandPort& commands, HudViewPort& view );
	[[nodiscard]] const HudState& state() const noexcept { return state_; }
	void beginWorld( WorldEpoch world );
	void endWorld();
	void setSettlement( SettlementSummary value );
	void setClock( ClockCalendarState value );
	void setCamera( CameraState value );
	void setOverlays( RenderOverlayState value );
	void setTool( ToolState tool, SelectionSummary selection = {} );
	void setWatchRows( std::vector<HudWatchRow> rows );
	void setBuildCatalog( std::vector<BuildCatalogRow> rows );
	void setTutorial( TutorialViewState value );
	void tutorialAdvance();
	void tutorialSkip();
	void tutorialRestart();
	void tutorialToggleHints();
	void tutorialFinish();
	bool enqueuePrompt( std::optional<EventResponseTargetId> target, std::string title,
		std::string body, bool requestsPause, bool yesNo );
	void setPaused( bool paused );
	void setSpeed( GameSpeed speed );
	void changeLevel( std::int32_t level );
	void setOverlay( OverlayKind overlay, bool enabled );
	void activateTool( ToolId tool );
	void cancelTool();
	void rotateTool();
	void chooseBuild( CatalogId item );
	void chooseBuildAction( CatalogId item, BuildAction action );
	void selectBuildMaterial( CatalogId item, std::uint32_t componentIndex, CatalogId material, bool notifyView = true );
	void requestBuildItems( BuildSelection selection, std::string_view category );
	void closeBuildMenu();
	void respondToPrompt( EventResponse response );
	void onActionFinished( RequestId request, CommandResult result );

private:
	bool dispatch( std::string_view id, UiActionPayload payload );
	void notify();
	HudCommandPort& commands_;
	HudViewPort& view_;
	HudState state_;
	std::uint64_t nextRequest_{ 1 };
	std::uint64_t nextPrompt_{ 1 };
};
} // namespace ingnomia::ui::hud
