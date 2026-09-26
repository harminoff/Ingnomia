/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <string>

namespace ingnomia::ui
{
inline void showManagementTooltip( Rml::ElementDocument* document, Rml::Context& context,
    const char* tooltipId, Rml::Element* source, const std::string& supplied = {} )
{
    if( !document || !source ) return;
    auto* tooltip = document->GetElementById( tooltipId );
    if( !tooltip ) return;
    const auto value = supplied.empty() ? source->GetAttribute<Rml::String>( "title", "" ) : supplied;
    if( value.empty() ) return;
    tooltip->SetInnerRML( Rml::StringUtilities::EncodeRml( value ) );
    const auto offset = source->GetAbsoluteOffset();
    const auto bounds = context.GetDimensions();
    const auto width = std::min( 280.f, std::max( 80.f, static_cast<float>( bounds.x ) - 16.f ) );
    float left = offset.x + source->GetOffsetWidth() + 8.f;
    if( left + width > bounds.x - 8.f ) left = offset.x - width - 8.f;
    left = std::clamp( left, 8.f, std::max( 8.f, static_cast<float>( bounds.x ) - width - 8.f ) );
    const float top = std::clamp( offset.y + source->GetOffsetHeight() + 4.f, 8.f,
        std::max( 8.f, static_cast<float>( bounds.y ) - 96.f ) );
    tooltip->SetProperty( "width", std::to_string( width ) + "px" );
    tooltip->SetProperty( "left", std::to_string( left ) + "px" );
    tooltip->SetProperty( "top", std::to_string( top ) + "px" );
    tooltip->SetProperty( "transform", "none" );
    tooltip->SetClass( "is-visible", true );
    tooltip->SetAttribute( "aria-hidden", "false" );
    tooltip->SetProperty("position","fixed");
    tooltip->SetProperty("margin","0px");
    tooltip->SetProperty("box-sizing","border-box");
    tooltip->SetProperty("max-height",std::to_string(std::max(1,bounds.y-16))+"px");
    tooltip->SetProperty("overflow-y","hidden");
    // Mouseover/focus handlers must not re-enter the context hover dispatcher.
    // Update only this document to measure the newly wrapped tooltip.
    document->UpdateDocument();
    const auto height=tooltip->GetOffsetHeight();
    tooltip->SetProperty("top",std::to_string(std::clamp(offset.y+source->GetOffsetHeight()+4.f,8.f,
        std::max(8.f,static_cast<float>(bounds.y)-height-8.f)))+"px");
}

inline void hideManagementTooltip( Rml::ElementDocument* document, const char* tooltipId )
{
    if( !document ) return;
    if( auto* tooltip = document->GetElementById( tooltipId ) )
    {
        tooltip->SetClass( "is-visible", false );
        tooltip->SetAttribute( "aria-hidden", "true" );
    }
}
} // namespace ingnomia::ui
