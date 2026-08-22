/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

namespace ingnomia::ui::management6c
{

inline constexpr std::size_t MaximumDynamicRowsPerList = 64;

struct DomWindow
{
	std::size_t begin{}, end{};
	[[nodiscard]] std::size_t size() const noexcept { return end - begin; }
	[[nodiscard]] bool hasPrevious() const noexcept { return begin != 0; }
	[[nodiscard]] bool hasNext( std::size_t count ) const noexcept { return end < count; }
};

[[nodiscard]] inline DomWindow boundedDomWindow( std::size_t count, std::optional<std::size_t> selectedIndex,
	std::size_t requestedBegin, bool manualPage ) noexcept
{
	if( count == 0 ) return {};
	const auto maximumBegin = count > MaximumDynamicRowsPerList ? count - MaximumDynamicRowsPerList : 0;
	std::size_t begin = std::min( requestedBegin, maximumBegin );
	if( !manualPage && selectedIndex && *selectedIndex < count )
	{
		const auto halfWindow = MaximumDynamicRowsPerList / 2;
		begin = *selectedIndex > halfWindow ? *selectedIndex - halfWindow : 0;
		begin = std::min( begin, maximumBegin );
	}
	return { begin, std::min( count, begin + MaximumDynamicRowsPerList ) };
}

} // namespace ingnomia::ui::management6c
