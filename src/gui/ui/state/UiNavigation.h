/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "UiFoundationTypes.h"

#include <optional>
#include <span>

namespace ingnomia::ui
{

enum class NavigationResult : std::uint8_t
{
	Changed,
	Unchanged,
	UnknownRoute,
	UnavailableRoute,
	IllegalContext,
	CannotClosePrimary,
	NotTopmost,
	AlreadyOpen
};

struct NavigationMutation
{
	NavigationResult result{ NavigationResult::Unchanged };
	DirtySet dirty;

	NavigationMutation() = default;
	NavigationMutation( NavigationResult resultValue ) : result( resultValue ) {}
	NavigationMutation( NavigationResult resultValue, DirtySet dirtyValue )
		: result( resultValue ), dirty( std::move( dirtyValue ) ) {}

	[[nodiscard]] bool accepted() const noexcept
	{
		return result == NavigationResult::Changed || result == NavigationResult::Unchanged;
	}
};

class UiRouter
{
public:
	explicit UiRouter( bool developmentBuild = false );

	[[nodiscard]] const NavigationState& state() const noexcept { return state_; }
	[[nodiscard]] NavigationMutation open( const RouteId& route );
	[[nodiscard]] NavigationMutation close( const RouteId& route );
	[[nodiscard]] NavigationMutation back();

private:
	[[nodiscard]] bool gameContext() const noexcept;
	[[nodiscard]] NavigationMutation changed();

	bool developmentBuild_{};
	NavigationState state_;
};

enum class ModalResult : std::uint8_t
{
	Changed,
	Unchanged,
	InvalidId,
	DuplicateId,
	InvalidDocument,
	MissingInputBlocker,
	NotTopmost,
	NotDismissible
};

struct ModalMutation
{
	ModalResult result{ ModalResult::Unchanged };
	DirtySet dirty;
	std::optional<FocusToken> restoreFocus;

	ModalMutation() = default;
	ModalMutation( ModalResult resultValue ) : result( resultValue ) {}
	ModalMutation( ModalResult resultValue, DirtySet dirtyValue,
		std::optional<FocusToken> restoreFocusValue = std::nullopt )
		: result( resultValue ), dirty( std::move( dirtyValue ) ),
		  restoreFocus( restoreFocusValue ) {}

	[[nodiscard]] bool accepted() const noexcept
	{
		return result == ModalResult::Changed || result == ModalResult::Unchanged;
	}
};

class ModalController
{
public:
	[[nodiscard]] std::span<const ModalEntry> stack() const noexcept { return stack_; }
	[[nodiscard]] const ModalEntry* top() const noexcept;
	[[nodiscard]] bool blocksWorldInput() const noexcept;
	[[nodiscard]] bool canInteract( ModalInstanceId id ) const noexcept;

	[[nodiscard]] ModalMutation push( ModalEntry entry );
	[[nodiscard]] ModalMutation closeTop( ModalInstanceId id );
	[[nodiscard]] ModalMutation dismissTopOnEscape();
	[[nodiscard]] ModalMutation clear();

private:
	[[nodiscard]] static bool documentMatchesKind( const ModalEntry& entry ) noexcept;
	[[nodiscard]] static ModalMutation changed( const ModalEntry* removed = nullptr );

	std::vector<ModalEntry> stack_;
};

} // namespace ingnomia::ui
