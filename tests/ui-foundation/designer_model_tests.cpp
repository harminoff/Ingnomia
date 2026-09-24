/*
 * This file is part of Ingnomia.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "gui/ui/designer/UiDesignerModel.h"

#include <cassert>
#include <string>

using ingnomia::ui::designer::Bounds;
using ingnomia::ui::designer::ElementSelection;
using ingnomia::ui::designer::Mode;
using ingnomia::ui::designer::UiDesignerModel;

int main()
{
	UiDesignerModel model;
	assert( model.mode() == Mode::Play );
	assert( model.components().size() >= 5 );
	model.enterDesign();
	assert( model.designMode() );
	assert( model.select( ElementSelection { "inventory_workbench", "main", 10.0f, 20.0f, 400.0f, 300.0f, true, false } ) );
	assert( model.updateBounds( Bounds { 12.0f, 24.0f, 420.0f, 310.0f } ) );
	assert( model.undoDepth() == 1 );
	assert( model.undo() );
	assert( model.selection().left == 10.0f );
	assert( model.selection().width == 400.0f );
	assert( !model.updateBounds( Bounds { 10.0f, 20.0f, 400.0f, 300.0f } ) );
	model.recordAddedComponent( { "designer_added_button_1", "button", "button", "inventory_workbench", 16.0f, 12.0f, 96.0f, 24.0f } );
	assert( !model.select( ElementSelection { "generated_rows", "div", 0, 0, 10, 10, true, true } ) || model.selection().generated );
	const std::string project = model.projectJson( "main" );
	assert( project.find( "\"formatVersion\": 1" ) != std::string::npos );
	assert( project.find( "generated_rows" ) != std::string::npos );
	assert( project.find( "designer_added_button_1" ) != std::string::npos );
	assert( project.find( "\"parent\":\"inventory_workbench\"" ) != std::string::npos );
	assert( project.find( "\"width\":96.00" ) != std::string::npos );
	model.leaveDesign();
	assert( model.mode() == Mode::Play );
	assert( !model.hasSelection() );
	return 0;
}
