/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "UiText.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/StringUtilities.h>
#include <cctype>
#include <utility>
namespace ingnomia::ui::localization
{
/// Splits a catalog string at its access key marker, as in Windows resources ("&Name:", PDF p.214): the
/// character after a single '&' is the access key, and "&&" is a literal ampersand. Returns the RML to show
/// (the key underlined with `w98-ak`) and the key in lower case, or 0 when the string has none.
inline std::pair<Rml::String, char> accessKeyMarkup( const std::string& text )
{
	Rml::String markup, plain;
	char key = 0;
	const auto flush = [&] { markup += Rml::StringUtilities::EncodeRml( plain ); plain.clear(); };
	for( std::size_t index = 0; index < text.size(); ++index )
	{
		if( text[index] != '&' || index + 1 >= text.size() ) { plain += text[index]; continue; }
		if( text[index + 1] == '&' ) { plain += '&'; ++index; continue; }
		if( key ) { plain += text[++index]; continue; }
		flush();
		key = static_cast<char>( std::tolower( static_cast<unsigned char>( text[index + 1] ) ) );
		markup += "<span class=\"w98-ak\">" + Rml::StringUtilities::EncodeRml( std::string( 1, text[index + 1] ) ) + "</span>";
		++index;
	}
	flush();
	return { markup, key };
}
/// The control that owns an access key: the label or button that holds the text, or the element itself.
inline Rml::Element* accessKeyOwner( Rml::Element& element )
{
	Rml::Element* node = &element;
	for( int depth = 0; node && depth < 3; ++depth, node = node->GetParentNode() )
		if( node->GetTagName() == "label" || node->GetTagName() == "button" ) return node;
	return &element;
}
inline void applyRmlText( Rml::Element& element, const UiText& catalog )
{
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n", "" ); !key.empty() )
	{
		const auto [markup, access] = accessKeyMarkup( catalog.format( LocalizationKey{key} ) );
		element.SetInnerRML( markup );
		if( access ) accessKeyOwner( element )->SetAttribute( "accesskey", Rml::String( 1, access ) );
	}
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-placeholder", "" ); !key.empty() )
		element.SetAttribute( "placeholder", catalog.format( LocalizationKey{key} ) );
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-title", "" ); !key.empty() )
		element.SetAttribute( "title", catalog.format( LocalizationKey{key} ) );
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-aria-label", "" ); !key.empty() )
		element.SetAttribute( "aria-label", catalog.format( LocalizationKey{key} ) );
	if ( element.IsClassSet( "c-title-bar__title" ) && element.GetInnerRML().find_first_not_of( " \t\r\n" ) == Rml::String::npos )
        element.SetInnerRML( Rml::StringUtilities::EncodeRml( catalog.format( LocalizationKey{ "common.untitled_window" } ) ) );
	// A drop-down list keeps its options outside the document's child list; localize them there and
	// re-select the chosen option so the shown value follows the new text.
	if( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( &element ) )
	{
		bool changed = false;
		for( int index = 0; index < select->GetNumOptions(); ++index )
			if( auto* option = select->GetOption( index ) )
				if( const auto key = option->GetAttribute<Rml::String>( "data-l10n", "" ); !key.empty() )
				{
					option->SetInnerRML( Rml::StringUtilities::EncodeRml( catalog.format( LocalizationKey{key} ) ) );
					changed = true;
				}
		if( changed && select->GetSelection() >= 0 ) select->SetSelection( select->GetSelection() );
	}
    for( int index = 0; index < element.GetNumChildren(); ++index )
		if( auto* child = element.GetChild( index ) ) applyRmlText( *child, catalog );
}
}
