/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorRmlBinding.h"
#include "../../localization/RmlText.h"
#include "../../runtime/SelectOptions.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string_view>
namespace ingnomia::ui::inspector
{
namespace
{
void traceInspectorSkills( const QString& message )
{
	if ( !qEnvironmentVariableIsSet( "INGNOMIA_TRACE_INSPECTOR_SKILLS" ) ) return;
	qInfo().noquote() << message;
	const auto path = qEnvironmentVariable( "INGNOMIA_TRACE_INSPECTOR_SKILLS_PATH" );
	if ( path.isEmpty() ) return;
	QFile trace( path );
	if ( !trace.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) return;
	QTextStream stream( &trace );
	stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs ) << ' ' << message << '\n';
}

std::string asciiLower( const std::string& value )
{
	std::string lowered = value;
	std::transform( lowered.begin(), lowered.end(), lowered.begin(), []( unsigned char character ) {
		return static_cast<char>( std::tolower( character ) );
	} );
	return lowered;
}

std::string displayName( std::string_view value )
{
	std::string out;
	bool capitalize = true;
	char previous = 0;
	for ( const unsigned char character : value )
	{
		if ( character == '_' || character == '-' )
		{
			out += ' ';
			capitalize = true;
			previous = ' ';
			continue;
		}
		if ( std::isupper( character ) && previous && std::islower( static_cast<unsigned char>( previous ) ) ) out += ' ';
		out += static_cast<char>( capitalize ? std::toupper( character ) : character );
		capitalize = false;
		previous = static_cast<char>( character );
	}
	return out;
}

int skillLevel( const std::string& detail )
{
	const auto marker = detail.find( "level " );
	if ( marker == std::string::npos ) return -1;
	std::size_t cursor = marker + 6;
	int level = 0;
	bool foundDigit = false;
	while ( cursor < detail.size() && std::isdigit( static_cast<unsigned char>( detail[cursor] ) ) )
	{
		foundDigit = true;
		level = level * 10 + ( detail[cursor] - '0' );
		++cursor;
	}
	return foundDigit ? level : -1;
}

bool skillActive( const std::string& detail )
{
	return detail.find( "| active" ) != std::string::npos;
}

const char* equipmentElement( UniformSlot slot )
{
	switch( slot )
	{
		case UniformSlot::HeadArmor: return "creature_equipment_slot_head";
		case UniformSlot::ChestArmor: return "creature_equipment_slot_chest";
		case UniformSlot::ArmArmor: return "creature_equipment_slot_arms";
		case UniformSlot::HandArmor: return "creature_equipment_slot_hands";
		case UniformSlot::LegArmor: return "creature_equipment_slot_legs";
		case UniformSlot::FootArmor: return "creature_equipment_slot_feet";
		case UniformSlot::LeftHandHeld: return "creature_equipment_slot_left_hand";
		case UniformSlot::RightHandHeld: return "creature_equipment_slot_right_hand";
		case UniformSlot::Back: return "creature_equipment_slot_back";
	}
	return "";
}

} // namespace

bool InspectorRmlBinding::stockpileItemIconInline( std::string* detail )
{
	if( !document_ )
	{
		if( detail ) *detail = "document unavailable";
		return false;
	}
	document_->UpdateDocument();
	auto* contents = document_->GetElementById( "stockpile_contents" );
	auto* row = contents && contents->GetNumChildren() > 0 ? contents->GetChild( 0 ) : nullptr;
	auto* item = row && row->GetNumChildren() > 0 ? row->GetChild( 0 ) : nullptr;
	auto* icon = item && item->GetNumChildren() > 0 ? item->GetChild( 0 ) : nullptr;
	auto* label = item && item->GetNumChildren() > 1 ? item->GetChild( 1 ) : nullptr;
	if( !icon || !label )
	{
		if( detail ) *detail = "stockpile item icon or label unavailable";
		return false;
	}
	const auto iconOffset = icon->GetAbsoluteOffset( Rml::BoxArea::Border );
	const auto iconSize = icon->GetBox().GetSize( Rml::BoxArea::Border );
	const auto labelOffset = label->GetAbsoluteOffset( Rml::BoxArea::Border );
	const auto labelSize = label->GetBox().GetSize( Rml::BoxArea::Border );
	const bool horizontal = labelOffset.x >= iconOffset.x + iconSize.x;
	const bool verticalOverlap = labelOffset.y < iconOffset.y + iconSize.y
		&& labelOffset.y + labelSize.y > iconOffset.y;
	if( detail )
	{
		std::ostringstream stream;
		stream << "icon=" << iconOffset.x << ',' << iconOffset.y << ',' << iconSize.x << ',' << iconSize.y
			<< " label=" << labelOffset.x << ',' << labelOffset.y << ',' << labelSize.x << ',' << labelSize.y;
		*detail = stream.str();
	}
	return horizontal && verticalOverlap;
}


namespace
{
std::string esc( const std::string& value ) { return Rml::StringUtilities::EncodeRml( value ); }
// A fact the game did not report is shown as "Unknown", never as zero (Stage 16).
std::string unknownOr( bool reported, std::int32_t value ) { return reported ? std::to_string( value ) : std::string( "Unknown" ); }
void setDisabled( Rml::Element* element, bool disabled )
{
	if( !element ) return;
	if( disabled ) element->SetAttribute( "disabled", "disabled" ); else element->RemoveAttribute( "disabled" );
	element->SetAttribute( "aria-disabled", disabled ? "true" : "false" );
}
std::string selectValue( Rml::Element* element )
{
	auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( element );
	return control ? std::string( control->GetValue() ) : std::string{};
}
void setSelectValue( Rml::Element* element, const std::string& value )
{
	if( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( element ); control && control->GetValue() != value ) control->SetValue( value );
}
std::string coordinates( const WorldPosition& p ) { return std::to_string( p.x ) + ", " + std::to_string( p.y ) + ", " + std::to_string( p.z ); }
// The game labels some tile entries "Kind: name" (for example "Gnome: Winkle"); the kind belongs in the Type column.
std::pair<std::string, std::string> typed( const std::string& fallback, const std::string& value )
{
	const auto colon = value.find( ": " );
	if( colon == std::string::npos || colon == 0 || colon > 24 ) return { fallback, value };
	return { value.substr( 0, colon ), value.substr( colon + 2 ) };
}
std::string counted( const TextCountRow& row )
{
	return ( row.count ? std::to_string( row.count ) + " x " : std::string{} ) + row.label + ( row.detail.empty() ? std::string{} : " (" + row.detail + ")" );
}
} // namespace

void InspectorRmlBinding::Callback::ProcessEvent( Rml::Event& e ) { fn_( e ); }
InspectorRmlBinding::InspectorRmlBinding( Rml::Context& c, int cameraSlot, int windowIndex, bool detachedWindow )
	: context_( c ), cameraSlot_( cameraSlot ), windowIndex_( windowIndex ), detachedWindow_( detachedWindow )
{
	cameraSource_ = cameraSlot_ == 0 ? "?camera-preview://selected" : "?camera-preview://slot-" + std::to_string( cameraSlot_ );
}
InspectorRmlBinding::~InspectorRmlBinding() { shutdown(); }

bool InspectorRmlBinding::initialize( InspectorController& c )
{
	controller_ = &c;
	document_ = documentLoader_ ? documentLoader_() : context_.LoadDocument( "screens/inspector.rml" );
	if( !document_ ) return false;
	localization::applyRmlText( *document_, textCatalog_ );
	if( auto* image = document_->GetElementById( "creature_preview_camera_image" ) ) image->SetAttribute( "src", cameraSource_ );
	if( detachedWindow_ )
	{
		if( auto* preview = document_->GetElementById( "creature_preview" ) ) preview->SetClass( "is-detached", true );
		if( auto* panel = document_->GetElementById( "inspector_panel" ) ) panel->SetClass( "is-detached", true );
	}
	if( windowIndex_ > 0 )
		if( auto* preview = document_->GetElementById( "creature_preview" ) )
		{
			const int index = windowIndex_ - 1;
			preview->SetProperty( "right", std::to_string( 16 + ( index % 3 ) * 380 ) + "dp" );
			preview->SetProperty( "bottom", std::to_string( 62 + ( index / 3 ) * 42 ) + "dp" );
		}
	const auto closeInspector = [this] { controller_->close(); if( closeHandler_ ) closeHandler_(); };
	bind( "inspector_back", [this] { controller_->back(); } );
	bind( "inspector_close", closeInspector );
	bind( "inspector_locate", [this] { controller_->locate(); } );
	bind( "inspector_refresh", [this] { controller_->refresh(); } );
	bind( "creature_preview_close", closeInspector );
	bind( "creature_preview_nav_camera", [this] { setPreviewPage( PreviewPage::Camera ); } );
	bind( "creature_preview_nav_stats", [this] { setPreviewPage( PreviewPage::Stats ); } );
	bind( "creature_preview_nav_expertise", [this] { setPreviewPage( PreviewPage::Expertise ); } );
	bind( "creature_preview_nav_equipment", [this] { setPreviewPage( PreviewPage::Equipment ); } );
	bind( "creature_preview_nav_inventory", [this] { setPreviewPage( PreviewPage::Inventory ); } );
	bind( "creature_preview_locate", [this] { controller_->locate(); } );
	bind( "creature_preview_skill_sort_name", [this] { setSkillSort( SkillSort::Name ); } );
	bind( "creature_preview_skill_sort_level", [this] { setSkillSort( SkillSort::Level ); } );
	bind( "creature_preview_skill_sort_active", [this] { setSkillSort( SkillSort::Active ); } );
	syncSkillSortButtons();
	// The profession drop-down applies its choice at once: an inspector has no Apply button (PDF p.167).
	bindEvent( "creature_preview_profession", "change", [this]( Rml::Event& )
	{
		if( rendering_ || !controller_ || !controller_->state().creature ) return;
		const auto value = selectValue( document_->GetElementById( "creature_preview_profession" ) );
		if( !value.empty() && value != controller_->state().creature->profession ) controller_->setProfession( value );
	} );
	const struct { const char* id; UniformSlot slot; } slotIds[] = {
		{ "creature_equipment_slot_head", UniformSlot::HeadArmor }, { "creature_equipment_slot_chest", UniformSlot::ChestArmor },
		{ "creature_equipment_slot_arms", UniformSlot::ArmArmor }, { "creature_equipment_slot_hands", UniformSlot::HandArmor },
		{ "creature_equipment_slot_legs", UniformSlot::LegArmor }, { "creature_equipment_slot_feet", UniformSlot::FootArmor },
		{ "creature_equipment_slot_left_hand", UniformSlot::LeftHandHeld }, { "creature_equipment_slot_right_hand", UniformSlot::RightHandHeld },
		{ "creature_equipment_slot_back", UniformSlot::Back } };
	for( const auto& entry : slotIds ) bind( entry.id, [this, slot = entry.slot] { controller_->selectEquipmentSlot( slot ); } );
	bindEvent( "creature_preview_equipment_type", "change", [this]( Rml::Event& )
	{
		if( rendering_ || !controller_ ) return;
		const auto value = selectValue( document_->GetElementById( "creature_preview_equipment_type" ) );
		if( !value.empty() && value != controller_->state().equipmentDraftType.value ) controller_->setEquipmentDraftType( CatalogId{ value } );
	} );
	bindEvent( "creature_preview_equipment_material", "change", [this]( Rml::Event& )
	{
		if( rendering_ || !controller_ ) return;
		const auto value = selectValue( document_->GetElementById( "creature_preview_equipment_material" ) );
		const auto& current = controller_->state().equipmentDraftMaterial;
		if( !value.empty() && ( !current || current->value != value ) ) controller_->setEquipmentDraftMaterial( CatalogId{ value } );
	} );
	bind( "creature_preview_equipment_apply", [this] { controller_->applyEquipmentSlot(); } );
	bind( "creature_preview_equipment_cancel", [this] { controller_->closeEquipmentEditor(); } );
	bind( "blueprint_cancel_job", [this] { controller_->executeContext( TileContextAction::CancelJob ); } );
	bind( "blueprint_raise_job", [this] { controller_->executeContext( TileContextAction::RaisePriority ); } );
	bind( "blueprint_lower_job", [this] { controller_->executeContext( TileContextAction::LowerPriority ); } );
	bind( "workshop_toggle_suspended", [this] { controller_->toggleWorkshopSuspended(); } );
	bind( "stockpile_toggle_suspended", [this] { controller_->toggleStockpileSuspended(); } );
	bind( "agriculture_toggle_suspended", [this] { controller_->toggleAgricultureSuspended(); } );
	bind( "agriculture_toggle_primary", [this] { controller_->toggleAgriculturePrimaryOption(); } );
	bind( "selection_cancel", [this] { controller_->cancelSelection(); } );
	bind( "selection_rotate", [this] { controller_->rotateSelection(); } );
	// Ctrl+Tab and Ctrl+Shift+Tab move between the creature pages (PDF p.147).
	bindEvent( "creature_preview", "keydown", [this]( Rml::Event& event )
	{
		const auto key = static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) );
		if( key != Rml::Input::KI_TAB || event.GetParameter<int>( "ctrl_key", 0 ) == 0 ) return;
		cyclePreviewPage( event.GetParameter<int>( "shift_key", 0 ) != 0 ? -1 : 1 );
		event.StopPropagation();
	} );
	stateChanged( c.state() );
	// In the game window the inspector is an overlay and must not take the keyboard focus from the screen the
	// player is using; in its own window it takes the focus so its keys work.
	if( detachedWindow_ ) document_->Show(); else document_->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
	return true;
}

bool InspectorRmlBinding::reloadDocument( InspectorController& c, bool preserveUiState )
{
	const auto previewPage = previewPage_;
	const auto skillSort = skillSort_;
	const auto skillScrollOffset = skillScrollOffset_;
	clearListeners( equipmentListeners_ );
	clearListeners( professionListeners_ );
	clearListeners( liveTileListeners_ );
	clearListeners( listeners_ );
	liveTileRowsMarkup_.clear();
	liveTileId_ = 0;
	tileCreature_ = 0;
	lastSkillRows_.clear();
	renderedProfessionChoices_.clear();
	renderedProfessionKnown_ = false;
	renderedTypeOptions_.clear();
	renderedMaterialOptions_.clear();
	lastSkillCreatureId_ = 0;
	skillScrollOffset_ = preserveUiState ? skillScrollOffset : 0;
	renderedPreviewSkillRows_.reset();
	renderedFullSkillRows_.reset();
	previewPage_ = preserveUiState ? previewPage : PreviewPage::Camera;
	previewCreatureId_ = 0;
	skillSort_ = preserveUiState ? skillSort : SkillSort::Name;
	lastSkillPanelOpen_ = false;
	if( document_ ) { context_.UnloadDocument( document_ ); document_ = nullptr; }
	return initialize( c );
}

bool InspectorRmlBinding::activateElement( std::string_view id )
{
	if( !document_ ) return false;
	auto* e = document_->GetElementById( std::string( id ) );
	if( !e || e->HasAttribute( "disabled" ) || !e->IsVisible( true ) ) return false;
	e->DispatchEvent( "click", Rml::Dictionary{} );
	return true;
}

bool InspectorRmlBinding::scrollLiveTile( int x, int y, float step )
{
	if( !liveInspection_ || !document_ ) return false;
	auto* list = document_->GetElementById( "live_tile_rows" );
	if( !list ) return false;
	for( auto* element = context_.GetElementAtPoint( { static_cast<float>( x ), static_cast<float>( y ) } ); element; element = element->GetParentNode() )
	{
		if( element != list ) continue;
		const float limit = std::max( 0.f, list->GetScrollHeight() - list->GetClientHeight() );
		list->SetScrollTop( std::clamp( list->GetScrollTop() - step, 0.f, limit ) );
		return true;
	}
	return false;
}

void InspectorRmlBinding::shutdown()
{
	clearListeners( equipmentListeners_ );
	clearListeners( professionListeners_ );
	clearListeners( liveTileListeners_ );
	clearListeners( listeners_ );
	liveTileRowsMarkup_.clear();
	liveTileId_ = 0;
	if( document_ ) { context_.UnloadDocument( document_ ); document_ = nullptr; }
	controller_ = nullptr;
	closeHandler_ = {};
	documentLoader_ = {};
}
void InspectorRmlBinding::bind( const char* id, std::function<void()> fn ) { bindEvent( id, "click", [fn = std::move( fn )]( Rml::Event& ) mutable { fn(); } ); }
void InspectorRmlBinding::bindEvent( const char* id, const char* event, std::function<void( Rml::Event& )> fn ) { if( auto* e = document_->GetElementById( id ) ) bindElement( e, event, std::move( fn ), listeners_ ); }
void InspectorRmlBinding::bindElement( Rml::Element* e, const char* event, std::function<void( Rml::Event& )> fn, std::vector<Listener>& store )
{
	if( !e ) return;
	auto cb = std::make_unique<Callback>( std::move( fn ) );
	e->AddEventListener( event, cb.get() );
	store.push_back( { e, event, std::move( cb ) } );
}
void InspectorRmlBinding::clearListeners( std::vector<Listener>& store )
{
	for( auto& l : store ) if( l.target ) l.target->RemoveEventListener( l.event.c_str(), l.callback.get() );
	store.clear();
}
void InspectorRmlBinding::text( const char* id, const std::string& v ) { if( auto* e = document_->GetElementById( id ) ) e->SetInnerRML( Rml::StringUtilities::EncodeRml( v ) ); }
void InspectorRmlBinding::rml( const char* id, const std::string& v ) { if( auto* e = document_->GetElementById( id ) ) e->SetInnerRML( v ); }
void InspectorRmlBinding::visible( const char* id, bool v )
{
	if( !document_ ) return;
	const auto element = std::string_view( id ? id : "" );
	if( !v && !lastSkillRows_.empty() && element == "creature_preview_skills" ) v = true;
	if( !lastSkillRows_.empty() && element == "creature_preview_skills_empty" ) v = false;
	if( auto* e = document_->GetElementById( id ) )
	{
		e->SetClass( "is-hidden", !v );
		if( v ) e->RemoveProperty( "display" ); else e->SetProperty( "display", "none" );
	}
}
void InspectorRmlBinding::positionTileLabel( const SelectionConfigurationState& s )
{
	if( !document_ ) return;
	if( auto* e = document_->GetElementById( "tile_world_label" ) )
	{
		const auto p = s.pointer.value_or( PointerPosition{ 16, 16 } );
		e->SetProperty( "left", std::to_string( p.x - 42 ) + "dp" );
		e->SetProperty( "top", std::to_string( p.y - 10 ) + "dp" );
	}
}
void InspectorRmlBinding::positionSelectionTip( const SelectionConfigurationState& s )
{
	if( !document_ ) return;
	if( auto* e = document_->GetElementById( "selection_pointer_tip" ) )
	{
		const auto p = s.pointer.value_or( PointerPosition{ 16, 16 } );
		e->SetProperty( "left", std::to_string( p.x + 14 ) + "dp" );
		e->SetProperty( "top", std::to_string( p.y + 18 ) + "dp" );
	}
}

// List rows (list view in details view). Skills get Skill / Level / Active columns.
void InspectorRmlBinding::rows( const char* id, const std::vector<TextCountRow>& values )
{
	if( !document_ ) return;
	const auto element = std::string_view( id ? id : "" );
	const bool skillRows = element == "creature_preview_skills";
	std::uint32_t creatureID = 0;
	if( skillRows && controller_ && controller_->state().creature )
	{
		creatureID = controller_->state().creature->id.value;
		if( creatureID != lastSkillCreatureId_ )
		{
			lastSkillCreatureId_ = creatureID;
			lastSkillRows_.clear();
			skillScrollOffset_ = 0;
			renderedPreviewSkillRows_.reset();
			renderedFullSkillRows_.reset();
		}
	}
	const auto* renderValues = &values;
	bool usedCache = false;
	if( skillRows )
	{
		if( !values.empty() && &values != &lastSkillRows_ ) lastSkillRows_ = values;
		else if( values.empty() && !lastSkillRows_.empty() ) { renderValues = &lastSkillRows_; usedCache = true; }
	}
	std::vector<TextCountRow> sortedValues;
	if( skillRows )
	{
		sortedValues = *renderValues;
		const auto byName = []( const TextCountRow& left, const TextCountRow& right ) { return asciiLower( left.label ) < asciiLower( right.label ); };
		switch( skillSort_ )
		{
		case SkillSort::Name:
			std::stable_sort( sortedValues.begin(), sortedValues.end(), byName );
			break;
		case SkillSort::Level:
			std::stable_sort( sortedValues.begin(), sortedValues.end(), [&byName]( const TextCountRow& left, const TextCountRow& right ) { const int l = skillLevel( left.detail ), r = skillLevel( right.detail ); return l != r ? l > r : byName( left, right ); } );
			break;
		case SkillSort::Active:
			std::stable_sort( sortedValues.begin(), sortedValues.end(), [&byName]( const TextCountRow& left, const TextCountRow& right ) { const bool l = skillActive( left.detail ), r = skillActive( right.detail ); return l != r ? l > r : byName( left, right ); } );
			break;
		}
		renderValues = &sortedValues;
	}
	auto* e = document_->GetElementById( id );
	if( !e ) return;
	std::string r;
	for( const auto& v : *renderValues )
	{
		r += "<div class='w98-list-item c-inspector-row' role='option'><span class='w98-cell w98-cell--grow c-inspector-row__label'>" + esc( v.label ) + "</span>";
		if( skillRows )
		{
			const int level = skillLevel( v.detail );
			r += "<span class='w98-cell w98-w-state c-inspector-row__detail'>" + ( level < 0 ? std::string( "Unknown" ) : std::to_string( level ) ) + "</span>";
			r += "<span class='w98-cell w98-w-state'>" + std::string( skillActive( v.detail ) ? "Yes" : "No" ) + "</span>";
		}
		else
		{
			if( !v.detail.empty() ) r += "<span class='w98-cell w98-w-profession c-inspector-row__detail'>" + esc( v.detail ) + "</span>";
			if( v.count ) r += "<span class='w98-cell w98-cell--num w98-w-amount'>" + std::to_string( v.count ) + "</span>";
		}
		r += "</div>";
	}
	bool skippedUnchanged = false;
	if( skillRows )
	{
		auto& rendered = renderedPreviewSkillRows_;
		if( rendered && *rendered == r ) skippedUnchanged = true; else rendered = r;
	}
	if( !skippedUnchanged ) e->SetInnerRML( r );
	if( skillRows )
	{
		const auto creatureName = controller_ && controller_->state().creature ? QString::fromStdString( controller_->state().creature->name ) : QStringLiteral( "-" );
		const auto firstLabel = !renderValues->empty() ? QString::fromStdString( renderValues->front().label ) : QStringLiteral( "-" );
		traceInspectorSkills( QStringLiteral( "rows window=%1 camera=%2 element=%3 creatureId=%4 name=%5 source=%6 rendered=%7 cache=%8 usedCache=%9 unchanged=%10 children=%11 first=%12" )
			.arg( windowIndex_ ).arg( cameraSlot_ ).arg( QString::fromUtf8( id ? id : "-" ) ).arg( static_cast<qulonglong>( creatureID ) ).arg( creatureName )
			.arg( values.size() ).arg( renderValues->size() ).arg( lastSkillRows_.size() ).arg( usedCache ? QStringLiteral( "true" ) : QStringLiteral( "false" ) )
			.arg( skippedUnchanged ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ).arg( e->GetNumChildren() ).arg( firstLabel ) );
	}
}

// Equipment: a Slot / Item list view. The uniform belongs to the citizen's military role, so the page states
// that scope before any change, and a change is committed with Apply (it affects every member of the role).
void InspectorRmlBinding::renderEquipment( const InspectorState& state )
{
	clearListeners( equipmentListeners_ );
	if( !document_ || !state.creature ) return;
	const auto& creature = *state.creature;
	const bool editable = static_cast<bool>( creature.equipmentRole );
	for( const auto& slot : creature.equipmentSlots )
	{
		auto* element = document_->GetElementById( equipmentElement( slot.slot ) );
		if( !element ) continue;
		const bool selected = state.equipmentSlotEditor && *state.equipmentSlotEditor == slot.slot;
		element->SetClass( "is-selected", selected );
		element->SetClass( "is-empty", slot.item.empty() );
		setDisabled( element, !editable );
		element->SetAttribute( "aria-selected", selected ? "true" : "false" );
		const auto item = slot.item.empty() ? std::string( "None" ) : ( slot.material.empty() ? std::string{} : slot.material + " " ) + slot.item;
		element->SetAttribute( "aria-label", slot.label + ": " + item );
		element->SetInnerRML( "<span class='w98-cell w98-w-slot'>" + esc( slot.label ) + "</span><span class='w98-cell w98-cell--grow'>" + esc( item ) + "</span>" );
	}
	text( "creature_preview_equipment_scope", editable
		? "This is the uniform of the " + ( creature.equipmentRoleName.empty() ? std::string( "assigned" ) : creature.equipmentRoleName ) + " role. A change applies to every citizen with this role."
		: "This citizen has no military role, so there is no uniform to change. Assign a role in the Military window." );
	visible( "creature_preview_equipment_editor", editable && state.equipmentSlotEditor.has_value() );
	if( !editable || !state.equipmentSlotEditor ) return;
	const auto slot = std::find_if( creature.equipmentSlots.begin(), creature.equipmentSlots.end(), [&state]( const EquipmentSlotState& value ) { return value.slot == *state.equipmentSlotEditor; } );
	if( slot == creature.equipmentSlots.end() ) return;
	text( "creature_preview_equipment_editor_title", "Change " + slot->label );
	std::vector<std::pair<std::string, std::string>> types;
	for( const auto& choice : slot->choices ) types.emplace_back( choice.type.value, choice.type.value == "none" ? std::string( "Empty" ) : displayName( choice.type.value ) );
	const bool typeKnown = std::any_of( types.begin(), types.end(), [&state]( const auto& option ) { return option.first == state.equipmentDraftType.value; } );
	auto* typeSelect = document_->GetElementById( "creature_preview_equipment_type" );
	auto typeKey = types;
	if( !typeKnown ) typeKey.emplace_back( "", "" );
	if( typeKey != renderedTypeOptions_ )
	{
		setSelectOptions( typeSelect, types, !typeKnown );
		renderedTypeOptions_ = std::move( typeKey );
	}
	setSelectValue( typeSelect, typeKnown ? state.equipmentDraftType.value : std::string{} );
	std::vector<std::pair<std::string, std::string>> materials;
	const auto type = std::find_if( slot->choices.begin(), slot->choices.end(), [&state]( const EquipmentTypeChoice& value ) { return value.type == state.equipmentDraftType; } );
	if( type != slot->choices.end() )
		for( const auto& material : type->materials ) materials.emplace_back( material.value, material.value == "any" ? std::string( "Any" ) : displayName( material.value ) );
	const auto draftMaterial = state.equipmentDraftMaterial ? state.equipmentDraftMaterial->value : std::string{};
	const bool materialKnown = std::any_of( materials.begin(), materials.end(), [&draftMaterial]( const auto& option ) { return option.first == draftMaterial; } );
	auto* materialSelect = document_->GetElementById( "creature_preview_equipment_material" );
	auto materialKey = materials;
	if( !materialKnown ) materialKey.emplace_back( "", "" );
	if( materialKey != renderedMaterialOptions_ )
	{
		setSelectOptions( materialSelect, materials, !materialKnown );
		renderedMaterialOptions_ = std::move( materialKey );
	}
	setSelectValue( materialSelect, materialKnown ? draftMaterial : std::string{} );
	setDisabled( materialSelect, materials.empty() );
	setDisabled( document_->GetElementById( "creature_preview_equipment_apply" ), state.equipmentDraftType.value.empty() );
}

bool InspectorRmlBinding::previewPageAvailable( PreviewPage page ) const
{
	if( page == PreviewPage::Camera ) return true;
	if( !controller_ || !controller_->state().creature ) return false;
	const auto& creature = *controller_->state().creature;
	switch( page )
	{
	case PreviewPage::Camera: return true;
	case PreviewPage::Stats:
		return std::any_of( creature.attributesReported.begin(), creature.attributesReported.end(), []( bool reported ) { return reported; } )
			|| std::any_of( creature.needsReported.begin(), creature.needsReported.end(), []( bool reported ) { return reported; } );
	case PreviewPage::Expertise: return creature.professionReported || creature.skillsReported;
	case PreviewPage::Equipment: return creature.equipmentReported;
	case PreviewPage::Inventory: return creature.inventoryReported;
	}
	return false;
}

// Tile page: one list view (Type / Name) of everything on the tile, and command buttons that act on the tile.
// Creatures are list rows chosen by ID; Inspect (or a double-click) opens the chosen one, so on a crowded tile
// the player picks the creature instead of getting whichever is first.
void InspectorRmlBinding::renderTile( const TileInspectorState* tile )
{
	if( !document_ ) return;
	struct Command
	{
		std::string id, label, description;
		bool enabled{ true };
		std::function<void()> invoke;
	};
	std::vector<Command> commands;
	std::vector<CreatureId> creatures;
	std::string rowsMarkup;
	const std::string prefix = liveInspection_ ? "live_tile_" : "tile_";
	const auto tr = [this]( const char* key ) { return textCatalog_.format( LocalizationKey{ key } ); };
	const auto row = [&rowsMarkup]( const std::string& id, const std::string& fallbackType, const std::string& value )
	{
		const auto [type, name] = typed( fallbackType, value );
		rowsMarkup += "<div" + ( id.empty() ? std::string{} : " id='" + id + "'" ) + " class='w98-list-item c-inspector-row' role='option' aria-selected='false'><span class='w98-cell w98-w-slot'>" + esc( type )
			+ "</span><span class='w98-cell w98-cell--grow' title='" + esc( name ) + "'>" + esc( name ) + "</span></div>";
	};
	const auto command = [this, &commands, &prefix]( const char* suffix, const char* label, TileContextAction action, bool enabled = true )
	{
		commands.push_back( { prefix + suffix, label, label, enabled, [this, action] { controller_->executeContext( action ); } } );
	};
	if( tile )
	{
		const auto& t = *tile;
		if( !t.wall.empty() ) row( {}, "Wall", t.wall );
		if( !t.floor.empty() ) row( {}, "Floor", t.floor );
		if( !t.embedded.empty() ) row( {}, "Embedded", t.embedded );
		if( !t.plant.empty() ) row( {}, t.plantIsTree ? "Tree" : "Plant", t.plant );
		if( !t.water.empty() ) row( {}, "Water", t.water );
		if( !t.construction.empty() ) row( {}, "Construction", t.construction );
		if( t.wall.empty() && t.floor.empty() && t.embedded.empty() && t.plant.empty() && t.water.empty() && t.construction.empty() )
			row( {}, "Tile", tr( "inspector.live.empty_tile" ) );
		for( const auto& item : t.items ) row( {}, "Item", counted( item ) );
		if( std::none_of( t.creatures.begin(), t.creatures.end(), [this]( const CreatureRow& value ) { return value.id.value == tileCreature_; } ) )
			tileCreature_ = t.creatures.empty() ? 0 : t.creatures.front().id.value;
		for( std::size_t index = 0; index < t.creatures.size(); ++index )
		{
			row( prefix + "creature_" + std::to_string( index ), "Creature", t.creatures[index].label );
			creatures.push_back( t.creatures[index].id );
		}
		if( t.hasJob )
		{
			row( {}, "Job", t.jobName + " (" + ( t.jobWorker.empty() ? std::string( "unassigned" ) : t.jobWorker ) + ", priority " + ( t.jobPriority.empty() ? std::string( "unknown" ) : t.jobPriority ) + ")" );
			for( const auto& item : t.requiredItems ) row( {}, "Job needs", counted( item ) );
		}
		if( t.designation || !t.roomSummary.empty() || !t.mechanismSummary.empty() )
		{
			const auto detail = t.roomSummary + ( !t.roomSummary.empty() && !t.mechanismSummary.empty() ? ", " : "" ) + t.mechanismSummary;
			row( {}, "Designation", t.designationName.empty() ? detail : t.designationName + ( detail.empty() ? std::string{} : " (" + detail + ")" ) );
		}
		if( t.canMine ) command( "mine", "Mine", TileContextAction::Mine );
		if( t.canRemoveFloor )
		{
			command( "remove_floor", "Remove Floor", TileContextAction::RemoveFloor );
			if( replaceFloorHandler_ )
				commands.push_back( { prefix + "replace_floor", "Replace Floor...", tr( "inspector.live.replace_floor_hint" ), true, [this] { if( replaceFloorHandler_ ) replaceFloorHandler_(); } } );
		}
		if( t.canHarvest ) command( "harvest", "Harvest", TileContextAction::Harvest );
		if( t.canFell ) command( "fell", "Fell Tree", TileContextAction::FellTree );
		if( t.canRemovePlant ) command( "remove_plant", "Remove Plant", TileContextAction::RemovePlant );
		if( t.hasJob )
		{
			command( "cancel_job", "Cancel Job", TileContextAction::CancelJob );
			command( "raise_job", "Raise Priority", TileContextAction::RaisePriority, t.canRaisePriority );
			command( "lower_job", "Lower Priority", TileContextAction::LowerPriority, t.canLowerPriority );
		}
		if( t.canManage ) command( "manage", "Manage", TileContextAction::Manage );
		if( t.canDeleteStockpile ) command( "delete_stockpile", "Delete Stockpile", TileContextAction::DeleteStockpile );
		if( !t.creatures.empty() )
			commands.push_back( { prefix + "open_creature", "Inspect", "Inspect the creature selected in the list", true, [this] { if( tileCreature_ ) controller_->openCreature( CreatureId{ tileCreature_ } ); } } );
	}
	else
		tileCreature_ = 0;
	std::string commandMarkup;
	for( const auto& c : commands )
		commandMarkup += "<button id='" + c.id + "' class='w98-button' title='" + esc( c.description ) + "'" + ( c.enabled ? std::string{} : std::string( " disabled='disabled' aria-disabled='true'" ) ) + ">" + esc( c.label ) + "</button>";
	const std::uint32_t tileId = tile ? tile->id.value : 0;
	const bool newTile = tileId != liveTileId_;
	auto* list = document_->GetElementById( "live_tile_rows" );
	auto markup = rowsMarkup + '\x1f' + commandMarkup;
	if( markup == liveTileRowsMarkup_ )
	{
		if( newTile && list ) list->SetScrollTop( 0 );
		liveTileId_ = tileId;
		return;
	}
	const float scrollTop = list ? list->GetScrollTop() : 0.f;
	clearListeners( liveTileListeners_ );
	liveTileRowsMarkup_ = std::move( markup );
	liveTileId_ = tileId;
	rml( "live_tile_rows", rowsMarkup );
	rml( "live_tile_commands", commandMarkup );
	for( const auto& c : commands )
		if( auto* button = document_->GetElementById( c.id ) )
			bindElement( button, "click", [invoke = c.invoke]( Rml::Event& ) { invoke(); }, liveTileListeners_ );
	for( std::size_t index = 0; index < creatures.size(); ++index )
		if( auto* element = document_->GetElementById( prefix + "creature_" + std::to_string( index ) ) )
		{
			const auto value = creatures[index].value;
			// Selecting a row only restyles it; the list is not rebuilt while its own event is being dispatched.
			bindElement( element, "click", [this, value]( Rml::Event& ) { tileCreature_ = value; if( controller_ ) syncTileSelection( controller_->state().tile ? &*controller_->state().tile : nullptr ); }, liveTileListeners_ );
			bindElement( element, "dblclick", [this, value]( Rml::Event& ) { tileCreature_ = value; if( controller_ ) controller_->openCreature( CreatureId{ value } ); }, liveTileListeners_ );
		}
	if( list ) list->SetScrollTop( newTile ? 0.f : scrollTop );
}

void InspectorRmlBinding::syncTileSelection( const TileInspectorState* tile )
{
	if( !document_ || !tile ) return;
	const std::string prefix = liveInspection_ ? "live_tile_" : "tile_";
	for( std::size_t index = 0; index < tile->creatures.size(); ++index )
		if( auto* element = document_->GetElementById( prefix + "creature_" + std::to_string( index ) ) )
		{
			const bool selected = tile->creatures[index].id.value == tileCreature_;
			element->SetClass( "is-selected", selected );
			element->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
	setDisabled( document_->GetElementById( prefix + "open_creature" ), tileCreature_ == 0 );
}

void InspectorRmlBinding::setPreviewPage( PreviewPage page )
{
	if( !previewPageAvailable( page ) ) return;
	if( previewPage_ == page )
	{
		if( page == PreviewPage::Expertise && expertiseOpenedHandler_ ) expertiseOpenedHandler_();
		return;
	}
	previewPage_ = page;
	if( previewPage_ == PreviewPage::Expertise ) renderedPreviewSkillRows_.reset();
	if( controller_ ) stateChanged( controller_->state() );
	else syncPreviewPageButtons();
	if( previewPage_ == PreviewPage::Expertise && expertiseOpenedHandler_ ) expertiseOpenedHandler_();
}

void InspectorRmlBinding::cyclePreviewPage( int step )
{
	constexpr int count = 5;
	int index = static_cast<int>( previewPage_ );
	for( int n = 0; n < count; ++n )
	{
		index = ( index + step + count ) % count;
		if( previewPageAvailable( static_cast<PreviewPage>( index ) ) ) { setPreviewPage( static_cast<PreviewPage>( index ) ); return; }
	}
}

void InspectorRmlBinding::syncPreviewPageButtons()
{
	if( !document_ ) return;
	const struct { const char* button; const char* panel; PreviewPage page; } views[] = {
		{ "creature_preview_nav_camera", "creature_preview_camera_panel", PreviewPage::Camera },
		{ "creature_preview_nav_stats", "creature_preview_stats_panel", PreviewPage::Stats },
		{ "creature_preview_nav_expertise", "creature_preview_expertise_panel", PreviewPage::Expertise },
		{ "creature_preview_nav_equipment", "creature_preview_equipment_panel", PreviewPage::Equipment },
		{ "creature_preview_nav_inventory", "creature_preview_inventory_panel", PreviewPage::Inventory } };
	for( const auto& view : views )
	{
		const bool available = previewPageAvailable( view.page );
		const bool selected = available && view.page == previewPage_;
		visible( view.button, available );
		visible( view.panel, selected );
		if( auto* element = document_->GetElementById( view.button ) )
		{
			element->SetClass( "is-selected", selected );
			element->SetAttribute( "aria-selected", selected ? "true" : "false" );
			element->SetAttribute( "tab-index", selected ? "0" : "-1" );
		}
	}
}

void InspectorRmlBinding::setSkillSort( SkillSort sort )
{
	skillSort_ = sort;
	renderedPreviewSkillRows_.reset();
	syncSkillSortButtons();
	if( !lastSkillRows_.empty() ) rows( "creature_preview_skills", lastSkillRows_ );
}

// The sorted column shows the sort mark (PDF p.143): names ascend, levels and active skills come first.
void InspectorRmlBinding::syncSkillSortButtons()
{
	if( !document_ ) return;
	const struct { const char* id; const char* mark; SkillSort sort; } buttons[] = {
		{ "creature_preview_skill_sort_name", "creature_preview_skill_mark_name", SkillSort::Name },
		{ "creature_preview_skill_sort_level", "creature_preview_skill_mark_level", SkillSort::Level },
		{ "creature_preview_skill_sort_active", "creature_preview_skill_mark_active", SkillSort::Active } };
	for( const auto& button : buttons )
	{
		const bool selected = button.sort == skillSort_;
		if( auto* element = document_->GetElementById( button.id ) ) element->SetAttribute( "aria-pressed", selected ? "true" : "false" );
		if( auto* mark = document_->GetElementById( button.mark ) ) mark->SetClass( "is-hidden", !selected );
	}
}

void InspectorRmlBinding::stateChanged( const InspectorState& s )
{
	if( detachedWindow_ && qEnvironmentVariableIsSet( "INGNOMIA_TRACE_INSPECTOR_SKILLS" ) )
	{
		const auto creatureID = s.creature ? QString::number( static_cast<qulonglong>( s.creature->id.value ) ) : QStringLiteral( "-" );
		const auto creatureName = s.creature ? QString::fromStdString( s.creature->name ) : QStringLiteral( "-" );
		traceInspectorSkills( QStringLiteral( "state window=%1 camera=%2 revision=%3 kind=%4 creatureId=%5 name=%6 rows=%7" )
			.arg( windowIndex_ ).arg( cameraSlot_ ).arg( s.revision.value ).arg( static_cast<int>( s.kind ) ).arg( creatureID ).arg( creatureName )
			.arg( s.creature ? s.creature->skills.size() : 0 ) );
	}
	if( !document_ ) return;
	rendering_ = true;
	const auto tr = [this]( const char* key, std::initializer_list<localization::TextArgument> args = {} ) { return textCatalog_.format( LocalizationKey{ key }, args ); };
	const auto label = [this]( const char* key, const char* fallback ) { auto value = textCatalog_.format( LocalizationKey{ key } ); return value.empty() || value.rfind( "⟦", 0 ) == 0 ? std::string( fallback ) : value; };

	// ---- Which window and page is showing.
	const bool pendingBlueprint = !liveInspection_ && s.kind == InspectorKind::Tile && s.tile && s.tile->hasJob && s.tile->jobName.rfind( "Build", 0 ) == 0;
	const bool stockpileInspection = s.kind == InspectorKind::Stockpile && s.stockpile.has_value();
	const bool tileView = s.kind == InspectorKind::Tile && s.tile.has_value() && !pendingBlueprint;
	if( auto* panel = document_->GetElementById( "inspector_panel" ) )
	{
		panel->SetClass( "is-live-tile", liveInspection_ );
		panel->SetClass( "is-blueprint", pendingBlueprint );
		panel->SetClass( "is-stockpile", stockpileInspection );
	}
	std::string tileLabel;
	if( s.tile ) tileLabel = !s.tile->water.empty() ? s.tile->water : !s.tile->wall.empty() ? s.tile->wall : s.tile->construction;
	const bool tileWorldLabel = false;
	const bool selectionTip = s.selection.active && !s.selection.sizeLabel.empty();
	const bool creaturePreview = s.kind == InspectorKind::Creature && s.creature && !s.creatureDetailsOpen;
	const bool open = liveInspection_ || s.kind != InspectorKind::None;
	visible( "inspector_root", open || tileWorldLabel || selectionTip );
	visible( "inspector_panel", open && !creaturePreview );
	visible( "tile_inspection_empty", liveInspection_ && !s.tile );
	visible( "creature_preview", creaturePreview );
	visible( "tile_world_label", tileWorldLabel );
	visible( "selection_pointer_tip", selectionTip );
	text( "tile_world_label_text", tileLabel );
	visible( "inspector_blueprint", pendingBlueprint );
	visible( "inspector_tile", tileView );
	visible( "inspector_workshop", s.kind == InspectorKind::Workshop );
	visible( "inspector_stockpile", stockpileInspection );
	visible( "inspector_agriculture", s.kind == InspectorKind::Agriculture );
	visible( "selection_configuration", false );
	positionTileLabel( s.selection );
	positionSelectionTip( s.selection );
	renderTile( tileView ? &*s.tile : nullptr );
	syncTileSelection( tileView ? &*s.tile : nullptr );

	// ---- Creature inspector.
	const std::uint32_t creatureId = s.creature ? s.creature->id.value : 0;
	if( creatureId != previewCreatureId_ )
	{
		previewCreatureId_ = creatureId;
		previewPage_ = PreviewPage::Camera;
	}
	if( !previewPageAvailable( previewPage_ ) ) previewPage_ = PreviewPage::Camera;
	const bool skillsOpen = previewPage_ == PreviewPage::Expertise;
	if( detachedWindow_ && skillsOpen && !lastSkillPanelOpen_ ) renderedPreviewSkillRows_.reset();
	syncPreviewPageButtons();
	if( s.creature )
	{
		const auto& c = *s.creature;
		const bool gnome = c.professionReported || c.skillsReported;
		text( "creature_preview_title", ( c.name.empty() ? std::string( gnome ? "Gnome" : "Creature" ) : c.name ) + " Properties" );
		text( "creature_preview_kind", gnome ? label( "inspector.preview.gnome", "Gnome" ) : label( "inspector.preview.creature", "Creature" ) );
		text( "creature_preview_activity", c.activity.empty() ? std::string( "Unknown" ) : c.activity );
		text( "creature_preview_locate_label", "Center on Map" );
		const std::int32_t attributes[] = { c.strength, c.dexterity, c.constitution, c.intelligence, c.wisdom, c.charisma };
		const char* attributeIds[] = { "creature_preview_strength", "creature_preview_dexterity", "creature_preview_constitution", "creature_preview_intelligence", "creature_preview_wisdom", "creature_preview_charisma" };
		for( std::size_t index = 0; index < 6; ++index ) text( attributeIds[index], unknownOr( c.attributesReported[index], attributes[index] ) );
		const std::int32_t needs[] = { c.hunger, c.thirst, c.sleep, c.happiness };
		const char* needIds[] = { "creature_preview_hunger", "creature_preview_thirst", "creature_preview_sleep", "creature_preview_happiness" };
		for( std::size_t index = 0; index < 4; ++index ) text( needIds[index], unknownOr( c.needsReported[index], needs[index] ) );
		const bool hasCreatureStats = std::any_of( c.attributesReported.begin(), c.attributesReported.end(), []( bool v ) { return v; } )
			|| std::any_of( c.needsReported.begin(), c.needsReported.end(), []( bool v ) { return v; } );
		visible( "creature_preview_stats_empty", !hasCreatureStats );
		rows( "creature_preview_skills", c.skills );
		visible( "creature_preview_skill_sort", !c.skills.empty() || !lastSkillRows_.empty() );
		visible( "creature_preview_skills_empty", c.skills.empty() );
		renderEquipment( s );
		rows( "creature_preview_inventory", c.inventory );
		visible( "creature_preview_inventory_empty", !c.inventoryReported || c.inventory.empty() );
		// Profession drop-down: the game's professions; a blank entry only while the current one is not among them.
		const std::vector<std::string> professionOptions = c.professionReported ? s.professionChoices : std::vector<std::string>{};
		const bool professionKnown = std::find( professionOptions.begin(), professionOptions.end(), c.profession ) != professionOptions.end();
		auto* professionSelect = document_->GetElementById( "creature_preview_profession" );
		if( renderedProfessionChoices_ != professionOptions || renderedProfessionKnown_ != professionKnown )
		{
			std::vector<std::pair<std::string, std::string>> options;
			for( const auto& profession : professionOptions ) options.emplace_back( profession, profession );
			setSelectOptions( professionSelect, options, !professionKnown );
			renderedProfessionChoices_ = professionOptions;
			renderedProfessionKnown_ = professionKnown;
		}
		setSelectValue( professionSelect, professionKnown ? c.profession : std::string{} );
		setDisabled( professionSelect, professionOptions.empty() );
		visible( "creature_preview_professions_empty", c.professionReported && s.professionChoices.empty() );
	}
	else
		clearListeners( equipmentListeners_ );

	// ---- Object inspector.
	std::string title = "Properties", kind = label( "inspector.static.inspector_kind", "Selection" ), pos, coords;
	if( s.selected && s.selected->position )
	{
		coords = coordinates( *s.selected->position );
		pos = "at " + coords;
	}
	visible( "inspector_locate", s.selected && s.selected->position.has_value() );
	if( s.tile )
	{
		const auto& t = *s.tile;
		title = "Tile Properties";
		kind = t.designationName.empty() ? std::string( "Tile" ) : t.designationName;
		std::string pointerDetail;
		for( const auto* detail : { &t.floor, &t.water, &t.wall, &t.construction } )
			if( !detail->empty() ) pointerDetail += ( pointerDetail.empty() ? "" : ", " ) + *detail;
		text( "selection_pointer_tool", kind );
		text( "selection_pointer_size", pointerDetail );
		visible( "selection_pointer_size", !pointerDetail.empty() );
	}
	if( pendingBlueprint && s.tile )
	{
		const auto& t = *s.tile;
		title = t.jobName == "BuildWorkshop" ? "Workshop Blueprint Properties" : "Construction Blueprint Properties";
		kind = label( "inspector.static.blueprint_kind", "Construction" );
		std::string missingRows;
		bool waiting = false;
		for( const auto& requirement : t.requiredItems )
			if( !requirement.available )
			{
				waiting = true;
				const auto item = displayName( requirement.label );
				const auto material = displayName( requirement.detail.empty() ? std::string_view{ "any" } : std::string_view{ requirement.detail } );
				missingRows += "<div class='w98-list-item c-blueprint-row is-missing' role='option'><span class='w98-cell w98-cell--grow c-blueprint-row__item'>" + esc( item ) + "</span><span class='w98-cell w98-w-profession c-blueprint-row__material'>" + esc( material )
					+ "</span><span class='w98-cell w98-cell--num w98-w-amount c-blueprint-row__needed'>" + std::to_string( requirement.count ) + "</span></div>";
			}
		rml( "blueprint_missing_items", missingRows );
		visible( "blueprint_missing_items", waiting );
		visible( "blueprint_resources_ready", !waiting );
		text( "blueprint_status", waiting ? "Waiting for materials." : "Materials are available; waiting for a builder." );
		text( "blueprint_worker", t.jobWorker.empty() ? std::string( "Unassigned" ) : t.jobWorker );
		text( "blueprint_priority", t.jobPriority.empty() ? std::string( "Unknown" ) : t.jobPriority );
		text( "blueprint_skill", t.requiredSkill );
		visible( "blueprint_skill_row", !t.requiredSkill.empty() );
		text( "blueprint_tool", t.requiredTool + ( t.requiredTool.empty() ? "" : " (" + ( t.requiredToolAvailable.empty() ? std::string( "availability unknown" ) : t.requiredToolAvailable ) + ")" ) );
		visible( "blueprint_tool_row", !t.requiredTool.empty() );
		setDisabled( document_->GetElementById( "blueprint_raise_job" ), !t.canRaisePriority );
		setDisabled( document_->GetElementById( "blueprint_lower_job" ), !t.canLowerPriority );
	}
	if( s.creature )
	{
		title = ( s.creature->name.empty() ? std::string( "Creature" ) : s.creature->name ) + " Properties";
		kind = "Creature";
	}
	if( s.workshop )
	{
		const auto& w = *s.workshop;
		title = w.name + " Properties";
		kind = "Workshop";
		text( "workshop_state", w.suspended ? "Suspended" : "Active" );
		text( "workshop_priority", std::to_string( w.priority ) + " of " + std::to_string( w.maxPriority ) );
		text( "workshop_products", std::to_string( w.productCount ) );
		text( "workshop_queued", std::to_string( w.queuedJobs ) );
		text( "workshop_generated", w.acceptGenerated ? "Accepted" : "Not accepted" );
		text( "workshop_auto", w.autoCraftMissing ? "On" : "Off" );
		text( "workshop_link", w.linkedStockpile ? "Linked" : "Not linked" );
		text( "workshop_toggle_suspended", w.suspended ? "Resume" : "Suspend" );
	}
	if( s.stockpile )
	{
		const auto& v = *s.stockpile;
		title = v.name + " Properties";
		kind = "Stockpile";
		text( "stockpile_status", v.suspended ? "Suspended" : "Active" );
		text( "stockpile_priority", std::to_string( v.priority + 1 ) + " of " + std::to_string( v.maxPriority ) );
		text( "stockpile_capacity", std::to_string( v.itemCount ) + " of " + std::to_string( v.capacity ) );
		text( "stockpile_reserved", std::to_string( v.reserved ) );
		text( "stockpile_pull_from_others", v.pullFromOthers ? "Yes" : "No" );
		text( "stockpile_allow_pull", v.allowPullFromHere ? "Yes" : "No" );
		std::string contents;
		for( const auto& row : v.contents )
		{
			const auto icon = row.icon.empty() ? std::string{ "<span class='c-stockpile-row__icon c-stockpile-row__icon--empty' aria-hidden='true'></span>" }
				: "<span class='c-stockpile-row__icon' aria-hidden='true'><img src='" + esc( row.icon ) + "' /></span>";
			contents += "<div class='w98-list-item c-stockpile-row' role='option'><span class='w98-cell w98-cell--grow c-stockpile-row__item'>" + icon + "<span class='c-stockpile-row__item-label'>" + esc( displayName( row.label ) )
				+ "</span></span><span class='w98-cell w98-w-profession c-stockpile-row__material'>" + esc( row.detail.empty() ? std::string{ "Any" } : displayName( row.detail ) )
				+ "</span><span class='w98-cell w98-cell--num w98-w-amount c-stockpile-row__quantity'>" + std::to_string( row.count ) + "</span></div>";
		}
		rml( "stockpile_contents", contents );
		visible( "stockpile_contents_empty", v.contents.empty() );
		text( "stockpile_toggle_suspended", v.suspended ? "Resume" : "Suspend" );
	}
	if( s.agriculture )
	{
		const auto& a = *s.agriculture;
		const bool grove = a.target.kind == AgricultureKind::Grove;
		kind = a.target.kind == AgricultureKind::Farm ? "Farm" : a.target.kind == AgricultureKind::Pasture ? "Pasture" : "Grove";
		title = a.name + " Properties";
		text( "agriculture_kind", kind );
		text( "agriculture_state", a.suspended ? "Suspended" : "Active" );
		text( "agriculture_priority", std::to_string( a.priority ) + " of " + std::to_string( a.maxPriority ) );
		text( "agriculture_product", a.product.empty() ? std::string( "None" ) : a.product );
		text( "agriculture_plots", std::to_string( a.plots ) );
		text( "agriculture_planted", std::to_string( a.planted ) );
		text( "agriculture_ready", std::to_string( a.ready ) );
		text( "agriculture_harvest", grove ? ( a.pick ? "Pick fruit" : "Do not pick" ) : ( a.harvest ? "On" : "Off" ) );
		text( "agriculture_toggle_suspended", a.suspended ? "Resume" : "Suspend" );
		text( "agriculture_toggle_primary", grove ? ( a.pick ? "Stop Picking" : "Pick Fruit" ) : ( a.harvest ? "Stop Harvest" : "Harvest" ) );
	}
	text( "inspector_title", title );
	text( "inspector_kind", kind );
	text( "inspector_position", pos );
	text( "creature_preview_camera_position", coords.empty() ? tr( "inspector.preview.camera_unavailable" ) : coords );
	visible( "inspector_back", s.kind == InspectorKind::Creature && ( s.previous.has_value() || s.creatureDetailsOpen ) );
	visible( "inspector_refresh", s.kind != InspectorKind::None && !pendingBlueprint && !stockpileInspection );
	text( "selection_action", "Active tool: " + s.selection.actionLabel );
	std::string geometry = "Size " + ( s.selection.sizeLabel.empty() ? std::string( "not anchored" ) : s.selection.sizeLabel );
	if( s.selection.cursor ) geometry += ", cursor at " + coordinates( *s.selection.cursor );
	text( "selection_geometry", geometry );
	visible( "selection_rotate", s.selection.canRotate );
	if( s.selection.active )
	{
		text( "selection_pointer_tool", s.selection.actionLabel );
		text( "selection_pointer_size", "Size " + s.selection.sizeLabel );
		visible( "selection_pointer_size", !s.selection.sizeLabel.empty() );
	}
	text( "inspector_status", s.status );
	lastSkillPanelOpen_ = skillsOpen;
	rendering_ = false;
}
} // namespace ingnomia::ui::inspector
