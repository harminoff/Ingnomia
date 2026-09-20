#include "gui/ui/screens/hud/HudController.h"
#include <cstdlib>
#include <iostream>
using namespace ingnomia::ui;
using namespace ingnomia::ui::hud;
namespace {
void check( bool value, const char* message ) { if ( !value ) { std::cerr << message << '\n'; std::exit( 1 ); } }
struct Port final : HudCommandPort {
	int dispatches{};
	CommandResult dispatch( const UiActionEnvelope& ) override { ++dispatches; return {}; }
	void registerPrompt( PromptInstanceId, std::optional<EventResponseTargetId> ) override {}
};
struct View final : HudViewPort {
	HudState state;
	void stateChanged( const HudState& value ) override { state = value; }
};
}
int main()
{
	Port port;
	View view;
	HudController hud( port, view );
	hud.beginWorld( WorldEpoch{ 9 } );
	BuildCatalogRow forge;
	forge.id = CatalogId{ "forge" };
	forge.name = "Forge";
	forge.kind = BuildKind::Workshop;
	forge.available = false;
	forge.unavailableReason = "iron x2";
	hud.setBuildCatalog( { forge } );
	hud.chooseBuild( CatalogId{ "forge" } );
	check( port.dispatches == 1, "unavailable workshop blueprint was blocked" );
	check( hud.state().tool.active == ToolId{ "build" }, "workshop blueprint did not activate placement" );
	BuildCatalogRow wall;
	wall.id = CatalogId{ "wall" };
	wall.name = "Wall";
	wall.kind = BuildKind::Terrain;
	wall.available = false;
	wall.unavailableReason = "block x1";
	hud.setBuildCatalog( { wall } );
	hud.chooseBuild( CatalogId{ "wall" } );
	check( port.dispatches == 1, "unavailable non-workshop build dispatched" );
	check( hud.state().status == "block x1", "unavailable material reason missing" );
	std::cout << "HUD workshop-blueprint availability passed\n";
}
