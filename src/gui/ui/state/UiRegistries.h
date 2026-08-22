/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "UiFoundationTypes.h"

#include <span>

namespace ingnomia::ui
{

struct RouteDefinition
{
	std::string_view id;
	RouteKind kind;
	std::string_view documentId;
	bool developmentOnly{};
};

struct DocumentDefinition
{
	std::string_view id;
	std::string_view path;
	bool builtIn{};
	bool developmentOnly{};
};

struct NamedDefinition
{
	std::string_view id;
	bool developmentOnly{};
};

struct RegistryAudit
{
	bool valid{};
	std::string message;
};

class UiRegistries
{
public:
	[[nodiscard]] static std::span<const RouteDefinition> routes() noexcept;
	[[nodiscard]] static std::span<const DocumentDefinition> documents() noexcept;
	[[nodiscard]] static std::span<const NamedDefinition> modelNames() noexcept;
	[[nodiscard]] static std::span<const NamedDefinition> tools() noexcept;

	[[nodiscard]] static const RouteDefinition* findRoute( std::string_view id, bool developmentBuild = false ) noexcept;
	[[nodiscard]] static const DocumentDefinition* findDocument( std::string_view id, bool developmentBuild = false ) noexcept;
	[[nodiscard]] static bool hasModel( std::string_view id, bool developmentBuild = false ) noexcept;
	[[nodiscard]] static bool hasTool( std::string_view id ) noexcept;
	[[nodiscard]] static RegistryAudit audit() noexcept;
};

} // namespace ingnomia::ui
