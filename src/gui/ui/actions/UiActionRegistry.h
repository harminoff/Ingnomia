/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "UiActions.h"

#include <array>
#include <span>
#include <type_traits>

namespace ingnomia::ui
{

enum class ActionScope : std::uint8_t
{
	Application,
	Presentation,
	World
};

struct ActionDefinition
{
	std::string_view id;
	ActionScope scope{ ActionScope::Presentation };
	std::array<std::size_t, 2> payloadIndexes{};
	std::uint8_t payloadCount{ 1 };
	bool requiresConfirmation{};
	bool requiresExpectedRevision{};

	[[nodiscard]] bool acceptsPayload( std::size_t index ) const noexcept;
};

enum class ActionValidationCode : std::uint8_t
{
	Valid,
	UnknownAction,
	InvalidRequest,
	PayloadMismatch,
	InvalidPayload,
	MissingWorld,
	StaleWorld,
	WorldUnavailable,
	StaleRevision,
	MissingExpectedRevision,
	IllegalContext,
	BlockedByModal,
	ConfirmationRequired
};

struct ActionValidation
{
	ActionValidationCode code{ ActionValidationCode::Valid };
	std::string message;

	[[nodiscard]] bool valid() const noexcept { return code == ActionValidationCode::Valid; }
};

struct ActionValidationContext
{
	WorldEpoch activeWorld;
	bool acceptsWorldActions{};
	RouteId primaryRoute{ "shell.main_menu" };
	std::optional<Revision> authoritativeRevision;
	std::optional<std::int32_t> minimumLevel;
	std::optional<std::int32_t> maximumLevel;
	std::optional<ModalInstanceId> topModal;
	std::optional<ModalKind> topModalKind;
	std::optional<ModalInstanceId> sourceModal;
	bool developmentBuild{};
};

class UiActionRegistry
{
public:
	[[nodiscard]] static std::span<const ActionDefinition> actions() noexcept;
	[[nodiscard]] static const ActionDefinition* find( std::string_view id ) noexcept;
	[[nodiscard]] static ActionValidation audit() noexcept;
	[[nodiscard]] ActionValidation validate( const UiActionEnvelope& envelope,
		const ActionValidationContext& context ) const;

private:
	[[nodiscard]] static ActionValidation validatePayload( const UiActionPayload& payload,
		const ActionValidationContext& context );
};

namespace detail
{
template<class Payload, class Variant>
struct VariantIndex;

template<class Payload, class... Rest>
struct VariantIndex<Payload, std::variant<Payload, Rest...>>
	: std::integral_constant<std::size_t, 0>
{};

template<class Payload, class First, class... Rest>
struct VariantIndex<Payload, std::variant<First, Rest...>>
	: std::integral_constant<std::size_t,
		1 + VariantIndex<Payload, std::variant<Rest...>>::value>
{};
} // namespace detail

template<class Payload>
[[nodiscard]] constexpr std::size_t actionPayloadIndex()
{
	return detail::VariantIndex<Payload, UiActionPayload>::value;
}

} // namespace ingnomia::ui
