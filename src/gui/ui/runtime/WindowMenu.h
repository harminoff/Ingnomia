/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// The window menu (PDF p.95, p.113, p.181): the shortcut menu of a window, opened by clicking the title bar with the
// secondary button, clicking the title bar icon, or pressing Alt+Space. It lists the window's commands; every title
// bar button has its command here. Built as a Windows 98 drop-down menu inside the window's own document, so access
// letters (AccessKeys::activate) and the menu styles apply to it as to any other menu.
#include "../localization/RmlText.h"
#include "../localization/UiText.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace ingnomia::ui::window_menu
{
struct Item
{
	std::string command;         ///< restore, move, size, minimize, maximize, always_on_top, close or whats_this
	bool enabled        = true;
	bool separatorBefore = false;
	bool checked        = false;  ///< An on/off setting such as Always on Top shows a check mark (PDF p.118).
};

class WindowMenu final : public Rml::EventListener
{
public:
	using Handler = std::function<void( const std::string& command )>;

	WindowMenu( Rml::ElementDocument& document, Handler handler ) :
		document_( document ), handler_( std::move( handler ) )
	{
		auto element = document_.CreateElement( "div" );
		element->SetId( "window_menu" );
		element->SetClass( "w98-menu", true );
		element->SetClass( "w98-window-menu", true );
		element->SetClass( "is-hidden", true );
		element->SetAttribute( "role", "menu" );
		menu_ = document_.AppendChild( std::move( element ) );
		menu_->AddEventListener( Rml::EventId::Click, this );
		menu_->AddEventListener( Rml::EventId::Keydown, this );
		alive_ = menu_->GetObserverPtr();
	}
	~WindowMenu() override
	{
		// The document may already have been unloaded with the menu in it.
		auto* menu = alive_.get();
		if ( !menu ) return;
		menu->RemoveEventListener( Rml::EventId::Click, this );
		menu->RemoveEventListener( Rml::EventId::Keydown, this );
		if ( auto* parent = menu->GetParentNode() ) parent->RemoveChild( menu );
	}
	/// False once the document that holds the menu has been unloaded.
	[[nodiscard]] bool alive() const { return static_cast<bool>( alive_ ); }
	WindowMenu( const WindowMenu& )            = delete;
	WindowMenu& operator=( const WindowMenu& ) = delete;

	[[nodiscard]] bool isOpen() const { return alive() && !menu_->IsClassSet( "is-hidden" ); }
	[[nodiscard]] Rml::Element* element() const { return menu_; }
	[[nodiscard]] bool contains( Rml::Element* element ) const
	{
		for ( ; element; element = element->GetParentNode() )
			if ( element == menu_ ) return true;
		return false;
	}

	/// Opens the menu with its top left corner at `at` (document pixels), kept inside the document. From the
	/// keyboard the first available command is highlighted; from the mouse none is until the pointer moves.
	void open( const std::vector<Item>& items, Rml::Vector2f at, bool fromKeyboard )
	{
		if ( !alive() ) return;
		std::string rml;
		for ( const auto& item : items )
		{
			if ( item.separatorBefore ) rml += "<span class=\"w98-menu__separator\"></span>";
			rml += "<button id=\"window_menu_" + item.command + "\" class=\"w98-menu__item" + std::string( item.command == "close" ? " is-default" : "" )
				+ std::string( item.checked ? " is-checked" : "" ) + "\" role=\"" + std::string( item.command == "always_on_top" ? "menuitemcheckbox" : "menuitem" )
				+ "\" aria-checked=\"" + std::string( item.checked ? "true" : "false" ) + "\" data-command=\"" + item.command + "\"" + std::string( item.enabled ? "" : " disabled=\"disabled\"" )
				+ "><span class=\"w98-menu__mark\"></span><span data-l10n=\"win98.window_menu." + item.command + "\"></span>"
				+ std::string( item.command == "close" ? "<span class=\"w98-menu__shortcut\" data-l10n=\"win98.window_menu.close_shortcut\"></span>" : "" )
				+ "</button>";
		}
		menu_->SetInnerRML( rml );
		localization::applyRmlText( *menu_, localization::UiText::english() );
		menu_->SetClass( "is-hidden", false );
		menu_->SetProperty( "left", "0px" );
		menu_->SetProperty( "top", "0px" );
		document_.UpdateDocument();
		const auto size = document_.GetBox().GetSize( Rml::BoxArea::Border );
		const float width = menu_->GetOffsetWidth(), height = menu_->GetOffsetHeight();
		const auto origin = document_.GetAbsoluteOffset( Rml::BoxArea::Border );
		const float x = std::clamp( at.x - origin.x, 0.f, std::max( 0.f, size.x - width ) );
		const float y = std::clamp( at.y - origin.y, 0.f, std::max( 0.f, size.y - height ) );
		menu_->SetProperty( "left", std::to_string( int( x ) ) + "px" );
		menu_->SetProperty( "top", std::to_string( int( y ) ) + "px" );
		auto* focus = document_.GetContext() ? document_.GetContext()->GetFocusElement() : nullptr;
		previousFocus_ = focus && !contains( focus ) ? focus->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>{};
		if ( fromKeyboard ) focusItem( 0, 1 );
		else menu_->Focus();
	}
	void close()
	{
		if ( !isOpen() ) return;
		menu_->SetClass( "is-hidden", true );
		// Lay the document out now so the closed menu no longer takes pointer hits before the next frame.
		document_.UpdateDocument();
		if ( auto* previous = previousFocus_.get(); previous && previous->IsVisible( true ) ) previous->Focus();
		previousFocus_.reset();
	}

	void ProcessEvent( Rml::Event& event ) override
	{
		if ( event.GetId() == Rml::EventId::Click )
		{
			for ( auto* element = event.GetTargetElement(); element && element != menu_; element = element->GetParentNode() )
			{
				const auto command = element->GetAttribute<Rml::String>( "data-command", "" );
				if ( command.empty() ) continue;
				if ( element->HasAttribute( "disabled" ) ) return;
				close();
				if ( handler_ ) handler_( command );
				return;
			}
			return;
		}
		if ( event.GetId() != Rml::EventId::Keydown ) return;
		const auto key = static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) );
		if ( key == Rml::Input::KI_ESCAPE ) close();
		else if ( key == Rml::Input::KI_DOWN ) focusItem( current() + 1, 1 );
		else if ( key == Rml::Input::KI_UP ) focusItem( current() < 0 ? itemCount() - 1 : current() - 1, -1 );
		else if ( key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER )
		{
			if ( auto* focused = item( current() ) ) focused->Click();
		}
		else return;
		event.StopPropagation();
	}

private:
	[[nodiscard]] std::vector<Rml::Element*> items() const
	{
		Rml::ElementList list;
		menu_->QuerySelectorAll( list, "button.w98-menu__item" );
		return { list.begin(), list.end() };
	}
	[[nodiscard]] int itemCount() const { return static_cast<int>( items().size() ); }
	[[nodiscard]] Rml::Element* item( int index ) const
	{
		const auto list = items();
		return index >= 0 && index < static_cast<int>( list.size() ) ? list[index] : nullptr;
	}
	[[nodiscard]] int current() const
	{
		auto* focus = document_.GetContext() ? document_.GetContext()->GetFocusElement() : nullptr;
		const auto list = items();
		const auto found = std::find( list.begin(), list.end(), focus );
		return found == list.end() ? -1 : static_cast<int>( found - list.begin() );
	}
	/// Highlights the next available command from `start` in `step` direction, wrapping around (PDF p.115-116).
	void focusItem( int start, int step )
	{
		const auto list = items();
		const int count = static_cast<int>( list.size() );
		for ( int n = 0; n < count; ++n )
		{
			const int index = ( ( start + step * n ) % count + count ) % count;
			if ( !list[index]->HasAttribute( "disabled" ) )
			{
				list[index]->Focus( true );
				return;
			}
		}
	}

	Rml::ElementDocument& document_;
	Handler handler_;
	Rml::Element* menu_          = nullptr;
	Rml::ObserverPtr<Rml::Element> alive_;
	Rml::ObserverPtr<Rml::Element> previousFocus_;
};
} // namespace ingnomia::ui::window_menu
