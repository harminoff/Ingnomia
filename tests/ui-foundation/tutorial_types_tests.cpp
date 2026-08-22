#include "../../src/game/tutorialtypes.h"

#include <cassert>

int main()
{
	assert( tutorialBit( TutorialStepId::Orientation ) == 1u );
	assert( tutorialBit( TutorialStepId::Graduation ) == ( 1u << 8 ) );
	assert( tutorialFactBit( TutorialFact::Pan ) != tutorialFactBit( TutorialFact::Bread ) );
	TutorialProgress progress;
	progress.mode = TutorialMode::Interactive;
	progress.completedMask |= tutorialBit( TutorialStepId::Farming );
	progress.skippedMask |= tutorialBit( TutorialStepId::PopulationExpansion );
	assert( ( progress.completedMask & tutorialBit( TutorialStepId::Farming ) ) != 0 );
	assert( ( progress.skippedMask & tutorialBit( TutorialStepId::PopulationExpansion ) ) != 0 );
	return 0;
}
