/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "ShellRmlAdapter.h"

#include <array>
#include <utility>

namespace ingnomia::ui::shell
{

std::optional<ShellControl> ShellRmlAdapter::controlForElement( std::string_view elementId ) noexcept
{
	using Entry = std::pair<std::string_view, ShellControl>;
	static constexpr std::array entries{
		Entry{ "shell-continue", ShellControl::ContinueLastGame },
		Entry{ "shell-tutorial", ShellControl::StartTutorial },
		Entry{ "shell-new-default", ShellControl::StartDefaultGame }, Entry{ "shell-new-setup", ShellControl::OpenNewGame },
		Entry{ "shell-load", ShellControl::OpenLoadGame }, Entry{ "shell-settings", ShellControl::OpenSettings },
		Entry{ "shell-exit", ShellControl::Exit }, Entry{ "shell-back", ShellControl::Back },
		Entry{ "new-start", ShellControl::StartConfiguredGame }, Entry{ "new-random-name", ShellControl::RandomizeName },
		Entry{ "new-random-seed", ShellControl::RandomizeSeed }, Entry{ "load-refresh", ShellControl::RefreshLoads },
		Entry{ "load-selected", ShellControl::LoadSelected }, Entry{ "settings-apply", ShellControl::ApplySettings },
		Entry{ "settings-revert", ShellControl::RevertSettings }, Entry{ "settings-reset", ShellControl::ResetSettings },
		Entry{ "pause-resume", ShellControl::Resume }, Entry{ "pause-save", ShellControl::Save },
		Entry{ "pause-load", ShellControl::LoadFromPause }, Entry{ "pause-settings", ShellControl::SettingsFromPause },
		Entry{ "pause-menu", ShellControl::ReturnToMenu }, Entry{ "confirm-cancel", ShellControl::CancelDestructive },
		Entry{ "confirm-accept", ShellControl::ConfirmDestructive }, Entry{ "loading-retry", ShellControl::Retry },
	};
	for( const auto& [id, control] : entries ) if( id == elementId ) return control;
	return std::nullopt;
}

std::string_view ShellRmlAdapter::initialFocusForRoute( std::string_view route ) noexcept
{
	if( route == "shell.main_menu" ) return "shell-continue";
	if( route == "shell.new_game" ) return "new-kingdom-name";
	if( route == "shell.load_game" ) return "load-refresh";
	if( route == "shell.settings" || route == "game.settings" ) return "shell-back";
	if( route == "shell.loading" ) return "loading-heading";
	if( route == "game.pause" ) return "pause-resume";
	return {};
}

} // namespace ingnomia::ui::shell
