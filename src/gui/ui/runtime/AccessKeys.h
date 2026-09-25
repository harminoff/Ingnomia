/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "ConnectedTabs.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cctype>

namespace ingnomia::ui::access_keys
{
// Access keys (PDF p.46-47, p.161, p.328): a control's underlined letter is named by its `accesskey` attribute.
// Alt+letter moves to the control and has the same effect as clicking it; the plain letter works too when the focused
// control does not take text. A label passes the key on to its control.
inline bool takesText( Rml::Element* focus )
{
	if( !focus ) return false;
	const auto tag = focus->GetTagName();
	if( tag == "textarea" || tag == "select" ) return true;
	if( tag != "input" ) return false;
	const auto type = focus->GetAttribute<Rml::String>( "type", "text" );
	return type == "text" || type == "password";
}
inline bool insideWindow( Rml::Element* element )
{
	for( auto* e = element; e; e = e->GetParentNode() )
		if( e->IsClassSet( "w98-sheet" ) ) return true;
	return false;
}
inline Rml::Element* targetOf( Rml::ElementDocument& document, Rml::Element* element )
{
	if( element->GetTagName() != "label" ) return element;
	if( const auto id = element->GetAttribute<Rml::String>( "for", "" ); !id.empty() ) return document.GetElementById( id );
	Rml::ElementList inputs;
	element->GetElementsByTagName( inputs, "input" );
	return inputs.empty() ? nullptr : inputs.front();
}
inline bool activate( Rml::Context& context, char letter, bool alt )
{
	auto* focus = context.GetFocusElement();
	auto* document = focus ? focus->GetOwnerDocument() : nullptr;
	if( !document ) return false;
	// A plain letter belongs to a text box that has the focus, and to the game outside Windows 98 windows.
	if( !alt && ( takesText( focus ) || !insideWindow( focus ) ) ) return false;
	Rml::ElementList candidates;
	document->QuerySelectorAll( candidates, "[accesskey]" );
	for( auto* candidate : candidates )
	{
		const auto key = candidate->GetAttribute<Rml::String>( "accesskey", "" );
		if( key.size() != 1 || std::tolower( static_cast<unsigned char>( key[0] ) ) != std::tolower( static_cast<unsigned char>( letter ) ) ) continue;
		if( !candidate->IsVisible( true ) || !connected_tabs::enabled( candidate ) ) continue;
		auto* target = targetOf( *document, candidate );
		if( !target || !target->IsVisible( true ) || !connected_tabs::enabled( target ) ) continue;
		auto observer = target->GetObserverPtr();
		target->Focus( true );
		// Text boxes, drop-down lists and sliders only take the focus; everything else acts as if clicked.
		const auto tag = target->GetTagName();
		const auto type = target->GetAttribute<Rml::String>( "type", "" );
		const bool focusOnly = tag == "select" || tag == "textarea" || ( tag == "input" && ( type == "text" || type == "range" || type == "password" ) );
		if( !focusOnly && observer ) observer->Click();
		return true;
	}
	return false;
}
} // namespace ingnomia::ui::access_keys
