/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "UiText.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/StringUtilities.h>
namespace ingnomia::ui::localization
{
inline void applyRmlText( Rml::Element& element, const UiText& catalog )
{
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n", "" ); !key.empty() )
		element.SetInnerRML( Rml::StringUtilities::EncodeRml( catalog.format( LocalizationKey{key} ) ) );
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-placeholder", "" ); !key.empty() )
		element.SetAttribute( "placeholder", catalog.format( LocalizationKey{key} ) );
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-title", "" ); !key.empty() )
		element.SetAttribute( "title", catalog.format( LocalizationKey{key} ) );
	if( const auto key = element.GetAttribute<Rml::String>( "data-l10n-aria-label", "" ); !key.empty() )
		element.SetAttribute( "aria-label", catalog.format( LocalizationKey{key} ) );
	for( int index = 0; index < element.GetNumChildren(); ++index )
		if( auto* child = element.GetChild( index ) ) applyRmlText( *child, catalog );
}
}
