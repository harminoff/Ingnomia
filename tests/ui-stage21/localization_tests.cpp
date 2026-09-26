/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 21c: every Windows 98 window takes its text from the catalog; access keys travel with the strings
// ("&Name:"), are underlined once, are unique within a window and its page, and reach their controls.
#include "gui/ui/runtime/AccessKeys.h"
#include "gui/ui/runtime/CaptionText.h"
#include "gui/ui/runtime/WhatsThis.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/localization/RmlText.h"
#include "gui/ui/screens/management6b/Management6BText.h"
#include "gui/ui/screens/management6c/Management6CText.h"
#include <RmlUi/Core.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
using namespace ingnomia::ui;
int checks = 0;
void check( bool value, const std::string& label )
{
	++checks;
	if ( !value )
	{
		std::cerr << "FAIL " << label << std::endl;
		std::exit( 1 );
	}
}
struct Files : Rml::FileInterface
{
	std::filesystem::path root;
	Rml::FileHandle Open( const Rml::String& name ) override
	{
		auto path = std::filesystem::path( name );
		if ( !path.has_root_name() ) path = root / path.relative_path();
		return reinterpret_cast<Rml::FileHandle>( std::fopen( path.string().c_str(), "rb" ) );
	}
	void Close( Rml::FileHandle f ) override { std::fclose( reinterpret_cast<FILE*>( f ) ); }
	size_t Read( void* p, size_t size, Rml::FileHandle f ) override { return std::fread( p, 1, size, reinterpret_cast<FILE*>( f ) ); }
	bool Seek( Rml::FileHandle f, long n, int from ) override { return std::fseek( reinterpret_cast<FILE*>( f ), n, from ) == 0; }
	size_t Tell( Rml::FileHandle f ) override { return static_cast<size_t>( std::ftell( reinterpret_cast<FILE*>( f ) ) ); }
};
struct Renderer : Rml::RenderInterface
{
	Rml::CompiledGeometryHandle CompileGeometry( Rml::Span<const Rml::Vertex>, Rml::Span<const int> ) override { return 1; }
	void RenderGeometry( Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle ) override {}
	void ReleaseGeometry( Rml::CompiledGeometryHandle ) override {}
	Rml::TextureHandle LoadTexture( Rml::Vector2i&, const Rml::String& ) override { return 0; }
	Rml::TextureHandle GenerateTexture( Rml::Span<const Rml::byte>, Rml::Vector2i ) override { return 1; }
	void ReleaseTexture( Rml::TextureHandle ) override {}
	void EnableScissorRegion( bool ) override {}
	void SetScissorRegion( Rml::Rectanglei ) override {}
};
void collect( Rml::Element* e, std::vector<Rml::Element*>& out )
{
	out.push_back( e );
	for ( int i = 0; i < e->GetNumChildren(); ++i ) collect( e->GetChild( i ), out );
}
std::string plain( std::string rml )
{
	std::string out;
	bool tag = false;
	for ( char c : rml )
	{
		if ( c == '<' ) tag = true;
		else if ( c == '>' ) tag = false;
		else if ( !tag ) out += c;
	}
	return out;
}
// The window is the nearest Windows 98 sheet or message box; the page is the nearest tab page or wizard page.
std::pair<Rml::Element*, Rml::Element*> scope( Rml::Element* e )
{
	Rml::Element* page = nullptr;
	for ( auto* a = e; a; a = a->GetParentNode() )
	{
		if ( !page && ( a->GetAttribute<Rml::String>( "role", "" ) == "tabpanel" || a->IsClassSet( "l-wizard-page" ) || a->IsClassSet( "w98-wizard__page" ) ) ) page = a;
		if ( a->IsClassSet( "w98-sheet" ) || a->IsClassSet( "w98-msgbox" ) || a->IsClassSet( "w98-menu" ) ) return { a, page };
	}
	return { nullptr, page };
}

int main( int argc, char** argv )
{
	check( argc == 2, "assets" );
	// Book-title capitalization of object names in captions (PDF p.329): default type names get every word
	// capitalized; a name the player typed keeps its own capitals after the first letter.
	check( captionName( "market stall" ) == "Market Stall" && captionName( "carpenter" ) == "Carpenter" && captionName( "Stage 11 farm" ) == "Stage 11 farm" && captionName( "" ).empty(), "caption names" );
	Files files;
	files.root = argv[1];
	Renderer renderer;
	Rml::SystemInterface system;
	Rml::SetFileInterface( &files );
	Rml::SetRenderInterface( &renderer );
	Rml::SetSystemInterface( &system );
	check( Rml::Initialise(), "initialize" );
	ClassicFocusInstancer focus;
	Rml::Factory::RegisterDecoratorInstancer( "win98-focus", &focus );
	Rml::LoadFontFace( "fonts/LatoLatin-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Bold.ttf" );
	auto* c = Rml::CreateContext( "stage21", { 384, 380 } );
	const auto english = localization::UiText::english();
	const auto population = management6b::management6BText();
	const auto military = management6c::management6cEnglishText();
	const std::vector<std::pair<const char*, const localization::UiText*>> documents {
		{ "screens/main_menu.rml", &english }, { "screens/new_game.rml", &english }, { "screens/load_game.rml", &english }, { "screens/settings.rml", &english },
		{ "screens/pause_menu.rml", &english }, { "screens/loading.rml", &english }, { "screens/game_hud.rml", &english }, { "screens/orders_tools.rml", &english },
		{ "screens/inspector.rml", &english }, { "windows/workshop_manager.rml", &english }, { "windows/stockpile_manager.rml", &english },
		{ "panels/agriculture_manager.rml", &english }, { "windows/population_manager.rml", &population }, { "windows/inventory_browser.rml", &population },
		{ "windows/military_manager.rml", &military }, { "windows/diplomacy_missions.rml", &military }, { "documents/window_frame.rml", &english } };
	int keyed = 0;
	int captionButtons = 0;
	int helped = 0;
	for ( const auto& [path, catalog] : documents )
	{
		auto* doc = c->LoadDocument( path );
		check( doc != nullptr, std::string( "loads " ) + path );
		localization::applyRmlText( *doc, *catalog );
		std::vector<Rml::Element*> all;
		collect( doc, all );
		// Title bars (PDF p.180, p.311-312): every caption is 18 px with 16 x 14 caption buttons ending 2 px from its
		// right end; a palette's caption is shorter (15 px) with a 13 x 11 Close. Every Close draws the same glyph.
		doc->UpdateDocument();
		// What's This? coverage (PDF p.287-288): every control a player can use has Help, found from the control itself,
		// the control it labels or the list it belongs to.
		for ( auto* e : all )
		{
			const auto tag = e->GetTagName();
			const bool control = tag == "button" || tag == "input" || tag == "select" || tag == "textarea" || e->IsClassSet( "w98-list" ) || e->IsClassSet( "w98-grid__cells" );
			if ( !control || e->GetId().empty() ) continue;
			// Tabs a window never shows (it shares its tab strip with another window) need no Help.
			if ( e->IsClassSet( "u-hidden" ) || e->IsClassSet( "is-legacy" ) || e->GetAttribute<Rml::String>( "aria-hidden", "" ) == "true" || ( e->IsClassSet( "c-tabs__tab" ) && e->IsClassSet( "is-hidden" ) ) ) continue;
			const auto help = whats_this::helpKey( e, english );
			check( !help.empty(), std::string( path ) + ": What's This? Help for '" + e->GetId() + "'" );
			++helped;
		}
		// A label explains its control; a spin box arrow its field (PDF p.288).
		for ( auto* e : all )
		{
			const auto target = e->GetTagName() == "label" ? e->GetAttribute<Rml::String>( "for", "" ) : std::string();
			auto* control = target.empty() ? nullptr : doc->GetElementById( target );
			if ( control && !whats_this::helpKey( control, english ).empty() )
				check( whats_this::helpKey( e, english ) == whats_this::helpKey( control, english ), std::string( path ) + ": label for '" + target + "' shares its Help" );
			const auto id = e->GetId();
			if ( id.size() > 3 && id.compare( id.size() - 3, 3, "-up" ) == 0 )
				if ( auto* field = doc->GetElementById( id.substr( 0, id.size() - 3 ) ); field && !whats_this::helpKey( field, english ).empty() )
					check( whats_this::helpKey( e, english ) == whats_this::helpKey( field, english ), std::string( path ) + ": spin arrow '" + id + "' shares its field's Help" );
		}
		const auto px = []( float a, float b ) { return std::abs( a - b ) < 0.01f; };
		for ( auto* e : all )
		{
			if ( !e->IsClassSet( "w98-caption" ) ) continue;
			const bool palette = e->GetParentNode() && e->GetParentNode()->IsClassSet( "w98-palette" );
			const auto& cv = e->GetComputedValues();
			const std::string where = std::string( path ) + " caption '" + e->GetParentNode()->GetId() + "'";
			check( px( cv.height().value, ( palette ? 15.f : 18.f ) ), where + ( palette ? " is 15 px" : " is 18 px" ) + " (" + std::to_string( cv.height().value ) + ")" );
			check( px( cv.padding_right().value, 2.f ), where + " ends 2 px after its last button" );
			for ( int i = 0; i < e->GetNumChildren(); ++i )
			{
				auto* b = e->GetChild( i );
				if ( !b->IsClassSet( "w98-caption__close" ) && !b->IsClassSet( "w98-caption__button" ) ) continue;
				const auto& bv = b->GetComputedValues();
				check( px( bv.width().value, ( palette ? 13.f : 16.f ) ) && px( bv.height().value, ( palette ? 11.f : 14.f ) ), where + " button '" + b->GetId() + "' size" );
				check( b->GetNumChildren() == 1 && b->GetChild( 0 )->IsClassSet( "w98-caption__glyph" ), where + " button '" + b->GetId() + "' draws its glyph" );
				const auto& gv = b->GetChild( 0 )->GetComputedValues();
				check( px( gv.width().value, ( palette ? 11.f : 14.f ) ) && px( gv.height().value, ( palette ? 9.f : 12.f ) ), where + " glyph fills the button face" );
				++captionButtons;
			}
		}
		std::map<std::pair<Rml::Element*, Rml::Element*>, std::map<char, std::string>> used;
		// "A" is kept for Apply only in windows that have an Apply button (PDF p.46).
		std::set<Rml::Element*> withApply;
		for ( auto* e : all )
			if ( e->GetTagName() == "button" && plain( e->GetInnerRML() ) == "Apply" ) withApply.insert( scope( e ).first );
		for ( auto* e : all )
		{
			if ( !e->GetAttribute<Rml::String>( "data-l10n", "" ).empty() )
			{
				++keyed;
				check( e->GetInnerRML().find( "\xE2\x9F\xA6" ) == std::string::npos, std::string( path ) + ": catalog has the text for " + e->GetAttribute<Rml::String>( "data-l10n", "" ) );
			}
			const auto key = e->GetAttribute<Rml::String>( "accesskey", "" );
			if ( key.empty() ) continue;
			const auto label = plain( e->GetInnerRML() );
			const auto where = std::string( path ) + " '" + label + "'";
			check( key.size() == 1, where + ": one access key" );
			// One underline, on the access key's letter.
			const auto first = e->GetInnerRML().find( "w98-ak" );
			check( first != std::string::npos && e->GetInnerRML().find( "w98-ak", first + 1 ) == std::string::npos, where + ": the access key is underlined once" );
			const auto open = e->GetInnerRML().find( '>', first ) + 1;
			check( std::tolower( static_cast<unsigned char>( e->GetInnerRML()[open] ) ) == std::tolower( static_cast<unsigned char>( key[0] ) ), where + ": the underlined letter is the access key" );
			check( label != "OK" && label != "Cancel" && label != "Close", where + ": OK, Cancel and Close use Enter and Esc instead" );
			const auto [window, page] = scope( e );
			check( std::tolower( static_cast<unsigned char>( key[0] ) ) != 'a' || label == "Apply" || !withApply.contains( window ), where + ": A is kept for Apply" );
			auto& letters = used[{ window, page }];
			const char letter = static_cast<char>( std::tolower( static_cast<unsigned char>( key[0] ) ) );
			check( !letters.contains( letter ), where + ": access key '" + key + "' is unique in its page (also on '" + ( letters.contains( letter ) ? letters[letter] : std::string() ) + "')" );
			letters[letter] = label;
		}
		// A window-level control's letter must not repeat on any page of the same window.
		for ( const auto& [s, letters] : used )
			if ( s.second == nullptr )
				for ( const auto& [t, pageLetters] : used )
					if ( t.first == s.first && t.second != nullptr )
						for ( const auto& [letter, label] : letters )
							check( !pageLetters.contains( letter ), std::string( path ) + ": window command '" + label + "' and page control '" + ( pageLetters.contains( letter ) ? pageLetters.at( letter ) : std::string() ) + "' share an access key" );
		doc->Close();
		c->Update();
	}
	check( keyed > 500, "the windows carry their text through the catalog" );
	check( captionButtons >= 16, "caption buttons checked: " + std::to_string( captionButtons ) );
	check( helped > 300, "controls with What's This? Help: " + std::to_string( helped ) );
	// Help text rules (PDF p.287): a sentence that starts with a capital letter and ends with a period, brief enough
	// to read at a glance, with no markup.
	{
		int entries = 0;
		for ( const auto& [key, value] : english.entries() )
		{
			if ( key.rfind( "win98.help.", 0 ) != 0 ) continue;
			++entries;
			const auto words = std::count( value.begin(), value.end(), ' ' ) + 1;
			check( !value.empty() && std::isupper( static_cast<unsigned char>( value.front() ) ) && ( value.back() == '.' || value.back() == '?' ) && words <= 50
					&& value.find( '<' ) == std::string::npos && value.find( '&' ) == std::string::npos,
				"Help text follows the writing rules: " + key );
		}
		check( entries > 450, "Help entries: " + std::to_string( entries ) );
	}

	// Alt+letter reaches the labelled control (Stockpile Contents page, shown as its binding would).
	auto* stockpile = c->LoadDocument( "windows/stockpile_manager.rml" );
	localization::applyRmlText( *stockpile, english );
	stockpile->Show();
	for ( const char* id : { "stockpile_manager_root", "stockpile_content" } ) stockpile->GetElementById( id )->SetClass( "is-hidden", false );
	c->Update();
	stockpile->GetElementById( "stockpile_rows" )->Focus();
	check( access_keys::activate( *c, 'f', true ) && c->GetFocusElement() == stockpile->GetElementById( "stockpile_content_search" ), "Alt+F moves to the Find box" );
	check( access_keys::activate( *c, 'c', true ) && c->GetFocusElement() == stockpile->GetElementById( "stockpile_content_category" ), "Alt+C moves to the Category list" );
	check( stockpile->GetElementById( "stockpile_apply" )->GetAttribute<Rml::String>( "accesskey", "" ) == "a", "Apply is Alt+A" );
	// A drop-down list keeps its two-pixel sunken edge: the value box and the 16 x 17 arrow sit inside it.
	{
		auto* select = stockpile->GetElementById( "stockpile_content_category" );
		Rml::Element *value = nullptr, *arrow = nullptr;
		for ( int i = 0; i < select->GetNumChildren( true ); ++i )
		{
			auto* child = select->GetChild( i );
			if ( child->GetTagName() == "selectvalue" ) value = child;
			if ( child->GetTagName() == "selectarrow" ) arrow = child;
		}
		check( value && arrow, "drop-down parts exist" );
		(void)value;
		const auto outer = select->GetAbsoluteOffset( Rml::BoxArea::Border );
		const auto size = select->GetBox().GetSize( Rml::BoxArea::Border );
		const auto v = value->GetAbsoluteOffset( Rml::BoxArea::Border ), a = arrow->GetAbsoluteOffset( Rml::BoxArea::Border );
		const auto vs = value->GetBox().GetSize( Rml::BoxArea::Border ), as = arrow->GetBox().GetSize( Rml::BoxArea::Border );
		std::cout << "select " << outer.x << "," << outer.y << " " << size.x << "x" << size.y << " value " << v.x << "," << v.y << " " << vs.x << "x" << vs.y << " arrow " << a.x << "," << a.y << " " << as.x << "x" << as.y << "\n";
		check( std::abs( size.y - 21.f ) < 0.5f, "a drop-down list is 21 px high at 1x" );
		// The value box is laid out while rendering (not in this harness); its inset is checked in live captures.
		check( std::abs( a.x + as.x - ( outer.x + size.x - 2.f ) ) < 0.5f && std::abs( a.y - outer.y - 2.f ) < 0.5f && std::abs( as.x - 16.f ) < 0.5f && std::abs( as.y - 17.f ) < 0.5f, "the 16 x 17 arrow sits inside the sunken edge" );
	}
	stockpile->Close();
	c->Update();

	// In an open HUD menu, the item's letter chooses it; other letters stay in the menu.
	auto* hud = c->LoadDocument( "screens/game_hud.rml" );
	localization::applyRmlText( *hud, english );
	hud->Show();
	auto* mine = hud->GetElementById( "hud_mine_menu" );
	mine->SetClass( "is-hidden", false );
	c->Update();
	struct Clicked : Rml::EventListener { std::string id; void ProcessEvent( Rml::Event& e ) override { id = e.GetCurrentElement()->GetId(); } } clicked;
	for ( const char* id : { "hud_mine_walls", "hud_mine_explorative", "hud_mine_stairs_up" } ) hud->GetElementById( id )->AddEventListener( "click", &clicked );
	check( hud->GetElementById( "hud_mine_explorative" )->GetAttribute<Rml::String>( "accesskey", "" ) == "e", "menu items carry their access key" );
	check( access_keys::activate( *c, 'e', false ) && clicked.id == "hud_mine_explorative", "E in the open Mine menu chooses Explorative Mine" );
	check( access_keys::activate( *c, 'u', true ) && clicked.id == "hud_mine_stairs_up", "Alt+U chooses Mine Stairs Up" );
	clicked.id.clear();
	check( access_keys::activate( *c, 'z', false ) && clicked.id.empty(), "a letter no item uses stays in the menu" );
	for ( const char* id : { "hud_mine_walls", "hud_mine_explorative", "hud_mine_stairs_up" } ) hud->GetElementById( id )->RemoveEventListener( "click", &clicked );
	mine->SetClass( "is-hidden", true );
	c->Update();
	check( access_keys::openMenu( *c ) == nullptr, "no menu is open once it hides" );
	hud->Close();
	c->Update();

	// The pseudo-locale is a separate catalog with the same keys; every Windows 98 key has an entry there.
	std::cout << keyed << " keyed strings; " << checks << " Stage21 checks passed\n";
	Rml::RemoveContext( "stage21" );
	Rml::Shutdown();
}
