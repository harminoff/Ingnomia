/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "tutorialtypes.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class Game;

/** A small, serialisable projection of tutorial state for the HUD. */
struct TutorialSnapshot
{
	bool active{};
	std::uint8_t step{};
	std::uint8_t stepCount{ 9 };
	std::uint32_t completedMask{};
	std::uint32_t skippedMask{};
	bool hintsEnabled{};
	bool pausedForLesson{};
	bool completed{};
	bool incompatible{};
	QString scenarioId;
	QString title;
	QString explanation;
	QString objective;
	QStringList steps;
	QVector<bool> completedSteps;
	QString progress;
	QStringList highlightedIds;
	bool operator==( const TutorialSnapshot& ) const = default;
};
Q_DECLARE_METATYPE( TutorialSnapshot )

/**
 * Game-side tutorial state machine.  It deliberately observes facts rather
 * than issuing gameplay commands; the normal job, build, farm and event
 * systems remain authoritative.
 */
class TutorialManager final : public QObject
{
	Q_OBJECT
public:
	explicit TutorialManager( Game* game );

	[[nodiscard]] const TutorialProgress& progress() const noexcept { return progress_; }
	[[nodiscard]] TutorialSnapshot snapshot() const;
	[[nodiscard]] bool active() const noexcept { return progress_.mode == TutorialMode::Interactive && !progress_.completed; }
	[[nodiscard]] bool tutorialMode() const noexcept { return progress_.mode == TutorialMode::Interactive; }
	[[nodiscard]] bool incompatible() const noexcept { return incompatible_; }

	void start( const char* scenarioId = "scenario_v1", std::uint32_t version = 1 );
	void tick();
	void onPauseChanged( bool paused );
	void observeFact( TutorialFact fact );
	void observeFacts( std::uint32_t facts );
	void advance();
	void skip();
	void restart();
	void toggleHints();
	void finish();
	void continueAnyway();

	void serialize( QVariantMap& out ) const;
	bool deserialize( const QVariantMap& in );

signals:
	void signalSnapshot( TutorialSnapshot snapshot );

private:
	void evaluateCurrentStep();
	void emitIfChanged();
	QString titleFor( TutorialStepId step ) const;
	QString explanationFor( TutorialStepId step ) const;
	QString objectiveFor( TutorialStepId step ) const;
	QStringList stepsFor( TutorialStepId step ) const;
	QVector<bool> completedStepsFor( TutorialStepId step ) const;
	std::uint32_t requiredFacts( TutorialStepId step ) const noexcept;

	Game* game_{};
	TutorialProgress progress_;
	std::uint32_t facts_{};
	bool pausedForLesson_{};
	bool migrationQueued_{};
	bool incompatible_{};
	int baselinePlank_{};
	int baselineFlour_{};
	int baselineBread_{};
	int baselineFruit_{};
	QHash<unsigned int, QString> baselineProfessions_;
	QSet<unsigned int> completedExcavationJobs_;
	TutorialSnapshot lastSnapshot_;
};
