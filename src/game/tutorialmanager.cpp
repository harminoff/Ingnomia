/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "tutorialmanager.h"

#include "game.h"
#include "eventmanager.h"
#include "gnomemanager.h"
#include "gnome.h"
#include "inventory.h"
#include "workshopmanager.h"
#include "workshop.h"
#include "farmingmanager.h"
#include "farm.h"
#include "roommanager.h"
#include "stockpilemanager.h"
#include "stockpile.h"
#include "jobmanager.h"
#include "job.h"
#include "world.h"
#include "../base/config.h"
#include "../base/gamestate.h"
#include "../base/global.h"

#include <QStringList>

namespace
{
QString stepName( TutorialStepId step )
{
	switch( step )
	{
	case TutorialStepId::Orientation: return QStringLiteral( "tutorial.step.orientation" );
	case TutorialStepId::InspectAndAssign: return QStringLiteral( "tutorial.step.inspect_assign" );
	case TutorialStepId::Gathering: return QStringLiteral( "tutorial.step.gathering" );
	case TutorialStepId::MiningAndLevels: return QStringLiteral( "tutorial.step.mining_levels" );
	case TutorialStepId::Stockpile: return QStringLiteral( "tutorial.step.stockpile" );
	case TutorialStepId::Crafting: return QStringLiteral( "tutorial.step.crafting" );
	case TutorialStepId::Farming: return QStringLiteral( "tutorial.step.farming" );
	case TutorialStepId::Shelter: return QStringLiteral( "tutorial.step.shelter" );
	case TutorialStepId::Graduation: return QStringLiteral( "tutorial.step.graduation" );
	}
	return {};
}
}

TutorialManager::TutorialManager( Game* game ) : QObject( game ), game_( game )
{
	qRegisterMetaType<TutorialSnapshot>();
}

void TutorialManager::start( const char* scenarioId, std::uint32_t version )
{
	progress_ = {};
	progress_.mode = TutorialMode::Interactive;
	progress_.scenarioId = scenarioId ? scenarioId : "scenario_v2";
	progress_.scenarioVersion = version;
	progress_.hintsEnabled = true;
	incompatible_ = false;
	facts_ = {};
	workshopStockpileLinked_ = false;
	stockpileAllowsRawWood_ = false;
	starterBedRepairApplied_ = true; // New worlds receive the corrected starter kit.
	pausedForLesson_ = true;
	migrationQueued_ = false;
	baselinePlank_ = game_ && game_->inv() ? static_cast<int>( game_->inv()->itemCount( "Plank", "any" ) ) : 0;
	baselineFlour_ = game_ && game_->inv() ? static_cast<int>( game_->inv()->itemCount( "Flour", "any" ) ) : 0;
	baselineBread_ = game_ && game_->inv() ? static_cast<int>( game_->inv()->itemCount( "Bread", "any" ) ) : 0;
	baselineFruit_ = game_ && game_->inv() ? static_cast<int>( game_->inv()->itemCount( "Fruit", "any" ) ) : 0;
	baselineProfessions_.clear();
	completedExcavationJobs_.clear();
	lastSnapshot_ = {};
	if( game_ && game_->gm() )
		for( auto* gnome : game_->gm()->gnomes() )
			if( gnome ) baselineProfessions_.insert( gnome->id(), gnome->profession() );
	emitIfChanged();
}

void TutorialManager::tick()
{
	if( !active() ) return;
	if( game_->rm() )
	{
		int beds = 0;
		for( auto* room : game_->rm()->allRooms() )
			if( room && room->type() == RoomType::Dorm )
			{
				facts_ |= tutorialFactBit( TutorialFact::Shelter );
				beds += room->numBeds();
			}
		if( beds >= 5 ) facts_ |= tutorialFactBit( TutorialFact::Beds );
	}
	if( game_->spm() && !game_->spm()->allStockpiles().isEmpty() ) facts_ |= tutorialFactBit( TutorialFact::Stockpile );
	stockpileAllowsRawWood_ = hasAllowedRawWoodStockpile();
	if( game_->fm() && game_->fm()->countFarms() > 0 )
	{
		facts_ |= tutorialFactBit( TutorialFact::Farm );
		for( auto* farm : game_->fm()->allFarms() )
		{
			if( !farm ) continue;
			if( farm->hasAssignedCrop() ) facts_ |= tutorialFactBit( TutorialFact::CropSelected );
			if( farm->hasQueuedCropOrder() ) facts_ |= tutorialFactBit( TutorialFact::CropQueued );
			int plots = 0, tilled = 0, planted = 0, ready = 0;
			farm->getInfo( plots, tilled, planted, ready );
			Q_UNUSED( plots ); Q_UNUSED( tilled ); Q_UNUSED( ready );
			if( planted > 0 ) facts_ |= tutorialFactBit( TutorialFact::Plant );
		}
	}
	if( game_->wsm() )
	{
		for( auto* workshop : game_->wsm()->workshops() )
		{
			if( !workshop ) continue;
			if( workshop->type() == "Crude" )
			{
				facts_ |= tutorialFactBit( TutorialFact::Workshop );
				if( !workshop->linkedStockpiles().isEmpty() ) workshopStockpileLinked_ = true;
			}
			if( workshop->type() == "Kitchen" ) facts_ |= tutorialFactBit( TutorialFact::Kitchen );
		}
	}
	if( game_->inv() )
	{
		if( static_cast<int>( game_->inv()->itemCount( "Plank", "any" ) ) > baselinePlank_ ) facts_ |= tutorialFactBit( TutorialFact::Plank );
		if( static_cast<int>( game_->inv()->itemCount( "Flour", "any" ) ) > baselineFlour_ ) facts_ |= tutorialFactBit( TutorialFact::Flour );
		if( static_cast<int>( game_->inv()->itemCount( "Bread", "any" ) ) > baselineBread_ ) facts_ |= tutorialFactBit( TutorialFact::Bread );
		if( static_cast<int>( game_->inv()->itemCount( "Fruit", "any" ) ) > baselineFruit_ ) facts_ |= tutorialFactBit( TutorialFact::Harvest );
	}
	evaluateCurrentStep();
}

void TutorialManager::observeProfession( unsigned int gnomeId, const QString& profession )
{
	if( !active() ) return;
	if( profession == QStringLiteral( "Woodcutter" ) && baselineProfessions_.contains( gnomeId )
		&& baselineProfessions_.value( gnomeId ) != profession ) observeFact( TutorialFact::AssignWork );
}

void TutorialManager::observeWorkshopStockpileLink( unsigned int workshopId )
{
	if( !active() || !game_ || !game_->wsm() ) return;
	auto* workshop = game_->wsm()->workshop( workshopId );
	if( !workshop || workshop->type() != "Crude" || workshop->linkedStockpiles().isEmpty() ) return;
	workshopStockpileLinked_ = true;
	evaluateCurrentStep();
}

void TutorialManager::observeStockpileAllowRule( unsigned int stockpileId )
{
	if( !active() || !game_ || !game_->spm() ) return;
	Q_UNUSED( stockpileId );
	stockpileAllowsRawWood_ = hasAllowedRawWoodStockpile();
	evaluateCurrentStep();
}

bool TutorialManager::hasAllowedRawWoodStockpile() const
{
	if( !game_ || !game_->spm() ) return false;
	for( const auto id : game_->spm()->allStockpiles() )
	{
		auto* stockpile = game_->spm()->getStockpile( id );
		if( stockpile && stockpile->pFilter()
			&& stockpile->pFilter()->getCheckState( "Materials", "Wood", "RawWood", "Pine" ) ) return true;
	}
	return false;
}

bool TutorialManager::hasLinkedAllowedRawWoodStockpile() const
{
	if( !game_ || !game_->wsm() || !game_->spm() ) return false;
	for( auto* workshop : game_->wsm()->workshops() )
	{
		if( !workshop || workshop->type() != "Crude" ) continue;
		for( const auto id : workshop->linkedStockpiles() )
		{
			auto* stockpile = game_->spm()->getStockpile( id );
			if( stockpile && stockpile->pFilter()
				&& stockpile->pFilter()->getCheckState( "Materials", "Wood", "RawWood", "Pine" ) ) return true;
		}
	}
	return false;
}

void TutorialManager::repairMissingStarterBeds()
{
	if( starterBedRepairApplied_ || !active() || progress_.step != TutorialStepId::Shelter || !game_ || !game_->inv() ) return;
	int beds = static_cast<int>( game_->inv()->itemCountWithInJob( "Bed", "any" ) );
	if( game_->rm() )
		for( auto* room : game_->rm()->allRooms() )
			if( room ) beds += room->numBeds();
	for( ; beds < 5; ++beds ) game_->inv()->createItem( GameState::origin, "Bed", "Pine" );
	starterBedRepairApplied_ = true;
}

void TutorialManager::observeCompletedJob( const QString& type )
{
	if( !active() ) return;
	if( type == "Harvest" ) facts_ |= tutorialFactBit( TutorialFact::Harvest );
	else if( type == "FellTree" ) facts_ |= tutorialFactBit( TutorialFact::FellTree );
	else if( type == "Mine" ) facts_ |= tutorialFactBit( TutorialFact::MineWall );
	else if( type == "DigStairsDown" || type == "MineStairsUp" ) facts_ |= tutorialFactBit( TutorialFact::LowerLevel );
	evaluateCurrentStep();
}

void TutorialManager::onPauseChanged( bool paused )
{
	if( paused || !active() || !pausedForLesson_ ) return;
	pausedForLesson_ = false;
	emitIfChanged();
}

void TutorialManager::observeFact( TutorialFact fact )
{
	if( !active() ) return;
	facts_ |= tutorialFactBit( fact );
	evaluateCurrentStep();
}

void TutorialManager::observeFacts( std::uint32_t facts )
{
	if( !active() ) return;
	facts_ |= facts;
	evaluateCurrentStep();
}

void TutorialManager::advance()
{
	if( !active() ) return;
	const auto bit = tutorialBit( progress_.step );
	if( ( progress_.completedMask & bit ) == 0 && ( progress_.skippedMask & bit ) == 0 ) return;
	if( progress_.step == TutorialStepId::Graduation )
	{
		finish();
		return;
	}
	progress_.step = static_cast<TutorialStepId>( static_cast<std::uint8_t>( progress_.step ) + 1 );
	repairMissingStarterBeds();
	pausedForLesson_ = true;
	evaluateCurrentStep();
}

void TutorialManager::skip()
{
	if( !active() ) return;
	progress_.skippedMask |= tutorialBit( progress_.step );
	advance();
}

void TutorialManager::restart()
{
	if( progress_.mode == TutorialMode::Off && !progress_.completed && !incompatible_ ) return;
	// Restart the guidance in-place so the world and real player work remain
	// intact. Preserve observed actions as well as item baselines: a felled tree
	// or harvested plant cannot necessarily be repeated after a restart.
	const bool migrationAlreadyHandled = migrationQueued_ || ( game_ && game_->gm() && game_->gm()->numGnomes() >= 8 );
	progress_.mode = TutorialMode::Interactive;
	progress_.scenarioId = "scenario_v2";
	progress_.scenarioVersion = 2;
	progress_.step = TutorialStepId::Orientation;
	progress_.completedMask = 0;
	progress_.skippedMask = 0;
	progress_.hintsEnabled = true;
	progress_.completed = false;
	incompatible_ = false;
	pausedForLesson_ = true;
	migrationQueued_ = migrationAlreadyHandled;
	lastSnapshot_ = {};
	emitIfChanged();
}

void TutorialManager::toggleHints()
{
	if( progress_.mode == TutorialMode::Off ) return;
	progress_.hintsEnabled = !progress_.hintsEnabled;
	emitIfChanged();
}

void TutorialManager::finish()
{
	if( progress_.mode == TutorialMode::Off ) return;
	if( ( progress_.skippedMask & tutorialBit( TutorialStepId::Graduation ) ) == 0 ) progress_.completedMask |= tutorialBit( TutorialStepId::Graduation );
	progress_.completed = true;
	progress_.mode = TutorialMode::Off;
	pausedForLesson_ = false;
	emitIfChanged();
}

void TutorialManager::continueAnyway()
{
	if( !active() || progress_.step != TutorialStepId::Graduation ) return;
	finish();
}

void TutorialManager::serialize( QVariantMap& out ) const
{
	if( progress_.mode == TutorialMode::Off && !progress_.completed && !incompatible_ ) return;
	QVariantMap tutorial;
	tutorial.insert( "mode", static_cast<int>( progress_.mode ) );
	tutorial.insert( "scenarioId", QString::fromStdString( progress_.scenarioId ) );
	tutorial.insert( "scenarioVersion", static_cast<uint>( progress_.scenarioVersion ) );
	tutorial.insert( "step", static_cast<int>( progress_.step ) );
	tutorial.insert( "completedMask", static_cast<uint>( progress_.completedMask ) );
	tutorial.insert( "skippedMask", static_cast<uint>( progress_.skippedMask ) );
	tutorial.insert( "hintsEnabled", progress_.hintsEnabled );
	tutorial.insert( "completed", progress_.completed );
	tutorial.insert( "incompatible", incompatible_ );
	tutorial.insert( "facts", static_cast<uint>( facts_ ) );
	tutorial.insert( "lessonRevision", 4 );
	tutorial.insert( "workshopStockpileLinked", workshopStockpileLinked_ );
	tutorial.insert( "stockpileAllowsRawWood", stockpileAllowsRawWood_ );
	tutorial.insert( "starterBedRepairApplied", starterBedRepairApplied_ );
	tutorial.insert( "migrationQueued", migrationQueued_ );
	tutorial.insert( "baselinePlank", baselinePlank_ );
	tutorial.insert( "baselineFlour", baselineFlour_ );
	tutorial.insert( "baselineBread", baselineBread_ );
	tutorial.insert( "baselineFruit", baselineFruit_ );
	QVariantMap professions;
	for( auto it = baselineProfessions_.cbegin(); it != baselineProfessions_.cend(); ++it ) professions.insert( QString::number( it.key() ), it.value() );
	tutorial.insert( "baselineProfessions", professions );
	QVariantList excavationJobs;
	for( const auto id : completedExcavationJobs_ ) excavationJobs.append( id );
	tutorial.insert( "completedExcavationJobs", excavationJobs );
	out.insert( "tutorial", tutorial );
}

bool TutorialManager::deserialize( const QVariantMap& in )
{
	const auto tutorial = in.value( "tutorial" ).toMap();
	if( tutorial.isEmpty() )
	{
		progress_ = {};
		facts_ = {};
		workshopStockpileLinked_ = false;
		stockpileAllowsRawWood_ = false;
		starterBedRepairApplied_ = false;
		incompatible_ = false;
		emitIfChanged();
		return true;
	}
	const auto version = tutorial.value( "scenarioVersion", 0 ).toUInt();
	if( version != 2 || tutorial.value( "scenarioId" ).toString() != QStringLiteral( "scenario_v2" ) )
	{
		progress_ = {};
		progress_.scenarioId = tutorial.value( "scenarioId" ).toString().toStdString();
		progress_.scenarioVersion = version;
		facts_ = {};
		workshopStockpileLinked_ = false;
		stockpileAllowsRawWood_ = false;
		starterBedRepairApplied_ = false;
		incompatible_ = true;
		emitIfChanged();
		return false;
	}
	progress_.mode = static_cast<TutorialMode>( tutorial.value( "mode" ).toInt() );
	progress_.scenarioVersion = version;
	progress_.step = static_cast<TutorialStepId>( tutorial.value( "step" ).toInt() );
	progress_.completedMask = tutorial.value( "completedMask" ).toUInt();
	progress_.skippedMask = tutorial.value( "skippedMask" ).toUInt();
	progress_.hintsEnabled = tutorial.value( "hintsEnabled", true ).toBool();
	progress_.completed = tutorial.value( "completed" ).toBool();
	progress_.scenarioId = "scenario_v2";
	incompatible_ = false;
	facts_ = tutorial.value( "facts" ).toUInt();
	workshopStockpileLinked_ = tutorial.value( "workshopStockpileLinked", false ).toBool();
	stockpileAllowsRawWood_ = tutorial.value( "stockpileAllowsRawWood", false ).toBool();
	starterBedRepairApplied_ = tutorial.value( "starterBedRepairApplied", false ).toBool();
	stockpileAllowsRawWood_ = hasAllowedRawWoodStockpile();
	if( tutorial.value( "lessonRevision", 1 ).toInt() < 2 && progress_.step == TutorialStepId::InspectAndAssign )
		facts_ &= ~( tutorialFactBit( TutorialFact::InspectGnome ) | tutorialFactBit( TutorialFact::OpenPopulation ) | tutorialFactBit( TutorialFact::OpenInventory ) | tutorialFactBit( TutorialFact::AssignWork ) );
	if( tutorial.value( "lessonRevision", 1 ).toInt() < 4
		&& ( progress_.step == TutorialStepId::Stockpile || progress_.step == TutorialStepId::Crafting ) )
	{
		progress_.step = TutorialStepId::Stockpile;
		progress_.completedMask &= ~( tutorialBit( TutorialStepId::Stockpile ) | tutorialBit( TutorialStepId::Crafting ) );
		progress_.skippedMask &= ~( tutorialBit( TutorialStepId::Stockpile ) | tutorialBit( TutorialStepId::Crafting ) );
	}
	migrationQueued_ = tutorial.value( "migrationQueued", false ).toBool();
	baselinePlank_ = tutorial.value( "baselinePlank", 0 ).toInt();
	baselineFlour_ = tutorial.value( "baselineFlour", 0 ).toInt();
	baselineBread_ = tutorial.value( "baselineBread", 0 ).toInt();
	baselineFruit_ = tutorial.value( "baselineFruit", 0 ).toInt();
	baselineProfessions_.clear();
	const auto baselineProfessions = tutorial.value( "baselineProfessions" ).toMap();
	for( auto it = baselineProfessions.cbegin(); it != baselineProfessions.cend(); ++it ) baselineProfessions_.insert( it.key().toUInt(), it.value().toString() );
	completedExcavationJobs_.clear();
	for( const auto& id : tutorial.value( "completedExcavationJobs" ).toList() ) completedExcavationJobs_.insert( id.toUInt() );
	pausedForLesson_ = active();
	repairMissingStarterBeds();
	emitIfChanged();
	return true;
}

void TutorialManager::evaluateCurrentStep()
{
	if( !active() )
	{
		emitIfChanged();
		return;
	}
	const auto bit = tutorialBit( progress_.step );
	if( ( facts_ & requiredFacts( progress_.step ) ) == requiredFacts( progress_.step )
		&& ( progress_.step != TutorialStepId::Stockpile || hasLinkedAllowedRawWoodStockpile() ) )
	{
		progress_.completedMask |= bit;
		pausedForLesson_ = true;
	}
	else if( progress_.step == TutorialStepId::Stockpile ) progress_.completedMask &= ~bit;
	emitIfChanged();
}

void TutorialManager::emitIfChanged()
{
	const auto value = snapshot();
	if( value == lastSnapshot_ ) return;
	lastSnapshot_ = value;
	GameState::tutorial.clear();
	serialize( GameState::tutorial );
	if( active() && pausedForLesson_ && game_ && !game_->paused() ) game_->setPaused( true );
	emit signalSnapshot( value );
}

TutorialSnapshot TutorialManager::snapshot() const
{
	TutorialSnapshot value;
	value.active = active();
	value.step = static_cast<std::uint8_t>( progress_.step );
	value.completedMask = progress_.completedMask;
	value.skippedMask = progress_.skippedMask;
	value.hintsEnabled = progress_.hintsEnabled;
	value.pausedForLesson = pausedForLesson_;
	value.completed = progress_.completed;
	value.incompatible = incompatible_;
	value.scenarioId = QString::fromStdString( progress_.scenarioId );
	value.title = incompatible_ ? QStringLiteral( "tutorial.incompatible.title" ) : titleFor( progress_.step );
	value.explanation = incompatible_ ? QStringLiteral( "tutorial.incompatible.explanation" ) : explanationFor( progress_.step );
	value.objective = incompatible_ ? QString{} : objectiveFor( progress_.step );
	value.steps = incompatible_ ? QStringList{} : stepsFor( progress_.step );
	value.completedSteps = incompatible_ ? QVector<bool>{} : completedStepsFor( progress_.step );
	value.progress = incompatible_ ? QString{} : QStringLiteral( "%1 / 9" ).arg( static_cast<int>( progress_.step ) + 1 );
	// Point at the next unfinished interaction. The target changes as the player
	// completes each action, including actions completed before its lesson opens.
	static const QVector<QStringList> highlights{
		{ "", "", "", "", "hud_level_down", "hud_pause", "hud_speed_fast" },
		{ "hud_tool_inspect", "", "", "hud_open_population", "hud_open_inventory" },
		{ "hud_tool_agriculture", "hud_tool_inspect" },
		{ "hud_tool_mine", "hud_tool_mine" },
		{ "hud_tool_build", "hud_tool_designations", "hud_tool_inspect", "", "" },
		{ "", "hud_open_inventory" },
		{ "hud_tool_designations", "", "", "hud_pause" },
		{ "hud_tool_designations", "hud_tool_build" },
		{ "tutorial-finish" } };
	const auto index = static_cast<int>( progress_.step );
	if( !incompatible_ && index < highlights.size() )
	{
		const auto done = value.completedSteps;
		int next = 0;
		while( next < done.size() && done[next] ) ++next;
		if( next < highlights[index].size() )
		{
			value.highlightedIds = { highlights[index][next] };
			if( progress_.step == TutorialStepId::Gathering && next == 0 ) value.highlightedIds << "hud_tool_fell_tree";
			else if( progress_.step == TutorialStepId::MiningAndLevels ) value.highlightedIds << ( next == 0 ? "hud_mine_stairs_down" : "hud_mine_walls" );
			else if( progress_.step == TutorialStepId::Stockpile && next == 0 ) value.highlightedIds << "hud_build_workshop";
			else if( progress_.step == TutorialStepId::Stockpile && next == 1 ) value.highlightedIds << "hud_tool_stockpile";
			else if( progress_.step == TutorialStepId::Farming && next == 0 ) value.highlightedIds << "hud_tool_farm";
			else if( progress_.step == TutorialStepId::Shelter && next == 0 ) value.highlightedIds << "hud_tool_dormitory";
			else if( progress_.step == TutorialStepId::Shelter && next == 1 ) value.highlightedIds << "hud_build_furniture";
		}
	}
	return value;
}

QString TutorialManager::titleFor( TutorialStepId step ) const { return stepName( step ); }
QString TutorialManager::explanationFor( TutorialStepId step ) const
{
	if( step == TutorialStepId::InspectAndAssign ) return QStringLiteral( "tutorial.explanation.inspect_assign" );
	if( step == TutorialStepId::Stockpile ) return QStringLiteral( "tutorial.explanation.stockpile" );
	if( step == TutorialStepId::Farming ) return QStringLiteral( "tutorial.explanation.farming" );
	return QStringLiteral( "tutorial.explanation" );
}
QString TutorialManager::objectiveFor( TutorialStepId step ) const
{
	static const QStringList objectives{
		QStringLiteral( "tutorial.objective.orientation" ), QStringLiteral( "tutorial.objective.inspect_assign" ),
		QStringLiteral( "tutorial.objective.gathering" ), QStringLiteral( "tutorial.objective.mining_levels" ),
		QStringLiteral( "tutorial.objective.stockpile" ), QStringLiteral( "tutorial.objective.crafting" ),
		QStringLiteral( "tutorial.objective.farming" ), QStringLiteral( "tutorial.objective.shelter" ),
		QStringLiteral( "tutorial.objective.graduation" ) };
	const auto index = static_cast<std::size_t>( step );
	return index < objectives.size() ? objectives[index] : QString{};
}

QStringList TutorialManager::stepsFor( TutorialStepId step ) const
{
	const bool wheelChangesLevel = Global::cfg && Global::cfg->get( "toggleMouseWheel" ).toBool();
	const QStringList orientation{
		QStringLiteral( "tutorial.steps.orientation.pan" ),
		wheelChangesLevel ? QStringLiteral( "tutorial.steps.orientation.wheel_level_ctrl" ) : QStringLiteral( "tutorial.steps.orientation.wheel_level" ),
		wheelChangesLevel ? QStringLiteral( "tutorial.steps.orientation.zoom_plain" ) : QStringLiteral( "tutorial.steps.orientation.zoom" ),
		QStringLiteral( "tutorial.steps.orientation.rotate" ), QStringLiteral( "tutorial.steps.orientation.level" ),
		QStringLiteral( "tutorial.steps.orientation.pause" ), QStringLiteral( "tutorial.steps.orientation.speed" ) };
	static const QStringList inspect{
		QStringLiteral( "tutorial.steps.inspect.select" ), QStringLiteral( "tutorial.steps.inspect.inspect" ),
		QStringLiteral( "tutorial.steps.inspect.assignment" ), QStringLiteral( "tutorial.steps.inspect.population" ),
		QStringLiteral( "tutorial.steps.inspect.inventory" ) };
	static const QStringList gathering{
		QStringLiteral( "tutorial.steps.gathering.wood" ), QStringLiteral( "tutorial.steps.gathering.food" ) };
	static const QStringList mining{
		QStringLiteral( "tutorial.steps.mining.stairs" ), QStringLiteral( "tutorial.steps.mining.wall" ) };
	static const QStringList stockpile{ QStringLiteral( "tutorial.steps.stockpile.workshop" ),
		QStringLiteral( "tutorial.steps.stockpile.designate" ), QStringLiteral( "tutorial.steps.stockpile.allow" ), QStringLiteral( "tutorial.steps.stockpile.open" ),
		QStringLiteral( "tutorial.steps.stockpile.link" ) };
	static const QStringList farming{
		QStringLiteral( "tutorial.steps.farming.designate" ), QStringLiteral( "tutorial.steps.farming.crop" ),
		QStringLiteral( "tutorial.steps.farming.queue" ),
		QStringLiteral( "tutorial.steps.farming.plant" ) };
	static const QStringList crafting{
		QStringLiteral( "tutorial.steps.crafting.recipe" ), QStringLiteral( "tutorial.steps.crafting.collect" ) };
	static const QStringList shelter{
		QStringLiteral( "tutorial.steps.shelter.room" ), QStringLiteral( "tutorial.steps.shelter.beds" ) };
	static const QStringList graduation{
		QStringLiteral( "tutorial.steps.graduation.review" ), QStringLiteral( "tutorial.steps.graduation.continue" ) };
	switch( step )
	{
	case TutorialStepId::Orientation: return orientation;
	case TutorialStepId::InspectAndAssign: return inspect;
	case TutorialStepId::Gathering: return gathering;
	case TutorialStepId::MiningAndLevels: return mining;
	case TutorialStepId::Stockpile: return stockpile;
	case TutorialStepId::Crafting: return crafting;
	case TutorialStepId::Farming: return farming;
	case TutorialStepId::Shelter: return shelter;
	case TutorialStepId::Graduation: return graduation;
	}
	return {};
}

QVector<bool> TutorialManager::completedStepsFor( TutorialStepId step ) const
{
	if( step == TutorialStepId::Stockpile )
		return { ( facts_ & tutorialFactBit( TutorialFact::Workshop ) ) != 0,
			( facts_ & tutorialFactBit( TutorialFact::Stockpile ) ) != 0,
			stockpileAllowsRawWood_,
			( facts_ & tutorialFactBit( TutorialFact::OpenWorkshop ) ) != 0,
			workshopStockpileLinked_ && hasLinkedAllowedRawWoodStockpile() };
	static const auto factsFor = []( TutorialStepId value ) {
		switch( value )
		{
		case TutorialStepId::Orientation: return QVector<TutorialFact>{ TutorialFact::Pan, TutorialFact::WheelLevel, TutorialFact::Zoom, TutorialFact::Rotate, TutorialFact::ChangeLevel, TutorialFact::PauseResume, TutorialFact::ChangeSpeed };
		case TutorialStepId::InspectAndAssign: return QVector<TutorialFact>{ TutorialFact::SelectGnome, TutorialFact::InspectGnome, TutorialFact::AssignWork, TutorialFact::OpenPopulation, TutorialFact::OpenInventory };
		case TutorialStepId::Gathering: return QVector<TutorialFact>{ TutorialFact::FellTree, TutorialFact::Harvest };
		case TutorialStepId::MiningAndLevels: return QVector<TutorialFact>{ TutorialFact::LowerLevel, TutorialFact::MineWall };
		case TutorialStepId::Crafting: return QVector<TutorialFact>{ TutorialFact::QueuePlank, TutorialFact::Plank };
		case TutorialStepId::Farming: return QVector<TutorialFact>{ TutorialFact::Farm, TutorialFact::CropSelected, TutorialFact::CropQueued, TutorialFact::Plant };
		case TutorialStepId::Shelter: return QVector<TutorialFact>{ TutorialFact::Shelter, TutorialFact::Beds };
		case TutorialStepId::Graduation: return QVector<TutorialFact>{};
		}
		return QVector<TutorialFact>{};
	};
	QVector<bool> result;
	for( const auto fact : factsFor( step ) ) result.push_back( ( facts_ & tutorialFactBit( fact ) ) != 0 );
	return result;
}

std::uint32_t TutorialManager::requiredFacts( TutorialStepId step ) const noexcept
{
	switch( step )
	{
	case TutorialStepId::Orientation: return tutorialFactBit( TutorialFact::Pan ) | tutorialFactBit( TutorialFact::WheelLevel ) | tutorialFactBit( TutorialFact::Zoom ) | tutorialFactBit( TutorialFact::Rotate ) | tutorialFactBit( TutorialFact::ChangeLevel ) | tutorialFactBit( TutorialFact::PauseResume ) | tutorialFactBit( TutorialFact::ChangeSpeed );
	case TutorialStepId::InspectAndAssign: return tutorialFactBit( TutorialFact::SelectGnome ) | tutorialFactBit( TutorialFact::InspectGnome ) | tutorialFactBit( TutorialFact::OpenPopulation ) | tutorialFactBit( TutorialFact::OpenInventory ) | tutorialFactBit( TutorialFact::AssignWork );
	case TutorialStepId::Gathering: return tutorialFactBit( TutorialFact::FellTree ) | tutorialFactBit( TutorialFact::Harvest );
	case TutorialStepId::MiningAndLevels: return tutorialFactBit( TutorialFact::LowerLevel ) | tutorialFactBit( TutorialFact::MineWall );
	case TutorialStepId::Stockpile: return tutorialFactBit( TutorialFact::Workshop ) | tutorialFactBit( TutorialFact::Stockpile ) | tutorialFactBit( TutorialFact::OpenWorkshop );
	case TutorialStepId::Crafting: return tutorialFactBit( TutorialFact::QueuePlank ) | tutorialFactBit( TutorialFact::Plank );
	case TutorialStepId::Farming: return tutorialFactBit( TutorialFact::Farm ) | tutorialFactBit( TutorialFact::CropSelected ) | tutorialFactBit( TutorialFact::CropQueued ) | tutorialFactBit( TutorialFact::Plant );
	case TutorialStepId::Shelter: return tutorialFactBit( TutorialFact::Shelter ) | tutorialFactBit( TutorialFact::Beds );
	case TutorialStepId::Graduation: return 0;
	}
	return 0;
}
