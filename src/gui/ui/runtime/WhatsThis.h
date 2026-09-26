/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// Context-sensitive Help, "What's This?" (PDF p.285-288). A window enters What's This? mode from the ? title bar
// button of a secondary window, the What's This? toolbar button or Shift+F1 in the game window; the pointer becomes the
// context-sensitive Help pointer over that window only, and the next click shows a pop-up window explaining the item
// and ends the mode. Esc, choosing What's This? again, clicking outside the window or clicking an item without Help
// cancels the mode. Clicking a control with the secondary button offers a What's This? shortcut menu, and F1 (or
// Shift+F1 in a secondary window) explains the control that has the input focus.
//
// Help text lives in the catalog under "win98.help.<element id>" (or "win98.help.<document id>.<element id>"). An item without its own entry takes the entry of
// the control it labels or belongs to: a label its control, a spin box arrow its field, a generated row its list.
// Group titles and static text have none, so clicking them cancels the mode (PDF p.288).
#include "../localization/UiText.h"
#include "WindowMenu.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <memory>
#include <string>

namespace ingnomia::ui::whats_this
{
inline bool has( const localization::UiText& catalog, const std::string& key ) { return catalog.contains( LocalizationKey{ key } ); }

/// The catalog key of the Help for an element, or an empty string when it has none.
inline std::string helpKey( Rml::Element* element, const localization::UiText& catalog )
{
	for ( int guard = 0; element && guard < 64; ++guard, element = element->GetParentNode() )
	{
		if ( element->GetTagName() == "body" || element->GetTagName() == "#root" ) return {};
		// Containers of static content end the search: their text explains itself (PDF p.288).
		if ( element->IsClassSet( "w98-group__title" ) || element->IsClassSet( "w98-group" ) || element->IsClassSet( "w98-page" )
			|| element->IsClassSet( "w98-page-frame" ) || element->IsClassSet( "w98-sheet__body" ) || element->IsClassSet( "w98-wizard__body" ) )
			return {};
		if ( element->IsClassSet( "w98-caption__help" ) ) return "win98.help.common.caption_help";
		if ( element->IsClassSet( "w98-caption__close" ) || element->IsClassSet( "w98-caption__button--close" ) )
			return element->GetId() == "frame_close" && has( catalog, "win98.help.frame_close" ) ? "win98.help.frame_close" : "win98.help.common.caption_close";
		if ( element->GetTagName() == "label" )
		{
			const auto target = element->GetAttribute<Rml::String>( "for", "" );
			if ( auto* document = element->GetOwnerDocument(); document && !target.empty() )
				if ( auto* control = document->GetElementById( target ) ) return helpKey( control, catalog );
		}
		auto id = element->GetId();
		if ( !id.empty() )
		{
			// An id used by two windows with different roles (a palette's read-only summary and a property sheet's
			// control) has an entry scoped by its document: "win98.help.<document id>.<id>".
			if ( auto* document = element->GetOwnerDocument(); document && !document->GetId().empty() && has( catalog, "win98.help." + document->GetId() + "." + id ) )
				return "win98.help." + document->GetId() + "." + id;
			if ( has( catalog, "win98.help." + id ) ) return "win98.help." + id;
			for ( const char* suffix : { "-up", "-down", "-error", "_up", "_down" } )
			{
				const std::string s( suffix );
				if ( id.size() > s.size() && id.compare( id.size() - s.size(), s.size(), s ) == 0 && has( catalog, "win98.help." + id.substr( 0, id.size() - s.size() ) ) )
					return "win98.help." + id.substr( 0, id.size() - s.size() );
			}
		}
		// The common commit buttons share one entry each.
		const auto text = element->GetAttribute<Rml::String>( "data-l10n", "" );
		for ( const char* common : { "ok", "cancel", "apply", "close" } )
			if ( text == std::string( "win98.common." ) + common ) return std::string( "win98.help.common." ) + common;
	}
	return {};
}

/// The context-sensitive Help pop-up window (PDF p.286, Figure 13.3): a brief explanation near the item, closed by the
/// next click or key press.
class Popup
{
public:
	~Popup() { close(); }
	[[nodiscard]] bool isOpen() const { return static_cast<bool>( popup_ ); }
	[[nodiscard]] Rml::Element* element() const { return popup_.get(); }
	/// Shows `text` for `target` in its document, below the item (above when there is no room), centred on `x`.
	void show( Rml::Element& target, const std::string& text, float x )
	{
		close();
		auto* document = target.GetOwnerDocument();
		if ( !document ) return;
		auto element = document->CreateElement( "div" );
		element->SetId( "whats_this_popup" );
		element->SetClass( "w98-help-popup", true );
		element->SetAttribute( "role", "tooltip" );
		auto* popup = document->AppendChild( std::move( element ) );
		popup->SetInnerRML( escape( text ) );
		popup->SetProperty( "left", "0px" );
		popup->SetProperty( "top", "0px" );
		document->UpdateDocument();
		const auto size   = document->GetBox().GetSize( Rml::BoxArea::Border );
		const auto origin = document->GetAbsoluteOffset( Rml::BoxArea::Border );
		const auto at     = target.GetAbsoluteOffset( Rml::BoxArea::Border ) - origin;
		const float width = popup->GetOffsetWidth(), height = popup->GetOffsetHeight();
		float top = at.y + target.GetOffsetHeight() + 2.f;
		if ( top + height > size.y ) top = at.y - height - 2.f;
		const float left = std::clamp( x - origin.x - width / 2.f, 0.f, std::max( 0.f, size.x - width ) );
		top              = std::clamp( top, 0.f, std::max( 0.f, size.y - height ) );
		popup->SetProperty( "left", std::to_string( int( left ) ) + "px" );
		popup->SetProperty( "top", std::to_string( int( top ) ) + "px" );
		popup_ = popup->GetObserverPtr();
	}
	void close()
	{
		if ( auto* popup = popup_.get() )
			if ( auto* parent = popup->GetParentNode() ) parent->RemoveChild( popup );
		popup_.reset();
	}

private:
	static std::string escape( const std::string& text )
	{
		std::string out;
		for ( const char c : text )
		{
			if ( c == '&' ) out += "&amp;";
			else if ( c == '<' ) out += "&lt;";
			else if ( c == '>' ) out += "&gt;";
			else out += c;
		}
		return out;
	}
	Rml::ObserverPtr<Rml::Element> popup_;
};

/// What's This? for one native window: its mode, pop-up window and shortcut menu.
class Controller
{
public:
	explicit Controller( localization::UiText catalog = localization::UiText::english() ) :
		catalog_( std::move( catalog ) ) {}

	[[nodiscard]] bool mode() const noexcept { return mode_; }
	[[nodiscard]] bool popupOpen() const { return popup_.isOpen(); }
	[[nodiscard]] Rml::Element* popupElement() const { return popup_.element(); }
	[[nodiscard]] window_menu::WindowMenu* menu() const { return menu_.get(); }
	[[nodiscard]] const localization::UiText& catalog() const noexcept { return catalog_; }

	/// Choosing What's This? starts the mode, or cancels it when it is already on (PDF p.286).
	void toggleMode()
	{
		popup_.close();
		mode_ = !mode_;
	}
	void cancel()
	{
		mode_ = false;
		popup_.close();
		if ( menu_ ) menu_->close();
	}

	/// Shows the Help for `element` (or what it belongs to). Returns false when the item has none.
	bool explain( Rml::Element* element, float x )
	{
		const auto key = helpKey( element, catalog_ );
		if ( key.empty() || !element ) return false;
		popup_.show( *element, catalog_.format( LocalizationKey{ key } ), x );
		return true;
	}

	/// A pointer press. While the pop-up is open the press only closes it. In What's This? mode the press explains the
	/// item under the pointer, or cancels the mode when the item has no Help; the mode then ends either way.
	/// A secondary-button press on a control with Help opens the What's This? shortcut menu (PDF p.286-287).
	/// Returns true when What's This? took the press.
	bool press( Rml::Element* hover, Rml::Vector2f at, bool secondary )
	{
		if ( menu_ && menu_->isOpen() )
		{
			if ( menu_->contains( hover ) ) return false;
			menu_->close();
			return true;
		}
		if ( popup_.isOpen() )
		{
			popup_.close();
			return true;
		}
		if ( mode_ )
		{
			mode_ = false;
			(void)explain( hover, at.x );
			return true;
		}
		if ( secondary && hover && !helpKey( hover, catalog_ ).empty() && hover->GetOwnerDocument() )
		{
			auto* document = hover->GetOwnerDocument();
			if ( !menu_ || !menu_->alive() || menu_->element()->GetOwnerDocument() != document )
				menu_ = std::make_unique<window_menu::WindowMenu>( *document, [this]( const std::string& ) { (void)explain( menuTarget_.get(), menuX_ ); } );
			menuTarget_ = hover->GetObserverPtr();
			menuX_      = at.x;
			menu_->open( { { "whats_this", true } }, at, false );
			return true;
		}
		return false;
	}

	/// A key press. Any key closes the pop-up; Esc cancels the mode. F1 (and Shift+F1 in a secondary window) explains
	/// the focused control. Returns true when What's This? took the key.
	bool key( Rml::Context& context, Rml::Input::KeyIdentifier key, bool shift, bool secondaryWindow )
	{
		if ( popup_.isOpen() )
		{
			popup_.close();
			return true;
		}
		if ( mode_ && key == Rml::Input::KI_ESCAPE )
		{
			mode_ = false;
			return true;
		}
		if ( key != Rml::Input::KI_F1 ) return false;
		if ( shift && !secondaryWindow )
		{
			toggleMode();
			return true;
		}
		auto* focus = context.GetFocusElement();
		if ( !focus || focus == focus->GetOwnerDocument() ) return false;
		const auto at = focus->GetAbsoluteOffset( Rml::BoxArea::Border );
		return explain( focus, at.x + focus->GetOffsetWidth() / 2.f );
	}

private:
	localization::UiText catalog_;
	Popup popup_;
	std::unique_ptr<window_menu::WindowMenu> menu_;
	Rml::ObserverPtr<Rml::Element> menuTarget_;
	float menuX_ = 0.f;
	bool mode_   = false;
};
} // namespace ingnomia::ui::whats_this
