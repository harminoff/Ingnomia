#include "TestSupport.h"

#include "../../src/gui/ui/state/UiNavigation.h"
#include "../../src/gui/ui/state/UiStore.h"

using namespace ingnomia::ui;

int main()
{
	TestContext test;

	UiRouter router;
	CHECK( test, router.state().primary.value == "shell.main_menu" );
	CHECK( test, !router.state().workbench );
	CHECK( test, router.open( RouteId{ "workbench.population" } ).result == NavigationResult::IllegalContext );
	CHECK( test, router.open( RouteId{ "game.hud" } ).accepted() );
	CHECK( test, router.open( RouteId{ "workbench.population" } ).accepted() );
	CHECK( test, router.open( RouteId{ "panel.tile" } ).accepted() );
	CHECK( test, router.state().workbench->value == "workbench.population" );
	CHECK( test, router.state().dock->value == "panel.tile" );
	CHECK( test, router.open( RouteId{ "game.pause" } ).accepted() );
	CHECK( test, router.open( RouteId{ "game.settings" } ).accepted() );
	CHECK( test, router.state().overlays.size() == 2 );
	CHECK( test, router.close( RouteId{ "game.pause" } ).result == NavigationResult::NotTopmost );
	CHECK( test, router.back().accepted() );
	CHECK( test, router.state().overlays.size() == 1 );
	CHECK( test, router.open( RouteId{ "game.pause" } ).result == NavigationResult::Unchanged );
	CHECK( test, router.close( RouteId{ "game.hud" } ).result == NavigationResult::CannotClosePrimary );
	CHECK( test, router.open( RouteId{ "shell.main_menu" } ).accepted() );
	CHECK( test, !router.state().workbench && !router.state().dock && router.state().overlays.empty() );
	CHECK( test, router.open( RouteId{ "debug.panel" } ).result == NavigationResult::UnavailableRoute );

	ModalController modals;
	ModalEntry event{ ModalInstanceId{ 10 }, ModalKind::EventPrompt, DocumentId{ "doc.event_prompt" },
		FocusToken{ 100 }, false, true };
	auto modalMutation = modals.push( event );
	CHECK( test, modalMutation.accepted() );
	CHECK( test, modalMutation.dirty.contains( DirtyVariable::ModalStack ) );
	CHECK( test, modalMutation.dirty.contains( DirtyVariable::ModalEventPrompt ) );
	CHECK( test, modals.blocksWorldInput() );
	CHECK( test, modals.canInteract( ModalInstanceId{ 10 } ) );
	CHECK( test, modals.dismissTopOnEscape().result == ModalResult::NotDismissible );

	ModalEntry confirmation{ ModalInstanceId{ 11 }, ModalKind::DestructiveConfirmation,
		DocumentId{ "doc.confirm_destructive" }, FocusToken{ 101 }, true, true };
	CHECK( test, modals.push( confirmation ).accepted() );
	CHECK( test, !modals.canInteract( ModalInstanceId{ 10 } ) );
	modalMutation = modals.dismissTopOnEscape();
	CHECK( test, modalMutation.accepted() );
	CHECK( test, modalMutation.restoreFocus == FocusToken{ 101 } );
	CHECK( test, modals.top() && modals.top()->id == ModalInstanceId{ 10 } );
	CHECK( test, modals.push( ModalEntry{ ModalInstanceId{ 12 }, ModalKind::Error,
		DocumentId{ "doc.event_prompt" }, FocusToken{ 102 }, true, true } ).result == ModalResult::InvalidDocument );
	CHECK( test, modals.clear().accepted() );
	CHECK( test, modals.stack().empty() );

	UiStore store;
	CHECK( test, store.activeWorld() == WorldEpoch{ 0 } );
	auto storeResult = store.beginWorld( WorldEpoch{ 7 }, WorldPhase::Loading );
	CHECK( test, storeResult.accepted() );
	CHECK( test, storeResult.dirty.size() == 8 );
	CHECK( test, storeResult.dirty.contains( DirtyVariable::Clock ) );

	ClockCalendarState clock;
	clock.minute = 15;
	clock.hour = 9;
	clock.day = 2;
	clock.year = 1;
	clock.season = Season::Spring;
	UiStoreUpdate clockUpdate{ WorldEpoch{ 7 }, Revision{ 1 }, ChannelId::Clock, UpdateKind::Snapshot, clock };
	storeResult = store.apply( clockUpdate );
	CHECK( test, storeResult.status == StoreApplyStatus::Applied );
	CHECK( test, storeResult.dirty.size() == 1 );
	CHECK( test, storeResult.dirty.contains( DirtyVariable::Clock ) );
	CHECK( test, !storeResult.dirty.contains( DirtyVariable::Settlement ) );

	clockUpdate.revision = Revision{ 2 };
	storeResult = store.apply( clockUpdate );
	CHECK( test, storeResult.status == StoreApplyStatus::Unchanged );
	CHECK( test, storeResult.dirty.empty() );
	CHECK( test, store.channelRevision( ChannelId::Clock ) == Revision{ 2 } );
	clockUpdate.revision = Revision{ 1 };
	CHECK( test, store.apply( clockUpdate ).status == StoreApplyStatus::StaleRevision );
	clockUpdate.revision = Revision{ 3 };
	clockUpdate.world = WorldEpoch{ 6 };
	CHECK( test, store.apply( clockUpdate ).status == StoreApplyStatus::StaleEpoch );

	UiStoreUpdate mismatch{ WorldEpoch{ 7 }, Revision{ 3 }, ChannelId::Clock, UpdateKind::Snapshot,
		SettlementSummary{ "Copperhome", 8, 3, 90 } };
	CHECK( test, store.apply( mismatch ).status == StoreApplyStatus::PayloadMismatch );
	CHECK( test, store.channelRevision( ChannelId::Clock ) == Revision{ 2 } );
	CHECK( test, store.worldWillUnload( WorldEpoch{ 7 } ).accepted() );
	CHECK( test, store.state().lifecycle.phase == WorldPhase::Unloading );
	CHECK( test, store.state().settlement == SettlementSummary{} );
	clockUpdate.world = WorldEpoch{ 7 };
	clockUpdate.revision = Revision{ 99 };
	CHECK( test, store.apply( clockUpdate ).status == StoreApplyStatus::InactiveWorld );
	CHECK( test, store.beginWorld( WorldEpoch{ 8 }, WorldPhase::Generating ).accepted() );
	CHECK( test, store.apply( clockUpdate ).status == StoreApplyStatus::StaleEpoch );

	return test.result();
}
