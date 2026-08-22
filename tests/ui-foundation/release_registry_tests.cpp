#include "TestSupport.h"

#include "../../src/gui/ui/state/UiRegistries.h"

using namespace ingnomia::ui;

int main()
{
	TestContext test;
	CHECK( test, UiRegistries::findRoute( "debug.panel", true ) == nullptr );
	CHECK( test, UiRegistries::findDocument( "doc.debug_panel", true ) == nullptr );
	CHECK( test, !UiRegistries::hasModel( "ui_debug", true ) );
	CHECK( test, UiRegistries::audit().valid );
	return test.result();
}
