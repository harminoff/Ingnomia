#include "gui/ui/state/UiRegistries.h"

#include <cstdlib>
#include <iostream>
using namespace ingnomia::ui;
int main()
{
	if ( UiRegistries::findRoute( "debug.panel", true ) || UiRegistries::findDocument( "doc.debug_panel", true ) || UiRegistries::hasModel( "ui_debug", true ) )
	{
		std::cerr << "release registry exposed developer UI\n";
		return EXIT_FAILURE;
	}
	std::cout << "release registry excludes developer UI\n";
}
