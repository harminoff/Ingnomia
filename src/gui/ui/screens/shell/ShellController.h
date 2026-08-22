/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "ShellState.h"

#include <optional>

namespace ingnomia::ui::shell
{

enum class CommandStatus : std::uint8_t { Accepted, Rejected };

struct CommandResult
{
	CommandStatus status{ CommandStatus::Accepted };
	std::optional<ShellError> error;
	bool pending{};
};

class ShellCommandPort
{
public:
	virtual ~ShellCommandPort() = default;
	virtual CommandResult dispatch( const UiActionEnvelope& action ) = 0;
};

class ShellViewPort
{
public:
	virtual ~ShellViewPort() = default;
	virtual void stateChanged( const ShellState& state ) = 0;
	virtual void showConfirmation( const Message& title, const Message& detail,
		FocusToken returnFocus ) = 0;
	virtual void closeConfirmation() = 0;
	virtual void restoreFocus( FocusToken token ) = 0;
};

enum class ShellControl : std::uint8_t
{
	ContinueLastGame,
	StartDefaultGame,
	StartTutorial,
	OpenNewGame,
	OpenLoadGame,
	OpenSettings,
	Exit,
	Back,
	StartConfiguredGame,
	RefreshLoads,
	LoadSelected,
	RandomizeName,
	RandomizeSeed,
	ApplySettings,
	RevertSettings,
	ResetSettings,
	OpenPause,
	Resume,
	Save,
	LoadFromPause,
	SettingsFromPause,
	ReturnToMenu,
	ConfirmDestructive,
	CancelDestructive,
	Retry
};

class ShellController
{
public:
	ShellController( ShellCommandPort& commands, ShellViewPort& view );

	[[nodiscard]] const ShellState& state() const noexcept { return state_; }
	void setVersion( std::string version );
	void setReducedMotion( bool enabled );
	void setContinueAvailability( bool available, std::optional<Message> blocker = std::nullopt );
	void setNewGameState( NewGameState state );
	void setLoadGameState( LoadGameState state );
	void setSettingsState( SettingsState state );
	void setLifecycle( LifecycleView state );
	void beginWorldTransition( bool generating, std::optional<UiActionEnvelope> retryAction = std::nullopt );
	void setLifecycleProgress( std::string progress );
	void finishWorldTransition( bool success );
	void endWorld();
	void setWorld( WorldEpoch world );
	void onPauseState( bool paused, PauseReason reason );
	void onSaveGameFinished( bool success );
	void onActionFinished( RequestId request, CommandResult result );
	/** Route Escape through the shell when the current game route owns it. */
	[[nodiscard]] bool handleEscape();

	void activate( ShellControl control, FocusToken sourceFocus = {} );
	void selectKingdom( SaveKingdomId id );
	void selectSave( SaveSlotId id );
	void moveKingdomSelection( int delta );
	void moveSaveSelection( int delta );
	void setSettingDraft( SettingId id, SettingValue value );
	void setNewGameField( NewGameFieldId id, NewGameScalarValue value );

private:
	struct PendingConfirmation
	{
		UiActionEnvelope action;
		Message title;
		Message detail;
		FocusToken returnFocus;
		std::optional<RouteId> routeAfterAccepted;
	};

	[[nodiscard]] UiActionEnvelope action( std::string_view id, UiActionPayload payload,
		bool worldAction = false );
	bool dispatch( UiActionEnvelope envelope );
	void navigate( std::string_view route );
	void back();
	void requestConfirmation( UiActionEnvelope action, Message title, Message detail,
		FocusToken returnFocus, std::optional<RouteId> routeAfterAccepted = std::nullopt );
	void notify();

	ShellCommandPort& commands_;
	ShellViewPort& view_;
	ShellState state_;
	WorldEpoch world_{};
	std::uint64_t nextRequest_{ 1 };
	std::optional<PendingConfirmation> confirmation_;
	std::optional<RouteId> routeAfterConfirmedAction_;
	std::optional<UiActionEnvelope> retryAction_;
	bool closePauseWhenUnpaused_{};
	std::optional<RequestId> pendingPauseRequest_;
	std::optional<bool> pendingPauseValue_;
	std::optional<RequestId> pendingSaveRequest_;
};

} // namespace ingnomia::ui::shell
