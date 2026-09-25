/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/StringUtilities.h>

#include <string>
#include <utility>
#include <vector>

namespace ingnomia::ui
{
/// Replaces every option of a drop-down list. Setting a select's inner RML appends to the options RmlUi
/// already moved into its list box, so options are always rebuilt through the select API.
/// A blank first entry keeps the value empty until the user chooses one (a drop-down never invents a value).
inline void setSelectOptions( Rml::Element* element, const std::vector<std::pair<std::string, std::string>>& options, bool blankFirst )
{
	auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element );
	if( !select ) return;
	select->RemoveAll();
	if( blankFirst ) select->Add( "", "" );
	for( const auto& [value, label] : options ) select->Add( Rml::StringUtilities::EncodeRml( label ), value );
}
} // namespace ingnomia::ui
