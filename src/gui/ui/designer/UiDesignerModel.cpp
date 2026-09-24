/*
 * This file is part of Ingnomia.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiDesignerModel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace ingnomia::ui::designer
{
namespace
{
std::string jsonEscape( std::string_view value )
{
	std::string result;
	result.reserve( value.size() + 2 );
	for ( const char character : value )
	{
		switch ( character )
		{
			case '\\': result += "\\\\"; break;
			case '"': result += "\\\""; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default: result += character; break;
		}
	}
	return result;
}

void appendNumber( std::ostringstream& output, float value )
{
	output << std::fixed << std::setprecision( 2 ) << value;
}
} // namespace

UiDesignerModel::UiDesignerModel() :
	components_ {
		{ "panel", "Panel", "div" },
		{ "label", "Label", "p" },
		{ "button", "Button", "button" },
		{ "input", "Input box", "input" },
		{ "list", "List", "div" },
		{ "checkbox", "Checkbox", "input" },
		{ "image", "Image", "img" },
		{ "separator", "Separator", "hr" },
	}
{
}

void UiDesignerModel::enterDesign()
{
	if ( mode_ == Mode::Design ) return;
	mode_ = Mode::Design;
	notifyChanged();
}

void UiDesignerModel::leaveDesign()
{
	if ( mode_ == Mode::Play ) return;
	mode_ = Mode::Play;
	clearSelection();
	notifyChanged();
}

bool UiDesignerModel::select( ElementSelection selection )
{
	if ( selection.id.empty() || !selection.editable ) return false;
	if ( selection_.id == selection.id && selection_.tagName == selection.tagName )
	{
		selection_ = std::move( selection );
		notifyChanged();
		return true;
	}
	selection_ = std::move( selection );
	notifyChanged();
	return true;
}

void UiDesignerModel::clearSelection()
{
	if ( selection_.id.empty() ) return;
	selection_ = {};
	notifyChanged();
}

bool UiDesignerModel::updateBounds( Bounds bounds )
{
	if ( !hasSelection() || !selection_.editable || selection_.generated ) return false;
	if ( bounds.width < 0.0f || bounds.height < 0.0f ) return false;
	if ( selection_.left == bounds.left && selection_.top == bounds.top &&
		selection_.width == bounds.width && selection_.height == bounds.height ) return false;
	pushUndo();
	selection_.left = bounds.left;
	selection_.top = bounds.top;
	selection_.width = bounds.width;
	selection_.height = bounds.height;
	notifyChanged();
	return true;
}

void UiDesignerModel::pushUndo()
{
	if ( !selection_.id.empty() ) undoStack_.push_back( selection_ );
	if ( undoStack_.size() > 64 ) undoStack_.erase( undoStack_.begin() );
}

bool UiDesignerModel::undo()
{
	if ( undoStack_.empty() ) return false;
	selection_ = undoStack_.back();
	undoStack_.pop_back();
	notifyChanged();
	return true;
}

std::string UiDesignerModel::projectJson( std::string_view surface ) const
{
	std::ostringstream output;
	output << "{\n  \"formatVersion\": 1,\n  \"surface\": \"" << jsonEscape( surface ) << "\",\n";
	output << "  \"mode\": \"" << ( designMode() ? "design" : "play" ) << "\",\n";
	output << "  \"selection\": ";
	if ( selection_.id.empty() )
		output << "null";
	else
	{
		output << "{\"id\":\"" << jsonEscape( selection_.id ) << "\",\"tag\":\""
			   << jsonEscape( selection_.tagName ) << "\",\"left\":";
		appendNumber( output, selection_.left );
		output << ",\"top\":";
		appendNumber( output, selection_.top );
		output << ",\"width\":";
		appendNumber( output, selection_.width );
		output << ",\"height\":";
		appendNumber( output, selection_.height );
		output << ",\"generated\":" << ( selection_.generated ? "true" : "false" ) << "}";
	}
	output << ",\n  \"components\": [";
	for ( std::size_t index = 0; index < components_.size(); ++index )
	{
		if ( index != 0 ) output << ',';
		const auto& component = components_[index];
		output << "{\"id\":\"" << jsonEscape( component.id ) << "\",\"label\":\""
			   << jsonEscape( component.label ) << "\",\"tag\":\"" << jsonEscape( component.tagName ) << "\"}";
	}
	output << "],\n  \"addedComponents\": [";
	for ( std::size_t index = 0; index < addedComponents_.size(); ++index )
	{
		if ( index != 0 ) output << ',';
		const auto& component = addedComponents_[index];
		output << "{\"id\":\"" << jsonEscape( component.id ) << "\",\"component\":\""
			   << jsonEscape( component.componentId ) << "\",\"tag\":\"" << jsonEscape( component.tagName )
			   << "\",\"parent\":\"" << jsonEscape( component.parentId ) << "\",\"left\":";
		appendNumber( output, component.left );
		output << ",\"top\":";
		appendNumber( output, component.top );
		output << ",\"width\":";
		appendNumber( output, component.width );
		output << ",\"height\":";
		appendNumber( output, component.height );
		output << "}";
	}
	output << "]\n}\n";
	return output.str();
}

void UiDesignerModel::recordAddedComponent( AddedComponent component )
{
	if ( component.id.empty() || component.componentId.empty() ) return;
	addedComponents_.push_back( std::move( component ) );
	notifyChanged();
}

void UiDesignerModel::notifyChanged()
{
	if ( changeHandler_ ) changeHandler_();
}

} // namespace ingnomia::ui::designer
