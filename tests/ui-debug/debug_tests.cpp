#include "gui/ui/screens/developer_ui/DebugController.h"

#include <cstdlib>
#include <iostream>
using namespace ingnomia::ui;
using namespace ingnomia::ui::debug;
void check( bool v, const char* m )
{
	if ( !v )
	{
		std::cerr << m;
		std::exit( 1 );
	}
}
struct P : CommandPort
{
	int n {};
	bool dispatch( const DebugAction& ) override
	{
		++n;
		return true;
	}
};
struct V : ViewPort
{
	DebugState s;
	void stateChanged( const DebugState& v ) override
	{
		s = v;
	}
};
int main()
{
	P p;
	V v;
	DebugController release( false, p, v );
	release.beginWorld( WorldEpoch { 1 } );
	release.open();
	check( !release.state().open && p.n == 0, "release dormant" );
	DebugController c( true, p, v );
	c.beginWorld( WorldEpoch { 7 } );
	check( !c.state().open, "hidden" );
	c.open();
	check( c.state().open && p.n == 1, "open refresh" );
	check( c.applyGnomes( WorldEpoch { 7 }, Revision { 1 }, { { "Ada", 7 }, { "Bera", 9 } } ), "snapshot" );
	c.selectGnome( 9 );
	c.setSearch( "ber" );
	check( c.visibleGnomes().size() == 1 && c.state().selectedGnome == std::uint32_t { 9 }, "stable search" );
	check( !c.applyGnomes( WorldEpoch { 6 }, Revision { 2 }, {} ), "epoch" );
	c.instrumentDirty( 3, false, true );
	check( c.state().counters.dirtyBindings == 3 && c.state().counters.rowPatches == 1, "measured counters" );
	c.close();
	c.endWorld();
	check( !c.state().open && c.state().gnomes.empty(), "lifecycle" );
	std::cout << "debug tests passed\n";
}
