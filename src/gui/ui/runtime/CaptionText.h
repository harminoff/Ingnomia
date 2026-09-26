/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace ingnomia::ui
{
/// The object name as it starts a caption or message box title. Title bars use book-title capitalization
/// (PDF p.329). The game's default object names are lower-case type names ("carpenter", "market stall"), so an
/// all-lower-case name gets each word capitalized ("Market Stall"). A name with any capital letter was written by
/// the player; only its first letter is capitalized and the rest stays as written ("Stage 11 farm").
inline std::string captionName( std::string name )
{
	const bool defaultName = std::none_of( name.begin(), name.end(), []( char c ) { return std::isupper( static_cast<unsigned char>( c ) ); } );
	bool wordStart = true;
	for( auto& c : name )
	{
		const auto u = static_cast<unsigned char>( c );
		if( wordStart && std::isalpha( u ) ) c = static_cast<char>( std::toupper( u ) );
		wordStart = std::isspace( u ) != 0;
		if( !defaultName && !wordStart ) break;
	}
	return name;
}
} // namespace ingnomia::ui
