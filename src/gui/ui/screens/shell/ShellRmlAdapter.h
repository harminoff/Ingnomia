/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "ShellController.h"

#include <optional>
#include <string_view>

namespace ingnomia::ui::shell
{

// The RML contains only stable element IDs. This adapter turns those IDs into a closed
// callback enum before the controller can create an ActionId or payload.
class ShellRmlAdapter
{
public:
	[[nodiscard]] static std::optional<ShellControl> controlForElement( std::string_view elementId ) noexcept;
	[[nodiscard]] static std::string_view initialFocusForRoute( std::string_view route ) noexcept;
};

} // namespace ingnomia::ui::shell
