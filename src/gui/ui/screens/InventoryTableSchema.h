/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ingnomia::ui
{
inline std::string inventoryQuantityLabel( unsigned value )
{
	return value > 0 ? "Has (>0)" : "None (0)";
}

inline bool inventoryQuantityMatches( unsigned value, const std::string& filter, const std::vector<std::string>& selections )
{
	const auto matches = [value]( const std::string& choice )
	{
		return choice.empty() || ((choice == "Has (>0)" || choice == "has (>0)") && value > 0)
			|| ((choice == "None (0)" || choice == "none (0)") && value == 0);
	};
	return matches( filter ) && ( selections.empty() || std::ranges::any_of( selections, matches ) );
}

// All three tables use the same selected-column ordering and full-path tie break.
inline auto inventoryTableSortKey( std::array<std::string, 4> labels, std::size_t column )
{
	for ( auto& label : labels )
		std::ranges::transform( label, label.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	return std::pair { labels[column], labels };
}

struct InventoryTableColumns
{
	std::string category { "Category" };
	std::string group { "Type" };
	std::string item { "Item" };
	std::string material { "Material" };
};

// The simulation exposes one stable Category > Group > Item > Material path.
// These captions translate that path into player-facing terms without changing
// the authoritative catalog IDs used by Inventory and Stockpile commands.
inline InventoryTableColumns inventoryTableColumns( std::string_view category )
{
	std::string key( category );
	std::ranges::transform( key, key.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	if ( key == "armor" || key == "armour" ) return { "Category", "Material", "Type", "Item" };
	if ( key == "butchery" ) return { "Category", "Product", "Type", "Source" };
	if ( key == "containers" ) return { "Category", "Type", "Container", "Material" };
	if ( key == "drinks" ) return { "Category", "Type", "Drink", "Variety" };
	if ( key == "food" ) return { "Category", "Type", "Food", "Variety" };
	if ( key == "furniture" ) return { "Category", "Type", "Furniture", "Material" };
	if ( key == "grown" ) return { "Category", "Type", "Plant", "Variety" };
	if ( key == "jewelry" || key == "jewellery" ) return { "Category", "Type", "Jewelry", "Material" };
	if ( key == "materials" ) return { "Category", "Type", "Material", "Form" };
	if ( key == "weapons" ) return { "Category", "Type", "Weapon", "Material" };
	if ( key == "workshop" ) return { "Category", "Type", "Tool", "Material" };
	return {};
}

inline std::string inventoryTableSingularLabel( std::string value )
{
	std::ranges::transform( value, value.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	while ( !value.empty() && std::isspace( static_cast<unsigned char>( value.back() ) ) ) value.pop_back();
	if ( value.size() > 3 && value.ends_with( "ies" ) ) value.replace( value.size() - 3, 3, "y" );
	else if ( value.size() > 1 && value.ends_with( "s" ) && !value.ends_with( "ss" ) ) value.pop_back();
	return value;
}

inline void normalizeInventoryTableLabels( std::string& group, std::string& item, std::string& material, std::string_view itemId = {} )
{
	// These are distinct saved-game item IDs sharing the English label "corpse".
	// Keep both identities actionable while avoiding indistinguishable table rows.
	if ( inventoryTableSingularLabel( item ) == "corpse" )
	{
		if ( itemId == "AnimalCorpse" ) item = "animal corpse";
		else if ( itemId == "GoblinCorpse" ) item = "goblin corpse";
	}
	if ( !material.empty() && inventoryTableSingularLabel( group ) == inventoryTableSingularLabel( item ) )
	{
		item = std::move( material );
		material.clear();
	}
	else if ( !material.empty() && inventoryTableSingularLabel( item ) == inventoryTableSingularLabel( material ) )
	{
		material.clear();
	}
}
} // namespace ingnomia::ui
