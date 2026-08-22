#include "gui/ui/screens/inspector/InspectorController.h"

#include <cstdlib>
#include <iostream>

using namespace ingnomia::ui;
using namespace ingnomia::ui::inspector;

namespace
{
struct Port final : InspectorCommandPort
{
	CommandResult dispatch( const UiActionEnvelope& ) override { return {}; }
};

struct View final : InspectorViewPort
{
	void stateChanged( const InspectorState& ) override {}
};

void check( bool condition, const char* message )
{
	if ( !condition )
	{
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}
} // namespace

int main()
{
	Port port;
	View view;
	InspectorController controller( port, view );
	controller.beginWorld( WorldEpoch{ 91 } );

	CreatureInspectorState creature;
	creature.id = CreatureId{ 22 };
	creature.name = "Ada";
	creature.skills.push_back( { "Mining", "level 4 | active", 0 } );
	creature.skills.push_back( { "Building", "level 0 | inactive", 0 } );
	controller.showCreature( creature );

	check( controller.state().creature.has_value(), "creature state is projected" );
	check( controller.state().creature->skills.size() == 2, "all projected skills are retained" );
	check( controller.state().creature->skills[0].label == "Mining", "skill order is retained" );
	check( controller.state().creature->skills[0].detail == "level 4 | active", "skill detail is retained" );
	check( controller.state().creature->skills[1].label == "Building", "second skill is retained" );

	controller.endWorld();
	check( !controller.state().creature.has_value(), "world teardown clears creature skills" );

	std::cout << "ui_inspector_skills: 5 checks passed\n";
	return 0;
}
