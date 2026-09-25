#include "gui/ui/accessibility/UiAccessibility.h"
#include "gui/ui/localization/UiText.h"

#include <cstdlib>
#include <iostream>

using namespace ingnomia::ui;
using namespace ingnomia::ui::accessibility;
using namespace ingnomia::ui::localization;

void check( bool value, const char* message )
{
	if( !value )
	{
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}

int main()
{
	auto text = UiText::english();
	check( text.format( LocalizationKey{"common.close"} ) == "Close", "lookup" );
	check( text.format( LocalizationKey{"missing.key"} ) == "⟦missing.key⟧", "visible missing key" );
	check( text.format( LocalizationKey{"hud.date"}, {{"day","17"},{"year","4"}} ) == "Day 17 / Year 4", "named numeric arguments" );
	check( text.format( LocalizationKey{"management.stockpile_summary"}, {{"items","12"},{"reserved","3"},{"status","Active"}} ) == "12 stored | 3 incoming | Active", "management named arguments" );

	const auto longText = UiText::longStringFixture().format( LocalizationKey{"common.close"} );
	check( longText.size() > 80 && longText.find( "Ångström" ) != std::string::npos && longText.find( "Жук" ) != std::string::npos && longText.find( "日本語" ) != std::string::npos && longText.find( "مرحبا" ) != std::string::npos, "long UTF-8 fixture" );

	check( escapeTarget({true,true,true,true,true,true}) == EscapeLayer::Composition, "composition first" );
	check( escapeTarget({false,true,false,true,true,true}) == EscapeLayer::None, "required modal blocks escape" );
	check( escapeTarget({false,true,true,true,true,true}) == EscapeLayer::DismissibleModal, "modal order" );
	check( escapeTarget({false,false,false,true,true,true}) == EscapeLayer::Overlay, "overlay order" );
	check( escapeTarget({false,false,false,false,true,true}) == EscapeLayer::Workbench, "workbench order" );
	EscapeContext dock;dock.dock=true;dock.activeTool=true;check(escapeTarget(dock)==EscapeLayer::Dock,"dock before active tool");
	EscapeContext tool;tool.activeTool=true;check(escapeTarget(tool)==EscapeLayer::ActiveTool,"active tool before game");
	check( escapeTarget({}) == EscapeLayer::Game, "game fallback" );
	check( clampScale(.2f) == .8f && clampScale(3.f) == 2.f && clampScale(1.25f) == 1.25f, "scale bounds" );

	FocusHistory focus;
	focus.remember({"workshop","search"});
	check( !focus.take("stockpile"), "document scoped focus" );
	auto restored = focus.take("workshop");
	check( restored && restored->element == "search" && !focus.take("workshop"), "single restore" );
	std::cout << "Accessibility localization, UTF-8, named arguments, scale, focus, and Escape tests passed\n";
}
