/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 11: farms, groves and pastures as Windows 98 property sheets with explicit targets.
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include "gui/ui/runtime/NumericEditor.h"
#include "gui/ui/screens/management6a/Management6ARmlBinding.h"
using namespace ingnomia::ui;
int checks = 0;
void check( bool value, const char* label )
{
	++checks;
	if ( !value )
	{
		std::cerr << "FAIL " << label << "\n";
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
void update( Rml::Context& c )
{
	c.Update();
	if ( connected_tabs::reconcile( c ) ) c.Update();
}
struct ManagerPort : management6a::CommandPort
{
	std::vector<UiActionEnvelope> sent;
	bool reject = false;
	management6a::CommandResult dispatch( const UiActionEnvelope& a, management6a::DispatchOrigin ) override
	{
		sent.push_back( a );
		return reject ? management6a::CommandResult { management6a::CommandStatus::Rejected, false, "Rejected" } : management6a::CommandResult {};
	}
	std::size_t count( const char* id ) const { return std::count_if( sent.begin(), sent.end(), [&]( const auto& a ) { return a.id.value == id; } ); }
};
struct ManagerView : management6a::ViewPort
{
	void stateChanged( const management6a::Management6AState& ) override {}
};
struct ClipboardSystem : Rml::SystemInterface
{
	Rml::String clipboard;
	void GetClipboardText( Rml::String& text ) override { text = clipboard; }
};

using namespace management6a;

FarmPlotRow plot( int x, int y, bool tilled = true, bool planted = false, bool ready = false )
{
	FarmPlotRow row;
	row.position = { x, y, 92 };
	row.tilled   = tilled;
	row.planted  = planted;
	row.ready    = ready;
	if ( planted ) row.plantedCrop = CatalogId { "Wheat" };
	return row;
}

int main( int argc, char** argv )
{
	check( argc == 2, "assets" );
	Files files;
	files.root = argv[1];
	Renderer renderer;
	ClipboardSystem system;
	Rml::SetFileInterface( &files );
	Rml::SetRenderInterface( &renderer );
	Rml::SetSystemInterface( &system );
	check( Rml::Initialise(), "initialize" );
	ClassicFocusInstancer focus;
	Rml::Factory::RegisterDecoratorInstancer( "win98-focus", &focus );
	Rml::LoadFontFace( "fonts/LatoLatin-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Bold.ttf" );
	auto* c = Rml::CreateContext( "stage11", { 720, 720 } );
	{
		ManagerPort port;
		ManagerView view;
		Management6AController controller( port, view );
		Management6ARmlBinding binding( *c );
		check( binding.initialize( controller ), "binding loads" );
		controller.addViewPort( binding );
		controller.beginWorld( WorldEpoch { 11 } );
		auto* doc      = binding.agricultureDocument();
		auto activate  = [&]( const char* id ) { check( binding.activateElement( id ), id ); update( *c ); };
		auto field     = [&]( const char* id, const char* value ) { check( binding.setFormValueForProbe( id, value ), id ); update( *c ); };
		auto el        = [&]( const char* id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id ); return e; };
		auto shown     = [&]( const char* id ) { return el( id )->IsVisible( true ); };
		auto disabled  = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto clickWith = [&]( const char* id, const char* modifier ) {
			Rml::Dictionary p;
			if ( modifier ) p[modifier] = 1;
			el( id )->DispatchEvent( "click", p );
			update( *c );
		};
		auto key = [&]( const char* id, int keyId, const char* modifier = nullptr ) {
			Rml::Dictionary p;
			p["key_identifier"] = keyId;
			if ( modifier ) p[modifier] = 1;
			el( id )->DispatchEvent( "keydown", p );
			update( *c );
		};
		auto lastPlots = [&]( const char* id ) {
			for ( auto it = port.sent.rbegin(); it != port.sent.rend(); ++it )
				if ( it->id.value == id )
				{
					if ( auto* p = std::get_if<SetFarmPlotCropPayload>( &it->payload ) ) return p->plots;
					if ( auto* q = std::get_if<QueueFarmPlotCropPayload>( &it->payload ) ) return q->plots;
				}
			return std::vector<WorldPosition> {};
		};

		// ---------------------------------------------------------------- Farm
		AgricultureSnapshot farm;
		farm.target  = { AgricultureKind::Farm, DesignationId { 31 } };
		farm.name    = "South farm";
		farm.product = CatalogId { "Wheat" };
		farm.plots = 6, farm.tilled = 5, farm.planted = 2, farm.ready = 1;
		farm.harvest = true;
		farm.catalog = { { CatalogId { "Wheat" }, "Wheat", 34, 12, 16 }, { CatalogId { "Barley" }, "Barley", 21, 8, 5 }, { CatalogId { "Cabbage" }, "Cabbage", 0, 3, 7 } };
		farm.fields  = { plot( 0, 0 ), plot( 1, 0 ), plot( 2, 0, true, true ), plot( 0, 1, false ), plot( 1, 1 ), plot( 2, 1, true, true, true ) };
		farm.fields[4].orders = { { 7, CatalogId { "Barley" }, 2, false }, { 8, CatalogId { "Cabbage" }, 1, true }, { 9, CatalogId { "Wheat" }, 3, false } };
		controller.showAgriculture( farm, Revision { 1 }, WorldPosition { 1, 1, 92 } );
		update( *c );

		check( el( "agriculture_tabs" )->IsClassSet( "c-connected-tabs" ), "connected property tabs" );
		check( el( "agriculture_title" )->GetInnerRML() == "South farm Properties", "caption names the object and Properties" );
		check( controller.state().agriculture.pane == AgriculturePane::Plots, "farm opens on Plots" );
		check( shown( "agriculture_view_general" ) && shown( "agriculture_view_plots" ) && shown( "agriculture_view_queue" ) && shown( "agriculture_view_crops" ), "farm tabs" );
		check( !shown( "agriculture_view_animals" ) && !shown( "agriculture_view_food" ), "farm hides pasture pages" );
		controller.setAgriculturePane( AgriculturePane::Animals );
		check( controller.state().agriculture.pane == AgriculturePane::Plots, "unsupported page is rejected" );
		check( doc->GetElementById( "agriculture_priority" ) == nullptr, "no Priority control: FarmingManager priority is unimplemented" );
		check( el( "agriculture_plot_grid" )->GetInnerRML().find( "is-tilled" ) != Rml::String::npos && el( "agriculture_plot_grid" )->GetInnerRML().find( "w98-plot__ready" ) != Rml::String::npos, "tilled and ready plots have their own glyphs, not only color" );

		// Selection model: click selects one, Ctrl+click toggles, Shift+click selects a rectangle, Ctrl+A all.
		clickWith( "agriculture_plot_0_0_92", nullptr );
		check( controller.state().agriculture.selectedPlots == std::vector<WorldPosition> { { 0, 0, 92 } }, "click selects exactly one plot" );
		clickWith( "agriculture_plot_1_0_92", nullptr );
		check( controller.state().agriculture.selectedPlots == std::vector<WorldPosition> { { 1, 0, 92 } }, "second click replaces the selection" );
		clickWith( "agriculture_plot_2_1_92", "ctrl_key" );
		check( controller.state().agriculture.selectedPlots.size() == 2, "Ctrl+click adds a plot" );
		clickWith( "agriculture_plot_2_1_92", "ctrl_key" );
		check( controller.state().agriculture.selectedPlots.size() == 1, "Ctrl+click removes a plot" );
		clickWith( "agriculture_plot_0_0_92", nullptr );
		clickWith( "agriculture_plot_1_1_92", "shift_key" );
		{
			auto sel = controller.state().agriculture.selectedPlots;
			std::sort( sel.begin(), sel.end() );
			check( sel == std::vector<WorldPosition> { { 0, 0, 92 }, { 0, 1, 92 }, { 1, 0, 92 }, { 1, 1, 92 } }, "Shift+click selects the rectangle from the anchor" );
		}
		check( el( "agriculture_selected_title" )->GetInnerRML() == "Selected plots (4)", "group names the selected plot count" );
		check( el( "agriculture_plot_detail" )->GetInnerRML().find( "each of the 4 selected plots" ) != Rml::String::npos, "scope sentence names the affected plots" );
		key( "agriculture_plot_grid", Rml::Input::KI_A, "ctrl_key" );
		check( controller.state().agriculture.selectedPlots.size() == 6, "Ctrl+A selects all plots" );
		activate( "agriculture_plot_clear" );
		check( controller.state().agriculture.selectedPlots.empty() && disabled( "agriculture_plot_assign" ) && disabled( "agriculture_plot_default" ), "commands unavailable without a selection" );
		// Keyboard: arrows move the focus rectangle, Space selects.
		clickWith( "agriculture_plot_0_0_92", nullptr );
		key( "agriculture_plot_grid", Rml::Input::KI_RIGHT );
		check( controller.state().agriculture.focusedPlot == WorldPosition { 1, 0, 92 } && controller.state().agriculture.selectedPlots.size() == 1, "arrow moves focus without changing the selection" );
		key( "agriculture_plot_grid", Rml::Input::KI_DOWN );
		key( "agriculture_plot_grid", Rml::Input::KI_SPACE, "ctrl_key" );
		check( controller.state().agriculture.selectedPlots.size() == 2, "Ctrl+Space adds the focused plot" );
		key( "agriculture_plot_grid", Rml::Input::KI_RIGHT, "shift_key" );
		{
			auto sel = controller.state().agriculture.selectedPlots;
			std::sort( sel.begin(), sel.end() );
			check( sel == std::vector<WorldPosition> { { 1, 1, 92 }, { 2, 1, 92 } }, "Shift+arrow selects the range from the anchor" );
		}

		// Plot commands act immediately and target exactly the selected plots.
		clickWith( "agriculture_plot_0_0_92", nullptr );
		clickWith( "agriculture_plot_2_0_92", "ctrl_key" );
		field( "agriculture_plot_crop", "Barley" );
		check( controller.state().agriculture.selectedProduct == CatalogId { "Barley" }, "crop drop-down chooses the command crop" );
		auto n = port.sent.size();
		activate( "agriculture_plot_assign" );
		check( port.sent.size() == n + 1 && std::get<SetFarmPlotCropPayload>( port.sent.back().payload ).crop == CatalogId { "Barley" }, "Assign Crop sends the chosen crop" );
		{
			auto sent = lastPlots( "agriculture.set_plot_crop" );
			std::sort( sent.begin(), sent.end() );
			check( sent == std::vector<WorldPosition> { { 0, 0, 92 }, { 2, 0, 92 } }, "Assign Crop targets exactly the selected plots" );
		}
		activate( "agriculture_plot_default" );
		check( std::get<SetFarmPlotCropPayload>( port.sent.back().payload ).crop.value.empty() && lastPlots( "agriculture.set_plot_crop" ).size() == 2, "Use Default clears only the selected plots' overrides" );
		field( "agriculture_plot_count", "3" );
		activate( "agriculture_plot_queue" );
		{
			const auto q = std::get<QueueFarmPlotCropPayload>( port.sent.back().payload );
			check( q.count == 3 && !q.repeat && q.plots.size() == 2, "Queue sends plantings per plot for the selected plots" );
		}
		activate( "agriculture_plot_repeat" );
		check( std::get<QueueFarmPlotCropPayload>( port.sent.back().payload ).repeat, "Queue Repeat sends a repeat planting" );
		field( "agriculture_plot_count", "0" );
		n = port.sent.size();
		activate( "agriculture_plot_queue" );
		check( port.sent.size() == n, "invalid planting count does not queue" );
		field( "agriculture_plot_count", "1" );

		// Plot Queue: one plot, a selected planting, and Move Up / Move Down / Remove on that planting's ID.
		clickWith( "agriculture_plot_1_1_92", nullptr );
		controller.setAgriculturePane( AgriculturePane::PlotQueue );
		update( *c );
		check( el( "agriculture_plot_orders" )->GetInnerRML().find( "Repeat" ) != Rml::String::npos, "repeat plantings are labelled" );
		check( disabled( "agriculture_order_up" ) && disabled( "agriculture_order_remove" ), "order commands need a selected planting" );
		activate( "agriculture_order_8" );
		check( !disabled( "agriculture_order_up" ) && !disabled( "agriculture_order_down" ), "middle planting can move both ways" );
		activate( "agriculture_order_up" );
		{
			const auto m = std::get<MoveFarmPlotOrderPayload>( port.sent.back().payload );
			check( m.order == 8 && m.direction == MoveDirection::Up && m.plot == WorldPosition { 1, 1, 92 }, "Move Up targets the selected planting ID on the selected plot" );
		}
		// Reordered snapshot keeps the target by ID.
		auto farm2 = farm;
		std::swap( farm2.fields[4].orders[0], farm2.fields[4].orders[1] );
		controller.showAgriculture( farm2, Revision { 2 } );
		update( *c );
		check( disabled( "agriculture_order_up" ) && !disabled( "agriculture_order_down" ), "top planting cannot move up" );
		activate( "agriculture_order_remove" );
		check( std::get<FarmPlotOrderPayload>( port.sent.back().payload ).order == 8, "Remove targets the same planting after reorder" );
		clickWith( "agriculture_plot_0_0_92", "ctrl_key" );
		update( *c );
		check( el( "agriculture_queue_plot" )->GetInnerRML().find( "exactly one plot" ) != Rml::String::npos && disabled( "agriculture_order_remove" ), "queue commands need exactly one plot" );

		// General page: settings stay pending until Apply.
		controller.setAgriculturePane( AgriculturePane::General );
		update( *c );
		check( shown( "agriculture_work_farm" ) && !shown( "agriculture_work_grove" ) && !shown( "agriculture_work_pasture" ), "farm shows only its work rules" );
		check( el( "agriculture_summary" )->GetInnerRML().find( "6 plots: 5 tilled, 2 planted, 1 ready" ) != Rml::String::npos, "summary presents subsets of the plots, not totals" );
		n = port.sent.size();
		field( "agriculture_name", "North farm" );
		activate( "agriculture_toggle_harvest" );
		check( port.sent.size() == n && controller.state().agriculture.draft.dirty && !controller.state().agriculture.draft.options.harvest, "name and check box stage pending changes" );
		check( !disabled( "agriculture_apply" ), "Apply available with changes" );
		controller.setAgriculturePane( AgriculturePane::Crops );
		update( *c );
		activate( "agriculture_farm_crop_" "426172" "6c6579" );
		check( controller.state().agriculture.draft.options.product == CatalogId { "Barley" } && port.sent.size() == n, "choosing a default crop is a pending setting" );
		check( el( "agriculture_default_crop" )->GetInnerRML().find( "applies when you click Apply" ) != Rml::String::npos, "pending default is labelled" );
		controller.setAgriculturePane( AgriculturePane::General );
		update( *c );
		check( controller.state().agriculture.draft.name == "North farm", "page switch keeps the draft" );
		activate( "agriculture_apply" );
		check( port.count( "agriculture.set_basics" ) == 1 && port.count( "agriculture.set_harvest_options" ) == 1 && port.count( "agriculture.select_product" ) == 1, "Apply sends one command per changed setting" );
		{
			const auto b = std::get<SetAgricultureBasicsPayload>( port.sent[n].payload );
			check( b.name == "North farm" && b.priority == farm.priority && b.target == farm.target, "basics keep the authoritative priority and exact target" );
		}
		check( controller.state().agriculture.draft.pending && disabled( "agriculture_name" ), "sheet waits for the authoritative snapshot" );
		farm.name    = "North farm";
		farm.harvest = false;
		farm.product = CatalogId { "Barley" };
		controller.showAgriculture( farm, Revision { 3 } );
		update( *c );
		check( !controller.state().agriculture.draft.dirty && !controller.state().agriculture.draft.pending && disabled( "agriculture_apply" ), "confirmed snapshot clears the draft" );
		// Close with pending changes asks; No discards.
		field( "agriculture_name", "Discard me" );
		n = port.sent.size();
		check( !binding.canClose(), "Close with pending changes asks" );
		activate( "agriculture_review_alternate" );
		check( port.sent.back().id.value == "nav.close" && controller.state().agriculture.draft.name == "North farm", "No discards and closes" );
		controller.showAgriculture( farm, Revision { 4 } );
		update( *c );
		field( "agriculture_name", "Cancelled" );
		activate( "agriculture_cancel" );
		check( port.sent.back().id.value == "nav.close" && !controller.state().agriculture.draft.dirty, "Cancel discards and closes" );
		// Empty name uses a message box.
		controller.showAgriculture( farm, Revision { 5 } );
		update( *c );
		field( "agriculture_name", "   " );
		n = port.sent.size();
		activate( "agriculture_apply" );
		check( port.sent.size() == n && !binding.canClose(), "empty name is refused with a message box" );
		activate( "agriculture_review_cancel" );
		update( *c );
		// OK applies and closes after confirmation.
		field( "agriculture_name", "Farm A" );
		activate( "agriculture_ok" );
		check( port.sent[port.sent.size() - 2].id.value == "agriculture.set_basics" && port.sent.back().id.value == "agriculture.refresh", "OK applies" );
		farm.name = "Farm A";
		controller.showAgriculture( farm, Revision { 6 } );
		update( *c );
		check( port.sent.back().id.value == "nav.close", "OK closes once the snapshot confirms" );
		// Drafts survive object switches by designation ID.
		controller.showAgriculture( farm, Revision { 7 } );
		update( *c );
		field( "agriculture_name", "Kept" );
		auto otherFarm   = farm;
		otherFarm.target = { AgricultureKind::Farm, DesignationId { 32 } };
		otherFarm.name   = "Other";
		controller.showAgriculture( otherFarm, Revision { 1 } );
		check( controller.state().agriculture.draft.name == "Other", "other farm has its own draft" );
		controller.showAgriculture( farm, Revision { 8 } );
		check( controller.state().agriculture.draft.name == "Kept", "draft retained by designation ID" );
		controller.revertAgricultureDraft();
		// A rejected command reports through a message box and releases the sheet.
		update( *c );
		field( "agriculture_name", "Rejected" );
		port.reject = true;
		activate( "agriculture_apply" );
		port.reject = false;
		check( !controller.state().agriculture.draft.pending && !controller.state().agriculture.feedback.empty(), "rejection releases the sheet with a message" );
		activate( "agriculture_review_cancel" );
		controller.revertAgricultureDraft();

		// ---------------------------------------------------------------- Pasture
		AgricultureSnapshot pasture;
		pasture.target  = { AgricultureKind::Pasture, DesignationId { 41 } };
		pasture.name    = "Sheep run";
		pasture.product = CatalogId { "Sheep" };
		pasture.male = 1, pasture.female = 2, pasture.total = 3, pasture.capacity = 8, pasture.maxMale = 2, pasture.maxFemale = 4;
		pasture.catalog = { { CatalogId { "Sheep" }, "Sheep", 0, 0, 0 }, { CatalogId { "Goat" }, "Goat", 0, 0, 0 } };
		pasture.animals = { { CreatureId { 501 }, "Ram", CatalogId { "Sheep" }, Gender::Male, false, false }, { CreatureId { 502 }, "Ewe", CatalogId { "Sheep" }, Gender::Female, false, false }, { CreatureId { 503 }, "Lamb", CatalogId { "Sheep" }, Gender::Female, true, false } };
		pasture.foods   = { { CatalogId { "Hay" }, CatalogId { "Grass" }, "Hay", true }, { CatalogId { "Vegetable" }, CatalogId { "Carrot" }, "Carrot", false }, { CatalogId { "Vegetable" }, CatalogId { "Cabbage" }, "Cabbage", true } };
		controller.showAgriculture( pasture, Revision { 1 } );
		update( *c );
		check( controller.state().agriculture.pane == AgriculturePane::General, "pasture opens on General" );
		check( shown( "agriculture_view_animals" ) && shown( "agriculture_view_food" ) && !shown( "agriculture_view_plots" ) && !shown( "agriculture_view_crops" ), "pasture tabs" );
		check( !controller.state().agriculture.selectedAnimal, "no animal is chosen for the user" );
		controller.setAgriculturePane( AgriculturePane::Animals );
		update( *c );
		field( "agriculture_animal_type", "Goat" );
		check( controller.state().agriculture.draft.options.product == CatalogId { "Goat" } && disabled( "agriculture_male_cap" ) && el( "agriculture_pasture_summary" )->GetInnerRML().find( "Apply the new animal type" ) != Rml::String::npos, "a pending animal type locks the type-dependent limits" );
		n = port.sent.size();
		activate( "agriculture_apply" );
		check( port.sent.size() > n && port.sent[n].id.value == "agriculture.select_product" && std::get<SetAgricultureProductPayload>( port.sent[n].payload ).product == CatalogId { "Goat" }, "Apply sends the pending type while the limits are unavailable" );
		{
			auto goat      = pasture;
			goat.product   = CatalogId { "Goat" };
			goat.maxMale   = 5; // the game's defaults for the new type
			goat.maxFemale = 9;
			goat.foods.pop_back();
			controller.showAgriculture( goat, Revision { 2 } );
		}
		update( *c );
		check( !controller.state().agriculture.draft.dirty && !controller.state().agriculture.draft.pending && !disabled( "agriculture_male_cap" ), "a confirmed type change adopts that type's limits and foods" );
		pasture.product = CatalogId { "Sheep" };
		controller.showAgriculture( pasture, Revision { 3 } );
		update( *c );
		n = port.sent.size();
		activate( "agriculture_butcher_503" );
		check( port.sent.size() == n && controller.state().agriculture.draft.options.butcher == std::vector<std::uint32_t> { 503 }, "checking a non-first animal stages its butchering mark" );
		activate( "agriculture_male_cap-up" );
		check( controller.state().agriculture.draft.options.maxMale == 3 && port.sent.size() == n, "male limit spin box stages the value" );
		field( "agriculture_female_cap", "6" );
		key( "agriculture_female_cap", Rml::Input::KI_RETURN );
		check( controller.state().agriculture.draft.options.maxFemale == 6, "typed female limit stages the value" );
		controller.setAgriculturePane( AgriculturePane::Food );
		update( *c );
		{
			const auto carrot = std::string( "agriculture_food_" ) + []( std::string v ) { static const char* d = "0123456789abcdef"; std::string o; for ( unsigned char ch : v ) { o += d[ch >> 4]; o += d[ch & 15]; } return o; }( "Vegetable|Carrot" );
			activate( carrot.c_str() );
		}
		check( controller.state().agriculture.draft.options.foods.size() == 3 && controller.state().agriculture.selectedFood == std::string( "Vegetable|Carrot" ), "a non-first food rule stages by item and material" );
		// Reorder and remove neighbours before Apply: targets stay by ID.
		auto moved = pasture;
		std::reverse( moved.animals.begin(), moved.animals.end() );
		moved.animals.erase( moved.animals.begin() + 1 ); // Ewe removed
		std::reverse( moved.foods.begin(), moved.foods.end() );
		controller.showAgriculture( moved, Revision { 12 } );
		update( *c );
		n = port.sent.size();
		activate( "agriculture_apply" );
		{
			const auto b = std::find_if( port.sent.begin() + n, port.sent.end(), []( const auto& a ) { return a.id.value == "agriculture.set_butchering"; } );
			check( port.count( "agriculture.set_butchering" ) == 1 && b != port.sent.end() && std::get<SetButcheringPayload>( b->payload ).creature.value == 503 && std::get<SetButcheringPayload>( b->payload ).butcher, "butchering targets the reviewed animal after reorder" );
		}
		{
			bool male = false, female = false, carrot = false;
			for ( std::size_t i = n; i < port.sent.size(); ++i )
			{
				if ( auto* cap = std::get_if<SetPastureCapPayload>( &port.sent[i].payload ) ) ( cap->gender == Gender::Male ? male : female ) = ( cap->gender == Gender::Male ? cap->max == 3 : cap->max == 6 );
				if ( auto* food = std::get_if<SetPastureFoodPayload>( &port.sent[i].payload ) ) carrot = food->item == CatalogId { "Vegetable" } && food->material == CatalogId { "Carrot" } && food->allowed && food->pasture == pasture.target.designation;
			}
			check( male && female && carrot && port.sent.size() == n + 5 && port.sent.back().id.value == "agriculture.refresh", "Apply sends caps, one butchering mark and one food rule, then asks for the authoritative snapshot" );
		}
		// Live updates refresh untouched fields; the user's edit stays pending; a change elsewhere to an edited field conflicts.
		auto live = moved;
		for ( auto& a : live.animals ) a.butcher = a.id.value == 503;
		live.maxMale = 3, live.maxFemale = 6;
		live.foods[1].allowed = true; // Carrot now allowed (reversed order)
		for ( auto& f : live.foods ) if ( f.material == CatalogId { "Carrot" } ) f.allowed = true;
		controller.showAgriculture( live, Revision { 13 } );
		check( !controller.state().agriculture.draft.dirty && !controller.state().agriculture.draft.pending, "confirmed pasture snapshot clears the sheet" );
		update( *c );
		activate( "agriculture_male_cap-up" );
		check( controller.state().agriculture.draft.options.maxMale == 4, "new pending male limit" );
		auto grown = live;
		grown.maxFemale = 7;
		grown.foods.push_back( { CatalogId { "Vegetable" }, CatalogId { "Turnip" }, "Turnip", true } );
		controller.showAgriculture( grown, Revision { 14 } );
		check( controller.state().agriculture.draft.options.maxFemale == 7 && controller.state().agriculture.draft.options.maxMale == 4 && controller.state().agriculture.draft.options.foods.size() == 4, "live update refreshes untouched fields and keeps the user's edit" );
		n = port.sent.size();
		activate( "agriculture_apply" );
		check( port.sent.size() == n + 2 && std::get<SetPastureCapPayload>( port.sent[n].payload ).max == 4, "Apply after a live update sends only the user's change" );
		grown.maxMale = 4;
		controller.showAgriculture( grown, Revision { 15 } );
		update( *c );
		activate( "agriculture_male_cap-up" );
		auto elsewhere = grown;
		elsewhere.maxMale = 9;
		controller.showAgriculture( elsewhere, Revision { 16 } );
		n = port.sent.size();
		activate( "agriculture_apply" );
		check( port.sent.size() == n && controller.state().agriculture.feedback.find( "changed elsewhere" ) != std::string::npos, "an edited field changed elsewhere is not overwritten" );
		activate( "agriculture_review_cancel" );
		controller.revertAgricultureDraft();
		// Grove.
		AgricultureSnapshot grove;
		grove.target  = { AgricultureKind::Grove, DesignationId { 51 } };
		grove.name    = "Apple grove";
		grove.product = CatalogId { "Apple" };
		grove.catalog = { { CatalogId { "Apple" }, "Apple tree", 3, 4, 5 } };
		controller.showAgriculture( grove, Revision { 1 } );
		update( *c );
		check( shown( "agriculture_view_crops" ) && el( "agriculture_view_crops" )->GetInnerRML() == "Trees" && !shown( "agriculture_view_plots" ) && !shown( "agriculture_view_animals" ), "grove tabs" );
		check( shown( "agriculture_work_grove" ) && !shown( "agriculture_work_farm" ), "grove work rules" );
		n = port.sent.size();
		activate( "agriculture_toggle_fell" );
		activate( "agriculture_apply" );
		{
			const auto g = std::get<SetGroveOptionsPayload>( port.sent[n].payload );
			check( port.sent.size() == n + 2 && g.fell && !g.pick && !g.plant && g.grove == grove.target.designation, "Fell trees changes only that rule" );
		}

		// ---------------------------------------------------------------- Pages fit the fixed sheet
		// Long catalogs and food lists must scroll inside their list, never push the page past the sheet.
		for ( int i = 0; i < 40; ++i )
		{
			farm.catalog.push_back( { CatalogId { "Crop" + std::to_string( i ) }, "Crop " + std::to_string( i ), 1, 1, 1 } );
			pasture.foods.push_back( { CatalogId { "Food" }, CatalogId { "M" + std::to_string( i ) }, "Food " + std::to_string( i ), false } );
			grove.catalog.push_back( { CatalogId { "Tree" + std::to_string( i ) }, "Tree " + std::to_string( i ), 1, 1, 1 } );
		}
		for ( auto* snapshot : { &farm, &pasture, &grove } )
		{
			controller.showAgriculture( *snapshot, Revision { 50 } );
			for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
			{
				c->SetDensityIndependentPixelRatio( scale );
				const int k = std::max( 1, int( scale + 0.5f ) );
				c->SetDimensions( { 384 * k, 380 * k } );
				for ( auto pane : { AgriculturePane::General, AgriculturePane::Plots, AgriculturePane::PlotQueue, AgriculturePane::Crops, AgriculturePane::Animals, AgriculturePane::Food } )
				{
					if ( !agricultureSupportsPane( snapshot->target.kind, pane ) ) continue;
					controller.setAgriculturePane( pane );
					update( *c );
					auto* frame = el( "agriculture_scroll" );
					check( el( "agriculture_tabs" )->GetOffsetWidth() <= 384.f * k, "tabs fit the property sheet" );
					const char* paneIds[] = { "agriculture_general_pane", "agriculture_plots_pane", "agriculture_queue_pane", "agriculture_crops_pane", "agriculture_animals_pane", "agriculture_food_pane" };
					auto* page = el( paneIds[static_cast<int>( pane )] );
					auto* ok   = el( "agriculture_ok" );
					if ( page->GetScrollHeight() > page->GetClientHeight() + 1.f || ok->GetAbsoluteTop() + ok->GetOffsetHeight() > 380.f * k + 1.f )
					{
						std::cerr << "clipped kind=" << int( snapshot->target.kind ) << " pane=" << int( pane ) << " scale=" << scale << " page=" << page->GetScrollHeight() << "/" << page->GetClientHeight() << " okBottom=" << ok->GetAbsoluteTop() + ok->GetOffsetHeight() << '\n';
						check( false, "page content and commit buttons stay inside the sheet" );
					}
					if ( pane == AgriculturePane::Plots )
					{
						auto* cell = doc->GetElementById( "agriculture_plot_2_1_92" );
						auto* grid = el( "agriculture_plot_grid" );
						if ( !cell || cell->GetAbsoluteTop() + cell->GetOffsetHeight() > grid->GetAbsoluteTop() + grid->GetClientHeight() + 1.f )
							std::cerr << "grid client=" << grid->GetClientHeight() << " top=" << grid->GetAbsoluteTop() << " scrollTop=" << grid->GetScrollTop() << " cellTop=" << ( cell ? cell->GetAbsoluteTop() : -1.f ) << " cellH=" << ( cell ? cell->GetOffsetHeight() : -1.f ) << " scale=" << scale << std::endl;
						check( cell && cell->GetAbsoluteTop() + cell->GetOffsetHeight() <= grid->GetAbsoluteTop() + grid->GetClientHeight() + 1.f, "the plot grid shows its second row" );
					}
					if ( frame->GetScrollHeight() > frame->GetClientHeight() + 1.f || frame->GetScrollWidth() > frame->GetClientWidth() + 1.f )
					{
						std::cerr << "kind=" << int( snapshot->target.kind ) << " pane=" << int( pane ) << " scale=" << scale << " scroll=" << frame->GetScrollHeight() << " client=" << frame->GetClientHeight() << "\n";
						check( false, "property page fits without scrolling" );
					}
					++checks;
				}
			}
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 720, 720 } );
		controller.endWorld();
		check( controller.state().agriculture.draft.name.empty(), "world end clears drafts" );
		controller.removeViewPort( binding );
		binding.shutdown();
	}
	Rml::RemoveContext( "stage11" );
	Rml::Shutdown();
	std::cout << checks << " checks passed\n";
}
