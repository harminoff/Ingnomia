/*
 * This file is part of Ingnomia.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "UiDesignerModel.h"

#include <RmlUi/Core/Element.h>

#include <QPointF>
#include <QString>

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace Rml
{
class Context;
class ElementDocument;
class Event;
}

namespace ingnomia::ui::designer
{

class UiDesignerRmlBinding final
{
public:
	using DocumentLoader = std::function<Rml::ElementDocument*( const char* )>;
	using PauseHandler = std::function<void( bool )>;

	UiDesignerRmlBinding( Rml::Context&, DocumentLoader );
	~UiDesignerRmlBinding();

	UiDesignerRmlBinding( const UiDesignerRmlBinding& ) = delete;
	UiDesignerRmlBinding& operator=( const UiDesignerRmlBinding& ) = delete;

	bool initialize();
	bool reloadDocument();
	void shutdown();
	bool visible() const noexcept;
	bool designMode() const noexcept { return model_.designMode(); }
	void toggle();
	void setVisible( bool value );
	void setDesignMode( bool value );
	bool editorConsumesPoint( QPointF point ) const;
	bool selectAt( QPointF point );
	bool keyPress( int key, bool shift );
	void setPauseHandler( PauseHandler handler ) { pauseHandler_ = std::move( handler ); }
	void setCurrentPaused( bool paused ) { if ( !pauseOwned_ ) pausedBeforeDesign_ = paused; }
	void setSurfaceName( std::string name ) { surfaceName_ = std::move( name ); }
	void saveProject();
	void exportHandoff();

	UiDesignerModel& model() noexcept { return model_; }
	const UiDesignerModel& model() const noexcept { return model_; }

private:
	struct Listener
	{
		Rml::Element* target{};
		std::string event;
		std::unique_ptr<Rml::EventListener> listener;
	};

	void bind( const char* id, const char* event, std::function<void( Rml::Event& )> callback );
	void sync();
	void onComponent( std::string_view componentId );
	void onPropertyChanged( const char* id );
	bool insertComponent( std::string_view componentId );
	bool isEditorElement( const Rml::Element* element ) const;
	Rml::Element* selectableAncestor( Rml::Element* element ) const;
	Rml::Element* selectedElement() const noexcept { return selectedElement_; }
	void setSelectedElementBounds( Bounds bounds );
	void applySelectedElementBounds( Bounds bounds );

	Rml::Context& context_;
	DocumentLoader documentLoader_;
	Rml::ElementDocument* document_{};
	Rml::Element* selectedElement_{};
	std::vector<Listener> listeners_;
	UiDesignerModel model_;
	PauseHandler pauseHandler_;
	std::string surfaceName_ = "main";
	bool pauseOwned_{};
	bool pausedBeforeDesign_{};
	std::string pendingComponent_;
	std::size_t nextGeneratedId_ = 1;
};

} // namespace ingnomia::ui::designer
