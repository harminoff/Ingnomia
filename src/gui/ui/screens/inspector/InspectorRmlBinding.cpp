/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorRmlBinding.h"
#include "../../localization/RmlText.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
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

std::string equipmentAbbreviation( UniformSlot slot )
{
	switch( slot )
	{
		case UniformSlot::HeadArmor: return "H";
		case UniformSlot::ChestArmor: return "C";
		case UniformSlot::ArmArmor: return "A";
		case UniformSlot::HandArmor: return "G";
		case UniformSlot::LegArmor: return "L";
		case UniformSlot::FootArmor: return "F";
		case UniformSlot::LeftHandHeld: return "LH";
		case UniformSlot::RightHandHeld: return "RH";
		case UniformSlot::Back: return "B";
	}
	return "?";
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

 void InspectorRmlBinding::Callback::ProcessEvent(Rml::Event&e){fn_(e);}
InspectorRmlBinding::InspectorRmlBinding(Rml::Context&c,int cameraSlot,int windowIndex,bool detachedWindow):context_(c),cameraSlot_(cameraSlot),windowIndex_(windowIndex),detachedWindow_(detachedWindow){cameraSource_=cameraSlot_==0?"?camera-preview://selected":"?camera-preview://slot-"+std::to_string(cameraSlot_);} InspectorRmlBinding::~InspectorRmlBinding(){shutdown();}
bool InspectorRmlBinding::initialize(InspectorController&c){controller_=&c;document_=documentLoader_?documentLoader_():context_.LoadDocument("screens/inspector.rml");if(!document_)return false;localization::applyRmlText(*document_,textCatalog_);if(auto*image=document_->GetElementById("creature_preview_camera_image"))image->SetAttribute("src",cameraSource_);if(detachedWindow_){if(auto*preview=document_->GetElementById("creature_preview"))preview->SetClass("is-detached",true);if(auto*panel=document_->GetElementById("inspector_panel"))panel->SetClass("is-detached",true);text("creature_preview_close_label","X");text("inspector_close_label","X");}if(windowIndex_>0)if(auto*preview=document_->GetElementById("creature_preview")){const int index=windowIndex_-1;preview->SetProperty("right",std::to_string(16+(index%3)*380)+"dp");preview->SetProperty("bottom",std::to_string(62+(index/3)*42)+"dp");}
	bind("inspector_back",[this]{controller_->back();});bind("inspector_close",[this]{controller_->close();if(closeHandler_)closeHandler_();});bind("inspector_locate",[this]{controller_->locate();});bind("inspector_refresh",[this]{controller_->refresh();});bind("creature_preview_close",[this]{controller_->close();if(closeHandler_)closeHandler_();});bind("creature_preview_nav_camera",[this]{setPreviewPage(PreviewPage::Camera);});bind("creature_preview_nav_stats",[this]{setPreviewPage(PreviewPage::Stats);});bind("creature_preview_nav_expertise",[this]{setPreviewPage(PreviewPage::Expertise);});bind("creature_preview_profession_toggle",[this]{professionMenuOpen_=!professionMenuOpen_;if(controller_)stateChanged(controller_->state());});bind("creature_preview_nav_equipment",[this]{setPreviewPage(PreviewPage::Equipment);});bind("creature_preview_nav_inventory",[this]{setPreviewPage(PreviewPage::Inventory);});bind("creature_preview_locate",[this]{controller_->locate();});bind("creature_preview_skill_sort_name",[this]{setSkillSort(SkillSort::Name);});bind("creature_preview_skill_sort_level",[this]{setSkillSort(SkillSort::Level);});bind("creature_preview_skill_sort_active",[this]{setSkillSort(SkillSort::Active);});syncSkillSortButtons();
	bind("creature_equipment_slot_head",[this]{controller_->selectEquipmentSlot(UniformSlot::HeadArmor);});bind("creature_equipment_slot_chest",[this]{controller_->selectEquipmentSlot(UniformSlot::ChestArmor);});bind("creature_equipment_slot_arms",[this]{controller_->selectEquipmentSlot(UniformSlot::ArmArmor);});bind("creature_equipment_slot_hands",[this]{controller_->selectEquipmentSlot(UniformSlot::HandArmor);});bind("creature_equipment_slot_legs",[this]{controller_->selectEquipmentSlot(UniformSlot::LegArmor);});bind("creature_equipment_slot_feet",[this]{controller_->selectEquipmentSlot(UniformSlot::FootArmor);});bind("creature_equipment_slot_left_hand",[this]{controller_->selectEquipmentSlot(UniformSlot::LeftHandHeld);});bind("creature_equipment_slot_right_hand",[this]{controller_->selectEquipmentSlot(UniformSlot::RightHandHeld);});bind("creature_equipment_slot_back",[this]{controller_->selectEquipmentSlot(UniformSlot::Back);});bind("creature_preview_equipment_apply",[this]{controller_->applyEquipmentSlot();});bind("creature_preview_equipment_cancel",[this]{controller_->closeEquipmentEditor();});
	bind("tile_open_creature",[this]{if(controller_->state().tile&&!controller_->state().tile->creatures.empty())controller_->openCreature(controller_->state().tile->creatures.front().id);});
	bind("tile_delete_stockpile",[this]{controller_->executeContext(TileContextAction::DeleteStockpile);});
	bind("tile_mine",[this]{controller_->executeContext(TileContextAction::Mine);});bind("tile_remove_floor",[this]{controller_->executeContext(TileContextAction::RemoveFloor);});bind("tile_harvest",[this]{controller_->executeContext(TileContextAction::Harvest);});bind("tile_fell",[this]{controller_->executeContext(TileContextAction::FellTree);});bind("tile_remove_plant",[this]{controller_->executeContext(TileContextAction::RemovePlant);});bind("tile_manage",[this]{controller_->executeContext(TileContextAction::Manage);});bind("tile_cancel_job",[this]{controller_->executeContext(TileContextAction::CancelJob);});bind("tile_raise_job",[this]{controller_->executeContext(TileContextAction::RaisePriority);});bind("tile_lower_job",[this]{controller_->executeContext(TileContextAction::LowerPriority);});
	bind("blueprint_cancel_job",[this]{controller_->executeContext(TileContextAction::CancelJob);});bind("blueprint_raise_job",[this]{controller_->executeContext(TileContextAction::RaisePriority);});bind("blueprint_lower_job",[this]{controller_->executeContext(TileContextAction::LowerPriority);});
	bind("workshop_toggle_suspended",[this]{controller_->toggleWorkshopSuspended();});bind("stockpile_toggle_suspended",[this]{controller_->toggleStockpileSuspended();});bind("agriculture_toggle_suspended",[this]{controller_->toggleAgricultureSuspended();});bind("agriculture_toggle_primary",[this]{controller_->toggleAgriculturePrimaryOption();});bind("selection_cancel",[this]{controller_->cancelSelection();});bind("selection_rotate",[this]{controller_->rotateSelection();});
	stateChanged(c.state());document_->Show();return true;}
	bool InspectorRmlBinding::reloadDocument( InspectorController& c, bool preserveUiState )
	{
		const auto previewPage = previewPage_;
		const auto skillSort = skillSort_;
		const auto skillScrollOffset = skillScrollOffset_;
		const bool professionMenuOpen = professionMenuOpen_;
		clearListeners( equipmentListeners_ );
		clearListeners( professionListeners_ );
		clearListeners( liveTileListeners_ );
		clearListeners( listeners_ );
		liveTileRowsMarkup_.clear();
		liveTileId_ = 0;
		lastSkillRows_.clear();
		renderedProfessionChoices_.clear();
		lastSkillCreatureId_ = 0;
		skillScrollOffset_ = preserveUiState ? skillScrollOffset : 0;
		renderedPreviewSkillRows_.reset();
		renderedFullSkillRows_.reset();
		previewPage_ = preserveUiState ? previewPage : PreviewPage::Camera;
		previewCreatureId_ = 0;
		skillSort_ = preserveUiState ? skillSort : SkillSort::Name;
		lastSkillPanelOpen_ = false;
		professionMenuOpen_ = preserveUiState && professionMenuOpen;
		if ( document_ ) { context_.UnloadDocument( document_ ); document_ = nullptr; }
		return initialize( c );
	}
bool InspectorRmlBinding::activateElement(std::string_view id){if(!document_)return false;if(auto*e=document_->GetElementById(std::string(id))){if(e->HasAttribute("disabled")||!e->IsVisible(true))return false;e->DispatchEvent("click",Rml::Dictionary{});return true;}return false;}
bool InspectorRmlBinding::scrollLiveTile( int x, int y, float step )
{
	if ( !liveInspection_ || !document_ ) return false;
	auto* scroll = document_->GetElementById( "inspector_scroll" );
	if ( !scroll ) return false;
	for ( auto* element = context_.GetElementAtPoint( { static_cast<float>( x ), static_cast<float>( y ) } );
		element; element = element->GetParentNode() )
	{
		if ( element != scroll ) continue;
		const float limit = std::max( 0.f, scroll->GetScrollHeight() - scroll->GetClientHeight() );
		scroll->SetScrollTop( std::clamp( scroll->GetScrollTop() - step, 0.f, limit ) );
		return true;
	}
	return false;
}
void InspectorRmlBinding::shutdown(){clearListeners(equipmentListeners_);clearListeners(professionListeners_);clearListeners(liveTileListeners_);clearListeners(listeners_);liveTileRowsMarkup_.clear();liveTileId_=0;if(document_){context_.UnloadDocument(document_);document_=nullptr;}controller_=nullptr;closeHandler_={};documentLoader_={};}
void InspectorRmlBinding::bind(const char*id,std::function<void()>fn){bindEvent(id,"click",[fn=std::move(fn)](Rml::Event&)mutable{fn();});}
void InspectorRmlBinding::bindEvent(const char*id,const char*event,std::function<void(Rml::Event&)>fn){if(auto*e=document_->GetElementById(id))bindElement(e,event,std::move(fn),listeners_);}
void InspectorRmlBinding::bindElement(Rml::Element*e,const char*event,std::function<void(Rml::Event&)>fn,std::vector<Listener>&store){if(!e)return;auto cb=std::make_unique<Callback>(std::move(fn));e->AddEventListener(event,cb.get());store.push_back({e,event,std::move(cb)});}
void InspectorRmlBinding::clearListeners(std::vector<Listener>&store){for(auto&l:store)if(l.target)l.target->RemoveEventListener(l.event.c_str(),l.callback.get());store.clear();}
void InspectorRmlBinding::text(const char*id,const std::string&v){if(auto*e=document_->GetElementById(id))e->SetInnerRML(Rml::StringUtilities::EncodeRml(v));}
void InspectorRmlBinding::rml(const char*id,const std::string&v){if(auto*e=document_->GetElementById(id))e->SetInnerRML(v);}
 void InspectorRmlBinding::visible(const char*id,bool v){if(!document_)return;const auto element=std::string_view(id?id:"");if(!v&&!lastSkillRows_.empty()&&(element=="creature_preview_skills"||element=="creature_skills"))v=true;if(lastSkillRows_.size()&& (element=="creature_preview_skills_empty"||element=="creature_skills_empty"))v=false;if(auto*e=document_->GetElementById(id)){e->SetClass("is-hidden",!v);if(v)e->RemoveProperty("display");else e->SetProperty("display","none");}}
void InspectorRmlBinding::positionTileLabel(const SelectionConfigurationState&s){if(!document_)return;if(auto*e=document_->GetElementById("tile_world_label")){const auto p=s.pointer.value_or(PointerPosition{16,16});e->SetProperty("left",std::to_string(p.x-42)+"dp");e->SetProperty("top",std::to_string(p.y-10)+"dp");}}
void InspectorRmlBinding::positionSelectionTip(const SelectionConfigurationState&s){if(!document_)return;if(auto*e=document_->GetElementById("selection_pointer_tip")){const auto p=s.pointer.value_or(PointerPosition{16,16});e->SetProperty("left",std::to_string(p.x+14)+"dp");e->SetProperty("top",std::to_string(p.y+18)+"dp");}}
 void InspectorRmlBinding::rows(const char*id,const std::vector<TextCountRow>&values)
 {
	if(!document_)return;
	const auto element=std::string_view(id?id:"");
	const bool skillRows=element=="creature_preview_skills"||element=="creature_skills";
	const bool previewSkillRows=element=="creature_preview_skills";
	std::uint32_t creatureID=0;
	if(skillRows&&controller_&&controller_->state().creature)
	{
		creatureID=controller_->state().creature->id.value;
		if(creatureID!=lastSkillCreatureId_)
		{
			lastSkillCreatureId_=creatureID;
			lastSkillRows_.clear();
			skillScrollOffset_=0;
			renderedPreviewSkillRows_.reset();
			renderedFullSkillRows_.reset();
		}
	}
	const auto*renderValues=&values;
	bool usedCache=false;
	if(skillRows)
	{
		if(!values.empty() && &values!=&lastSkillRows_)lastSkillRows_=values;
		else if(values.empty()&&!lastSkillRows_.empty()){renderValues=&lastSkillRows_;usedCache=true;}
	}
	std::vector<TextCountRow> sortedValues;
	if(previewSkillRows)
	{
		sortedValues=*renderValues;
		const auto byName=[](const TextCountRow&left,const TextCountRow&right){return asciiLower(left.label)<asciiLower(right.label);};
		switch(skillSort_)
		{
		case SkillSort::Name:
			std::stable_sort(sortedValues.begin(),sortedValues.end(),byName);
			break;
		case SkillSort::Level:
			std::stable_sort(sortedValues.begin(),sortedValues.end(),[&byName](const TextCountRow&left,const TextCountRow&right){const int leftLevel=skillLevel(left.detail);const int rightLevel=skillLevel(right.detail);return leftLevel!=rightLevel?leftLevel>rightLevel:byName(left,right);});
			break;
		case SkillSort::Active:
			std::stable_sort(sortedValues.begin(),sortedValues.end(),[&byName](const TextCountRow&left,const TextCountRow&right){const bool leftActive=skillActive(left.detail);const bool rightActive=skillActive(right.detail);return leftActive!=rightActive?leftActive>rightActive:byName(left,right);});
			break;
		}
		renderValues=&sortedValues;
	}
	auto*e=document_->GetElementById(id);
	if(!e)return;
	std::size_t first=0;
	std::size_t last=renderValues->size();
	std::string r;
	for(std::size_t index=first;index<last;++index)
	{
		const auto&v=(*renderValues)[index];
		const bool blueprintRequirement=element=="blueprint_missing_items";
		r+="<div class='c-inspector-row"+std::string(blueprintRequirement&&!v.available?" is-missing":"")+"'><span class='c-inspector-row__label'>"+Rml::StringUtilities::EncodeRml(v.label)+"</span>";
		if(!v.detail.empty())r+="<span class='c-inspector-row__detail'>"+Rml::StringUtilities::EncodeRml(v.detail)+"</span>";
		if(v.count)r+="<span class='c-inspector-row__count'>x"+std::to_string(v.count)+"</span>";
		r+="</div>";
	}
	bool skippedUnchanged=false;
	if(skillRows)
	{
		auto&rendered=previewSkillRows?renderedPreviewSkillRows_:renderedFullSkillRows_;
		if(rendered&&*rendered==r)skippedUnchanged=true;else rendered=r;
	}
	if(!skippedUnchanged)e->SetInnerRML(r);
	if(skillRows)
	{
		const auto creatureName=controller_&&controller_->state().creature?QString::fromStdString(controller_->state().creature->name):QStringLiteral("-");
		const auto firstLabel=first<renderValues->size()?QString::fromStdString((*renderValues)[first].label):QStringLiteral("-");
		traceInspectorSkills(QStringLiteral("rows window=%1 camera=%2 element=%3 creatureId=%4 name=%5 source=%6 rendered=%7 cache=%8 usedCache=%9 unchanged=%10 children=%11 first=%12 offset=%13").arg(windowIndex_).arg(cameraSlot_).arg(QString::fromUtf8(id?id:"-")).arg(static_cast<qulonglong>(creatureID)).arg(creatureName).arg(values.size()).arg(last-first).arg(lastSkillRows_.size()).arg(usedCache?QStringLiteral("true"):QStringLiteral("false")).arg(skippedUnchanged?QStringLiteral("true"):QStringLiteral("false")).arg(e->GetNumChildren()).arg(firstLabel).arg(skillScrollOffset_));
	}
 }

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
		if( editable ) element->RemoveAttribute( "disabled" ); else element->SetAttribute( "disabled", "disabled" );
		element->SetAttribute( "aria-pressed", selected ? "true" : "false" );
		std::string description = slot.label;
		if( !slot.item.empty() ) description += ": " + ( slot.material.empty() ? std::string{} : slot.material + " " ) + slot.item;
		else description += ": empty";
		element->SetAttribute( "aria-label", description );
		element->SetAttribute( "title", editable ? description + ". Click to change." : description );
		std::string content = "<span class='c-equipment-slot__icon'>";
		if( !slot.icon.empty() ) content += "<img src='" + Rml::StringUtilities::EncodeRml( slot.icon ) + "' />";
		else content += "<span class='c-equipment-slot__empty'>" + equipmentAbbreviation( slot.slot ) + "</span>";
		content += "</span><span class='c-equipment-slot__label'>" + Rml::StringUtilities::EncodeRml( slot.label ) + "</span>";
		element->SetInnerRML( content );
	}
	text( "creature_preview_equipment_scope", editable
		? "Controlled by the " + ( creature.equipmentRoleName.empty() ? std::string( "assigned" ) : creature.equipmentRoleName ) + " role. Changes apply to every gnome with this role."
		: "This gnome has no military role. Assign one before choosing equipment." );
	visible( "creature_preview_equipment_empty", !editable );
	visible( "creature_preview_equipment_editor", editable && state.equipmentSlotEditor.has_value() );
	if( !editable || !state.equipmentSlotEditor ) return;
	const auto slot = std::find_if( creature.equipmentSlots.begin(), creature.equipmentSlots.end(), [&state]( const EquipmentSlotState& value ) { return value.slot == *state.equipmentSlotEditor; } );
	if( slot == creature.equipmentSlots.end() ) return;
	text( "creature_preview_equipment_editor_title", "Change " + slot->label );
	text( "creature_preview_equipment_current", slot->item.empty() ? "Currently empty" : "Wearing: " + ( slot->material.empty() ? std::string{} : slot->material + " " ) + slot->item );
	std::string types;
	for( std::size_t index = 0; index < slot->choices.size(); ++index )
	{
		const auto& choice = slot->choices[index];
		const bool selected = choice.type == state.equipmentDraftType;
		const auto label = choice.type.value == "none" ? std::string( "Empty" ) : choice.type.value;
		types += "<button id='creature_equipment_type_" + std::to_string( index ) + "' class='c-button" + ( selected ? " is-selected" : "" ) + "' aria-pressed='" + ( selected ? "true" : "false" ) + "'>" + Rml::StringUtilities::EncodeRml( label ) + "</button>";
	}
	rml( "creature_preview_equipment_types", types );
	for( std::size_t index = 0; index < slot->choices.size(); ++index )
		if( auto* choice = document_->GetElementById( "creature_equipment_type_" + std::to_string( index ) ) )
		{
			const auto type = slot->choices[index].type;
			bindElement( choice, "click", [this,type]( Rml::Event& ) { controller_->setEquipmentDraftType( type ); }, equipmentListeners_ );
		}
	const auto type = std::find_if( slot->choices.begin(), slot->choices.end(), [&state]( const EquipmentTypeChoice& value ) { return value.type == state.equipmentDraftType; } );
	std::string materials;
	if( type != slot->choices.end() )
		for( std::size_t index = 0; index < type->materials.size(); ++index )
		{
			const auto& material = type->materials[index];
			const bool selected = state.equipmentDraftMaterial && *state.equipmentDraftMaterial == material;
			materials += "<button id='creature_equipment_material_" + std::to_string( index ) + "' class='c-button" + ( selected ? " is-selected" : "" ) + "' aria-pressed='" + ( selected ? "true" : "false" ) + "'>" + Rml::StringUtilities::EncodeRml( material.value ) + "</button>";
		}
	rml( "creature_preview_equipment_materials", materials );
	if( type != slot->choices.end() )
		for( std::size_t index = 0; index < type->materials.size(); ++index )
			if( auto* materialChoice = document_->GetElementById( "creature_equipment_material_" + std::to_string( index ) ) )
			{
				const auto material = type->materials[index];
				bindElement( materialChoice, "click", [this,material]( Rml::Event& ) { controller_->setEquipmentDraftMaterial( material ); }, equipmentListeners_ );
			}
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

 void InspectorRmlBinding::renderLiveTileRows( const TileInspectorState* tile )
 {
	if ( !document_ ) return;
	struct Action
	{
		std::string id, label, description;
		std::function<void()> invoke;
	};
	std::vector<Action> actions;
	std::string markup;
	const auto esc = []( const std::string& value ) { return Rml::StringUtilities::EncodeRml( value ); };
	const auto tr = [this]( const char* key ) { return textCatalog_.format( LocalizationKey{ key } ); };
	const auto contextAction = [this,&tr]( const char* id, const char* key, TileContextAction action ) -> Action
	{
		const auto description = tr( key );
		return { id, description, description, [this,action] { controller_->executeContext( action ); } };
	};
	bool groupOpen = false;
	const auto beginGroup = [&]( const std::string& title )
	{
		if ( groupOpen ) markup += "</section>";
		markup += "<section class='c-live-tile-group' role='group' aria-label='" + esc( title ) + "'><h3>" + esc( title ) + "</h3>";
		groupOpen = true;
	};
	const auto addRow = [&]( const std::string& value, const std::string& detail,
		std::vector<Action> rowActions = {} )
	{
		markup += "<div class='c-live-tile-row'><div class='c-live-tile-row__info'>";
		markup += "<span class='c-live-tile-row__value' title='" + esc( value ) + "'>" + esc( value ) + "</span>";
		if ( !detail.empty() ) markup += "<span class='c-live-tile-row__detail' title='" + esc( detail ) + "'>" + esc( detail ) + "</span>";
		markup += "</div>";
		markup += "<div class='c-live-tile-row__actions'>";
		for ( auto& action : rowActions )
		{
			markup += "<button id='" + action.id + "' class='c-button' title='" + esc( action.description ) + "' aria-label='" + esc( action.description ) + "'>" + esc( action.label ) + "</button>";
			actions.push_back( std::move( action ) );
		}
		markup += "</div>";
		markup += "</div>";
	};
	if ( tile )
	{
		const auto& t = *tile;
		beginGroup( tr( "inspector.live.tile" ) );
		bool terrain = false;
		if ( !t.wall.empty() ) { addRow( t.wall, {}, t.canMine ? std::vector<Action>{ contextAction( "live_tile_mine", "inspector.static.tile_mine", TileContextAction::Mine ) } : std::vector<Action>{} ); terrain = true; }
		if ( !t.floor.empty() )
		{
			std::vector<Action> floorActions;
			if ( t.canRemoveFloor )
			{
				floorActions.push_back( contextAction( "live_tile_remove_floor", "inspector.static.tile_remove_floor", TileContextAction::RemoveFloor ) );
				if ( replaceFloorHandler_ ) floorActions.push_back( { "live_tile_replace_floor", tr( "inspector.live.replace_floor" ),
					tr( "inspector.live.replace_floor_hint" ), [this] { if ( replaceFloorHandler_ ) replaceFloorHandler_(); } } );
			}
			addRow( t.floor, {}, std::move( floorActions ) );
			terrain = true;
		}
		if ( !t.embedded.empty() ) { addRow( t.embedded, {} ); terrain = true; }
		if ( !t.plant.empty() )
		{
			std::vector<Action> plantActions;
			if ( t.canHarvest ) plantActions.push_back( contextAction( "live_tile_harvest", "inspector.static.tile_harvest", TileContextAction::Harvest ) );
			if ( t.canFell ) plantActions.push_back( contextAction( "live_tile_fell", "inspector.static.tile_fell", TileContextAction::FellTree ) );
			if ( t.canRemovePlant ) plantActions.push_back( contextAction( "live_tile_remove_plant", "inspector.static.tile_remove_plant", TileContextAction::RemovePlant ) );
			addRow( t.plant, {}, std::move( plantActions ) );
			terrain = true;
		}
		if ( !t.water.empty() ) { addRow( t.water, {} ); terrain = true; }
		if ( !t.construction.empty() ) { addRow( t.construction, {} ); terrain = true; }
		if ( !terrain ) addRow( tr( "inspector.live.empty_tile" ), {} );
		if ( !t.items.empty() ) beginGroup( tr( "inspector.static.contents" ) );
		for ( const auto& item : t.items )
			addRow( ( item.count ? std::to_string( item.count ) + "x " : "" ) + item.label, item.detail );
		if ( !t.creatures.empty() ) beginGroup( tr( "inspector.static.creatures" ) );
		for ( std::size_t index = 0; index < t.creatures.size(); ++index )
		{
			const auto& creature = t.creatures[index];
			const auto id = creature.id;
			Action open{ "live_tile_open_creature_" + std::to_string( index ), tr( "hud.tool.inspection" ), tr( "inspector.static.tile_open_creature" ),
				[this,id] { controller_->openCreature( id ); } };
			addRow( creature.label, {}, { std::move( open ) } );
		}
		if ( t.hasJob )
		{
			beginGroup( tr( "inspector.static.active_job" ) );
			std::vector<Action> jobActions;
			jobActions.push_back( contextAction( "live_tile_cancel_job", "inspector.static.tile_cancel_job", TileContextAction::CancelJob ) );
			if ( t.canRaisePriority ) jobActions.push_back( { "live_tile_raise_job", "+", tr( "inspector.static.tile_raise_job" ), [this] { controller_->executeContext( TileContextAction::RaisePriority ); } } );
			if ( t.canLowerPriority ) jobActions.push_back( { "live_tile_lower_job", "-", tr( "inspector.static.tile_lower_job" ), [this] { controller_->executeContext( TileContextAction::LowerPriority ); } } );
			addRow( t.jobName,
				( t.jobWorker.empty() ? std::string{ "Unassigned" } : t.jobWorker ) + " | " + t.jobPriority,
				std::move( jobActions ) );
			for ( const auto& item : t.requiredItems )
				addRow( ( item.count ? std::to_string( item.count ) + "x " : "" ) + item.label, item.detail );
		}
		if ( t.designation || !t.roomSummary.empty() || !t.mechanismSummary.empty() )
		{
			beginGroup( tr( "inspector.live.designation" ) );
			std::vector<Action> designationActions;
			if ( t.canDeleteStockpile ) designationActions.push_back( contextAction( "live_tile_delete_stockpile", "inspector.tile.delete_stockpile", TileContextAction::DeleteStockpile ) );
			if ( t.canManage ) designationActions.push_back( contextAction( "live_tile_manage", "inspector.static.tile_manage", TileContextAction::Manage ) );
			const auto detail = t.roomSummary + ( !t.roomSummary.empty() && !t.mechanismSummary.empty() ? " | " : "" ) + t.mechanismSummary;
			addRow( t.designationName.empty() ? detail : t.designationName, t.designationName.empty() ? "" : detail, std::move( designationActions ) );
		}
	}
	if ( groupOpen ) markup += "</section>";
	const std::uint32_t tileId = tile ? tile->id.value : 0;
	const bool newTile = tileId != liveTileId_;
	if ( markup == liveTileRowsMarkup_ )
	{
		if ( newTile ) if ( auto* scroll = document_->GetElementById( "inspector_scroll" ) ) scroll->SetScrollTop( 0 );
		liveTileId_ = tileId;
		return;
	}
	auto* scroll = document_->GetElementById( "inspector_scroll" );
	const float scrollTop = scroll ? scroll->GetScrollTop() : 0.f;
	clearListeners( liveTileListeners_ );
	liveTileRowsMarkup_ = std::move( markup );
	liveTileId_ = tileId;
	rml( "live_tile_rows", liveTileRowsMarkup_ );
	for ( const auto& action : actions )
		if ( auto* button = document_->GetElementById( action.id ) )
			bindElement( button, "click", [invoke=action.invoke]( Rml::Event& ) { invoke(); }, liveTileListeners_ );
	if ( scroll ) scroll->SetScrollTop( newTile ? 0.f : scrollTop );
 }
 void InspectorRmlBinding::setPreviewPage(PreviewPage page)
 {
	if(!previewPageAvailable(page))return;
	if(previewPage_==page){if(page==PreviewPage::Expertise&&expertiseOpenedHandler_)expertiseOpenedHandler_();return;}
	previewPage_=page;
	if(previewPage_!=PreviewPage::Expertise)professionMenuOpen_=false;
	if(previewPage_==PreviewPage::Expertise)renderedPreviewSkillRows_.reset();
	if(controller_)stateChanged(controller_->state());
	else syncPreviewPageButtons();
	if(previewPage_==PreviewPage::Expertise && expertiseOpenedHandler_)expertiseOpenedHandler_();
 }

 void InspectorRmlBinding::syncPreviewPageButtons()
 {
	if(!document_)return;
	const struct { const char* button; const char* panel; PreviewPage page; } views[] = {
		{ "creature_preview_nav_camera", "creature_preview_camera_panel", PreviewPage::Camera },
		{ "creature_preview_nav_stats", "creature_preview_stats_panel", PreviewPage::Stats },
		{ "creature_preview_nav_expertise", "creature_preview_expertise_panel", PreviewPage::Expertise },
		{ "creature_preview_nav_equipment", "creature_preview_equipment_panel", PreviewPage::Equipment },
		{ "creature_preview_nav_inventory", "creature_preview_inventory_panel", PreviewPage::Inventory }
	};
	for(const auto&view:views)
	{
		const bool available=previewPageAvailable(view.page);
		const bool selected=available&&view.page==previewPage_;
		visible(view.button,available);
		visible(view.panel,selected);
		if(view.page==PreviewPage::Camera)visible("creature_preview_camera_actions",selected);
		if(auto*element=document_->GetElementById(view.button))
		{
			element->SetClass("is-selected",selected);
			element->SetAttribute("aria-selected",selected?"true":"false");
			element->SetAttribute("tab-index",selected?"0":"-1");
		}
	}
 }
 void InspectorRmlBinding::setSkillSort(SkillSort sort)
 {
	skillSort_=sort;
	renderedPreviewSkillRows_.reset();
	syncSkillSortButtons();
	if(!lastSkillRows_.empty())rows("creature_preview_skills",lastSkillRows_);
 }

 void InspectorRmlBinding::syncSkillSortButtons()
 {
	if(!document_)return;
	const struct { const char* id; SkillSort sort; } buttons[] = {
		{ "creature_preview_skill_sort_name", SkillSort::Name },
		{ "creature_preview_skill_sort_level", SkillSort::Level },
		{ "creature_preview_skill_sort_active", SkillSort::Active }
	};
	for(const auto&button:buttons)if(auto*element=document_->GetElementById(button.id)){const bool selected=button.sort==skillSort_;element->SetClass("is-selected",selected);element->SetAttribute("aria-pressed",selected?"true":"false");}
 }
 void InspectorRmlBinding::stateChanged(const InspectorState&s){if(detachedWindow_&&qEnvironmentVariableIsSet("INGNOMIA_TRACE_INSPECTOR_SKILLS")){const auto creatureID=s.creature?QString::number(static_cast<qulonglong>(s.creature->id.value)):QStringLiteral("-");const auto creatureName=s.creature?QString::fromStdString(s.creature->name):QStringLiteral("-");traceInspectorSkills(QStringLiteral("state window=%1 camera=%2 revision=%3 kind=%4 creatureId=%5 name=%6 rows=%7 skillsOpen=%8 statsOpen=%9 detailsOpen=%10").arg(windowIndex_).arg(cameraSlot_).arg(s.revision.value).arg(static_cast<int>(s.kind)).arg(creatureID).arg(creatureName).arg(s.creature?s.creature->skills.size():0).arg(s.creatureSkillsOpen?QStringLiteral("true"):QStringLiteral("false")).arg(s.creatureStatsOpen?QStringLiteral("true"):QStringLiteral("false")).arg(s.creatureDetailsOpen?QStringLiteral("true"):QStringLiteral("false")));}if(!document_)return;clearListeners(equipmentListeners_);if(!detachedWindow_)clearListeners(professionListeners_);const bool pendingBlueprint=!liveInspection_&&s.kind==InspectorKind::Tile&&s.tile&&s.tile->hasJob&&s.tile->jobName.rfind("Build",0)==0;const bool stockpileInspection=s.kind==InspectorKind::Stockpile&&s.stockpile.has_value();if(auto*panel=document_->GetElementById("inspector_panel")){panel->SetClass("is-live-tile",liveInspection_);panel->SetClass("is-blueprint",pendingBlueprint);panel->SetClass("is-stockpile",stockpileInspection);}std::string tileLabel;if(s.tile)tileLabel=!s.tile->water.empty()?s.tile->water:!s.tile->wall.empty()?s.tile->wall:s.tile->construction;const bool tileWorldLabel=false;const bool selectionTip=s.selection.active&&!s.selection.sizeLabel.empty();const bool creaturePreview=s.kind==InspectorKind::Creature&&s.creature&&!s.creatureDetailsOpen;const bool open=liveInspection_||s.kind!=InspectorKind::None;visible("inspector_root",open||tileWorldLabel||selectionTip);visible("inspector_panel",open&&(!creaturePreview));visible("tile_inspection_empty",liveInspection_&&!s.tile);visible("creature_preview",creaturePreview);visible("tile_world_label",tileWorldLabel);visible("selection_pointer_tip",selectionTip);text("tile_world_label_text",tileLabel);visible("inspector_blueprint",pendingBlueprint);visible("inspector_tile",s.kind==InspectorKind::Tile&&!pendingBlueprint);visible("inspector_creature",s.kind==InspectorKind::Creature);visible("inspector_workshop",s.kind==InspectorKind::Workshop);visible("inspector_stockpile",stockpileInspection);visible("inspector_agriculture",s.kind==InspectorKind::Agriculture);visible("selection_configuration",false);positionTileLabel(s.selection);positionSelectionTip(s.selection);
	renderLiveTileRows( liveInspection_ && s.kind == InspectorKind::Tile && s.tile ? &*s.tile : nullptr );
	visible( "live_tile_rows", liveInspection_ && s.kind == InspectorKind::Tile && s.tile.has_value() );
	auto tr=[this](const char* key,std::initializer_list<localization::TextArgument> args={}){return textCatalog_.format(LocalizationKey{key},args);};
	auto label=[this](const char* key,const char* fallback){auto value=textCatalog_.format(LocalizationKey{key});return value.empty()||value.rfind("⟦",0)==0?std::string(fallback):value;};
	text("creature_preview_locate_label",label("inspector.static.inspector_locate","Center on map"));
	const char* attributeRowIds[] = { "creature_preview_strength_stat", "creature_preview_dexterity_stat", "creature_preview_constitution_stat", "creature_preview_intelligence_stat", "creature_preview_wisdom_stat", "creature_preview_charisma_stat" };
	const char* needRowIds[] = { "creature_preview_hunger_need", "creature_preview_thirst_need", "creature_preview_sleep_need", "creature_preview_happiness_need" };
	bool anyAttributes = false;
	for ( std::size_t index = 0; index < 6; ++index )
	{
		const bool reported = s.creature && s.creature->attributesReported[index];
		anyAttributes = anyAttributes || reported;
		visible( attributeRowIds[index], reported );
	}
	bool anyNeeds = false;
	for ( std::size_t index = 0; index < 4; ++index )
	{
		const bool reported = s.creature && s.creature->needsReported[index];
		anyNeeds = anyNeeds || reported;
		visible( needRowIds[index], reported );
	}
	visible( "creature_preview_attributes_heading", anyAttributes );
	visible( "creature_preview_needs_heading", anyNeeds );
	visible( "creature_attributes_heading", anyAttributes );
	visible( "creature_needs_heading", anyNeeds );
	const bool hasCreatureStats = anyAttributes || anyNeeds;
	const std::uint32_t creatureId = s.creature ? s.creature->id.value : 0;
	if(creatureId != previewCreatureId_)
	{
		previewCreatureId_ = creatureId;
		previewPage_ = PreviewPage::Camera;
		professionMenuOpen_ = false;
	}
	if(!previewPageAvailable(previewPage_))previewPage_=PreviewPage::Camera;
	const bool skillsOpen = previewPage_ == PreviewPage::Expertise;
	if(detachedWindow_&&skillsOpen&&!lastSkillPanelOpen_)renderedPreviewSkillRows_.reset();
	visible("creature_preview_stats_empty",!hasCreatureStats);
	syncPreviewPageButtons();
	if(s.creature){const auto&c=*s.creature;const auto initial=c.name.empty()?std::string{"?"}:c.name.substr(0,1);text("creature_preview_kind",c.professionReported||c.skillsReported?tr("inspector.preview.gnome"):tr("inspector.preview.creature"));text("creature_preview_title",c.name);text("creature_preview_initial",initial);text("creature_preview_activity",c.activity);visible("creature_preview_activity",!c.activity.empty());rows("creature_preview_skills",c.skills);visible("creature_preview_skill_sort",skillsOpen&&c.skillsReported&&!c.skills.empty());visible("creature_preview_skills_empty",c.skills.empty());text("creature_preview_strength",std::to_string(c.strength));text("creature_preview_dexterity",std::to_string(c.dexterity));text("creature_preview_constitution",std::to_string(c.constitution));text("creature_preview_intelligence",std::to_string(c.intelligence));text("creature_preview_wisdom",std::to_string(c.wisdom));text("creature_preview_charisma",std::to_string(c.charisma));const auto meter=[this,&tr](const char*valueId,const char*meterId,int value,bool reported){text(valueId,reported?std::to_string(value):tr("inspector.static.not_reported"));if(auto*e=document_->GetElementById(meterId))e->SetProperty("width",std::to_string(reported?std::clamp(value,0,100):0)+"%");};meter("creature_preview_hunger","creature_preview_hunger_meter",c.hunger,c.needsReported[0]);meter("creature_preview_thirst","creature_preview_thirst_meter",c.thirst,c.needsReported[1]);meter("creature_preview_sleep","creature_preview_sleep_meter",c.sleep,c.needsReported[2]);meter("creature_preview_happiness","creature_preview_happiness_meter",c.happiness,c.needsReported[3]);}
	if(s.creature&&detachedWindow_)
	{
		const auto&c=*s.creature;
		renderEquipment(s);
		rows("creature_preview_inventory",c.inventory);
		visible("creature_preview_inventory_empty",!c.inventoryReported||c.inventory.empty());
		const auto professionLabel=c.profession.empty()?tr("inspector.preview.no_profession"):c.profession;
		text("creature_preview_profession_value",professionLabel);
		const std::vector<std::string> professionOptions=c.professionReported?s.professionChoices:std::vector<std::string>{};
		if(renderedProfessionChoices_!=professionOptions)
		{
			clearListeners(professionListeners_);
			std::string professionChoices;
			for(std::size_t index=0;index<professionOptions.size();++index)
				professionChoices+="<button id='creature_preview_profession_choice_"+std::to_string(index)+"' class='c-creature-preview-profession-option' role='option'>"+Rml::StringUtilities::EncodeRml(professionOptions[index])+"</button>";
			rml("creature_preview_profession_menu",professionChoices);
			for(std::size_t index=0;index<professionOptions.size();++index)
				if(auto*choice=document_->GetElementById("creature_preview_profession_choice_"+std::to_string(index)))
				{
					const auto profession=professionOptions[index];
					bindElement(choice,"click",[this,profession](Rml::Event&){professionMenuOpen_=false;visible("creature_preview_profession_menu",false);if(controller_)controller_->setProfession(profession);},professionListeners_);
				}
			renderedProfessionChoices_=professionOptions;
		}
		for(std::size_t index=0;index<professionOptions.size();++index)
			if(auto*choice=document_->GetElementById("creature_preview_profession_choice_"+std::to_string(index)))
			{
				const bool selected=professionOptions[index]==c.profession;
				choice->SetClass("is-selected",selected);
				choice->SetAttribute("aria-selected",selected?"true":"false");
			}
		const bool professionChoicesAvailable=c.professionReported&&!s.professionChoices.empty();
		if(auto*toggle=document_->GetElementById("creature_preview_profession_toggle"))
		{
			toggle->SetAttribute("aria-expanded",professionMenuOpen_&&professionChoicesAvailable?"true":"false");
			toggle->SetClass("is-open",professionMenuOpen_&&professionChoicesAvailable);
			if(professionChoicesAvailable)toggle->RemoveAttribute("disabled");else toggle->SetAttribute("disabled","disabled");
		}
		visible("creature_preview_profession_toggle",c.professionReported);
		visible("creature_preview_profession_menu",professionMenuOpen_&&professionChoicesAvailable);
		visible("creature_preview_professions_empty",!c.professionReported||s.professionChoices.empty());
	}
	// Need bars are projected separately from their text so unavailable data can
	// remain explicit while reported values receive a useful status color.
	if(s.creature){const auto&c=*s.creature;const auto meterColor=[this](const char*meterId,int value,bool reported){if(auto*e=document_->GetElementById(meterId)){const int normalized=std::clamp(value,0,100);e->SetProperty("width",std::to_string(reported?normalized:0)+"%");e->SetClass("is-good",reported&&normalized>=70);e->SetClass("is-middle",reported&&normalized>=40&&normalized<70);e->SetClass("is-danger",reported&&normalized<40);e->SetClass("is-unavailable",!reported);}};meterColor("creature_preview_hunger_meter",c.hunger,c.needsReported[0]);meterColor("creature_preview_thirst_meter",c.thirst,c.needsReported[1]);meterColor("creature_preview_sleep_meter",c.sleep,c.needsReported[2]);meterColor("creature_preview_happiness_meter",c.happiness,c.needsReported[3]);}
	std::string title=tr("inspector.selection"),kind=tr("inspector.selection"),pos;if(s.selected){if(s.selected->position){const auto&p=*s.selected->position;pos=tr("inspector.position",{{"x",std::to_string(p.x)},{"y",std::to_string(p.y)},{"z",std::to_string(p.z)}});}visible("inspector_locate",s.selected->position.has_value());}else visible("inspector_locate",false);
 if(s.tile){const auto&t=*s.tile;title=t.designationName.empty()?"Tile":t.designationName;kind="Tile";std::vector<std::string>pointerDetails;if(!t.floor.empty())pointerDetails.push_back(t.floor);if(!t.water.empty())pointerDetails.push_back(t.water);if(!t.wall.empty())pointerDetails.push_back(t.wall);if(!t.construction.empty())pointerDetails.push_back(t.construction);std::string pointerDetail;for(const auto&detail:pointerDetails){if(!pointerDetail.empty())pointerDetail+=" | ";pointerDetail+=detail;}text("selection_pointer_tool",title);text("selection_pointer_size",pointerDetail);visible("selection_pointer_size",!pointerDetail.empty());std::vector<TextCountRow>terrain;if(!t.wall.empty())terrain.push_back({t.wall,{},{}});if(!t.floor.empty())terrain.push_back({t.floor,{},{}});if(!t.embedded.empty())terrain.push_back({t.embedded,{},{}});if(!t.plant.empty())terrain.push_back({t.plant,{},{}});if(!t.water.empty())terrain.push_back({t.water,{},{}});rows("tile_terrain",terrain);visible("tile_construction_section",!t.construction.empty());text("tile_construction",t.construction);visible("tile_contents_section",!t.items.empty());rows("tile_contents",t.items);visible("tile_creatures_section",!t.creatures.empty());std::vector<TextCountRow>cr;for(const auto&v:t.creatures)cr.push_back({v.label,{},0});rows("tile_creatures",cr);visible("tile_job_section",t.hasJob);text("tile_job",t.jobName+" | worker "+(t.jobWorker.empty()?"unassigned":t.jobWorker)+" | priority "+t.jobPriority+" | skill "+t.requiredSkill+" | tool "+t.requiredTool+" ("+t.requiredToolAvailable+")");text("tile_job_work_positions","Work positions: "+t.workPositions);visible("tile_job_requirements_section",!t.requiredItems.empty());rows("tile_job_requirements",t.requiredItems);visible("tile_links_section",t.designation.has_value()||!t.roomSummary.empty()||!t.mechanismSummary.empty());text("tile_links",t.designationName+(t.roomSummary.empty()?"":" | "+t.roomSummary)+(t.mechanismSummary.empty()?"":" | "+t.mechanismSummary));visible("tile_mine",t.canMine);visible("tile_remove_floor",t.canRemoveFloor);visible("tile_harvest",t.canHarvest);visible("tile_fell",t.canFell);visible("tile_remove_plant",t.canRemovePlant);visible("tile_manage",t.canManage);visible("tile_delete_stockpile",t.canDeleteStockpile);if(auto*e=document_->GetElementById("tile_raise_job")){e->SetAttribute("aria-disabled",t.hasJob&&t.canRaisePriority?"false":"true");if(t.hasJob&&t.canRaisePriority)e->RemoveAttribute("disabled");else e->SetAttribute("disabled","disabled");}if(auto*e=document_->GetElementById("tile_lower_job")){e->SetAttribute("aria-disabled",t.hasJob&&t.canLowerPriority?"false":"true");if(t.hasJob&&t.canLowerPriority)e->RemoveAttribute("disabled");else e->SetAttribute("disabled","disabled");}}
 if(pendingBlueprint&&s.tile)
 {
	const auto&t=*s.tile;
	title=t.jobName=="BuildWorkshop"?label("inspector.static.blueprint_title","Workshop blueprint"):label("inspector.static.blueprint_construction_title","Construction blueprint");
	kind=label("inspector.static.blueprint_kind","Construction");
	std::string missingRows;
	bool waiting=false;
	const auto missingLabel=label("inspector.static.blueprint_missing","Missing");
	for(const auto&requirement:t.requiredItems)
		if(!requirement.available)
		{
			waiting=true;
			const auto item=displayName(requirement.label);
			const auto material=displayName(requirement.detail.empty()?std::string_view{"any"}:std::string_view{requirement.detail});
			missingRows+="<div class='c-blueprint-row is-missing'><span class='c-blueprint-row__item'>"+Rml::StringUtilities::EncodeRml(item)+"</span><span class='c-blueprint-row__material'>"+Rml::StringUtilities::EncodeRml(material)+"</span><span class='c-blueprint-row__status'>"+Rml::StringUtilities::EncodeRml(missingLabel)+"</span><span class='c-blueprint-row__needed'>x"+std::to_string(requirement.count)+"</span></div>";
		}
	rml("blueprint_missing_items",missingRows);
	visible("blueprint_missing_items",waiting);
	visible("blueprint_resources_ready",!waiting);
	text("blueprint_status",waiting?label("inspector.static.blueprint_waiting","Waiting for materials"):label("inspector.static.blueprint_ready","Materials available - waiting for a builder"));
	if(auto*status=document_->GetElementById("blueprint_status"))status->SetClass("is-ready",!waiting);
	text("blueprint_worker",t.jobWorker.empty()?label("inspector.static.blueprint_unassigned","Unassigned"):t.jobWorker);
	text("blueprint_priority",t.jobPriority.empty()?"0":t.jobPriority);
	text("blueprint_skill",t.requiredSkill);
	visible("blueprint_skill_row",!t.requiredSkill.empty());
	text("blueprint_tool",t.requiredTool+(t.requiredTool.empty()?"":" ("+(t.requiredToolAvailable.empty()?"unknown":t.requiredToolAvailable)+")"));
	visible("blueprint_tool_row",!t.requiredTool.empty());
	const auto configureButton=[this](const char*id,bool enabled){if(auto*element=document_->GetElementById(id)){element->SetAttribute("aria-disabled",enabled?"false":"true");if(enabled)element->RemoveAttribute("disabled");else element->SetAttribute("disabled","disabled");}};
	configureButton("blueprint_raise_job",t.canRaisePriority);
	configureButton("blueprint_lower_job",t.canLowerPriority);
 }
  if(s.creature){const auto&c=*s.creature;title=c.name;kind="Creature";if(!detachedWindow_){text("creature_profession","Profession: "+(c.profession.empty()?"None":c.profession));const bool professionSupported=c.professionReported;const bool skillsSupported=c.skillsReported;visible("creature_profession",professionSupported);visible("creature_profession_choices_heading",professionSupported);std::string choices;if(professionSupported)for(std::size_t i=0;i<s.professionChoices.size();++i){const auto&p=s.professionChoices[i];const auto selected=p==c.profession?" is-selected":"";choices+="<button id='inspector_profession_choice_"+std::to_string(i)+"' class='c-dropdown-button"+std::string(selected)+"' role='option' aria-selected='"+(p==c.profession?"true":"false")+"' data-profession='"+Rml::StringUtilities::EncodeRml(p)+"'>"+Rml::StringUtilities::EncodeRml(p)+"</button>";}if(auto*e=document_->GetElementById("creature_professions")){e->SetInnerRML(choices);if(professionSupported)for(std::size_t i=0;i<s.professionChoices.size();++i)if(auto*choice=document_->GetElementById("inspector_profession_choice_"+std::to_string(i))){const auto profession=s.professionChoices[i];bindElement(choice,"click",[this,profession](Rml::Event&){controller_->setProfession(profession);},professionListeners_);}}visible("creature_professions",professionSupported&&!s.professionChoices.empty());visible("creature_professions_empty",professionSupported&&s.professionChoices.empty());text("creature_activity",c.activity.empty()?std::string{}:"Activity: "+c.activity);visible("creature_activity",!c.activity.empty());visible("creature_skills_heading",skillsSupported);rows("creature_skills",c.skills);visible("creature_skills",skillsSupported&&!c.skills.empty());visible("creature_skills_empty",skillsSupported&&c.skills.empty());const auto attribute=[](const char*label,int value,bool reported){return reported?std::string("<div class='c-creature-attribute'><span>")+label+"</span><strong>"+std::to_string(value)+"</strong></div>":std::string{};};rml("creature_attributes",attribute("STR",c.strength,c.attributesReported[0])+attribute("DEX",c.dexterity,c.attributesReported[1])+attribute("CON",c.constitution,c.attributesReported[2])+attribute("INT",c.intelligence,c.attributesReported[3])+attribute("WIS",c.wisdom,c.attributesReported[4])+attribute("CHA",c.charisma,c.attributesReported[5]));const auto need=[&](const char*label,std::size_t index,int value){return c.needsReported[index]?std::string("<div class='c-creature-need-summary'><span>")+label+"</span><strong>"+std::to_string(value)+"</strong></div>":std::string{};};rml("creature_needs",need("Hunger",0,c.hunger)+need("Thirst",1,c.thirst)+need("Sleep",2,c.sleep)+need("Happiness",3,c.happiness));rows("creature_equipment",c.equipment);visible("creature_equipment_empty",c.equipment.empty());rows("creature_inventory",c.inventory);visible("creature_inventory_empty",c.inventoryReported&&c.inventory.empty());}if(detachedWindow_&&qEnvironmentVariableIsSet("INGNOMIA_TRACE_INSPECTOR_SKILLS")){qInfo()<<"Inspector view"<<static_cast<const void*>(this)<<"id"<<c.name.c_str()<<"rows"<<c.skills.size()<<"skillsOpen"<<s.creatureSkillsOpen;const auto tracePath=qEnvironmentVariable("INGNOMIA_TRACE_INSPECTOR_SKILLS_PATH");if(!tracePath.isEmpty()){QFile trace(tracePath);if(trace.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){QTextStream stream(&trace);stream<<static_cast<const void*>(this)<<" "<<c.name.c_str()<<" rows="<<c.skills.size()<<" skillsOpen="<<(s.creatureSkillsOpen?"true":"false")<<"\n";}}}}
	if(s.workshop){const auto&w=*s.workshop;title=w.name;kind="Workshop";text("workshop_summary","Priority "+std::to_string(w.priority)+" / "+std::to_string(w.maxPriority)+" | products "+std::to_string(w.productCount)+" | queued "+std::to_string(w.queuedJobs));text("workshop_options",std::string(w.suspended?"Suspended":"Active")+" | generated "+(w.acceptGenerated?"accepted":"rejected")+" | auto missing "+(w.autoCraftMissing?"on":"off")+" | stockpile "+(w.linkedStockpile?"linked":"not linked"));text("workshop_toggle_suspended",w.suspended?"Resume workshop":"Suspend workshop");}
	if(s.stockpile)
	{
		const auto&v=*s.stockpile;
		title=v.name;
		kind="Stockpile";
		text("stockpile_status",v.suspended?"Suspended":"Active");
		if(auto*status=document_->GetElementById("stockpile_status"))status->SetClass("is-suspended",v.suspended);
		text("stockpile_priority",std::to_string(v.priority)+" / "+std::to_string(v.maxPriority));
		text("stockpile_capacity",std::to_string(v.itemCount)+" / "+std::to_string(v.capacity));
		text("stockpile_reserved",std::to_string(v.reserved));
		text("stockpile_pull_from_others",v.pullFromOthers?"Yes":"No");
		text("stockpile_allow_pull",v.allowPullFromHere?"Yes":"No");
		std::string contents;
		for(const auto&row:v.contents)
		{
			const auto icon=row.icon.empty()?std::string{"<span class='c-stockpile-row__icon c-stockpile-row__icon--empty' aria-hidden='true'></span>"}:"<span class='c-stockpile-row__icon' aria-hidden='true'><img src='"+Rml::StringUtilities::EncodeRml(row.icon)+"' /></span>";
			contents+="<div class='c-stockpile-row' role='row'><span class='c-stockpile-row__item' role='cell'>"+icon+"<span class='c-stockpile-row__item-label'>"+Rml::StringUtilities::EncodeRml(displayName(row.label))+"</span></span><span class='c-stockpile-row__material' role='cell'>"+Rml::StringUtilities::EncodeRml(row.detail.empty()?std::string{"Any"}:displayName(row.detail))+"</span><span class='c-stockpile-row__quantity' role='cell'>"+std::to_string(row.count)+"</span></div>";
		}
		rml("stockpile_contents",contents);
		visible("stockpile_contents",!v.contents.empty());
		visible("stockpile_contents_empty",v.contents.empty());
		text("stockpile_toggle_suspended",v.suspended?"Resume stockpile":"Suspend stockpile");
	}
	if(s.agriculture){const auto&a=*s.agriculture;kind=a.target.kind==AgricultureKind::Farm?"Farm":a.target.kind==AgricultureKind::Pasture?"Pasture":"Grove";title=a.name;text("agriculture_kind",kind+" inspection");text("agriculture_summary","Priority "+std::to_string(a.priority)+" / "+std::to_string(a.maxPriority)+" | plots "+std::to_string(a.plots)+" | product "+a.product+" | planted "+std::to_string(a.planted)+" | ready "+std::to_string(a.ready));text("agriculture_options",std::string(a.suspended?"Suspended":"Active")+" | harvest "+(a.target.kind==AgricultureKind::Grove?(a.pick?"pick":"do not pick"):(a.harvest?"on":"off")));text("agriculture_toggle_suspended",a.suspended?"Resume":"Suspend");text("agriculture_toggle_primary",a.target.kind==AgricultureKind::Grove?(a.pick?"Stop picking":"Pick fruit"):(a.harvest?"Stop harvest":"Harvest"));}
	text("inspector_title",title);text("inspector_kind",kind);text("inspector_position",pos);text("creature_preview_camera_position",pos.empty()?tr("inspector.preview.camera_unavailable"):pos);visible("inspector_back",s.kind==InspectorKind::Creature&&(s.previous.has_value()||s.creatureDetailsOpen));visible("inspector_refresh",s.kind!=InspectorKind::None&&!pendingBlueprint&&!stockpileInspection);text("selection_action","Active tool: "+s.selection.actionLabel);std::string geometry="Size "+(s.selection.sizeLabel.empty()?"not anchored":s.selection.sizeLabel);if(s.selection.cursor)geometry+=" | cursor "+std::to_string(s.selection.cursor->x)+","+std::to_string(s.selection.cursor->y)+","+std::to_string(s.selection.cursor->z);text("selection_geometry",geometry);visible("selection_rotate",s.selection.canRotate);text("selection_pointer_tool",s.selection.actionLabel);text("selection_pointer_size","Size "+s.selection.sizeLabel);visible("selection_pointer_size",!s.selection.sizeLabel.empty());text("inspector_status",s.status);
	lastSkillPanelOpen_=skillsOpen;
 }
} // namespace ingnomia::ui::inspector
