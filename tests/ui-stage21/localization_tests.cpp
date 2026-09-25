/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 21c: every Windows 98 window takes its text from the catalog; access keys travel with the strings
// ("&Name:"), are underlined once, are unique within a window and its page, and reach their controls.
#include "gui/ui/runtime/AccessKeys.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/localization/RmlText.h"
#include "gui/ui/screens/management6b/Management6BText.h"
#include "gui/ui/screens/management6c/Management6CText.h"
#include <RmlUi/Core.h>
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
		if ( a->IsClassSet( "w98-sheet" ) || a->IsClassSet( "w98-msgbox" ) ) return { a, page };
	}
	return { nullptr, page };
}

int main( int argc, char** argv )
{
	check( argc == 2, "assets" );
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
		{ "windows/military_manager.rml", &military }, { "windows/diplomacy_missions.rml", &military } };
	int keyed = 0;
	for ( const auto& [path, catalog] : documents )
	{
		auto* doc = c->LoadDocument( path );
		check( doc != nullptr, std::string( "loads " ) + path );
		localization::applyRmlText( *doc, *catalog );
		std::vector<Rml::Element*> all;
		collect( doc, all );
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
	stockpile->Close();
	c->Update();

	// The pseudo-locale is a separate catalog with the same keys; every Windows 98 key has an entry there.
	std::cout << keyed << " keyed strings; " << checks << " Stage21 checks passed\n";
	Rml::RemoveContext( "stage21" );
	Rml::Shutdown();
}
