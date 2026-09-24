/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6ARmlBinding.h"
#include "../InventoryTableSchema.h"
#include "../../localization/RmlText.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <tuple>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/StringUtilities.h>

namespace ingnomia::ui::management6a
{
namespace
{
std::string safe( const std::string& value )
{
	return Rml::StringUtilities::EncodeRml( value );
}
std::string displayCatalogLabel( const std::string& value )
{
	if ( value == "any" ) return "Any material";
	std::string result;
	for ( std::size_t i = 0; i < value.size(); ++i )
	{
		const auto ch = static_cast<unsigned char>( value[i] );
		if ( i && std::isupper( ch ) && ( std::islower( static_cast<unsigned char>( value[i-1] ) ) || ( i+1 < value.size() && std::islower( static_cast<unsigned char>( value[i+1] ) ) ) ) ) result += ' ';
		result += value[i] == '_' ? ' ' : value[i];
	}
	return result;
}
std::string foldedLabel( std::string value )
{
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	return value;
}
std::string filterOptionMarkup( std::vector<std::string> values, const std::vector<std::string>& selected, std::string_view idPrefix )
{
	std::ranges::stable_sort( values, []( const auto& left, const auto& right ) { return foldedLabel( left ) < foldedLabel( right ); } );
	values.erase( std::unique( values.begin(), values.end(), []( const auto& left, const auto& right ) { return foldedLabel( left ) == foldedLabel( right ); } ), values.end() );
	std::string out = "<button id='" + std::string( idPrefix ) + "_option_all' type='button' class='c-excel-filter-combo__option" + std::string( selected.empty() ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( selected.empty() ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value=''><span class='c-excel-filter-combo__check'>" + ( selected.empty() ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> All</button>";
	std::size_t optionIndex = 0;
	for ( const auto& value : values )
	{
		if ( value.empty() ) continue;
		const bool active = std::ranges::any_of( selected, [&]( const auto& candidate ) { return foldedLabel( candidate ) == foldedLabel( value ); } );
		out += "<button id='" + std::string( idPrefix ) + "_option_" + std::to_string( ++optionIndex ) + "' type='button' class='c-excel-filter-combo__option" + std::string( active ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( active ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value='" + safe( value ) + "'><span class='c-excel-filter-combo__check'>" + ( active ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> " + safe( value ) + "</button>";
	}
	return out;
}
std::string mode( CraftRepeatMode value )
{
	return value == CraftRepeatMode::Once ? "Number" : value == CraftRepeatMode::Maintain ? "Stock limit"
																						  : "Repeat";
}
std::string check( TriState value )
{
	return value == TriState::On ? "[x]" : value == TriState::Mixed ? "[-]" : "[ ]";
}
std::string filterStateLabel( TriState value )
{
	return value == TriState::On ? "Allowed" : value == TriState::Mixed ? "Mixed" : "Blocked";
}
std::string filterStateClass( TriState value )
{
	return value == TriState::On ? "allowed" : value == TriState::Mixed ? "mixed" : "blocked";
}
std::string filterFallbackGlyph( const StockpileFilterRow& row )
{
	if ( row.id.depth == FilterDepth::Category )
		return "C";
	if ( row.id.depth == FilterDepth::Group )
		return "G";
	char glyph = '?';
	for ( const unsigned char c : row.label )
		if ( std::isalnum( c ) )
		{
			glyph = static_cast<char>( std::toupper( c ) );
			break;
		}
	return std::string( 1, glyph );
}
std::string filterIcon( const StockpileFilterRow& row )
{
	if ( row.id.depth == FilterDepth::Category || row.id.depth == FilterDepth::Group )
		return {};
	if ( row.icon.sheet.empty() || row.icon.width <= 0 || row.icon.height <= 0 )
		return "<span class='c-m6a-filter-icon c-m6a-filter-icon--fallback' aria-hidden='true'><strong>" + filterFallbackGlyph( row ) + "</strong></span>";
	for ( const unsigned char c : row.icon.sheet )
		if ( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' ) )
			return "<span class='c-m6a-filter-icon c-m6a-filter-icon--fallback' aria-hidden='true'><strong>" + filterFallbackGlyph( row ) + "</strong></span>";
	constexpr double frame = 40.0;
	const double scale = std::min( frame / static_cast<double>( row.icon.width ), frame / static_cast<double>( row.icon.height ) );
	char style[128] {};
	std::snprintf( style, sizeof style, "width:%.2fdp;height:%.2fdp;left:%.2fdp;top:%.2fdp;", static_cast<double>( row.icon.width ) * scale, static_cast<double>( row.icon.height ) * scale, ( 40.0 - static_cast<double>( row.icon.width ) * scale ) * 0.5, ( 40.0 - static_cast<double>( row.icon.height ) * scale ) * 0.5 );
	return "<span class='c-m6a-filter-icon'><img class='c-m6a-filter-icon-image' src='/tilesheet/" + row.icon.sheet + "' style='" + style + "' /></span>";
}
std::string contentIcon( const StockpileContentRow& row )
{
	char glyph = '?';
	for ( const unsigned char c : row.name )
		if ( std::isalnum( c ) )
		{
			glyph = static_cast<char>( std::toupper( c ) );
			break;
		}
	const auto fallback = [&]
	{ return "<span class='c-m6a-content-icon c-m6a-content-icon--fallback' aria-hidden='true'><strong>" + std::string( 1, glyph ) + "</strong></span>"; };
	if ( row.icon.sheet.empty() || row.icon.width <= 0 || row.icon.height <= 0 )
		return fallback();
	for ( const unsigned char c : row.icon.sheet )
		if ( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' ) )
			return fallback();
	constexpr double frame = 40.0;
	const double scale = std::min( frame / static_cast<double>( row.icon.width ), frame / static_cast<double>( row.icon.height ) );
	char style[128] {};
	std::snprintf( style, sizeof style, "width:%.2fdp;height:%.2fdp;left:%.2fdp;top:%.2fdp;", static_cast<double>( row.icon.width ) * scale, static_cast<double>( row.icon.height ) * scale, ( 40.0 - static_cast<double>( row.icon.width ) * scale ) * 0.5, ( 40.0 - static_cast<double>( row.icon.height ) * scale ) * 0.5 );
	return "<span class='c-m6a-content-icon' aria-hidden='true'><img class='c-m6a-content-icon-image' src='/tilesheet/" + row.icon.sheet + "' style='" + style + "' /></span>";
}
struct StockpilePathLabels
{
	std::string category, group, item, material;
};
template<class RowId>
StockpilePathLabels stockpilePathLabels( const RowId& leaf, const std::vector<StockpileFilterRow>& rows )
{
	StockpilePathLabels labels;
	for ( const auto& row : rows )
	{
		if ( row.id.category != leaf.category ) continue;
		if ( row.id.depth == FilterDepth::Category ) labels.category = row.label;
		else if ( row.id.depth == FilterDepth::Group && row.id.group == leaf.group ) labels.group = row.label;
		else if ( row.id.depth == FilterDepth::Item && row.id.group == leaf.group && row.id.item == leaf.item ) labels.item = row.label;
		else if ( row.id.depth == FilterDepth::Material && row.id.group == leaf.group && row.id.item == leaf.item && row.id.material == leaf.material ) labels.material = row.label;
	}
	normalizeInventoryTableLabels( labels.group, labels.item, labels.material, leaf.item.value );
	return labels;
}
Rml::Element* dataElement( Rml::Element* target, Rml::Element* boundary, const char* attribute )
{
	for ( auto* element = target; element && element != boundary; element = element->GetParentNode() )
		if ( element->HasAttribute( attribute ) )
			return element;
	return nullptr;
}
std::string stockpileToggleText( std::string_view label, bool value )
{
	return std::string( label ) + ": " + ( value ? "On" : "Off" );
}
std::string toggleText( std::string_view label, bool value )
{
	return std::string( value ? "[x] " : "[ ] " ) + std::string( label );
}
std::string kind( AgricultureKind value )
{
	return value == AgricultureKind::Farm ? "Farm" : value == AgricultureKind::Pasture ? "Pasture"
																					   : "Grove";
}
std::string hex( std::string_view value )
{
	static constexpr char digits[] = "0123456789abcdef";
	std::string out;
	out.reserve( value.size() * 2 );
	for ( unsigned char c : value )
	{
		out.push_back( digits[c >> 4] );
		out.push_back( digits[c & 15] );
	}
	return out;
}
std::string rowId( std::string_view prefix, std::string_view value )
{
	return std::string( prefix ) + hex( value );
}
std::string farmPlotKey( WorldPosition plot )
{
	return std::to_string( plot.x ) + "," + std::to_string( plot.y ) + "," + std::to_string( plot.z );
}
std::optional<WorldPosition> farmPlotFromKey( const std::string& key )
{
	WorldPosition plot;
	char trailing{};
	if ( std::sscanf( key.c_str(), "%d,%d,%d%c", &plot.x, &plot.y, &plot.z, &trailing ) != 3 ) return std::nullopt;
	return plot;
}
std::string stockpileFilterRowId( const StockpileFilterRowId& row )
{
	return "m6a_filter_" + hex( row.category.value ) + "_" + hex( row.group.value ) + "_" + hex( row.item.value ) + "_" + hex( row.material.value ) + "_" + std::to_string( static_cast<int>( row.depth ) );
}
std::string attr( const Rml::Element* element, const char* name )
{
	return element ? element->GetAttribute<Rml::String>( name, "" ) : std::string {};
}
bool priorityDigit( Rml::Input::KeyIdentifier key )
{
	const auto value = static_cast<int>( key );
	return ( value >= static_cast<int>( Rml::Input::KI_0 ) && value <= static_cast<int>( Rml::Input::KI_9 ) ) ||
	       ( value >= static_cast<int>( Rml::Input::KI_NUMPAD0 ) && value <= static_cast<int>( Rml::Input::KI_NUMPAD9 ) );
}
bool priorityEditingKey( Rml::Input::KeyIdentifier key )
{
	return key == Rml::Input::KI_BACK || key == Rml::Input::KI_DELETE || key == Rml::Input::KI_LEFT || key == Rml::Input::KI_RIGHT ||
	       key == Rml::Input::KI_HOME || key == Rml::Input::KI_END || key == Rml::Input::KI_TAB || key == Rml::Input::KI_RETURN ||
	       key == Rml::Input::KI_ESCAPE;
}
bool priorityTextIsNumeric( const Rml::String& value )
{
	return std::all_of( value.begin(), value.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } );
}
} // namespace
void Management6ARmlBinding::Callback::ProcessEvent( Rml::Event& event )
{
	fn_( event );
}
Management6ARmlBinding::Management6ARmlBinding( Rml::Context& context ) :
	context_( context )
{
}
Management6ARmlBinding::~Management6ARmlBinding()
{
	shutdown();
}
bool Management6ARmlBinding::initialize( Management6AController& controller )
{
	controller_  = &controller;
	const auto load = [this]( const char* path )
		{ return documentLoader_ ? documentLoader_( path ) : context_.LoadDocument( path ); };
	workshop_    = load( "windows/workshop_manager.rml" );
	stockpile_   = load( "windows/stockpile_manager.rml" );
	agriculture_ = load( "panels/agriculture_manager.rml" );
	if ( !workshop_ || !stockpile_ || !agriculture_ )
	{
		shutdown();
		return false;
	}
	localization::applyRmlText( *workshop_, textCatalog_ );
	localization::applyRmlText( *stockpile_, textCatalog_ );
	localization::applyRmlText( *agriculture_, textCatalog_ );
	for ( const char* id : { "workshop_close", "stockpile_close", "agriculture_close" } )
		bindClick( id, [this]
				   { controller_->close(); if ( closeHandler_ ) closeHandler_(); } );
	for ( const char* id : { "workshop_locate", "stockpile_locate", "agriculture_locate" } )
		bindClick( id, [this]
				   { controller_->locate(); } );
	for ( const char* id : { "workshop_refresh", "stockpile_refresh", "agriculture_refresh" } )
		bindClick( id, [this]
				   { controller_->refresh(); } );
	for ( const char* id : { "workshop_sort", "stockpile_sort", "agriculture_sort" } )
		bindClick( id, [this]
				   { controller_->toggleSort(); } );
	auto rerender = [this]
	{ stateChanged( controller_->state() ); };
	bindClick( "workshop_page_previous", [this, rerender]
			   {workshopPage_=workshopPage_>=pageSize_?workshopPage_-pageSize_:0;rerender(); } );
	bindClick( "workshop_page_next", [this, rerender]
			   {workshopPage_+=pageSize_;rerender(); } );
	bindClick( "agriculture_page_previous", [this, rerender]
			   {agriculturePage_=agriculturePage_>=pageSize_?agriculturePage_-pageSize_:0;rerender(); } );
	bindClick( "agriculture_page_next", [this, rerender]
			   {agriculturePage_+=pageSize_;rerender(); } );
	for ( const char* id : { "workshop_search", "agriculture_search" } )
	{
		auto search = [this, id]( Rml::Event& )
		{
			if ( std::string_view( id ) == "workshop_search" )
				workshopPage_ = 0;
			else
				agriculturePage_ = 0;
			controller_->setSearch( formValue( id ) );
		};
		bind( id, "input", search );
		bind( id, "change", search );
	}
	const std::array contentFilters {
		"stockpile_content_filter_category", "stockpile_content_filter_group", "stockpile_content_filter_item",
		"stockpile_content_filter_material", "stockpile_content_filter_stock", "stockpile_content_filter_total"
	};
	const std::array allowFilters {
		"stockpile_allow_filter_category", "stockpile_allow_filter_group", "stockpile_allow_filter_item",
		"stockpile_allow_filter_material", "stockpile_allow_filter_status"
	};
	const std::array contentFilterToggles {
		"stockpile_content_filter_category_toggle", "stockpile_content_filter_group_toggle", "stockpile_content_filter_item_toggle",
		"stockpile_content_filter_material_toggle", "stockpile_content_filter_stock_toggle", "stockpile_content_filter_total_toggle"
	};
	const std::array contentFilterOptions {
		"stockpile_content_filter_category_options", "stockpile_content_filter_group_options", "stockpile_content_filter_item_options",
		"stockpile_content_filter_material_options", "stockpile_content_filter_stock_options", "stockpile_content_filter_total_options"
	};
	const std::array allowFilterToggles {
		"stockpile_allow_filter_category_toggle", "stockpile_allow_filter_group_toggle", "stockpile_allow_filter_item_toggle",
		"stockpile_allow_filter_material_toggle", "stockpile_allow_filter_status_toggle"
	};
	const std::array allowFilterOptions {
		"stockpile_allow_filter_category_options", "stockpile_allow_filter_group_options", "stockpile_allow_filter_item_options",
		"stockpile_allow_filter_material_options", "stockpile_allow_filter_status_options"
	};
	const auto bindColumnFilters = [this]( const auto& ids, bool allowList )
	{
		for ( std::size_t column = 0; column < ids.size(); ++column )
		{
			const auto* id = ids[column];
			auto apply = [this, id, column, allowList]( Rml::Event& event, bool restoreFocus )
			{
				if ( !allowList && column >= 4 ) return;
				if ( allowList ) if ( auto* list = element( "stockpile_filters" ) ) list->SetScrollTop( 0.f );
				auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() );
				controller_->setStockpileColumnFilter( allowList, column, control ? control->GetValue() : std::string {} );
				if ( restoreFocus )
					if ( auto* input = element( id ); input && context_.GetFocusElement() != input ) input->Focus();
			};
			bind( id, "input", [apply]( Rml::Event& event ) { apply( event, true ); } );
			bind( id, "change", [apply]( Rml::Event& event ) { apply( event, false ); } );
		}
	};
	bindColumnFilters( contentFilters, false );
	bindColumnFilters( allowFilters, true );
	const auto bindFilterCombos = [this]( const auto& toggles, const auto& options, bool allowList )
	{
		for ( std::size_t column = 0; column < toggles.size(); ++column )
		{
			bindClick( toggles[column], [this, column, allowList]
			{
				auto& menu = allowList ? stockpileAllowFilterMenuColumn_ : stockpileContentFilterMenuColumn_;
				menu = menu && *menu == column ? std::nullopt : std::optional<std::size_t> { column };
				renderStockpile( controller_->state().stockpile );
			} );
			bind( options[column], "click", [this, column, allowList]( Rml::Event& event )
			{
				if ( auto* option = dataElement( event.GetTargetElement(), event.GetCurrentElement(), "data-filter-value" ) )
				{
					const auto value = attr( option, "data-filter-value" );
					if ( allowList ) if ( auto* list = element( "stockpile_filters" ) ) list->SetScrollTop( 0.f );
						controller_->toggleStockpileColumnSelection( allowList, column, value );
					event.StopPropagation();
				}
			} );
		}
	};
	bindFilterCombos( contentFilterToggles, contentFilterOptions, false );
	bindFilterCombos( allowFilterToggles, allowFilterOptions, true );
	for ( const auto& binding : std::array {
		std::pair { "stockpile_content_sort_category", StockpileSortKey::Category },
		std::pair { "stockpile_content_sort_group", StockpileSortKey::Group },
		std::pair { "stockpile_content_sort_item", StockpileSortKey::Item },
		std::pair { "stockpile_content_sort_material", StockpileSortKey::Material },
		std::pair { "stockpile_content_sort_stock", StockpileSortKey::Quantity },
		std::pair { "stockpile_content_sort_total", StockpileSortKey::Total }
	} )
		bindClick( binding.first, [this, key = binding.second] { controller_->setStockpileContentSort( key ); } );
	for ( const auto& binding : std::array {
		std::pair { "stockpile_allow_sort_category", StockpileSortKey::Category },
		std::pair { "stockpile_allow_sort_group", StockpileSortKey::Group },
		std::pair { "stockpile_allow_sort_item", StockpileSortKey::Item },
		std::pair { "stockpile_allow_sort_material", StockpileSortKey::Material },
		std::pair { "stockpile_allow_sort_status", StockpileSortKey::Status }
	} )
		bindClick( binding.first, [this, key = binding.second] { controller_->setStockpileAllowSort( key ); } );
	const auto commitStockpileBasics = [this]
	{
		if ( normalizingPriority_ )
			return;
		const auto& s = controller_->state().stockpile.value;
		const auto maximum = std::max( 1, s.maxPriority );
		controller_->setStockpileBasics( formValue( "stockpile_name" ), normalizePriority( "stockpile_priority", s.priority + 1, maximum ) - 1, s.suspended, s.pullFromOthers, s.allowPullFromHere );
	};
	bind( "stockpile_name", "change", [commitStockpileBasics]( Rml::Event& ) { commitStockpileBasics(); } );
	bind( "stockpile_priority", "change", [commitStockpileBasics]( Rml::Event& ) { commitStockpileBasics(); } );
	const auto adjustStockpilePriority = [this]( int delta )
	{
		const auto& s = controller_->state().stockpile.value;
		const auto maximum = std::max( 1, s.maxPriority );
		const auto current = normalizePriority( "stockpile_priority", s.priority + 1, maximum );
		const auto next = std::clamp( current + delta, 1, maximum );
		controller_->setStockpileBasics( formValue( "stockpile_name" ), next - 1, s.suspended, s.pullFromOthers, s.allowPullFromHere );
		if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_priority" ) ) )
			input->SetValue( std::to_string( next ) );
	};
	bindClick( "stockpile_priority_up", [adjustStockpilePriority] { adjustStockpilePriority( -1 ); } );
	bindClick( "stockpile_priority_down", [adjustStockpilePriority] { adjustStockpilePriority( 1 ); } );
	bind( "stockpile_name", "keydown", [this, commitStockpileBasics]( Rml::Event& e )
		  { const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) ); if ( key == Rml::Input::KI_RETURN ) { commitStockpileBasics(); e.StopPropagation(); } else if ( key == Rml::Input::KI_ESCAPE ) { if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_name" ) ) ) input->SetValue( controller_->state().stockpile.value.name ); e.StopPropagation(); } } );
	bind( "stockpile_priority", "keydown", [this, commitStockpileBasics]( Rml::Event& e )
		  { const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) ); if ( key == Rml::Input::KI_RETURN ) { commitStockpileBasics(); e.StopPropagation(); } else if ( key == Rml::Input::KI_ESCAPE ) { if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_priority" ) ) ) input->SetValue( std::to_string( std::clamp( std::max( 0, controller_->state().stockpile.value.priority ) + 1, 1, std::max( 1, controller_->state().stockpile.value.maxPriority ) ) ) ); e.StopPropagation(); } } );
	bind( "stockpile_manager_root", "keydown", [this]( Rml::Event& e )
		  {
			if ( e.GetTargetElement() != element( "stockpile_priority" ) )
				return;
			const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
			const bool modifier = e.GetParameter<int>( "ctrl_key", 0 ) != 0 || e.GetParameter<int>( "meta_key", 0 ) != 0;
			const bool clipboardEdit = modifier && !e.GetParameter<int>( "alt_key", 0 ) && ( key == Rml::Input::KI_A || key == Rml::Input::KI_C || key == Rml::Input::KI_V || key == Rml::Input::KI_X );
			if ( !priorityDigit( key ) && !priorityEditingKey( key ) && !clipboardEdit )
				e.StopPropagation();
		}, true );
	bind( "stockpile_manager_root", "textinput", [this]( Rml::Event& e )
		  {
			if ( e.GetTargetElement() != element( "stockpile_priority" ) )
				return;
			const auto value = e.GetParameter<Rml::String>( "text", "" );
			if ( !priorityTextIsNumeric( value ) )
				e.StopPropagation();
		}, true );
	bindClick( "stockpile_view_contents", [this] { controller_->setStockpilePane( StockpilePane::Contents ); } );
	bindClick( "stockpile_view_allow", [this] { controller_->setStockpilePane( StockpilePane::AllowList ); } );
	bindClick( "stockpile_view_settings", [this] { controller_->setStockpilePane( StockpilePane::Settings ); } );
	bindClick( "stockpile_restore_filter_search", [this] { controller_->restoreStockpileFilterSearch(); } );
	for ( const auto& [id, pane] : std::array {
		std::pair { "workshop_view_craft", WorkshopPane::Craft }, std::pair { "workshop_view_queue", WorkshopPane::Queue },
		std::pair { "workshop_view_settings", WorkshopPane::Settings }, std::pair { "workshop_view_trade", WorkshopPane::Trade } } )
		bindClick( id, [this, pane] { controller_->setWorkshopPane( pane ); } );
	auto orderCountChanged = [this]( Rml::Event& ) {
		if ( !renderingWorkshop_ ) controller_->setWorkshopOrderCount( static_cast<std::uint32_t>( priority( "workshop_order_count", 1, 999 ) ) );
	};
	bind( "workshop_order_count", "input", orderCountChanged );
	bind( "workshop_order_count", "change", orderCountChanged );
	bind( "workshop_product_selection", "change", [this]( Rml::Event& e ) {
		if ( renderingWorkshop_ ) return;
		auto* select = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-material-index" );
		if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( select ) )
			controller_->setWorkshopOrderMaterial( static_cast<std::size_t>( std::stoul( attr( select, "data-material-index" ) ) ), CatalogId { control->GetValue() } );
	} );
	bind( "workshop_products", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-catalog");const auto key=attr(target,"data-catalog");if(!key.empty())controller_->selectWorkshopProduct(CatalogId{key}); } );
	bind( "workshop_queue", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-job");const auto key=attr(target,"data-job");if(!key.empty())controller_->selectWorkshopJob({static_cast<std::uint32_t>(std::stoul(key))}); } );
	bind( "workshop_product_selection", "click", [this]( Rml::Event& e )
		  {
			  if ( auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-order-mode" ) )
			  {
				  const auto value = attr( target, "data-order-mode" );
				  controller_->setWorkshopOrderMode( value == "maintain" ? CraftRepeatMode::Maintain : value == "repeat" ? CraftRepeatMode::Repeat : CraftRepeatMode::Once );
				  e.StopPropagation();
				  return;
			  }

		  } );
	bind( "workshop_job_editor", "click", [this]( Rml::Event& e )
		  {
			  auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-job-action" );
			  if ( !target ) return;
			  const auto action = attr( target, "data-job-action" );
			  const auto& state = controller_->state().workshop;
			  if ( !state.selectedJob ) return;
			  const auto row = std::find_if( state.value.queue.begin(), state.value.queue.end(), [&]( const auto& value ) { return value.id == *state.selectedJob; } );
			  if ( row == state.value.queue.end() ) return;
			  if ( action == "mode-once" )
				  controller_->setSelectedJob( CraftRepeatMode::Once, static_cast<std::uint32_t>( priority( "workshop_job_count", row->count, 999 ) ), row->suspended, row->moveBack );
			  else if ( action == "mode-maintain" )
				  controller_->setSelectedJob( CraftRepeatMode::Maintain, static_cast<std::uint32_t>( priority( "workshop_job_count", row->count, 999 ) ), row->suspended, row->moveBack );
			  else if ( action == "mode-repeat" )
				  controller_->setSelectedJob( CraftRepeatMode::Repeat, static_cast<std::uint32_t>( priority( "workshop_job_count", row->count, 999 ) ), row->suspended, row->moveBack );
			  else if ( action == "apply-count" )
				  controller_->setSelectedJob( row->mode, static_cast<std::uint32_t>( priority( "workshop_job_count", static_cast<std::int32_t>( row->count ), 999 ) ), row->suspended, row->moveBack );
			  else if ( action == "toggle-suspended" )
				  controller_->setSelectedJob( row->mode, row->count, !row->suspended, row->moveBack );
			  else if ( action == "toggle-move-back" )
				  controller_->setSelectedJob( row->mode, row->count, row->suspended, !row->moveBack );
			  else if ( action == "front" ) controller_->moveSelectedJob( MoveDirection::Front );
			  else if ( action == "up" ) controller_->moveSelectedJob( MoveDirection::Up );
			  else if ( action == "down" ) controller_->moveSelectedJob( MoveDirection::Down );
			  else if ( action == "back" ) controller_->moveSelectedJob( MoveDirection::Back );
			  else if ( action == "cancel" ) controller_->cancelSelectedJob();
			  e.StopPropagation();
		  } );
	bind( "workshop_trade_rows", "click", [this]( Rml::Event& e )
		  {auto*target=e.GetTargetElement();const auto item=attr(target,"data-item"),material=attr(target,"data-material"),party=attr(target,"data-party"),quality=attr(target,"data-quality");if(!item.empty())controller_->selectTradeRow({party=="trader"?TradeParty::Trader:TradeParty::Player,CatalogId{item},CatalogId{material},static_cast<std::uint8_t>(std::stoul(quality))}); } );
	bind( "stockpile_filters", "click", [this]( Rml::Event& e )
			  {
				  auto* target = e.GetTargetElement();
				  auto* rule = dataElement( target, e.GetCurrentElement(), "data-rule" );
				  auto* row = dataElement( target, e.GetCurrentElement(), "data-category" );
				  if ( rule )
				  {
					  const auto category = attr( row, "data-category" );
					  if ( !category.empty() )
					  {
						  controller_->selectStockpileFilter( { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( row, "data-group" ) }, CatalogId { attr( row, "data-item" ) }, CatalogId { attr( row, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( row, "data-depth" ) ) ) } );
						  controller_->toggleSelectedStockpileFilter();
					  }
					  e.StopPropagation();
					  return;
				  }
				  const auto category = attr( row, "data-category" );
				  if ( category.empty() ) return;
				  controller_->selectStockpileFilter( { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( row, "data-group" ) }, CatalogId { attr( row, "data-item" ) }, CatalogId { attr( row, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( row, "data-depth" ) ) ) } );
			  } );
	bind( "stockpile_filters", "keydown", [this]( Rml::Event& e )
			  {
				  const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
				  const auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-category" );
				  const auto category = attr( target, "data-category" );
				  if ( !target || category.empty() )
					  return;
				  const StockpileFilterRowId id { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( target, "data-group" ) }, CatalogId { attr( target, "data-item" ) }, CatalogId { attr( target, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( target, "data-depth" ) ) ) };
				  controller_->selectStockpileFilter( id );
				  if ( key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN || key == Rml::Input::KI_HOME || key == Rml::Input::KI_END )
				  {
					  const auto delta = key == Rml::Input::KI_UP ? -1 : key == Rml::Input::KI_DOWN ? 1 : key == Rml::Input::KI_HOME ? -2147483647 : 2147483647;
					  controller_->moveStockpileFilterSelection( delta );
				  }
				  else if ( key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE )
					  controller_->toggleSelectedStockpileFilter();
				  else
					  return;
				  e.StopPropagation();
				  if ( controller_->state().stockpile.selectedFilter )
					  if ( auto* row = element( stockpileFilterRowId( *controller_->state().stockpile.selectedFilter ).c_str() ) )
						  row->Focus();
			  } );
	bind( "stockpile_rows", "click", [this]( Rml::Event& e )
		  {auto*t=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-depth");if(t){controller_->selectStockpileContent({CatalogId{attr(t,"data-category")},CatalogId{attr(t,"data-group")},CatalogId{attr(t,"data-item")},CatalogId{attr(t,"data-material")},static_cast<FilterDepth>(std::stoul(attr(t,"data-depth")))});e.StopPropagation();} } );
	bind( "stockpile_template_name", "input", [this]( Rml::Event& ) { controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) ); } );
	bind( "stockpile_template_name", "change", [this]( Rml::Event& ) { controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) ); } );
	bind( "stockpile_template_name", "keydown", [this]( Rml::Event& e )
		  {
			  const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
			  if ( key == Rml::Input::KI_DOWN ) controller_->toggleStockpileTemplateMenu();
			  else if ( key == Rml::Input::KI_ESCAPE && controller_->state().stockpile.templateMenuOpen ) controller_->toggleStockpileTemplateMenu();
			  else if ( key == Rml::Input::KI_RETURN ) { controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) ); controller_->saveStockpileTemplate(); }
			  else return;
			  e.StopPropagation();
		  } );
	bindClick( "stockpile_template_toggle", [this] { controller_->toggleStockpileTemplateMenu(); } );
	bindClick( "stockpile_template_save", [this] { controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) ); controller_->saveStockpileTemplate(); } );
	bind( "stockpile_template_options", "click", [this]( Rml::Event& e )
		  { if ( auto* option = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-template" ) ) { controller_->selectStockpileTemplate( attr( option, "data-template" ) ); e.StopPropagation(); } } );
	bindClick( "stockpile_template_overwrite_confirm", [this] { controller_->confirmStockpileTemplateOverwrite(); } );
	bindClick( "stockpile_template_overwrite_cancel", [this] { controller_->cancelStockpileTemplateOverwrite(); } );
	bind( "agriculture_products", "click", [this]( Rml::Event& e )
		  {if(auto* row=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-catalog"))controller_->selectAgricultureProduct(CatalogId{attr(row,"data-catalog")}); } );
	bind( "agriculture_animals", "click", [this]( Rml::Event& e )
		  {const auto key=attr(e.GetTargetElement(),"data-animal");if(!key.empty())controller_->selectAgricultureAnimal({static_cast<std::uint32_t>(std::stoul(key))}); } );

    const auto commitWorkshopBasics=[this] {
        if(renderingWorkshop_ || normalizingPriority_) return;
        const auto& s=controller_->state().workshop.value;
        controller_->setWorkshopBasics(formValue("workshop_name"),normalizePriority("workshop_priority",s.priority+1,s.maxPriority)-1,s.suspended,s.acceptGenerated,s.autoCraftMissing);
    };
    bind("workshop_priority","change",[commitWorkshopBasics](Rml::Event&) { commitWorkshopBasics(); });
    bind("workshop_priority","keydown",[commitWorkshopBasics](Rml::Event& e) {
        if(e.GetParameter<int>("key_identifier",0)==Rml::Input::KI_RETURN) { commitWorkshopBasics(); e.StopPropagation(); }
    });
    for(const auto& [id,delta] : std::array{std::pair{"workshop_priority_up",-1},std::pair{"workshop_priority_down",1}})
        bindClick(id,[this,delta] {
            const auto& s=controller_->state().workshop.value;
            const int next=std::clamp(priority("workshop_priority",s.priority+1,s.maxPriority)+delta,1,std::max(1,s.maxPriority));
            if(auto* input=rmlui_dynamic_cast<Rml::ElementFormControl*>(element("workshop_priority"))) {
                normalizingPriority_=true; input->SetValue(std::to_string(next)); normalizingPriority_=false;
            }
            controller_->setWorkshopBasics(formValue("workshop_name"),next-1,s.suspended,s.acceptGenerated,s.autoCraftMissing);
        });
    bindClick("workshop_apply_basics",commitWorkshopBasics);
    bindClick("workshop_link_add",[this] {
        const auto value=formValue("workshop_stockpile_choice");
        unsigned int id{}; const auto parsed=std::from_chars(value.data(),value.data()+value.size(),id);
        if(parsed.ec==std::errc{} && id) controller_->setWorkshopStockpileLink(StockpileId{id},true);
    });
    bind("workshop_linked_stockpiles","click",[this](Rml::Event& e) {
        if(auto* row=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-unlink")) {
            const auto value=attr(row,"data-unlink"); unsigned int id{};
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),id);
            if(parsed.ec==std::errc{} && id) controller_->setWorkshopStockpileLink(StockpileId{id},false);
        }
    });
	bindClick( "workshop_toggle_suspended", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,!s.suspended,s.acceptGenerated,s.autoCraftMissing); } );
	bind( "workshop_toggle_generated", "change", [this](Rml::Event&) { if(syncingCheckbox_) return; const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,s.suspended,element("workshop_toggle_generated")->HasAttribute("checked"),s.autoCraftMissing); } );
	bind( "workshop_toggle_auto_missing", "change", [this](Rml::Event&) { if(syncingCheckbox_) return; const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,s.suspended,s.acceptGenerated,element("workshop_toggle_auto_missing")->HasAttribute("checked")); } );
	bindClick( "workshop_next_product", [this]
			   { controller_->nextWorkshopProduct(); } );
	bindClick( "workshop_queue_once", [this]
			   {const auto&s=controller_->state().workshop;controller_->setWorkshopOrderCount(static_cast<std::uint32_t>(priority("workshop_order_count",static_cast<std::int32_t>(s.orderCount),999)));controller_->queueSelectedCraftOrder(); } );
	bindClick( "workshop_next_job", [this]
			   { controller_->nextWorkshopJob(); } );
	bindClick( "workshop_toggle_job", [this]
			   {const auto&s=controller_->state().workshop;if(!s.selectedJob)return;auto i=std::find_if(s.value.queue.begin(),s.value.queue.end(),[&](const auto&r){return r.id==*s.selectedJob;});if(i!=s.value.queue.end())controller_->setSelectedJob(i->mode,i->count,!i->suspended,i->moveBack); } );
	bindClick( "workshop_move_up", [this]
			   { controller_->moveSelectedJob( MoveDirection::Up ); } );
	bindClick( "workshop_move_down", [this]
			   { controller_->moveSelectedJob( MoveDirection::Down ); } );
	bindClick( "workshop_cancel_job", [this]
			   { controller_->cancelSelectedJob(); } );
	bindClick( "workshop_toggle_corpses", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setButcherOptions(!s.butcherCorpses,s.butcherExcess); } );
	bindClick( "workshop_toggle_excess", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setButcherOptions(s.butcherCorpses,!s.butcherExcess); } );
	bindClick( "workshop_toggle_catch", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setFisherOptions(!s.catchFish,s.processFish); } );
	bindClick( "workshop_toggle_process", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setFisherOptions(s.catchFish,!s.processFish); } );
	bindClick( "workshop_trade_refresh", [this]
			   { controller_->refreshTrade(); } );
	bindClick( "workshop_next_trade", [this]
			   { controller_->nextTradeRow(); } );
	bindClick( "workshop_trade_less", [this]
			   { controller_->adjustSelectedTradeOffer( -1 ); } );
	bindClick( "workshop_trade_more", [this]
			   { controller_->adjustSelectedTradeOffer( 1 ); } );
	bindClick( "workshop_trade_execute", [this]
			   { controller_->executeTrade(); } );
	bindClick( "workshop_trade_confirm", [this]
			   { controller_->confirmTrade(); } );
	bindClick( "workshop_trade_cancel", [this]
			   { controller_->cancelTrade(); } );

	bindClick( "stockpile_apply_basics", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(formValue("stockpile_name"),priority("stockpile_priority",s.priority+1,s.maxPriority)-1,s.suspended,s.pullFromOthers,s.allowPullFromHere); } );
	bindClick( "stockpile_toggle_suspended", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,!s.suspended,s.pullFromOthers,s.allowPullFromHere); } );
	bind( "stockpile_toggle_pull", "change", [this]( Rml::Event& )
			   {if(syncingCheckbox_)return;const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,s.suspended,element("stockpile_toggle_pull")->HasAttribute("checked"),s.allowPullFromHere); } );
	bind( "stockpile_toggle_allow_pull", "change", [this]( Rml::Event& )
			   {if(syncingCheckbox_)return;const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,s.suspended,s.pullFromOthers,element("stockpile_toggle_allow_pull")->HasAttribute("checked")); } );
	bindStockpileTooltip( "stockpile_toggle_suspended",
							 [this] { return textCatalog_.format( LocalizationKey { controller_->state().stockpile.value.suspended ? "management.stockpile.stockpile_resume_tooltip" : "management.stockpile.stockpile_suspend_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_toggle_pull",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_pull_from_others_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_toggle_allow_pull",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_allow_pull_from_here_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_priority_up",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_priority_up_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_priority_down",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_priority_down_tooltip" } ); } );
	bindClick( "stockpile_allow_bulk", [this] { controller_->setStockpileFilterMatches( true ); } );
	bindClick( "stockpile_block_bulk", [this] { controller_->setStockpileFilterMatches( false ); } );
	bind( "stockpile_filters", "scroll", [this]( Rml::Event& ) { renderStockpileFilterViewport(); } );
	bindClick( "stockpile_next_filter", [this]
			   { controller_->nextStockpileFilter(); } );
	bindClick( "stockpile_toggle_filter", [this]
			   { controller_->toggleSelectedStockpileFilter(); } );
	bindClick( "stockpile_next_content", [this]
			   { controller_->nextStockpileContent(); } );

	bindClick( "agriculture_apply_basics", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setAgricultureBasics(formValue("agriculture_name"),priority("agriculture_priority",s.priority,s.maxPriority),s.suspended); } );
	bindClick( "agriculture_view_overview", [this] { controller_->setAgriculturePane( AgriculturePane::Overview ); } );
	bindClick( "agriculture_view_products", [this] { controller_->setAgriculturePane( AgriculturePane::Products ); } );
	bindClick( "agriculture_view_settings", [this] { controller_->setAgriculturePane( AgriculturePane::Settings ); } );
	bindClick( "agriculture_view_work", [this] { controller_->setAgriculturePane( AgriculturePane::Work ); } );
	bind( "agriculture_plot_grid", "click", [this]( Rml::Event& e ) {
		if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-plot" ) )
			if ( auto plot = farmPlotFromKey( attr( row, "data-plot" ) ) ) controller_->toggleFarmPlot( *plot );
	} );
	bindClick( "agriculture_plot_select_all", [this] { controller_->selectAllFarmPlots(); } );
	bindClick( "agriculture_plot_clear", [this] { controller_->clearFarmPlotSelection(); } );
	bind( "agriculture_farm_catalog", "click", [this]( Rml::Event& e ) {
		if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-catalog" ) )
			controller_->selectAgricultureProduct( CatalogId { attr( row, "data-catalog" ) } );
	} );
	for ( const char* event : { "input", "change" } )
		bind( "agriculture_farm_search", event, [this]( Rml::Event& ) { agriculturePage_ = 0; controller_->setSearch( formValue( "agriculture_farm_search" ) ); } );
	bindClick( "agriculture_plot_assign", [this] { controller_->assignSelectedFarmPlotCrop(); } );
	bindClick( "agriculture_plot_default", [this] { controller_->useFarmDefaultForSelectedPlots(); } );
	bindClick( "agriculture_farm_default_crop", [this] { controller_->applySelectedAgricultureProduct(); } );
	bindClick( "agriculture_plot_queue", [this] {
		const auto value = formValue( "agriculture_plot_count" );
		try { controller_->queueSelectedFarmPlotCrop( static_cast<std::uint32_t>( std::clamp( std::stoi( value ), 1, 9999 ) ), false ); }
		catch ( const std::exception& ) { controller_->queueSelectedFarmPlotCrop( 1, false ); }
	} );
	bindClick( "agriculture_plot_repeat", [this] { controller_->queueSelectedFarmPlotCrop( 1, true ); } );
	bind( "agriculture_plot_orders", "click", [this]( Rml::Event& e ) {
		auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-order" );
		if ( !row ) return;
		const auto plot = farmPlotFromKey( attr( row, "data-plot" ) );
		if ( !plot ) return;
		const auto order = static_cast<std::uint32_t>( std::stoul( attr( row, "data-order" ) ) );
		const auto action = attr( row, "data-order-action" );
		if ( action == "cancel" ) controller_->cancelFarmPlotOrder( *plot, order );
		else if ( action == "up" || action == "down" ) controller_->moveFarmPlotOrder( *plot, order, action == "up" ? MoveDirection::Up : MoveDirection::Down );
	} );
	bindClick( "agriculture_toggle_suspended", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setAgricultureBasics(s.name,s.priority,!s.suspended); } );
	bindClick( "agriculture_next_product", [this]
			   { controller_->nextAgricultureProduct(); } );
	bindClick( "agriculture_apply_product", [this]
			   { controller_->applySelectedAgricultureProduct(); } );
	bindClick( "agriculture_toggle_harvest", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(!s.harvest,s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_pasture_harvest", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(!s.harvest,s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_hay", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(s.harvest,!s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_tame", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(s.harvest,s.harvestHay,!s.tame); } );
	bindClick( "agriculture_toggle_pick", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(!s.pick,s.plant,s.fell); } );
	bindClick( "agriculture_toggle_plant", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(s.pick,!s.plant,s.fell); } );
	bindClick( "agriculture_toggle_fell", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(s.pick,s.plant,!s.fell); } );
	bindClick( "agriculture_next_animal", [this]
			   { controller_->nextAgricultureAnimal(); } );
	bindClick( "agriculture_toggle_butcher", [this]
			   { controller_->toggleSelectedAnimalButchering(); } );
	bindClick( "agriculture_male_cap_down", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Male,static_cast<std::uint32_t>(std::max(0,s.maxMale-1))); } );
	bindClick( "agriculture_male_cap_up", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Male,static_cast<std::uint32_t>(s.maxMale+1)); } );
	bindClick( "agriculture_female_cap_down", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Female,static_cast<std::uint32_t>(std::max(0,s.maxFemale-1))); } );
	bindClick( "agriculture_female_cap_up", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Female,static_cast<std::uint32_t>(s.maxFemale+1)); } );
	bindClick( "agriculture_toggle_first_food", [this]
			   {const auto&s=controller_->state().agriculture.value;if(!s.foods.empty())controller_->setPastureFood(s.foods.front().item,s.foods.front().material,!s.foods.front().allowed); } );
	stateChanged( controller.state() );
	return true;
}
bool Management6ARmlBinding::reloadDocuments()
{
	auto* controller = controller_;
	if ( !controller ) return false;
	shutdown();
	return initialize( *controller );
}
void Management6ARmlBinding::shutdown()
{
	for ( auto& l : listeners_ )
		if ( l.target )
			l.target->RemoveEventListener( l.event, l.callback.get(), l.capture );
	listeners_.clear();
	stockpileContentFilterMenuColumn_.reset();
	stockpileAllowFilterMenuColumn_.reset();
	stockpileCategoryMarkup_.clear();
	workshopMarkup_.clear();
	workshopSettingsId_ = {};
	workshopEditingJob_.reset();
	tradeConfirmationVisible_ = false;
	stockpileTemplateConfirmationVisible_ = false;
	for ( auto** doc : { &agriculture_, &stockpile_, &workshop_ } )
		if ( *doc )
		{
			context_.UnloadDocument( *doc );
			*doc = nullptr;
		}
	controller_ = nullptr;
}
Rml::Element* Management6ARmlBinding::element( const char* id ) const
{
	for ( auto* doc : { workshop_, stockpile_, agriculture_ } )
		if ( doc )
			if ( auto* e = doc->GetElementById( id ) )
				return e;
	return nullptr;
}
void Management6ARmlBinding::bind( const char* id, const char* event, std::function<void( Rml::Event& )> fn, bool capture )
{
	if ( auto* e = element( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get(), capture );
		listeners_.push_back( { e, event, std::move( cb ), capture } );
	}
}
void Management6ARmlBinding::bindStockpileTooltip( const char* id, std::function<std::string()> textValue )
{
	if ( !stockpile_ ) return;
	if ( auto* target = stockpile_->GetElementById( id ) )
	{
		auto show = std::make_unique<Callback>( [this, textValue]( Rml::Event& event ) { showStockpileTooltip( textValue(), event.GetCurrentElement() ); } );
		target->AddEventListener( "mouseover", show.get() );
		listeners_.push_back( { target, "mouseover", std::move( show ) } );
		auto focus = std::make_unique<Callback>( [this, textValue]( Rml::Event& event ) { showStockpileTooltip( textValue(), event.GetCurrentElement() ); } );
		target->AddEventListener( "focus", focus.get() );
		listeners_.push_back( { target, "focus", std::move( focus ) } );
		auto hide = std::make_unique<Callback>( [this]( Rml::Event& ) { hideStockpileTooltip(); } );
		target->AddEventListener( "mouseout", hide.get() );
		listeners_.push_back( { target, "mouseout", std::move( hide ) } );
		auto blur = std::make_unique<Callback>( [this]( Rml::Event& ) { hideStockpileTooltip(); } );
		target->AddEventListener( "blur", blur.get() );
		listeners_.push_back( { target, "blur", std::move( blur ) } );
	}
}
void Management6ARmlBinding::showStockpileTooltip( const std::string& textValue, Rml::Element* source )
{
	if ( !stockpile_ || !source ) return;
	if ( auto* tooltip = stockpile_->GetElementById( "stockpile_tooltip" ) )
	{
		tooltip->SetInnerRML( safe( textValue ) );
		const auto offset = source->GetAbsoluteOffset();
		const auto size = source->GetBox().GetSize();
		const auto dimensions = context_.GetDimensions();
		const bool placeLeft = offset.x > dimensions.x * 0.5f;
		tooltip->SetProperty( "transform", placeLeft ? "translate(-100%, -50%)" : "translateY(-50%)" );
		tooltip->SetProperty( "left", std::to_string( placeLeft ? offset.x - 10.0f : offset.x + size.x + 10.0f ) + "px" );
		tooltip->SetProperty( "top", std::to_string( offset.y + size.y * 0.5f ) + "px" );
		tooltip->SetClass( "is-visible", true );
		tooltip->SetAttribute( "aria-hidden", "false" );
	}
}
void Management6ARmlBinding::hideStockpileTooltip()
{
	if ( !stockpile_ ) return;
	if ( auto* tooltip = stockpile_->GetElementById( "stockpile_tooltip" ) )
	{
		tooltip->SetClass( "is-visible", false );
		tooltip->SetAttribute( "aria-hidden", "true" );
	}
}
void Management6ARmlBinding::text( const char* id, const std::string& value )
{
	if ( auto* e = element( id ) )
		e->SetInnerRML( safe( value ) );
}
void Management6ARmlBinding::visible( const char* id, bool value )
{
	if ( auto* e = element( id ) )
		e->SetClass( "is-hidden", !value );
}
void Management6ARmlBinding::checked( const char* id, bool value )
{
	if ( auto* e = element( id ) )
	{
		if ( e->GetTagName() == "input" && e->HasAttribute( "checked" ) != value )
		{
			syncingCheckbox_ = true;
			if ( value ) e->SetAttribute( "checked", "checked" );
			else e->RemoveAttribute( "checked" );
			syncingCheckbox_ = false;
		}
		e->SetClass( "is-checked", value );
		e->SetClass( "is-unchecked", !value );
	}
}
void Management6ARmlBinding::enabled( const char* id, bool value )
{
	if ( auto* e = element( id ) )
	{
		if ( value )
			e->RemoveAttribute( "disabled" );
		else
			e->SetAttribute( "disabled", "" );
	}
}
std::string Management6ARmlBinding::formValue( const char* id ) const
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ) )
		return e->GetValue();
	return {};
}
void Management6ARmlBinding::formValue( const char* id, const std::string& value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ); e && context_.GetFocusElement() != e )
		e->SetValue( value );
}
std::int32_t Management6ARmlBinding::priority( const char* id, std::int32_t fallback, std::int32_t maximum ) const
{
	const auto value = formValue( id );
	std::int32_t out {};
	const auto result = std::from_chars( value.data(), value.data() + value.size(), out );
	return result.ec == std::errc {} && result.ptr == value.data() + value.size() ? std::clamp( out, 1, std::max( 1, maximum ) ) : fallback;
}
std::int32_t Management6ARmlBinding::normalizePriority( const char* id, std::int32_t fallback, std::int32_t maximum )
{
	const auto value = priority( id, fallback, maximum );
	const auto normalized = std::to_string( value );
	if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ) )
	{
		if ( input->GetValue() != normalized )
		{
			normalizingPriority_ = true;
			input->SetValue( normalized );
			normalizingPriority_ = false;
		}
	}
	return value;
}
bool Management6ARmlBinding::activateElement( std::string_view id )
{
	if ( auto* e = element( std::string( id ).c_str() ) )
	{
		e->DispatchEvent( "click", Rml::Dictionary {} );
		return true;
	}
	return false;
}
bool Management6ARmlBinding::setFormValueForProbe( std::string_view id, std::string_view value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( std::string( id ).c_str() ) ) )
	{
		e->SetValue( std::string( value ) );
		if(id=="workshop_priority") e->DispatchEvent("change",Rml::Dictionary{});
		if(id=="agriculture_search") e->DispatchEvent("input",Rml::Dictionary{});
		if(id=="agriculture_farm_search") e->DispatchEvent("input",Rml::Dictionary{});
		return true;
	}
	return false;
}
bool Management6ARmlBinding::setStockpileSearchForProbe( std::string_view value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_allow_filter_item" ) ) )
	{
		e->Focus();
		e->SetValue( std::string( value ) );
		e->DispatchEvent( "input", Rml::Dictionary {} );
		return controller_ && controller_->state().stockpile.allowColumnFilters[2] == std::string( value ) && context_.GetFocusElement() == e;
	}
	return false;
}
bool Management6ARmlBinding::activateFirstStockpileFilterForProbe( TriState state, FilterDepth depth )
{
	if ( !controller_ )
		return false;
	for ( const auto& row : controller_->state().stockpile.visibleFilters )
	{
		if ( row.state != state || row.id.depth != depth )
			continue;
		if ( auto* e = element( stockpileFilterRowId( row.id ).c_str() ) )
		{
			e->DispatchEvent( "click", Rml::Dictionary {} );
			return true;
		}
	}
	return false;
}
bool Management6ARmlBinding::activateStockpileFilterForProbe( std::string_view item, std::string_view material )
{
	if ( !controller_ )
		return false;
	for ( const auto& row : controller_->state().stockpile.visibleFilters )
	{
		if ( row.id.depth != FilterDepth::Material || row.id.item.value != item || row.id.material.value != material || row.state != TriState::On )
			continue;
		if ( auto* e = element( stockpileFilterRowId( row.id ).c_str() ) )
		{
			e->DispatchEvent( "click", Rml::Dictionary {} );
			return true;
		}
	}
	return false;
}
bool Management6ARmlBinding::dispatchStockpileFilterKeyForProbe( int keyIdentifier )
{
	if ( !controller_ || !controller_->state().stockpile.selectedFilter )
		return false;
	if ( auto* row = element( stockpileFilterRowId( *controller_->state().stockpile.selectedFilter ).c_str() ) )
	{
		row->Focus();
		Rml::Dictionary parameters;
		parameters["key_identifier"] = keyIdentifier;
		row->DispatchEvent( "keydown", parameters );
		return true;
	}
	return false;
}

void Management6ARmlBinding::workshopMarkup( const char* id, const std::string& markup )
{
	// Keep live controls, focus and scroll stable across simulation snapshots.
	if ( auto* e = element( id ); e && workshopMarkup_[id] != markup )
	{
		workshopMarkup_[id] = markup;
		e->SetInnerRML( markup );
	}
}
void Management6ARmlBinding::renderWorkshop( const WorkshopState& s )
{
	renderingWorkshop_ = true;
	const auto status = s.request.status;
	visible( "workshop_loading", status == RequestStatus::Loading );
	visible( "workshop_empty", status == RequestStatus::Empty );
	visible( "workshop_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "workshop_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "workshop_error", s.request.message );
	text( "workshop_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"workshop.title"} ) : s.value.name );
	text( "workshop_subtype", s.value.subtype.empty() ? "Production workshop" : s.value.subtype );
	if ( workshopSettingsId_ != s.value.id || workshopSettingsName_ != s.value.name )
		formValue( "workshop_name", s.value.name );
	formValue( "workshop_search", s.search );
	if ( workshopSettingsId_ != s.value.id || workshopSettingsPriority_ != s.value.priority )
		formValue( "workshop_priority", std::to_string( s.value.priority + 1 ) );
	workshopSettingsId_ = s.value.id;
	workshopSettingsName_ = s.value.name;
	workshopSettingsPriority_ = s.value.priority;
	text( "workshop_summary", "Priority " + std::to_string( s.value.priority + 1 ) + " / " + std::to_string( s.value.maxPriority ) + " | " + ( s.value.suspended ? "Suspended" : "Active" ) + " | linked stockpile " + ( s.value.connectStockpile ? "yes" : "no" ) );
	text( "workshop_toggle_suspended", s.value.suspended ? "Resume" : "Suspend" );
	checked( "workshop_toggle_suspended", s.value.suspended );
	checked( "workshop_toggle_generated", s.value.acceptGenerated );
	checked( "workshop_toggle_auto_missing", s.value.autoCraftMissing );
    if(auto* select=rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(element("workshop_stockpile_choice"))) {
        const auto previous=select->GetValue();
        // Retain the select itself while options change; selection never mutates the simulation.
        std::string options;
        for(const auto& row : s.value.stockpiles) options += "<option value='"+std::to_string(row.id.value)+"'>"+safe(row.name)+(row.linked?" (linked)":"")+"</option>";
        workshopMarkup("workshop_stockpile_choice",options);
        const auto current=std::find_if(s.value.stockpiles.begin(),s.value.stockpiles.end(),[&](const auto& row){return std::to_string(row.id.value)==previous && !row.linked;});
        const auto first=std::find_if(s.value.stockpiles.begin(),s.value.stockpiles.end(),[](const auto& row){return !row.linked;});
        const auto choice=current!=s.value.stockpiles.end()?current:first;
        if(choice!=s.value.stockpiles.end()) select->SetValue(std::to_string(choice->id.value));
        enabled("workshop_link_add",choice!=s.value.stockpiles.end());
    }
    std::string links;
    for(const auto& row : s.value.stockpiles) if(row.linked)
        links += "<div class='c-workshop-link-row'><span>"+safe(row.name)+"</span><button class='c-button' data-unlink='"+std::to_string(row.id.value)+"'>Unlink</button></div>";
    workshopMarkup("workshop_linked_stockpiles",links);
    text("workshop_link_help",s.value.stockpiles.empty()?"Create a stockpile to link it here.":links.empty()?"No linked stockpiles. Choose one above and click Link.":"Linked stockpiles can be anywhere on the map.");
    text("workshop_order_feedback",s.orderFeedback);
    text("workshop_queue_once",s.orderPending?"Adding...":"Add order");
    enabled("workshop_queue_once",!s.orderPending && s.selectedProduct.has_value());
	text( "workshop_sort", s.sort == SortDirection::Ascending ? "A-Z" : "Z-A" );
	text( "workshop_rail_status", std::string( s.value.suspended ? "Suspended" : "Active" ) + " | " + std::to_string( s.value.queue.size() ) + " orders" );
	text( "workshop_view_queue", "Queue (" + std::to_string( s.value.queue.size() ) + ")" );
	for ( const auto& [name, pane] : std::array {
		std::pair { "craft", WorkshopPane::Craft }, std::pair { "queue", WorkshopPane::Queue },
		std::pair { "settings", WorkshopPane::Settings }, std::pair { "trade", WorkshopPane::Trade } } )
	{
		visible( ( std::string( "workshop_" ) + name + "_pane" ).c_str(), s.pane == pane );
		if ( auto* tab = element( ( std::string( "workshop_view_" ) + name ).c_str() ) )
		{
			tab->SetClass( "is-selected", s.pane == pane );
			tab->SetAttribute( "aria-selected", s.pane == pane ? "true" : "false" );
		}
	}
	formValue( "workshop_order_count", std::to_string( s.orderCount ) );
	enabled( "workshop_queue_once", !s.orderPending && s.selectedProduct.has_value() );
	enabled( "workshop_order_count", s.selectedProduct.has_value() && s.orderMode != CraftRepeatMode::Repeat );
	text( "workshop_order_count_label", s.orderMode == CraftRepeatMode::Maintain ? "Stock limit" : "Quantity" );
	text( "workshop_order_help", s.orderMode == CraftRepeatMode::Maintain ? "Craft until the selected item and material reach this stock limit." : s.orderMode == CraftRepeatMode::Repeat ? "Keep crafting until you suspend or cancel the order." : "Craft this many items, then finish the order." );
	const auto workshopRows = s.visibleProducts.size();
	if ( workshopPage_ >= workshopRows )
		workshopPage_ = workshopRows ? pageSize_ * ( ( workshopRows - 1 ) / pageSize_ ) : 0;
	visible( "workshop_page_previous", workshopRows > pageSize_ );
	visible( "workshop_page_next", workshopRows > pageSize_ );
	enabled( "workshop_page_previous", workshopPage_ > 0 );
	enabled( "workshop_page_next", workshopPage_ + pageSize_ < workshopRows );
	std::string products;
	for ( std::size_t index = workshopPage_; index < std::min( workshopPage_ + pageSize_, s.visibleProducts.size() ); ++index )
	{
		const auto& r       = s.visibleProducts[index];
		const bool selected = s.selectedProduct && *s.selectedProduct == r.id;
		products += "<button id='" + rowId( "m6a_product_", r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "'>" + safe( displayCatalogLabel( r.id.value ) ) + "</button>";
	}
	if ( products.empty() )
		products = "<div class='c-m6a-row'>No crafts match the filter.</div>";
	if ( auto* e = element( "workshop_products" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		workshopMarkup( "workshop_products", products );
		if ( restore && s.selectedProduct )
			if ( auto* row = element( rowId( "m6a_product_", s.selectedProduct->value ).c_str() ) )
				row->Focus();
	}
	std::string productEditor = "<div class='c-workshop-editor__empty'>Select a craft to configure an order.</div>";
	if ( s.selectedProduct )
	{
		const auto product = std::find_if( s.value.products.begin(), s.value.products.end(), [&]( const auto& row ) { return row.id == *s.selectedProduct; } );
		if ( product != s.value.products.end() )
		{
			const auto modeButton = [&]( const char* value, CraftRepeatMode valueMode, const char* label )
			{ return "<button id='workshop_order_mode_" + std::string( value ) + "' class='c-button c-workshop-mode-button' data-order-mode='" + value + "'>" + label + "</button>"; };
			productEditor = "<div class='c-workshop-editor__title'>New order: " + safe( displayCatalogLabel( product->id.value ) ) + ( s.selectionFiltered ? " (hidden by filter)" : "" ) + "</div>";
			productEditor += "<div class='c-workshop-editor__label'>Order type</div><div class='c-workshop-mode-row'>" + modeButton( "once", CraftRepeatMode::Once, "Craft number" ) + modeButton( "maintain", CraftRepeatMode::Maintain, "Stock limit" ) + modeButton( "repeat", CraftRepeatMode::Repeat, "Repeat" ) + "</div>";
			if ( product->components.empty() )
				productEditor += "<div class='c-workshop-requirements'>No material requirements.</div>";
			else
			{
				productEditor += "<div class='c-workshop-editor__label'>Required materials</div><div class='c-workshop-requirements'>";
				for ( std::size_t index = 0; index < product->components.size(); ++index )
				{
					const auto& component = product->components[index];
					productEditor += "<label class='c-workshop-material-button'><span>" + safe( displayCatalogLabel( component.item.value ) ) + " x" + std::to_string( component.amount ) + ( component.requireSameMaterial ? " (same material)" : "" ) + "</span><select id='workshop_material_" + std::to_string( index ) + "' data-material-index='" + std::to_string( index ) + "'>";
					for ( const auto& choice : component.materials )
						productEditor += "<option value='" + safe( choice.first.value ) + "'>" + safe( displayCatalogLabel( choice.first.value ) ) + "</option>";
					productEditor += "</select><strong id='workshop_material_status_" + std::to_string( index ) + "'></strong></label>";
				}
				productEditor += "</div><div class='c-workshop-editor__hint'>Choose a material for each component. Missing stock leaves the order pending.</div>";
			}
		}
	}
	if ( auto* e = element( "workshop_product_selection" ) )
		workshopMarkup( "workshop_product_selection", productEditor );
	// A select dispatches change before WidgetDropDown finishes restoring focus.
	// Keep the select and its options alive: rebuilding this subtree here causes
	// a use-after-free in the remainder of the mouse-release event.
	for ( const auto& [id, modeValue] : std::array {
		std::pair { "once", CraftRepeatMode::Once }, std::pair { "maintain", CraftRepeatMode::Maintain },
		std::pair { "repeat", CraftRepeatMode::Repeat } } )
		if ( auto* button = element( ( std::string( "workshop_order_mode_" ) + id ).c_str() ) )
			button->SetClass( "is-selected", s.orderMode == modeValue );
	if ( s.selectedProduct )
	{
		const auto product = std::find_if( s.value.products.begin(), s.value.products.end(), [&]( const auto& row ) { return row.id == *s.selectedProduct; } );
		if ( product != s.value.products.end() ) for ( std::size_t index = 0; index < product->components.size(); ++index )
		{
			const auto& component = product->components[index];
			const auto material = index < s.orderMaterials.size() ? s.orderMaterials[index] : CatalogId { "any" };
			auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( ( "workshop_material_" + std::to_string( index ) ).c_str() ) );
			if ( !select ) continue;
			for ( std::size_t n = 0; n < component.materials.size(); ++n )
				if ( auto* option = select->GetOption( static_cast<int>( n ) ) )
				{
					const auto& choice = component.materials[n];
					const auto label = safe( displayCatalogLabel( choice.first.value ) ) + " (" + std::to_string( choice.second ) + " in stock)";
					if ( option->GetInnerRML() != label ) option->SetInnerRML( label );
				}
			if ( select->GetValue() != material.value ) select->SetValue( material.value );
			// Refresh the closed control's label after a stock-count update without
			// replacing any option or dispatching a different selection.
			select->SetSelection( select->GetSelection() );
			const auto choice = std::find_if( component.materials.begin(), component.materials.end(), [&]( const auto& entry ) { return entry.first == material; } );
			const bool shortage = choice == component.materials.end() || choice->second < component.amount;
			select->GetParentNode()->SetClass( "is-shortage", shortage );
			text( ( "workshop_material_status_" + std::to_string( index ) ).c_str(), shortage ? "Missing materials - order will wait" : "Materials available" );
		}
	}
	std::string queue;
	for ( std::size_t n = 0; n < s.visibleQueue.size(); ++n )
	{
		const auto& r       = s.visibleQueue[n];
		const bool selected = s.selectedJob && *s.selectedJob == r.id;
		queue += "<button id='m6a_job_" + std::to_string( r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-job='" + std::to_string( r.id.value ) + "'>#" + std::to_string( n + 1 ) + " " + safe( displayCatalogLabel( r.craft.value ) ) + " | " + mode( r.mode ) + " " + std::to_string( r.count ) + ( r.suspended ? " | paused" : "" ) + "</button>";
	}
	if ( queue.empty() )
		queue = "<div class='c-m6a-row'>No orders yet. Select a craft and add an order.</div>";
	if ( auto* e = element( "workshop_queue" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		workshopMarkup( "workshop_queue", queue );
		if ( restore && s.selectedJob )
			if ( auto* row = element( ( "m6a_job_" + std::to_string( s.selectedJob->value ) ).c_str() ) )
				row->Focus();
	}
	visible( "workshop_job_quantity", s.selectedJob.has_value() );
	text( "workshop_job_help", "" );
	std::string jobEditor = "<div class='c-workshop-editor__empty'>Select an order to edit it.</div>";
	if ( s.selectedJob )
	{
		const auto row = std::find_if( s.value.queue.begin(), s.value.queue.end(), [&]( const auto& value ) { return value.id == *s.selectedJob; } );
		if ( row != s.value.queue.end() )
		{
			if ( workshopEditingJob_ != s.selectedJob || workshopEditingCount_ != row->count )
			{
				if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "workshop_job_count" ) ) ) control->SetValue( std::to_string( row->count ) );
				workshopEditingJob_ = s.selectedJob;
				workshopEditingCount_ = row->count;
			}
			enabled( "workshop_job_count", row->mode != CraftRepeatMode::Repeat );
			text( "workshop_job_count_label", row->mode == CraftRepeatMode::Maintain ? "Stock limit" : "Quantity" );
			text( "workshop_job_help", row->mode == CraftRepeatMode::Maintain ? "Maintain this many in stock. Apply saves the new limit." : row->mode == CraftRepeatMode::Repeat ? "Repeats until suspended or cancelled." : "Apply saves the order quantity." );
			const auto modeButton = [&]( const char* action, CraftRepeatMode valueMode, const char* label )
			{ return "<button class='c-button c-workshop-mode-button" + std::string( row->mode == valueMode ? " is-selected" : "" ) + "' data-job-action='" + action + "'>" + label + "</button>"; };
			jobEditor = "<div class='c-workshop-editor__title'>" + safe( displayCatalogLabel( row->craft.value ) ) + "</div>";
			jobEditor += "<div class='c-workshop-editor__label'>Order type</div><div class='c-workshop-mode-row'>" + modeButton( "mode-once", CraftRepeatMode::Once, "Craft number" ) + modeButton( "mode-maintain", CraftRepeatMode::Maintain, "Stock limit" ) + modeButton( "mode-repeat", CraftRepeatMode::Repeat, "Repeat" ) + "</div>";
			jobEditor += "<div class='c-workshop-editor__summary'>Crafted " + std::to_string( row->alreadyCrafted ) + " | materials: ";
			for ( std::size_t index = 0; index < row->materials.size(); ++index )
				jobEditor += ( index ? ", " : "" ) + safe( displayCatalogLabel( row->materials[index].value ) );
			jobEditor += "</div><div class='c-workshop-actions'>";
			jobEditor += "<button class='c-button' data-job-action='toggle-suspended'>" + std::string( row->suspended ? "Resume" : "Suspend" ) + "</button>";
			jobEditor += "<button class='c-button" + std::string( row->moveBack ? " is-selected" : "" ) + "' data-job-action='toggle-move-back'>Move back when done</button>";
			jobEditor += "<button class='c-button' data-job-action='front'>Top</button><button class='c-button' data-job-action='up'>Up</button><button class='c-button' data-job-action='down'>Down</button><button class='c-button' data-job-action='back'>Bottom</button>";
			jobEditor += "<button class='c-button c-button--danger' data-job-action='cancel'>Cancel order</button></div>";
		}
	}
	if ( auto* e = element( "workshop_job_selection" ) )
		workshopMarkup( "workshop_job_selection", jobEditor );
	const bool butcher = s.value.subtype == "Butcher";
	const bool fisher  = s.value.subtype == "Fisher" || s.value.catchFish || s.value.processFish;
	const bool trade   = s.value.subtype == "TradingPost" || s.tradeLoaded;
	visible( "workshop_butcher_actions", butcher );
	visible( "workshop_fisher_actions", fisher );
	visible( "workshop_special", butcher || fisher );
	visible( "workshop_trade", trade );
	visible( "workshop_view_trade", trade );
	text( "workshop_toggle_corpses", toggleText( "Butcher corpses", s.value.butcherCorpses ) );
	text( "workshop_toggle_excess", toggleText( "Butcher excess", s.value.butcherExcess ) );
	text( "workshop_toggle_catch", toggleText( "Catch fish", s.value.catchFish ) );
	text( "workshop_toggle_process", toggleText( "Process fish", s.value.processFish ) );
	checked( "workshop_toggle_corpses", s.value.butcherCorpses );
	checked( "workshop_toggle_excess", s.value.butcherExcess );
	checked( "workshop_toggle_catch", s.value.catchFish );
	checked( "workshop_toggle_process", s.value.processFish );
	std::string trades;
	auto tradeRowId = []( const TradeRowId& r )
	{ return std::string( "m6a_trade_" ) + ( r.party == TradeParty::Trader ? "trader" : "player" ) + "_" + hex( r.item.value ) + "_" + hex( r.materialOrGender.value ) + "_" + std::to_string( r.quality ); };
	std::size_t tradeIndex = 0;
	auto add               = [&]( const auto& rows, const char* party )
	{for(const auto&r:rows){const auto index=tradeIndex++;(void)index;const bool selected=s.selectedTradeRow&&*s.selectedTradeRow==r.id;trades+="<button id='"+tradeRowId(r.id)+"' class='c-m6a-row-button"+std::string(selected?" is-selected":"")+"' data-party='"+std::string(party)+"' data-item='"+safe(r.id.item.value)+"' data-material='"+safe(r.id.materialOrGender.value)+"' data-quality='"+std::to_string(r.id.quality)+"'>"+(r.id.party==TradeParty::Trader?"Trader":"Settlement")+" | "+safe(r.name)+" | stock "+std::to_string(r.stock)+" | offered "+std::to_string(r.offered)+" | value "+std::to_string(r.unitValue)+"</button>";} };
	add( s.traderRows, "trader" );
	add( s.playerRows, "player" );
	if ( trades.empty() )
		trades = "<div class='c-m6a-row'>Trade inventory has not been loaded.</div>";
	if ( auto* e = element( "workshop_trade_rows" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( trades );
		if ( restore && s.selectedTradeRow )
			if ( auto* row = element( tradeRowId( *s.selectedTradeRow ).c_str() ) )
				row->Focus();
	}
	text( "workshop_trade_totals", "Trader offer " + std::to_string( s.traderOfferValue ) + " | settlement offer " + std::to_string( s.playerOfferValue ) );
	visible( "workshop_trade_confirmation", s.tradeConfirmationRequired );
	enabled( "workshop_trade_execute", s.tradeLoaded && !s.tradeConfirmationRequired );
	text( "workshop_status", s.request.refreshInProgress ? textCatalog_.format( LocalizationKey{"status.refreshing"} ) : std::string {} );
	renderingWorkshop_ = false;
}
void Management6ARmlBinding::renderStockpile( const StockpileState& s )
{
	const auto status = s.request.status;
	visible( "stockpile_loading", status == RequestStatus::Loading );
	visible( "stockpile_empty", status == RequestStatus::Empty );
	visible( "stockpile_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "stockpile_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "stockpile_error", s.request.message );
	text( "stockpile_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"stockpile.title"} ) : s.value.name );
	formValue( "stockpile_name", s.value.name );
	const auto maximumPriority = std::max( 1, s.value.maxPriority );
	const auto displayPriority = std::clamp( std::max( 0, s.value.priority ) + 1, 1, maximumPriority );
	formValue( "stockpile_priority", std::to_string( displayPriority ) );
	enabled( "stockpile_priority_up", displayPriority > 1 );
	enabled( "stockpile_priority_down", displayPriority < maximumPriority );
	text( "stockpile_summary", textCatalog_.format( LocalizationKey{"management.stockpile_summary"}, {{"items",std::to_string(s.value.itemCount)},{"reserved",std::to_string(s.value.reserved)},{"status",s.value.suspended ? "Suspended" : "Active"}} ) );
	text( "stockpile_toggle_suspended", s.value.suspended ? "Resume" : "Suspend" );
	checked( "stockpile_toggle_suspended", s.value.suspended );
	checked( "stockpile_toggle_pull", s.value.pullFromOthers );
	checked( "stockpile_toggle_allow_pull", s.value.allowPullFromHere );
	const auto pane = s.pane;
	visible( "stockpile_settings_pane", pane == StockpilePane::Settings );
	visible( "stockpile_contents_pane", pane == StockpilePane::Contents );
	visible( "stockpile_allow_pane", pane == StockpilePane::AllowList );
	for ( const auto& tab : { std::pair { "stockpile_view_contents", pane == StockpilePane::Contents }, std::pair { "stockpile_view_allow", pane == StockpilePane::AllowList }, std::pair { "stockpile_view_settings", pane == StockpilePane::Settings } } )
		if ( auto* e = element( tab.first ) )
		{
			e->SetClass( "is-selected", tab.second );
			 e->SetAttribute( "aria-selected", tab.second ? "true" : "false" );
		}
	const std::array contentFilters {
		"stockpile_content_filter_category", "stockpile_content_filter_group", "stockpile_content_filter_item",
		"stockpile_content_filter_material", "stockpile_content_filter_stock", "stockpile_content_filter_total"
	};
	const std::array allowFilters {
		"stockpile_allow_filter_category", "stockpile_allow_filter_group", "stockpile_allow_filter_item",
		"stockpile_allow_filter_material", "stockpile_allow_filter_status"
	};
	const std::array contentFilterToggles {
		"stockpile_content_filter_category_toggle", "stockpile_content_filter_group_toggle", "stockpile_content_filter_item_toggle",
		"stockpile_content_filter_material_toggle", "stockpile_content_filter_stock_toggle", "stockpile_content_filter_total_toggle"
	};
	const std::array contentFilterOptions {
		"stockpile_content_filter_category_options", "stockpile_content_filter_group_options", "stockpile_content_filter_item_options",
		"stockpile_content_filter_material_options", "stockpile_content_filter_stock_options", "stockpile_content_filter_total_options"
	};
	const std::array allowFilterToggles {
		"stockpile_allow_filter_category_toggle", "stockpile_allow_filter_group_toggle", "stockpile_allow_filter_item_toggle",
		"stockpile_allow_filter_material_toggle", "stockpile_allow_filter_status_toggle"
	};
	const std::array allowFilterOptions {
		"stockpile_allow_filter_category_options", "stockpile_allow_filter_group_options", "stockpile_allow_filter_item_options",
		"stockpile_allow_filter_material_options", "stockpile_allow_filter_status_options"
	};
	for ( std::size_t column = 0; column < contentFilters.size(); ++column ) formValue( contentFilters[column], column >= 4 && !s.contentColumnSelections[column].empty() ? ( s.contentColumnSelections[column].front() == "Has (>0)" ? std::string( ">0" ) : std::string( "0" ) ) : s.contentColumnFilters[column] );
	for ( std::size_t column = 0; column < allowFilters.size(); ++column ) formValue( allowFilters[column], s.allowColumnFilters[column] );
	const auto renderFilterCombos = [this, &s]( const auto& filters, const auto& toggles, const auto& options, const auto& selected, const auto& openColumn, bool allowList )
	{
		for ( std::size_t column = 0; column < filters.size(); ++column )
		{
			const bool open = openColumn && *openColumn == column;
			if ( open )
			{
				std::vector<std::string> values;
				if ( allowList )
				{
					std::array<std::string, 4> path;
					for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
					{
						const auto& row = s.value.filters[index];
						const auto depth = static_cast<std::size_t>( row.id.depth );
						path[depth] = row.label;
						for ( std::size_t deeper = depth + 1; deeper < path.size(); ++deeper ) path[deeper].clear();
						const bool hasChild = index + 1 < s.value.filters.size() && static_cast<std::size_t>( s.value.filters[index + 1].id.depth ) > depth;
						if ( hasChild ) continue;
						auto labels = path;
						normalizeInventoryTableLabels( labels[1], labels[2], labels[3], row.id.item.value );
						values.push_back( column < 4 ? labels[column] : filterStateLabel( row.state ) );
					}
				}
				else
				{
					for ( std::size_t index = 0; index < s.value.contents.size(); ++index )
					{
						const auto& row = s.value.contents[index];
						const auto depth = static_cast<std::size_t>( row.id.depth );
						const bool hasChild = index + 1 < s.value.contents.size() && static_cast<std::size_t>( s.value.contents[index + 1].id.depth ) > depth;
						if ( hasChild ) continue;
						const auto labels = stockpilePathLabels( row.id, s.value.filters );
						if ( column == 0 ) values.push_back( labels.category );
						else if ( column == 1 ) values.push_back( labels.group );
						else if ( column == 2 ) values.push_back( labels.item );
						else if ( column == 3 ) values.push_back( labels.material );
						else if ( column == 4 ) values.push_back( inventoryQuantityLabel( row.stockpiled ) );
						else values.push_back( inventoryQuantityLabel( row.total ) );
					}
				}
				if ( auto* list = element( options[column] ) ) list->SetInnerRML( filterOptionMarkup( std::move( values ), selected[column], options[column] ) );
			}
			visible( options[column], open );
			if ( auto* input = element( filters[column] ) ) input->SetAttribute( "aria-expanded", open ? "true" : "false" );
			if ( auto* toggle = element( toggles[column] ) )
			{
				toggle->SetAttribute( "aria-expanded", open ? "true" : "false" );
				toggle->SetClass( "is-selected", open );
				toggle->SetClass( "has-selection", !selected[column].empty() );
			}
		}
	};
	renderFilterCombos( contentFilters, contentFilterToggles, contentFilterOptions, s.contentColumnSelections, stockpileContentFilterMenuColumn_, false );
	renderFilterCombos( allowFilters, allowFilterToggles, allowFilterOptions, s.allowColumnSelections, stockpileAllowFilterMenuColumn_, true );
	formValue( "stockpile_template_name", s.templateName );
	std::string templates;
	for ( const auto& name : s.value.templateNames )
		templates += "<button type='button' class='c-stockpile-template-option" + std::string( foldedLabel( name ) == foldedLabel( s.templateName ) ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( foldedLabel( name ) == foldedLabel( s.templateName ) ? "true" : "false" ) + "' data-template='" + safe( name ) + "'>" + safe( name ) + "</button>";
	if ( templates.empty() ) templates = "<div class='c-stockpile-template-empty'>" + safe( textCatalog_.format( LocalizationKey { "management.stockpile.template_empty" } ) ) + "</div>";
	if ( auto* e = element( "stockpile_template_options" ) ) e->SetInnerRML( templates );
	visible( "stockpile_template_options", s.templateMenuOpen );
	const auto existingTemplate = std::ranges::any_of( s.value.templateNames, [&]( const auto& name ) { return foldedLabel( name ) == foldedLabel( s.templateName ); } );
	text( "stockpile_template_save", textCatalog_.format( LocalizationKey { existingTemplate ? "management.stockpile.template_overwrite" : "management.stockpile.template_save_new" } ) );
	enabled( "stockpile_template_save", !s.templateName.empty() );
	if ( auto* input = element( "stockpile_template_name" ) ) input->SetAttribute( "aria-expanded", s.templateMenuOpen ? "true" : "false" );
	if ( auto* toggle = element( "stockpile_template_toggle" ) )
	{
		toggle->SetAttribute( "aria-expanded", s.templateMenuOpen ? "true" : "false" );
		toggle->SetClass( "is-selected", s.templateMenuOpen );
	}
	text( "stockpile_template_overwrite_name", s.pendingTemplateOverwrite );
	visible( "stockpile_template_confirmation", s.templateOverwriteConfirmationRequired );
	std::size_t allowedRules = 0;
	std::size_t totalRules = 0;
	for ( const auto& r : s.value.filters )
		if ( r.id.depth == FilterDepth::Material )
		{
			++totalRules;
			allowedRules += r.state == TriState::On ? 1 : 0;
		}
	const auto matchingRules = s.matchingFilterLeaves.size();
	text( "stockpile_filter_count", std::to_string( allowedRules ) + " / " + std::to_string( totalRules ) + " allowed" );
	const auto storedEntries = std::ranges::count_if( s.value.contents, []( const auto& row ) { return row.id.depth == FilterDepth::Material; } );
	text( "stockpile_content_count", std::to_string( storedEntries ) + " entries" );
	const auto tableColumns = inventoryTableColumns( "" );
	for ( const char* id : { "stockpile_content_column_category", "stockpile_filter_column_category" } ) text( id, tableColumns.category );
	for ( const char* id : { "stockpile_content_column_group", "stockpile_filter_column_group" } ) text( id, tableColumns.group );
	for ( const char* id : { "stockpile_content_column_item", "stockpile_filter_column_item" } ) text( id, tableColumns.item );
	for ( const char* id : { "stockpile_content_column_material", "stockpile_filter_column_material" } ) text( id, tableColumns.material );
	const auto updateSort = [this]( const char* id, const char* directionId, StockpileSortKey key, StockpileSortKey selectedKey, SortDirection direction )
	{
		const bool selected = selectedKey == key;
		if ( auto* e = element( id ) )
		{
			e->SetClass( "is-selected", selected );
			e->SetAttribute( "aria-pressed", selected ? "true" : "false" );
		}
		text( directionId, selected ? ( direction == SortDirection::Ascending ? "^" : "v" ) : "" );
	};
	for ( const auto& binding : std::array {
		std::tuple { "stockpile_content_sort_category", "stockpile_content_sort_category_direction", StockpileSortKey::Category },
		std::tuple { "stockpile_content_sort_group", "stockpile_content_sort_group_direction", StockpileSortKey::Group },
		std::tuple { "stockpile_content_sort_item", "stockpile_content_sort_item_direction", StockpileSortKey::Item },
		std::tuple { "stockpile_content_sort_material", "stockpile_content_sort_material_direction", StockpileSortKey::Material },
		std::tuple { "stockpile_content_sort_stock", "stockpile_content_sort_stock_direction", StockpileSortKey::Quantity },
		std::tuple { "stockpile_content_sort_total", "stockpile_content_sort_total_direction", StockpileSortKey::Total }
	} ) updateSort( std::get<0>( binding ), std::get<1>( binding ), std::get<2>( binding ), s.contentSort, s.sort );
	for ( const auto& binding : std::array {
		std::tuple { "stockpile_allow_sort_category", "stockpile_allow_sort_category_direction", StockpileSortKey::Category },
		std::tuple { "stockpile_allow_sort_group", "stockpile_allow_sort_group_direction", StockpileSortKey::Group },
		std::tuple { "stockpile_allow_sort_item", "stockpile_allow_sort_item_direction", StockpileSortKey::Item },
		std::tuple { "stockpile_allow_sort_material", "stockpile_allow_sort_material_direction", StockpileSortKey::Material },
		std::tuple { "stockpile_allow_sort_status", "stockpile_allow_sort_status_direction", StockpileSortKey::Status }
	} ) updateSort( std::get<0>( binding ), std::get<1>( binding ), std::get<2>( binding ), s.allowSort, s.allowSortDirection );
	enabled( "stockpile_allow_bulk", matchingRules > 0 );
	enabled( "stockpile_block_bulk", matchingRules > 0 );
	visible( "stockpile_restore_filter_search", s.filterSearchRevealed );
	stockpileFilterMarkup_.clear();
	for ( std::size_t index = 0; index < s.visibleFilters.size(); ++index )
	{
		const auto& r = s.visibleFilters[index];
		const bool selected = s.selectedFilter && *s.selectedFilter == r.id;
		const auto checked = r.state == TriState::On ? "true" : r.state == TriState::Off ? "false" : "mixed";
		const auto rowAttrs = " data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "' data-depth='" + std::to_string( static_cast<int>( r.id.depth ) ) + "'";
		const auto labels = stockpilePathLabels( r.id, s.value.filters );
		const auto cell = []( const std::string& value ) { return value.empty() ? std::string( "-" ) : safe( value ); };
		const auto rowClass = std::string( "c-m6a-filter-row c-stockpile-flat-row" ) + ( r.state == TriState::Mixed ? " is-mixed" : r.state == TriState::Off ? " is-off" : " is-on" ) + ( selected ? " is-selected" : "" );
		stockpileFilterMarkup_.push_back( "<button id='" + stockpileFilterRowId( r.id ) + "' type='button' class='" + rowClass + "' role='row' data-rule='true'" + rowAttrs + " aria-selected='" + ( selected ? "true" : "false" ) + "' aria-checked='" + checked + "' aria-label='" + cell( labels.category ) + ", " + cell( labels.group ) + ", " + cell( labels.item ) + ", " + cell( labels.material ) + ": " + filterStateLabel( r.state ) + "'><span class='c-m6a-rule-check'>" + check( r.state ) + "</span><span class='c-stockpile-flat__category'>" + cell( labels.category ) + "</span><span class='c-stockpile-flat__group'>" + cell( labels.group ) + "</span><span class='c-stockpile-flat__item'>" + filterIcon( r ) + "<span class='c-stockpile-flat__label'>" + cell( labels.item ) + "</span></span><span class='c-stockpile-flat__material'>" + cell( labels.material ) + "</span><span class='c-stockpile-flat__status'>" + filterStateLabel( r.state ) + "</span></button>" );
	}
	if ( stockpileFilterMarkup_.empty() ) stockpileFilterMarkup_.push_back( "<div class='c-m6a-row'>No rules match these filters.</div>" );
	stockpileFilterFirst_ = static_cast<std::size_t>( -1 );
	if ( auto* list = element( "stockpile_filters" ) )
	{
		const bool restore = list->Contains( context_.GetFocusElement() );
		if ( restore && s.selectedFilter )
		{
			const auto selected = std::ranges::find_if( s.visibleFilters, [&]( const auto& row ) { return row.id == *s.selectedFilter; } );
			if ( selected != s.visibleFilters.end() )
			{
				const float top = static_cast<float>( std::distance( s.visibleFilters.begin(), selected ) ) * 48.f;
				if ( top < list->GetScrollTop() || top + 48.f > list->GetScrollTop() + list->GetClientHeight() ) list->SetScrollTop( top );
			}
		}
		renderStockpileFilterViewport();
		if ( restore && s.selectedFilter )
			if ( auto* row = element( stockpileFilterRowId( *s.selectedFilter ).c_str() ) ) row->Focus();
	}
	auto contentRowId = []( const StockpileContentRowId& r )
	{ return "m6a_content_" + hex( r.category.value ) + "_" + hex( r.group.value ) + "_" + hex( r.item.value ) + "_" + hex( r.material.value ) + "_" + std::to_string( static_cast<int>( r.depth ) ); };
	std::string rows;
	for ( const auto& r : s.visibleContents )
	{
		const bool selected = s.selectedContent && *s.selectedContent == r.id;
		const auto labels = stockpilePathLabels( r.id, s.value.filters );
		const auto cell = []( const std::string& value ) { return value.empty() ? std::string( "-" ) : safe( value ); };
		rows += "<button id='" + contentRowId( r.id ) + "' class='c-m6a-row-button c-m6a-content-row c-stockpile-flat-row" + std::string( selected ? " is-selected" : "" ) + "' role='row' data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "' data-depth='" + std::to_string( static_cast<int>( r.id.depth ) ) + "' aria-label='" + cell( labels.category ) + ", " + cell( labels.group ) + ", " + cell( labels.item ) + ", " + cell( labels.material ) + ", " + std::to_string( r.stockpiled ) + " in this stockpile, " + std::to_string( r.total ) + " total'><span class='c-stockpile-flat__category'>" + cell( labels.category ) + "</span><span class='c-stockpile-flat__group'>" + cell( labels.group ) + "</span><span class='c-stockpile-flat__item'>" + contentIcon( r ) + "<span class='c-stockpile-flat__label'>" + cell( labels.item ) + "</span></span><span class='c-stockpile-flat__material'>" + cell( labels.material ) + "</span><span class='c-m6a-content-metric'>" + std::to_string( r.stockpiled ) + "</span><span class='c-m6a-content-metric'>" + std::to_string( r.total ) + "</span></button>";
	}
	if ( rows.empty() )
		rows = "<div class='c-m6a-row c-m6a-content-empty'>" + std::string( std::ranges::none_of( s.contentColumnFilters, []( const auto& value ) { return !value.empty(); } ) ? "No items stored here yet." : "No stored items match these filters." ) + "</div>";
	if ( auto* e = element( "stockpile_rows" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( rows );
		if ( restore && s.selectedContent )
			if ( auto* row = element( contentRowId( *s.selectedContent ).c_str() ) )
				row->Focus();
	}
}
void Management6ARmlBinding::renderStockpileFilterViewport()
{
	if ( renderingStockpileFilters_ ) return;
	auto* list = element( "stockpile_filters" );
	if ( !list ) return;
	const float scroll = std::clamp( list->GetScrollTop(), 0.f,
		std::max( 0.f, static_cast<float>( stockpileFilterMarkup_.size() ) * 48.f - list->GetClientHeight() ) );
	const auto anchor = static_cast<std::size_t>( scroll / 48.f );
	const auto first = std::min( anchor > 6 ? anchor - 6 : 0, stockpileFilterMarkup_.size() );
	if ( first == stockpileFilterFirst_ ) return;
	const auto count = std::max<std::size_t>( 24, static_cast<std::size_t>( std::max( list->GetClientHeight(), 480.f ) / 48.f ) + 14 );
	const auto last = std::min( first + count, stockpileFilterMarkup_.size() );
	std::string markup;
	if ( first ) markup += "<div style='display:block;width:100%;height:" + std::to_string( first * 48 ) + "dp; flex-shrink:0;'></div>";
	for ( auto index = first; index < last; ++index ) markup += stockpileFilterMarkup_[index];
	if ( last < stockpileFilterMarkup_.size() ) markup += "<div style='display:block;width:100%;height:" + std::to_string( ( stockpileFilterMarkup_.size() - last ) * 48 ) + "dp; flex-shrink:0;'></div>";
	renderingStockpileFilters_ = true;
	list->SetInnerRML( markup );
	list->SetScrollTop( scroll );
	stockpileFilterFirst_ = first;
	renderingStockpileFilters_ = false;
}
void Management6ARmlBinding::renderAgriculture( const AgricultureState& s )
{
	const auto status = s.request.status;
	visible( "agriculture_loading", status == RequestStatus::Loading );
	visible( "agriculture_empty", status == RequestStatus::Empty );
	visible( "agriculture_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "agriculture_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "agriculture_error", s.request.message );
	text( "agriculture_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"agriculture.title"} ) : s.value.name );
	text( "agriculture_kind", kind( s.value.target.kind ) );
	const bool farm = s.value.target.kind == AgricultureKind::Farm;
	if ( auto* pane = element( "agriculture_products_pane" ) ) pane->SetClass( "is-farm", farm );
	visible( "agriculture_farm_planner", farm );
	text( "agriculture_overview_heading", textCatalog_.format( LocalizationKey { farm ? "management.agriculture.farm_overview" : s.value.target.kind == AgricultureKind::Grove ? "management.agriculture.grove_overview" : "management.agriculture.pasture_overview" } ) );
	text( "agriculture_view_products", textCatalog_.format( LocalizationKey { farm ? "management.agriculture.view_farm_plan" : "management.agriculture.view_products" } ) );
	text( "agriculture_view_work", textCatalog_.format( LocalizationKey { farm ? "management.agriculture.view_harvest" : s.value.target.kind == AgricultureKind::Grove ? "management.agriculture.grove_work" : "management.agriculture.pasture" } ) );
	for ( const auto& tab : { std::pair { "agriculture_view_overview", AgriculturePane::Overview }, std::pair { "agriculture_view_products", AgriculturePane::Products }, std::pair { "agriculture_view_settings", AgriculturePane::Settings }, std::pair { "agriculture_view_work", AgriculturePane::Work } } )
	{
		const bool active = s.pane == tab.second;
		if ( auto* e = element( tab.first ) ) { e->SetClass( "is-selected", active ); e->SetAttribute( "aria-selected", active ? "true" : "false" ); }
	}
	visible( "agriculture_overview_pane", s.pane == AgriculturePane::Overview );
	visible( "agriculture_products_pane", s.pane == AgriculturePane::Products );
	visible( "agriculture_settings_pane", s.pane == AgriculturePane::Settings );
	visible( "agriculture_work_pane", s.pane == AgriculturePane::Work );
	visible( "agriculture_farm_counts", farm );
	visible( "agriculture_product_counts", farm || s.value.target.kind == AgricultureKind::Grove );
	text( "agriculture_plots", std::to_string( s.value.plots ) );
	text( "agriculture_tilled", std::to_string( s.value.tilled ) );
	text( "agriculture_planted", std::to_string( s.value.planted ) );
	text( "agriculture_ready", std::to_string( s.value.ready ) );
	const auto current = std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& row ) { return row.id == s.value.product; } );
	const auto productName = !s.value.productName.empty() ? s.value.productName : current != s.value.catalog.end() ? current->name : s.value.product.value;
	const auto productLabel = productName.empty() ? std::string( "No product selected" ) : textCatalog_.format( LocalizationKey { "management.agriculture.current_product" }, { { "product", productName } } );
	text( "agriculture_overview_product", productLabel );
	text( "agriculture_current_product", productLabel );
	const auto chosen = std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& row ) { return s.selectedProduct && row.id == *s.selectedProduct; } );
	const auto& counts = chosen != s.value.catalog.end() ? chosen : current;
	text( "agriculture_product_seeds", counts != s.value.catalog.end() ? std::to_string( counts->available ) : "—" );
	text( "agriculture_product_items", counts != s.value.catalog.end() ? std::to_string( counts->harvested ) : "—" );
	text( "agriculture_product_plants", counts != s.value.catalog.end() ? std::to_string( counts->planted ) : "—" );
	text( "agriculture_overview_summary", "Priority " + std::to_string( s.value.priority ) + " / " + std::to_string( s.value.maxPriority ) + ( s.value.suspended ? " | Suspended" : " | Active" ) );
	text( "agriculture_farm_harvest_summary", textCatalog_.format( LocalizationKey { "management.agriculture.harvest_summary" }, { { "ready", std::to_string( s.value.ready ) }, { "plots", std::to_string( s.value.plots ) } } ) );
	formValue( "agriculture_name", s.value.name );
	formValue( "agriculture_priority", std::to_string( s.value.priority ) );
	text( "agriculture_summary", textCatalog_.format( LocalizationKey{"management.agriculture_summary"}, {{"priority",std::to_string(s.value.priority)},{"maximum",std::to_string(s.value.maxPriority)},{"plots",std::to_string(s.value.plots)},{"planted",std::to_string(s.value.planted)},{"ready",std::to_string(s.value.ready)}} ) );
	text( "agriculture_toggle_suspended", s.value.suspended ? "Resume" : "Suspend" );
	text( "agriculture_sort", std::string( "Sort " ) + ( s.sort == SortDirection::Ascending ? "A-Z v" : "Z-A v" ) );
	checked( "agriculture_toggle_suspended", s.value.suspended );
	visible( "agriculture_farm", s.value.target.kind == AgricultureKind::Farm );
	visible( "agriculture_pasture", s.value.target.kind == AgricultureKind::Pasture );
	visible( "agriculture_grove", s.value.target.kind == AgricultureKind::Grove );
	const auto agricultureRows = std::max( s.visibleCatalog.size(), s.visibleAnimals.size() );
	if ( agriculturePage_ >= agricultureRows )
		agriculturePage_ = agricultureRows ? pageSize_ * ( ( agricultureRows - 1 ) / pageSize_ ) : 0;
	enabled( "agriculture_page_previous", agriculturePage_ > 0 );
	enabled( "agriculture_page_next", agriculturePage_ + pageSize_ < agricultureRows );
	std::string products;
	for ( std::size_t index = agriculturePage_; index < std::min( agriculturePage_ + pageSize_, s.visibleCatalog.size() ); ++index )
	{
		const auto& r       = s.visibleCatalog[index];
		const bool selected = s.selectedProduct && *s.selectedProduct == r.id;
		products += "<button id='" + rowId( "m6a_agri_product_", r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "'>" + safe( r.name ) + " | available " + std::to_string( r.available ) + "</button>";
	}
	if ( products.empty() )
		products = "<div class='c-m6a-row'>No products match the filter.</div>";
	if ( auto* e = element( "agriculture_products" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( products );
		if ( restore && s.selectedProduct )
			if ( auto* row = element( rowId( "m6a_agri_product_", s.selectedProduct->value ).c_str() ) )
				row->Focus();
	}
	text( "agriculture_product_selection", s.selectedProduct ? "Selected product: " + s.selectedProduct->value + ( s.selectionFiltered ? " (hidden by filter)" : "" ) : "No product selected" );
	text( "agriculture_toggle_harvest", toggleText( "Harvest", s.value.harvest ) );
	text( "agriculture_toggle_pasture_harvest", toggleText( "Harvest products", s.value.harvest ) );
	text( "agriculture_toggle_hay", toggleText( "Harvest hay", s.value.harvestHay ) );
	text( "agriculture_toggle_tame", toggleText( "Tame wild animals", s.value.tame ) );
	text( "agriculture_toggle_pick", toggleText( "Pick fruit", s.value.pick ) );
	text( "agriculture_toggle_plant", toggleText( "Plant trees", s.value.plant ) );
	text( "agriculture_toggle_fell", toggleText( "Fell trees", s.value.fell ) );
	checked( "agriculture_toggle_harvest", s.value.harvest );
	checked( "agriculture_toggle_pasture_harvest", s.value.harvest );
	checked( "agriculture_toggle_hay", s.value.harvestHay );
	checked( "agriculture_toggle_tame", s.value.tame );
	checked( "agriculture_toggle_pick", s.value.pick );
	checked( "agriculture_toggle_plant", s.value.plant );
	checked( "agriculture_toggle_fell", s.value.fell );
	text( "agriculture_pasture_summary", "Animals " + std::to_string( s.value.total ) + " / " + std::to_string( s.value.capacity ) + " | male cap " + std::to_string( s.value.maxMale ) + " | female cap " + std::to_string( s.value.maxFemale ) + " | food " + std::to_string( s.value.foodCurrent ) + " / " + std::to_string( s.value.foodMax ) + " | hay " + std::to_string( s.value.hayCurrent ) + " / " + std::to_string( s.value.hayMax ) );
	std::string animals;
	for ( std::size_t index = agriculturePage_; index < std::min( agriculturePage_ + pageSize_, s.visibleAnimals.size() ); ++index )
	{
		const auto& r       = s.visibleAnimals[index];
		const bool selected = s.selectedAnimal && *s.selectedAnimal == r.id;
		animals += "<button id='m6a_animal_" + std::to_string( r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-animal='" + std::to_string( r.id.value ) + "'>" + safe( r.name ) + " | " + ( r.gender == Gender::Male ? "male" : "female" ) + ( r.young ? " | young" : "" ) + ( r.butcher ? " | marked for butchering" : "" ) + "</button>";
	}
	if ( animals.empty() )
		animals = "<div class='c-m6a-row'>No animals match the filter.</div>";
	if ( auto* e = element( "agriculture_animals" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( animals );
		if ( restore && s.selectedAnimal )
			if ( auto* row = element( ( "m6a_animal_" + std::to_string( s.selectedAnimal->value ) ).c_str() ) )
				row->Focus();
	}
	text( "agriculture_animal_selection", s.selectedAnimal ? "Selected animal ID " + std::to_string( s.selectedAnimal->value ) : "No animal selected" );
	std::string foods;
	for ( const auto& r : s.value.foods )
		foods += "<div class='c-m6a-row c-check-row'>" + check( r.allowed ? TriState::On : TriState::Off ) + " " + safe( r.name ) + "</div>";
	if ( foods.empty() )
		foods = "<div class='c-m6a-row'>No pasture food rules are available.</div>";
	if ( auto* e = element( "agriculture_foods" ) )
		e->SetInnerRML( foods );
	enabled( "agriculture_toggle_first_food", !s.value.foods.empty() );
	text( "agriculture_status", s.request.refreshInProgress ? textCatalog_.format( LocalizationKey{"status.refreshing"} ) : std::string {} );
	if ( farm ) renderFarmPlanner( s );
}
void Management6ARmlBinding::renderFarmPlanner( const AgricultureState& s )
{
	const auto cropIcon = []( const AgricultureCatalogRow* crop ) {
		std::string sheet = crop && !crop->iconSheet.empty() ? crop->iconSheet : "build_Seed.tga";
		if ( !std::ranges::all_of( sheet, []( unsigned char c ) { return std::isalnum( c ) || c == '_' || c == '-' || c == '.'; } ) )
			sheet = "build_Seed.tga";
		return "<img class='l-farm-crop-icon' src='/tilesheet/" + sheet + "' alt='' aria-hidden='true' />";
	};
	const auto iconFor = [&]( const std::string& id ) {
		const auto crop = std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& row ) { return row.id.value == id; } );
		return cropIcon( crop == s.value.catalog.end() ? nullptr : &*crop );
	};
	const auto updateList = [this]( const char* id, const std::string& markup, std::string& previous ) {
		auto* container = element( id );
		if ( !container || ( markup == previous && container->GetNumChildren() > 0 ) ) return;
		std::string focused;
		if ( auto* focus = context_.GetFocusElement(); focus && container->Contains( focus ) ) focused = focus->GetId();
		const float scrollTop = container->GetScrollTop();
		const float scrollLeft = container->GetScrollLeft();
		container->SetInnerRML( markup );
		container->SetScrollTop( scrollTop );
		container->SetScrollLeft( scrollLeft );
		previous = markup;
		if ( !focused.empty() ) if ( auto* row = element( focused.c_str() ) ) row->Focus();
	};
	text( "agriculture_plot_selection_count", std::to_string( s.selectedPlots.size() ) + " / " + std::to_string( s.value.fields.size() ) );
	formValue( "agriculture_farm_search", s.search );
	std::string grid;
	if ( s.value.fields.empty() ) grid = "<p>No plots in this Farm.</p>";
	else
	{
		std::map<int, std::map<std::pair<int, int>, const FarmPlotRow*>> layers;
		for ( const auto& field : s.value.fields )
			layers[field.position.z][{ field.position.x, field.position.y }] = &field;
		for ( const auto& [z, cells] : layers )
		{
			int minX = cells.begin()->first.first, maxX = minX;
			int minY = cells.begin()->first.second, maxY = minY;
			for ( const auto& [position, field] : cells )
			{
				minX = std::min( minX, position.first ); maxX = std::max( maxX, position.first );
				minY = std::min( minY, position.second ); maxY = std::max( maxY, position.second );
			}
			if ( layers.size() > 1 ) grid += "<p class='l-farm-grid-layer'>Level " + std::to_string( z ) + "</p>";
			for ( int y = minY; y <= maxY; ++y )
			{
				grid += "<div class='l-farm-grid-row' style='width:" + std::to_string( ( maxX - minX + 1 ) * 33 ) + "dp'>";
				for ( int x = minX; x <= maxX; ++x )
				{
					const auto found = cells.find( { x, y } );
					if ( found == cells.end() ) { grid += "<span class='l-farm-cell-gap'></span>"; continue; }
					const auto& field = *found->second;
					const bool selected = std::find( s.selectedPlots.begin(), s.selectedPlots.end(), field.position ) != s.selectedPlots.end();
					const auto crop = field.planted ? field.plantedCrop.value : !field.orders.empty() ? field.orders.front().crop.value : !field.assignedCrop.value.empty() ? field.assignedCrop.value : s.value.product.value;
					const std::string icon = crop.empty() ? "<span class='l-farm-cell__empty' aria-hidden='true'>+</span>" : iconFor( crop );
					const auto key = farmPlotKey( field.position );
					const auto description = "Plot " + key + " | " + ( crop.empty() ? "No crop" : crop ) + ( field.ready ? " | Ready" : field.planted ? " | Growing" : field.tilled ? " | Tilled" : " | Untilled" ) + ( field.busy ? " | Working" : "" );
					grid += "<button id='agriculture_plot_" + std::to_string( field.position.x ) + "_" + std::to_string( field.position.y ) + "_" + std::to_string( field.position.z ) + "' class='l-farm-cell" + ( field.ready ? std::string( " is-ready" ) : field.planted ? " is-planted" : field.tilled ? " is-tilled" : "" ) + ( selected ? " is-selected" : "" ) + "' data-plot='" + key + "' aria-pressed='" + ( selected ? "true" : "false" ) + "' aria-label='" + safe( description ) + "' title='" + safe( description ) + "'>" + icon + "</button>";
				}
				grid += "</div>";
			}
		}
	}
	updateList( "agriculture_plot_grid", grid, farmGridMarkup_ );
	std::string crops;
	for ( const auto& crop : s.visibleCatalog )
	{
		const bool selected = s.selectedProduct && *s.selectedProduct == crop.id;
		crops += "<button id='agriculture_farm_crop_" + hex( crop.id.value ) + "' class='l-farm-crop-row" + ( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( crop.id.value ) + "' title='" + safe( crop.name ) + "' aria-selected='" + ( selected ? "true" : "false" ) + "'>" + cropIcon( &crop ) + "<span>" + safe( crop.name ) + " · " + std::to_string( crop.available ) + " seeds</span></button>";
	}
	if ( crops.empty() ) crops = "<p>No crops match the filter.</p>";
	updateList( "agriculture_farm_catalog", crops, farmCatalogMarkup_ );
	const auto chosen = s.selectedProduct ? std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& crop ) { return crop.id == *s.selectedProduct; } ) : s.value.catalog.end();
	text( "agriculture_farm_chosen_crop", chosen == s.value.catalog.end() ? "Choose a crop from the list." : "Selected: " + chosen->name );
	text( "agriculture_farm_crop_counts", chosen == s.value.catalog.end() ? "" : std::to_string( chosen->available ) + " seeds · " + std::to_string( chosen->harvested ) + " harvested items · " + std::to_string( chosen->planted ) + " plants on map" );
	const bool canAssign = !s.selectedPlots.empty() && chosen != s.value.catalog.end();
	enabled( "agriculture_plot_assign", canAssign );
	enabled( "agriculture_plot_queue", canAssign );
	enabled( "agriculture_plot_repeat", canAssign );
	enabled( "agriculture_plot_default", !s.selectedPlots.empty() );
	enabled( "agriculture_farm_default_crop", chosen != s.value.catalog.end() );
	std::string plotDetail = s.selectedPlots.empty() ? "Select one or more plots in the grid." : std::to_string( s.selectedPlots.size() ) + " plots selected · Changes apply to each plot.";
	if ( s.selectedPlots.size() == 1 )
	{
		const auto field = std::find_if( s.value.fields.begin(), s.value.fields.end(), [&]( const auto& row ) { return row.position == s.selectedPlots.front(); } );
		if ( field != s.value.fields.end() )
			plotDetail = "Plot " + farmPlotKey( field->position ) + " · Assigned: " + ( field->assignedCrop.value.empty() ? "Farm default" : field->assignedCrop.value ) + " · Growing: " + ( field->plantedCrop.value.empty() ? "None" : field->plantedCrop.value );
	}
	text( "agriculture_plot_detail", plotDetail );
	std::string orders;
	if ( s.selectedPlots.size() == 1 )
	{
		const auto field = std::find_if( s.value.fields.begin(), s.value.fields.end(), [&]( const auto& row ) { return row.position == s.selectedPlots.front(); } );
		if ( field != s.value.fields.end() )
		{
			for ( const auto& order : field->orders )
			{
				const auto key = farmPlotKey( field->position );
				const auto common = " data-plot='" + key + "' data-order='" + std::to_string( order.id ) + "'";
				const auto rowId = "agriculture_order_" + std::to_string( order.id );
				orders += "<div class='l-farm-order-row'><span>" + safe( order.crop.value ) + ( order.repeat ? " · repeat" : " · " + std::to_string( order.remaining ) + " left" ) + "</span><button id='" + rowId + "_up'" + common + " data-order-action='up' title='" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_earlier_help" } ) ) + "'>" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_up" } ) ) + "</button><button id='" + rowId + "_down'" + common + " data-order-action='down' title='" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_later_help" } ) ) + "'>" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_down" } ) ) + "</button><button id='" + rowId + "_cancel'" + common + " data-order-action='cancel' title='" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_remove_help" } ) ) + "'>" + safe( textCatalog_.format( LocalizationKey { "management.agriculture.order_remove" } ) ) + "</button></div>";
			}
			if ( orders.empty() ) orders = "<p>No queued plantings. The assigned or farm default crop repeats.</p>";
		}
	}
	else orders = "<p>Select one plot to inspect or edit its queue.</p>";
	updateList( "agriculture_plot_orders", orders, farmOrdersMarkup_ );
}

void Management6ARmlBinding::stateChanged( const Management6AState& s )
{
	if ( !workshop_ || !stockpile_ || !agriculture_ )
		return;
	if ( !presentationEnabled_ )
	{
		workshop_->Hide();
		stockpile_->Hide();
		agriculture_->Hide();
		return;
	}
	if ( s.view != activeView_ )
	{
		if ( activeView_ == ManagementView::Workshop )
			workshop_->Hide();
		else if ( activeView_ == ManagementView::Stockpile )
			stockpile_->Hide();
		else if ( activeView_ == ManagementView::Agriculture )
			agriculture_->Hide();
		if ( s.view == ManagementView::Workshop )
			workshop_->Show();
		else if ( s.view == ManagementView::Stockpile )
			stockpile_->Show();
		else if ( s.view == ManagementView::Agriculture )
			agriculture_->Show();
		activeView_ = s.view;
	}
	visible( "workshop_manager_root", s.view == ManagementView::Workshop );
	visible( "stockpile_manager_root", s.view == ManagementView::Stockpile );
	visible( "agriculture_manager_root", s.view == ManagementView::Agriculture );
	// Only the active detached workbench needs DOM projection. Rebuilding all
	// three documents on every live-search keystroke made Stockpile text entry
	// pay for two hidden screens as well as its own large rule tree.
	if ( s.view == ManagementView::Workshop )
		renderWorkshop( s.workshop );
	else if ( s.view == ManagementView::Stockpile )
		renderStockpile( s.stockpile );
	else if ( s.view == ManagementView::Agriculture )
		renderAgriculture( s.agriculture );
	if ( s.workshop.tradeConfirmationRequired && !tradeConfirmationVisible_ )
	{
		if ( auto* confirm = element( "workshop_trade_confirm" ) )
			confirm->Focus();
	}
	else if ( !s.workshop.tradeConfirmationRequired && tradeConfirmationVisible_ )
	{
		if ( auto* review = element( "workshop_trade_execute" ) )
			review->Focus();
	}
	tradeConfirmationVisible_ = s.workshop.tradeConfirmationRequired;
	if ( s.stockpile.templateOverwriteConfirmationRequired && !stockpileTemplateConfirmationVisible_ )
	{
		if ( auto* cancel = element( "stockpile_template_overwrite_cancel" ) ) cancel->Focus();
	}
	else if ( !s.stockpile.templateOverwriteConfirmationRequired && stockpileTemplateConfirmationVisible_ )
	{
		if ( auto* save = element( "stockpile_template_save" ) ) save->Focus();
	}
	stockpileTemplateConfirmationVisible_ = s.stockpile.templateOverwriteConfirmationRequired;
	const std::string status  = s.pendingAction ? textCatalog_.format( LocalizationKey{"status.updating"} ) : s.status;
	if ( s.view == ManagementView::Workshop )
		text( "workshop_status", status );
	else if ( s.view == ManagementView::Agriculture )
		text( "agriculture_status", status );
}
} // namespace ingnomia::ui::management6a
