/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// Scroll arrow availability (PDF p.101-102, p.146): when a view cannot scroll any further in a direction, the scroll
// arrow for that direction is disabled; when all of the content is visible, both arrows are disabled and the scroll
// box fills the shaft. RmlUi draws the scroll bars but keeps their arrows enabled, so after layout the Windows 98
// lists and grids get classes that the stylesheet uses to draw the unavailable arrows.
#include <RmlUi/Core.h>

namespace ingnomia::ui::scroll_arrows
{
inline bool setClass( Rml::Element& element, const char* name, bool value )
{
	if ( element.IsClassSet( name ) == value ) return false;
	element.SetClass( name, value );
	return true;
}
/// Updates one scrolling element. Returns true when a class changed.
inline bool reconcile( Rml::Element& element )
{
	const float top = element.GetScrollTop(), left = element.GetScrollLeft();
	const float maxTop  = element.GetScrollHeight() - element.GetClientHeight();
	const float maxLeft = element.GetScrollWidth() - element.GetClientWidth();
	bool changed = false;
	changed |= setClass( element, "is-scroll-top", top <= 0.5f );
	changed |= setClass( element, "is-scroll-bottom", top >= maxTop - 0.5f );
	changed |= setClass( element, "is-scroll-left", left <= 0.5f );
	changed |= setClass( element, "is-scroll-right", left >= maxLeft - 0.5f );
	return changed;
}
/// Updates every visible Windows 98 list and grid in the context. Call after the context updates; when it returns
/// true, update the context again so the arrows are drawn in their new state.
inline bool reconcile( Rml::Context& context )
{
	bool changed = false;
	for ( const char* name : { "w98-list", "w98-grid__cells" } )
	{
		Rml::ElementList elements;
		context.GetRootElement()->GetElementsByClassName( elements, name );
		for ( auto* element : elements )
			if ( element->IsVisible( true ) ) changed |= reconcile( *element );
	}
	return changed;
}
} // namespace ingnomia::ui::scroll_arrows
