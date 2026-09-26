/*
	This file is part of Ingnomia https://github.com/rschurade/Ingnomia
    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/** @file mainwindow.h
 *  @brief MainWindow: the QWindow + QOpenGLContext that hosts the RmlUi interface and the
 *         MainWindowRenderer. Handles keyboard/mouse input, fullscreen toggling, and
 *         routes events between Qt, RmlUi, and the game.
 */
#pragma once

#include "../base/position.h"
#include "../base/tile.h"
#include "ui/state/UiFoundationTypes.h"

#include <QElapsedTimer>
#include <QVariant>
#include <QWindow>
#include <QTimer>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class QOpenGLContext;
namespace Rml { class Element; }

namespace ingnomia::ui {
class RmlUiHost;
class MainWindowFrame;
namespace whats_this { class Controller; }
class RmlUiDetachedContext;
class RmlUiDetachedWindow;
namespace shell { class ShellController; class ShellQtCommandPort; class ShellRmlBinding; }
namespace hud { class HudController; class HudQtCommandPort; class HudRmlBinding; }
namespace inspector { struct CreatureInspectorState; struct TileInspectorState; class InspectorController; class InspectorQtCommandPort; class InspectorRmlBinding; }
namespace management6b {
class Management6BController;
class Management6BQtCommandPort;
class Management6BQtDataAdapter;
class Management6BRmlBinding;
}
namespace management6a { class Management6AIntegration; class Management6AController; class Management6ARmlBinding; }
namespace management6c {
class Management6CQtCommandPort;
class Management6CRmlBinding;
class Management6CController;
class Management6CQtBridge;
}
namespace designer { class UiDesignerRmlBinding; }
namespace navigation { class WorkbenchCoordinator; }
#if defined(INGNOMIA_DEVELOPER_UI)
namespace debug {
class DebugQtCommandPort;
class DebugController;
class DebugQtDataAdapter;
class DebugRmlBinding;
}
#endif
}

/// @brief Bitfield tracking which WASD-style camera keys are currently held.
enum class KeyboardMove : unsigned char
{
	None  = 0,     ///< No keys held.
	Up    = 0x01,  ///< Up/W.
	Down  = 0x02,  ///< Down/S.
	Left  = 0x04,  ///< Left/A.
	Right = 0x08   ///< Right/D.
};

inline KeyboardMove operator+( KeyboardMove a, KeyboardMove b )
{
	return static_cast<KeyboardMove>( static_cast<unsigned char>( a ) | static_cast<unsigned char>( b ) );
}

inline KeyboardMove operator&( KeyboardMove a, KeyboardMove b )
{
	return static_cast<KeyboardMove>( static_cast<unsigned char>( a ) & static_cast<unsigned char>( b ) );
}

inline KeyboardMove operator-( KeyboardMove a )
{
	return static_cast<KeyboardMove>( ~static_cast<unsigned char>( a ) );
}

inline KeyboardMove& operator+=( KeyboardMove& a, KeyboardMove b )
{
	return a = a + b;
}

inline KeyboardMove& operator-=( KeyboardMove& a, KeyboardMove b )
{
	return a = a & -b;
}

struct Position;
class MainWindowRenderer;

/// @brief Top-level game window. A bare QWindow with a manual QOpenGLContext. Hosts the
///        RmlUi composition plus a MainWindowRenderer that draws the game world.
class MainWindow : public QWindow
{
	Q_OBJECT

public:
	MainWindow( QWidget* parent = Q_NULLPTR );
	~MainWindow();
	static MainWindow& getInstance();

	MainWindowRenderer* renderer();
	/// @brief Dispatches a diagnostic click through the live HUD RmlUi listener.
	///        This is also used by automated UI smoke tests; normal input remains
	///        routed through MainWindow's event boundary.
	bool activateHudElement( std::string_view id );
	/// @brief Re-arms the one-shot production framebuffer capture for a diagnostic probe.
	void armUiCapture();
	/// @brief Dispatches a diagnostic click through the live inspector RmlUi listener.
	///        This is opt-in and exists only for production inspector probes.
	bool activateInspectorElement( std::string_view id );
	/// @brief Activates an element in every live inspector window for multi-window UI probes.
	int activateInspectorElementInAllWindows( std::string_view id );
	/// @brief Opens a deterministic synthetic creature in the live inspector for UI probes.
	///        This does not touch the simulation and is only enabled by an explicit
	///        INGNOMIA_AUTOMATE_UI_FIXTURE launch environment.
	bool showInspectorCreatureFixture();
	bool showInspectorBlueprintFixture();
	bool showInspectorStockpileFixture();
	void setTileInspection( bool active );
	bool showManagementStockpileFixture();
 bool showManagementWorkshopFixture();
    std::string stockpileStage09Probe(std::string_view action);
    /// Stage 21 live probe for the Inventory window (INGNOMIA_AUTOMATE_STAGE21_INVENTORY=1): find:<text>, select-first,
    /// open, tab:<0-3>, watch, close-detail, click:<id> and state.
    std::string inventoryStage21Probe(std::string_view action);
    std::string workshopStage10Probe(std::string_view action);
    std::string agricultureStage11Probe(std::string_view action);
    std::string populationStage12Probe(std::string_view action);
    std::string militaryStage14Probe(std::string_view action);
    std::string diplomacyStage15Probe(std::string_view action);
    std::string inspectorStage16Probe(std::string_view action);
    std::string hudStage17Probe(std::string_view action);
    /// @brief Stage 17 live probe of the primary window frame: "state", "click:<id>", "dblclick" on the caption
    ///        and "restore". Opt-in with INGNOMIA_AUTOMATE_HUD_STAGE17_LIVE.
    std::string windowFrameProbe(std::string_view action);
    /// @brief Stage 10 live probe of What's This? in the workshop property sheet ("press:<id>", "state").
    std::string workshopWhatsThisProbe(std::string_view action);
    std::string shellStage18Probe(std::string_view action);
	bool showManagementFarmFixture();
	bool showInventoryFixture();
	/// @brief Loads the shared component gallery into the primary production RmlUi host.
	///        This is restricted to the explicit INGNOMIA_AUTOMATE_UI_FIXTURE probe.
	bool showComponentFixture();
	/// @brief Confirms that the production engine generated the fixture select and scrollbar parts.
	bool verifyComponentFixture( std::string* detail = nullptr );
	/// @brief Validates that the first stockpile item icon and label share one row.
	bool verifyInspectorStockpileItemGeometry( std::string* detail = nullptr );
	/// @brief Returns the current typed HUD status for opt-in production probes.
	std::string hudStatus() const;
	/// @brief Dispatches a diagnostic click through the live shell RmlUi listener.
	///        This is opt-in and exists only for production route smoke tests.
	bool activateShellElement( std::string_view id );
	bool dispatchShellClickForProbe( std::string_view id, std::string* focusedTarget = nullptr );
	std::string shellRouteForProbe() const;
	std::string shellFocusedElementForProbe() const;
	std::string verifyShellTabsForProbe();
	bool dispatchShellSettingChangeForProbe( std::string_view id, float value, bool checked );
	/// @brief Dispatches a diagnostic click through a live management document.
	///        This is opt-in and used only by production save/workbench probes.
	bool activateManagementElement( std::string_view id );
	bool clickManagementFarmCropForProbe( std::string_view crop );
	std::string managementFarmSelectedCropForProbe() const;
	bool requestManagementCaptureForProbe( std::string_view kind, const QString& path );
	bool createPopulationProfessionForProbe( std::string_view name );
	bool populationHasProfessionForProbe( std::string_view name ) const;
	bool verifyManagementWindowsForProbe(bool closePeers);
	std::string workshopSettingsStatusForProbe() const;
	bool managementFarmReadyForProbe() const;
	/// @brief Sets a live management form control for an opt-in production probe.
	///        Normal input remains routed through MainWindow's event boundary.
	bool setManagementFormValueForProbe( std::string_view id, std::string_view value );
	/// @brief Sets the live Stockpile search for an opt-in mixed-filter production probe.
	bool setManagementStockpileSearchForProbe( std::string_view value );
	/// @brief Activates the first live Stockpile material row in the requested state.
	bool activateFirstManagementStockpileFilterForProbe();
	/// @brief Activates a specific live Stockpile item/material row for a copied-save probe.
	bool activateManagementStockpileMaterialForProbe( std::string_view item, std::string_view material );
	/// @brief Selects the first live mixed Stockpile item row for an opt-in visual probe.
	bool selectFirstManagementMixedStockpileFilterForProbe();
	/// @brief Sends an opt-in key event through the live selected Stockpile filter row.
	bool dispatchManagementStockpileFilterKeyForProbe( int keyIdentifier );
	bool activateFirstManagementElement( std::string_view kind );
	/// @brief Opt-in runtime probe helpers for the Inventory report keyboard route.
	bool focusInventoryRowsForProbe();
	bool dispatchInventoryKeyForProbe( int qtKey );
	std::string inventoryWatchStatusForProbe() const;
	/// @brief Opt-in production probe for the authoritative inventory history path.
	bool requestInventoryHistoryProbe();
	std::string inventoryHistoryStatus() const;

	/// @brief Returns the owned QOpenGLContext.
	QOpenGLContext* context() const { return m_context; }
	void makeCurrent();
	void doneCurrent();

protected:
	bool event( QEvent* event ) override;
	void closeEvent( QCloseEvent* event ) override;
	void exposeEvent( QExposeEvent* event ) override;
	void resizeEvent( QResizeEvent* event ) override;

	void keyPressEvent( QKeyEvent* event ) override;
	void keyReleaseEvent( QKeyEvent* event ) override;
	void mouseMoveEvent( QMouseEvent* event ) override;
	void mousePressEvent( QMouseEvent* event ) override;
	void mouseReleaseEvent( QMouseEvent* event ) override;
	void mouseDoubleClickEvent( QMouseEvent* event ) override;
	bool nativeEvent( const QByteArray& eventType, void* message, qintptr* result ) override;
	ingnomia::ui::whats_this::Controller& whatsThis();
	void wheelEvent( QWheelEvent* event ) override;
	void focusInEvent( QFocusEvent* e ) override;
	void focusOutEvent( QFocusEvent* e ) override;

private:
	void initializeGL();
	void paintGL();
	void resizeGL( int w, int h );

	bool initializeRmlUi();
	void refreshContinueSave();
	void shutdownRmlUi();
	void resizeRmlUi();
	bool reloadRmlUiDocuments();
	bool rmlUiActive() const;
	bool beginRmlWindowDrag( QPointF position );
	void updateRmlWindowDrag( QPointF position );
	void endRmlWindowDrag();
	int frameTimerIntervalMs() const;
	void restartFrameTimer();
	bool openDetachedCreatureInspector( const ingnomia::ui::inspector::CreatureInspectorState&, std::optional<ingnomia::ui::WorldPosition> position );
	bool openDetachedBlueprintInspector( const ingnomia::ui::inspector::TileInspectorState& );
	bool openDetachedLiveTileInspector();
	void closeDetachedCreatureInspector( int slot );
	bool ensureDetachedManagement6A(int view = 0);
	bool ensureDetachedManagement6B( bool inventoryView = false );
	bool ensureDetachedManagement6C(bool diplomacyView = false);
	bool openDetachedOrdersTools( std::string_view elementId );
	bool ensureDetachedOrdersTools( std::string_view elementId );
	void hideDetachedManagement6A(int view = 0);
	void hideDetachedManagement6B(int surface = -1);
	void hideDetachedManagement6C(int surface = -1);
	void hideDetachedOrdersTools();
	void hideDetachedOrdersTools( std::string_view panelKey );
	void hideDetachedManagementWindows();
	void destroyDetachedManagementWindows();
	void closeDetachedManagement6B();
	void closeDetachedManagement6C();
#if defined( INGNOMIA_UI_DESIGNER )
	bool uiDesignerEnabled() const;
	std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> createDetachedUiDesigner(
		ingnomia::ui::RmlUiDetachedContext&, std::string surfaceName );
	void wireDetachedUiDesigner( ingnomia::ui::RmlUiDetachedWindow&, ingnomia::ui::designer::UiDesignerRmlBinding* );
	void focusUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* );
	void toggleUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* );
	void deactivateUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* );
#endif

	void keyboardZPlus( bool shift = false, bool ctrl = false );
	void keyboardZMinus( bool shift = false, bool ctrl = false );

	void toggleFullScreen();
	bool m_isFullScreen = false;                 ///< True when the window is currently fullscreen.

	void onExit();

	QOpenGLContext* m_context = nullptr;         ///< Owned GL context.
	bool m_glInitialized = false;                ///< True once initializeGL() has run.

	QTimer* m_timer = nullptr;                   ///< Frame pacing timer for menu and gameplay rendering.
	QElapsedTimer m_keyboardMovementTimer;       ///< Measures time between keyboardMove ticks.

	MainWindowRenderer* m_renderer = nullptr;    ///< Game world renderer.
	std::unique_ptr<ingnomia::ui::RmlUiHost> m_rmlUiHost; ///< UI host, owned before the GL context.
	std::unique_ptr<ingnomia::ui::whats_this::Controller> m_whatsThis; ///< What's This? mode, pop-up and menu; released before m_rmlUiHost shuts down.
	std::unique_ptr<ingnomia::ui::MainWindowFrame> m_windowFrame; ///< Windows 98 frame drawn in the main context; released before m_rmlUiHost shuts down.
	ingnomia::ui::shell::ShellRmlBinding* m_shellBinding = nullptr; ///< Borrowed from m_rmlUiHost.
	std::unique_ptr<ingnomia::ui::shell::ShellQtCommandPort> m_shellCommands;
	std::unique_ptr<ingnomia::ui::shell::ShellController> m_shellController;
	ingnomia::ui::hud::HudRmlBinding* m_hudBinding = nullptr; ///< Borrowed from m_rmlUiHost.
	bool m_tileInspectionActive = false;
	ingnomia::ui::inspector::InspectorRmlBinding* m_inspectorBinding = nullptr; ///< Borrowed from m_rmlUiHost.
	std::unique_ptr<ingnomia::ui::hud::HudQtCommandPort> m_hudCommands;
	std::unique_ptr<ingnomia::ui::hud::HudController> m_hudController;
	std::unique_ptr<ingnomia::ui::inspector::InspectorQtCommandPort> m_inspectorCommands;
	std::unique_ptr<ingnomia::ui::inspector::InspectorController> m_inspectorController;
	struct CreatureInspectorWindow
	{
		int slot{};
		std::unique_ptr<ingnomia::ui::RmlUiDetachedContext> context;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedWindow> nativeWindow;
		ingnomia::ui::inspector::InspectorRmlBinding* binding{};
		std::unique_ptr<ingnomia::ui::inspector::InspectorQtCommandPort> commands;
		std::unique_ptr<ingnomia::ui::inspector::InspectorController> controller;
#if defined( INGNOMIA_UI_DESIGNER )
		std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> designer;
#endif
	};
	std::vector<CreatureInspectorWindow> m_creatureInspectorWindows;
	std::vector<std::string> m_creatureProfessionChoices;
	struct Management6AWindow
	{
        int view{};
		std::unique_ptr<ingnomia::ui::RmlUiDetachedContext> context;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedWindow> nativeWindow;
		std::unique_ptr<ingnomia::ui::management6a::Management6ARmlBinding> binding;
#if defined( INGNOMIA_UI_DESIGNER )
		std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> designer;
#endif
	};
	struct Management6BWindow
	{
		std::unique_ptr<ingnomia::ui::RmlUiDetachedContext> context;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedWindow> nativeWindow;
		std::unique_ptr<ingnomia::ui::management6b::Management6BRmlBinding> binding;
#if defined( INGNOMIA_UI_DESIGNER )
		std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> designer;
#endif
	};
	struct Management6CWindow
	{
		std::unique_ptr<ingnomia::ui::RmlUiDetachedContext> context;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedWindow> nativeWindow;
		std::unique_ptr<ingnomia::ui::management6c::Management6CRmlBinding> binding;
#if defined( INGNOMIA_UI_DESIGNER )
		std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> designer;
#endif
	};
	struct OrdersToolsWindow
	{
		std::string panelKey;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedContext> context;
		std::unique_ptr<ingnomia::ui::RmlUiDetachedWindow> nativeWindow;
		std::unique_ptr<ingnomia::ui::hud::HudRmlBinding> binding;
#if defined( INGNOMIA_UI_DESIGNER )
		std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> designer;
#endif
	};
	std::vector<std::unique_ptr<OrdersToolsWindow>> m_ordersToolsWindows;
	std::unique_ptr<Management6AWindow> m_management6aWindow, m_stockpileWindow, m_agricultureWindow;
	std::unique_ptr<Management6BWindow> m_management6bWindow, m_inventoryWindow;
	std::unique_ptr<Management6CWindow> m_management6cWindow, m_diplomacyWindow;
	ingnomia::ui::management6b::Management6BRmlBinding* m_management6bBinding = nullptr; ///< Borrowed from m_rmlUiHost.
	std::unique_ptr<ingnomia::ui::management6b::Management6BQtCommandPort> m_management6bCommands;
	std::unique_ptr<ingnomia::ui::management6b::Management6BController> m_management6bController;
	std::unique_ptr<ingnomia::ui::management6b::Management6BQtDataAdapter> m_management6bData;
	std::unique_ptr<ingnomia::ui::management6a::Management6AIntegration> m_management6a;
	std::unique_ptr<ingnomia::ui::management6c::Management6CQtCommandPort> m_management6cCommands;
	std::unique_ptr<ingnomia::ui::management6c::Management6CRmlBinding> m_management6cBinding;
	std::unique_ptr<ingnomia::ui::management6c::Management6CController> m_management6cController;
	std::unique_ptr<ingnomia::ui::management6c::Management6CQtBridge> m_management6cBridge;
	std::unique_ptr<ingnomia::ui::navigation::WorkbenchCoordinator> m_workbenchCoordinator;
#if defined( INGNOMIA_UI_DESIGNER )
	std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> m_uiDesigner;
	ingnomia::ui::designer::UiDesignerRmlBinding* m_uiDesignerActive = nullptr;
#endif
#if defined(INGNOMIA_DEVELOPER_UI)
	ingnomia::ui::debug::DebugRmlBinding* m_debugBinding = nullptr;
	std::unique_ptr<ingnomia::ui::debug::DebugQtCommandPort> m_debugCommands;
	std::unique_ptr<ingnomia::ui::debug::DebugController> m_debugController;
	std::unique_ptr<ingnomia::ui::debug::DebugQtDataAdapter> m_debugData;
#endif
	std::uint64_t m_uiWorldEpoch = 0;
	bool m_uiCompositionActive = false;

	int m_clickX = 0;                            ///< Global X of last mouse click.
	int m_clickY = 0;                            ///< Global Y of last mouse click.
	int m_mouseX = 0;                            ///< Window-local X of last mouse click.
	int m_mouseY = 0;                            ///< Window-local Y of last mouse click.
	int m_moveX = 0;                             ///< Global X of last mouse drag event.
	int m_moveY = 0;                             ///< Global Y of last mouse drag event.

	bool m_leftDown  = false;                    ///< True while the left mouse button is pressed.
	bool m_rightDown = false;                    ///< True while the right mouse button is pressed.
	bool m_isMove    = false;                    ///< True after the pointer moves past the click threshold.
	bool m_draggingAreaSelection = false;       ///< Area tools anchor on press and commit on drag release.
	bool m_rmlWindowDragging = false;            ///< True while an RmlUi move_target handle is moving a panel.
	Rml::Element* m_rmlWindowDragTarget = nullptr; ///< Non-owning target element for the active panel drag.
	Rml::Element* m_rmlWindowDragParent = nullptr; ///< Non-owning offset parent for the active panel drag.
	QPointF m_rmlWindowDragStartPointer;
	QPointF m_rmlWindowDragStartOffset;

	bool m_pendingUpdate = false;                ///< True when a QEvent::UpdateRequest is already queued.
	bool m_uiCaptureDone = false;                ///< One-shot diagnostic framebuffer capture guard.
	std::uint32_t m_uiFrameCount = 0;            ///< Frames rendered for the optional capture probe.

	KeyboardMove m_keyboardMove = KeyboardMove::None; ///< Current held-key bitfield for camera panning.

public slots:
	void redraw();
	void idleRenderTick();
	void onFullScreen( bool value );
	void keyboardMove();

	void onSetWindowSize( int width, int height );
	void onUiSetViewLevel( int level );

	void onInitViewAfterLoad();

	/// @brief Broadcasts the renderer's current camera params to interested aggregators
	///        (selection, sound, …) by emitting signalRenderParams. Used by paths that
	///        change camera state without going through wheelEvent / onInitViewAfterLoad
	///        (e.g. onCenterCameraPosition on a fresh new-game embark).
	void pushRenderParams();

signals:
	void signalWindowSize( int w, int h );
	void signalViewLevel( int level );


	void signalKeyPress( int key );
	void signalUpdateRenderOptions();
	void signalTogglePause();

	void signalRenderParams( int width, int height, int moveX, int moveY, float scale, int rotation );
	void signalRotateSelection();
	void signalMouse( int mouseX, int mouseY, bool shift, bool ctrl );
	void signalLeftClick( bool shift, bool ctrl );
	void signalRightClick();
};
