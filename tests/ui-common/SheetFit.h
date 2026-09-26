/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// A property page never scrolls and never clips (PDF p.147, p.157): every visible control on it lies inside the
// page frame. Only the contents of a list, list view or grid may extend beyond, because those scroll.
#include <RmlUi/Core/Element.h>
#include <string>

namespace ingnomia::ui::test
{
inline bool scrollsItsContent( Rml::Element* e )
{
	return e->IsClassSet( "w98-list" ) || e->IsClassSet( "w98-grid__cells" ) || e->IsClassSet( "w98-grid__rowheads" ) || e->IsClassSet( "w98-grid__colheads" )
		|| e->IsClassSet( "w98-combo__list" ) || e->GetTagName() == "select" || e->GetTagName() == "selectbox";
}
inline void findOverflow( Rml::Element* e, float left, float top, float right, float bottom, std::string& out )
{
	for ( int i = 0; i < e->GetNumChildren(); ++i )
	{
		auto* child = e->GetChild( i );
		if ( !child->IsVisible( true ) || child->GetOffsetWidth() <= 0.f || child->GetOffsetHeight() <= 0.f ) continue;
		if ( child->GetProperty<Rml::String>( "position" ) == "absolute" ) continue;
		const auto at = child->GetAbsoluteOffset( Rml::BoxArea::Border );
		const float r = at.x + child->GetOffsetWidth(), b = at.y + child->GetOffsetHeight();
		if ( at.x < left - 1.f || at.y < top - 1.f || r > right + 1.f || b > bottom + 1.f )
		{
			if ( out.size() < 400 )
				out += " <" + child->GetTagName() + " id='" + child->GetId() + "' class='" + child->GetClassNames() + "'> " + std::to_string( int( at.x ) ) + "," + std::to_string( int( at.y ) ) + "-" + std::to_string( int( r ) ) + ","
					+ std::to_string( int( b ) ) + " outside " + std::to_string( int( left ) ) + "," + std::to_string( int( top ) ) + "-" + std::to_string( int( right ) ) + "," + std::to_string( int( bottom ) ) + ";";
			continue;
		}
		if ( !scrollsItsContent( child ) ) findOverflow( child, left, top, right, bottom, out );
	}
}
/// Empty when every visible control of the page frame lies inside it; otherwise a description of what does not.
inline std::string pageOverflow( Rml::Element* frame )
{
	std::string out;
	if ( !frame ) return "no page frame";
	const auto at = frame->GetAbsoluteOffset( Rml::BoxArea::Padding );
	const auto size = frame->GetBox().GetSize( Rml::BoxArea::Padding );
	findOverflow( frame, at.x, at.y, at.x + size.x, at.y + size.y, out );
	return out;
}
} // namespace ingnomia::ui::test
