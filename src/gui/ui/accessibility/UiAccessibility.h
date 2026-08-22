/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace ingnomia::ui::accessibility
{
struct Preferences { bool highContrast{}; bool reducedMotion{}; float scale{1.0f}; };
enum class EscapeLayer:std::uint8_t{None,Composition,DismissibleModal,Overlay,Workbench,Dock,ActiveTool,Game};
struct EscapeContext{bool composing{};bool modal{};bool modalDismissible{};bool overlay{};bool workbench{};bool activeTool{};bool dock{};};
[[nodiscard]] EscapeLayer escapeTarget(const EscapeContext&) noexcept;
[[nodiscard]] float clampScale(float) noexcept;
struct FocusToken{std::string document;std::string element;auto operator<=>(const FocusToken&)const=default;};
class FocusHistory{public:void remember(FocusToken);[[nodiscard]]std::optional<FocusToken>take(std::string_view);void clear();private:std::optional<FocusToken>token_;};
}
