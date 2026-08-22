/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "ShellState.h"

namespace ingnomia::ui::shell
{

// Value-only input at the Qt/aggregator boundary. Absolute save paths are deliberately absent.
struct UpstreamSettingsSnapshot
{
	bool fullscreen{};
	bool followMonitorRefresh{ true };
	std::int32_t frameRateLimit{ 60 };
	float uiScale{ 1.0f };
	std::int32_t keyboardPanSpeed{ 20 };
	std::int32_t minimumLightPercent{ 30 };
	bool wheelChangesLevel{};
};

class ShellDataAdapter
{
public:
	[[nodiscard]] static SettingsState settings( const UpstreamSettingsSnapshot& source );
	[[nodiscard]] static LoadGameState saves( std::vector<SaveKingdomRow> kingdoms,
		std::vector<SaveSlotRow> saves );
};

} // namespace ingnomia::ui::shell
