#include "gui/ui/screens/hud/HudController.h"

#include <cstdlib>
#include <iostream>

using namespace ingnomia::ui;
using namespace ingnomia::ui::hud;

namespace
{
struct Port final : HudCommandPort
{
	UiActionEnvelope last;
	CommandResult dispatch( const UiActionEnvelope& value ) override
	{
		last = value;
		return {};
	}
};

struct View final : HudViewPort
{
	void stateChanged( const HudState& ) override {}
};

void check( bool value, const char* message )
{
	if ( !value )
	{
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}
}

int main()
{
	Port port;
	View view;
	HudController controller( port, view );
	controller.beginWorld( WorldEpoch{ 11 } );
	BuildCatalogRow terrain;
	terrain.id = CatalogId{ "stone_wall" };
	terrain.name = "Stone wall";
	terrain.kind = BuildKind::Terrain;
	terrain.defaultMaterials = { CatalogId{ "granite" } };
	terrain.components = { { CatalogId{ "stone" }, 1, { { CatalogId{ "granite" }, 4 }, { CatalogId{ "basalt" }, 3 } }, CatalogId{ "granite" } } };
	controller.setBuildCatalog( { terrain } );
	controller.chooseBuildAction( terrain.id, BuildAction::Build );
	check( std::get<ChooseBuildPayload>( port.last.payload ).action == BuildAction::Build, "Build action" );
	controller.chooseBuildAction( terrain.id, BuildAction::FillHole );
	check( std::get<ChooseBuildPayload>( port.last.payload ).action == BuildAction::FillHole, "Fill Hole action" );
	controller.chooseBuildAction( terrain.id, BuildAction::Replace );
	check( std::get<ChooseBuildPayload>( port.last.payload ).action == BuildAction::Replace, "Replace action" );
	controller.selectBuildMaterial( terrain.id, 0, CatalogId{ "basalt" } );
	check( port.last.id.value == "tool.set_material", "material action" );
	check( std::get<SetBuildMaterialPayload>( port.last.payload ).material.value == "basalt", "material payload" );
	std::cout << "HUD Build action payloads passed\n";
}
