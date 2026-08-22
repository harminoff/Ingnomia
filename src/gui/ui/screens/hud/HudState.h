/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <deque>
#include <optional>

namespace ingnomia::ui::hud
{
struct HudWatchRow
{
	InventoryRowId id;
	std::string label;
	std::uint32_t count{};
	bool operator==( const HudWatchRow& ) const = default;
};

enum class EventResponseKind : std::uint8_t { Acknowledge, YesNo };
struct EventPrompt
{
	PromptInstanceId instanceId;
	std::optional<EventResponseTargetId> responseTarget;
	std::string title;
	std::string body;
	EventResponseKind responses{ EventResponseKind::Acknowledge };
	bool requestsPause{};
	bool answered{};
	bool operator==( const EventPrompt& ) const = default;
};

struct BuildCatalogRow
{
	struct MaterialOption
	{
		CatalogId id;
		std::int32_t available{};
		bool operator==( const MaterialOption& ) const = default;
	};
	struct RequiredComponent
	{
		CatalogId item;
		std::int32_t amount{};
		std::vector<MaterialOption> options;
		CatalogId selected;
		bool operator==( const RequiredComponent& ) const = default;
	};
	CatalogId id;
	std::string name;
    BuildKind kind{ BuildKind::Item };
    std::vector<CatalogId> defaultMaterials;
	std::vector<RequiredComponent> components;
    std::string spriteSheet;
    int spriteX{};
    int spriteY{};
    int spriteWidth{};
    int spriteHeight{};
	int spriteSheetWidth{};
	int spriteSheetHeight{};
	bool available{ true };
	std::string unavailableReason;
	bool operator==( const BuildCatalogRow& ) const = default;
};

struct TutorialViewState
{
	bool active{};
	std::uint8_t step{};
	std::uint8_t stepCount{ 9 };
	std::uint32_t completedMask{};
	std::uint32_t skippedMask{};
	bool hintsEnabled{};
	bool pausedForLesson{};
	bool completed{};
	bool incompatible{};
	std::string scenarioId;
	std::string title;
	std::string explanation;
	std::string objective;
	std::vector<std::string> steps;
	std::vector<bool> completedSteps;
	std::string progress;
	std::vector<std::string> highlightedIds;
	bool operator==( const TutorialViewState& ) const = default;
};

struct HudState
{
	WorldEpoch world;
	bool acceptsWorldActions{};
	SettlementSummary settlement;
	ClockCalendarState clock;
	CameraState camera;
	RenderOverlayState overlays;
	ToolState tool;
	SelectionSummary selection;
	std::vector<HudWatchRow> watchRows;
	std::vector<BuildCatalogRow> buildCatalog;
	std::deque<EventPrompt> prompts;
	std::optional<RequestId> pendingAction;
	std::string status;
	TutorialViewState tutorial;
	bool operator==( const HudState& ) const = default;
};
} // namespace ingnomia::ui::hud
