/*
 * This file is part of Ingnomia.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiDesignerRmlBinding.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ingnomia::ui::designer
{
namespace
{
class Callback final : public Rml::EventListener
{
public:
	explicit Callback( std::function<void( Rml::Event& )> callback ) : callback_( std::move( callback ) ) {}
	void ProcessEvent( Rml::Event& event ) override { callback_( event ); }
private:
	std::function<void( Rml::Event& )> callback_;
};

std::string valueOf( Rml::ElementDocument* document, const char* id )
{
	if ( !document ) return {};
	if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( document->GetElementById( id ) ) )
		return control->GetValue();
	return {};
}

bool parseFloat( std::string_view value, float& output )
{
	std::string copy( value );
	char* end = nullptr;
	output = std::strtof( copy.c_str(), &end );
	return end == copy.c_str() + copy.size() && std::isfinite( output );
}

std::string unitValue( float value )
{
	return std::to_string( static_cast<double>( value ) ) + "dp";
}

bool isGeneratedId( std::string_view id )
{
	return id.find( "rows" ) != std::string_view::npos || id.find( "schedule_" ) != std::string_view::npos;
}
} // namespace

UiDesignerRmlBinding::UiDesignerRmlBinding( Rml::Context& context, DocumentLoader loader ) :
	context_( context ),
	documentLoader_( std::move( loader ) )
{
	model_.setChangeHandler( [this] { sync(); } );
}

UiDesignerRmlBinding::~UiDesignerRmlBinding()
{
	shutdown();
}

bool UiDesignerRmlBinding::visible() const noexcept
{
	return document_ && document_->IsVisible();
}

bool UiDesignerRmlBinding::initialize()
{
	if ( document_ || !documentLoader_ ) return document_ != nullptr;
	document_ = documentLoader_( "developer_ui/designer_overlay.rml" );
	if ( !document_ ) return false;
	document_->Show();
	bind( "designer_toggle", "click", [this]( Rml::Event& ) { toggle(); } );
	bind( "designer_play", "click", [this]( Rml::Event& ) { setDesignMode( false ); } );
	bind( "designer_design", "click", [this]( Rml::Event& ) { setDesignMode( true ); } );
	bind( "designer_undo", "click", [this]( Rml::Event& )
		{ if ( model_.undo() && selectedElement_ ) applySelectedElementBounds( { model_.selection().left, model_.selection().top, model_.selection().width, model_.selection().height } ); } );
	bind( "designer_save", "click", [this]( Rml::Event& ) { saveProject(); } );
	bind( "designer_export", "click", [this]( Rml::Event& ) { exportHandoff(); } );
	bind( "designer_close", "click", [this]( Rml::Event& ) { setVisible( false ); } );
	for ( const auto& component : model_.components() )
	{
		const auto id = "designer_component_" + component.id;
		bind( id.c_str(), "click", [this, componentId = component.id]( Rml::Event& ) { onComponent( componentId ); } );
	}
	bind( "designer_x", "change", [this]( Rml::Event& ) { onPropertyChanged( "designer_x" ); } );
	bind( "designer_y", "change", [this]( Rml::Event& ) { onPropertyChanged( "designer_y" ); } );
	bind( "designer_width", "change", [this]( Rml::Event& ) { onPropertyChanged( "designer_width" ); } );
	bind( "designer_height", "change", [this]( Rml::Event& ) { onPropertyChanged( "designer_height" ); } );
	sync();
	return true;
}

bool UiDesignerRmlBinding::reloadDocument()
{
	const bool wasVisible = visible();
	shutdown();
	if ( !initialize() ) return false;
	if ( wasVisible ) setVisible( true );
	return true;
}

void UiDesignerRmlBinding::shutdown()
{
	if ( !document_ ) return;
	for ( auto& listener : listeners_ )
		if ( listener.target && listener.listener ) listener.target->RemoveEventListener( listener.event, listener.listener.get() );
	listeners_.clear();
	selectedElement_ = nullptr;
	model_.clearSelection();
	context_.UnloadDocument( document_ );
	document_ = nullptr;
}

void UiDesignerRmlBinding::bind( const char* id, const char* event, std::function<void( Rml::Event& )> callback )
{
	if ( !document_ ) return;
	auto* target = document_->GetElementById( id );
	if ( !target ) return;
	Listener listener;
	listener.target = target;
	listener.event = event;
	listener.listener = std::make_unique<Callback>( std::move( callback ) );
	target->AddEventListener( event, listener.listener.get(), false );
	listeners_.push_back( std::move( listener ) );
}

void UiDesignerRmlBinding::toggle()
{
	if ( visible() ) setVisible( false );
	else setVisible( true );
}

void UiDesignerRmlBinding::setVisible( bool value )
{
	if ( !document_ ) return;
	if ( value )
	{
		document_->Show();
		setDesignMode( true );
	}
	else
	{
		setDesignMode( false );
		document_->Hide();
		selectedElement_ = nullptr;
	}
	sync();
}

void UiDesignerRmlBinding::setDesignMode( bool value )
{
	if ( value )
	{
		model_.enterDesign();
		if ( pauseHandler_ && !pauseOwned_ )
		{
			pauseHandler_( true );
			pauseOwned_ = true;
		}
	}
	else
	{
		model_.leaveDesign();
		if ( pauseHandler_ && pauseOwned_ )
		{
			pauseHandler_( pausedBeforeDesign_ );
			pauseOwned_ = false;
		}
	}
	sync();
}

bool UiDesignerRmlBinding::isEditorElement( const Rml::Element* element ) const
{
	for ( auto* current = element; current; current = current->GetParentNode() )
		if ( current == document_ || current->GetId() == "designer_overlay" ) return true;
	return false;
}

Rml::Element* UiDesignerRmlBinding::selectableAncestor( Rml::Element* element ) const
{
	for ( auto* current = element; current; current = current->GetParentNode() )
	{
		if ( current == document_ || current->GetTagName() == "body" ) return nullptr;
		if ( !current->GetId().empty() ) return current;
	}
	return nullptr;
}

bool UiDesignerRmlBinding::editorConsumesPoint( QPointF point ) const
{
	if ( !visible() ) return false;
	return isEditorElement( context_.GetElementAtPoint( { static_cast<float>( point.x() ), static_cast<float>( point.y() ) } ) );
}

bool UiDesignerRmlBinding::selectAt( QPointF point )
{
	if ( !visible() || !designMode() ) return false;
	auto* hit = context_.GetElementAtPoint( { static_cast<float>( point.x() ), static_cast<float>( point.y() ) } );
	if ( !hit || isEditorElement( hit ) ) return false;
	auto* target = selectableAncestor( hit );
	if ( !target ) return false;
	const std::string id = target->GetId();
	const bool generated = isGeneratedId( id );
	if ( !pendingComponent_.empty() )
	{
		if ( generated ) return false;
		ElementSelection selection { id, target->GetTagName(), target->GetAbsoluteLeft(), target->GetAbsoluteTop(),
			target->GetOffsetWidth(), target->GetOffsetHeight(), true, false };
		if ( !model_.select( std::move( selection ) ) ) return false;
		selectedElement_ = target;
		return insertComponent( pendingComponent_ );
	}
	ElementSelection selection { id, target->GetTagName(), target->GetAbsoluteLeft(), target->GetAbsoluteTop(),
		target->GetOffsetWidth(), target->GetOffsetHeight(), true, generated };
	if ( !model_.select( std::move( selection ) ) ) return false;
	selectedElement_ = target;
	sync();
	return true;
}

bool UiDesignerRmlBinding::keyPress( int key, bool shift )
{
	if ( !designMode() || !selectedElement_ || model_.selection().generated ) return false;
	const auto selection = model_.selection();
	Bounds bounds { selection.left, selection.top, selection.width, selection.height };
	const float amount = shift ? 10.0f : 1.0f;
	switch ( key )
	{
		case 0x01000012: bounds.left -= amount; break; // Left
		case 0x01000014: bounds.left += amount; break; // Right
		case 0x01000013: bounds.top -= amount; break; // Up
		case 0x01000015: bounds.top += amount; break; // Down
		default: return false;
	}
	setSelectedElementBounds( bounds );
	return true;
}

void UiDesignerRmlBinding::setSelectedElementBounds( Bounds bounds )
{
	if ( !selectedElement_ || !model_.updateBounds( bounds ) ) return;
	applySelectedElementBounds( bounds );
}

void UiDesignerRmlBinding::applySelectedElementBounds( Bounds bounds )
{
	if ( !selectedElement_ ) return;
	selectedElement_->SetProperty( "left", unitValue( bounds.left ) );
	selectedElement_->SetProperty( "top", unitValue( bounds.top ) );
	selectedElement_->SetProperty( "width", unitValue( bounds.width ) );
	selectedElement_->SetProperty( "height", unitValue( bounds.height ) );
	sync();
}

void UiDesignerRmlBinding::onPropertyChanged( const char* id )
{
	if ( !selectedElement_ || model_.selection().generated ) return;
	float value{};
	if ( !parseFloat( valueOf( document_, id ), value ) ) return;
	const auto selection = model_.selection();
	Bounds bounds { selection.left, selection.top, selection.width, selection.height };
	if ( std::string_view( id ) == "designer_x" ) bounds.left = value;
	else if ( std::string_view( id ) == "designer_y" ) bounds.top = value;
	else if ( std::string_view( id ) == "designer_width" ) bounds.width = value;
	else if ( std::string_view( id ) == "designer_height" ) bounds.height = value;
	setSelectedElementBounds( bounds );
}

void UiDesignerRmlBinding::onComponent( std::string_view componentId )
{
	pendingComponent_ = std::string( componentId );
	if ( document_ )
		if ( auto* status = document_->GetElementById( "designer_status" ) )
			status->SetInnerRML( "Click a container to insert " + std::string( componentId ) + "." );
}

bool UiDesignerRmlBinding::insertComponent( std::string_view componentId )
{
	if ( !selectedElement_ || isGeneratedId( selectedElement_->GetId() ) ) return false;
	auto* parent = selectedElement_;
	if ( parent->GetTagName() == "button" || parent->GetTagName() == "input" || parent->GetTagName() == "img" ) parent = parent->GetParentNode();
	if ( !parent ) return false;
	const auto found = std::find_if( model_.components().begin(), model_.components().end(), [&]( const auto& component ) { return component.id == componentId; } );
	if ( found == model_.components().end() ) return false;
	auto element = selectedElement_->GetOwnerDocument()->CreateElement( found->tagName );
	if ( !element ) return false;
	const std::string id = "designer_added_" + found->id + "_" + std::to_string( nextGeneratedId_++ );
	element->SetId( id );
	element->SetAttribute( "data-ui-designer", "true" );
	element->SetClass( "designer-added", true );
	if ( found->tagName == "input" ) element->SetAttribute( "type", found->id == "checkbox" ? "checkbox" : "text" );
	else if ( found->tagName == "img" ) element->SetAttribute( "alt", "New image" );
	else element->SetInnerRML( found->label );
	parent->AppendChild( std::move( element ) );
	if ( auto* added = selectedElement_->GetOwnerDocument()->GetElementById( id.c_str() ) )
		model_.recordAddedComponent( { id, found->id, found->tagName, parent->GetId(), added->GetAbsoluteLeft(),
			added->GetAbsoluteTop(), added->GetOffsetWidth(), added->GetOffsetHeight() } );
	pendingComponent_.clear();
	sync();
	return true;
}

void UiDesignerRmlBinding::sync()
{
	if ( !document_ ) return;
	if ( auto* mode = document_->GetElementById( "designer_mode" ) ) mode->SetInnerRML( designMode() ? "DESIGN" : "PLAY" );
	if ( auto* surface = document_->GetElementById( "designer_surface" ) )
		surface->SetInnerRML( "Live target: " + surfaceName_ );
	if ( auto* status = document_->GetElementById( "designer_status" ) )
	{
		if ( !visible() ) status->SetInnerRML( "Designer hidden" );
		else if ( !model_.hasSelection() ) status->SetInnerRML( "Select a UI element to edit." );
		else status->SetInnerRML( model_.selection().generated ? "Generated region: layout is protected." : "Selected: " + model_.selection().id );
	}
	if ( auto* box = document_->GetElementById( "designer_selection" ) )
	{
		const auto& selection = model_.selection();
		box->SetProperty( "display", model_.hasSelection() ? "block" : "none" );
		if ( model_.hasSelection() )
		{
			box->SetProperty( "left", unitValue( selection.left ) );
			box->SetProperty( "top", unitValue( selection.top ) );
			box->SetProperty( "width", unitValue( selection.width ) );
			box->SetProperty( "height", unitValue( selection.height ) );
		}
	}
	const auto& selection = model_.selection();
	for ( const auto& [id, value] : std::initializer_list<std::pair<const char*, float>> {
		{ "designer_x", selection.left }, { "designer_y", selection.top }, { "designer_width", selection.width }, { "designer_height", selection.height } } )
		if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( document_->GetElementById( id ) ) ) control->SetValue( std::to_string( static_cast<double>( value ) ) );
}

void UiDesignerRmlBinding::saveProject()
{
	const QString path = qEnvironmentVariable( "INGNOMIA_UI_DESIGNER_PROJECT", "artifacts/ui-designer/live-preview/project.json" );
	const QFileInfo info( path );
	QDir().mkpath( info.absolutePath() );
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) return;
	file.write( QByteArray::fromStdString( model_.projectJson( surfaceName_ ) ) );
	file.close();
	if ( auto* status = document_->GetElementById( "designer_status" ) ) status->SetInnerRML( "Project saved: " + path.toStdString() );
}

void UiDesignerRmlBinding::exportHandoff()
{
	const QString root = qEnvironmentVariable( "INGNOMIA_UI_DESIGNER_EXPORT", "artifacts/ui-designer/live-preview/handoff" );
	if ( !QDir().mkpath( root ) ) return;
	const QString projectPath = QDir( root ).filePath( "project.json" );
	const QString manifestPath = QDir( root ).filePath( "manifest.json" );
	const QString handoffPath = QDir( root ).filePath( "handoff.md" );
	QFile project( projectPath );
	QFile manifest( manifestPath );
	QFile handoff( handoffPath );
	if ( !project.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ||
		!manifest.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ||
		!handoff.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) return;
	project.write( QByteArray::fromStdString( model_.projectJson( surfaceName_ ) ) );
	const QByteArray surface = QByteArray::fromStdString( surfaceName_ );
	manifest.write( QByteArray( "{\n  \"formatVersion\": 1,\n  \"surface\": \"" ) + surface
		+ QByteArray( "\",\n  \"designerMode\": true,\n  \"generatedRegionsProtected\": true\n}\n" ) );
	handoff.write( QByteArray( "# Ingnomia UI Designer handoff\n\nSurface: " ) + surface + QByteArray( "\n\n"
		"This export contains a live designer project for the selected RmlUi surface.\n"
		"Apply the authored layout changes through the owning RML/RCSS binding; generated rows and game actions remain protected.\n\n"
		"Files:\n- project.json: selected surface and draft properties\n- manifest.json: export contract\n" ) );
	project.close();
	manifest.close();
	handoff.close();
	if ( auto* status = document_->GetElementById( "designer_status" ) ) status->SetInnerRML( "Handoff exported: " + root.toStdString() );
}

} // namespace ingnomia::ui::designer
