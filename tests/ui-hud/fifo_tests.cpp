#include "gui/ui/screens/hud/HudController.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace ingnomia::ui;
using namespace ingnomia::ui::hud;

namespace
{
struct Port final : HudCommandPort
{
	std::vector<UiActionEnvelope> actions;
	std::vector<PromptInstanceId> registered;

	CommandResult dispatch( const UiActionEnvelope& action ) override
	{
		actions.push_back( action );
		return {};
	}

	void registerPrompt( PromptInstanceId prompt, std::optional<EventResponseTargetId> ) override
	{
		registered.push_back( prompt );
	}
};

struct View final : HudViewPort
{
	HudState state;

	void stateChanged( const HudState& value ) override { state = value; }
};

void check( bool value, const char* message )
{
	if ( !value )
	{
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}
}

int main()
{
	Port port;
	View view;
	HudController hud( port, view );
	hud.beginWorld( WorldEpoch{ 17 } );

	check( hud.enqueuePrompt( EventResponseTargetId{ 101 }, "First", "Question", true, true ), "first prompt rejected" );
	check( hud.enqueuePrompt( std::nullopt, "Second", "Notice", false, false ), "second prompt rejected" );
	check( hud.state().prompts.size() == 2, "prompt queue size" );
	check( hud.state().prompts.front().title == "First", "first prompt was not front" );
	check( hud.state().prompts.back().title == "Second", "second prompt was not back" );
	check( port.registered.size() == 2 && port.registered[0] != port.registered[1], "prompt registrations" );
	port.actions.clear();

	hud.respondToPrompt( EventResponse::Acknowledge );
	check( port.actions.empty(), "invalid response dispatched for yes/no prompt" );
	check( hud.state().prompts.size() == 2, "invalid response removed prompt" );

	hud.respondToPrompt( EventResponse::Yes );
	check( port.actions.size() == 1, "yes response not dispatched" );
	check( std::get<EventResponsePayload>( port.actions.front().payload ).response == EventResponse::Yes, "yes response payload" );
	check( hud.state().prompts.size() == 1 && hud.state().prompts.front().title == "Second", "second prompt did not become front" );

	hud.respondToPrompt( EventResponse::Yes );
	check( port.actions.size() == 1 && hud.state().prompts.size() == 1, "invalid response dispatched for acknowledge prompt" );
	hud.respondToPrompt( EventResponse::Acknowledge );
	check( port.actions.size() == 2 && hud.state().prompts.empty(), "acknowledge prompt did not finish" );
	check( std::get<EventResponsePayload>( port.actions.back().payload ).response == EventResponse::Acknowledge, "acknowledge response payload" );

	std::cout << "HUD FIFO prompt ordering and response gating passed\n";
}
