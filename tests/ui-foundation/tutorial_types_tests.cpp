#include "../../src/game/tutorialtypes.h"

#include <cassert>

int main()
{
	assert( tutorialBit( TutorialStepId::Orientation ) == 1u );
	assert( tutorialBit( TutorialStepId::Graduation ) == ( 1u << 8 ) );
	assert( tutorialBit( TutorialStepId::Gathering ) == ( 1u << 2 ) );
	assert( tutorialBit( TutorialStepId::Stockpile ) == ( 1u << 4 ) );
	assert( static_cast<unsigned>( TutorialFact::Count ) <= 32 );
	assert( tutorialFactBit( TutorialFact::QueuePlank ) != tutorialFactBit( TutorialFact::Plank ) );
	assert( tutorialFactBit( TutorialFact::Pan ) != tutorialFactBit( TutorialFact::Bread ) );
	assert( tutorialFactBit( TutorialFact::CropQueued ) != tutorialFactBit( TutorialFact::CropSelected ) );
	TutorialProgress progress;
	progress.mode = TutorialMode::Interactive;
	progress.completedMask |= tutorialBit( TutorialStepId::Farming );
	progress.skippedMask |= tutorialBit( TutorialStepId::Shelter );
	assert( ( progress.completedMask & tutorialBit( TutorialStepId::Farming ) ) != 0 );
	assert( ( progress.skippedMask & tutorialBit( TutorialStepId::Shelter ) ) != 0 );
	return 0;
}
