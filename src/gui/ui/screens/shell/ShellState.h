/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "../../actions/UiActions.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ingnomia::ui::shell
{

enum class RequestStatus : std::uint8_t { Idle, Loading, Ready, Empty, Error };

struct Message
{
	LocalizationKey key;
	std::vector<std::string> arguments;
};

struct ShellError
{
	Message message;
	bool retryable{};
	std::optional<UiActionEnvelope> retryAction;
};

struct SaveKingdomRow
{
	SaveKingdomId id;
	std::string displayName;
	std::int64_t modifiedUtcSeconds{};
	bool operator==( const SaveKingdomRow& ) const = default;
};

struct SaveSlotRow
{
	SaveSlotId id;
	std::string displayName;
	std::string version;
	std::int64_t modifiedUtcSeconds{};
	bool compatible{};
	bool operator==( const SaveSlotRow& ) const = default;
};

struct LoadGameState
{
	RequestStatus kingdomsStatus{ RequestStatus::Idle };
	RequestStatus savesStatus{ RequestStatus::Idle };
	std::vector<SaveKingdomRow> kingdoms;
	std::vector<SaveSlotRow> saves;
	std::optional<SaveKingdomId> selectedKingdom;
	std::optional<SaveSlotId> selectedSlot;
	std::optional<ShellError> error;
};

enum class SettingControl : std::uint8_t { Toggle, Integer, Decimal };
enum class SettingApplyMode : std::uint8_t { Immediate, OnApply };

struct SettingRow
{
	SettingId id;
	LocalizationKey labelKey;
	LocalizationKey descriptionKey;
	SettingControl control{ SettingControl::Toggle };
	SettingApplyMode applyMode{ SettingApplyMode::Immediate };
	SettingValue authoritative;
	SettingValue draft;
	std::optional<SettingValue> minimum;
	std::optional<SettingValue> maximum;
	std::optional<Message> validationError;
	bool operator==( const SettingRow& ) const = default;
};

struct SettingsState
{
	RequestStatus status{ RequestStatus::Idle };
	std::vector<SettingRow> rows;
	std::optional<ShellError> error;
};

struct NewGameState
{
	RequestStatus status{ RequestStatus::Idle };
	NewGameDraft acceptedDraft;
	NewGameDraft draft;
	std::vector<Message> validationErrors;
};

struct LifecycleView
{
	WorldPhase phase{ WorldPhase::NoWorld };
	std::optional<Message> progress;
	// The generator reports authoritative human-readable steps. Keep the raw
	// value alongside the localized fallback so the loading surface can show
	// real progress without inventing a localization key for every step.
	std::string progressText;
	std::optional<ShellError> error;
	bool blocksWorldInput{};
};

struct ShellState
{
	RouteId route{ "shell.main_menu" };
	std::string version;
	bool reducedMotion{};
	bool continueAvailable{};
	std::string continueSaveName;
	std::string continueSavedAt;
	std::optional<Message> continueBlocker;
	LoadGameState loadGame;
	SettingsState settings;
	NewGameState newGame;
	LifecycleView lifecycle;
	std::optional<RequestId> pendingRequest;
	// Save completion is reported by the authoritative game-thread bridge.
	RequestStatus saveStatus{ RequestStatus::Idle };
	std::optional<ShellError> actionError;
	bool authoritativePaused{};
	PauseReason pauseReason{ PauseReason::None };
};

} // namespace ingnomia::ui::shell
