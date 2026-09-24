#include "TestSupport.h"

#include "../../src/gui/ui/actions/UiActionRegistry.h"

using namespace ingnomia::ui;

int main()
{
	TestContext test;
	UiActionRegistry registry;

	ActionValidationContext worldContext;
	worldContext.activeWorld = WorldEpoch{ 7 };
	worldContext.acceptsWorldActions = true;
	worldContext.primaryRoute = RouteId{ "game.hud" };
	worldContext.authoritativeRevision = Revision{ 12 };
	worldContext.minimumLevel = -20;
	worldContext.maximumLevel = 10;

	UiActionEnvelope pause{ ActionId{ "sim.set_paused" }, RequestId{ 1 }, WorldEpoch{ 7 },
		std::nullopt, SetPausedPayload{ true } };
	CHECK( test, registry.validate( pause, worldContext ).valid() );

	auto wrongPayload = pause;
	wrongPayload.payload = NoPayload{};
	CHECK( test, registry.validate( wrongPayload, worldContext ).code == ActionValidationCode::PayloadMismatch );

	auto unknown = pause;
	unknown.id = ActionId{ "legacy.pause.command" };
	CHECK( test, registry.validate( unknown, worldContext ).code == ActionValidationCode::UnknownAction );

	auto missingWorld = pause;
	missingWorld.world.reset();
	CHECK( test, registry.validate( missingWorld, worldContext ).code == ActionValidationCode::MissingWorld );

	auto staleWorld = pause;
	staleWorld.world = WorldEpoch{ 6 };
	CHECK( test, registry.validate( staleWorld, worldContext ).code == ActionValidationCode::StaleWorld );

	auto staleRevision = pause;
	staleRevision.expectedRevision = Revision{ 11 };
	CHECK( test, registry.validate( staleRevision, worldContext ).code == ActionValidationCode::StaleRevision );
	staleRevision.expectedRevision = Revision{ 12 };
	CHECK( test, registry.validate( staleRevision, worldContext ).valid() );

	UiActionEnvelope level{ ActionId{ "view.change_level" }, RequestId{ 2 }, WorldEpoch{ 7 },
		std::nullopt, ChangeLevelPayload{ 11 } };
	CHECK( test, registry.validate( level, worldContext ).code == ActionValidationCode::InvalidPayload );
	level.payload = ChangeLevelPayload{ -20 };
	CHECK( test, registry.validate( level, worldContext ).valid() );

	UiActionEnvelope tool{ ActionId{ "tool.activate" }, RequestId{ 3 }, WorldEpoch{ 7 }, std::nullopt,
		ActivateToolPayload{ ToolId{ "not_a_tool" }, std::nullopt, {} } };
	CHECK( test, registry.validate( tool, worldContext ).code == ActionValidationCode::InvalidPayload );
	tool.payload = ActivateToolPayload{ ToolId{ "mine" }, std::nullopt, {} };
	CHECK( test, registry.validate( tool, worldContext ).valid() );

	const StockpileFilterRowId filterRow{ StockpileId{ 4 }, CatalogId{ "food" }, CatalogId{ "raw" }, CatalogId{ "fruit" }, CatalogId{ "apple" }, FilterDepth::Material };
	UiActionEnvelope bulkFilters{ ActionId{ "stockpile.set_filters" }, RequestId{ 31 }, WorldEpoch{ 7 }, std::nullopt,
		SetStockpileFiltersPayload{ StockpileId{ 4 }, { filterRow }, true } };
	CHECK( test, registry.validate( bulkFilters, worldContext ).valid() );
	bulkFilters.payload = SetStockpileFiltersPayload{ StockpileId{ 4 }, {}, true };
	CHECK( test, registry.validate( bulkFilters, worldContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope select{ ActionId{ "inspect.select" }, RequestId{ 4 }, WorldEpoch{ 7 }, std::nullopt,
		SelectPayload{ EntityRef{ WorldEpoch{ 6 }, EntityKind::Creature, 23, std::nullopt } } };
	CHECK( test, registry.validate( select, worldContext ).code == ActionValidationCode::InvalidPayload );
	select.payload = SelectPayload{ EntityRef{ WorldEpoch{ 7 }, EntityKind::Creature, 23, std::nullopt } };
	CHECK( test, registry.validate( select, worldContext ).valid() );

	worldContext.topModal = ModalInstanceId{ 40 };
	CHECK( test, registry.validate( pause, worldContext ).code == ActionValidationCode::BlockedByModal );
	worldContext.sourceModal = ModalInstanceId{ 39 };
	CHECK( test, registry.validate( pause, worldContext ).code == ActionValidationCode::BlockedByModal );
	worldContext.sourceModal = ModalInstanceId{ 40 };
	CHECK( test, registry.validate( pause, worldContext ).valid() );
	worldContext.topModal.reset();
	worldContext.sourceModal.reset();

	UiActionEnvelope eventResponse{ ActionId{ "event.respond" }, RequestId{ 7 }, WorldEpoch{ 7 }, std::nullopt,
		EventResponsePayload{ PromptInstanceId{ 60 }, EventResponse::Acknowledge } };
	CHECK( test, registry.validate( eventResponse, worldContext ).code == ActionValidationCode::IllegalContext );
	worldContext.topModal = ModalInstanceId{ 60 };
	worldContext.topModalKind = ModalKind::EventPrompt;
	worldContext.sourceModal = ModalInstanceId{ 60 };
	CHECK( test, registry.validate( eventResponse, worldContext ).valid() );
	worldContext.topModal.reset();
	worldContext.topModalKind.reset();
	worldContext.sourceModal.reset();

	UiActionEnvelope mechanism{ ActionId{ "mechanism.set_active" }, RequestId{ 8 }, WorldEpoch{ 7 },
		std::nullopt, SetMechanismStatePayload{ MechanismId{ 9 }, true } };
	CHECK( test, registry.validate( mechanism, worldContext ).code == ActionValidationCode::MissingExpectedRevision );
	mechanism.expectedRevision = Revision{ 12 };
	CHECK( test, registry.validate( mechanism, worldContext ).valid() );

	UiActionEnvelope schedule{ ActionId{ "population.set_schedule_cell" }, RequestId{ 10 }, WorldEpoch{ 7 },
		std::nullopt, SetScheduleCellPayload{ ScheduleCellId{ CreatureId{ 2 }, 23 }, ScheduleActivity::Training } };
	CHECK( test, registry.validate( schedule, worldContext ).valid() );
	schedule.payload = SetScheduleCellPayload{ ScheduleCellId{ CreatureId{ 2 }, 24 }, ScheduleActivity::Sleep };
	CHECK( test, registry.validate( schedule, worldContext ).code == ActionValidationCode::InvalidPayload );
	schedule.payload = SetScheduleCellPayload{ ScheduleCellId{ CreatureId{ 2 }, 1 }, static_cast<ScheduleActivity>( 255 ) };
	CHECK( test, registry.validate( schedule, worldContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope attitude{ ActionId{ "military.set_attitude" }, RequestId{ 11 }, WorldEpoch{ 7 },
		std::nullopt, SetAttitudePayload{ SquadId{ 3 }, CatalogId{ "creature.goblin" }, MilitaryAttitude::Defend } };
	CHECK( test, registry.validate( attitude, worldContext ).valid() );
	attitude.payload = SetAttitudePayload{ SquadId{ 3 }, CatalogId{ "creature.goblin" }, static_cast<MilitaryAttitude>( 255 ) };
	CHECK( test, registry.validate( attitude, worldContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope mission{ ActionId{ "diplomacy.start_mission" }, RequestId{ 12 }, WorldEpoch{ 7 },
		std::nullopt, StartMissionPayload{ MissionType::Explore, MissionAction::None, NeighborId{ 4 }, CreatureId{ 2 } } };
	CHECK( test, registry.validate( mission, worldContext ).valid() );
	mission.payload = StartMissionPayload{ MissionType::None, MissionAction::None, NeighborId{ 4 }, CreatureId{ 2 } };
	CHECK( test, registry.validate( mission, worldContext ).code == ActionValidationCode::InvalidPayload );
	mission.payload = StartMissionPayload{ MissionType::Raid, static_cast<MissionAction>( 255 ), NeighborId{ 4 }, CreatureId{ 2 } };
	CHECK( test, registry.validate( mission, worldContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope uniform{ ActionId{ "military.set_uniform_slot" }, RequestId{ 13 }, WorldEpoch{ 7 },
		std::nullopt, SetUniformSlotPayload{ MilitaryRoleId{ 5 }, UniformSlot::Back,
			CatalogId{ "armor.backpack" }, std::nullopt } };
	CHECK( test, registry.validate( uniform, worldContext ).valid() );
	uniform.payload = SetUniformSlotPayload{ MilitaryRoleId{ 5 }, static_cast<UniformSlot>( 255 ),
		CatalogId{ "armor.backpack" }, std::nullopt };
	CHECK( test, registry.validate( uniform, worldContext ).code == ActionValidationCode::InvalidPayload );

	ActionValidationContext menuContext;
	UiActionEnvelope load{ ActionId{ "app.load_game" }, RequestId{ 5 }, std::nullopt, std::nullopt,
		LoadGamePayload{ SaveSlotId{ "Copperhome/slot-1" } } };
	CHECK( test, registry.validate( load, menuContext ).valid() );
	load.payload = LoadGamePayload{ SaveSlotId{ "../outside" } };
	CHECK( test, registry.validate( load, menuContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope exit{ ActionId{ "app.exit" }, RequestId{ 9 }, std::nullopt, std::nullopt, NoPayload{} };
	CHECK( test, registry.validate( exit, menuContext ).code == ActionValidationCode::ConfirmationRequired );
	menuContext.topModal = ModalInstanceId{ 70 };
	menuContext.topModalKind = ModalKind::DestructiveConfirmation;
	menuContext.sourceModal = ModalInstanceId{ 70 };
	CHECK( test, registry.validate( exit, menuContext ).valid() );
	menuContext.topModal.reset();
	menuContext.topModalKind.reset();
	menuContext.sourceModal.reset();
	load.payload = LoadGamePayload{ SaveSlotId{ "C:/absolute" } };
	CHECK( test, registry.validate( load, menuContext ).code == ActionValidationCode::InvalidPayload );

	UiActionEnvelope nav{ ActionId{ "nav.open" }, RequestId{ 6 }, std::nullopt, std::nullopt,
		RoutePayload{ RouteId{ "workbench.population" } } };
	CHECK( test, registry.validate( nav, menuContext ).valid() );
	nav.payload = RoutePayload{ RouteId{ "invented.route" } };
	CHECK( test, registry.validate( nav, menuContext ).code == ActionValidationCode::InvalidPayload );
	nav.payload = RoutePayload{ RouteId{ "debug.panel" } };
	CHECK( test, registry.validate( nav, menuContext ).code == ActionValidationCode::InvalidPayload );
	menuContext.developmentBuild = true;
	CHECK( test, registry.validate( nav, menuContext ).valid() );

	return test.result();
}
