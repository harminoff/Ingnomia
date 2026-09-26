/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// The primary window's Windows 98 frame (PDF p.311-313). The native frame is removed (Qt::FramelessWindowHint)
// and documents/window_frame.rml draws the sizing border, the caption with the small icon and title, and the
// Minimize, Maximize or Restore and Close buttons above every other document of the main context. Dragging the
// caption or a sizing zone hands the move or resize to the system; the other documents are inset to the client
// area so nothing lies under the frame.
#include "../localization/RmlText.h"
#include "../localization/UiText.h"
#include "NativeWindowCommands.h"
#include "WindowMenu.h"

#include <QGuiApplication>
#include <QPointF>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>
#include <RmlUi/Core.h>

#include <memory>
#include <optional>
#include <string>

namespace ingnomia::ui
{
class MainWindowFrame final : public Rml::EventListener
{
public:
	MainWindowFrame( QWindow& window, Rml::Context& context ) :
		window_( window ), context_( context )
	{
		document_ = context_.LoadDocument( "documents/window_frame.rml" );
		if ( !document_ ) return;
		const auto catalog = localization::UiText::english();
		localization::applyRmlText( *document_, catalog );
		maximizeTip_ = catalog.format( LocalizationKey{ "win98.frame.maximize.title" } );
		restoreTip_  = catalog.format( LocalizationKey{ "win98.frame.restore.title" } );
		document_->AddEventListener( Rml::EventId::Click, this );
		document_->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
		menu_ = std::make_unique<window_menu::WindowMenu>( *document_, [this]( const std::string& command ) { run( command ); } );
	}
	~MainWindowFrame() override
	{
		if ( !document_ ) return;
		menu_.reset();
		document_->RemoveEventListener( Rml::EventId::Click, this );
		document_->Close();
	}
	MainWindowFrame( const MainWindowFrame& )            = delete;
	MainWindowFrame& operator=( const MainWindowFrame& ) = delete;

	[[nodiscard]] Rml::ElementDocument* document() const noexcept { return document_; }

	/// Brings the frame up to date with the window (full screen, maximized, active, title) and insets the other
	/// documents to the client area. Call once per frame before the context updates.
	void sync()
	{
		if ( !document_ ) return;
		const auto states      = window_.windowStates();
		const bool fullScreen  = states.testFlag( Qt::WindowFullScreen );
		const bool maximized   = !fullScreen && states.testFlag( Qt::WindowMaximized );
		if ( fullScreen && menu_ ) menu_->close();
		if ( fullScreen && document_->IsVisible() ) document_->Hide();
		if ( !fullScreen && !document_->IsVisible() ) document_->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
		document_->SetClass( "is-maximized", maximized );
		// The caption is drawn inactive while another application is active (PDF p.313).
		document_->SetClass( "is-window-inactive", QGuiApplication::applicationState() != Qt::ApplicationActive );
		QString title = window_.title();
		if ( title.isEmpty() ) title = QGuiApplication::applicationDisplayName();
		const std::string titleText = title.toStdString();
		if ( auto* caption = document_->GetElementById( "frame_title" ); caption && titleText != shownTitle_ )
		{
			caption->SetInnerRML( escape( titleText ) );
			shownTitle_ = titleText;
		}
		if ( auto* button = document_->GetElementById( "frame_maximize" ) )
		{
			const auto& tip = maximized ? restoreTip_ : maximizeTip_;
			if ( button->GetAttribute<Rml::String>( "title", "" ) != tip )
			{
				button->SetAttribute( "title", tip );
				button->SetAttribute( "aria-label", tip );
			}
		}
		insetDocuments( fullScreen );
		// The game window's size grip shows only while the window can be sized.
		for ( int i = 0; i < context_.GetNumDocuments(); ++i )
			if ( auto* document = context_.GetDocument( i ); document && document != document_ && document->IsClassSet( "l-hud-root" ) && document->IsClassSet( "is-window-sizable" ) != sizable() )
				document->SetClass( "is-window-sizable", sizable() );
	}

	/// A left press. A press on a sizing zone starts a system resize; a press on the caption (not on its buttons)
	/// is remembered so the move starts once the pointer is dragged. Returns true when the frame took the press.
	bool press( Rml::Element* hover, QPointF position )
	{
		captionPress_.reset();
		// A click outside the open window menu only closes it; a click on it is the menu's own.
		if ( menu_ && menu_->isOpen() )
		{
			if ( menu_->contains( hover ) ) return false;
			menu_->close();
			return true;
		}
		// The status bar's size grip sizes the window's lower right corner, like the sizing border (PDF p.100).
		if ( sizable() && inside( hover, "hud_size_grip" ) )
		{
			window_.startSystemResize( Qt::BottomEdge | Qt::RightEdge );
			return true;
		}
		if ( !owns( hover ) ) return false;
		if ( const auto edges = sizeEdges( hover ) )
		{
			window_.startSystemResize( *edges );
			return true;
		}
		if ( inside( hover, "frame_minimize" ) || inside( hover, "frame_maximize" ) || inside( hover, "frame_close" ) ) return false;
		if ( !inside( hover, "frame_caption" ) ) return false;
		// Clicking the title bar icon opens the window menu below it (PDF p.113).
		if ( inside( hover, "frame_icon" ) )
		{
			openMenuBelowIcon( false );
			return true;
		}
		captionPress_ = position;
		return true;
	}
	/// A secondary-button press: on the title bar it opens the window menu at the pointer (`at`, context pixels).
	bool contextPress( Rml::Element* hover, Rml::Vector2f at )
	{
		if ( menu_ && menu_->isOpen() )
		{
			if ( menu_->contains( hover ) ) return false;
			menu_->close();
			return true;
		}
		if ( !owns( hover ) || !inside( hover, "frame_caption" ) ) return false;
		if ( inside( hover, "frame_minimize" ) || inside( hover, "frame_maximize" ) || inside( hover, "frame_close" ) ) return false;
		openMenu( at, false );
		return true;
	}
	/// Alt+Space opens the window menu below the title bar icon with its first command highlighted.
	bool openMenuFromKeyboard()
	{
		if ( !document_ || !document_->IsVisible() ) return false;
		openMenuBelowIcon( true );
		return true;
	}
	[[nodiscard]] bool menuOpen() const { return menu_ && menu_->isOpen(); }
	[[nodiscard]] window_menu::WindowMenu* menu() const { return menu_.get(); }
	/// Pointer motion while a caption press is held. A maximized window is not moved (PDF p.314).
	bool move( QPointF position, Qt::MouseButtons buttons )
	{
		if ( !captionPress_ ) return false;
		if ( !buttons.testFlag( Qt::LeftButton ) )
		{
			captionPress_.reset();
			return false;
		}
		if ( window_.windowStates().testFlag( Qt::WindowMaximized ) ) return true;
		if ( ( position - *captionPress_ ).manhattanLength() >= QGuiApplication::styleHints()->startDragDistance() )
		{
			captionPress_.reset();
			window_.startSystemMove();
		}
		return true;
	}
	/// Ends a caption press. Returns true when the frame owned the press.
	bool release()
	{
		const bool had = captionPress_.has_value();
		captionPress_.reset();
		return had;
	}
	/// Double-clicking the caption maximizes or restores the window (PDF p.314).
	bool doubleClick( Rml::Element* hover )
	{
		if ( !owns( hover ) || !inside( hover, "frame_caption" ) ) return false;
		if ( inside( hover, "frame_minimize" ) || inside( hover, "frame_maximize" ) || inside( hover, "frame_close" ) ) return false;
		captionPress_.reset();
		// Double-clicking the title bar icon closes the window (PDF p.98); the rest of the title bar maximizes or
		// restores it.
		if ( inside( hover, "frame_icon" ) )
		{
			if ( menu_ ) menu_->close();
			QTimer::singleShot( 0, &window_, [this] { window_.close(); } );
			return true;
		}
		toggleMaximized();
		return true;
	}

	void ProcessEvent( Rml::Event& event ) override
	{
		auto* target = event.GetTargetElement();
		// Window state changes resize the context, so they run after RmlUi finishes dispatching this click.
		if ( inside( target, "frame_minimize" ) ) QTimer::singleShot( 0, &window_, [this] { window_.showMinimized(); } );
		else if ( inside( target, "frame_maximize" ) ) QTimer::singleShot( 0, &window_, [this] { toggleMaximized(); } );
		else if ( inside( target, "frame_close" ) ) QTimer::singleShot( 0, &window_, [this] { window_.close(); } );
	}

private:
	/// A window that is neither maximized nor full screen can be sized with its border and size grip.
	[[nodiscard]] bool sizable() const
	{
		const auto states = window_.windowStates();
		return !states.testFlag( Qt::WindowMaximized ) && !states.testFlag( Qt::WindowFullScreen ) && !states.testFlag( Qt::WindowMinimized );
	}
	/// The window menu of a primary window (PDF p.95, p.113): every title bar button's command, with Move and Size.
	void openMenu( Rml::Vector2f at, bool fromKeyboard )
	{
		if ( !menu_ ) return;
		const bool max = window_.windowStates().testFlag( Qt::WindowMaximized );
		menu_->open( { { "restore", max }, { "move", !max }, { "size", !max }, { "minimize", true }, { "maximize", !max }, { "close", true, true } }, at, fromKeyboard );
	}
	void openMenuBelowIcon( bool fromKeyboard )
	{
		auto* icon    = document_->GetElementById( "frame_icon" );
		auto* caption = document_->GetElementById( "frame_caption" );
		if ( !icon || !caption ) return;
		const auto at = Rml::Vector2f( icon->GetAbsoluteOffset( Rml::BoxArea::Border ).x, caption->GetAbsoluteOffset( Rml::BoxArea::Border ).y + caption->GetOffsetHeight() );
		openMenu( at, fromKeyboard );
	}
	/// Carries out a window menu command after the menu has closed.
	void run( const std::string& command )
	{
		QTimer::singleShot( 0, &window_, [this, command] {
			if ( command == "restore" ) window_.showNormal();
			else if ( command == "move" ) native_window::beginKeyboardMove( window_ );
			else if ( command == "size" ) native_window::beginKeyboardSize( window_ );
			else if ( command == "minimize" ) window_.showMinimized();
			else if ( command == "maximize" ) window_.showMaximized();
			else if ( command == "close" ) window_.close();
		} );
	}
	void toggleMaximized()
	{
		if ( window_.windowStates().testFlag( Qt::WindowMaximized ) ) window_.showNormal();
		else window_.showMaximized();
	}
	[[nodiscard]] bool owns( Rml::Element* element ) const
	{
		return element && document_ && document_->IsVisible() && element->GetOwnerDocument() == document_;
	}
	static bool inside( Rml::Element* element, const char* id )
	{
		for ( ; element; element = element->GetParentNode() )
			if ( element->GetId() == id ) return true;
		return false;
	}
	static std::optional<Qt::Edges> sizeEdges( Rml::Element* element )
	{
		if ( !element || !element->IsClassSet( "w98-window-frame__size" ) ) return std::nullopt;
		const std::string id = element->GetId();
		const std::string zone = id.substr( id.rfind( '_' ) + 1 );
		Qt::Edges edges;
		for ( const char c : zone )
		{
			if ( c == 'n' ) edges |= Qt::TopEdge;
			else if ( c == 's' ) edges |= Qt::BottomEdge;
			else if ( c == 'w' ) edges |= Qt::LeftEdge;
			else if ( c == 'e' ) edges |= Qt::RightEdge;
		}
		return edges ? std::optional<Qt::Edges>( edges ) : std::nullopt;
	}
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
	/// Places the application shell, its screens and the HUD in the client area (the whole context in full screen).
	void insetDocuments( bool fullScreen )
	{
		const auto size = context_.GetDimensions();
		int left = 0, top = 0, width = size.x, height = size.y;
		if ( !fullScreen )
		{
			auto* client = document_->GetElementById( "frame_client" );
			if ( !client ) return;
			const auto at   = client->GetAbsoluteOffset( Rml::BoxArea::Padding );
			const auto area = client->GetBox().GetSize( Rml::BoxArea::Padding );
			if ( area.x <= 0.f || area.y <= 0.f ) return; // the frame has not been laid out yet
			left   = static_cast<int>( at.x + 0.5f );
			top    = static_cast<int>( at.y + 0.5f );
			width  = static_cast<int>( area.x + 0.5f );
			height = static_cast<int>( area.y + 0.5f );
		}
		const std::string stamp = std::to_string( left ) + "," + std::to_string( top ) + "," + std::to_string( width ) + "," + std::to_string( height );
		for ( int i = 0; i < context_.GetNumDocuments(); ++i )
		{
			auto* document = context_.GetDocument( i );
			if ( !document || document == document_ ) continue;
			if ( !document->IsClassSet( "l-hud-root" ) && !document->IsClassSet( "l-shell-root" ) && !document->IsClassSet( "l-shell-screen" ) ) continue;
			if ( document->GetAttribute<Rml::String>( "data-frame-client", "" ) == stamp ) continue;
			document->SetProperty( "position", "absolute" );
			document->SetProperty( "box-sizing", "border-box" );
			document->SetProperty( "left", std::to_string( left ) + "px" );
			document->SetProperty( "top", std::to_string( top ) + "px" );
			document->SetProperty( "width", std::to_string( width ) + "px" );
			document->SetProperty( "height", std::to_string( height ) + "px" );
			document->SetAttribute( "data-frame-client", stamp );
		}
	}

	QWindow& window_;
	Rml::Context& context_;
	Rml::ElementDocument* document_ = nullptr;
	std::optional<QPointF> captionPress_;
	std::unique_ptr<window_menu::WindowMenu> menu_;
	std::string shownTitle_;
	std::string maximizeTip_;
	std::string restoreTip_;
};
} // namespace ingnomia::ui
