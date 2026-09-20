/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "DebugRmlBinding.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
namespace ingnomia::ui::debug
{
class DebugRmlBinding::Callback final : public Rml::EventListener
{
public:
	explicit Callback( std::function<void( Rml::Event& )> f ) :
		fn( std::move( f ) )
	{
	}
	void ProcessEvent( Rml::Event& event ) override
	{
		fn( event );
	}
	std::function<void( Rml::Event& )> fn;
};
DebugRmlBinding::DebugRmlBinding( Rml::Context& c ) :
	context_( c )
{
}
DebugRmlBinding::~DebugRmlBinding()
{
	shutdown();
}
bool DebugRmlBinding::initialize( DebugController& c )
{
#if !defined( INGNOMIA_DEVELOPER_UI )
	(void)c;
	return false;
#else
	controller_ = &c;
	document_   = context_.LoadDocument( "developer_ui/debug_panel.rml" );
	if ( !document_ )
	{
		controller_ = nullptr;
		return false;
	}
	bind( "debug_close", [this]
		  { controller_->close(); } );
	bind( "debug_tab_diagnostics", [this]
		  { controller_->setPage( Page::Diagnostics ); } );
	bind( "debug_tab_gnomes", [this]
		  { controller_->setPage( Page::Gnomes ); } );
	bind( "debug_tab_items", [this]
		  { controller_->setPage( Page::Items ); } );
	bind( "debug_tab_game", [this]
		  { controller_->setPage( Page::Game ); } );
	bind( "debug_refresh_gnomes", [this]
		  { controller_->requestGnomes(); } );
	bind( "debug_refresh_groups", [this]
		  { controller_->requestGroups(); } );
	bindEvent( "debug_search", "change", [this]( Rml::Event& event )
			   { if(auto* element=event.GetCurrentElement()) controller_->setSearch(element->GetAttribute<Rml::String>("value", "")); } );
	bindEvent( "debug_gnome_rows", "click", [this]( Rml::Event& event )
			   { for(auto* element=event.GetTargetElement();element&&element!=event.GetCurrentElement();element=element->GetParentNode()){const int id=element->GetAttribute<int>("data-gnome",0);if(id>0){controller_->selectGnome(static_cast<std::uint32_t>(id));break;}} } );
	bindEvent( "debug_gnome_rows", "keydown", [this]( Rml::Event& event )
			   { const auto key=static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier",0));if(key==Rml::Input::KI_UP)controller_->moveGnomeSelection(-1);else if(key==Rml::Input::KI_DOWN)controller_->moveGnomeSelection(1);else return;event.StopPropagation(); } );
	stateChanged( c.state() );
	return true;
#endif
}
void DebugRmlBinding::bind( const char* id, std::function<void()> fn )
{
	bindEvent( id, "click", [fn = std::move( fn )]( Rml::Event& )
			   { fn(); } );
}
void DebugRmlBinding::bindEvent( const char* id, const char* event, std::function<void( Rml::Event& )> fn )
{
	if ( auto* e = document_->GetElementById( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get() );
		listeners_.push_back( { e, event, std::move( cb ) } );
	}
}
void DebugRmlBinding::shutdown()
{
	for ( auto& l : listeners_ )
		if ( l.element )
			l.element->RemoveEventListener( l.event, l.callback.get() );
	listeners_.clear();
	if ( document_ )
	{
		context_.UnloadDocument( document_ );
		document_ = nullptr;
	}
	controller_ = nullptr;
}
void DebugRmlBinding::stateChanged( const DebugState& s )
{
	if ( !document_ )
		return;
	if ( !s.enabled || !s.open )
	{
		document_->Hide();
		return;
	}
	document_->Show();
	auto show = [this]( const char* id, bool v )
	{if(auto*e=document_->GetElementById(id)){e->SetClass("is-hidden",!v);e->SetProperty("display",v?"block":"none");} };
	show( "debug_diagnostics", s.page == Page::Diagnostics );
	show( "debug_gnomes", s.page == Page::Gnomes );
	show( "debug_items", s.page == Page::Items );
	show( "debug_game", s.page == Page::Game );
	const auto selectTab = [this]( const char* id, bool selected )
	{
		if ( auto* e = document_->GetElementById( id ) )
		{
			e->SetClass( "is-selected", selected );
			e->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
	};
	selectTab( "debug_tab_diagnostics", s.page == Page::Diagnostics );
	selectTab( "debug_tab_gnomes", s.page == Page::Gnomes );
	selectTab( "debug_tab_items", s.page == Page::Items );
	selectTab( "debug_tab_game", s.page == Page::Game );
	if ( auto* e = document_->GetElementById( "debug_counters" ) )
		e->SetInnerRML( "notifications " + std::to_string( s.counters.notifications ) + " | rendered " + std::to_string( s.counters.renderedNotifications ) + " | dirty bindings " + std::to_string( s.counters.dirtyBindings ) + " | full snapshots " + std::to_string( s.counters.fullSnapshots ) + " | row patches " + std::to_string( s.counters.rowPatches ) + " | stale epoch " + std::to_string( s.counters.staleEpochRejects ) + " | stale revision " + std::to_string( s.counters.staleRevisionRejects ) );
	if ( auto* e = document_->GetElementById( "debug_gnome_rows" ) )
	{
		std::string rows;
		for ( const auto& r : controller_->visibleGnomes() )
			rows += "<button class='c-list__row' data-gnome='" + std::to_string( r.id ) + "'><span class='c-list__primary'>" + Rml::StringUtilities::EncodeRml( r.name ) + "</span><span class='c-list__meta'>id " + std::to_string( r.id ) + "</span></button>";
		e->SetInnerRML( rows );
	}
}
} // namespace ingnomia::ui::debug
