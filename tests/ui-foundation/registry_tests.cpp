#include "TestSupport.h"

#include "../../src/gui/ui/actions/UiActionRegistry.h"
#include "../../src/gui/ui/state/UiRegistries.h"

#include <unordered_set>

using namespace ingnomia::ui;

int main()
{
	TestContext test;

	const auto registryAudit = UiRegistries::audit();
	CHECK( test, registryAudit.valid );
	CHECK( test, UiRegistries::routes().size() == 18 );
	CHECK( test, UiRegistries::documents().size() == 23 );
	CHECK( test, UiRegistries::modelNames().size() == 16 );
	CHECK( test, UiRegistries::tools().size() == 31 );

	const auto* hud = UiRegistries::findRoute( "game.hud" );
	CHECK( test, hud != nullptr );
	CHECK( test, hud && hud->kind == RouteKind::Primary );
	CHECK( test, hud && hud->documentId == "doc.game_hud" );
	CHECK( test, UiRegistries::findRoute( "debug.panel" ) == nullptr );
	CHECK( test, UiRegistries::findRoute( "debug.panel", true ) != nullptr );
	CHECK( test, UiRegistries::findDocument( "doc.error_fallback" ) != nullptr );
	CHECK( test, UiRegistries::findDocument( "doc.unregistered" ) == nullptr );
	CHECK( test, UiRegistries::hasModel( "ui_hud" ) );
	CHECK( test, !UiRegistries::hasModel( "ui_debug" ) );
	CHECK( test, UiRegistries::hasModel( "ui_debug", true ) );
	CHECK( test, UiRegistries::hasTool( "explorative_mine" ) );
	CHECK( test, UiRegistries::hasTool( "suspend_job" ) );

	const auto actionAudit = UiActionRegistry::audit();
	CHECK( test, actionAudit.valid() );
	CHECK( test, UiActionRegistry::actions().size() == 117 );
	CHECK( test, UiActionRegistry::find( "app.exit" ) != nullptr );
	CHECK( test, UiActionRegistry::find( "app.exit" )->requiresConfirmation );
	CHECK( test, UiActionRegistry::find( "event.respond" ) != nullptr );
	CHECK( test, UiActionRegistry::find( "mechanism.set_active" )->requiresExpectedRevision );
	CHECK( test, UiActionRegistry::find( "agriculture.set_food_allowed" ) != nullptr );
	CHECK( test, UiActionRegistry::find( "legacy.mine.command" ) == nullptr );

	std::unordered_set<std::string_view> ids;
	for( const auto& action : UiActionRegistry::actions() ) CHECK( test, ids.emplace( action.id ).second );

	return test.result();
}
