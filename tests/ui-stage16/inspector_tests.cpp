/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 16: the creature and object inspectors as Windows 98 property inspectors in palette windows.
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/inspector/InspectorRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace ingnomia::ui;
using namespace ingnomia::ui::inspector;
int checks = 0;
void check( bool value, const char* label )
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
struct Port final : InspectorCommandPort
{
	std::vector<UiActionEnvelope> sent;
	CommandResult dispatch( const UiActionEnvelope& a ) override { sent.push_back( a ); return {}; }
	int count( const char* id ) { return static_cast<int>( std::count_if( sent.begin(), sent.end(), [&]( auto& a ) { return a.id.value == id; } ) ); }
	template <class T> T last( const char* id )
	{
		const auto it = std::find_if( sent.rbegin(), sent.rend(), [&]( auto& a ) { return a.id.value == id; } );
		check( it != sent.rend(), id );
		return std::get<T>( it->payload );
	}
};

CreatureInspectorState gnome( bool role )
{
	CreatureInspectorState c;
	c.id = CreatureId { 7 };
	c.name = "Ada";
	c.profession = "Miner";
	c.activity = "Mining";
	c.professionReported = c.skillsReported = c.equipmentReported = c.inventoryReported = true;
	c.strength = 12;
	c.dexterity = 9;
	c.attributesReported = { true, true, false, false, false, false };
	c.hunger = 80;
	c.needsReported = { true, false, false, false };
	c.skills = { { "Mining", "level 5 | active", 0 }, { "Cooking", "level 9 | inactive", 0 }, { "Brewing", "level 2 | active", 0 } };
	c.inventory = { { "Pickaxe", {}, 0 } };
	if ( role )
	{
		c.equipmentRole = MilitaryRoleId { 3 };
		c.equipmentRoleName = "Guard";
	}
	const UniformSlot slots[] = { UniformSlot::HeadArmor, UniformSlot::ChestArmor, UniformSlot::ArmArmor, UniformSlot::HandArmor, UniformSlot::LegArmor,
		UniformSlot::FootArmor, UniformSlot::LeftHandHeld, UniformSlot::RightHandHeld, UniformSlot::Back };
	const char* labels[] = { "Head", "Chest", "Arms", "Hands", "Legs", "Feet", "Left hand", "Right hand", "Back" };
	for ( int i = 0; i < 9; ++i )
	{
		EquipmentSlotState slot;
		slot.slot = slots[i];
		slot.label = labels[i];
		if ( i == 0 ) { slot.item = "Helm"; slot.material = "Iron"; }
		slot.desiredType = CatalogId { i == 0 ? "Helm" : "none" };
		slot.choices = { { CatalogId { "none" }, {} }, { CatalogId { "Helm" }, { CatalogId { "any" }, CatalogId { "Iron" }, CatalogId { "Copper" } } } };
		c.equipmentSlots.push_back( slot );
	}
	return c;
}

TileInspectorState crowdedTile()
{
	TileInspectorState t;
	t.id = TileId { 55 };
	t.position = WorldPosition { 10, 20, 5 };
	t.floor = "Granite floor";
	t.plant = "Apple tree";
	t.plantIsTree = true;
	t.canRemoveFloor = t.canFell = true;
	for ( int i = 0; i < 12; ++i ) t.items.push_back( { "Plank", "Oak", static_cast<std::uint32_t>( i + 1 ) } );
	t.creatures = { { CreatureId { 31 }, "Ada" }, { CreatureId { 32 }, "Bert" }, { CreatureId { 33 }, "Cow" } };
	t.hasJob = true;
	t.jobName = "Fell tree";
	t.jobPriority = "3";
	t.canRaisePriority = true;
	t.canLowerPriority = false;
	t.designation = DesignationId { 4 };
	t.designationName = "Stockpile 1";
	t.canManage = t.canDeleteStockpile = true;
	return t;
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
	auto* c = Rml::CreateContext( "stage16", { 384, 380 } );
	{
		// ================================================================ Creature inspector
		Port port;
		InspectorRmlBinding binding( *c, 1, 0, true );
		InspectorController controller( port, binding );
		check( binding.initialize( controller ), "binding loads" );
		auto* doc  = binding.document();
		auto el    = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click = [&]( const std::string& id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); c->Update(); };
		auto inner = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto shown = [&]( const std::string& id ) { return el( id )->IsVisible( true ); };
		auto off   = [&]( const std::string& id ) { return el( id )->HasAttribute( "disabled" ); };
		auto choose = [&]( const std::string& id, const std::string& value )
		{
			auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( el( id ) );
			check( select != nullptr, "select" );
			select->SetValue( value );
			c->Update();
		};
		controller.beginWorld( WorldEpoch { 16 } );
		controller.showCreature( gnome( false ), WorldPosition { 4, 5, 6 } );
		controller.setProfessionChoices( { "Miner", "Woodcutter", "Cook" } );
		c->Update();

		check( el( "creature_preview" )->IsClassSet( "w98-palette" ) && el( "creature_preview" )->IsClassSet( "w98-sheet" ), "creature inspector is a palette window" );
		check( shown( "creature_preview" ) && !shown( "inspector_panel" ), "only the creature inspector shows" );
		check( inner( "creature_preview_title" ) == "Ada Properties", "caption names the object: <Object> Properties" );
		check( el( "creature_preview" )->QuerySelector( ".w98-buttonbar" ) == nullptr, "no OK / Cancel / Apply button bar (edits apply at once)" );
		check( shown( "creature_preview_close" ), "caption has only a Close button" );
		check( shown( "creature_preview_camera_panel" ) && inner( "creature_preview_activity" ) == "Mining" && inner( "creature_preview_camera_position" ) == "4, 5, 6", "General page shows activity and position" );
		for ( const char* tab : { "creature_preview_nav_camera", "creature_preview_nav_stats", "creature_preview_nav_expertise", "creature_preview_nav_equipment", "creature_preview_nav_inventory" } )
			check( shown( tab ) && el( tab )->IsClassSet( "c-tabs__tab" ), "a gnome shows all five tabs" );

		// Attributes: reported values, and "Unknown" (never zero) for the rest.
		click( "creature_preview_nav_stats" );
		check( shown( "creature_preview_stats_panel" ) && !shown( "creature_preview_camera_panel" ), "Attributes tab shows its page" );
		check( inner( "creature_preview_strength" ) == "12" && inner( "creature_preview_dexterity" ) == "9", "reported attributes shown" );
		check( inner( "creature_preview_wisdom" ) == "Unknown" && inner( "creature_preview_charisma" ) == "Unknown", "unreported attributes read Unknown" );
		check( inner( "creature_preview_hunger" ) == "80" && inner( "creature_preview_thirst" ) == "Unknown", "unreported needs read Unknown" );

		// Ctrl+Tab moves to the next page, Ctrl+Shift+Tab back.
		{
			Rml::Dictionary p;
			p["key_identifier"] = static_cast<int>( Rml::Input::KI_TAB );
			p["ctrl_key"] = 1;
			el( "creature_preview_nav_stats" )->DispatchEvent( "keydown", p );
			c->Update();
			check( shown( "creature_preview_expertise_panel" ), "Ctrl+Tab moves to the next page" );
			p["shift_key"] = 1;
			el( "creature_preview_nav_stats" )->DispatchEvent( "keydown", p );
			c->Update();
			check( shown( "creature_preview_stats_panel" ), "Ctrl+Shift+Tab moves back" );
		}

		// Skills: list view with Level and Active columns, sortable headings with one sort mark.
		click( "creature_preview_nav_expertise" );
		auto* skills = el( "creature_preview_skills" );
		check( skills->GetNumChildren() == 3 && std::string( skills->GetChild( 0 )->GetChild( 0 )->GetInnerRML() ) == "Brewing", "skills sorted by name" );
		check( std::string( skills->GetChild( 0 )->GetChild( 1 )->GetInnerRML() ) == "2" && std::string( skills->GetChild( 0 )->GetChild( 2 )->GetInnerRML() ) == "Yes", "level and active columns" );
		check( shown( "creature_preview_skill_mark_name" ) && !shown( "creature_preview_skill_mark_level" ), "sort mark on the sorted column only" );
		click( "creature_preview_skill_sort_level" );
		check( std::string( skills->GetChild( 0 )->GetChild( 0 )->GetInnerRML() ) == "Cooking" && shown( "creature_preview_skill_mark_level" ) && !shown( "creature_preview_skill_mark_name" ), "Level heading sorts highest first" );

		// Profession: a drop-down list that applies at once; re-rendering never sends a change.
		check( rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( el( "creature_preview_profession" ) )->GetValue() == "Miner", "drop-down shows the current profession" );
		const auto before = port.count( "population.set_profession" );
		controller.setProfessionChoices( { "Miner", "Woodcutter", "Cook", "Brewer" } );
		c->Update();
		check( port.count( "population.set_profession" ) == before, "rebuilding the list sends nothing" );
		choose( "creature_preview_profession", "Cook" );
		check( port.count( "population.set_profession" ) == before + 1 && port.last<SetProfessionPayload>( "population.set_profession" ).profession.value == "Cook", "choosing a profession applies it once" );

		// Equipment without a military role: scope explains why nothing can change.
		click( "creature_preview_nav_equipment" );
		check( inner( "creature_preview_equipment_scope" ).find( "no military role" ) != std::string::npos, "scope states there is no uniform without a role" );
		check( off( "creature_equipment_slot_head" ) && !shown( "creature_preview_equipment_editor" ), "slots unavailable without a role" );
		check( std::string( el( "creature_equipment_slot_head" )->GetChild( 1 )->GetInnerRML() ) == "Iron Helm" && std::string( el( "creature_equipment_slot_chest" )->GetChild( 1 )->GetInnerRML() ) == "None", "slot list shows each item" );

		// Inventory.
		click( "creature_preview_nav_inventory" );
		check( el( "creature_preview_inventory" )->GetNumChildren() == 1 && !shown( "creature_preview_inventory_empty" ), "carried items listed" );

		// Equipment with a role: scope first, then an explicit Apply for the whole role.
		controller.showCreature( gnome( true ), WorldPosition { 4, 5, 6 } );
		c->Update();
		click( "creature_preview_nav_equipment" );
		check( inner( "creature_preview_equipment_scope" ).find( "Guard role" ) != std::string::npos && inner( "creature_preview_equipment_scope" ).find( "every citizen" ) != std::string::npos, "scope names the role and its members before any change" );
		click( "creature_equipment_slot_chest" );
		check( shown( "creature_preview_equipment_editor" ) && inner( "creature_preview_equipment_editor_title" ) == "Change Chest" && el( "creature_equipment_slot_chest" )->IsClassSet( "is-selected" ), "choosing a slot opens its editor" );
		const auto uniformBefore = port.count( "military.set_uniform_slot" );
		choose( "creature_preview_equipment_type", "Helm" );
		check( controller.state().equipmentDraftType.value == "Helm" && port.count( "military.set_uniform_slot" ) == uniformBefore, "choosing a type only drafts it" );
		choose( "creature_preview_equipment_material", "Copper" );
		check( controller.state().equipmentDraftMaterial && controller.state().equipmentDraftMaterial->value == "Copper", "material drafted" );
		click( "creature_preview_equipment_apply" );
		{
			check( port.count( "military.set_uniform_slot" ) == uniformBefore + 1, "Apply sends one uniform change" );
			const auto p = port.last<SetUniformSlotPayload>( "military.set_uniform_slot" );
			check( p.role == MilitaryRoleId { 3 } && p.slot == UniformSlot::ChestArmor && p.type.value == "Helm" && p.material && p.material->value == "Copper", "uniform change carries role, slot, type and material" );
		}

		// An animal reports no skills or equipment: those tabs are not offered.
		{
			CreatureInspectorState cow;
			cow.id = CreatureId { 40 };
			cow.name = "Cow";
			cow.hunger = 50;
			cow.needsReported = { true, false, false, false };
			controller.showCreature( cow, WorldPosition { 1, 1, 1 } );
			c->Update();
			check( !shown( "creature_preview_nav_expertise" ) && !shown( "creature_preview_nav_equipment" ) && shown( "creature_preview_nav_stats" ), "animals only get the tabs they report" );
			check( inner( "creature_preview_strength" ) == "Unknown", "animal attributes Unknown" );
			controller.showCreature( gnome( true ), WorldPosition { 4, 5, 6 } );
			c->Update();
		}

		// Fit: every page and the tabs inside the 384 x 380 window at four scales; pages never scroll.
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			for ( const char* tab : { "creature_preview_nav_camera", "creature_preview_nav_stats", "creature_preview_nav_expertise", "creature_preview_nav_equipment", "creature_preview_nav_inventory" } )
			{
				click( tab );
				if ( std::string( tab ) == "creature_preview_nav_equipment" ) { click( "creature_equipment_slot_head" ); c->Update(); }
				const std::string panel = std::string( el( tab )->GetAttribute<Rml::String>( "aria-controls", "" ) );
				auto* page = el( panel );
				check( page->IsVisible( true ) && page->GetScrollHeight() <= page->GetClientHeight() + 1.f, "page fits without scrolling" );
				auto* frame = page->GetParentNode();
				check( frame->GetAbsoluteTop() + frame->GetOffsetHeight() <= 380.f * k + 1.f && frame->GetAbsoluteLeft() + frame->GetOffsetWidth() <= 384.f * k + 1.f, "page frame inside the window" );
				auto* last = el( "creature_preview_nav_inventory" );
				check( last->GetAbsoluteLeft() + last->GetOffsetWidth() <= 384.f * k, "tabs fit on one row" );
			}
			check( inner( "creature_preview_equipment_editor_title" ) == "Change Head", "editor fits with the slot list" );
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 384, 380 } );
		bool closed = false;
		binding.setCloseHandler( [&] { closed = true; } );
		click( "creature_preview_close" );
		check( closed && port.count( "inspect.clear" ) == 1, "Close closes the inspector" );
		binding.shutdown();
	}
	{
		// ================================================================ Tile inspector (crowded tile)
		Port port;
		InspectorRmlBinding binding( *c, 0, 0, true );
		InspectorController controller( port, binding );
		check( binding.initialize( controller ), "tile binding loads" );
		binding.setLiveInspection( true );
		auto* doc  = binding.document();
		auto el    = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click = [&]( const std::string& id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); c->Update(); };
		auto shown = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); return e && e->IsVisible( true ); };
		auto off   = [&]( const std::string& id ) { return el( id )->HasAttribute( "disabled" ); };
		controller.beginWorld( WorldEpoch { 16 } );
		c->Update();
		check( shown( "inspector_panel" ) && shown( "tile_inspection_empty" ), "empty live inspector explains what to do" );
		controller.showTile( crowdedTile() );
		c->Update();
		check( el( "inspector_panel" )->IsClassSet( "w98-palette" ) && std::string( el( "inspector_title" )->GetInnerRML() ) == "Tile Properties", "tile inspector is a palette titled Tile Properties" );
		check( shown( "inspector_tile" ) && shown( "live_tile_rows" ), "tile list shown" );
		check( shown( "live_tile_creature_0" ) && shown( "live_tile_creature_1" ) && shown( "live_tile_creature_2" ), "each creature is its own list row" );
		check( el( "live_tile_creature_0" )->IsClassSet( "is-selected" ) && !off( "live_tile_open_creature" ), "first creature chosen by default and Inspect available" );
		click( "live_tile_creature_1" );
		check( el( "live_tile_creature_1" )->IsClassSet( "is-selected" ) && !el( "live_tile_creature_0" )->IsClassSet( "is-selected" ), "clicking a row selects that creature" );
		click( "live_tile_open_creature" );
		check( port.count( "inspect.select" ) == 1 && port.last<SelectPayload>( "inspect.select" ).target.id == 32 && port.last<SelectPayload>( "inspect.select" ).target.kind == EntityKind::Creature, "Inspect opens the chosen creature by ID" );
		el( "live_tile_creature_2" )->DispatchEvent( "dblclick", Rml::Dictionary {} );
		c->Update();
		check( port.count( "inspect.select" ) == 2 && port.last<SelectPayload>( "inspect.select" ).target.id == 33, "double-click opens that creature" );
		// Tile commands act on the tile; unavailable ones are shown unavailable.
		check( shown( "live_tile_remove_floor" ) && shown( "live_tile_fell" ) && !shown( "live_tile_mine" ), "only applicable commands are offered" );
		check( !off( "live_tile_raise_job" ) && off( "live_tile_lower_job" ), "priority commands follow the game" );
		click( "live_tile_fell" );
		check( port.last<TileContextPayload>( "tile.execute_context_action" ).action == TileContextAction::FellTree && port.last<TileContextPayload>( "tile.execute_context_action" ).tile == TileId { 55 }, "Fell Tree acts on the tile" );
		// The same creature stays chosen when the tile refreshes; a creature that leaves is not replaced silently by position.
		auto refreshed = crowdedTile();
		refreshed.items.pop_back();
		controller.showTile( refreshed );
		c->Update();
		check( el( "live_tile_creature_2" )->IsClassSet( "is-selected" ), "selection kept by ID across a refresh" );
		refreshed.creatures.erase( refreshed.creatures.begin() + 2 );
		controller.showTile( refreshed );
		c->Update();
		check( el( "live_tile_creature_0" )->IsClassSet( "is-selected" ) && !shown( "live_tile_creature_2" ), "a creature that left falls back to the first row" );
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			c->Update();
			auto* frame = el( "inspector_scroll" );
			auto* page = el( "inspector_tile" );
			check( page->GetScrollHeight() <= page->GetClientHeight() + 1.f && frame->GetScrollHeight() <= frame->GetClientHeight() + 1.f, "tile page does not scroll" );
			check( el( "live_tile_rows" )->GetScrollHeight() > el( "live_tile_rows" )->GetClientHeight(), "the busy list scrolls inside its border instead" );
			auto* locate = el( "inspector_locate" );
			check( locate->GetAbsoluteTop() + locate->GetOffsetHeight() <= 380.f * k + 1.f, "buttons inside the window" );
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 384, 380 } );
		binding.shutdown();
	}
	{
		// ================================================================ Blueprint, workshop, stockpile, agriculture
		Port port;
		InspectorRmlBinding binding( *c, 0, 0, true );
		InspectorController controller( port, binding );
		check( binding.initialize( controller ), "object binding loads" );
		auto* doc  = binding.document();
		auto el    = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto inner = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto shown = [&]( const std::string& id ) { return el( id )->IsVisible( true ); };
		auto fits  = [&]( const char* id, const char* label )
		{
			for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
			{
				c->SetDensityIndependentPixelRatio( scale );
				const int k = std::max( 1, int( scale + 0.5f ) );
				c->SetDimensions( { 384 * k, 380 * k } );
				c->Update();
				auto* page = el( id );
				auto* frame = el( "inspector_scroll" );
				check( page->IsVisible( true ) && page->GetScrollHeight() <= page->GetClientHeight() + 1.f && frame->GetScrollHeight() <= frame->GetClientHeight() + 1.f, label );
			}
			c->SetDensityIndependentPixelRatio( 1.f );
			c->SetDimensions( { 384, 380 } );
			c->Update();
		};
		controller.beginWorld( WorldEpoch { 16 } );
		TileInspectorState blueprint;
		blueprint.id = TileId { 60 };
		blueprint.hasJob = true;
		blueprint.jobName = "BuildWall";
		blueprint.jobPriority = "2";
		blueprint.requiredSkill = "Masonry";
		blueprint.canRaisePriority = false;
		blueprint.canLowerPriority = true;
		blueprint.requiredItems = { { "RawStone", "Granite", 2, false }, { "Plank", "", 1, true } };
		controller.showTile( blueprint );
		c->Update();
		check( shown( "inspector_blueprint" ) && !shown( "inspector_tile" ) && inner( "inspector_title" ) == "Construction Blueprint Properties", "blueprint page" );
		check( el( "blueprint_missing_items" )->GetNumChildren() == 1 && inner( "blueprint_status" ) == "Waiting for materials.", "only missing materials listed" );
		check( el( "blueprint_raise_job" )->HasAttribute( "disabled" ) && !el( "blueprint_lower_job" )->HasAttribute( "disabled" ), "priority buttons follow the game" );
		check( el( "blueprint_cancel_job" )->GetParentNode()->GetPreviousSibling()->IsClassSet( "w98-separator" ), "Cancel Blueprint is set apart below a separator" );
		fits( "inspector_blueprint", "blueprint page fits" );

		WorkshopInspectorState workshop;
		workshop.id = WorkshopId { 5 };
		workshop.name = "Carpenter";
		workshop.priority = 2;
		workshop.maxPriority = 5;
		workshop.linkedStockpile = true;
		controller.showWorkshop( workshop );
		c->Update();
		check( shown( "inspector_workshop" ) && inner( "inspector_title" ) == "Carpenter Properties" && inner( "workshop_priority" ) == "2 of 5" && inner( "workshop_link" ) == "Linked", "workshop properties" );
		el( "workshop_toggle_suspended" )->DispatchEvent( "click", Rml::Dictionary {} );
		check( port.count( "workshop.set_basics" ) == 1, "Suspend applies at once" );
		fits( "inspector_workshop", "workshop page fits" );

		StockpileInspectorState stockpile;
		stockpile.id = StockpileId { 8 };
		stockpile.name = "Stockpile 1";
		stockpile.priority = 0;
		stockpile.maxPriority = 5;
		stockpile.capacity = 40;
		stockpile.itemCount = 12;
		for ( int i = 0; i < 20; ++i ) stockpile.contents.push_back( { "Plank", "Oak", static_cast<std::uint32_t>( i + 1 ) } );
		controller.showStockpile( stockpile );
		c->Update();
		check( shown( "inspector_stockpile" ) && inner( "stockpile_priority" ) == "1 of 5" && inner( "stockpile_capacity" ) == "12 of 40", "stockpile properties" );
		check( el( "stockpile_contents" )->GetNumChildren() == 20 && !shown( "stockpile_contents_empty" ), "stockpile contents listed" );
		fits( "inspector_stockpile", "stockpile page fits" );

		AgricultureInspectorState grove;
		grove.target.kind = AgricultureKind::Grove;
		grove.target.designation = DesignationId { 9 };
		grove.name = "Grove 1";
		grove.pick = true;
		controller.showAgriculture( grove );
		c->Update();
		check( shown( "inspector_agriculture" ) && inner( "agriculture_kind" ) == "Grove" && inner( "agriculture_toggle_primary" ) == "Stop Picking" && inner( "agriculture_harvest" ) == "Pick fruit", "grove properties" );
		fits( "inspector_agriculture", "agriculture page fits" );
		binding.shutdown();
	}
	Rml::RemoveContext( "stage16" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
