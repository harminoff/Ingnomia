/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>

/** The kind of world requested by the shell.  Normal remains the default. */
enum class GameStartMode : std::uint8_t
{
	Normal,
	InteractiveTutorial
};

enum class TutorialMode : std::uint8_t
{
	Off,
	Interactive
};

enum class TutorialStepId : std::uint8_t
{
	Orientation,
	InspectAndAssign,
	Gathering,
	MiningAndLevels,
	Stockpile,
	Crafting,
	Farming,
	Shelter,
	Graduation
};

enum class TutorialFact : std::uint8_t
{
	Pan,
	Zoom,
	Rotate,
	ChangeLevel,
	PauseResume,
	ChangeSpeed,
	SelectGnome,
	InspectGnome,
	OpenPopulation,
	OpenInventory,
	AssignWork,
	Shelter,
	Beds,
	Stockpile,
	Excavation,
	LowerLevel,
	Farm,
	CropSelected,
	Plant,
	Harvest,
	Workshop,
	Plank,
	Kitchen,
	Flour,
	Bread,
	Migration,
	FellTree,
	OpenWorkshop,
	QueuePlank,
	MineWall,
	WheelLevel,
	CropQueued,
	Count
};

struct TutorialProgress
{
	TutorialMode mode{ TutorialMode::Off };
	std::string scenarioId{ "scenario_v2" };
	std::uint32_t scenarioVersion{ 2 };
	TutorialStepId step{ TutorialStepId::Orientation };
	std::uint32_t completedMask{};
	std::uint32_t skippedMask{};
	bool hintsEnabled{ true };
	bool completed{};
};

inline constexpr std::uint32_t tutorialBit( TutorialStepId step ) noexcept
{
	return 1u << static_cast<std::uint8_t>( step );
}

inline constexpr std::uint32_t tutorialFactBit( TutorialFact fact ) noexcept
{
	return 1u << static_cast<std::uint8_t>( fact );
}
