/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "Management6CController.h"
#include "Management6CDomWindow.h"
#include "Management6CText.h"
#include "../../runtime/ModalDialog.h"

#include <RmlUi/Core/EventListener.h>

#include <array>
#include <map>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Rml
{
class Context;
class Element;
class ElementDocument;
class Event;
}

namespace ingnomia::ui::management6c
{

class Management6CRmlBinding final : public ViewPort
{
public:
	using DocumentLoader = std::function<Rml::ElementDocument*( const char* )>;
	using RouteCloseHandler = std::function<void( RouteId, FocusToken )>;

	explicit Management6CRmlBinding( Rml::Context& );
	~Management6CRmlBinding() override;
	bool initialize( Management6CController& );
	bool reloadDocuments();
	void shutdown();
	void setDocumentLoader( DocumentLoader loader ) { documentLoader_ = std::move( loader ); }
	void setPresentationEnabled( bool enabled ) { presentationEnabled_ = enabled; }
    void setWindowSurface(bool secondary) { secondarySurface_ = secondary; }
	bool openMilitary( View, FocusToken );
	bool openDiplomacy( View, FocusToken );
	void closeMilitary();
	void closeDiplomacy();
	void closeRoute();
	void setRouteCloseHandler( RouteCloseHandler handler ) { routeCloseHandler_ = std::move( handler ); }
	void stateChanged( const Management6CState& ) override;
	[[nodiscard]] bool activateElement( std::string_view );
	[[nodiscard]] bool activateFirstDataElement( std::string_view kind );
	[[nodiscard]] std::size_t listenerCount() const noexcept { return listeners_.size(); }
	[[nodiscard]] std::size_t dynamicRenderCount() const noexcept { return dynamicRenderCount_; }
	[[nodiscard]] Rml::ElementDocument* militaryDocument() const noexcept { return military_; }
	[[nodiscard]] Rml::ElementDocument* diplomacyDocument() const noexcept { return diplomacy_; }

private:
	enum class RowSurface : std::uint8_t
	{
		Squads, Roles, Members, Unassigned, Priorities, UniformSlots, UniformTypes, UniformMaterials,
		Neighbors, Missions, Gnomes, MemberRoles, Count
	};
	struct WindowState
	{
		std::size_t begin{};
		std::string selectedKey;
		bool manualPage{};
	};

	class Callback final : public Rml::EventListener
	{
	public:
		explicit Callback( std::function<void( Rml::Event& )> function ) : function_( std::move( function ) ) {}
		void ProcessEvent( Rml::Event& event ) override { function_( event ); }
	private:
		std::function<void( Rml::Event& )> function_;
	};

	void bind( Rml::ElementDocument*, const char*, std::function<void()> );
	void bindEvent( Rml::ElementDocument*, const char*, const char*, std::function<void( Rml::Event& )> );
	void syncDocuments( const Management6CState& );
	void renderMilitary( const Management6CState& );
	void renderDiplomacy( const Management6CState& );
	void text( Rml::ElementDocument*, const char*, const std::string& );
	void rml( Rml::ElementDocument*, const char*, const std::string& );
	void visible( Rml::ElementDocument*, const char*, bool );
	void selected( Rml::ElementDocument*, const char*, bool );
	void enabled( Rml::ElementDocument*, const char*, bool );
	void inputValue( Rml::ElementDocument*, const char*, const std::string& );
	void focusCurrentRow();
	void showDetails( bool );
	std::string tr( const char* ) const;
	std::string choices( RowSurface, const std::vector<std::pair<std::string, std::string>>&, const std::string&, const char* );
	void focusSelectedMember();
	void focusSelectedPriority();
	void focusSelectedUniform();
	void focusSelectedGnome();
	[[nodiscard]] DomWindow windowFor( RowSurface, std::size_t, std::optional<std::size_t>, std::string );
	[[nodiscard]] bool handleWindowPage( Rml::Event& );
	static const char* rowSurfaceName( RowSurface );
	static std::optional<RowSurface> rowSurface( std::string_view );
	static void appendWindowControls( std::string&, RowSurface, const DomWindow&, std::size_t );

	Rml::Context& context_;
	Management6CController* controller_{};
	Rml::ElementDocument* military_{};
	Rml::ElementDocument* diplomacy_{};
	Rml::ElementDocument* shown_{};
	std::optional<RouteId> activeRoute_;
	FocusToken returnFocus_;
	FocusToken militaryFocus_;
	FocusToken diplomacyFocus_;
	RouteCloseHandler routeCloseHandler_;
	bool confirmationVisible_{};
	bool rendering_{};
	bool detailOpen_{};
	bool filtersOpen_{};
	std::optional<View> renderedView_;
	enum class MilitaryPage : std::uint8_t { Squads, Members, Roles, Uniforms, Targets };
	MilitaryPage militaryPage_{ MilitaryPage::Squads };
	std::map<std::string, std::string> renderedMilitaryOptions_;
	ModalDialog dialog_{ context_ };
	ModalInstanceId shownDestructive_{};
	// Send Mission wizard: 0 closed, 1 Mission, 2 Citizen, 3 Review.
	int wizardPage_{};
	std::optional<NeighborId> wizardNeighbor_;
	MissionDraft reviewedDraft_;
	std::string wizardNote_;
	void openWizard();
	void closeWizard();
	void wizardNext();
	void finishWizard();
	WorldEpoch renderedWorld_;
	DestructiveKind lastDestructiveKind_{ DestructiveKind::Squad };
	std::array<WindowState, static_cast<std::size_t>( RowSurface::Count )> windows_{};
	std::unordered_map<std::string, std::string> renderedRml_;
	std::size_t dynamicRenderCount_{};
	localization::UiText textCatalog_{ management6cEnglishText() };
	struct Listener
	{
		Rml::Element* target{};
		std::string event;
		std::unique_ptr<Callback> callback;
	};
	std::vector<Listener> listeners_;
	DocumentLoader documentLoader_;
	std::optional<bool> secondarySurface_;bool presentationEnabled_{ true };
};

} // namespace ingnomia::ui::management6c
