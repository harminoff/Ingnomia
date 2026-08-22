/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "InspectorController.h"
#include "../../localization/UiText.h"
#include <RmlUi/Core/EventListener.h>
#include <functional>
#include <memory>
#include <vector>
namespace Rml { class Context; class Element; class ElementDocument; class Event; }
namespace ingnomia::ui::inspector
{
class InspectorRmlBinding final : public InspectorViewPort
{
public:
    explicit InspectorRmlBinding( Rml::Context&, int cameraSlot = 0, int windowIndex = 0 );
	~InspectorRmlBinding() override;
	bool initialize( InspectorController& );
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] Rml::ElementDocument* document() const noexcept { return document_; }
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	void shutdown();
	void stateChanged( const InspectorState& ) override;
private:
	class Callback final : public Rml::EventListener { public: explicit Callback(std::function<void(Rml::Event&)> f):fn_(std::move(f)){}void ProcessEvent(Rml::Event&)override;private:std::function<void(Rml::Event&)>fn_;};
	struct Listener { Rml::Element* target{}; std::string event; std::unique_ptr<Callback> callback; };
	void bind(const char*,std::function<void()>);void bindEvent(const char*,const char*,std::function<void(Rml::Event&)>);void bindElement(Rml::Element*,const char*,std::function<void(Rml::Event&)>,std::vector<Listener>&);void clearListeners(std::vector<Listener>&);void text(const char*,const std::string&);void rml(const char*,const std::string&);void visible(const char*,bool);void rows(const char*,const std::vector<TextCountRow>&);void positionTileLabel(const SelectionConfigurationState&);void positionSelectionTip(const SelectionConfigurationState&);
    Rml::Context& context_; InspectorController* controller_{}; Rml::ElementDocument* document_{};
    localization::UiText textCatalog_;
    std::vector<Listener> listeners_, professionListeners_;
    int cameraSlot_{};
    int windowIndex_{};
    std::string cameraSource_ = "?camera-preview://selected";
};
} // namespace ingnomia::ui::inspector
