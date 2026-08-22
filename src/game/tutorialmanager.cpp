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
#include "jobmanager.h"
#include "job.h"
#include "world.h"
#include "../base/gamestate.h"

#include <QStringList>

namespace
{
QString stepName( TutorialStepId step )
{
	switch( step )
	{
	case TutorialStepId::Orientation: return QStringLiteral( "tutorial.step.orientation" );
	case TutorialStepId::InspectAndAssign: return QStringLiteral( "tutorial.step.inspect_assign" );
	case TutorialStepId::ShelterAndStorage: return QStringLiteral( "tutorial.step.shelter_storage" );
	case TutorialStepId::MiningAndLevels: return QStringLiteral( "tutorial.step.mining_levels" );
	case TutorialStepId::Farming: return QStringLiteral( "tutorial.step.farming" );
	case TutorialStepId::Crafting: return QStringLiteral( "tutorial.step.crafting" );
	case TutorialStepId::Cooking: return QStringLiteral( "tutorial.step.cooking" );
	case TutorialStepId::PopulationExpansion: return QStringLiteral( "tutorial.step.population" );
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
	progress_.scenarioId = scenarioId ? scenarioId : "scenario_v1";
	progress_.scenarioVersion = version;
	progress_.hintsEnabled = true;
	incompatible_ = false;
	facts_ = {};
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
	if( game_->rm() && !game_->rm()->allRooms().isEmpty() )
	{
		facts_ |= tutorialFactBit( TutorialFact::Shelter );
		int beds = 0;
		for( auto* room : game_->rm()->allRooms() ) if( room ) beds += room->numBeds();
		if( beds >= 5 ) facts_ |= tutorialFactBit( TutorialFact::Beds );
	}
	if( game_->spm() && !game_->spm()->allStockpiles().isEmpty() ) facts_ |= tutorialFactBit( TutorialFact::Stockpile );
	if( game_->fm() && game_->fm()->countFarms() > 0 )
	{
		facts_ |= tutorialFactBit( TutorialFact::Farm );
		for( auto* farm : game_->fm()->allFarms() )
		{
			if( !farm ) continue;
			if( !farm->plantType().isEmpty() ) facts_ |= tutorialFactBit( TutorialFact::CropSelected );
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
			if( workshop->type() == "Crude" ) facts_ |= tutorialFactBit( TutorialFact::Workshop );
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
	if( game_ && game_->gm() )
		for( auto* gnome : game_->gm()->gnomes() )
			if( gnome && baselineProfessions_.contains( gnome->id() ) && gnome->profession() != baselineProfessions_.value( gnome->id() ) ) facts_ |= tutorialFactBit( TutorialFact::AssignWork );
	if( game_ && game_->jm() )
		for( auto it = game_->jm()->allJobs().cbegin(); it != game_->jm()->allJobs().cend(); ++it )
			if( it.value() && it.value()->phase() == JobPhase::COMPLETE )
			{
				const auto type = it.value()->type();
				if( type == "Harvest" ) facts_ |= tutorialFactBit( TutorialFact::Harvest );
				if( type == "Mine" || type == "RemoveFloor" || type == "DigHole" || type == "DigStairsDown" || type == "MineStairsUp" )
				{
					completedExcavationJobs_.insert( it.key() );
					facts_ |= tutorialFactBit( TutorialFact::Excavation );
					if( type != "Mine" ) facts_ |= tutorialFactBit( TutorialFact::LowerLevel );
				}
			}
	// Keep the migration deterministic and let EventManager own its prompt and
	// acceptance semantics.  The event is queued once after the initial lesson
	// window, so a paused tutorial is still immediately playable.
	if( !migrationQueued_ && game_ && game_->em() && progress_.step == TutorialStepId::PopulationExpansion )
	{
		QVariantMap args;
		args.insert( "Amount", 3 );
		Position migrationAnchor = GameState::origin;
		migrationAnchor.x += 16;
		if( game_->w() ) game_->w()->getFloorLevelBelow( migrationAnchor, false );
		if( game_->w() && !game_->w()->isWalkableGnome( migrationAnchor ) )
		{
			for( int radius = 1; radius <= 4 && !game_->w()->isWalkableGnome( migrationAnchor ); ++radius )
				for( int dx = -radius; dx <= radius && !game_->w()->isWalkableGnome( migrationAnchor ); ++dx )
					for( int dy = -radius; dy <= radius && !game_->w()->isWalkableGnome( migrationAnchor ); ++dy )
					{
						Position candidate = migrationAnchor;
						candidate.x += dx;
						candidate.y += dy;
						game_->w()->getFloorLevelBelow( candidate, false );
						if( game_->w()->isWalkableGnome( candidate ) ) migrationAnchor = candidate;
					}
		}
		args.insert( "Location", migrationAnchor.toString() );
		game_->em()->onDebugEvent( EventType::MIGRATION, args );
		migrationQueued_ = true;
	}
	if( game_ && game_->gm() && game_->gm()->numGnomes() >= 8 ) facts_ |= tutorialFactBit( TutorialFact::Migration );
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
	// intact. Scenario baselines are preserved, so prior crafted items still
	// count as out-of-order progress after the restart.
	const bool migrationAlreadyHandled = migrationQueued_ || ( game_ && game_->gm() && game_->gm()->numGnomes() >= 8 );
	progress_.mode = TutorialMode::Interactive;
	progress_.step = TutorialStepId::Orientation;
	progress_.completedMask = 0;
	progress_.skippedMask = 0;
	progress_.hintsEnabled = true;
	progress_.completed = false;
	incompatible_ = false;
	facts_ = 0;
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
		incompatible_ = false;
		emitIfChanged();
		return true;
	}
	const auto version = tutorial.value( "scenarioVersion", 0 ).toUInt();
	if( version != 1 || tutorial.value( "scenarioId" ).toString() != QStringLiteral( "scenario_v1" ) )
	{
		progress_ = {};
		progress_.scenarioId = tutorial.value( "scenarioId" ).toString().toStdString();
		progress_.scenarioVersion = version;
		facts_ = {};
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
	progress_.scenarioId = "scenario_v1";
	incompatible_ = false;
	facts_ = tutorial.value( "facts" ).toUInt();
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
	if( ( facts_ & requiredFacts( progress_.step ) ) == requiredFacts( progress_.step ) )
	{
		progress_.completedMask |= bit;
		pausedForLesson_ = true;
	}
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
	static const QStringList highlights{
		QStringLiteral( "hud_tool_inspect" ), QStringLiteral( "hud_open_population" ), QStringLiteral( "hud_tool_build" ),
		QStringLiteral( "hud_tool_mine" ), QStringLiteral( "hud_tool_agriculture" ), QStringLiteral( "hud_tool_build" ),
		QStringLiteral( "hud_tool_build" ), QStringLiteral( "hud_open_population" ), QStringLiteral( "tutorial-finish" ) };
	if( !incompatible_ && static_cast<int>( progress_.step ) < highlights.size() ) value.highlightedIds = { highlights[static_cast<int>( progress_.step)] };
	return value;
}

QString TutorialManager::titleFor( TutorialStepId step ) const { return stepName( step ); }
QString TutorialManager::explanationFor( TutorialStepId step ) const
{
	Q_UNUSED( step );
	return QStringLiteral( "tutorial.explanation" );
}
QString TutorialManager::objectiveFor( TutorialStepId step ) const
{
	static const QStringList objectives{
		QStringLiteral( "tutorial.objective.orientation" ), QStringLiteral( "tutorial.objective.inspect_assign" ),
		QStringLiteral( "tutorial.objective.shelter_storage" ), QStringLiteral( "tutorial.objective.mining_levels" ),
		QStringLiteral( "tutorial.objective.farming" ), QStringLiteral( "tutorial.objective.crafting" ),
		QStringLiteral( "tutorial.objective.cooking" ), QStringLiteral( "tutorial.objective.population" ),
		QStringLiteral( "tutorial.objective.graduation" ) };
	const auto index = static_cast<std::size_t>( step );
	return index < objectives.size() ? objectives[index] : QString{};
}

QStringList TutorialManager::stepsFor( TutorialStepId step ) const
{
	static const QStringList orientation{
		QStringLiteral( "tutorial.steps.orientation.pan" ), QStringLiteral( "tutorial.steps.orientation.zoom" ),
		QStringLiteral( "tutorial.steps.orientation.rotate" ), QStringLiteral( "tutorial.steps.orientation.level" ),
		QStringLiteral( "tutorial.steps.orientation.pause" ), QStringLiteral( "tutorial.steps.orientation.speed" ) };
	static const QStringList inspect{
		QStringLiteral( "tutorial.steps.inspect.select" ), QStringLiteral( "tutorial.steps.inspect.inspect" ),
		QStringLiteral( "tutorial.steps.inspect.population" ), QStringLiteral( "tutorial.steps.inspect.inventory" ),
		QStringLiteral( "tutorial.steps.inspect.assignment" ) };
	static const QStringList shelter{
		QStringLiteral( "tutorial.steps.shelter.build" ), QStringLiteral( "tutorial.steps.shelter.room" ),
		QStringLiteral( "tutorial.steps.shelter.beds" ), QStringLiteral( "tutorial.steps.shelter.stockpile" ) };
	static const QStringList mining{
		QStringLiteral( "tutorial.steps.mining.open" ), QStringLiteral( "tutorial.steps.mining.wall" ),
		QStringLiteral( "tutorial.steps.mining.floor" ), QStringLiteral( "tutorial.steps.mining.stairs" ) };
	static const QStringList farming{
		QStringLiteral( "tutorial.steps.farming.designate" ), QStringLiteral( "tutorial.steps.farming.crop" ),
		QStringLiteral( "tutorial.steps.farming.plant" ), QStringLiteral( "tutorial.steps.farming.harvest" ) };
	static const QStringList crafting{
		QStringLiteral( "tutorial.steps.crafting.build" ), QStringLiteral( "tutorial.steps.crafting.recipe" ),
		QStringLiteral( "tutorial.steps.crafting.collect" ) };
	static const QStringList cooking{
		QStringLiteral( "tutorial.steps.cooking.kitchen" ), QStringLiteral( "tutorial.steps.cooking.flour" ),
		QStringLiteral( "tutorial.steps.cooking.bread" ) };
	static const QStringList population{
		QStringLiteral( "tutorial.steps.population.prompt" ), QStringLiteral( "tutorial.steps.population.accept" ),
		QStringLiteral( "tutorial.steps.population.confirm" ) };
	static const QStringList graduation{
		QStringLiteral( "tutorial.steps.graduation.review" ), QStringLiteral( "tutorial.steps.graduation.continue" ) };
	switch( step )
	{
	case TutorialStepId::Orientation: return orientation;
	case TutorialStepId::InspectAndAssign: return inspect;
	case TutorialStepId::ShelterAndStorage: return shelter;
	case TutorialStepId::MiningAndLevels: return mining;
	case TutorialStepId::Farming: return farming;
	case TutorialStepId::Crafting: return crafting;
	case TutorialStepId::Cooking: return cooking;
	case TutorialStepId::PopulationExpansion: return population;
	case TutorialStepId::Graduation: return graduation;
	}
	return {};
}

QVector<bool> TutorialManager::completedStepsFor( TutorialStepId step ) const
{
	static const auto factsFor = []( TutorialStepId value ) {
		switch( value )
		{
		case TutorialStepId::Orientation: return QVector<TutorialFact>{ TutorialFact::Pan, TutorialFact::Zoom, TutorialFact::Rotate, TutorialFact::ChangeLevel, TutorialFact::PauseResume, TutorialFact::ChangeSpeed };
		case TutorialStepId::InspectAndAssign: return QVector<TutorialFact>{ TutorialFact::SelectGnome, TutorialFact::InspectGnome, TutorialFact::OpenPopulation, TutorialFact::OpenInventory, TutorialFact::AssignWork };
		case TutorialStepId::ShelterAndStorage: return QVector<TutorialFact>{ TutorialFact::Shelter, TutorialFact::Shelter, TutorialFact::Beds, TutorialFact::Stockpile };
		case TutorialStepId::MiningAndLevels: return QVector<TutorialFact>{ TutorialFact::Excavation, TutorialFact::Excavation, TutorialFact::LowerLevel, TutorialFact::LowerLevel };
		case TutorialStepId::Farming: return QVector<TutorialFact>{ TutorialFact::Farm, TutorialFact::CropSelected, TutorialFact::Plant, TutorialFact::Harvest };
		case TutorialStepId::Crafting: return QVector<TutorialFact>{ TutorialFact::Workshop, TutorialFact::Workshop, TutorialFact::Plank };
		case TutorialStepId::Cooking: return QVector<TutorialFact>{ TutorialFact::Kitchen, TutorialFact::Flour, TutorialFact::Bread };
		case TutorialStepId::PopulationExpansion: return QVector<TutorialFact>{ TutorialFact::Migration, TutorialFact::Migration, TutorialFact::Migration };
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
	case TutorialStepId::Orientation: return tutorialFactBit( TutorialFact::Pan ) | tutorialFactBit( TutorialFact::Zoom ) | tutorialFactBit( TutorialFact::Rotate ) | tutorialFactBit( TutorialFact::ChangeLevel ) | tutorialFactBit( TutorialFact::PauseResume ) | tutorialFactBit( TutorialFact::ChangeSpeed );
	case TutorialStepId::InspectAndAssign: return tutorialFactBit( TutorialFact::SelectGnome ) | tutorialFactBit( TutorialFact::InspectGnome ) | tutorialFactBit( TutorialFact::OpenPopulation ) | tutorialFactBit( TutorialFact::OpenInventory ) | tutorialFactBit( TutorialFact::AssignWork );
	case TutorialStepId::ShelterAndStorage: return tutorialFactBit( TutorialFact::Shelter ) | tutorialFactBit( TutorialFact::Beds ) | tutorialFactBit( TutorialFact::Stockpile );
	case TutorialStepId::MiningAndLevels: return tutorialFactBit( TutorialFact::Excavation ) | tutorialFactBit( TutorialFact::LowerLevel );
	case TutorialStepId::Farming: return tutorialFactBit( TutorialFact::Farm ) | tutorialFactBit( TutorialFact::CropSelected ) | tutorialFactBit( TutorialFact::Plant ) | tutorialFactBit( TutorialFact::Harvest );
	case TutorialStepId::Crafting: return tutorialFactBit( TutorialFact::Workshop ) | tutorialFactBit( TutorialFact::Plank );
	case TutorialStepId::Cooking: return tutorialFactBit( TutorialFact::Kitchen ) | tutorialFactBit( TutorialFact::Flour ) | tutorialFactBit( TutorialFact::Bread );
	case TutorialStepId::PopulationExpansion: return tutorialFactBit( TutorialFact::Migration );
	case TutorialStepId::Graduation: return tutorialBit( TutorialStepId::ShelterAndStorage ) | tutorialBit( TutorialStepId::MiningAndLevels ) | tutorialBit( TutorialStepId::Farming ) | tutorialBit( TutorialStepId::Crafting ) | tutorialBit( TutorialStepId::Cooking ) | tutorialBit( TutorialStepId::PopulationExpansion );
	}
	return 0;
}
