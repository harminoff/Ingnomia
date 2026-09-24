/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "InspectorController.h"
#include "../../localization/UiText.h"
#include <RmlUi/Core/EventListener.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace Rml { class Context; class Element; class ElementDocument; class Event; }
namespace ingnomia::ui::inspector
{
class InspectorRmlBinding final : public InspectorViewPort
{
public:
	using DocumentLoader = std::function<Rml::ElementDocument*()>;
	explicit InspectorRmlBinding( Rml::Context&, int cameraSlot = 0, int windowIndex = 0, bool detachedWindow = false );
	~InspectorRmlBinding() override;
	void setDocumentLoader( DocumentLoader loader ) { documentLoader_ = std::move( loader ); }
	bool initialize( InspectorController& );
	bool reloadDocument( InspectorController&, bool preserveUiState = false );
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] bool scrollLiveTile( int x, int y, float step );
	[[nodiscard]] bool stockpileItemIconInline( std::string* detail = nullptr );
	[[nodiscard]] Rml::ElementDocument* document() const noexcept { return document_; }
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	void setCloseHandler( std::function<void()> handler ) { closeHandler_ = std::move( handler ); }
	void setReplaceFloorHandler( std::function<void()> handler ) { replaceFloorHandler_ = std::move( handler ); }
	void setExpertiseOpenedHandler( std::function<void()> handler ) { expertiseOpenedHandler_ = std::move( handler ); }
	void shutdown();
	void stateChanged( const InspectorState& ) override;
	void setLiveInspection( bool active ) { liveInspection_ = active; if(controller_)stateChanged(controller_->state()); }
private:
	enum class PreviewPage
	{
		Camera,
		Stats,
		Expertise,
		Equipment,
		Inventory
	};
	enum class SkillSort
	{
		Name,
		Level,
		Active
	};
	class Callback final : public Rml::EventListener { public: explicit Callback(std::function<void(Rml::Event&)> f):fn_(std::move(f)){}void ProcessEvent(Rml::Event&)override;private:std::function<void(Rml::Event&)>fn_;};
	struct Listener { Rml::Element* target{}; std::string event; std::unique_ptr<Callback> callback; };
	void bind(const char*,std::function<void()>);void bindEvent(const char*,const char*,std::function<void(Rml::Event&)>);void bindElement(Rml::Element*,const char*,std::function<void(Rml::Event&)>,std::vector<Listener>&);void clearListeners(std::vector<Listener>&);void text(const char*,const std::string&);void rml(const char*,const std::string&);void visible(const char*,bool);void rows(const char*,const std::vector<TextCountRow>&);void renderLiveTileRows(const TileInspectorState*);void renderEquipment(const InspectorState&);bool previewPageAvailable(PreviewPage) const;void setPreviewPage(PreviewPage);void syncPreviewPageButtons();void setSkillSort(SkillSort);void syncSkillSortButtons();void positionTileLabel(const SelectionConfigurationState&);void positionSelectionTip(const SelectionConfigurationState&);
    Rml::Context& context_; InspectorController* controller_{}; Rml::ElementDocument* document_{};
    localization::UiText textCatalog_;
	std::vector<Listener> listeners_, professionListeners_, equipmentListeners_, liveTileListeners_;
	std::string liveTileRowsMarkup_;
	std::uint32_t liveTileId_{};
	std::function<void()> closeHandler_;
	std::function<void()> replaceFloorHandler_;
	std::function<void()> expertiseOpenedHandler_;
	DocumentLoader documentLoader_;
	std::vector<TextCountRow> lastSkillRows_;
	std::uint32_t lastSkillCreatureId_{};
	std::size_t skillScrollOffset_{};
	std::optional<std::string> renderedPreviewSkillRows_;
	std::optional<std::string> renderedFullSkillRows_;
	std::vector<std::string> renderedProfessionChoices_;
	PreviewPage previewPage_{ PreviewPage::Camera };
	SkillSort skillSort_{ SkillSort::Name };
	std::uint32_t previewCreatureId_{};
	bool lastSkillPanelOpen_{};
	bool professionMenuOpen_{};
	int cameraSlot_{};
	int windowIndex_{};
	bool detachedWindow_{};
	bool liveInspection_{};
    std::string cameraSource_ = "?camera-preview://selected";
};
} // namespace ingnomia::ui::inspector
