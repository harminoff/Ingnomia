/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../state/UiFoundationTypes.h"
#include <optional>
#include <string_view>
namespace ingnomia::ui::hud
{
class LegacyToolActionAdapter
{
public:
	[[nodiscard]] static std::optional<std::string_view> action( const ToolId& tool ) noexcept;
};
}
