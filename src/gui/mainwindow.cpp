/*
This file is part of Ingnomia https://github.com/rschurade/Ingnomia    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team    This program is free software: you can redistribute it and/or modify    it under the terms of the GNU Affero General Public License as    published by the Free Software Foundation, either version 3 of the    License, or (at your option) any later version.    This program is distributed in the hope that it will be useful,    but WITHOUT ANY WARRANTY; without even the implied warranty of    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    GNU Affero General Public License for more details.    You should have received a copy of the GNU Affero General Public License    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/

#include "mainwindow.h"

#if defined( Q_OS_WIN )
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "../../tests/ui-shell/runtime_tab_probe.h"
#include "../../tests/ui-shell/runtime_host_probe.h"
#include "../../tests/ui-stage05/runtime_probe.h"
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include "../base/config.h"
#include "../base/io.h"
#include "../gui/aggregatorselection.h"
#include "../gui/eventconnector.h"
#include "aggregatoragri.h"
#include "aggregatorcreatureinfo.h"
#include "aggregatorinventory.h"
#include "aggregatorloadgame.h"
#include "aggregatorpopulation.h"
#include "aggregatorsettings.h"
#include "aggregatorstockpile.h"
#include "aggregatortileinfo.h"
#include "aggregatorworkshop.h"
#include "mainwindowrenderer.h"
#include "ui/controllers/hud/HudQtCommandPort.h"
#include "ui/controllers/inspector/InspectorQtCommandPort.h"
#include "ui/controllers/inspector/InspectorQtDataAdapter.h"
#include "ui/controllers/management6a/Management6AIntegration.h"
#include "ui/controllers/management6b/Management6BQtCommandPort.h"
#include "ui/controllers/management6b/Management6BQtDataAdapter.h"
#include "ui/controllers/management6c/Management6CQtBridge.h"
#include "ui/controllers/management6c/Management6CQtCommandPort.h"
#include "ui/controllers/shell/ShellQtCommandPort.h"
#include "ui/navigation/WorkbenchCoordinator.h"
#include "ui/runtime/RmlUiHost.h"
#include "ui/runtime/MainWindowFrame.h"
#include "ui/runtime/AccessKeys.h"
#include "ui/runtime/WindowMenu.h"
#include "ui/runtime/WhatsThis.h"
#include "ui/runtime/RmlUiDetachedWindow.h"
#include "ui/designer/UiDesignerRmlBinding.h"
#include "ui/screens/hud/HudRmlBinding.h"
#include "ui/screens/inspector/InspectorRmlBinding.h"
#include "ui/screens/management6b/Management6BRmlBinding.h"
#include "ui/screens/management6c/Management6CRmlBinding.h"
#include "ui/screens/shell/ShellDataAdapter.h"
#include "ui/screens/shell/ShellRmlBinding.h"
#if defined( INGNOMIA_DEVELOPER_UI )
#include "ui/controllers/developer_ui/DebugQtCommandPort.h"
#include "ui/controllers/developer_ui/DebugQtDataAdapter.h"
#include "ui/screens/developer_ui/DebugRmlBinding.h"
#endif
#if defined( INGNOMIA_DEVELOPER_UI )
#include "aggregatordebug.h"
#endif

#include <QCoreApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QExposeEvent>
#include <QFile>
#include <QFileInfo>
#include <QInputMethodEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTextStream>
#include <QPointer>
#include <QPointer>
#include <QGlobal.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include "ui/localization/RmlText.h"
#include <RmlUi/Core/ElementScroll.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <array>
#include <utility>
#include <string> /** @file mainwindow.cpp *  @brief MainWindow implementation: GL context bring-up, RmlUi host init, Qt event loop *         routing (keyboard/mouse/wheel/focus/resize/expose/update), fullscreen toggling, *         and the frame-timer used during menus. Also owns the global MainWindow singleton. */
#include <vector>
#include <algorithm>
#include <cmath>

#include <glad/gl.h>

static MainWindow* instance;

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

bool isDetachedOrdersToolsElement( std::string_view id )
{
	return id == "hud_tool_build" || id.starts_with( "hud_build_" );
}

using HudToolPanel = ingnomia::ui::hud::HudRmlBinding::ToolPanel;

std::string_view ordersToolsPanelKeyForElement( std::string_view id )
{
	if ( id == "hud_tool_build" || id.starts_with( "hud_build_" ) ) return "build";
	if ( id == "hud_tool_agriculture" || id.starts_with( "hud_agriculture_" ) || id == "hud_tool_fell_tree" || id == "hud_tool_plant_tree"
		|| id == "hud_tool_harvest_tree" || id == "hud_tool_forage" || id == "hud_tool_remove_plant" ) return "agriculture";
	if ( id == "hud_tool_designations" || id.starts_with( "hud_designations_" ) || id == "hud_tool_stockpile" || id == "hud_tool_farm"
		|| id == "hud_tool_grove" || id == "hud_tool_pasture" || id == "hud_tool_personal_room"
		|| id == "hud_tool_dormitory" || id == "hud_tool_dining_hall" || id == "hud_tool_hospital"
		|| id == "hud_tool_forbidden" || id == "hud_tool_remove_designation" ) return "designations";
	if ( id == "hud_tool_jobs" || id.starts_with( "hud_jobs_" ) || id == "hud_tool_suspend_job" || id == "hud_tool_resume_job"
		|| id == "hud_tool_cancel_job" || id == "hud_tool_lower_priority" || id == "hud_tool_raise_priority" ) return "jobs";
	if ( id == "hud_tool_mine" || id.starts_with( "hud_mine_" ) ) return "mine";
	return {};
}

HudToolPanel ordersToolsPanelForElement( std::string_view id )
{
	if ( id == "hud_tool_build" || id.starts_with( "hud_build_" ) ) return HudToolPanel::Build;
	if ( id == "hud_tool_agriculture" || id.starts_with( "hud_agriculture_" ) || id == "hud_tool_fell_tree" || id == "hud_tool_plant_tree"
		|| id == "hud_tool_harvest_tree" || id == "hud_tool_forage" || id == "hud_tool_remove_plant" ) return HudToolPanel::Agriculture;
	if ( id == "hud_tool_designations" || id.starts_with( "hud_designations_" ) || id == "hud_tool_stockpile" || id == "hud_tool_farm"
		|| id == "hud_tool_grove" || id == "hud_tool_pasture" || id == "hud_tool_personal_room"
		|| id == "hud_tool_dormitory" || id == "hud_tool_dining_hall" || id == "hud_tool_hospital"
		|| id == "hud_tool_forbidden" || id == "hud_tool_remove_designation" ) return HudToolPanel::Designations;
	if ( id == "hud_tool_jobs" || id.starts_with( "hud_jobs_" ) || id == "hud_tool_suspend_job" || id == "hud_tool_resume_job"
		|| id == "hud_tool_cancel_job" || id == "hud_tool_lower_priority" || id == "hud_tool_raise_priority" ) return HudToolPanel::Jobs;
	return HudToolPanel::Mine;
}

QString ordersToolsWindowTitle( HudToolPanel panel )
{
	switch ( panel )
	{
	case HudToolPanel::Build: return QStringLiteral( "Build" );
	case HudToolPanel::Agriculture: return QStringLiteral( "Agriculture" );
	case HudToolPanel::Designations: return QStringLiteral( "Designations" );
	case HudToolPanel::Jobs: return QStringLiteral( "Job commands" );
	case HudToolPanel::Mine: return QStringLiteral( "Mining orders" );
	}
	return QStringLiteral( "Orders & tools" );
}

QSize ordersToolsWindowSize( HudToolPanel panel )
{
	switch ( panel )
	{
	case HudToolPanel::Build: return QSize( 400, 720 );
	case HudToolPanel::Agriculture: return QSize( 270, 240 );
	case HudToolPanel::Designations: return QSize( 300, 430 );
	case HudToolPanel::Jobs: return QSize( 320, 290 );
	case HudToolPanel::Mine: return QSize( 260, 300 );
	}
	return QSize( 260, 300 );
}

QSize detachedWindowSize( const QString& key, QSize baseSize, float userScale, const QWindow* owner )
{
	// Detached windows have fixed layouts. Old resize preferences can be smaller
	// than the current layout and would leave its content clipped on first open.
	(void)key;
	QSize desired( qRound( baseSize.width() * userScale ), qRound( baseSize.height() * userScale ) );

	QRect available;
	if ( owner && owner->screen() ) available = owner->screen()->availableGeometry();
	if ( available.isValid() )
	{
		const QSize maximum( qMax( 240, available.width() - 16 ), qMax( 240, available.height() - 16 ) );
		desired.setWidth( qBound( 240, desired.width(), maximum.width() ) );
		desired.setHeight( qBound( 240, desired.height(), maximum.height() ) );
	}
	return desired.expandedTo( QSize( 240, 240 ) );
}

// Palette windows open at a fixed offset from the game window the first time, then where the player last put them
// (PDF p.160, p.181). The position is kept in the configuration under <key>.x / <key>.y.
QPoint palettePosition( const QString& key, QPoint firstTime )
{
	if ( !Global::cfg ) return firstTime;
	const auto x = Global::cfg->get( key + QStringLiteral( ".x" ) );
	const auto y = Global::cfg->get( key + QStringLiteral( ".y" ) );
	return x.isValid() && y.isValid() ? QPoint( x.toInt(), y.toInt() ) : firstTime;
}

void persistPalettePosition( const QString& key, QPoint position )
{
	if ( !Global::cfg ) return;
	Global::cfg->set( key + QStringLiteral( ".x" ), position.x() );
	Global::cfg->set( key + QStringLiteral( ".y" ), position.y() );
}

QString inspectorPositionKey( int slot )
{
	return slot == -1 ? QStringLiteral( "UiWindow.blueprint_inspector.position" )
		: slot == -2 ? QStringLiteral( "UiWindow.live_tile_inspector.position" )
		: QStringLiteral( "UiWindow.creature_inspector.%1.position" ).arg( slot );
}

void persistDetachedWindowSize( const QString& key, QSize size )
{
	if ( !Global::cfg || !size.isValid() ) return;
	Global::cfg->set( key + QStringLiteral( ".width" ), size.width() );
	Global::cfg->set( key + QStringLiteral( ".height" ), size.height() );
}
} // namespace

/// @brief Constructs the MainWindow: sets up a OpenGL 4.3 Core surface format, double
///        buffer, vsync off, and wires all signals into EventConnector and the selection
///        aggregator. The GL context is created lazily on the first exposeEvent.
/// @param parent Unused (legacy QWidget parent pointer).
MainWindow::MainWindow( QWidget* parent ) :

	QWindow()
{

	qDebug() << "Create main window."; // Set up OpenGL surface format
	// When the explicit post-load capture probe is requested, suppress the
	// initial menu frame.  The world load is asynchronous; allowing frame 1 to
	// satisfy the one-shot capture would leave the later post-load reset with no
	// useful evidence and make a loaded-world run look like a menu-only run.
	m_uiCaptureDone = qEnvironmentVariable( "INGNOMIA_UI_CAPTURE_AFTER_LOAD" ) == "1";
	QSurfaceFormat format;
	format.setRenderableType( QSurfaceFormat::OpenGL );
	format.setProfile( QSurfaceFormat::CoreProfile );
	format.setVersion( 4, 3 );
	format.setSwapBehavior( QSurfaceFormat::DoubleBuffer );
	// Detached RmlUi windows reuse this context and need an alpha channel so
	// their transparent host area can be composited outside the game window.
	format.setAlphaBufferSize( 8 );
	format.setDepthBufferSize( 24 );
	format.setStencilBufferSize( 8 );
	format.setSwapInterval( 0 ); // Disable vsync for max performance
	setFormat( format );
	setSurfaceType( QWindow::OpenGLSurface );
	// The Windows 98 frame is drawn by MainWindowFrame, so the native one is removed. The system menu and the
	// Minimize and Maximize styles stay so the taskbar button and Alt+Space keep working.
	setFlags( Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint );
	connect( Global::eventConnector, &EventConnector::signalExit, this, &MainWindow::onExit );
	connect( this, &MainWindow::signalWindowSize, Global::eventConnector, &EventConnector::onWindowSize );
	connect( this, &MainWindow::signalViewLevel, Global::eventConnector, &EventConnector::onViewLevel );
	connect( this, &MainWindow::signalKeyPress, Global::eventConnector, &EventConnector::onKeyPress );
	connect( this, &MainWindow::signalTogglePause, Global::eventConnector, &EventConnector::onTogglePause );
	connect( this, &MainWindow::signalUpdateRenderOptions, Global::eventConnector, &EventConnector::onUpdateRenderOptions );
	connect( this, &MainWindow::signalMouse, Global::eventConnector->aggregatorSelection(), &AggregatorSelection::onMouse, Qt::QueuedConnection );
	connect( this, &MainWindow::signalLeftClick, Global::eventConnector->aggregatorSelection(), &AggregatorSelection::onLeftClick, Qt::QueuedConnection );
	connect( this, &MainWindow::signalRightClick, Global::eventConnector->aggregatorSelection(), &AggregatorSelection::onRightClick, Qt::QueuedConnection );
	connect( this, &MainWindow::signalRotateSelection, Global::eventConnector->aggregatorSelection(), &AggregatorSelection::onRotateSelection, Qt::QueuedConnection );
	connect( this, &MainWindow::signalRenderParams, Global::eventConnector->aggregatorSelection(), &AggregatorSelection::onRenderParams, Qt::QueuedConnection );
#if defined( INGNOMIA_DEVELOPER_UI )
	connect( Global::eventConnector->aggregatorDebug(), &AggregatorDebug::signalSetWindowSize, this, &MainWindow::onSetWindowSize, Qt::QueuedConnection );
#endif
	connect( Global::eventConnector->aggregatorSettings(), &AggregatorSettings::signalFullScreen, this, &MainWindow::onFullScreen, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorSettings(), &AggregatorSettings::signalUIScale, this, [this]( float )

			 {


if ( rmlUiActive() )


{



resizeRmlUi();



redraw();


} }, Qt::QueuedConnection );
	connect( this, &QWindow::screenChanged, this, [this]( QScreen* )

			 {


if ( rmlUiActive() )


{



resizeRmlUi();



redraw();


} } );
	connect( Global::eventConnector, &EventConnector::signalInitView, this, &MainWindow::onInitViewAfterLoad, Qt::QueuedConnection );
	instance = this;
}

/// @brief Destructor: persists the last windowed-mode size/position to config, saves the
///        config file, and releases the GL context.
MainWindow::~MainWindow()
{

	qDebug() << "MainWindow destructor";

	if ( m_timer )

		m_timer->stop();

	shutdownRmlUi();

	if ( !m_isFullScreen )

	{

		Global::cfg->set( "WindowWidth", this->width() );

		Global::cfg->set( "WindowHeight", this->height() );

		Global::cfg->set( "WindowPosX", this->position().x() );

		Global::cfg->set( "WindowPosY", this->position().y() );
	}

	IO::saveConfig();

	if ( m_context )

	{

		delete m_context;

		m_context = nullptr;
	}

	instance = nullptr;
}

/// @brief Returns the global MainWindow singleton.
/// @return Reference to the single MainWindow instance.
MainWindow&
MainWindow::getInstance()
{

	return *instance;
}

bool MainWindow::activateHudElement( std::string_view id )
{
	if ( isDetachedOrdersToolsElement( id ) )
		return openDetachedOrdersTools( id );
	return m_hudBinding && m_hudBinding->activateElement( id );
}

void MainWindow::armUiCapture()
{
	m_uiCaptureDone = false;
	m_uiFrameCount = 0;
	redraw();
}

bool MainWindow::activateInspectorElement( std::string_view id )
{
	if ( m_inspectorBinding && m_inspectorBinding->activateElement( id ) ) return true;
	for ( auto& window : m_creatureInspectorWindows )
		if ( window.binding && window.binding->activateElement( id ) ) return true;
	return false;
}

int MainWindow::activateInspectorElementInAllWindows( std::string_view id )
{
	int activated = 0;
	if ( m_inspectorBinding && m_inspectorBinding->activateElement( id ) ) ++activated;
	for ( auto& window : m_creatureInspectorWindows )
		if ( window.binding && window.binding->activateElement( id ) ) ++activated;
	return activated;
}

bool MainWindow::showInspectorCreatureFixture()
{
	ingnomia::ui::inspector::CreatureInspectorState fixture;
	fixture.id = ingnomia::ui::CreatureId { 0xC0DEu };
	fixture.name = "UI Test Gnome";
	fixture.profession = "Gnomad";
	fixture.professionReported = true;
	fixture.skillsReported = true;
	fixture.equipmentReported = true;
	fixture.activity = "Walking to the stockpile";
	fixture.strength = 8;
	fixture.dexterity = 6;
	fixture.constitution = 7;
	fixture.intelligence = 5;
	fixture.wisdom = 9;
	fixture.charisma = 4;
	fixture.attributesReported = { true, true, true, true, true, true };
	fixture.hunger = 86;
	fixture.thirst = 72;
	fixture.sleep = 54;
	fixture.happiness = 91;
	fixture.needsReported = { true, true, true, true };
	fixture.skills = {
		{ "Animal Husbandry", "level 4 | active", 0 },
		{ "Woodcutting", "level 5 | active", 0 },
		{ "Mining", "level 3 | active", 0 },
		{ "Crafting", "level 2 | inactive", 0 },
		{ "Hauling", "level 4 | active", 0 },
		{ "Masonry", "level 3 | active", 0 },
	};
	fixture.equipment = {
		{ "Head", "Iron chain armor", 0 },
		{ "Chest", "Iron chain armor", 0 },
		{ "Right hand", "Iron sword", 0 },
		{ "Back", "Leather backpack", 0 },
	};
	fixture.equipmentRole = ingnomia::ui::MilitaryRoleId { 71 };
	fixture.equipmentRoleName = "Guard";
	const std::vector<ingnomia::ui::inspector::EquipmentTypeChoice> armorChoices {
		{ ingnomia::ui::CatalogId { "none" }, { ingnomia::ui::CatalogId { "any" } } },
		{ ingnomia::ui::CatalogId { "ChainArmor" }, { ingnomia::ui::CatalogId { "any" }, ingnomia::ui::CatalogId { "Iron" }, ingnomia::ui::CatalogId { "Copper" } } },
		{ ingnomia::ui::CatalogId { "PlateArmor" }, { ingnomia::ui::CatalogId { "any" }, ingnomia::ui::CatalogId { "Iron" }, ingnomia::ui::CatalogId { "Steel" } } },
	};
	const std::vector<ingnomia::ui::inspector::EquipmentTypeChoice> heldChoices {
		{ ingnomia::ui::CatalogId { "none" }, { ingnomia::ui::CatalogId { "any" } } },
		{ ingnomia::ui::CatalogId { "Sword" }, { ingnomia::ui::CatalogId { "any" }, ingnomia::ui::CatalogId { "Iron" }, ingnomia::ui::CatalogId { "Steel" } } },
		{ ingnomia::ui::CatalogId { "Shield" }, { ingnomia::ui::CatalogId { "any" }, ingnomia::ui::CatalogId { "Wood" }, ingnomia::ui::CatalogId { "Iron" } } },
	};
	fixture.equipmentSlots = {
		{ ingnomia::ui::UniformSlot::HeadArmor, "Head", "ChainArmorHead", "Iron", "../tilesheet/inventory_ChainArmorHead.tga", ingnomia::ui::CatalogId { "ChainArmor" }, ingnomia::ui::CatalogId { "Iron" }, armorChoices },
		{ ingnomia::ui::UniformSlot::ChestArmor, "Chest", "ChainArmorChest", "Iron", "../tilesheet/inventory_ChainArmorChest.tga", ingnomia::ui::CatalogId { "ChainArmor" }, ingnomia::ui::CatalogId { "Iron" }, armorChoices },
		{ ingnomia::ui::UniformSlot::ArmArmor, "Arms", {}, {}, {}, ingnomia::ui::CatalogId { "none" }, ingnomia::ui::CatalogId { "any" }, armorChoices },
		{ ingnomia::ui::UniformSlot::HandArmor, "Hands", {}, {}, {}, ingnomia::ui::CatalogId { "none" }, ingnomia::ui::CatalogId { "any" }, armorChoices },
		{ ingnomia::ui::UniformSlot::LegArmor, "Legs", {}, {}, {}, ingnomia::ui::CatalogId { "none" }, ingnomia::ui::CatalogId { "any" }, armorChoices },
		{ ingnomia::ui::UniformSlot::FootArmor, "Feet", {}, {}, {}, ingnomia::ui::CatalogId { "none" }, ingnomia::ui::CatalogId { "any" }, armorChoices },
		{ ingnomia::ui::UniformSlot::LeftHandHeld, "Left hand", {}, {}, {}, ingnomia::ui::CatalogId { "none" }, ingnomia::ui::CatalogId { "any" }, heldChoices },
		{ ingnomia::ui::UniformSlot::RightHandHeld, "Right hand", "SwordBlade", "Iron", "../tilesheet/inventory_SwordBlade.tga", ingnomia::ui::CatalogId { "Sword" }, ingnomia::ui::CatalogId { "Iron" }, heldChoices },
		{ ingnomia::ui::UniformSlot::Back, "Back", "Backpack", "Leather", "../tilesheet/inventory_Backpack.tga", ingnomia::ui::CatalogId { "Backpack" }, ingnomia::ui::CatalogId { "Leather" }, heldChoices },
	};
	fixture.inventory = { { "Copper pickaxe", {}, 0 }, { "Apple", {}, 0 } };
	fixture.inventoryReported = true;
	const auto id = fixture.id;
	const bool shown = openDetachedCreatureInspector( fixture, ingnomia::ui::WorldPosition { 50, 50, 92 } );
	if ( shown )
		for ( auto& window : m_creatureInspectorWindows )
			if ( window.controller && window.controller->state().creature && window.controller->state().creature->id == id )
				window.controller->setProfessionChoices( { "Farmer", "Gnomad", "Mason", "Miner", "Woodcutter" } );
	return shown;
}

bool MainWindow::showInspectorBlueprintFixture()
{
	using namespace ingnomia::ui;
	using namespace ingnomia::ui::inspector;
	TileInspectorState fixture;
	fixture.id = TileId { 0xB10Eu };
	fixture.position = WorldPosition { 50, 50, 92 };
	fixture.jobName = "BuildItem";
	fixture.jobWorker.clear();
	fixture.jobPriority = "2";
	fixture.requiredSkill = "Construction";
	fixture.requiredItems = {
		{ "RawWood", "any", 1, false },
		{ "RawStone", "Granite", 1, false },
	};
	fixture.hasJob = true;
	fixture.canRaisePriority = true;
	fixture.canLowerPriority = true;
	return openDetachedBlueprintInspector( fixture );
}

void MainWindow::setTileInspection( bool active )
{
	if ( active && (!m_hudController || !m_hudController->state().acceptsWorldActions) ) return;
	if ( active && !openDetachedLiveTileInspector() ) return;
	m_tileInspectionActive = active;
	if ( m_hudBinding ) m_hudBinding->setInspectionActive( active );
	if ( m_inspectorBinding ) m_inspectorBinding->setLiveInspection( false );
	if ( m_inspectorController && m_inspectorController->state().kind != ingnomia::ui::inspector::InspectorKind::None )
		m_inspectorController->close();
	if ( !active ) closeDetachedCreatureInspector( -2 );
	if ( Global::eventConnector )
	{
		auto* selection = Global::eventConnector->aggregatorSelection();
		QMetaObject::invokeMethod( selection, [selection,active]{ selection->onSetInspection(active); }, Qt::QueuedConnection );
	}
	redraw();
}

bool MainWindow::showInspectorStockpileFixture()
{
	using namespace ingnomia::ui;
	using namespace ingnomia::ui::inspector;
	if ( !m_inspectorController || !m_inspectorBinding ) return false;
	m_inspectorController->beginWorld( WorldEpoch { 0xC0DEu } );
	StockpileInspectorState fixture;
	fixture.id = StockpileId { 0x570Cu };
	fixture.name = "Stockpile";
	fixture.priority = 0;
	fixture.maxPriority = 2;
	fixture.capacity = 48;
	fixture.itemCount = 48;
	fixture.reserved = 0;
	fixture.pullFromOthers = false;
	fixture.allowPullFromHere = false;
	fixture.contents = {
		{ "Raw wood", "Applewood", 26, false, "../tilesheet/inventory_RawWood.tga" },
		{ "Raw wood", "Oak", 6, false, "../tilesheet/inventory_RawWood.tga" },
		{ "Raw wood", "Pine", 16, false, "../tilesheet/inventory_RawWood.tga" },
	};
	m_inspectorController->showStockpile( std::move( fixture ) );
	return m_inspectorController->state().kind == InspectorKind::Stockpile;
}

bool MainWindow::showManagementStockpileFixture()
{
	using namespace ingnomia::ui;
	using namespace ingnomia::ui::management6a;
	if ( !m_management6a || !m_management6a->controller(ManagementView::Stockpile) ) return false;
	m_management6a->beginWorld( WorldEpoch { 0xC0DEu } );
	StockpileSnapshot fixture;
	fixture.id = StockpileId { 0x570Cu };
	fixture.name = "Raw materials";
	fixture.priority = 1;
	fixture.maxPriority = 5;
	fixture.capacity = 64;
	fixture.itemCount = 48;
	const StockpileFilterRowId materials { fixture.id, CatalogId { "materials" }, {}, {}, {}, FilterDepth::Category };
	const StockpileFilterRowId raw { fixture.id, CatalogId { "materials" }, CatalogId { "raw" }, {}, {}, FilterDepth::Group };
	const StockpileFilterRowId rawWood { fixture.id, CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawWood" }, {}, FilterDepth::Item };
	const StockpileFilterRowId appleWood { fixture.id, CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawWood" }, CatalogId { "AppleWood" }, FilterDepth::Material };
	const StockpileFilterRowId rawStone { fixture.id, CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawStone" }, {}, FilterDepth::Item };
	const StockpileFilterRowId granite { fixture.id, CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawStone" }, CatalogId { "Granite" }, FilterDepth::Material };
	const StockpileFilterRowId food { fixture.id, CatalogId { "food" }, {}, {}, {}, FilterDepth::Category };
	const StockpileFilterRowId grown { fixture.id, CatalogId { "food" }, CatalogId { "grown" }, {}, {}, FilterDepth::Group };
	const StockpileFilterRowId apple { fixture.id, CatalogId { "food" }, CatalogId { "grown" }, CatalogId { "Apple" }, {}, FilterDepth::Item };
	const StockpileFilterRowId appleMaterial { fixture.id, CatalogId { "food" }, CatalogId { "grown" }, CatalogId { "Apple" }, CatalogId { "Apple" }, FilterDepth::Material };
	fixture.filters = {
		{ materials, "Materials", TriState::On, { "", 0, 0 } },
		{ raw, "Raw materials", TriState::On, { "", 0, 0 } },
		{ rawWood, "Raw wood", TriState::On, { "filter_RawWood.tga", 24, 24 } },
		{ appleWood, "Applewood", TriState::On, { "filter_RawWood.tga", 24, 24 } },
		{ rawStone, "Raw stone", TriState::On, { "filter_RawStone.tga", 24, 24 } },
		{ granite, "Granite", TriState::On, { "filter_RawStone.tga", 24, 24 } },
		{ food, "Food", TriState::On, { "", 0, 0 } },
		{ grown, "Grown", TriState::On, { "", 0, 0 } },
		{ apple, "Apple", TriState::On, { "filter_Apple.tga", 24, 24 } },
		{ appleMaterial, "Apple", TriState::On, { "filter_Apple.tga", 24, 24 } },
	};
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_STOCKPILE_SEARCH_FIXTURE" ) == "1" )
	{
		for ( int categoryIndex = 0; categoryIndex < 8; ++categoryIndex )
		{
			const CatalogId category { "perf_category_" + std::to_string( categoryIndex ) };
			fixture.filters.push_back( { { fixture.id, category, {}, {}, {}, FilterDepth::Category }, "Performance category " + std::to_string( categoryIndex ), TriState::Mixed } );
			for ( int groupIndex = 0; groupIndex < 8; ++groupIndex )
			{
				const CatalogId group { "perf_group_" + std::to_string( groupIndex ) };
				fixture.filters.push_back( { { fixture.id, category, group, {}, {}, FilterDepth::Group }, "Performance group " + std::to_string( groupIndex ), TriState::Mixed } );
				for ( int itemIndex = 0; itemIndex < 10; ++itemIndex )
				{
					const CatalogId item { "perf_item_" + std::to_string( itemIndex ) };
					fixture.filters.push_back( { { fixture.id, category, group, item, {}, FilterDepth::Item }, "Performance item " + std::to_string( itemIndex ), TriState::Mixed } );
					for ( int materialIndex = 0; materialIndex < 2; ++materialIndex )
					{
						const CatalogId material { "perf_material_" + std::to_string( materialIndex ) };
						fixture.filters.push_back( { { fixture.id, category, group, item, material, FilterDepth::Material }, "Performance material " + std::to_string( materialIndex ), TriState::Off } );
					}
				}
			}
		}
	}
	fixture.contents = {
		{ { CatalogId { "food" }, {}, {}, {}, FilterDepth::Category }, "Food", 6, 18, {} },
		{ { CatalogId { "food" }, CatalogId { "grown" }, {}, {}, FilterDepth::Group }, "Grown", 6, 18, {} },
		{ { CatalogId { "food" }, CatalogId { "grown" }, CatalogId { "Apple" }, {}, FilterDepth::Item }, "Apple", 6, 18, { "filter_Apple.tga", 24, 24 } },
		{ { CatalogId { "food" }, CatalogId { "grown" }, CatalogId { "Apple" }, CatalogId { "Apple" }, FilterDepth::Material }, "Apple", 6, 18, { "filter_Apple.tga", 24, 24 } },
		{ { CatalogId { "materials" }, {}, {}, {}, FilterDepth::Category }, "Materials", 42, 144, {} },
		{ { CatalogId { "materials" }, CatalogId { "raw" }, {}, {}, FilterDepth::Group }, "Raw materials", 42, 144, {} },
		{ { CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawStone" }, {}, FilterDepth::Item }, "Raw stone", 16, 54, { "filter_RawStone.tga", 24, 24 } },
		{ { CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawStone" }, CatalogId { "Granite" }, FilterDepth::Material }, "Granite", 16, 54, { "filter_RawStone.tga", 24, 24 } },
		{ { CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawWood" }, {}, FilterDepth::Item }, "Raw wood", 26, 90, { "filter_RawWood.tga", 24, 24 } },
		{ { CatalogId { "materials" }, CatalogId { "raw" }, CatalogId { "RawWood" }, CatalogId { "AppleWood" }, FilterDepth::Material }, "Applewood", 26, 90, { "filter_RawWood.tga", 24, 24 } },
	};
	fixture.templateNames = { "Food only", "Raw materials" };
	m_management6a->controller(ManagementView::Stockpile)->showStockpile( std::move( fixture ), Revision { 1 }, WorldPosition { 50, 50, 92 } );
	if ( !ensureDetachedManagement6A(static_cast<int>(ManagementView::Stockpile)) ) return false;
    if(!qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09")) QTimer::singleShot(200,this,[this]{
        auto* binding=m_stockpileWindow->binding.get();auto* doc=binding->stockpileDocument();auto* context=m_stockpileWindow->context->context();
        const auto mode=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09");
        if(mode=="settings") {activateManagementElement("stockpile_view_settings");setManagementFormValueForProbe("stockpile_name","Unsaved supplies");}
        else if(mode=="allow" || mode=="bulk" || mode=="template") {
            activateManagementElement("stockpile_view_allow");
            if(mode=="bulk")activateManagementElement("stockpile_block_bulk");
            if(mode=="template") {setManagementFormValueForProbe("stockpile_template_name","Food only");activateManagementElement("stockpile_template_save");}
        }
        else if(mode=="popup") {activateManagementElement("stockpile_view_allow");activateManagementElement("stockpile_template_toggle");}
        context->Update();doc->UpdateDocument();
        QFile out(qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_RESULT"));
        if(out.open(QIODevice::WriteOnly|QIODevice::Text))out.write(QString("mode=%1 density=%2 tabs=%3 stock-height=%4 rules-height=%5\n").arg(mode).arg(context->GetDensityIndependentPixelRatio()).arg(doc->GetElementById("stockpile_tabs")->IsVisible(true)).arg(doc->GetElementById("stockpile_rows")->GetClientHeight()).arg(doc->GetElementById("stockpile_filters")->GetClientHeight()).toUtf8());
        requestManagementCaptureForProbe("stockpile",qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_CAPTURE"));
    });
	return m_management6a->controller(ManagementView::Stockpile)->state().view == ManagementView::Stockpile;
}

std::string MainWindow::stockpileStage09Probe(std::string_view action)
{
    using namespace ingnomia::ui;
    using namespace ingnomia::ui::management6a;
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_LIVE")!="1" || !m_management6a || !m_inspectorController)return "FAIL probe unavailable";
    auto* controller=m_management6a->controller(ManagementView::Stockpile);
    const auto& s=controller->state().stockpile;
    static std::vector<StockpileFilterRow> before;
    static std::vector<StockpileFilterRowId> scope;
    if(action=="scope") {before=s.value.filters;scope=s.matchingFilterLeaves;return "PASS reviewed "+std::to_string(scope.size())+" stable rule IDs";}
    if(action=="blocked" || action=="allowed") {
        bool ok=!scope.empty();int changed=0;
        for(const auto& old:before) {
            if(old.id.depth!=FilterDepth::Material)continue;
            auto row=std::ranges::find_if(s.value.filters,[&](const auto& r){return r.id==old.id;});
            const bool targeted=std::ranges::find(scope,old.id)!=scope.end();
            const auto expected=targeted?(action=="allowed"?TriState::On:TriState::Off):old.state;
            ok=ok&&row!=s.value.filters.end()&&row->state==expected;
            if(row!=s.value.filters.end()&&row->state!=old.state)++changed;
        }
        return std::string(ok?"PASS ":"FAIL ")+"authoritative "+std::string(action)+" scope="+std::to_string(scope.size())+" changed="+std::to_string(changed);
    }
    if(action=="inspector") {
        const auto& v=s.value;
        m_inspectorController->showStockpile({v.id,v.name,v.priority,v.maxPriority,v.capacity,v.itemCount,v.reserved,v.suspended,v.pullFromOthers,v.allowPullFromHere,{}});
        return "PASS inspector opened on authoritative manager snapshot";
    }
    if(action=="toggle-inspector") {m_inspectorController->toggleStockpileSuspended();return "PASS inspector suspension dispatched";}
    const auto& i=m_inspectorController->state().stockpile;
    const auto& v=s.value;
    const bool equal=i&&i->id==v.id&&i->name==v.name&&i->priority==v.priority&&i->suspended==v.suspended&&i->itemCount==v.itemCount&&i->reserved==v.reserved;
    return std::string(equal?"PASS ":"FAIL ")+"inspector manager snapshot agreement suspended="+std::to_string(v.suspended)+" name="+v.name;
}

std::string MainWindow::inventoryStage21Probe(std::string_view action)
{
    using namespace ingnomia::ui::management6b;
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_STAGE21_INVENTORY")!="1" || !m_management6bController || !m_inventoryWindow || !m_inventoryWindow->binding)return "FAIL probe unavailable";
    auto& c=*m_management6bController;
    auto* doc=m_inventoryWindow->binding->inventoryDocument();
    if(!doc)return "FAIL no Inventory document";
    const auto& s=c.state();
    if(action.starts_with("find:")) {
        auto* f=rmlui_dynamic_cast<Rml::ElementFormControl*>(doc->GetElementById("inventory_search"));
        if(!f)return "FAIL no Find box";
        f->SetValue(std::string(action.substr(5)));f->DispatchEvent("input",Rml::Dictionary{});
        return std::string(s.inventoryFilter==action.substr(5)?"PASS ":"FAIL ")+"Find="+s.inventoryFilter+" shown="+std::to_string(c.inventoryPage().size());
    }
    if(action=="select-first") {
        const auto rows=c.inventoryPage();
        if(rows.empty())return "FAIL no rows";
        c.selectInventory(rows.front().id);return "PASS selected "+rows.front().name;
    }
    if(action=="open") {
        if(!s.selectedInventory)return "FAIL nothing selected";
        auto* button=doc->GetElementById("inventory_open_item");if(!button)return "FAIL no Properties";
        button->Click();return std::string(s.inventoryDetail?"PASS ":"FAIL ")+"Properties opened Item Properties";
    }
    if(action.starts_with("tab:")) {
        const char* tabs[]={"inventory_detail_tab_general","inventory_detail_tab_stockpiles","inventory_detail_tab_recipes","inventory_detail_tab_history"};
        const int n=action[4]-'0';if(n<0||n>3)return "FAIL bad tab";
        doc->GetElementById(tabs[n])->Click();return std::string("PASS tab ")+tabs[n];
    }
    if(action.starts_with("click:")) {
        auto* e=doc->GetElementById(std::string(action.substr(6)));if(!e)return "FAIL missing "+std::string(action.substr(6));
        e->Click();return "PASS clicked "+std::string(action.substr(6));
    }
    if(action=="watch") {
        if(!s.inventoryDetail)return "FAIL no Item Properties";
        const auto row=std::ranges::find_if(s.inventory,[&](const auto& r){return r.id==*s.inventoryDetail;});
        const bool before=row!=s.inventory.end()&&row->watched;
        doc->GetElementById("inventory_detail_watch")->Click();
        const auto after=std::ranges::find_if(s.inventory,[&](const auto& r){return r.id==*s.inventoryDetail;});
        return std::string(after!=s.inventory.end()&&after->watched!=before?"PASS ":"FAIL ")+"Watch this item changed the watch state";
    }
    if(action=="close-detail") {
        doc->GetElementById("inventory_detail_close_button")->Click();
        return std::string(!s.inventoryDetail?"PASS ":"FAIL ")+"Close returned to the Inventory list";
    }
    const auto detail=s.inventoryDetail?std::ranges::find_if(s.inventory,[&](const auto& r){return r.id==*s.inventoryDetail;}):s.inventory.end();
    return "STATE open="+std::to_string(s.inventoryOpen)+" rows="+std::to_string(c.inventoryPage().size())+" find="+s.inventoryFilter+" detail="+(detail!=s.inventory.end()?detail->name:std::string("-"))
        +" watched="+std::to_string(detail!=s.inventory.end()&&detail->watched)+" locations="+std::to_string(detail!=s.inventory.end()?detail->locations.size():0);
}

std::string MainWindow::agricultureStage11Probe(std::string_view action)
{
    using namespace ingnomia::ui::management6a;
    if((qEnvironmentVariable("INGNOMIA_AUTOMATE_AGRICULTURE_STAGE11_LIVE")!="1" && qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_PLAN_PROBE")!="1") || !m_management6a || !m_agricultureWindow || !m_agricultureWindow->binding)return "FAIL probe unavailable";
    auto* controller=m_management6a->controller(ManagementView::Agriculture);
    const auto& s=controller->state().agriculture;
    // "ctrl-click:<id>" / "shift-click:<id>" dispatch a modified click on a live element, like a user holding the key.
    for(const auto& [prefix,key]:{std::pair{std::string_view("ctrl-click:"),"ctrl_key"},std::pair{std::string_view("shift-click:"),"shift_key"}})
        if(action.starts_with(prefix)) {
            auto* doc=m_agricultureWindow->binding->agricultureDocument();
            auto* e=doc?doc->GetElementById(std::string(action.substr(prefix.size()))):nullptr;
            if(!e)return "FAIL missing "+std::string(action.substr(prefix.size()));
            Rml::Dictionary p;p[key]=1;e->DispatchEvent("click",p);
            return "PASS "+std::string(action);
        }
    if(action=="select-first-order") {
        if(s.selectedPlots.size()!=1)return "FAIL one plot is not selected";
        auto f=std::ranges::find_if(s.value.fields,[&](const auto& r){return r.position==s.selectedPlots.front();});
        if(f==s.value.fields.end()||f->orders.empty())return "FAIL selected plot has no queued planting";
        return m_agricultureWindow->binding->activateElement("agriculture_order_"+std::to_string(f->orders.front().id))?"PASS selected planting "+std::to_string(f->orders.front().id):"FAIL planting row missing";
    }
    std::string sel;for(const auto& p:s.selectedPlots)sel+=std::to_string(p.x)+","+std::to_string(p.y)+";";
    std::string plots;for(const auto& f:s.value.fields)plots+=std::to_string(f.position.x)+","+std::to_string(f.position.y)+","+std::to_string(f.position.z)+";";
    const auto& v=s.value;
    return "STATE kind="+std::to_string(int(v.target.kind))+" name="+v.name+" pane="+std::to_string(int(s.pane))+" selected="+sel+" plots="+plots+" product="+v.product.value
        +" harvest="+std::to_string(v.harvest)+" hay="+std::to_string(v.harvestHay)+" tame="+std::to_string(v.tame)+" pick="+std::to_string(v.pick)+" plant="+std::to_string(v.plant)+" fell="+std::to_string(v.fell)
        +" suspended="+std::to_string(v.suspended)+" maxMale="+std::to_string(v.maxMale)+" maxFemale="+std::to_string(v.maxFemale)+" foods="+std::to_string(v.foods.size())+" animals="+std::to_string(v.animals.size())
        +" dirty="+std::to_string(s.draft.dirty)+" pending="+std::to_string(s.draft.pending)+" feedback="+s.feedback;
}

std::string MainWindow::workshopStage10Probe(std::string_view action)
{
    using namespace ingnomia::ui::management6a;
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGE10_LIVE")!="1" || !m_management6a)return "FAIL probe unavailable";
    auto* controller=m_management6a->controller(ManagementView::Workshop);
    const auto& s=controller->state().workshop;
    if(action=="select-first-job") {
        if(s.value.queue.empty())return "FAIL no queued job to select";
        controller->selectWorkshopJob(s.value.queue.front().id);return "PASS selected job "+std::to_string(s.value.queue.front().id.value);
    }
    if(action=="select-first-trade-row") {
        if(s.traderRows.empty())return "FAIL no merchant row to select";
        controller->selectTradeRow(s.traderRows.front().id);return "PASS selected merchant row "+s.traderRows.front().name;
    }
    if(action=="select-first-player-row") {
        if(s.playerRows.empty())return "FAIL no settlement row to select";
        controller->selectTradeRow(s.playerRows.front().id);return "PASS selected settlement row "+s.playerRows.front().name+" stock="+std::to_string(s.playerRows.front().stock);
    }
    if(action=="offers") {
        std::string out="OFFERS receive="+std::to_string(s.traderOfferValue)+" give="+std::to_string(s.playerOfferValue)+" rows:";
        for(const auto& r:s.traderRows)out+=" trader:"+r.name+"="+std::to_string(r.offered)+"/"+std::to_string(r.stock);
        for(const auto& r:s.playerRows)out+=" player:"+r.name+"="+std::to_string(r.offered)+"/"+std::to_string(r.stock);
        return out;
    }
    const auto tab=[&](const char* id){auto* doc=m_management6aWindow&&m_management6aWindow->binding?m_management6aWindow->binding->workshopDocument():nullptr;auto* e=doc?doc->GetElementById(id):nullptr;return e&&e->IsVisible(true);};
    const char* pane=s.pane==WorkshopPane::Craft?"craft":s.pane==WorkshopPane::Queue?"queue":s.pane==WorkshopPane::Settings?"settings":"trade";
    return "STATE subtype="+s.value.subtype+" pane="+pane+" craftTab="+std::to_string(tab("workshop_view_craft"))+" queueTab="+std::to_string(tab("workshop_view_queue"))
        +" tradeTab="+std::to_string(tab("workshop_view_trade"))+" butcher="+std::to_string(tab("workshop_butcher_actions"))+" fisher="+std::to_string(tab("workshop_fisher_actions"))
        +" corpses="+std::to_string(s.value.butcherCorpses)+" excess="+std::to_string(s.value.butcherExcess)+" catch="+std::to_string(s.value.catchFish)+" process="+std::to_string(s.value.processFish)
        +" queue="+std::to_string(s.value.queue.size())+" traderRows="+std::to_string(s.traderRows.size())+" playerRows="+std::to_string(s.playerRows.size())
        +" review="+std::to_string(s.tradeConfirmationRequired);
}

bool MainWindow::showManagementFarmFixture()
{
	using namespace ingnomia::ui;
	using namespace ingnomia::ui::management6a;
	if ( !m_management6a || !m_management6a->controller( ManagementView::Agriculture ) ) return false;
	m_management6a->beginWorld( WorldEpoch { 0xC0DEu } );
	AgricultureSnapshot fixture;
	fixture.target = { AgricultureKind::Farm, DesignationId { 0xFA12u } };
	fixture.name = "South farm";
	fixture.product = CatalogId { "Wheat" };
	fixture.productName = "Wheat";
	fixture.priority = 2;
	fixture.maxPriority = 5;
	fixture.plots = 20;
	fixture.tilled = 18;
	fixture.planted = 12;
	fixture.ready = 4;
	fixture.harvest = true;
	fixture.catalog = { { CatalogId { "Wheat" }, "Wheat", 34, 12, 16 }, { CatalogId { "Barley" }, "Barley", 21, 8, 5 }, { CatalogId { "Cabbage" }, "Cabbage", 9, 3, 7 } };
	m_management6a->controller( ManagementView::Agriculture )->showAgriculture( std::move( fixture ), Revision { 1 }, WorldPosition { 50, 50, 92 } );
	if ( !ensureDetachedManagement6A( static_cast<int>( ManagementView::Agriculture ) ) ) return false;
	return m_management6a->controller( ManagementView::Agriculture )->state().view == ManagementView::Agriculture;
}

bool MainWindow::verifyInspectorStockpileItemGeometry( std::string* detail )
{
	return m_inspectorBinding && m_inspectorBinding->stockpileItemIconInline( detail );
}

bool MainWindow::showInventoryFixture()
{
	using namespace ingnomia::ui;
	using namespace ingnomia::ui::management6b;
	if ( !m_management6bController ) return false;
	const WorldEpoch world { 0xC0DEu };
	m_management6bController->beginWorld( world );
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_DIALOGS_PROBE")=="1") {
        if(!ensureDetachedManagement6B(false) || !m_management6bWindow) return false;
        auto* binding=m_management6bWindow->binding.get(); binding->openPopulation(FocusToken{2});
        m_management6bController->applyProfessions({world,Revision{1},{{ProfessionId{"Miner"},"Miner",{CatalogId{"Mining"}}}}});
        m_management6bController->applyPopulation({world,Revision{1},{}});
        m_management6bController->open(View::Professions);m_management6bController->selectProfession(ProfessionId{"Miner"});
        QTimer::singleShot(100,this,[this]{
            auto* binding=m_management6bWindow->binding.get();auto* doc=binding->populationDocument();auto*c=doc->GetContext();c->Update();
            const auto key=[this](int k){QKeyEvent down(QEvent::KeyPress,k,Qt::NoModifier);QCoreApplication::sendEvent(m_management6bWindow->nativeWindow.get(),&down);QKeyEvent up(QEvent::KeyRelease,k,Qt::NoModifier);QCoreApplication::sendEvent(m_management6bWindow->nativeWindow.get(),&up);};
            if(qEnvironmentVariable("INGNOMIA_AUTOMATE_POPULATION_HOVER")=="1") {
                int hovered=0, tabs=0;
                for(int round=0;round<25;++round) {
                    for(const char* id : {"population_tab_citizens","population_tab_skills","population_tab_schedules","population_tab_professions"}) {
                        auto* tab=doc->GetElementById(id);if(!tab->IsVisible(true)){doc->GetElementById("population_views_toggle")->Click();c->Update();}tab->ScrollIntoView();c->Update();auto p=tab->GetAbsoluteOffset();
                        c->ProcessMouseMove(int(p.x+tab->GetOffsetWidth()/2),int(p.y+tab->GetOffsetHeight()/2),0);
                        c->ProcessMouseButtonDown(0,0);c->ProcessMouseButtonUp(0,0);c->Update();++tabs;
                    }
                    c->ProcessMouseLeave();
    auto* target=doc->GetElementById("population_tab_professions");if(!target->IsVisible(true)){doc->GetElementById("population_views_toggle")->Click();c->Update();}target->ScrollIntoView();c->Update();auto p=target->GetAbsoluteOffset();
                    c->ProcessMouseMove(int(p.x+target->GetOffsetWidth()/2),int(p.y+target->GetOffsetHeight()/2),0);c->Update();
                    if(doc->GetElementById("population_tooltip")->IsVisible(true))++hovered;
                    c->ProcessMouseLeave();c->Update();
                }
                QFile hoverResult(qEnvironmentVariable("INGNOMIA_AUTOMATE_POPULATION_HOVER_RESULT"));
                if(hoverResult.open(QIODevice::WriteOnly|QIODevice::Text))hoverResult.write(QString("tabs=%1 hover_visible=%2 rounds=25 density=%3\n").arg(tabs).arg(hovered).arg(c->GetDensityIndependentPixelRatio()).toUtf8());
            }
            auto* opener=doc->GetElementById("profession_delete");opener->Focus();key(Qt::Key_Return);c->Update();
            const bool opened=c->GetFocusElement()&&c->GetFocusElement()->GetId()=="confirm-cancel";
            key(Qt::Key_Escape);c->Update();const bool restored=c->GetFocusElement()==opener;
            m_management6bController->setProfessionDraftName("Prospector");
            m_management6bWindow->nativeWindow->requestClose();c->Update();
            const bool guarded=m_management6bWindow->nativeWindow->isVisible()&&m_management6bController->state().populationOpen&&c->GetFocusElement()->GetId()=="confirm-cancel";
            key(Qt::Key_Escape);c->Update();const bool kept=m_management6bController->state().professionDraftDirty;
            if(qEnvironmentVariable("INGNOMIA_AUTOMATE_DIALOGS_DRAFT")=="1")binding->closePopulation();
            else {m_management6bController->discardProfessionDraft();m_management6bController->open(View::Professions);opener->Focus();key(Qt::Key_Return);}
            QFile result(qEnvironmentVariable("INGNOMIA_AUTOMATE_DIALOGS_RESULT"));if(result.open(QIODevice::WriteOnly|QIODevice::Text))result.write(QString("dialogs open=%1 restored=%2 native_close_guard=%3 kept=%4\n").arg(opened).arg(restored).arg(guarded).arg(kept).toUtf8());
            requestManagementCaptureForProbe("population",qEnvironmentVariable("INGNOMIA_AUTOMATE_UI_FIXTURE_DETACHED_CAPTURE_PATH"));
        });
        return true;
    }
    if ( !ensureDetachedManagement6B( true ) || !m_inventoryWindow || !m_inventoryWindow->binding ) return false;
	if ( !m_inventoryWindow->binding->openInventory( FocusToken { 2 } ) ) return false;
	std::vector<InventoryRow> rows;
	const auto addCategory = [&]( std::string id, std::string name ) {
		InventoryRow category;
		category.id = { CatalogId { std::move( id ) }, {}, {}, {}, InventoryDepth::Category };
		category.name = std::move( name );
		rows.push_back( std::move( category ) );
	};
	addCategory( "materials", "Materials" );
	addCategory( "grown", "Grown" );
	addCategory( "workshop", "Workshop" );
	addCategory( "food", "Food" );
	addCategory( "drinks", "Drinks" );
	addCategory( "containers", "Containers" );
	addCategory( "furniture", "Furniture" );
	addCategory( "cloth", "Cloth" );
	addCategory( "armor", "Armor" );
	const auto addGroup = [&]( std::string id, std::string name, std::uint32_t total ) {
		InventoryRow group;
		group.id = { CatalogId { "materials" }, CatalogId { std::move( id ) }, {}, {}, InventoryDepth::Group };
		group.name = std::move( name );
		group.total = total;
		rows.push_back( std::move( group ) );
	};
	addGroup( "drinks", "Drinks", 200 );
	addGroup( "food", "Food", 100 );
	addGroup( "grown", "Grown", 90 );
	addGroup( "workshop", "Workshop", 17 );
	addGroup( "weapons", "Weapons", 0 );
	const InventoryRowId vegetableGroupId { CatalogId { "food" }, CatalogId { "vegetables" }, {}, {}, InventoryDepth::Group };
	InventoryRow vegetableGroup;
	vegetableGroup.id = vegetableGroupId;
	vegetableGroup.name = "Vegetables";
	vegetableGroup.total = 24;
	rows.push_back( vegetableGroup );
	InventoryRow vegetableItem;
	vegetableItem.id = { CatalogId { "food" }, CatalogId { "vegetables" }, CatalogId { "Vegetable" }, {}, InventoryDepth::Item };
	vegetableItem.name = "Vegetable";
	vegetableItem.total = 24;
	vegetableItem.spriteSheet = "build_Carrot.tga";
	vegetableItem.spriteWidth = vegetableItem.spriteSheetWidth = 32;
	vegetableItem.spriteHeight = vegetableItem.spriteSheetHeight = 36;
	rows.push_back( vegetableItem );
	const std::array<std::pair<const char*, const char*>, 24> vegetables {{
		{ "Artichoke", "Artichoke" }, { "Asparagus", "Asparagus" }, { "Beans", "Beans" }, { "BeetRoot", "Beet root" },
		{ "Bottlegourd", "Bottle gourd" }, { "Broccoli", "Broccoli" }, { "Cabbage", "Cabbage" }, { "Capsicum", "Capsicum" },
		{ "Carrot", "Carrot" }, { "Cauliflower", "Cauliflower" }, { "Corn", "Corn" }, { "Cucumber", "Cucumber" },
		{ "Garlic", "Garlic" }, { "Leek", "Leek" }, { "Lettuce", "Lettuce" }, { "Onion", "Onion" },
		{ "Parsnip", "Parsnip" }, { "Peas", "Peas" }, { "Potato", "Potato" }, { "Radish", "Radish" },
		{ "Sugarbeet", "Sugar beet" }, { "Tomato", "Tomato" }, { "Turnip", "Turnip" }, { "Woad", "Woad" }
	} };
	for ( const auto& [id, name] : vegetables )
	{
		InventoryRow vegetable;
		vegetable.id = { CatalogId { "food" }, CatalogId { "vegetables" }, CatalogId { "Vegetable" }, CatalogId { id }, InventoryDepth::Material };
		vegetable.name = name;
		vegetable.total = 1;
		vegetable.spriteSheet = std::string { "inventory_" } + id + ".tga";
		vegetable.spriteWidth = vegetable.spriteSheetWidth = 40;
		vegetable.spriteHeight = vegetable.spriteSheetHeight = 40;
		rows.push_back( std::move( vegetable ) );
	}
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_REPORTS_PROBE")=="1") {
        for(int i=0;i<10000;++i){InventoryRow row;row.id={CatalogId{"food"},CatalogId{"vegetables"},CatalogId{"ReportItem"+std::to_string(i)},CatalogId{"Oak"},InventoryDepth::Material};row.name="Report item "+std::to_string(i);row.stockpiled=i%7;row.total=i%19;row.locations.push_back({7,"Supply stockpile",int(row.stockpiled)});row.madeBy.push_back({"Recipe","Vegetable","Vegetable","Kitchen","Cooking",1,{{"Vegetable","Vegetable","","",2}}});row.usedIn=row.madeBy;rows.push_back(std::move(row));}
    }
    m_management6bController->applyInventory( { world, Revision { 1 }, std::move( rows ) } );
	m_management6bController->setInventoryCategory( "food" );
	m_management6bController->toggleInventoryExpanded( vegetableGroupId );
	m_management6bController->toggleInventoryExpanded( vegetableItem.id );
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_REPORTS_PROBE")=="1") QTimer::singleShot(100,this,[this]{
        auto*doc=m_inventoryWindow->binding->inventoryDocument();auto*c=doc->GetContext();c->Update();
        auto*rows=doc->GetElementById("inventory_rows");rows->Focus();
        const auto key=[this](int k){QKeyEvent down(QEvent::KeyPress,k,Qt::NoModifier);QCoreApplication::sendEvent(m_inventoryWindow->nativeWindow.get(),&down);QKeyEvent up(QEvent::KeyRelease,k,Qt::NoModifier);QCoreApplication::sendEvent(m_inventoryWindow->nativeWindow.get(),&up);};
        key(Qt::Key_End);c->Update();c->Update();const bool end=m_management6bController->state().selectedInventory.has_value()&&c->GetFocusElement()->HasAttribute("data-item");
        const auto identity=m_management6bController->state().selectedInventory;
        doc->GetElementById("inventory_sort_stock")->Click();c->Update();const bool stable=identity==m_management6bController->state().selectedInventory;
        auto*field=doc->GetElementById("inventory_filter_stock");field->Focus();key(Qt::Key_Down);c->Update();const bool opened=doc->GetElementById("inventory_filter_stock_options")->IsVisible(true);key(Qt::Key_Escape);c->Update();const bool canceled=c->GetFocusElement()==field&&!doc->GetElementById("inventory_filter_stock_options")->IsVisible(true);
        const bool bounded=rows->GetNumChildren()<65;
        QFile trace(qEnvironmentVariable("INGNOMIA_AUTOMATE_REPORTS_RESULT"));if(trace.open(QIODevice::WriteOnly|QIODevice::Text))trace.write(QString("reports end=%1 stable=%2 open=%3 cancel=%4 bounded=%5 children=%6 scroll=%7 density=%8\n").arg(end).arg(stable).arg(opened).arg(canceled).arg(bounded).arg(rows->GetNumChildren()).arg(rows->GetScrollTop()).arg(c->GetDensityIndependentPixelRatio()).toUtf8());
        if(qEnvironmentVariable("INGNOMIA_AUTOMATE_REPORTS_POPUP")=="1")key(Qt::Key_Down);
        else {rows->Focus();key(Qt::Key_Home);}
        if(qEnvironmentVariable("INGNOMIA_AUTOMATE_REPORTS_RIGHT")=="1")rows->GetParentNode()->SetScrollLeft(100000.f);
        if(qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_STAGE08")=="1") {
            m_management6bController->setInventoryColumnFilter(3,"Report");c->Update();rows->Focus();key(Qt::Key_End);c->Update();
            const auto selected=m_management6bController->state().selectedInventory;
            const float top=rows->GetScrollTop();const float left=rows->GetParentNode()->GetScrollLeft();
            key(Qt::Key_Return);c->Update();const bool detail=selected.has_value()&&m_management6bController->state().inventoryDetail==selected;
            doc->GetElementById("inventory_detail_back")->Click();c->Update();
            const bool returned=std::abs(rows->GetScrollTop()-top)<2.f&&std::abs(rows->GetParentNode()->GetScrollLeft()-left)<2.f;
            const bool focused=c->GetFocusElement()&&c->GetFocusElement()->HasAttribute("data-item");
            QFile result(qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_STAGE08_RESULT"));
            if(result.open(QIODevice::WriteOnly|QIODevice::Text))result.write(QString("detail=%1 return_scroll=%2 return_focus=%3 top=%4 count=%5 density=%6\n").arg(detail).arg(returned).arg(focused).arg(top).arg(m_management6bController->inventoryPage().size()).arg(c->GetDensityIndependentPixelRatio()).toUtf8());
            if(qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_DETAIL")=="1")key(Qt::Key_Return);
            if(qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_CHOICES")=="1"){auto* f=doc->GetElementById("inventory_filter_total");f->ScrollIntoView();f->Focus();key(Qt::Key_Down);}
        }
        requestManagementCaptureForProbe("inventory",qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_STAGE08")=="1"?qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_STAGE08_CAPTURE"):qEnvironmentVariable("INGNOMIA_AUTOMATE_UI_FIXTURE_DETACHED_CAPTURE_PATH"));
    });
    return true;
}
bool MainWindow::showComponentFixture()
{
	if ( !m_rmlUiHost || !m_context || !m_rmlUiHost->initialized() ) return false;
	const bool contextWasCurrent = QOpenGLContext::currentContext() == m_context;
	if ( !contextWasCurrent && !m_context->makeCurrent( this ) ) return false;
	auto* document = m_rmlUiHost->loadDocument( "fixtures/components.rml", true );
	if ( document ) ingnomia::ui::localization::applyRmlText( *document, ingnomia::ui::localization::UiText::english() );
	const bool loaded = document != nullptr;
	if ( !contextWasCurrent ) m_context->doneCurrent();
	return loaded;
}

bool MainWindow::verifyComponentFixture( std::string* detail )
{
	if ( detail ) detail->clear();
	if ( !m_rmlUiHost || !m_context || !m_rmlUiHost->initialized() )
	{
		if ( detail ) *detail = "RmlUi host is not initialized";
		return false;
	}

	const bool contextWasCurrent = QOpenGLContext::currentContext() == m_context;
	if ( !contextWasCurrent && !m_context->makeCurrent( this ) )
	{
		if ( detail ) *detail = "could not make the primary OpenGL context current";
		return false;
	}

	bool verified = false;
	std::string result;
	if ( auto* context = m_rmlUiHost->context() )
	{
		(void)m_rmlUiHost->update();
		auto* root = context->GetRootElement();
			auto* fixtureBody = root ? root->GetElementById( "fixture-body" ) : nullptr;
			auto* fixtureHeader = root ? root->GetElementById( "fixture-header" ) : nullptr;
			auto* fixtureGrid = root ? root->GetElementById( "fixture-grid" ) : nullptr;
			auto* selectElement = root ? root->GetElementById( "fixture-generated-select" ) : nullptr;
		auto* scrollRegion = root ? root->GetElementById( "fixture-generated-scroll" ) : nullptr;
		auto* select = selectElement && selectElement->GetTagName() == "select"
			? static_cast<Rml::ElementFormControlSelect*>( selectElement ) : nullptr;
		auto* scroll = scrollRegion ? scrollRegion->GetElementScroll() : nullptr;
		auto* scrollbar = scroll ? scroll->GetScrollbar( Rml::ElementScroll::VERTICAL ) : nullptr;

		bool selectArrow = false;
		bool selectValue = false;
		bool selectBox = false;
		if ( select )
		{
			for ( int index = 0; index < select->GetNumChildren( true ); ++index )
			{
				const auto* child = select->GetChild( index );
				if ( !child ) continue;
				const auto& tag = child->GetTagName();
				selectArrow |= tag == "selectarrow";
				selectValue |= tag == "selectvalue";
				selectBox |= tag == "selectbox";
			}
		}

		bool sliderTrack = false;
		bool sliderBar = false;
		bool decrementArrow = false;
		bool incrementArrow = false;
		if ( scrollbar )
		{
			for ( int index = 0; index < scrollbar->GetNumChildren( true ); ++index )
			{
				const auto* child = scrollbar->GetChild( index );
				if ( !child ) continue;
				const auto& tag = child->GetTagName();
				sliderTrack |= tag == "slidertrack";
				sliderBar |= tag == "sliderbar";
				decrementArrow |= tag == "sliderarrowdec";
				incrementArrow |= tag == "sliderarrowinc";
			}
		}

			const int optionCount = select ? select->GetNumOptions() : 0;
			const bool layoutContainers = fixtureHeader && fixtureGrid
				&& fixtureHeader->GetOffsetWidth() > 0 && fixtureGrid->GetOffsetWidth() > 0;
			verified = layoutContainers && select && optionCount == 3 && selectArrow && selectValue && selectBox
				&& scrollbar && scrollbar->GetTagName() == "scrollbarvertical"
				&& sliderTrack && sliderBar && decrementArrow && incrementArrow;
			const auto elementSize = []( Rml::Element* element )
			{
				if ( !element ) return std::string( "missing" );
				const auto size = element->GetBox().GetSize();
				return std::to_string( static_cast<int>( size.x ) ) + "x" + std::to_string( static_cast<int>( size.y ) );
			};
			result = "root=" + elementSize( root ) + " body=" + elementSize( fixtureBody ) + " header=" + elementSize( fixtureHeader ) + " grid=" + elementSize( fixtureGrid )
				+ " layout=" + ( layoutContainers ? "pass" : "fail" )
				+ " select_tag=" + ( selectElement ? selectElement->GetTagName() : std::string( "missing" ) )
			+ " options=" + std::to_string( optionCount )
			+ " generated_select_parts=" + ( selectArrow ? "selectarrow," : "" )
			+ ( selectValue ? "selectvalue," : "" ) + ( selectBox ? "selectbox" : "" )
			+ " scrollbar_tag=" + ( scrollbar ? scrollbar->GetTagName() : std::string( "missing" ) )
			+ " generated_scroll_parts=" + ( sliderTrack ? "slidertrack," : "" )
			+ ( sliderBar ? "sliderbar," : "" ) + ( decrementArrow ? "sliderarrowdec," : "" )
			+ ( incrementArrow ? "sliderarrowinc" : "" );

            // Probe the real renderer's layout at narrow and normal host sizes.
            // Nonzero containers alone previously accepted horizontally clipped UI.
            auto* emptyTitle = root->GetElementById( "fixture-empty-title" );
            const bool fallback = emptyTitle && emptyTitle->GetInnerRML() == "Untitled window";
            verified = verified && fallback;
            result += std::string( " empty_title=" ) + ( fallback ? "pass" : "fail" );
            auto* longTitle = root->GetElementById( "fixture-long-title" );
            auto* titleClose = root->GetElementById( "fixture-title-close" );
            bool titleRecovery = false;
            if ( longTitle && titleClose )
            {
                longTitle->ScrollIntoView();
                (void)m_rmlUiHost->update();
                const auto closeBefore = titleClose->GetAbsoluteOffset( Rml::BoxArea::Border );
                const auto titleBefore = longTitle->GetAbsoluteOffset( Rml::BoxArea::Border );
                context->ProcessMouseMove( static_cast<int>( titleBefore.x + 4 ), static_cast<int>( titleBefore.y + 4 ), 0 );
                (void)m_rmlUiHost->update();
                const auto expanded = longTitle->GetAbsoluteOffset( Rml::BoxArea::Border );
                const auto closeAfter = titleClose->GetAbsoluteOffset( Rml::BoxArea::Border );
                titleRecovery = std::abs( closeBefore.x - closeAfter.x ) < 1.0f
                    && std::abs( closeBefore.y - closeAfter.y ) < 1.0f
                    && expanded.y >= closeAfter.y + titleClose->GetOffsetHeight();
                context->ProcessMouseMove( 0, 0, 0 );
                (void)m_rmlUiHost->update();
            }
            if ( fixtureBody ) fixtureBody->SetScrollTop( 0 );
            (void)m_rmlUiHost->update();
            verified = verified && titleRecovery;
            result += std::string( " title_recovery=" ) + ( titleRecovery ? "pass" : "fail" );
            const auto savedDimensions = context->GetDimensions();
            const float savedScroll = fixtureBody ? fixtureBody->GetScrollTop() : 0;
            for ( const auto dimensions : { Rml::Vector2i( 640, 480 ), Rml::Vector2i( 1280, 900 ), savedDimensions } )
            {
                context->SetDimensions( dimensions );
                (void)m_rmlUiHost->update();
                bool bounded = fixtureBody && fixtureHeader && fixtureGrid;
                for ( auto* element : { fixtureHeader, fixtureGrid } )
                {
                    if ( !element ) { bounded = false; continue; }
                    const auto offset = element->GetAbsoluteOffset( Rml::BoxArea::Border );
                    bounded = bounded && element->GetOffsetWidth() > dimensions.x / 2
                        && offset.x >= 0 && offset.x + element->GetOffsetWidth() <= dimensions.x + 1;
                }
                bool scrolls = false;
                if ( fixtureBody )
                {
                    const float maximum = std::max( 0.0f, fixtureBody->GetScrollHeight() - fixtureBody->GetClientHeight() );
                    fixtureBody->SetScrollTop( maximum );
                    (void)m_rmlUiHost->update();
                    scrolls = std::abs( fixtureBody->GetScrollTop() - maximum ) < 2.0f;
                    fixtureBody->SetScrollTop( 0 );
                }
                result += " viewport=" + std::to_string( dimensions.x ) + "x" + std::to_string( dimensions.y )
                    + ":bounds=" + ( bounded ? "pass" : "fail" ) + ":scroll=" + ( scrolls ? "pass" : "fail" );
                verified = verified && bounded && scrolls;
            }
            context->SetDimensions( savedDimensions );
            if ( fixtureBody ) fixtureBody->SetScrollTop( savedScroll );
            if(fixtureBody && qEnvironmentVariable("INGNOMIA_AUTOMATE_HIGH_CONTRAST") == "1") fixtureBody->SetClass("is-high-contrast", true);
            const auto focusId=qEnvironmentVariable("INGNOMIA_AUTOMATE_FIXTURE_FOCUS");
            if(!focusId.isEmpty()) if(auto* target=root->GetElementById(focusId.toStdString())) {target->Focus(true);if(qEnvironmentVariable("INGNOMIA_AUTOMATE_FIXTURE_SCROLL")=="1")target->ScrollIntoView();}
            (void)m_rmlUiHost->update();
    }
    if ( !contextWasCurrent ) m_context->doneCurrent();
    if ( detail ) *detail = result.empty() ? "fixture elements were not found in the RmlUi context" : result;
    return verified;
}
std::string MainWindow::hudStatus() const
{
	return m_hudController ? m_hudController->state().status : std::string{};
}

bool MainWindow::activateShellElement( std::string_view id )
{
	return m_shellBinding && m_shellBinding->activateElement( id );
}

bool MainWindow::dispatchShellSettingChangeForProbe( std::string_view id, float value, bool checked )
{
	return m_shellBinding && m_shellBinding->dispatchSettingChangeForProbe( id, value, checked );
}

bool MainWindow::activateManagementElement( std::string_view id )
{
	auto* productionWindow = id.starts_with("stockpile_") ? m_stockpileWindow.get() : (id.starts_with("agriculture_") || id.starts_with("m6a_agri_")) ? m_agricultureWindow.get() : m_management6aWindow.get();
    if (productionWindow && productionWindow->binding && productionWindow->binding->activateElement(id)) return true;
	auto* window6B = id.starts_with("inventory_") ? m_inventoryWindow.get() : m_management6bWindow.get();
    if(window6B && window6B->binding && window6B->binding->activateElement(id)) return true;
	auto* window6C = id.starts_with("diplomacy_") ? m_diplomacyWindow.get() : m_management6cWindow.get();
    if(window6C && window6C->binding && window6C->binding->activateElement(id)) return true;
	if ( m_management6a && m_management6a->activateElement( id ) ) return true;
	if ( m_management6bBinding && m_management6bBinding->activateElement( id ) ) return true;
	if ( m_management6cBinding && m_management6cBinding->activateElement( id ) ) return true;
	return false;
}
// What's This? in the workshop property sheet through the native pointer (Stage 10 probe): "press:<id>" presses the
// primary button on an element of the sheet; the result reports the mode and the pop-up.
std::string MainWindow::workshopWhatsThisProbe( std::string_view action )
{
	auto* window = m_management6aWindow.get();
	if ( !window || !window->nativeWindow || !window->binding || !window->binding->workshopDocument() ) return "FAIL workshop sheet not open";
	auto* native = window->nativeWindow.get();
	auto& help   = native->whatsThis();
	if ( action.starts_with( "press:" ) )
	{
		auto* element = window->binding->workshopDocument()->GetElementById( std::string( action.substr( 6 ) ) );
		if ( !element ) return "FAIL no element";
		const auto at = element->GetAbsoluteOffset( Rml::BoxArea::Border );
		const QPointF pos( ( at.x + element->GetOffsetWidth() / 2.f ) / native->devicePixelRatio(), ( at.y + element->GetOffsetHeight() / 2.f ) / native->devicePixelRatio() );
		QMouseEvent press( QEvent::MouseButtonPress, pos, native->mapToGlobal( pos ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
		QCoreApplication::sendEvent( native, &press );
		QMouseEvent release( QEvent::MouseButtonRelease, pos, native->mapToGlobal( pos ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
		QCoreApplication::sendEvent( native, &release );
	}
	auto* popup = help.popupElement();
	return "PASS whats mode=" + std::to_string( help.mode() ) + " cursor=" + std::to_string( int( native->cursor().shape() ) )
		+ " popup=" + ( popup ? QString::fromStdString( popup->GetInnerRML() ).left( 40 ).replace( ' ', '_' ).toStdString() : std::string( "-" ) );
}
bool MainWindow::clickManagementFarmCropForProbe( std::string_view crop )
{
	if ( !m_agricultureWindow || !m_agricultureWindow->nativeWindow || !m_agricultureWindow->binding ) return false;
	auto* document = m_agricultureWindow->binding->agricultureDocument();
	if ( !document || !document->IsVisible() ) return false;
	const auto id = std::string( "agriculture_farm_crop_" ) + QByteArray( crop.data(), static_cast<qsizetype>( crop.size() ) ).toHex().toStdString();
	auto* element = document->GetElementById( id );
	if ( !element || !element->IsVisible( true ) ) return false;
    element->ScrollIntoView();
    document->UpdateDocument();
	const auto offset = element->GetAbsoluteOffset( Rml::BoxArea::Border );
	const auto size = element->GetBox().GetSize();
	const qreal dpr = qMax<qreal>( 0.01, m_agricultureWindow->nativeWindow->devicePixelRatio() );
	const QPointF point( ( offset.x + size.x * 0.5f ) / dpr, ( offset.y + size.y * 0.5f ) / dpr );
	auto* window = m_agricultureWindow->nativeWindow.get();
	QMouseEvent move( QEvent::MouseMove, point, window->mapToGlobal( point.toPoint() ), Qt::NoButton, Qt::NoButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &move );
	QMouseEvent press( QEvent::MouseButtonPress, point, window->mapToGlobal( point.toPoint() ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &press );
	QMouseEvent release( QEvent::MouseButtonRelease, point, window->mapToGlobal( point.toPoint() ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &release );
	return true;
}
std::string MainWindow::managementFarmSelectedCropForProbe() const
{
	if ( !m_management6a || !m_management6a->controller( ingnomia::ui::management6a::ManagementView::Agriculture ) ) return {};
	// The Crops page list chooses the pending default crop (Stage 11 property sheet).
	return m_management6a->controller( ingnomia::ui::management6a::ManagementView::Agriculture )->state().agriculture.draft.options.product.value;
}
bool MainWindow::dispatchShellClickForProbe( std::string_view id, std::string* focusedTarget )
{
    return m_shellBinding && m_shellBinding->dispatchElementClickForProbe( id, focusedTarget );
}
std::string MainWindow::verifyShellTabsForProbe()
{
    auto* document = m_shellBinding ? m_shellBinding->routeDocument() : nullptr;
    if(!document) return "FAIL shell document unavailable";
    const auto tabs = verifyNewGameTabs(*document);
    const auto host = verifyShellHostInput(*this, *document->GetContext(), [this] { return m_shellBinding->routeDocument(); });
    if(auto* current = m_shellBinding->routeDocument(); current && qEnvironmentVariable("INGNOMIA_AUTOMATE_HIGH_CONTRAST") == "1") current->SetClass("is-high-contrast", true);
    const auto forms=qEnvironmentVariable("INGNOMIA_AUTOMATE_FORM_CONTROLS")=="1" ? verifyStage05Shell(*this,*m_shellBinding->routeDocument()) : "not_requested";
    return tabs + " qt_host=" + host + " forms=" + forms;
}
std::string MainWindow::shellRouteForProbe() const
{
    return m_shellController ? m_shellController->state().route.value : std::string {};
}
std::string MainWindow::shellFocusedElementForProbe() const
{
    return m_shellBinding ? m_shellBinding->focusedElementIdForProbe() : std::string {};
}
bool MainWindow::requestManagementCaptureForProbe( std::string_view kind, const QString& path )
{
	ingnomia::ui::RmlUiDetachedWindow* native = nullptr;
	if ( kind == "population" && m_management6bWindow ) native = m_management6bWindow->nativeWindow.get();
	else if ( kind == "inventory" && m_inventoryWindow ) native = m_inventoryWindow->nativeWindow.get();
	else if ( kind == "stockpile" && m_stockpileWindow ) native = m_stockpileWindow->nativeWindow.get();
	else if ( kind == "workshop" && m_management6aWindow ) native = m_management6aWindow->nativeWindow.get();
	else if ( kind == "agriculture" && m_agricultureWindow ) native = m_agricultureWindow->nativeWindow.get();
	else if ( kind == "military" && m_management6cWindow ) native = m_management6cWindow->nativeWindow.get();
	else if ( kind == "diplomacy" && m_diplomacyWindow ) native = m_diplomacyWindow->nativeWindow.get();
	else if ( kind == "build" )
	{
		for ( auto& window : m_ordersToolsWindows )
			if ( window && window->panelKey == "build" ) native = window->nativeWindow.get();
	}
	else if ( kind == "game" )
	{
		// The game window itself: re-arm the development capture for the next frame.
		qputenv( "INGNOMIA_UI_CAPTURE", path.toUtf8() );
		qputenv( "INGNOMIA_UI_CAPTURE_FRAME", "1" );
		armUiCapture();
		return true;
	}
	if ( !native || !native->isVisible() ) return false;
	native->requestAutomationCapture( path );
	return true;
}
bool MainWindow::createPopulationProfessionForProbe( std::string_view name )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_PERSISTENCE_MUTATION" ) != "population_profession" || !m_management6bController ) return false;
	if ( populationHasProfessionForProbe( name ) ) return false;
	m_management6bController->createProfession( std::string( name ) );
	return m_management6bController->state().selectedProfession
		&& m_management6bController->state().selectedProfession->value == name;
}
// Shared probe actions for a detached management document (Stages 14-15): clicks rows, checks, options and
// message-box buttons the way a user does, through the production RmlUi elements.
namespace
{
std::string driveManagementDocumentForProbe( Rml::ElementDocument* doc, Rml::Context* context, std::string_view action )
{
	const auto suffix = [&]( std::string_view prefix ) { return std::string( action.substr( prefix.size() ) ); };
	if ( action.starts_with( "click:" ) )
	{
		auto* e = doc ? doc->GetElementById( suffix( "click:" ) ) : nullptr;
		if ( !e ) return "FAIL no element " + suffix( "click:" );
		e->Click();
		return "PASS clicked " + suffix( "click:" );
	}
	if ( action.starts_with( "click-row:" ) )
	{
		const auto rest  = suffix( "click-row:" );
		const auto colon = rest.find( ':' );
		auto* list       = doc ? doc->GetElementById( rest.substr( 0, colon ) ) : nullptr;
		const int index  = std::stoi( rest.substr( colon + 1 ) );
		if ( !list || index < 0 || index >= list->GetNumChildren() ) return "FAIL no row " + rest;
		list->GetChild( index )->Click();
		return "PASS clicked row " + rest;
	}
	if ( action.starts_with( "choose:" ) )
	{
		const auto rest  = suffix( "choose:" );
		const auto colon = rest.find( ':' );
		auto* select     = doc ? rmlui_dynamic_cast<Rml::ElementFormControl*>( doc->GetElementById( rest.substr( 0, colon ) ) ) : nullptr;
		if ( !select ) return "FAIL no select " + rest;
		select->SetValue( rest.substr( colon + 1 ) );
		select->DispatchEvent( "change", Rml::Dictionary {} );
		return "PASS chose " + rest;
	}
	if ( action.starts_with( "choose-index:" ) )
	{
		// choose-index:<select>:<n> picks the n-th real option (skipping a blank entry), as a user would.
		const auto rest  = suffix( "choose-index:" );
		const auto colon = rest.find( ':' );
		auto* select     = doc ? rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( doc->GetElementById( rest.substr( 0, colon ) ) ) : nullptr;
		if ( !select ) return "FAIL no select " + rest;
		int wanted = std::stoi( rest.substr( colon + 1 ) );
		for ( int i = 0; i < select->GetNumOptions(); ++i )
		{
			const auto value = select->GetOption( i )->GetAttribute<Rml::String>( "value", "" );
			if ( value.empty() ) continue;
			if ( wanted-- == 0 )
			{
				select->SetValue( value );
				select->DispatchEvent( "change", Rml::Dictionary {} );
				return "PASS chose " + rest + " value=" + value;
			}
		}
		return "FAIL no option " + rest;
	}
	if ( action.starts_with( "type:" ) )
	{
		const auto rest  = suffix( "type:" );
		const auto colon = rest.find( ':' );
		auto* field      = doc ? rmlui_dynamic_cast<Rml::ElementFormControl*>( doc->GetElementById( rest.substr( 0, colon ) ) ) : nullptr;
		if ( !field ) return "FAIL no field " + rest;
		field->SetValue( rest.substr( colon + 1 ) );
		return "PASS typed " + rest;
	}
	if ( action.starts_with( "dialog:" ) )
	{
		const auto id = "confirm-" + suffix( "dialog:" );
		if ( context )
			for ( int i = 0; i < context->GetNumDocuments(); ++i )
				if ( auto* e = context->GetDocument( i )->GetElementById( id ); e && context->GetDocument( i ) != doc && context->GetDocument( i )->IsVisible() )
				{
					e->Click();
					return "PASS dialog " + id;
				}
		return "FAIL no dialog " + id;
	}
	return {};
}
}
std::string MainWindow::militaryStage14Probe( std::string_view action )
{
	using namespace ingnomia::ui::management6c;
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_MILITARY_STAGE14_LIVE" ) != "1" || !m_management6cController || !m_management6cWindow || !m_management6cWindow->binding ) return "FAIL probe unavailable";
	auto* doc     = m_management6cWindow->binding->militaryDocument();
	auto* context = m_management6cWindow->context ? m_management6cWindow->context->context() : nullptr;
	if ( auto driven = driveManagementDocumentForProbe( doc, context, action ); !driven.empty() ) return driven;
	const auto& s = m_management6cController->state();
	// Names are written with '_' for spaces so the state line stays space-separated.
	const auto word = []( std::string v ) { std::ranges::replace( v, ' ', '_' ); return v; };
	std::string squads;
	for ( const auto& q : s.roster.squads )
	{
		squads += word( q.name ) + "[";
		for ( const auto& m : q.members ) squads += word( m.name ) + ",";
		squads += "]{";
		for ( const auto& t : q.priorities ) squads += t.targetType.value + "=" + std::to_string( int( t.attitude ) ) + ",";
		squads += "};";
	}
	std::string roles;
	for ( const auto& r : s.roles ) roles += word( r.name ) + ( r.civilian ? "(civ);" : ";" );
	std::string unassigned;
	for ( const auto& m : s.roster.unassigned ) unassigned += word( m.name ) + ",";
	return "STATE squads=" + squads + " unassigned=" + unassigned + " roles=" + roles + " selectedSquad=" + ( s.selectedSquad ? std::to_string( s.selectedSquad->value ) : std::string( "-" ) )
		+ " selectedMember=" + ( s.selectedMember ? std::to_string( s.selectedMember->value ) : std::string( "-" ) ) + " pending=" + std::to_string( s.pendingAction.has_value() ) + " open=" + std::to_string( s.militaryOpen );
}
std::string MainWindow::diplomacyStage15Probe( std::string_view action )
{
	using namespace ingnomia::ui::management6c;
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_DIPLOMACY_STAGE15_LIVE" ) != "1" || !m_management6cController || !m_diplomacyWindow || !m_diplomacyWindow->binding ) return "FAIL probe unavailable";
	auto* doc     = m_diplomacyWindow->binding->diplomacyDocument();
	auto* context = m_diplomacyWindow->context ? m_diplomacyWindow->context->context() : nullptr;
	if ( auto driven = driveManagementDocumentForProbe( doc, context, action ); !driven.empty() ) return driven;
	const auto& s   = m_management6cController->state();
	const auto word = []( std::string v ) { std::ranges::replace( v, ' ', '_' ); return v; };
	std::string neighbors;
	for ( const auto& n : s.neighbors )
		neighbors += std::to_string( n.id.value ) + ":" + ( n.discovered ? word( n.name.value_or( "?" ) ) : std::string( "undiscovered" ) ) + ( n.canSendEmissary ? "+E" : "" ) + ( n.canSpy ? "+S" : "" ) + ( n.canRaid ? "+R" : "" ) + ( n.canSabotage ? "+B" : "" ) + ";";
	std::string missions;
	for ( const auto& m : s.missions ) missions += std::to_string( m.id.value ) + ":" + std::to_string( int( m.type ) ) + "/" + std::to_string( int( m.action ) ) + "@" + std::to_string( m.target.value ) + ";";
	auto* wizard = doc ? doc->GetElementById( "mission_wizard" ) : nullptr;
	return "STATE neighbors=" + neighbors + " missions=" + missions + " selected=" + ( s.selectedNeighbor ? std::to_string( s.selectedNeighbor->value ) : std::string( "-" ) )
		+ " draft=" + std::to_string( int( s.missionDraft.type ) ) + "/" + std::to_string( int( s.missionDraft.action ) ) + "/" + ( s.missionDraft.creature ? std::to_string( s.missionDraft.creature->value ) : std::string( "-" ) )
		+ " gnomes=" + std::to_string( s.availableGnomes.size() ) + " wizard=" + std::to_string( wizard && wizard->IsVisible( true ) ) + " view=" + std::to_string( int( s.diplomacyView ) ) + " open=" + std::to_string( s.diplomacyOpen );
}
// Stage 18 live probe: drives the main menu, the Custom Game wizard and the Load Game dialog through the routed
// shell document. "dblclick:<id>" double-clicks an element; "state" reports the route, focus and dialog contents.
std::string MainWindow::shellStage18Probe( std::string_view action )
{
	if ( ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE18_LIVE" ) != "1" && qEnvironmentVariable( "INGNOMIA_AUTOMATE_SHELL_STAGE19_LIVE" ) != "1" && qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE20_LIVE" ) != "1" ) || !m_shellBinding || !m_shellController ) return "FAIL probe unavailable";
	auto* doc = m_shellBinding->routeDocument();
	if ( action == "escape" ) return m_shellController->handleEscape() ? "PASS Esc handled" : "FAIL Esc not handled";
	if ( action.starts_with( "dblclick:" ) )
	{
		auto* e = doc ? doc->GetElementById( std::string( action.substr( 9 ) ) ) : nullptr;
		if ( !e ) return "FAIL no element";
		e->DispatchEvent( "dblclick", Rml::Dictionary {} );
		return "PASS double-clicked " + std::string( action.substr( 9 ) );
	}
	if ( auto driven = driveManagementDocumentForProbe( doc, m_rmlUiHost ? m_rmlUiHost->context() : nullptr, action ); !driven.empty() ) return driven;
	const auto& s = m_shellController->state();
	const auto shown = [doc]( const char* id ) { auto* e = doc ? doc->GetElementById( id ) : nullptr; return e && e->IsVisible( true ); };
	std::string settingsPage = "-";
	for ( const char* id : { "settings-page-display", "settings-page-controls", "settings-page-sound", "settings-page-saving" } )
		if ( shown( id ) ) settingsPage = id;
	bool dialog = false;
	if ( auto* context = m_rmlUiHost ? m_rmlUiHost->context() : nullptr )
		for ( int i = 0; i < context->GetNumDocuments(); ++i )
			if ( auto* e = context->GetDocument( i )->GetElementById( "confirm-accept" ); e && context->GetDocument( i )->IsVisible() ) dialog = true;
	std::string page = "-";
	for ( const char* id : { "new-panel-welcome", "new-panel-world", "new-panel-settlement", "new-panel-terrain", "new-panel-review" } )
		if ( shown( id ) ) page = id;
	auto* name = doc ? rmlui_dynamic_cast<Rml::ElementFormControl*>( doc->GetElementById( "load-file-name" ) ) : nullptr;
	auto* back = doc ? doc->GetElementById( "new-back" ) : nullptr;
	std::string fileName = name ? std::string( name->GetValue() ) : std::string( "-" );
	std::ranges::replace( fileName, ' ', '_' );
	return "STATE route=" + s.route.value + " focus=" + m_shellBinding->focusedElementIdForProbe() + " page=" + page + " next=" + std::to_string( shown( "new-next" ) )
		+ " finish=" + std::to_string( shown( "new-start" ) ) + " backEnabled=" + std::to_string( back && !back->HasAttribute( "disabled" ) ) + " kingdoms=" + std::to_string( s.loadGame.kingdoms.size() )
		+ " saves=" + std::to_string( s.loadGame.saves.size() ) + " selectedSave=" + std::to_string( s.loadGame.selectedSlot.has_value() ) + " fileName=" + ( fileName.empty() ? std::string( "-" ) : fileName )
		+ " finishEnabled=" + std::to_string( doc && doc->GetElementById( "new-start" ) && !doc->GetElementById( "new-start" )->HasAttribute( "disabled" ) ) + " pending=" + std::to_string( s.pendingRequest.has_value() )
		+ " errors=" + std::to_string( s.newGame.validationErrors.size() ) + " newGameStatus=" + std::to_string( int( s.newGame.status ) )
		+ " openEnabled=" + std::to_string( doc && doc->GetElementById( "load-selected" ) && !doc->GetElementById( "load-selected" )->HasAttribute( "disabled" ) )
		+ " settingsPage=" + settingsPage + " inGame=" + std::to_string( doc && doc->IsClassSet( "is-in-game" ) ) + " loadingError=" + std::to_string( shown( "loading-error" ) ) + " retry=" + std::to_string( shown( "loading-retry" ) )
		+ " confirm=" + std::to_string( dialog ) + " paused=" + std::to_string( s.authoritativePaused ) + " cursor=" + std::to_string( int( cursor().shape() ) );
}
// Primary window frame probe: reports the frame's state and geometry (physical pixels), clicks its caption buttons,
// double-clicks its caption, and restores a minimized window so the run can continue.
std::string MainWindow::windowFrameProbe( std::string_view action )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_STAGE17_LIVE" ) != "1" || !m_windowFrame || !m_windowFrame->document() || !m_rmlUiHost ) return "FAIL frame probe unavailable";
	auto* doc = m_windowFrame->document();
	if ( action.starts_with( "click:" ) )
	{
		auto* element = doc->GetElementById( Rml::String( action.substr( 6 ) ) );
		if ( !element ) return "FAIL no frame element " + std::string( action.substr( 6 ) );
		element->Click();
		return "PASS clicked " + std::string( action.substr( 6 ) );
	}
	if ( action == "restore" )
	{
		showNormal();
		return "PASS restored";
	}
	const auto menuState = []( ingnomia::ui::window_menu::WindowMenu* menu ) -> std::string
	{
		if ( !menu || !menu->isOpen() ) return "closed";
		Rml::ElementList items;
		menu->element()->QuerySelectorAll( items, "button.w98-menu__item" );
		std::string out = "open";
		for ( auto* item : items )
			out += " " + item->GetAttribute<Rml::String>( "data-command", "" ) + ( item->HasAttribute( "disabled" ) ? "-" : "+" ) + item->GetAttribute<Rml::String>( "accesskey", "?" ) + ( item->IsClassSet( "is-checked" ) ? "*" : "" );
		return out;
	};
	// Window menu: "menu" opens it as Alt+Space does and lists its commands (command, + or - for available, access
	// letter); "menu-key:<letter>" types a letter into it.
	if ( action == "menu" ) return m_windowFrame->openMenuFromKeyboard() ? "PASS menu " + menuState( m_windowFrame->menu() ) : "FAIL menu did not open";
	// What's This? in the game window: "whats-this" chooses the toolbar button; "whats-click:<id>" clicks a HUD element
	// through the native pointer; "whats-state" reports the mode, pointer and pop-up.
	if ( action == "whats-this" || action.starts_with( "whats-click:" ) || action == "whats-state" )
	{
		auto* hud = m_hudBinding ? m_hudBinding->document() : nullptr;
		if ( !hud ) return "FAIL no game window document";
		if ( action == "whats-this" )
		{
			auto* button = hud->GetElementById( "hud_whats_this" );
			if ( !button ) return "FAIL no What's This? button";
			button->Click();
		}
		else if ( action.starts_with( "whats-click:" ) )
		{
			auto* element = hud->GetElementById( std::string( action.substr( 12 ) ) );
			if ( !element ) return "FAIL no element";
			const auto at = element->GetAbsoluteOffset( Rml::BoxArea::Border );
			const QPointF pos( ( at.x + element->GetOffsetWidth() / 2.f ) / devicePixelRatio(), ( at.y + element->GetOffsetHeight() / 2.f ) / devicePixelRatio() );
			QMouseEvent press( QEvent::MouseButtonPress, pos, mapToGlobal( pos ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
			mousePressEvent( &press );
			QMouseEvent release( QEvent::MouseButtonRelease, pos, mapToGlobal( pos ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
			mouseReleaseEvent( &release );
		}
		auto* popup = whatsThis().popupElement();
		return "PASS whats mode=" + std::to_string( whatsThis().mode() ) + " cursor=" + std::to_string( int( cursor().shape() ) )
			+ " popup=" + ( popup ? QString::fromStdString( popup->GetInnerRML() ).left( 40 ).replace( ' ', '_' ).toStdString() : std::string( "-" ) );
	}
	if ( action.starts_with( "menu-key:" ) )
	{
		const char letter = action.back();
		if ( !ingnomia::ui::access_keys::activate( *m_rmlUiHost->context(), letter, false ) ) return "FAIL letter not taken";
		return "PASS typed " + std::string( 1, letter ) + " menu " + menuState( m_windowFrame->menu() );
	}
	if ( action == "dblclick" )
	{
		auto* title = doc->GetElementById( "frame_title" );
		if ( !title ) return "FAIL no caption title";
		const auto at = title->GetAbsoluteOffset( Rml::BoxArea::Border );
		const QPointF pos( ( at.x + title->GetOffsetWidth() / 2.f ) / devicePixelRatio(), ( at.y + title->GetOffsetHeight() / 2.f ) / devicePixelRatio() );
		QMouseEvent event( QEvent::MouseButtonDblClick, pos, mapToGlobal( pos ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
		event.setAccepted( false );
		mouseDoubleClickEvent( &event );
		return event.isAccepted() ? "PASS caption double-clicked" : "FAIL the caption did not take the double-click";
	}
	const auto rect = []( Rml::Element* e ) -> std::string
	{
		if ( !e ) return "-";
		const auto at = e->GetAbsoluteOffset( Rml::BoxArea::Border );
		return std::to_string( int( at.x + 0.5f ) ) + "," + std::to_string( int( at.y + 0.5f ) ) + "," + std::to_string( int( e->GetOffsetWidth() + 0.5f ) ) + "x" + std::to_string( int( e->GetOffsetHeight() + 0.5f ) );
	};
	std::string hud = "-", grip = "-";
	if ( m_hudBinding && m_hudBinding->document() )
	{
		hud = m_hudBinding->document()->GetAttribute<Rml::String>( "data-frame-client", "-" );
		if ( auto* g = m_hudBinding->document()->GetElementById( "hud_size_grip" ); g && g->IsVisible( true ) ) grip = rect( g );
	}
	auto* maximize = doc->GetElementById( "frame_maximize" );
	auto* title    = doc->GetElementById( "frame_title" );
	const std::string tip = maximize ? maximize->GetAttribute<Rml::String>( "title", "-" ) : "-";
	const auto states     = windowStates();
	const QRect work      = screen() ? screen()->availableGeometry() : QRect();
	const auto size       = m_rmlUiHost->context()->GetDimensions();
	return "state visible=" + std::to_string( doc->IsVisible() ) + " frameMax=" + std::to_string( doc->IsClassSet( "is-maximized" ) )
		+ " maximized=" + std::to_string( states.testFlag( Qt::WindowMaximized ) ) + " minimized=" + std::to_string( states.testFlag( Qt::WindowMinimized ) )
		+ " fullScreen=" + std::to_string( states.testFlag( Qt::WindowFullScreen ) ) + " fillsWork=" + std::to_string( geometry() == work )
		+ " context=" + std::to_string( size.x ) + "x" + std::to_string( size.y ) + " caption=" + rect( doc->GetElementById( "frame_caption" ) )
		+ " icon=" + rect( doc->GetElementById( "frame_icon" ) ) + " min=" + rect( doc->GetElementById( "frame_minimize" ) ) + " max=" + rect( maximize )
		+ " close=" + rect( doc->GetElementById( "frame_close" ) ) + " client=" + rect( doc->GetElementById( "frame_client" ) ) + " hud=" + hud + " grip=" + grip + " maxTip=" + tip
		+ " title=" + ( title ? QString::fromStdString( title->GetInnerRML() ).replace( ' ', '_' ).toStdString() : std::string( "-" ) );
}
// Stage 17 live probe: drives the game window's toolbar, menus and status bar and the Build window through their
// production documents. "build|<action>" acts on the Build window; "hover:<id>" moves the pointer onto a control;
// "prompt" queues an information message box; "state" reports what the player would see.
std::string MainWindow::hudStage17Probe( std::string_view action )
{
	if ( ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_HUD_STAGE17_LIVE" ) != "1" && qEnvironmentVariable( "INGNOMIA_AUTOMATE_STAGE20_LIVE" ) != "1" ) || !m_hudBinding || !m_hudController ) return "FAIL probe unavailable";
	auto* doc = m_hudBinding->document();
	Rml::ElementDocument* buildDoc = nullptr;
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->panelKey == "build" && window->binding && window->nativeWindow && window->nativeWindow->isVisible() ) buildDoc = window->binding->document();
	// The Build palette's window menu (PDF p.113, p.181): "build|menu" opens it as Alt+Space does and lists its
	// commands; "build|menu-key:<letter>" types a letter into it.
	if ( action == "build|menu" || action.starts_with( "build|menu-key:" ) )
	{
		ingnomia::ui::RmlUiDetachedWindow* native = nullptr;
		Rml::Context* context = nullptr;
		for ( auto& window : m_ordersToolsWindows )
			if ( window && window->panelKey == "build" && window->nativeWindow && window->nativeWindow->isVisible() && window->context )
			{
				native  = window->nativeWindow.get();
				context = window->context->context();
			}
		if ( !native || !context ) return "FAIL Build window not open";
	const auto menuState = []( ingnomia::ui::window_menu::WindowMenu* menu ) -> std::string
	{
		if ( !menu || !menu->isOpen() ) return "closed";
		Rml::ElementList items;
		menu->element()->QuerySelectorAll( items, "button.w98-menu__item" );
		std::string out = "open";
		for ( auto* item : items )
			out += " " + item->GetAttribute<Rml::String>( "data-command", "" ) + ( item->HasAttribute( "disabled" ) ? "-" : "+" ) + item->GetAttribute<Rml::String>( "accesskey", "?" ) + ( item->IsClassSet( "is-checked" ) ? "*" : "" );
		return out;
	};
		if ( action == "build|menu" ) return native->openWindowMenu() ? "PASS menu " + menuState( native->windowMenu() ) + " onTop=" + std::to_string( native->alwaysOnTop() ) : "FAIL no title bar";
		const char letter = action.back();
		if ( !ingnomia::ui::access_keys::activate( *context, letter, false ) ) return "FAIL letter not taken";
		auto* help = native->whatsThis().popupElement();
		return "PASS typed " + std::string( 1, letter ) + " menu " + menuState( native->windowMenu() ) + " onTop=" + std::to_string( native->alwaysOnTop() )
			+ " help=" + ( help ? std::string( "open" ) : std::string( "closed" ) );
	}
	// What's This? in the Build palette through the native pointer: "build|right:<id>" presses the secondary button on
	// the element; "build|help" reports the What's This? menu and pop-up.
	if ( action.starts_with( "build|right:" ) || action == "build|help" || action == "build|activate" )
	{
		ingnomia::ui::RmlUiDetachedWindow* native = nullptr;
		for ( auto& window : m_ordersToolsWindows )
			if ( window && window->panelKey == "build" && window->nativeWindow && window->nativeWindow->isVisible() ) native = window->nativeWindow.get();
		if ( !native || !buildDoc ) return "FAIL Build window not open";
		auto& help = native->whatsThis();
		if ( action == "build|activate" )
		{
			native->requestActivate();
			return "PASS activated";
		}
		if ( action.starts_with( "build|right:" ) )
		{
			auto* element = buildDoc->GetElementById( std::string( action.substr( 12 ) ) );
			if ( !element ) return "FAIL no element";
			const auto at  = element->GetAbsoluteOffset( Rml::BoxArea::Border );
			const QPointF pos( ( at.x + element->GetOffsetWidth() / 2.f ) / native->devicePixelRatio(), ( at.y + element->GetOffsetHeight() / 2.f ) / native->devicePixelRatio() );
			QMouseEvent press( QEvent::MouseButtonPress, pos, native->mapToGlobal( pos ), Qt::RightButton, Qt::RightButton, Qt::NoModifier );
			QCoreApplication::sendEvent( native, &press );
		}
		const bool menuOpen = help.menu() && help.menu()->isOpen();
		auto* popup = help.popupElement();
		return "PASS help menu=" + std::to_string( menuOpen ) + " popup=" + ( popup ? QString::fromStdString( popup->GetInnerRML() ).left( 40 ).replace( ' ', '_' ).toStdString() : std::string( "-" ) );
	}
	if ( action.starts_with( "build|" ) )
	{
		if ( !buildDoc ) return "FAIL Build window not open";
		auto driven = driveManagementDocumentForProbe( buildDoc, nullptr, action.substr( 6 ) );
		return driven.empty() ? "FAIL unknown action" : driven;
	}
	if ( action.starts_with( "hover:" ) )
	{
		auto* e = doc ? doc->GetElementById( std::string( action.substr( 6 ) ) ) : nullptr;
		if ( !e ) return "FAIL no element";
		e->DispatchEvent( "mouseover", Rml::Dictionary {} );
		return "PASS hovered " + std::string( action.substr( 6 ) );
	}
	if ( action == "unhover" )
	{
		for ( const char* id : { "hud_open_population", "hud_tool_cancel" } )
			if ( auto* e = doc ? doc->GetElementById( id ) : nullptr ) e->DispatchEvent( "mouseout", Rml::Dictionary {} );
		return "PASS unhovered";
	}
	// Stage 20: real Qt key events into the game window, and the Build palette's native position.
	if ( action.starts_with( "key:" ) )
	{
		const auto parts = QString::fromStdString( std::string( action.substr( 4 ) ) ).split( ':' );
		const int qtKey = parts.value( 0 ).toInt();
		const auto mods = Qt::KeyboardModifiers( parts.value( 1 ).toInt() );
		QKeyEvent press( QEvent::KeyPress, qtKey, mods, qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z && !( mods & Qt::AltModifier ) ? QString( QChar( 'a' + ( qtKey - Qt::Key_A ) ) ) : QString() );
		QCoreApplication::sendEvent( this, &press );
		QKeyEvent release( QEvent::KeyRelease, qtKey, mods );
		QCoreApplication::sendEvent( this, &release );
		return "PASS key " + std::to_string( qtKey ) + "/" + std::to_string( int( mods ) );
	}
	if ( action.starts_with( "config:" ) )
		return "PASS " + ( Global::cfg ? Global::cfg->get( QString::fromStdString( std::string( action.substr( 7 ) ) ) ).toString().toStdString() : std::string( "-" ) );
	if ( action == "build-position" || action.starts_with( "build-move:" ) )
	{
		for ( auto& window : m_ordersToolsWindows )
			if ( window && window->panelKey == "build" && window->nativeWindow )
			{
				if ( action.starts_with( "build-move:" ) )
				{
					const auto parts = QString::fromStdString( std::string( action.substr( 11 ) ) ).split( ':' );
					window->nativeWindow->setPosition( window->nativeWindow->position() + QPoint( parts.value( 0 ).toInt(), parts.value( 1 ).toInt() ) );
				}
				const auto at = window->nativeWindow->position();
				return "PASS position=" + std::to_string( at.x() ) + "," + std::to_string( at.y() ) + " visible=" + std::to_string( window->nativeWindow->isVisible() );
			}
		return "FAIL no Build window";
	}
	if ( action == "prompt" )
		return m_hudController->enqueuePrompt( std::nullopt, "Probe Message", "A caravan arrived at the edge of the map.", false, false ) ? "PASS prompt queued" : "FAIL prompt refused";
	if ( auto driven = driveManagementDocumentForProbe( doc, nullptr, action ); !driven.empty() ) return driven;
	const auto word = []( std::string v ) { std::ranges::replace( v, ' ', '_' ); return v; };
	const auto textOf = [word]( Rml::ElementDocument* d, const char* id ) { auto* e = d ? d->GetElementById( id ) : nullptr; return e ? word( std::string( e->GetInnerRML() ) ) : std::string( "-" ); };
	std::string selected, menus;
	if ( doc )
	{
		for ( const char* id : { "hud_pause", "hud_speed_normal", "hud_speed_fast", "hud_tool_inspect", "hud_tool_build", "hud_tool_deconstruct", "hud_tool_mine", "hud_tool_agriculture", "hud_tool_designations", "hud_tool_jobs" } )
			if ( auto* e = doc->GetElementById( id ); e && e->IsClassSet( "is-selected" ) ) selected += std::string( id ) + ",";
		for ( const char* id : { "hud_mine_menu", "hud_agriculture_menu", "hud_designations_menu", "hud_jobs_menu", "hud_view_menu" } )
			if ( auto* e = doc->GetElementById( id ); e && e->IsVisible( true ) ) menus += std::string( id ) + ",";
	}
	const auto& s = m_hudController->state();
	std::string buildRow = "-";
	if ( auto* items = buildDoc ? buildDoc->GetElementById( "hud_build_items" ) : nullptr )
		for ( int i = 0; i < items->GetNumChildren(); ++i )
			if ( items->GetChild( i )->IsClassSet( "is-selected" ) ) buildRow = items->GetChild( i )->GetId();
	auto* cancel = doc ? doc->GetElementById( "hud_tool_cancel" ) : nullptr;
	return "STATE selected=" + selected + " menus=" + menus + " tool=" + ( s.tool.active ? s.tool.active->value : std::string( "-" ) ) + " status=" + textOf( doc, "hud_status" )
		+ " activeTool=" + textOf( doc, "hud_active_tool" ) + " cursor=" + std::to_string( int( cursor().shape() ) ) + " overlayJobs=" + std::to_string( s.overlays.jobs )
		+ " speed=" + std::to_string( int( s.clock.speed ) ) + " cancelEnabled=" + std::to_string( cancel && !cancel->HasAttribute( "disabled" ) ) + " build=" + std::to_string( buildDoc != nullptr )
		+ " buildRows=" + std::to_string( buildDoc && buildDoc->GetElementById( "hud_build_items" ) ? buildDoc->GetElementById( "hud_build_items" )->GetNumChildren() : 0 ) + " buildRow=" + buildRow
		+ " prompt=" + std::to_string( !s.prompts.empty() ) + " level=" + textOf( doc, "hud_level" ) + " gameLevel=" + std::to_string( GameState::viewLevel );
}
// Stage 16 live probe: drives the tile and creature inspectors (palette windows) through their production
// documents. "tile|<action>" acts on the live tile inspector, "creature|<action>" on the newest visible creature
// inspector, "capture:<tile|creature>:<path>" captures one, and "state" reports both.
std::string MainWindow::inspectorStage16Probe( std::string_view action )
{
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_INSPECTOR_STAGE16_LIVE" ) != "1" ) return "FAIL probe unavailable";
	const auto find = [this]( bool tile ) -> CreatureInspectorWindow*
	{
		CreatureInspectorWindow* found = nullptr;
		for ( auto& window : m_creatureInspectorWindows )
			if ( window.binding && window.nativeWindow && window.nativeWindow->isVisible() && ( tile ? window.slot == -2 : window.slot > 0 ) ) found = &window;
		return found;
	};
	const auto word = []( std::string v ) { std::ranges::replace( v, ' ', '_' ); return v; };
	if ( action.starts_with( "capture:" ) )
	{
		const auto rest = std::string( action.substr( 8 ) );
		const auto colon = rest.find( ':' );
		auto* window = find( rest.substr( 0, colon ) == "tile" );
		if ( !window ) return "FAIL no window to capture";
		window->nativeWindow->requestAutomationCapture( QString::fromStdString( rest.substr( colon + 1 ) ) );
		return "PASS capture requested " + rest.substr( 0, colon );
	}
	if ( action.starts_with( "population|" ) )
	{
		if ( !m_management6bWindow || !m_management6bWindow->binding ) return "FAIL population not open";
		auto driven = driveManagementDocumentForProbe( m_management6bWindow->binding->populationDocument(), m_management6bWindow->context ? m_management6bWindow->context->context() : nullptr, action.substr( 11 ) );
		return driven.empty() ? "FAIL unknown action" : driven;
	}
	if ( const auto bar = action.find( '|' ); bar != std::string_view::npos )
	{
		auto* window = find( action.substr( 0, bar ) == "tile" );
		if ( !window ) return "FAIL no inspector " + std::string( action.substr( 0, bar ) );
		auto driven = driveManagementDocumentForProbe( window->binding->document(), window->context ? window->context->context() : nullptr, action.substr( bar + 1 ) );
		return driven.empty() ? "FAIL unknown action" : driven;
	}
	std::string tile = "-", creatures;
	if ( auto* window = find( true ); window && window->controller )
	{
		const auto& s = window->controller->state();
		auto* rows = window->binding->document() ? window->binding->document()->GetElementById( "live_tile_rows" ) : nullptr;
		tile = std::to_string( s.tile ? s.tile->id.value : 0 ) + ":rows=" + std::to_string( rows ? rows->GetNumChildren() : 0 ) + ":creatures=";
		if ( s.tile )
			for ( const auto& c : s.tile->creatures ) tile += std::to_string( c.id.value ) + ",";
	}
	for ( auto& window : m_creatureInspectorWindows )
		if ( window.slot > 0 && window.controller && window.controller->state().creature && window.nativeWindow && window.nativeWindow->isVisible() )
		{
			const auto& c = *window.controller->state().creature;
			auto* doc = window.binding->document();
			std::string page = "-";
			for ( const char* id : { "creature_preview_camera_panel", "creature_preview_stats_panel", "creature_preview_expertise_panel", "creature_preview_equipment_panel", "creature_preview_inventory_panel" } )
				if ( auto* e = doc ? doc->GetElementById( id ) : nullptr; e && e->IsVisible( true ) ) page = id;
			creatures += std::to_string( c.id.value ) + ":" + word( c.name ) + ":" + word( c.profession ) + ":" + page + ":role=" + std::to_string( c.equipmentRole.value ) + ";";
		}
	const bool populationDetail = m_management6bWindow && m_management6bWindow->binding && m_management6bWindow->binding->populationDocument()
		&& m_management6bWindow->binding->populationDocument()->GetElementById( "creature_detail" );
	const auto popSelected = m_management6bController && m_management6bController->state().selectedCreature ? std::to_string( m_management6bController->state().selectedCreature->value ) : std::string( "-" );
	return "STATE tile=" + tile + " creatures=" + creatures + " populationDetail=" + std::to_string( populationDetail ) + " popSelected=" + popSelected;
}
// Stage 12 live probe: reads and drives the Population sheet through its production document.
std::string MainWindow::populationStage12Probe( std::string_view action )
{
	using namespace ingnomia::ui::management6b;
	if ( ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_POPULATION_STAGE12_LIVE" ) != "1" && qEnvironmentVariable( "INGNOMIA_AUTOMATE_SCHEDULE_STAGE13_LIVE" ) != "1" ) || !m_management6bController || !m_management6bWindow || !m_management6bWindow->binding ) return "FAIL probe unavailable";
	auto* doc = m_management6bWindow->binding->populationDocument();
	auto* context = m_management6bWindow->context ? m_management6bWindow->context->context() : nullptr;
	const auto& s = m_management6bController->state();
	const auto nth = [&]( const char* list, int index ) -> Rml::Element* {
		auto* l = doc ? doc->GetElementById( list ) : nullptr;
		return l && index >= 0 && index < l->GetNumChildren() ? l->GetChild( index ) : nullptr;
	};
	const auto suffix = [&]( std::string_view prefix ) { return std::string( action.substr( prefix.size() ) ); };
	if ( action.starts_with( "click-row:" ) )
	{
		// click-row:<list>:<index> clicks a row (or the check box inside it) like the user would.
		const auto rest = suffix( "click-row:" );
		const auto colon = rest.find( ':' );
		auto* row = nth( rest.substr( 0, colon ).c_str(), std::stoi( rest.substr( colon + 1 ) ) );
		if ( !row ) return "FAIL no row " + rest;
		auto* target = row;
		for ( int i = 0; i < row->GetNumChildren(); ++i )
			if ( row->GetChild( i )->GetTagName() == "input" ) target = row->GetChild( i );
		target->DispatchEvent( "click", Rml::Dictionary {} );
		return "PASS clicked " + rest;
	}
	if ( action.starts_with( "choose:" ) )
	{
		// choose:<select id>:<value> picks a drop-down value the way a user choice does (change event).
		const auto rest = suffix( "choose:" );
		const auto colon = rest.find( ':' );
		auto* select = doc ? rmlui_dynamic_cast<Rml::ElementFormControl*>( doc->GetElementById( rest.substr( 0, colon ) ) ) : nullptr;
		if ( !select ) return "FAIL no select " + rest;
		select->SetValue( rest.substr( colon + 1 ) );
		select->DispatchEvent( "change", Rml::Dictionary {} );
		return "PASS chose " + rest;
	}
	if ( action.starts_with( "click-cell:" ) )
	{
		// click-cell:<row>:<hour>[:shift] clicks a schedule cell (Shift extends the range).
		const auto rest  = suffix( "click-cell:" );
		const auto a     = rest.find( ':' );
		const auto b     = rest.find( ':', a + 1 );
		auto* row        = nth( "schedule_rows", std::stoi( rest.substr( 0, a ) ) );
		const int hour   = std::stoi( rest.substr( a + 1, b == std::string::npos ? std::string::npos : b - a - 1 ) );
		auto* cell       = row && hour < row->GetNumChildren() ? row->GetChild( hour ) : nullptr;
		if ( !cell ) return "FAIL no cell " + rest;
		Rml::Dictionary p;
		if ( b != std::string::npos ) p["shift_key"] = 1;
		cell->DispatchEvent( "click", p );
		return "PASS clicked cell " + rest;
	}
	if ( action.starts_with( "key:" ) )
	{
		// key:<name>[+shift|+ctrl] sends a key to the schedule grid like a keyboard user.
		auto rest = suffix( "key:" );
		Rml::Dictionary p;
		for ( const char* mod : { "shift", "ctrl" } )
			if ( const auto at = rest.find( std::string( "+" ) + mod ); at != std::string::npos ) { p[std::string( mod ) + "_key"] = 1; rest.erase( at ); }
		const int k = rest == "end" ? Rml::Input::KI_END : rest == "home" ? Rml::Input::KI_HOME : rest == "right" ? Rml::Input::KI_RIGHT : rest == "down" ? Rml::Input::KI_DOWN : rest == "return" ? Rml::Input::KI_RETURN : 0;
		auto* grid = doc ? doc->GetElementById( "schedule_rows" ) : nullptr;
		if ( !grid || !k ) return "FAIL key " + rest;
		p["key_identifier"] = k;
		grid->DispatchEvent( "keydown", p );
		return "PASS key " + std::string( action.substr( 4 ) );
	}
	if ( action.starts_with( "schedule-hour:" ) || action.starts_with( "schedule-row:" ) )
	{
		// Authoritative schedule codes: one hour for every citizen, or one citizen's 24 hours.
		const bool hour = action.starts_with( "schedule-hour:" );
		const int n = std::stoi( suffix( hour ? "schedule-hour:" : "schedule-row:" ) );
		const auto code = []( ManagedScheduleActivity a ) { return a == ManagedScheduleActivity::Eat ? 'E' : a == ManagedScheduleActivity::Sleep ? 'S' : a == ManagedScheduleActivity::Training ? 'T' : '.'; };
		std::string out;
		if ( hour ) for ( const auto& r : s.schedules ) out += code( r.hours[static_cast<std::size_t>( n )] );
		else if ( n < static_cast<int>( s.schedules.size() ) ) for ( auto a : s.schedules[static_cast<std::size_t>( n )].hours ) out += code( a );
		return out;
	}
	if ( action == "schedule-scope" )
	{
		const auto scope = m_management6bController->scheduleScope();
		return "SCOPE citizens=" + std::to_string( scope.citizens.size() ) + " hours=" + std::to_string( scope.firstHour ) + "-" + std::to_string( scope.lastHour ) + " active=" + ( s.selectedScheduleCell ? std::to_string( s.selectedScheduleCell->hour ) : std::string( "-" ) );
	}
	if ( action.starts_with( "dialog:" ) )
	{
		const auto id = "confirm-" + suffix( "dialog:" );
		if ( context )
			for ( int i = 0; i < context->GetNumDocuments(); ++i )
				if ( auto* e = context->GetDocument( i )->GetElementById( id ); e && context->GetDocument( i ) != doc && context->GetDocument( i )->IsVisible() )
				{
					e->Click();
					return "PASS dialog " + id;
				}
		return "FAIL no dialog " + id;
	}
	std::string order;
	for ( const auto& r : m_management6bController->visiblePopulation() ) order += r.name + ";";
	std::string professions;
	for ( const auto& p : s.professions ) professions += p.name + "(" + std::to_string( p.skills.size() ) + ");";
	std::string skillState;
	if ( s.selectedSkill )
		for ( const auto& r : s.population )
			for ( const auto& k : r.skills )
				if ( k.id == *s.selectedSkill ) skillState += r.name + "=" + ( k.active ? "on" : "off" ) + ";";
	return "STATE view=" + std::to_string( int( s.populationView ) ) + " sort=" + std::to_string( int( s.populationSort ) ) + " desc=" + std::to_string( s.populationSortDescending )
		+ " selected=" + ( s.selectedCreature ? std::to_string( s.selectedCreature->value ) : std::string( "-" ) ) + " order=" + order + " skill=" + ( s.selectedSkill ? s.selectedSkill->value : std::string( "-" ) ) + " skillState=" + skillState
		+ " profession=" + ( s.selectedProfession ? s.selectedProfession->value : std::string( "-" ) ) + " draft=" + std::to_string( s.professionDraftSkills.size() ) + " dirty=" + std::to_string( s.professionDraftDirty )
		+ " pending=" + std::to_string( s.professionSavePending ) + " professions=" + professions + " open=" + std::to_string( s.populationOpen );
}
bool MainWindow::populationHasProfessionForProbe( std::string_view name ) const
{
	if ( !m_management6bController ) return false;
	for ( const auto& profession : m_management6bController->state().professions )
		if ( profession.name == name ) return true;
	return false;
}
bool MainWindow::verifyManagementWindowsForProbe(bool closePeers)
{
    using namespace ingnomia::ui;
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_INDEPENDENT_WINDOWS") != "1") return false;
    auto visible = [](const auto& w) { return w && w->nativeWindow && w->nativeWindow->isVisible(); };
    if(!closePeers) {
        if(!visible(m_management6aWindow) || !visible(m_stockpileWindow)) return false;
        if(m_inspectorController && m_inspectorController->state().kind != inspector::InspectorKind::None) return false;
        if(!ensureDetachedManagement6B() || !m_management6bWindow->binding->openPopulation(FocusToken{1})) return false;
        if(!ensureDetachedManagement6B(true) || !m_inventoryWindow->binding->openInventory(FocusToken{2})) return false;
        if(!ensureDetachedManagement6C() || !m_management6cWindow->binding->openMilitary(management6c::View::Squads,FocusToken{3})) return false;
        if(!ensureDetachedManagement6C(true) || !m_diplomacyWindow->binding->openDiplomacy(management6c::View::Missions,FocusToken{4})) return false;
        return visible(m_management6aWindow) && visible(m_stockpileWindow) && visible(m_management6bWindow)
            && visible(m_inventoryWindow) && visible(m_management6cWindow) && visible(m_diplomacyWindow)
            && m_management6bWindow->binding->populationDocument()->IsVisible()
            && m_inventoryWindow->binding->inventoryDocument()->IsVisible()
            && m_management6cWindow->binding->militaryDocument()->IsVisible()
            && m_diplomacyWindow->binding->diplomacyDocument()->IsVisible();
    }
    m_stockpileWindow->nativeWindow->requestClose();
    m_management6bWindow->nativeWindow->requestClose();
    m_management6cWindow->nativeWindow->requestClose();
    return visible(m_management6aWindow) && visible(m_inventoryWindow) && visible(m_diplomacyWindow)
        && !visible(m_stockpileWindow) && !visible(m_management6bWindow) && !visible(m_management6cWindow)
        && m_management6aWindow->binding->workshopDocument()->IsVisible()
        && m_inventoryWindow->binding->inventoryDocument()->IsVisible()
        && m_diplomacyWindow->binding->diplomacyDocument()->IsVisible();
}

std::string MainWindow::workshopSettingsStatusForProbe() const
{
	if ((qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_SETTINGS") != "1"
		&& qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_LINKS") != "1"
		&& qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGED_EDIT_PROBE") != "1") || !m_management6a) return {};
    const auto& state = m_management6a->controller(ingnomia::ui::management6a::ManagementView::Workshop)->state();
    const auto& value = state.workshop.value;
	return "name=" + value.name + " generated=" + std::to_string(value.acceptGenerated) + " auto=" + std::to_string(value.autoCraftMissing)
        + " suspended=" + std::to_string(value.suspended) + " linked=" + std::to_string(value.connectStockpile)
        + " available=" + std::to_string(value.canLinkStockpile) + " pending=" + std::to_string(state.pendingAction.has_value())
        + " revision=" + std::to_string(state.workshop.revision.value)
        + " priority=" + std::to_string(value.priority+1)
        + " orderPending=" + std::to_string(state.workshop.orderPending)
        + " feedback=" + state.workshop.orderFeedback;
}
bool MainWindow::managementFarmReadyForProbe() const
{
	using namespace ingnomia::ui::management6a;
	if ( !m_management6a || !m_management6a->controller( ManagementView::Agriculture ) ) return false;
	const auto& state = m_management6a->controller( ManagementView::Agriculture )->state();
	return state.view == ManagementView::Agriculture
		&& state.agriculture.request.status == RequestStatus::Ready
		&& state.agriculture.value.target.kind == ingnomia::ui::AgricultureKind::Farm
		&& static_cast<bool>( state.agriculture.value.target.designation );
}
bool MainWindow::setManagementFormValueForProbe( std::string_view id, std::string_view value )
{
	auto* window = id.starts_with("stockpile_") ? m_stockpileWindow.get() : id.starts_with("agriculture_") ? m_agricultureWindow.get() : m_management6aWindow.get();
	if ( window && window->binding )
		return window->binding->setFormValueForProbe( id, value );
	return m_management6a && m_management6a->setFormValueForProbe( id, value );
}
bool MainWindow::setManagementStockpileSearchForProbe( std::string_view value )
{
	if ( m_stockpileWindow && m_stockpileWindow->binding )
		return m_stockpileWindow->binding->setStockpileSearchForProbe( value );
	return m_management6a && m_management6a->setStockpileSearchForProbe( value );
}
bool MainWindow::activateFirstManagementStockpileFilterForProbe()
{
	if ( m_stockpileWindow && m_stockpileWindow->binding )
		return m_stockpileWindow->binding->activateFirstStockpileFilterForProbe( ingnomia::ui::management6a::TriState::On, ingnomia::ui::FilterDepth::Material );
	return m_management6a && m_management6a->activateFirstStockpileFilterForProbe( ingnomia::ui::management6a::TriState::On, ingnomia::ui::FilterDepth::Material );
}
bool MainWindow::activateManagementStockpileMaterialForProbe( std::string_view item, std::string_view material )
{
	if ( m_stockpileWindow && m_stockpileWindow->binding )
		return m_stockpileWindow->binding->activateStockpileFilterForProbe( item, material );
	return m_management6a && m_management6a->activateStockpileFilterForProbe( item, material );
}
bool MainWindow::selectFirstManagementMixedStockpileFilterForProbe()
{
	if ( m_stockpileWindow && m_stockpileWindow->binding )
		return m_stockpileWindow->binding->activateFirstStockpileFilterForProbe( ingnomia::ui::management6a::TriState::Mixed, ingnomia::ui::FilterDepth::Item );
	return m_management6a && m_management6a->activateFirstStockpileFilterForProbe( ingnomia::ui::management6a::TriState::Mixed, ingnomia::ui::FilterDepth::Item );
}
bool MainWindow::dispatchManagementStockpileFilterKeyForProbe( int keyIdentifier )
{
	if ( m_stockpileWindow && m_stockpileWindow->binding )
		return m_stockpileWindow->binding->dispatchStockpileFilterKeyForProbe( keyIdentifier );
	return m_management6a && m_management6a->dispatchStockpileFilterKeyForProbe( keyIdentifier );
}
bool MainWindow::activateFirstManagementElement( std::string_view kind )
{
	if ( kind == "population" || kind == "schedule" || kind == "inventory" )
	{
		auto* window = kind == "inventory" ? m_inventoryWindow.get() : m_management6bWindow.get();
        if (window && window->binding)
			return window->binding->activateFirstDataElement( kind );
		return m_management6bBinding && m_management6bBinding->activateFirstDataElement( kind );
	}
	auto* window = kind.starts_with("diplomacy") ? m_diplomacyWindow.get() : m_management6cWindow.get();
    if (window && window->binding)
		return window->binding->activateFirstDataElement( kind );
	return m_management6cBinding && m_management6cBinding->activateFirstDataElement( kind );
}
bool MainWindow::focusInventoryRowsForProbe()
{
	if ( m_inventoryWindow && m_inventoryWindow->binding )
		return m_inventoryWindow->binding->focusInventoryRowsForProbe();
	return m_management6bBinding && m_management6bBinding->focusInventoryRowsForProbe();
}
bool MainWindow::dispatchInventoryKeyForProbe( int qtKey )
{
	if ( !m_inventoryWindow || !m_inventoryWindow->nativeWindow ||
		( qtKey != Qt::Key_Down && qtKey != Qt::Key_Space ) )
		return false;
	const QString text = qtKey == Qt::Key_Space ? QStringLiteral( " " ) : QString {};
	QKeyEvent press( QEvent::KeyPress, qtKey, Qt::NoModifier, text );
	const bool pressDelivered = QCoreApplication::sendEvent( m_inventoryWindow->nativeWindow.get(), &press );
	QKeyEvent release( QEvent::KeyRelease, qtKey, Qt::NoModifier, text );
	const bool releaseDelivered = QCoreApplication::sendEvent( m_inventoryWindow->nativeWindow.get(), &release );
	return pressDelivered && releaseDelivered;
}
std::string MainWindow::inventoryWatchStatusForProbe() const
{
	if ( !m_management6bController ) return "unavailable";
	const auto& state = m_management6bController->state();
	if ( !state.selectedInventory ) return "unselected|revision=" + std::to_string( state.inventoryRevision.value );
	const auto& id = *state.selectedInventory;
	const auto row = std::ranges::find_if( state.inventory, [&]( const auto& candidate ) { return candidate.id == id; } );
	if ( row == state.inventory.end() ) return "selected-row-missing|revision=" + std::to_string( state.inventoryRevision.value );
	return id.category.value + "/" + id.group.value + "/" + id.item.value + "/" + id.material.value +
		"|depth=" + std::to_string( static_cast<int>( id.depth ) ) +
		"|watched=" + ( row->watched ? "true" : "false" ) +
		"|revision=" + std::to_string( state.inventoryRevision.value );
}
bool MainWindow::requestInventoryHistoryProbe()
{
	if ( !m_management6bController || !m_management6bBinding || !m_management6bController->state().inventoryOpen )
		return false;
	const auto rows = m_management6bController->visibleInventory();
	auto it = std::ranges::find_if( rows, []( const auto& row )
		{ return row.stockpiled > 0 && ( !row.madeBy.empty() || !row.usedIn.empty() ); } );
	if ( it == rows.end() ) it = std::ranges::find_if( rows, []( const auto& row )
		{ return row.stockpiled > 0 && ( row.id.depth == ingnomia::ui::InventoryDepth::Item || row.id.depth == ingnomia::ui::InventoryDepth::Material ); } );
	if ( it == rows.end() ) it = std::ranges::find_if( rows, []( const auto& row )
		{ return row.id.depth == ingnomia::ui::InventoryDepth::Item || row.id.depth == ingnomia::ui::InventoryDepth::Material; } );
	if ( it == rows.end() )
		return false;
	m_management6bController->selectInventory( it->id );
	m_management6bController->openInventoryDetail( it->id );
	return true;
}
std::string MainWindow::inventoryHistoryStatus() const
{
	if ( !m_management6bController ) return {};
	const auto& state = m_management6bController->state();
	if ( !state.status.empty() )
		return "error:" + state.status;
	return std::to_string( state.inventoryHistory.size() ) + ":" + ( state.inventoryHistoryLoading ? "loading" : "ready" );
}

/// @brief Makes this window's GL context current on the calling thread. Safe to call with
///        a null context.
void MainWindow::makeCurrent()
{

	if ( m_context )

	{

		m_context->makeCurrent( this );
	}
}

/// @brief Releases the GL context from the calling thread.
void MainWindow::doneCurrent()
{

	if ( m_context )

	{

		m_context->doneCurrent();
	}
}

/// @brief Requests application shutdown while keeping the GL surface alive until
/// MainWindow's destructor has released RmlUi resources.
void MainWindow::onExit()
{
	// Calling QWindow::close() here can destroy the native surface before the
	// destructor reaches shutdownRmlUi(), making its owning context impossible
	// to reacquire and turning a normal Save -> Exit into qFatal.  Quitting the
	// event loop first preserves the surface through orderly object teardown.
	QCoreApplication::quit();
}

/// @brief Keeps the native OpenGL surface alive until orderly application teardown.
///
/// A user-driven WM_CLOSE (Alt+F4 or the window manager close action) otherwise
/// destroys the surface before MainWindow::~MainWindow() can reacquire the
/// owning context for RmlUi shutdown. Route it through the same event-loop exit
/// path as the in-game Exit command instead.
void MainWindow::closeEvent( QCloseEvent* event )
{
	event->ignore();
	QCoreApplication::quit();
}

/// @brief Toggles the window between fullscreen and windowed modes and stores the new
///        state in Global::cfg.
void MainWindow::toggleFullScreen()
{

	QWindow* w = this;

	m_isFullScreen = !m_isFullScreen;

	if ( m_isFullScreen )

	{

		w->showFullScreen();

		Global::cfg->set( "fullscreen", true );
	}

	else

	{ // Reset from fullscreen:

		w->showNormal();

		Global::cfg->set( "fullscreen", false );
	}
	m_renderer->onRenderParamsChanged();
	emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );
}

/// @brief Applies an explicit fullscreen state (called when the settings window changes it).

/// @param value True to go fullscreen, false to restore a windowed size from config.

void

MainWindow::onFullScreen( bool value )

{

	QWindow* w = this;

	m_isFullScreen = value;

	Global::cfg->set( "fullscreen", value );

	if ( value )

	{

		w->showFullScreen();
	}

	else

	{ // Reset from fullscreen:

		w->showNormal();
	}
	m_renderer->onRenderParamsChanged();
	emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );
}

/// @brief Qt key-press override: gives RmlUi first refusal on the event, then translates

///        unhandled keys via KeyBindings into camera/zoom/menu/quick-save/build actions

///        and triggers the matching game command.

/// @param event Incoming Qt key event.

void

MainWindow::keyPressEvent( QKeyEvent* event )

{
    if(event->key()==Qt::Key_Escape && event->isAutoRepeat()) { event->accept(); return; }
	// What's This? (PDF p.286-287): any key closes the Help pop-up; Esc cancels the mode; Shift+F1 starts or cancels the
	// mode; F1 explains the control that has the input focus.
	if ( m_rmlUiHost && m_rmlUiHost->context() && ( event->key() == Qt::Key_F1 || event->key() == Qt::Key_Escape || ( m_whatsThis && m_whatsThis->popupOpen() ) ) )
	{
		const auto rmlKey = event->key() == Qt::Key_F1 ? Rml::Input::KI_F1 : event->key() == Qt::Key_Escape ? Rml::Input::KI_ESCAPE : Rml::Input::KI_UNKNOWN;
		const bool wasMode = whatsThis().mode();
		if ( whatsThis().key( *m_rmlUiHost->context(), rmlKey, event->modifiers().testFlag( Qt::ShiftModifier ), false ) )
		{
			if ( whatsThis().mode() ) setCursor( Qt::WhatsThisCursor );
			else if ( wasMode ) unsetCursor();
			event->accept();
			redraw();
			return;
		}
	}
	if ( m_rmlUiHost && event->key() == Qt::Key_F5 && m_rmlUiHost->hotReloadEnabled() )
	{
		m_rmlUiHost->requestDocumentReload();
		event->accept();
		redraw();
		return;
	}
	if ( m_rmlUiHost && event->key() == Qt::Key_F7 && m_rmlUiHost->toggleDebugger() )
	{
		event->accept();
		redraw();
		return;
	}
	if ( m_rmlUiHost && event->key() == Qt::Key_F8 && m_rmlUiHost->hotReloadEnabled() )
	{
		if ( qEnvironmentVariableIsSet( "INGNOMIA_UI_CAPTURE" ) )
		{
			armUiCapture();
			qInfo() << "RmlUi development capture re-armed:" << qEnvironmentVariable( "INGNOMIA_UI_CAPTURE" );
		}
		else qWarning() << "RmlUi development capture needs -CapturePath in tools/run-rmlui-dev.ps1";
		event->accept();
		return;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	if ( m_uiDesigner && event->key() == Qt::Key_F6 )
	{
		toggleUiDesignerSurface( m_uiDesigner.get() );
		event->accept();
		redraw();
		return;
	}
	if ( m_uiDesigner && m_uiDesignerActive && m_uiDesignerActive != m_uiDesigner.get() )
		focusUiDesignerSurface( m_uiDesigner.get() );
	if ( m_uiDesigner && m_uiDesigner->designMode() &&
		m_uiDesigner->keyPress( event->key(), event->modifiers() & Qt::ShiftModifier ) )
	{
		event->accept();
		redraw();
		return;
	}
#endif
    // Pause remains a game shortcut. Space first belongs to focused UI controls.
    if(event->key() == Qt::Key_Pause && m_shellController && m_shellController->state().route.value == "game.hud")
    {
        if(!event->isAutoRepeat()) emit signalTogglePause();
        event->accept(); return;
    }

    if(event->key() == Qt::Key_Escape && m_rmlUiHost) {
        auto* context=m_rmlUiHost->input().context();
        auto* focus=context ? context->GetFocusElement() : nullptr;
        bool reportPopup=false;for(auto*e=focus;e;e=e->GetParentNode())if(e->HasAttribute("data-report-popup"))reportPopup=true;
        if(focus && ((focus->GetOwnerDocument() && focus->GetOwnerDocument()->HasAttribute("data-modal-dialog")) || reportPopup || focus->HasAttribute("data-numeric-editor") || (focus->GetTagName()=="select" && static_cast<Rml::ElementFormControlSelect*>(focus)->IsSelectBoxVisible()))) {
            m_rmlUiHost->input().keyDown(event->key(),event->modifiers(),event->isAutoRepeat());
            event->accept();return;
        }
        m_rmlUiHost->input().cancelInteraction();
    }
	// Escape is a shell command while the game HUD or pause route owns the
	// foreground.  Handle it at the host boundary because a focused RmlUi
	// control can consume an otherwise unhandled key before the legacy switch
	// below sees it.  Higher-priority game layers retain their existing Escape
	// behavior (composition, prompts, inspectors, workbenches, and tools).
	if ( event->key() == Qt::Key_Escape &&
		 !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) ) &&
		 m_shellController )
	{
		const bool prompt = m_hudController && !m_hudController->state().prompts.empty();
		const bool inspector = m_tileInspectionActive || (m_inspectorController && m_inspectorController->state().kind != ingnomia::ui::inspector::InspectorKind::None);
		ingnomia::ui::accessibility::EscapeContext escape { m_uiCompositionActive, prompt, false, inspector,
			m_workbenchCoordinator && m_workbenchCoordinator->state().workbench.has_value(),
			m_hudController && m_hudController->state().tool.active.has_value(), false };
		const auto escapeLayer = ingnomia::ui::accessibility::escapeTarget( escape );
		if ( escapeLayer == ingnomia::ui::accessibility::EscapeLayer::ActiveTool )
		{
			// Match the HUD's "RMB/Esc cancel" contract.  The active placement
			// tool owns Escape before the shell can interpret it as Pause.
			if ( m_hudController ) m_hudController->cancelTool();
			event->accept();
			idleRenderTick();
			return;
		}
		if ( escapeLayer == ingnomia::ui::accessibility::EscapeLayer::Game &&
				m_shellController->handleEscape() )
		{
			event->accept();
			idleRenderTick();
			return;
		}
	}

	int qtKey = event->key();

	bool ret = false;

	if ( rmlUiActive() )

	{

		const auto key = m_rmlUiHost->input().keyDown( qtKey, event->modifiers(), event->isAutoRepeat() );
        ret = key.uiConsumed;

		if ( !key.suppressText && !event->text().isEmpty() && !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) ) )

			ret = m_rmlUiHost->input().committedText( event->text() ).uiConsumed || ret;
	}

    if(m_shellController && m_shellController->state().route.value != "game.hud") ret = true;
    if(!ret && event->key() == Qt::Key_Space && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        if(!event->isAutoRepeat()) emit signalTogglePause();
        ret = true;
    }

	if ( ret )

	{

		event->accept();

		idleRenderTick();
	}

	if ( !ret )

	{

		switch ( event->key() )

		{

			case Qt::Key_H:

				Global::wallsLowered = !Global::wallsLowered;

				emit signalUpdateRenderOptions();

				break;

			case Qt::Key_K:

				break;

			case Qt::Key_O:

				if ( event->modifiers() & Qt::ControlModifier )

				{

					Global::debugMode = !Global::debugMode;
				}

				else

				{

					Global::showDesignations = !Global::showDesignations;

					emit signalUpdateRenderOptions();
				}

				m_renderer->onRenderParamsChanged();

				break;

			case Qt::Key_F: // toggleFullScreen();

				break;

			case Qt::Key_R:

				if ( event->modifiers() & Qt::ControlModifier )

				{

					Global::debugOpenGL = !Global::debugOpenGL;
				}

				else

				{

					emit signalRotateSelection();

					redraw();
				}

				break;

			case Qt::Key_Q:

				m_renderer->rotate( 1 );
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Rotate ) );

				break;

			case Qt::Key_E:

				m_renderer->rotate( -1 );
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Rotate ) );

				break;

			case Qt::Key_Escape:

			{

				const bool prompt = m_hudController && !m_hudController->state().prompts.empty();

				const bool inspector = m_tileInspectionActive || (m_inspectorController && m_inspectorController->state().kind != ingnomia::ui::inspector::InspectorKind::None);

				ingnomia::ui::accessibility::EscapeContext escape { m_uiCompositionActive, prompt, false, inspector, m_workbenchCoordinator && m_workbenchCoordinator->state().workbench.has_value(), m_hudController && m_hudController->state().tool.active.has_value(), false };

				switch ( ingnomia::ui::accessibility::escapeTarget( escape ) )
				{

					case ingnomia::ui::accessibility::EscapeLayer::Composition:
						if ( auto* method = QGuiApplication::inputMethod() )
							method->reset();
						m_uiCompositionActive = false;
						break;

					case ingnomia::ui::accessibility::EscapeLayer::None:
						break;

					case ingnomia::ui::accessibility::EscapeLayer::Workbench:
						if ( m_workbenchCoordinator )
							(void)m_workbenchCoordinator->closeActive();
						break;

					case ingnomia::ui::accessibility::EscapeLayer::Overlay:
						if ( m_tileInspectionActive ) { setTileInspection(false); break; }
						if ( m_inspectorController )
							m_inspectorController->back();
						break;

					case ingnomia::ui::accessibility::EscapeLayer::ActiveTool:
						if ( m_inspectorController && m_inspectorController->state().selection.active )
							m_inspectorController->cancelSelection();
						else
							emit signalKeyPress( event->key() );
						break;

					case ingnomia::ui::accessibility::EscapeLayer::Game:
						if ( !m_shellController || !m_shellController->handleEscape() )
							emit signalKeyPress( event->key() );
						break;

					default:
						emit signalKeyPress( event->key() );
						break;
				}
			}

			break;

			case Qt::Key_Space:

				emit signalTogglePause();

				break;

			case Qt::Key_W:

				keyboardMove();
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Pan ) );

				m_keyboardMove += KeyboardMove::Up;

				redraw();

				break;

			case Qt::Key_S:

				keyboardMove();
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Pan ) );

				m_keyboardMove += KeyboardMove::Down;

				redraw();

				break;

			case Qt::Key_A:

				keyboardMove();
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Pan ) );

				m_keyboardMove += KeyboardMove::Left;

				redraw();

				break;

			case Qt::Key_D:

				keyboardMove();
				Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Pan ) );

				m_keyboardMove += KeyboardMove::Right;

				redraw();

				break;
		}

		emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );

		emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );
	}
}

/// @brief Qt key-release override: clears held-camera bits on the KeyboardMove bitfield and

///        forwards the event to RmlUi.

/// @param event Incoming Qt key event.

void

MainWindow::keyReleaseEvent( QKeyEvent* event )

{

	const bool uiConsumed = rmlUiActive() && m_rmlUiHost->input().keyUp( event->key(), event->modifiers(), event->isAutoRepeat() ).uiConsumed;

	if ( uiConsumed )

	{

		event->accept();

		idleRenderTick();
	}

	switch ( event->key() )

	{

		case Qt::Key_W:

			keyboardMove();

			m_keyboardMove -= KeyboardMove::Up;

			redraw();

			break;

		case Qt::Key_S:

			keyboardMove();

			m_keyboardMove -= KeyboardMove::Down;

			redraw();

			break;

		case Qt::Key_A:

			keyboardMove();

			m_keyboardMove -= KeyboardMove::Left;

			redraw();

			break;

		case Qt::Key_D:

			keyboardMove();

			m_keyboardMove -= KeyboardMove::Right;

			redraw();

			break;
	}

	emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );

	emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );
}

/// @brief Timer slot fired while any camera key is held. Pans the camera by

///        cfg["keyboardMoveSpeed"] pixels × elapsed milliseconds / 1000 in the direction

///        encoded in m_keyboardMove.

void

MainWindow::keyboardMove()

{

	int x = 0;

	int y = 0;

	if ( (bool)( m_keyboardMove & KeyboardMove::Up ) )

		y -= 1;

	if ( (bool)( m_keyboardMove & KeyboardMove::Down ) )

		y += 1;

	if ( (bool)( m_keyboardMove & KeyboardMove::Left ) )

		x -= 1;

	if ( (bool)( m_keyboardMove & KeyboardMove::Right ) )

		x += 1; // Elapsed time in second
	const float elapsedTime = m_keyboardMovementTimer.nsecsElapsed() * 0.000000001f;
	m_keyboardMovementTimer.restart();
	if ( m_renderer && ( x || y ) )
	{

		const float keyboardMoveSpeed = ( Global::cfg->get( "keyboardMoveSpeed" ).toFloat() + 50.f ) * 4.f;

		float moveX = -x * keyboardMoveSpeed * elapsedTime;

		float moveY = -y * keyboardMoveSpeed * elapsedTime;

		if ( x && y )

		{

			moveX /= sqrt( 2 );

			moveY /= sqrt( 2 );
		}

		m_renderer->move( moveX, moveY );

		emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );
	}
}

/// @brief Qt mouse-move override: classifies drags as camera pans when the drag distance

///        exceeds a small threshold, otherwise forwards to RmlUi and the selection

///        aggregator for cursor hover.

/// @param event Incoming Qt mouse event.

void

MainWindow::mouseMoveEvent( QMouseEvent* event )

{

	auto gp = this->mapFromGlobal( event->globalPosition().toPoint() );

	if ( rmlUiActive() )

	{
		if ( m_windowFrame && m_windowFrame->move( event->position(), event->buttons() ) )
		{
			event->accept();
			return;
		}
		if ( m_rmlWindowDragging )
		{
			updateRmlWindowDrag( event->position() );
			event->accept();
			return;
		}

		const auto hover = m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
		if ( m_whatsThis && m_whatsThis->mode() ) setCursor( Qt::WhatsThisCursor );
		// Hover inspection must retain its map target while the player enters
		// any UI surface to read details or choose an action.
		if ( m_tileInspectionActive && (hover.owner == ingnomia::ui::PointerOwner::Ui || hover.uiInteracting) )
		{
			event->accept();
			redraw();
			return;
		}

		if ( event->buttons() & Qt::LeftButton && m_leftDown && m_rmlUiHost->input().pointerOwner( Qt::LeftButton ) == ingnomia::ui::PointerOwner::World )

		{

			if ( ( abs( gp.x() - m_clickX ) > 5 || abs( gp.y() - m_clickY ) > 5 ) && !m_isMove )

			{

				m_isMove = true;

				m_moveX = m_clickX;

				m_moveY = m_clickY;
			}

			if ( m_isMove && !m_draggingAreaSelection )

			{

				m_renderer->move( gp.x() - m_moveX, gp.y() - m_moveY );
				if( Global::eventConnector ) Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Pan ) );

				m_moveX = gp.x();

				m_moveY = gp.y();
			}

			if ( !m_draggingAreaSelection )
				emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );
		}
	}

	m_mouseX = gp.x();

	m_mouseY = gp.y();

	if ( m_inspectorController )
		m_inspectorController->setSelectionPointer( ingnomia::ui::inspector::PointerPosition { m_mouseX, m_mouseY } );

	emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );

	redraw();
}

/// @brief Triggers renderer initialisation once a game has finished loading so the

///        world view is ready for the first paint. The renderer persists across games,

///        so we use setMove (absolute) rather than move (delta) to avoid compounding

///        the saved offset with whatever was left over from the previous game.

void

MainWindow::onInitViewAfterLoad()

{
	const bool hasInitialCameraTarget = !GameState::initialCameraTarget.isZero();
	const Position initialCameraTarget = GameState::initialCameraTarget;

	qInfo() << ( hasInitialCameraTarget ? "RmlUi generated-view restore" : "RmlUi saved-view restore" )
		<< GameState::moveX << GameState::moveY << GameState::scale << GameState::viewLevel;

	m_moveX = GameState::moveX;

	m_moveY = GameState::moveY;

	m_renderer->setMove( m_moveX, m_moveY );

	m_renderer->setScale( GameState::scale );
	if ( hasInitialCameraTarget )
	{
		// New worlds publish the actual embark tile. Apply it after the saved-view
		// values above so the renderer opens on the settlement instead of (0, 0).
		m_renderer->onCenterCameraPosition( initialCameraTarget );
		GameState::initialCameraTarget = Position();
	}
	if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_CENTER_VIEW" ) == "1" )
	{
		m_renderer->setMove( 0.0f, 0.0f );
		m_renderer->setScale( 1.0f );
	}
	bool automatedLevelOk = false;
	const int automatedLevel = qEnvironmentVariable( "INGNOMIA_AUTOMATE_VIEW_LEVEL" ).toInt( &automatedLevelOk );
	if ( automatedLevelOk )
		onUiSetViewLevel( automatedLevel );

	pushRenderParams();
	if ( qEnvironmentVariable( "INGNOMIA_UI_CAPTURE_AFTER_LOAD" ) == "1" )
	{
		// Let queued renderer/aggregator tile uploads settle before taking the
		// diagnostic frame; an immediate capture is indistinguishable from a
		// black world while the first SSBO/atlas update is still in flight.
		m_uiCaptureDone = true;
		bool delayOk = false;
		const int captureDelayMs = std::clamp( qEnvironmentVariable( "INGNOMIA_UI_CAPTURE_DELAY_MS", "1500" ).toInt( &delayOk ), 250, 30000 );
		QTimer::singleShot( delayOk ? captureDelayMs : 1500, this, [this]() { m_uiCaptureDone = false; } );
	}
}

void

MainWindow::pushRenderParams()

{

	emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );
}

/// @brief Qt mouse-press override: records click position, tracks button state, and

///        forwards to RmlUi. If the click isn't over a GUI element, eventually dispatches

///        to the selection aggregator on release.

/// @param event Incoming Qt mouse event.

void

MainWindow::mousePressEvent( QMouseEvent* event )

{ // qDebug() << "mousePressEvent";
	auto gp = this->mapFromGlobal( event->globalPosition().toPoint() );
	if ( !rmlUiActive() )
		return;
	m_mouseX = gp.x();
	m_mouseY = gp.y();
	if ( m_inspectorController )
		m_inspectorController->setSelectionPointer( ingnomia::ui::inspector::PointerPosition { m_mouseX, m_mouseY } );
#if defined( INGNOMIA_UI_DESIGNER )
	if ( m_uiDesigner && m_uiDesignerActive && m_uiDesignerActive != m_uiDesigner.get() )
		focusUiDesignerSurface( m_uiDesigner.get() );
	if ( m_uiDesigner && m_uiDesigner->designMode() && event->button() == Qt::LeftButton &&
		!m_uiDesigner->editorConsumesPoint( event->position() * devicePixelRatio() ) )
	{
		(void)m_uiDesigner->selectAt( event->position() * devicePixelRatio() );
		event->accept();
		redraw();
		return;
	}
#endif
	if ( event->button() == Qt::LeftButton && m_windowFrame )
	{
		m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
		if ( m_windowFrame->press( m_rmlUiHost->context()->GetHoverElement(), event->position() ) )
		{
			event->accept();
			redraw();
			return;
		}
	}
	if ( event->button() == Qt::RightButton && m_windowFrame )
	{
		m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
		const auto at = event->position() * devicePixelRatio();
		if ( m_windowFrame->contextPress( m_rmlUiHost->context()->GetHoverElement(), Rml::Vector2f( float( at.x() ), float( at.y() ) ) ) )
		{
			event->accept();
			redraw();
			return;
		}
	}
	// What's This? (PDF p.285-287): in the mode the next click explains the item (or cancels the mode); the secondary
	// button on a control with Help offers the What's This? shortcut menu. Choosing the toolbar button again cancels.
	if ( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
	{
		m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
		auto* hover = m_rmlUiHost->context()->GetHoverElement();
		bool onButton = false;
		for ( auto* e = hover; e; e = e->GetParentNode() )
			if ( e->GetId() == "hud_whats_this" ) onButton = true;
		const auto at = event->position() * devicePixelRatio();
		const bool wasMode = whatsThis().mode();
		bool taken = false;
		if ( onButton && wasMode && event->button() == Qt::LeftButton )
		{
			whatsThis().cancel();
			taken = true;
		}
		else if ( !( onButton && event->button() == Qt::LeftButton ) )
			taken = whatsThis().press( hover, Rml::Vector2f( float( at.x() ), float( at.y() ) ), event->button() == Qt::RightButton );
		if ( taken )
		{
			if ( whatsThis().mode() ) setCursor( Qt::WhatsThisCursor );
			else if ( wasMode ) unsetCursor();
			event->accept();
			redraw();
			return;
		}
	}
	if ( event->button() == Qt::LeftButton && beginRmlWindowDrag( event->position() ) )
	{
		event->accept();
		redraw();
		return;
	}
	m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
	const auto dispatch = m_rmlUiHost->input().mouseButtonDown( event->button(), event->modifiers() );
	if ( dispatch.owner == ingnomia::ui::PointerOwner::Ui )
	{

		event->accept();

		redraw();

		return;
	}
	if ( dispatch.owner != ingnomia::ui::PointerOwner::World )
		return;
	if ( event->button() == Qt::LeftButton )
	{

		m_clickX = gp.x();

		m_clickY = gp.y();

		m_isMove = false;

		m_leftDown = true;
		m_draggingAreaSelection = m_hudController && m_hudController->state().tool.active
			&& ( m_hudController->state().tool.active->value == "fell_tree"
				|| m_hudController->state().tool.active->value == "create_stockpile" );
		if ( m_draggingAreaSelection )
		{
			emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );
			emit signalLeftClick( event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );
		}
	}
	else if ( event->button() == Qt::RightButton )
	{

		m_rightDown = true;
	}
}

/// @brief What's This? for the game window (PDF p.285-287), created on first use.
ingnomia::ui::whats_this::Controller& MainWindow::whatsThis()
{
	if ( !m_whatsThis ) m_whatsThis = std::make_unique<ingnomia::ui::whats_this::Controller>();
	return *m_whatsThis;
}

/// @brief Native message hook: Alt+Space opens the window menu drawn by the frame instead of the system's own menu
///        (PDF p.113).
bool MainWindow::nativeEvent( const QByteArray& eventType, void* message, qintptr* result )
{
	using ingnomia::ui::native_window::AltSpace;
	const auto altSpace = ingnomia::ui::native_window::altSpace( eventType, message );
	if ( altSpace == AltSpace::None || !m_windowFrame ) return QWindow::nativeEvent( eventType, message, result );
	if ( altSpace == AltSpace::Open && m_windowFrame->openMenuFromKeyboard() ) redraw();
	if ( result ) *result = 0;
	return true;
}

/// @brief Qt double-click override: double-clicking the frame caption maximizes or restores the window;
///        Qt has already delivered the second press, so nothing else is done here.
void MainWindow::mouseDoubleClickEvent( QMouseEvent* event )
{
	if ( rmlUiActive() && m_windowFrame && event->button() == Qt::LeftButton )
	{
		m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
		if ( m_windowFrame->doubleClick( m_rmlUiHost->context()->GetHoverElement() ) )
		{
			event->accept();
			return;
		}
	}
	QWindow::mouseDoubleClickEvent( event );
}


/// @brief Qt mouse-release override: commits a click to the selection aggregator if the

///        button was pressed and not classified as a drag. Forwards to RmlUi otherwise.

/// @param event Incoming Qt mouse event.

void

MainWindow::mouseReleaseEvent( QMouseEvent* event )

{ // qDebug() << "mouseReleaseEvent";
	if ( !rmlUiActive() )
		return;
	const auto gp = this->mapFromGlobal( event->globalPosition().toPoint() );
	m_mouseX      = gp.x();
	m_mouseY      = gp.y();
	if ( event->button() == Qt::LeftButton && m_windowFrame && m_windowFrame->release() )
	{
		event->accept();
		return;
	}
	if ( m_rmlWindowDragging && event->button() == Qt::LeftButton )
	{
		endRmlWindowDrag();
		event->accept();
		redraw();
		return;
	}
	if ( m_inspectorController )
		m_inspectorController->setSelectionPointer( ingnomia::ui::inspector::PointerPosition { m_mouseX, m_mouseY } );
	m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
	const auto dispatch = m_rmlUiHost->input().mouseButtonUp( event->button(), event->modifiers() );
	if ( m_whatsThis && m_whatsThis->mode() ) setCursor( Qt::WhatsThisCursor );
	if ( dispatch.owner == ingnomia::ui::PointerOwner::Ui )
	{

		event->accept();

		m_isMove = false;

		m_leftDown = false;
		m_draggingAreaSelection = false;

		m_rightDown = false;

		redraw();

		return;
	}
	if ( dispatch.owner != ingnomia::ui::PointerOwner::World )
		return;
	if ( event->button() == Qt::LeftButton )
	{

		if ( m_leftDown && ( m_draggingAreaSelection ? m_isMove : !m_isMove ) )

		{

			emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );

			emit signalLeftClick( event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );
		}

		m_isMove = false;

		m_leftDown = false;
		m_draggingAreaSelection = false;
	}
	else if ( event->button() == Qt::RightButton )
	{

		if ( m_rightDown )

		{

			emit signalMouse( m_mouseX, m_mouseY, event->modifiers() & Qt::ShiftModifier, event->modifiers() & Qt::ControlModifier );

			emit signalRightClick();
		}

		m_rightDown = false;
	}
	redraw();
}

/// @brief Qt mouse-wheel override: either cycles z-level or zooms the camera based on the

///        toggleMouseWheel config flag, or scrolls a GUI element if RmlUi claims the event.

/// @param event Incoming Qt wheel event.

void

MainWindow::wheelEvent( QWheelEvent* event )

{

	if ( !rmlUiActive() )

		return;

	m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
	if ( m_tileInspectionActive && m_inspectorBinding )
	{
		const qreal dpr = qMax<qreal>( 0.01, devicePixelRatio() );
		const float step = event->pixelDelta().isNull()
			? event->angleDelta().y() / 120.f * 60.f
			: static_cast<float>( event->pixelDelta().y() / dpr );
		if ( m_inspectorBinding->scrollLiveTile( qRound( event->position().x() * dpr ), qRound( event->position().y() * dpr ), step ) )
		{
			event->accept();
			redraw();
			return;
		}
	}

	if ( m_rmlUiHost->input().mouseWheel( event->angleDelta(), event->pixelDelta(), event->modifiers() ).uiConsumed )

	{

		event->accept();

		redraw();

		return;
	}

	const int delta = event->angleDelta().y();

	if ( delta == 0 )

		return;

	if ( (bool)( event->modifiers() & Qt::ControlModifier ) ^ Global::cfg->get( "toggleMouseWheel" ).toBool() )
	{
		m_renderer->scale( pow( 1.002, -delta ) );
		Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::Zoom ) );
	}
	else if ( delta > 0 )
	{
		keyboardZPlus( event->modifiers() & Qt::ShiftModifier );
			Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::WheelLevel ) );
	}
	else
	{
		keyboardZMinus( event->modifiers() & Qt::ShiftModifier );
			Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::WheelLevel ) );
	}

	emit signalRenderParams( width(), height(), m_renderer->moveX(), m_renderer->moveY(), m_renderer->scale(), m_renderer->rotation() );

	redraw();
}

/// @brief Qt focus-in override: clears any held camera keys so leaving and re-entering the

///        window doesn't leave the camera panning indefinitely.

/// @param e Focus event.

void

MainWindow::focusInEvent( QFocusEvent* e )

{

	QWindow::focusInEvent( e );

	redraw();
}

/// @brief Qt focus-out override: same job as focusInEvent, clears any stuck camera keys.

/// @param e Focus event.

void

MainWindow::focusOutEvent( QFocusEvent* e )

{

	if ( rmlUiActive() )

		m_rmlUiHost->input().cancelInteraction();

	m_keyboardMove = KeyboardMove::None;

	m_leftDown = false;

	m_rightDown = false;

	m_isMove = false;
	endRmlWindowDrag();

	QWindow::focusOutEvent( e );

	redraw();
}

/// @brief Shows the next layer without panning the canvas.

/// @param shift True if Shift is held.

/// @param ctrl  True if Ctrl is held.

void

MainWindow::keyboardZPlus( bool shift, bool ctrl )

{

	onUiSetViewLevel( GameState::viewLevel + 1 );

	emit signalMouse( m_mouseX, m_mouseY, shift, ctrl );

	redraw();
}

/// @brief Shows the previous layer without panning the canvas.

/// @param shift True if Shift is held.

/// @param ctrl  True if Ctrl is held.

void

MainWindow::keyboardZMinus( bool shift, bool ctrl )

{

	onUiSetViewLevel( GameState::viewLevel - 1 );

	emit signalMouse( m_mouseX, m_mouseY, shift, ctrl );

	redraw();
}

void MainWindow::refreshContinueSave()
{
	if( !m_shellController ) return;
	if( const auto save = IO::newestCompatibleSave() )
	{
		m_shellController->setContinueAvailability( true, std::nullopt,
			save->kingdomName.toStdString(), save->modified.toLocalTime().toString( "yyyy-MM-dd h:mm AP" ).toStdString() );
	}
	else
	{
		m_shellController->setContinueAvailability( false,
			ingnomia::ui::shell::Message{ ingnomia::ui::LocalizationKey{ "ui.shell.continue_unverified" }, {} } );
	}
}

bool

MainWindow::initializeRmlUi()

{

	if ( !m_context || QOpenGLContext::currentContext() != m_context )

	{

		qCritical() << "RmlUi MainWindow initialization requires its Qt OpenGL context to be current";

		return false;
	}

	m_rmlUiHost = std::make_unique<ingnomia::ui::RmlUiHost>();

	ingnomia::ui::RmlUiHost::Config config;

	config.window = this;

	const QString packagedRmlUiRoot = Global::cfg->get( "dataPath" ).toString() + "/rmlui";
	const QString sourceRmlUiRoot = qEnvironmentVariable( "INGNOMIA_RMLUI_ASSET_ROOT" );
	config.assetRoot = sourceRmlUiRoot.isEmpty() ? packagedRmlUiRoot : QFileInfo( sourceRmlUiRoot ).absoluteFilePath();
	if ( !sourceRmlUiRoot.isEmpty() ) config.fallbackAssetRoot = packagedRmlUiRoot;
	config.enableHotReload = qEnvironmentVariableIntValue( "INGNOMIA_UI_DEV_MODE" ) == 1 || !sourceRmlUiRoot.isEmpty();
	if ( config.enableHotReload && sourceRmlUiRoot.isEmpty() )
		qWarning() << "RmlUi development mode is watching staged content. Set INGNOMIA_RMLUI_ASSET_ROOT to the source content/rmlui directory.";
	else if ( config.enableHotReload )
		qInfo() << "RmlUi development mode is watching source assets:" << config.assetRoot;

	config.fontFiles = { "fonts/LatoLatin-Regular.ttf", "fonts/MSW98UI-Regular.ttf", "fonts/MSW98UI-Bold.ttf" };

	config.contextName = "ingnomia-main-window";

	config.physicalSize = QSize( qMax( 1, qRound( width() * devicePixelRatio() ) ), qMax( 1, qRound( height() * devicePixelRatio() ) ) );

	config.densityIndependentPixelRatio = static_cast<float>( devicePixelRatio() ) * qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
#if defined( INGNOMIA_RMLUI_DEBUGGER )
	config.enableDebugger = true;
#endif

	if ( !m_rmlUiHost->initialize( config ) )

	{

		m_rmlUiHost.reset();

		return false;
	}
	m_rmlUiHost->setCameraPreviewTexture( m_renderer->cameraPreviewTexture(), m_renderer->cameraPreviewWidth(), m_renderer->cameraPreviewHeight() );

	auto* shellBinding = m_rmlUiHost->createShellBinding();

	if ( !shellBinding )

	{

		m_rmlUiHost->shutdown();

		m_rmlUiHost.reset();

		return false;
	}
	m_shellBinding = shellBinding;

	m_shellCommands = std::make_unique<ingnomia::ui::shell::ShellQtCommandPort>( Global::eventConnector );

	m_shellController = std::make_unique<ingnomia::ui::shell::ShellController>( *m_shellCommands, *shellBinding );

	if ( !shellBinding->initialize( *m_shellController ) )

	{

		m_shellController.reset();

		m_shellCommands.reset();

		m_rmlUiHost->shutdown();

		m_rmlUiHost.reset();

		return false;
	}

	auto* hudBinding = m_rmlUiHost->createHudBinding();

	if ( !hudBinding )

		return false;
	m_hudBinding = hudBinding;

	m_hudCommands = std::make_unique<ingnomia::ui::hud::HudQtCommandPort>( Global::eventConnector, this );

	m_hudController = std::make_unique<ingnomia::ui::hud::HudController>( *m_hudCommands, *hudBinding );

	if ( !hudBinding->initialize( *m_hudController ) )

		return false;
	hudBinding->setPauseHandler( [this]()
			{
				if ( m_shellController ) m_shellController->activate( ingnomia::ui::shell::ShellControl::TogglePause );
			} );
	hudBinding->setOrdersToolsHandler( [this]( std::string_view id )
		{
			if ( !openDetachedOrdersTools( id ) )
			{
				qWarning() << "Could not open detached Orders & tools window";
				return;
			}
		} );

	auto* inspectorBinding = m_rmlUiHost->createInspectorBinding();

	if ( !inspectorBinding )

		return false;
	m_inspectorBinding = inspectorBinding;
	hudBinding->setInspectionHandler([this]{setTileInspection(!m_tileInspectionActive);});
	hudBinding->setToolCursorHandler( [this]( bool armed ) { if ( m_rmlUiHost ) m_rmlUiHost->setMapCursor( armed ? Qt::CrossCursor : Qt::ArrowCursor ); } );
	// The What's This? toolbar button starts the mode, or cancels it when chosen again (PDF p.286).
	hudBinding->setWhatsThisHandler( [this] {
		whatsThis().toggleMode();
		if ( whatsThis().mode() ) setCursor( Qt::WhatsThisCursor ); else unsetCursor();
		redraw();
	} );
	// High Contrast is read at start-up and then checked every second, so a change in Windows applies without a
	// restart (PDF p.373-374). INGNOMIA_AUTOMATE_HIGH_CONTRAST=1 forces it on for tests.
	{
		const auto readHighContrast = []
		{
			if ( qEnvironmentVariable( "INGNOMIA_AUTOMATE_HIGH_CONTRAST" ) == "1" ) return true;
#if defined( Q_OS_WIN )
			HIGHCONTRASTW contrast{};
			contrast.cbSize = sizeof( contrast );
			return SystemParametersInfoW( SPI_GETHIGHCONTRAST, sizeof( contrast ), &contrast, 0 ) && ( contrast.dwFlags & HCF_HIGHCONTRASTON ) != 0;
#else
			return false;
#endif
		};
		if ( m_rmlUiHost ) m_rmlUiHost->setHighContrast( readHighContrast() );
		auto* contrastTimer = new QTimer( this );
		connect( contrastTimer, &QTimer::timeout, this, [this, readHighContrast] { if ( m_rmlUiHost ) m_rmlUiHost->setHighContrast( readHighContrast() ); } );
		contrastTimer->start( 1000 );
	}
	inspectorBinding->setCloseHandler([this]{setTileInspection(false);});
	inspectorBinding->setExpertiseOpenedHandler([]{ Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::InspectGnome ) ); });
	inspectorBinding->setReplaceFloorHandler([this]{ QTimer::singleShot(0,this,[this]{setTileInspection(false);(void)openDetachedOrdersTools("hud_build_floor");}); });
	connect(Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalInspectionChanged, this, [this](bool active){if(active!=m_tileInspectionActive)setTileInspection(active);}, Qt::QueuedConnection);

	m_inspectorCommands = std::make_unique<ingnomia::ui::inspector::InspectorQtCommandPort>( Global::eventConnector );

	m_inspectorController = std::make_unique<ingnomia::ui::inspector::InspectorController>( *m_inspectorCommands, *inspectorBinding );

	if ( !inspectorBinding->initialize( *m_inspectorController ) )

		return false;

	auto* management6bBinding = m_rmlUiHost->createManagement6BBinding();

	if ( !management6bBinding )

		return false;
	m_management6bBinding = management6bBinding;

	m_management6bCommands = std::make_unique<ingnomia::ui::management6b::Management6BQtCommandPort>( Global::eventConnector );

	m_management6bController = std::make_unique<ingnomia::ui::management6b::Management6BController>( *m_management6bCommands, *management6bBinding );

	m_management6bData = std::make_unique<ingnomia::ui::management6b::Management6BQtDataAdapter>();

	if ( !management6bBinding->initialize( *m_management6bController ) )

		return false;
	management6bBinding->setPresentationEnabled( false );
	management6bBinding->stateChanged( m_management6bController->state() );

	m_management6a = std::make_unique<ingnomia::ui::management6a::Management6AIntegration>( Global::eventConnector, *m_rmlUiHost->context(), this );

	if ( !m_management6a->initialize() )

		return false;
	if ( auto* management6aBinding = m_management6a->binding() )
	{
		management6aBinding->setPresentationEnabled( false );
		management6aBinding->stateChanged( m_management6a->controller()->state() );
	}
	m_management6a->setViewHandler( [this]( ingnomia::ui::management6a::ManagementView view )
		{
            if (m_inspectorController && !m_tileInspectionActive) m_inspectorController->close();
			if ( !ensureDetachedManagement6A(static_cast<int>(view)) )
				qWarning() << "Could not open detached production management window";
		} );

	m_management6cCommands = std::make_unique<ingnomia::ui::management6c::Management6CQtCommandPort>( Global::eventConnector );

	m_management6cBinding = std::make_unique<ingnomia::ui::management6c::Management6CRmlBinding>( *m_rmlUiHost->context() );

	m_management6cController = std::make_unique<ingnomia::ui::management6c::Management6CController>( *m_management6cCommands, *m_management6cBinding );

	m_management6cBridge = std::make_unique<ingnomia::ui::management6c::Management6CQtBridge>( this );

	if ( !m_management6cBinding->initialize( *m_management6cController ) || !m_management6cBridge->attach( Global::eventConnector, *m_management6cController ) )

		return false;
	m_management6cBinding->setPresentationEnabled( false );
	m_management6cBinding->stateChanged( m_management6cController->state() );

	m_management6cBinding->setRouteCloseHandler( []( ingnomia::ui::RouteId, ingnomia::ui::FocusToken ) {} );

	using namespace ingnomia::ui;

	m_workbenchCoordinator = std::make_unique<navigation::WorkbenchCoordinator>( navigation::WorkbenchPorts { [this, management6bBinding]( FocusToken focus )

																											  { if ( m_inspectorController ) m_inspectorController->close(); return ensureDetachedManagement6B() && m_management6bWindow && m_management6bWindow->binding && m_management6bWindow->binding->openPopulation( focus ); }, [this, management6bBinding]( FocusToken focus )

																											  { return ensureDetachedManagement6B( true ) && m_inventoryWindow && m_inventoryWindow->binding && m_inventoryWindow->binding->openInventory( focus ); }, [this]( FocusToken focus )

																											  { return ensureDetachedManagement6C() && m_management6cWindow && m_management6cWindow->binding && m_management6cWindow->binding->openMilitary( management6c::View::Squads, focus ); }, [this]( FocusToken focus )

																								  { return ensureDetachedManagement6C(true) && m_diplomacyWindow && m_diplomacyWindow->binding && m_diplomacyWindow->binding->openDiplomacy( management6c::View::Missions, focus ); }, [this, management6bBinding]

																								  { if ( m_management6bWindow && m_management6bWindow->binding ) m_management6bWindow->binding->closeRoute(); else management6bBinding->closeRoute(); }, [this]

																								  { if ( m_management6cWindow && m_management6cWindow->binding ) m_management6cWindow->binding->closeRoute(); else m_management6cBinding->closeRoute(); }, [this, management6bBinding]

																								  { if ( m_management6bWindow && m_management6bWindow->binding ) m_management6bWindow->binding->closePopulation(); else management6bBinding->closePopulation(); }, [this, management6bBinding]

																								  { if ( m_inventoryWindow && m_inventoryWindow->binding ) m_inventoryWindow->binding->closeInventory(); else management6bBinding->closeInventory(); }, [this]

																								  { if ( m_management6cWindow && m_management6cWindow->binding ) m_management6cWindow->binding->closeMilitary(); else m_management6cBinding->closeMilitary(); }, [this]

																								  { if ( m_diplomacyWindow && m_diplomacyWindow->binding ) m_diplomacyWindow->binding->closeDiplomacy(); else m_management6cBinding->closeDiplomacy(); }, [hudBinding]( FocusToken focus )

																											  { hudBinding->restoreWorkbenchFocus( focus ); } } );

	management6bBinding->setRouteCloseHandler( [this]( RouteId route, FocusToken focus )

											   { hideDetachedManagement6B(); if(m_workbenchCoordinator)(void)m_workbenchCoordinator->close(route,focus); } );
	management6bBinding->setStockpileOpenHandler( []( unsigned int id )
		{ if ( auto* stockpile = Global::eventConnector->aggregatorStockpile() )
			QMetaObject::invokeMethod( stockpile, [stockpile, id] { stockpile->onOpenStockpileInfo( id ); }, Qt::QueuedConnection ); } );
	management6bBinding->setCitizenSelectedHandler([]{ Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::OpenPopulation ) ); });

	m_management6cBinding->setRouteCloseHandler( [this]( RouteId route, FocusToken focus )

												 { hideDetachedManagement6C(); if(m_workbenchCoordinator)(void)m_workbenchCoordinator->close(route,focus); } );

	hudBinding->setWorkbenchHandlers( [this]( FocusToken focus )

										  {if(m_workbenchCoordinator)(void)m_workbenchCoordinator->open(navigation::Workbench::Population,focus); }, [this]( FocusToken focus )

										  {if(m_workbenchCoordinator)(void)m_workbenchCoordinator->open(navigation::Workbench::Inventory,focus); }, [this]( FocusToken focus )

									  {if(m_workbenchCoordinator)(void)m_workbenchCoordinator->open(navigation::Workbench::Military,focus); }, [this]( FocusToken focus )

									  {if(m_workbenchCoordinator)(void)m_workbenchCoordinator->open(navigation::Workbench::Diplomacy,focus); } );
#if defined( INGNOMIA_DEVELOPER_UI )
	auto* debugBinding = m_rmlUiHost->createDebugBinding();
	if ( !debugBinding )
		return false;
	m_debugBinding = debugBinding;
	m_debugCommands   = std::make_unique<ingnomia::ui::debug::DebugQtCommandPort>( Global::eventConnector );
	m_debugController = std::make_unique<ingnomia::ui::debug::DebugController>( true, *m_debugCommands, *debugBinding );
	m_debugData       = std::make_unique<ingnomia::ui::debug::DebugQtDataAdapter>();
	if ( !debugBinding->initialize( *m_debugController ) )
		return false;
#endif

	connect( Global::eventConnector, &EventConnector::signalWorldTransitionStarted, this, [this]( bool generating )
		{
			if ( m_shellController ) m_shellController->beginWorldTransition( generating );
		}, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalWorldTransitionProgress, this, [this]( const QString& progress )
		{
			if ( m_shellController ) m_shellController->setLifecycleProgress( progress.toStdString() );
		}, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalWorldTransitionFinished, this, [this]( bool success )
		{
			if ( m_shellController ) m_shellController->finishWorldTransition( success );
		}, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalInMenu, this, [this]( bool inMenu )

	 {
			if ( m_shellBinding ) m_shellBinding->setInMenu( inMenu );
			if ( inMenu && m_shellController ) { refreshContinueSave(); m_shellController->endWorld(); }
			if ( inMenu ) setTitle( QStringLiteral( "Ingnomia" ) );

if( inMenu ) {

hideDetachedManagementWindows();


if(m_workbenchCoordinator)m_workbenchCoordinator->leaveGame();


if(m_hudController)m_hudController->endWorld();


if(m_inspectorCommands)m_inspectorCommands->setWorld({},false);


setTileInspection(false);
if(m_inspectorController)m_inspectorController->endWorld();
for(auto& window:m_creatureInspectorWindows){if(window.commands)window.commands->setWorld({},false);if(window.controller)window.controller->endWorld();}


if(m_management6bCommands)m_management6bCommands->setWorld({},false);


if(m_management6bData)m_management6bData->setWorld({});


if(m_management6bController)m_management6bController->endWorld();


if(m_management6a)m_management6a->endWorld();


if(m_management6cCommands)m_management6cCommands->setWorld({},false);


if(m_management6cBridge)m_management6cBridge->endWorld();
#if defined( INGNOMIA_DEVELOPER_UI )
if ( m_debugController ) m_debugController->endWorld(); if ( m_debugData )
m_debugData->setWorld( {} );
#endif


}

	else {


const ingnomia::ui::WorldEpoch epoch{++m_uiWorldEpoch};

m_creatureProfessionChoices.clear();


if(m_workbenchCoordinator)(void)m_workbenchCoordinator->enterGame();


if(m_hudController)
{
	m_hudController->beginWorld(epoch);
	// The saved view level is not announced when a world starts; show it at once so the Level pane and the
	// Level Up / Level Down buttons are right from the first frame (Stage 17).
	auto camera=m_hudController->state().camera;
	const int configuredDimZ=Global::cfg->get("dimensionZ").toInt();
	const int dimZ=Global::dimZ>0?Global::dimZ:configuredDimZ;
	camera.viewLevel=GameState::viewLevel;camera.minLevel=0;camera.maxLevel=qMax(0,dimZ-1);
	m_hudController->setCamera(camera);
}


if(m_inspectorCommands)m_inspectorCommands->setWorld(epoch,true);


if(m_inspectorController)m_inspectorController->beginWorld(epoch);
for(auto& window:m_creatureInspectorWindows){if(window.commands)window.commands->setWorld(epoch,true);if(window.controller)window.controller->beginWorld(epoch);}


if(m_management6bCommands)m_management6bCommands->setWorld(epoch,true);


if(m_management6bData)m_management6bData->setWorld(epoch);


if(m_management6bController)m_management6bController->beginWorld(epoch);


if(m_management6a)m_management6a->beginWorld(epoch);


if(m_management6cCommands)m_management6cCommands->setWorld(epoch,true);


if(m_management6cBridge)m_management6cBridge->beginWorld(epoch);
#if defined( INGNOMIA_DEVELOPER_UI )
if ( m_debugData ) m_debugData->setWorld( epoch ); if ( m_debugController )
m_debugController->beginWorld( epoch );
#endif


} }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorTileInfo(), &AggregatorTileInfo::signalUpdateLiveTileInfo, this, [this]( const GuiTileInfo& value )
	{
		if ( !m_tileInspectionActive ) return;
		for ( auto& window : m_creatureInspectorWindows )
			if ( window.slot == -2 && window.controller ) window.controller->showTile( ingnomia::ui::inspector::InspectorQtDataAdapter::tile(value) );
	}, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorTileInfo(), &AggregatorTileInfo::signalUpdateTileInfo, this, [this]( const GuiTileInfo& value )

			 {
				 const auto tile = ingnomia::ui::inspector::InspectorQtDataAdapter::tile( value );
				 const bool blueprint = tile.hasJob && tile.jobName.rfind( "Build", 0 ) == 0;
				 if ( blueprint && !m_tileInspectionActive )
				 {
					 if ( m_inspectorController )
					 {
						 m_inspectorController->endWorld();
						 m_inspectorController->beginWorld( ingnomia::ui::WorldEpoch{ m_uiWorldEpoch ? m_uiWorldEpoch : 1 } );
					 }
					 (void)openDetachedBlueprintInspector( tile );
				 }
				 else if ( m_tileInspectionActive )
					 for ( auto& window : m_creatureInspectorWindows )
						 if ( window.slot == -2 && window.controller ) window.controller->showTile( tile );
			 }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorCreatureInfo(), &AggregatorCreatureInfo::signalCreatureUpdate, this, [this]( const GuiCreatureInfo& value )

			 {
				 // While Population is open, creature updates feed its roster. Properties on a citizen (the
				 // Creature view) opens the one creature inspector instead of a second detail page (Stage 16).
				 if(m_management6bController&&m_management6bController->state().populationOpen)
				 {
					 const auto& population=m_management6bController->state();
					 const bool properties=population.populationView==ingnomia::ui::management6b::View::Creature
						 &&population.selectedCreature&&population.selectedCreature->value==value.id;
					 const bool inspected=std::ranges::any_of(m_creatureInspectorWindows,[&value](const CreatureInspectorWindow& window)
						 {return window.controller&&window.controller->state().creature&&window.controller->state().creature->id.value==value.id;});
					 if(!properties&&!inspected)return;
				 }
				 traceInspectorSkills( QStringLiteral( "route payload id=%1 name=%2 rows=%3" )
					.arg( static_cast<qulonglong>( value.id ) ).arg( value.name ).arg( value.skills.size() ) );
				 const auto creature=ingnomia::ui::inspector::InspectorQtDataAdapter::creature(value);
				 const auto position=ingnomia::ui::inspector::InspectorQtDataAdapter::position(value.position);
				 for(auto& window:m_creatureInspectorWindows)
				 {
					 if(window.controller&&window.controller->state().creature&&window.controller->state().creature->id==creature.id)
					 {
						const auto& before = *window.controller->state().creature;
						traceInspectorSkills( QStringLiteral( "route match slot=%1 id=%2 name=%3 beforeRows=%4 beforeSkillsReported=%5" )
							.arg( window.slot )
							.arg( static_cast<qulonglong>( before.id.value ) )
							.arg( QString::fromStdString( before.name ) )
							.arg( before.skills.size() )
							.arg( before.skillsReported ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ) );
						 window.controller->showCreature(creature,position);
						const auto& after = *window.controller->state().creature;
						traceInspectorSkills( QStringLiteral( "route applied slot=%1 id=%2 name=%3 afterRows=%4 afterSkillsReported=%5" )
							.arg( window.slot )
							.arg( static_cast<qulonglong>( after.id.value ) )
							.arg( QString::fromStdString( after.name ) )
							.arg( after.skills.size() )
							.arg( after.skillsReported ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ) );
						 return;
					 }
				 }
				 traceInspectorSkills( QStringLiteral( "route no-match id=%1 name=%2 existingWindows=%3" )
					.arg( static_cast<qulonglong>( value.id ) ).arg( value.name ).arg( m_creatureInspectorWindows.size() ) );
					 // Creature inspectors now live in their own native windows. Clear any
					 // stale in-canvas inspector state before opening one; leaving a tile or
					 // workshop document mounted in the primary context lets its layout
					 // compete with the HUD when a real creature selection arrives.
					 if(m_inspectorController&&m_inspectorController->state().kind!=ingnomia::ui::inspector::InspectorKind::None)
						m_inspectorController->close();
					 (void)openDetachedCreatureInspector(creature,position);
				 }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorCreatureInfo(), &AggregatorCreatureInfo::signalCreatureCleared, this, [this]()

			 // A detached inspector is an independent window. Clearing the shared
			 // selection must not close every other inspector that the player opened.
			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Creature)m_inspectorController->close();}, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorCreatureInfo(), &AggregatorCreatureInfo::signalProfessionList, this, [this]( const QStringList& value )

			 {std::vector<std::string> choices;choices.reserve(value.size());for(const auto&v:value)choices.push_back(v.toStdString());m_creatureProfessionChoices=choices;if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Creature)m_inspectorController->setProfessionChoices(choices);for(auto&window:m_creatureInspectorWindows)if(window.controller&&window.controller->state().kind==ingnomia::ui::inspector::InspectorKind::Creature)window.controller->setProfessionChoices(choices);}, Qt::QueuedConnection );

    // Both presentations consume the same authoritative stockpile snapshots.
    const auto stockpileInspector=[this](const GuiStockpileInfo& value) {
        if(!m_inspectorController || !m_inspectorController->state().stockpile ||
            m_inspectorController->state().stockpile->id.value!=value.stockpileID)return;
        m_inspectorController->showStockpile(ingnomia::ui::inspector::InspectorQtDataAdapter::stockpile(value));
    };
    connect(Global::eventConnector->aggregatorStockpile(),&AggregatorStockpile::signalUpdateInfo,this,stockpileInspector,Qt::QueuedConnection);
    connect(Global::eventConnector->aggregatorStockpile(),&AggregatorStockpile::signalUpdateContent,this,stockpileInspector,Qt::QueuedConnection);

	connect( Global::eventConnector->aggregatorWorkshop(), &AggregatorWorkshop::signalUpdateInfo, this, [this]( const GuiWorkshopInfo& value )

			 {if(m_inspectorController&&m_inspectorCommands&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Workshop){m_inspectorCommands->rememberWorkshopLink(ingnomia::ui::WorkshopId{value.workshopID},value.linkStockpile);m_inspectorController->showWorkshop(ingnomia::ui::inspector::InspectorQtDataAdapter::workshop(value));} }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorWorkshop(), &AggregatorWorkshop::signalUpdateContent, this, [this]( const GuiWorkshopInfo& value )

			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Workshop)m_inspectorController->showWorkshop(ingnomia::ui::inspector::InspectorQtDataAdapter::workshop(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorWorkshop(), &AggregatorWorkshop::signalUpdateCraftList, this, [this]( const GuiWorkshopInfo& value )

			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Workshop)m_inspectorController->showWorkshop(ingnomia::ui::inspector::InspectorQtDataAdapter::workshop(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorAgri(), &AggregatorAgri::signalUpdateFarm, this, [this]( const GuiFarmInfo& value )

			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Agriculture)m_inspectorController->showAgriculture(ingnomia::ui::inspector::InspectorQtDataAdapter::farm(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorAgri(), &AggregatorAgri::signalUpdatePasture, this, [this]( const GuiPastureInfo& value )

			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Agriculture)m_inspectorController->showAgriculture(ingnomia::ui::inspector::InspectorQtDataAdapter::pasture(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorAgri(), &AggregatorAgri::signalUpdateGrove, this, [this]( const GuiGroveInfo& value )

			 {if(m_inspectorController&&m_inspectorController->state().kind==ingnomia::ui::inspector::InspectorKind::Agriculture)m_inspectorController->showAgriculture(ingnomia::ui::inspector::InspectorQtDataAdapter::grove(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalAction, this, [this]( const QString& value )

			 {if(m_inspectorController)m_inspectorController->setSelectionAction(value.toStdString(),value.startsWith("Build")); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalCursorPos, this, [this]( const QString& value )

			 {

const auto position = ingnomia::ui::inspector::InspectorQtDataAdapter::position(value);

if(m_inspectorController)m_inspectorController->setSelectionCursor(position);

if(m_management6a)m_management6a->setSelectedPosition(position); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalFirstClick, this, [this]( const QString& value )

			 {if(m_inspectorController)m_inspectorController->setSelectionAnchor(ingnomia::ui::inspector::InspectorQtDataAdapter::position(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalSize, this, [this]( const QString& value )

			 {if(m_inspectorController)m_inspectorController->setSelectionSize(value.toStdString()); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalPopulationUpdate, this, [this]( const GuiPopulationInfo& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applyPopulation(m_management6bData->population(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalUpdateSingleGnome, this, [this]( const GuiGnomeInfo& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applyPopulationPatch(m_management6bData->populationPatch(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalProfessionList, this, [this]( const QStringList& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applyProfessions(m_management6bData->professions(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalProfessionSkills, this, [this]( const QString& profession, const QList<GuiSkillInfo>& skills )

			 {

if(!m_management6bController||!m_management6bData)return;

auto converted=m_management6bData->professionSkills(profession,skills);

m_management6bController->applyProfessionSkills(m_management6bController->state().world,std::move(converted.first),std::move(converted.second)); }, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalSkillCatalog, this, [this]( const QList<GuiSkillInfo>& skills )
			 { if(m_management6bController&&m_management6bData) m_management6bController->applySkillCatalog(m_management6bController->state().world,m_management6bData->skillCatalog(skills)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalScheduleUpdate, this, [this]( const GuiScheduleInfo& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applySchedules(m_management6bData->schedules(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorPopulation(), &AggregatorPopulation::signalScheduleUpdateSingleGnome, this, [this]( const GuiGnomeScheduleInfo& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applySchedulePatch(m_management6bData->schedulePatch(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorInventory(), &AggregatorInventory::signalInventoryCategories, this, [this]( const QList<GuiInventoryCategory>& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applyInventory(m_management6bData->inventory(value)); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorInventory(), &AggregatorInventory::signalInventoryChanged, this, [this]

			 {if(m_management6bController)m_management6bController->inventoryChanged(); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorInventory(), &AggregatorInventory::signalInventoryHistory, this, [this]( const QString& item, const QString& material, const QList<GuiInventoryHistoryPoint>& value )

			 {
				if ( !m_management6bController || !m_management6bData || !m_management6bController->state().historyTarget ) return;
				auto target = *m_management6bController->state().historyTarget;
				if ( target.item.value != item.toStdString() || target.material.value != material.toStdString() )
				{
					return;
				}
				m_management6bController->applyInventoryHistory( m_management6bController->state().world, std::move( target ), m_management6bData->inventoryHistory( value ) );
			 }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorInventory(), &AggregatorInventory::signalBuildItems, this, [this]( const QList<GuiBuildItem>& value )

			 {
				if ( !m_hudController ) return;
				std::vector<ingnomia::ui::hud::BuildCatalogRow> rows;
				rows.reserve( static_cast<std::size_t>( value.size() ) );
				for ( const auto& item : value )
				{
					ingnomia::ui::BuildKind kind = ingnomia::ui::BuildKind::Item;
					if ( item.biType == BuildItemType::Workshop ) kind = ingnomia::ui::BuildKind::Workshop;
					else if ( item.biType == BuildItemType::Terrain ) kind = ingnomia::ui::BuildKind::Terrain;
					std::vector<ingnomia::ui::CatalogId> materials;
					materials.reserve( static_cast<std::size_t>( item.requiredItems.size() ) );
					std::vector<ingnomia::ui::hud::BuildCatalogRow::RequiredComponent> components;
					components.reserve( static_cast<std::size_t>( item.requiredItems.size() ) );
					bool buildable = true;
					QStringList missingMaterials;
					for ( const auto& required : item.requiredItems )
					{
						ingnomia::ui::hud::BuildCatalogRow::RequiredComponent component;
						component.item = ingnomia::ui::CatalogId{ required.itemID.toStdString() };
						component.amount = required.amount;
						bool componentAvailable = false;
						for ( const auto& available : required.availableMats )
						{
							if ( available.second > 0 )
							{
								const ingnomia::ui::CatalogId material{ available.first.toStdString() };
								component.options.push_back( { material, available.second } );
								if ( component.selected.value.empty() || available.second >= required.amount ) component.selected = material;
							}
							if ( available.second >= required.amount ) componentAvailable = true;
						}
						// A workshop blueprint must carry one material token per required
						// component. "any" lets it claim a suitable future item safely.
						materials.push_back( component.selected.value.empty()
							? ingnomia::ui::CatalogId{ "any" }
							: component.selected );
						if ( !componentAvailable )
						{
							buildable = false;
							missingMaterials.push_back( QStringLiteral( "%1 x%2" ).arg( required.itemID ).arg( required.amount ) );
						}
						components.push_back( std::move( component ) );
					}
					ingnomia::ui::hud::BuildCatalogRow row;
					row.id = ingnomia::ui::CatalogId{ item.id.toStdString() };
					row.name = item.name.toStdString();
					row.type = item.type.toStdString();
					row.type = item.type.toStdString();
					row.kind = kind;
					row.defaultMaterials = std::move( materials );
					row.components = std::move( components );
					row.spriteSheet = item.spriteSheet.toStdString();
					row.spriteX = item.spriteX;
					row.spriteY = item.spriteY;
					row.spriteWidth = item.spriteWidth;
					row.spriteHeight = item.spriteHeight;
					row.spriteSheetWidth = item.spriteSheetWidth;
					row.spriteSheetHeight = item.spriteSheetHeight;
					row.available = buildable;
					row.unavailableReason = missingMaterials.join( QStringLiteral( ", " ) ).toStdString();
					rows.push_back( std::move( row ) );
				}
				m_hudController->setBuildCatalog( std::move( rows ) );
			 }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorInventory(), &AggregatorInventory::signalWatchList, this, [this]( const QList<GuiWatchedItem>& value )
			 {
				if ( !m_hudController ) return;
				std::vector<ingnomia::ui::hud::HudWatchRow> rows;
				rows.reserve( static_cast<std::size_t>( value.size() ) );
				for ( const auto& item : value )
				{
					ingnomia::ui::InventoryRowId id;
					id.category = ingnomia::ui::CatalogId{ item.category.toStdString() };
					id.group = ingnomia::ui::CatalogId{ item.group.toStdString() };
					id.item = ingnomia::ui::CatalogId{ item.item.toStdString() };
					id.material = ingnomia::ui::CatalogId{ item.material.toStdString() };
					id.depth = item.material.isEmpty() ? ( item.item.isEmpty() ? ( item.group.isEmpty() ? ingnomia::ui::InventoryDepth::Category : ingnomia::ui::InventoryDepth::Group ) : ingnomia::ui::InventoryDepth::Item ) : ingnomia::ui::InventoryDepth::Material;
					const auto label = item.guiString.isEmpty() ? ( item.item.isEmpty() ? ( item.group.isEmpty() ? item.category : item.group ) : item.item ) : item.guiString;
					rows.push_back( { std::move( id ), label.toStdString(), static_cast<std::uint32_t>( std::max( 0, item.count ) ) } );
				}
				m_hudController->setWatchRows( std::move( rows ) );
			 }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorCreatureInfo(), &AggregatorCreatureInfo::signalCreatureUpdate, this, [this]( const GuiCreatureInfo& value )

			 {if(m_management6bController&&m_management6bData)m_management6bController->applyCreature(m_management6bData->creature(value)); }, Qt::QueuedConnection );
#if defined( INGNOMIA_DEVELOPER_UI )
	connect( Global::eventConnector->aggregatorDebug(), &AggregatorDebug::signalGnomeList, this, [this]( const QList<QPair<QString, unsigned int>>& value )
			 { if ( !m_debugController || !m_debugData ) return; auto converted = m_debugData->gnomes( value ); m_debugController->applyGnomes( m_debugData->world(), converted.first, std::move( converted.second ) ); }, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorDebug(), &AggregatorDebug::signalItemGroups, this, [this]( const QStringList& value )
			 { if ( !m_debugController || !m_debugData ) return; auto converted = m_debugData->groups( value ); m_debugController->applyCatalog( m_debugData->world(), converted.first, std::move( converted.second ) ); }, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorDebug(), &AggregatorDebug::signalItems, this, [this]( const QStringList& value )
			 { if ( !m_debugController || !m_debugData ) return; auto converted = m_debugData->items( value ); m_debugController->applyCatalog( m_debugData->world(), converted.first, std::move( converted.second ) ); }, Qt::QueuedConnection );
	connect( Global::eventConnector->aggregatorDebug(), &AggregatorDebug::signalMaterials, this, [this]( int count, const QStringList& first, const QStringList& second )
			 { if ( !m_debugController || !m_debugData ) return; auto converted = m_debugData->materials( count, first, second ); m_debugController->applyCatalog( m_debugData->world(), converted.first, std::move( converted.second )
); }, Qt::QueuedConnection );
#endif

	connect( Global::eventConnector, &EventConnector::signalHudSettlement, this, [this]( const QString& name, unsigned int gnomes, unsigned int animals, unsigned int items )

			 {


if( m_hudController ) m_hudController->setSettlement( { name.toStdString(), gnomes, animals, items } );
// The title bar names the open kingdom, then the application (PDF p.93).
const QString title = name.isEmpty() ? QStringLiteral( "Ingnomia" ) : name + QStringLiteral( " - Ingnomia" );
if( this->title() != title ) setTitle( title ); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalHudClock, this, [this]( int minute, int hour, int day, const QString& season, int year, bool daylight, int nextSunMinute )

			 {


if( !m_hudController ) return; auto state = m_hudController->state().clock;


state.minute=static_cast<std::uint8_t>(minute); state.hour=static_cast<std::uint8_t>(hour); state.day=static_cast<std::uint16_t>(day); state.year=static_cast<std::uint16_t>(year);


const auto normalized=season.toCaseFolded(); state.season=normalized=="spring"?ingnomia::ui::Season::Spring:normalized=="summer"?ingnomia::ui::Season::Summer:normalized=="autumn"?ingnomia::ui::Season::Autumn:normalized=="winter"?ingnomia::ui::Season::Winter:ingnomia::ui::Season::Unknown;


state.daylight=daylight?ingnomia::ui::DaylightPhase::Day:ingnomia::ui::DaylightPhase::Night; state.nextSunEventMinute=static_cast<std::uint16_t>(nextSunMinute); m_hudController->setClock(state); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalViewLevel, this, [this]( int level )

			 { if(m_hudController){auto s=m_hudController->state().camera;const int configuredDimZ=Global::cfg->get("dimensionZ").toInt();s.viewLevel=level;s.minLevel=0;s.maxLevel=qMax(0,(Global::dimZ>0?Global::dimZ:configuredDimZ)-1);m_hudController->setCamera(s);} }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalUpdatePause, this, [this]( bool paused )

			 {if(m_hudController){auto s=m_hudController->state().clock;s.paused=paused;s.pauseReason=paused?ingnomia::ui::PauseReason::Player:ingnomia::ui::PauseReason::None;m_hudController->setClock(s);} }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalUpdateGameSpeed, this, [this]( ::GameSpeed speed )

			 {if(m_hudController){auto s=m_hudController->state().clock;s.speed=speed==::GameSpeed::Fast?ingnomia::ui::GameSpeed::Fast:ingnomia::ui::GameSpeed::Normal;m_hudController->setClock(s);} }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalUpdateRenderOptions, this, [this]( bool d, bool j, bool w, bool a )

			 {if(m_hudController&&m_hudCommands){ingnomia::ui::RenderOverlayState s{d,j,w,a};m_hudCommands->setAuthoritativeOverlays(s);m_hudController->setOverlays(s);} }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalEvent, this, [this]( unsigned int id, const QString& title, const QString& body, bool pause, bool yesNo )

				 {if(m_hudController)m_hudController->enqueuePrompt(id?std::optional{ingnomia::ui::EventResponseTargetId{id}}:std::nullopt,title.toStdString(),body.toStdString(),pause,yesNo); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalHudTutorial, this, [this]( const TutorialSnapshot& snapshot )
				 { if( !m_hudController ) return; ingnomia::ui::hud::TutorialViewState state; state.active=snapshot.active; state.step=snapshot.step; state.stepCount=snapshot.stepCount; state.completedMask=snapshot.completedMask; state.skippedMask=snapshot.skippedMask; state.hintsEnabled=snapshot.hintsEnabled; state.pausedForLesson=snapshot.pausedForLesson; state.completed=snapshot.completed; state.incompatible=snapshot.incompatible; state.scenarioId=snapshot.scenarioId.toStdString(); state.title=snapshot.title.toStdString(); state.explanation=snapshot.explanation.toStdString(); state.objective=snapshot.objective.toStdString(); for( const auto& step : snapshot.steps ) state.steps.push_back( step.toStdString() ); for( const auto completed : snapshot.completedSteps ) state.completedSteps.push_back( completed ); state.progress=snapshot.progress.toStdString(); for( const auto& id : snapshot.highlightedIds ) state.highlightedIds.push_back( id.toStdString() ); m_hudController->setTutorial( std::move( state ) ); }, Qt::QueuedConnection );

	m_shellController->setVersion( Global::cfg->get( "CurrentVersion" ).toString().toStdString() );

	refreshContinueSave();

	connect( Global::eventConnector, &EventConnector::signalUpdatePause, this, [this]( bool paused )

			 {


if ( m_shellController ) m_shellController->onPauseState( paused,



			 paused ? ingnomia::ui::PauseReason::Player : ingnomia::ui::PauseReason::None ); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalSaveGameFinished, this, [this]( bool success )
			 {
				if ( m_shellController ) m_shellController->onSaveGameFinished( success );
				if ( success ) refreshContinueSave();
			}, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorSettings(), &AggregatorSettings::signalUpdateSettings, this, [this]( const GuiSettings& settings )

			 {


if ( !m_shellController ) return;


m_shellController->setSettingsState( ingnomia::ui::shell::ShellDataAdapter::settings( {



				settings.fullscreen, settings.followMonitorRefresh, settings.frameRateLimit, settings.scale, settings.keyboardSpeed, settings.lightMin, settings.toggleMouseWheel,
				static_cast<int>( settings.audioMasterVolume ), settings.autoSaveInterval, settings.autoSaveContinue } ) );
		 restartFrameTimer(); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalNewGameSettings, this,
		[this]( const NewGameSettingsSnapshot& settings ) {
			if ( !m_shellController ) return;
			ingnomia::ui::shell::NewGameState state;
			state.status = ingnomia::ui::shell::RequestStatus::Ready;
			state.draft.fields = {
				{ ingnomia::ui::NewGameFieldId{ "kingdom_name" }, settings.kingdomName.toStdString() },
				{ ingnomia::ui::NewGameFieldId{ "seed" }, settings.seed.toStdString() },
				{ ingnomia::ui::NewGameFieldId{ "peaceful" }, settings.peaceful },
				{ ingnomia::ui::NewGameFieldId{ "world_size" }, static_cast<std::int32_t>( settings.worldSize ) },
				{ ingnomia::ui::NewGameFieldId{ "z_levels" }, static_cast<std::int32_t>( settings.zLevels ) },
				{ ingnomia::ui::NewGameFieldId{ "ground" }, static_cast<std::int32_t>( settings.ground ) },
				{ ingnomia::ui::NewGameFieldId{ "flatness" }, static_cast<std::int32_t>( settings.flatness ) },
				{ ingnomia::ui::NewGameFieldId{ "ocean_size" }, static_cast<std::int32_t>( settings.oceanSize ) },
				{ ingnomia::ui::NewGameFieldId{ "rivers" }, static_cast<std::int32_t>( settings.rivers ) },
				{ ingnomia::ui::NewGameFieldId{ "river_size" }, static_cast<std::int32_t>( settings.riverSize ) },
				{ ingnomia::ui::NewGameFieldId{ "tree_density" }, static_cast<std::int32_t>( settings.treeDensity ) },
				{ ingnomia::ui::NewGameFieldId{ "plant_density" }, static_cast<std::int32_t>( settings.plantDensity ) },
				{ ingnomia::ui::NewGameFieldId{ "wild_animals" }, static_cast<std::int32_t>( settings.numWildAnimals ) },
				{ ingnomia::ui::NewGameFieldId{ "gnomes" }, static_cast<std::int32_t>( settings.numGnomes ) },
				{ ingnomia::ui::NewGameFieldId{ "start_zone" }, static_cast<std::int32_t>( settings.startZone ) }
			};
			state.acceptedDraft = state.draft;
			m_shellController->setNewGameState( std::move( state ) );
		}, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorLoadGame(), &AggregatorLoadGame::signalKingdoms, this, [this]( const QList<GuiSaveInfo>& values )

			 {


if ( !m_shellController || !m_shellCommands ) return;


m_shellCommands->clearPrivatePaths();


std::vector<ingnomia::ui::shell::SaveKingdomRow> rows;


for( const auto& value : values )


{



const auto key = QFileInfo( value.folder ).fileName().toStdString();



m_shellCommands->rememberKingdomPath( ingnomia::ui::SaveKingdomId{ key }, value.folder );



rows.push_back( { ingnomia::ui::SaveKingdomId{ key }, value.name.toStdString(), value.date.toSecsSinceEpoch() } );


}


auto state = ingnomia::ui::shell::ShellDataAdapter::saves( std::move( rows ), {} );
// "Look in" keeps the kingdom the last game was opened from; Cancel never changes it (PDF p.171).
if( const auto last = m_shellCommands->lastOpenedKingdom(); last && std::ranges::any_of( state.kingdoms, [&]( const auto& row ) { return row.id == *last; } ) ) state.selectedKingdom = last;
const auto firstKingdom = state.selectedKingdom;
m_shellController->setLoadGameState( std::move( state ) );
if ( firstKingdom ) m_shellController->selectKingdom( *firstKingdom ); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorLoadGame(), &AggregatorLoadGame::signalSaveGames, this, [this]( const QList<GuiSaveInfo>& values )

			 {

if ( !m_shellController || !m_shellCommands ) return;


auto state = m_shellController->state().loadGame;


std::vector<ingnomia::ui::shell::SaveSlotRow> rows;


for( const auto& value : values )


{



const auto kingdom = state.selectedKingdom ? state.selectedKingdom->relativeKey : std::string{};



const auto key = kingdom + "/" + value.dir.toStdString();



m_shellCommands->rememberSavePath( ingnomia::ui::SaveSlotId{ key }, value.folder );



rows.push_back( { ingnomia::ui::SaveSlotId{ key }, value.name.toStdString(), value.version.toStdString(),




value.date.toSecsSinceEpoch(), value.compatible } );


}


			 state.saves = std::move( rows );
			 state.error.reset();


state.savesStatus = state.saves.empty() ? ingnomia::ui::shell::RequestStatus::Empty : ingnomia::ui::shell::RequestStatus::Ready;


state.selectedSlot.reset();


for( const auto& row : state.saves ) if( row.compatible ) { state.selectedSlot = row.id; break; }


			 m_shellController->setLoadGameState( std::move( state ) ); }, Qt::QueuedConnection );

	connect( Global::eventConnector->aggregatorLoadGame(), &AggregatorLoadGame::signalLoadError, this, [this]( bool kingdoms )
			 {
				if( !m_shellController ) return;
				auto state = m_shellController->state().loadGame;
				state.error = ingnomia::ui::shell::ShellError{
					ingnomia::ui::shell::Message{ ingnomia::ui::LocalizationKey{ "ui.error.save_list_unavailable" }, {} }, false, std::nullopt };
				if( kingdoms )
				{
					state.kingdomsStatus = ingnomia::ui::shell::RequestStatus::Error;
				}
				else
				{
					state.savesStatus = ingnomia::ui::shell::RequestStatus::Error;
					state.selectedSlot.reset();
				}
				m_shellController->setLoadGameState( std::move( state ) );
			}, Qt::QueuedConnection );

	QMetaObject::invokeMethod( Global::eventConnector->aggregatorSettings(), &AggregatorSettings::onRequestSettings, Qt::QueuedConnection );
	QMetaObject::invokeMethod( Global::eventConnector, &EventConnector::onRequestNewGameSettings, Qt::QueuedConnection );

	QMetaObject::invokeMethod( Global::eventConnector->aggregatorLoadGame(), &AggregatorLoadGame::onRequestKingdoms, Qt::QueuedConnection );
	if ( m_rmlUiHost->hotReloadEnabled() )
		m_rmlUiHost->setDocumentReloadHandler( [this] { return reloadRmlUiDocuments(); } );

#if defined( INGNOMIA_UI_DESIGNER )
	if ( qEnvironmentVariableIntValue( "INGNOMIA_UI_DESIGNER" ) == 1 )
	{
		m_uiDesigner = std::make_unique<ingnomia::ui::designer::UiDesignerRmlBinding>( *m_rmlUiHost->context(),
			[this]( const char* path ) { return m_rmlUiHost ? m_rmlUiHost->loadDocument( QString::fromUtf8( path ), false ) : nullptr; } );
		m_uiDesigner->setSurfaceName( "main" );
		m_uiDesigner->setPauseHandler( [this]( bool paused )
			{ if ( m_hudController ) m_hudController->setPaused( paused ); } );
		m_uiDesigner->setCurrentPaused( m_hudController && m_hudController->state().clock.paused );
		if ( !m_uiDesigner->initialize() )
		{
			qWarning() << "RmlUi UI Designer failed to initialize";
			m_uiDesigner.reset();
		}
		else
		{
			m_uiDesigner->setVisible( false );
		}
	}
#endif

	m_windowFrame = std::make_unique<ingnomia::ui::MainWindowFrame>( *this, *m_rmlUiHost->context() );
	if ( !m_windowFrame->document() )
	{
		qWarning() << "RmlUi window frame document failed to load";
		m_windowFrame.reset();
	}
	connect( this, &QWindow::windowStateChanged, this, [this]( Qt::WindowState ) { redraw(); } );
	connect( this, &QWindow::windowTitleChanged, this, [this]( const QString& ) { redraw(); } );
	connect( qGuiApp, &QGuiApplication::applicationStateChanged, this, [this]( Qt::ApplicationState ) { redraw(); } );
	// Clicking outside the game window cancels What's This? mode (PDF p.286).
	connect( this, &QWindow::activeChanged, this, [this] {
		if ( isActive() || !m_whatsThis || !( m_whatsThis->mode() || m_whatsThis->popupOpen() ) ) return;
		m_whatsThis->cancel();
		unsetCursor();
		redraw();
	} );

	qInfo() << "RmlUi MainWindow host initialized at" << config.physicalSize << "DPR/UI ratio" << config.densityIndependentPixelRatio;

	return true;
}

bool MainWindow::openDetachedOrdersTools( std::string_view elementId )
{
	if ( !ensureDetachedOrdersTools( elementId ) ) return false;
	const auto panelKey = ordersToolsPanelKeyForElement( elementId );
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->panelKey == panelKey && window->binding )
		{
			// The Build window has no toolbar of its own; activating an element it lacks is not a failure.
			(void)window->binding->activateElement( elementId );
			return true;
		}
	return true;
}

#if defined( INGNOMIA_UI_DESIGNER )
bool MainWindow::uiDesignerEnabled() const
{
	return qEnvironmentVariableIntValue( "INGNOMIA_UI_DESIGNER" ) == 1 && m_rmlUiHost;
}

std::unique_ptr<ingnomia::ui::designer::UiDesignerRmlBinding> MainWindow::createDetachedUiDesigner(
	ingnomia::ui::RmlUiDetachedContext& context, std::string surfaceName )
{
	if ( !uiDesignerEnabled() || !context.context() ) return {};
	auto* detached = &context;
	auto designer = std::make_unique<ingnomia::ui::designer::UiDesignerRmlBinding>( *context.context(),
		[this, detached]( const char* path )
		{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QString::fromUtf8( path ), false ) : nullptr; } );
	designer->setSurfaceName( std::move( surfaceName ) );
	designer->setPauseHandler( [this]( bool paused )
		{ if ( m_hudController ) m_hudController->setPaused( paused ); } );
	designer->setCurrentPaused( m_hudController && m_hudController->state().clock.paused );
	if ( !designer->initialize() )
	{
		qWarning() << "RmlUi detached UI Designer failed to initialize";
		return {};
	}
	// Detached designers are available on demand. Only the focused surface is shown.
	designer->setVisible( false );
	return designer;
}

void MainWindow::wireDetachedUiDesigner( ingnomia::ui::RmlUiDetachedWindow& nativeWindow,
	ingnomia::ui::designer::UiDesignerRmlBinding* designer )
{
	if ( !designer ) return;
	auto* native = &nativeWindow;
	nativeWindow.setDesignerFocusHandler( [this, designer] { focusUiDesignerSurface( designer ); } );
	nativeWindow.setDesignerKeyHandler( [this, designer]( int key, Qt::KeyboardModifiers modifiers )
		{
			focusUiDesignerSurface( designer );
			if ( key == Qt::Key_F6 )
			{
				toggleUiDesignerSurface( designer );
				return true;
			}
			return designer->designMode() && designer->keyPress( key, modifiers & Qt::ShiftModifier );
		} );
	nativeWindow.setDesignerMousePressHandler( [this, designer, native]( QPointF position, Qt::MouseButton button,
		Qt::KeyboardModifiers )
		{
			if ( button != Qt::LeftButton ) return false;
			focusUiDesignerSurface( designer );
			if ( !designer->designMode() ) return false;
			const QPointF physical = position * native->devicePixelRatio();
			if ( designer->editorConsumesPoint( physical ) ) return false;
			(void)designer->selectAt( physical );
			return true;
		} );
}

void MainWindow::focusUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* designer )
{
	if ( !designer || m_uiDesignerActive == designer ) return;
	const bool transferDesignMode = m_uiDesignerActive && m_uiDesignerActive->designMode();
	if ( m_uiDesignerActive ) m_uiDesignerActive->setVisible( false );
	m_uiDesignerActive = designer;
	if ( transferDesignMode )
	{
		designer->setCurrentPaused( m_hudController && m_hudController->state().clock.paused );
		designer->setVisible( true );
	}
}

void MainWindow::toggleUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* designer )
{
	if ( !designer ) return;
	if ( m_uiDesignerActive && m_uiDesignerActive != designer )
	{
		m_uiDesignerActive->setVisible( false );
		m_uiDesignerActive = nullptr;
	}
	if ( designer->visible() )
	{
		designer->setVisible( false );
		if ( m_uiDesignerActive == designer ) m_uiDesignerActive = nullptr;
	}
	else
	{
		designer->setCurrentPaused( m_hudController && m_hudController->state().clock.paused );
		designer->setVisible( true );
		m_uiDesignerActive = designer;
	}
}

void MainWindow::deactivateUiDesignerSurface( ingnomia::ui::designer::UiDesignerRmlBinding* designer )
{
	if ( !designer ) return;
	if ( designer->visible() || designer->designMode() ) designer->setVisible( false );
	if ( m_uiDesignerActive == designer ) m_uiDesignerActive = nullptr;
}
#endif

bool MainWindow::ensureDetachedOrdersTools( std::string_view elementId )
{
	if ( !m_rmlUiHost || !m_context || !m_renderer || !m_hudController ) return false;
	const auto panelKey = ordersToolsPanelKeyForElement( elementId );
	const auto panel = ordersToolsPanelForElement( elementId );
	const auto title = ordersToolsWindowTitle( panel );
	const auto size = ordersToolsWindowSize( panel );
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const auto sizeKey = panel == HudToolPanel::Build ? QStringLiteral( "UiWindow.orders.build.compact2" ) : QStringLiteral( "UiWindow.orders.%1" ).arg( QString::fromLatin1( panelKey.data(), static_cast<int>( panelKey.size() ) ) );
	// The Build window is a Windows 98 palette window: 384 x 380 px at 1x, drawn at a whole-number scale (Stage 17).
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( devicePixelRatio() * userScale + 0.5 ) ) );
	const QSize paletteSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	const QSize nativeSize = panel == HudToolPanel::Build ? paletteSize : detachedWindowSize( sizeKey, size, userScale, this );
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->panelKey == panelKey && window->nativeWindow && window->binding )
		{
			window->nativeWindow->showAndActivate();
			return true;
		}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		title, nativeSize );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;

	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-orders-tools-" ) + QString::fromLatin1( panelKey.data(), static_cast<int>( panelKey.size() ) ), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );

	auto window = std::make_unique<OrdersToolsWindow>();
	window->panelKey = std::string( panelKey );
	window->context = std::move( detachedContext );
	window->nativeWindow = std::move( nativeWindow );
	auto* detached = window->context.get();
	window->binding = std::make_unique<ingnomia::ui::hud::HudRmlBinding>( *detached->context(),
		ingnomia::ui::hud::HudRmlBinding::Presentation::OrdersTools, panel );
	window->binding->setDocumentLoader( [this, detached]( const char* path )
		{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QString::fromUtf8( path ), false ) : nullptr; } );
	if ( !window->binding->initialize( *m_hudController ) )
	{
		window->binding->shutdown();
		window->binding.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window->context );
		window->nativeWindow->doneCurrent();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window->designer = createDetachedUiDesigner( *detached, "orders/" + window->panelKey );
#endif
	m_hudController->addViewPort( *window->binding );
	m_ordersToolsWindows.push_back( std::move( window ) );
	auto& added = *m_ordersToolsWindows.back();
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
	const auto closeKey = added.panelKey;
	added.binding->setCloseHandler( [this, closeKey]
		{
			for ( auto& window : m_ordersToolsWindows )
				if ( window && window->panelKey == closeKey )
				{
					if ( window->nativeWindow ) window->nativeWindow->requestClose();
					return;
				}
		} );
	added.nativeWindow->setCloseHandler( [this, closeKey]
		{ hideDetachedOrdersTools( closeKey ); } );
	added.nativeWindow->setResizable( false );
	added.nativeWindow->setResizeMinimumSize( panel == HudToolPanel::Build ? nativeSize : QSize( 240, 240 ) );
	if ( panel != HudToolPanel::Build ) added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( nativeSize );
	const int cascade = static_cast<int>( m_ordersToolsWindows.size() - 1 ) * 32;
	const QPoint desired = palettePosition( QStringLiteral( "UiWindow.orders.%1.position" ).arg( QString::fromLatin1( panelKey.data(), static_cast<int>( panelKey.size() ) ) ), position() + QPoint( 72 + cascade, 104 + cascade ) );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		const int x = qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 );
		const int y = qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 );
		added.nativeWindow->setPosition( x, y );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

bool MainWindow::ensureDetachedManagement6A(int requestedView)
{
	if ( !m_rmlUiHost || !m_context || !m_renderer || !m_management6a || !m_management6a->controller() ) return false;
    using ingnomia::ui::management6a::ManagementView;
    const auto view = requestedView ? static_cast<ManagementView>(requestedView) : m_management6a->controller()->state().view;
    auto* controller = m_management6a->controller(view);
    auto& windowSlot = view == ManagementView::Stockpile ? m_stockpileWindow : view == ManagementView::Agriculture ? m_agricultureWindow : m_management6aWindow;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const bool stockpileView = view == ManagementView::Stockpile;
	const QSize stockpileBaseSize( 720, 720 );
	const QSize standardBaseSize( 900, 640 );
	const QSize stockpileMinimumSize( 640, 420 );
	const QSize standardMinimumSize( 560, 400 );
	const auto stockpileSizeKey = QStringLiteral( "UiWindow.stockpile.flat_columns1" );
	const auto standardSizeKey = (view == ManagementView::Agriculture ? QStringLiteral("UiWindow.agriculture") : QStringLiteral("UiWindow.production"));
	const auto sizeKey = stockpileView ? stockpileSizeKey : standardSizeKey;
	// Workshop, stockpile and agriculture are fixed-size Windows 98 property sheets (252 x 218 DLU plus
	// caption and frame = 384 x 380 px at 1x). Their stylesheet snaps to a whole-number scale of the display
	// density (1.5x and up -> 2x, 2.5x and up -> 3x) so text and borders stay pixel-exact.
	const bool workshopView = view == ManagementView::Workshop || view == ManagementView::Agriculture || view == ManagementView::Stockpile;
	const qreal density = devicePixelRatio() * userScale;
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( density + 0.5 ) ) );
	const QSize workshopSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	const QSize minimumSize = workshopView ? workshopSize : stockpileView ? stockpileMinimumSize : standardMinimumSize;
	const QSize nativeSize = workshopView ? workshopSize : ( stockpileView && qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_MINIMUM")=="1" ) ? minimumSize : detachedWindowSize( sizeKey, stockpileView ? stockpileBaseSize : standardBaseSize, userScale, this ).expandedTo( minimumSize );
	if ( windowSlot && windowSlot->nativeWindow && windowSlot->binding )
	{
		windowSlot->nativeWindow->setResizable( false );
		windowSlot->nativeWindow->setResizeMinimumSize( minimumSize );
		windowSlot->nativeWindow->resetView( nativeSize );
		windowSlot->nativeWindow->showAndActivate();
		return true;
	}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		QStringLiteral( "Production management" ), nativeSize );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;

	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-management6a-%1" ).arg(static_cast<int>(view)), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );

	auto window = std::make_unique<Management6AWindow>();
	window->view = static_cast<int>(view);
	window->context = std::move( detachedContext );
	window->nativeWindow = std::move( nativeWindow );
	auto* detached = window->context.get();
	window->binding = std::make_unique<ingnomia::ui::management6a::Management6ARmlBinding>( *detached->context() );
	window->binding->setDocumentLoader( [this, detached]( const char* path )
		{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QString::fromUtf8( path ), false ) : nullptr; } );
	if ( !window->binding->initialize( *controller ) )
	{
		window->binding->shutdown();
		window->binding.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window->context );
		window->nativeWindow->doneCurrent();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window->designer = createDetachedUiDesigner( *detached, "management6a-" + std::to_string(static_cast<int>(view)) );
#endif
	controller->addViewPort( *window->binding );
	windowSlot = std::move( window );
	auto& added = *windowSlot;
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
    auto* native = added.nativeWindow.get();
    added.binding->setCloseHandler([native] { native->requestClose(); });
    added.nativeWindow->setCloseGuard([binding=added.binding.get()]{return binding->canClose();});
    added.nativeWindow->setCloseHandler([this, controller, view] {
        if (controller->state().view != ManagementView::None) controller->close();
        hideDetachedManagement6A(static_cast<int>(view));
    });
    added.nativeWindow->setResizable(false);
    added.nativeWindow->setResizeMinimumSize(minimumSize);
    added.nativeWindow->setResizeHandler([this, sizeKey](QSize resized) { persistDetachedWindowSize(sizeKey, resized); });
	added.nativeWindow->resetView( nativeSize );
	const QPoint desired = position() + QPoint( 48, 80 );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		const int x = qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 );
		const int y = qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 );
		added.nativeWindow->setPosition( x, y );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

bool MainWindow::ensureDetachedManagement6B( bool inventoryView )
{
	if ( !m_rmlUiHost || !m_context || !m_renderer || !m_management6bController ) return false;
    auto& windowSlot = inventoryView ? m_inventoryWindow : m_management6bWindow;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	// Population and Inventory are fixed Windows 98 windows (384 x 380 px at 1x, whole-number scale), like the workshop.
	const qreal density = devicePixelRatio() * userScale;
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( density + 0.5 ) ) );
	const QSize sheetSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	if ( windowSlot && windowSlot->nativeWindow && windowSlot->binding )
	{
		windowSlot->nativeWindow->setResizable( false );
		windowSlot->nativeWindow->resetView( sheetSize );
		windowSlot->nativeWindow->showAndActivate();
		return true;
	}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	const auto sizeKey = inventoryView ? QStringLiteral( "UiWindow.population_inventory.flat_columns1" ) : QStringLiteral( "UiWindow.population.sidebar1" );
	const QSize nativeSize = sheetSize;
	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		inventoryView ? QStringLiteral( "Inventory" ) : QStringLiteral( "Population" ), nativeSize );
	nativeWindow->setResizable( false );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;

	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-management6b-%1" ).arg(inventoryView ? 1 : 0), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );

	auto window = std::make_unique<Management6BWindow>();
	window->context = std::move( detachedContext );
	window->nativeWindow = std::move( nativeWindow );
	auto* detached = window->context.get();
	window->binding = std::make_unique<ingnomia::ui::management6b::Management6BRmlBinding>( *detached->context() );
    window->binding->setWindowSurface(inventoryView);
	window->binding->setDocumentLoader( [this, detached]( const char* path )
		{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QString::fromUtf8( path ), false ) : nullptr; } );
	if ( !window->binding->initialize( *m_management6bController ) )
	{
		window->binding->shutdown();
		window->binding.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window->context );
		window->nativeWindow->doneCurrent();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window->designer = createDetachedUiDesigner( *detached, "management6b-" + std::to_string(inventoryView ? 1 : 0) );
#endif
	m_management6bController->addViewPort( *window->binding );
	windowSlot = std::move( window );
	auto& added = *windowSlot;
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
    auto* binding = added.binding.get();
    added.binding->setRouteCloseHandler([this, inventoryView](ingnomia::ui::RouteId route, ingnomia::ui::FocusToken focus) {
        hideDetachedManagement6B(inventoryView ? 1 : 0);
        if(m_workbenchCoordinator) (void)m_workbenchCoordinator->close(route, focus);
    });
	added.binding->setStockpileOpenHandler( []( unsigned int id )
		{ if ( auto* stockpile = Global::eventConnector->aggregatorStockpile() )
			QMetaObject::invokeMethod( stockpile, [stockpile, id] { stockpile->onOpenStockpileInfo( id ); }, Qt::QueuedConnection ); } );
	added.binding->setCitizenSelectedHandler([]{ Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::OpenPopulation ) ); });
    added.nativeWindow->setCloseGuard([this,binding,inventoryView] {
        if(!inventoryView && m_management6bController->state().professionDraftDirty) {
            binding->closePopulation(); return false;
        }
        return true;
    });
    added.nativeWindow->setCloseHandler([this, binding, inventoryView] {
        if(inventoryView) binding->closeInventory(); else binding->closePopulation();
        hideDetachedManagement6B(inventoryView ? 1 : 0);
    });
	added.nativeWindow->setResizeMinimumSize( sheetSize );
	added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( nativeSize );
	const QPoint desired = position() + QPoint( 72, 104 );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		const int x = qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 );
		const int y = qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 );
		added.nativeWindow->setPosition( x, y );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

bool MainWindow::ensureDetachedManagement6C(bool diplomacyView)
{
	if ( !m_rmlUiHost || !m_context || !m_renderer || !m_management6cController ) return false;
    auto& windowSlot = diplomacyView ? m_diplomacyWindow : m_management6cWindow;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	// Military and Diplomacy are fixed Windows 98 property sheets (384 x 380 px at 1x, whole-number scale).
	const bool sheetView = true;
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( devicePixelRatio() * userScale + 0.5 ) ) );
	const QSize sheetSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	if ( windowSlot && windowSlot->nativeWindow && windowSlot->binding )
	{
		if ( sheetView ) { windowSlot->nativeWindow->setResizable( false ); windowSlot->nativeWindow->resetView( sheetSize ); }
		windowSlot->nativeWindow->showAndActivate();
		return true;
	}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	const QSize baseSize( 960, 640 );
	const auto sizeKey = (diplomacyView ? QStringLiteral("UiWindow.diplomacy") : QStringLiteral("UiWindow.military_diplomacy"));
	const QSize nativeSize = sheetView ? sheetSize : detachedWindowSize( sizeKey, baseSize, userScale, this );
	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		diplomacyView ? QStringLiteral( "Diplomacy" ) : QStringLiteral( "Military" ), nativeSize );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;

	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-management6c-%1" ).arg(diplomacyView ? 1 : 0), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );

	auto window = std::make_unique<Management6CWindow>();
	window->context = std::move( detachedContext );
	window->nativeWindow = std::move( nativeWindow );
	auto* detached = window->context.get();
	window->binding = std::make_unique<ingnomia::ui::management6c::Management6CRmlBinding>( *detached->context() );
    window->binding->setWindowSurface(diplomacyView);
	window->binding->setDocumentLoader( [this, detached]( const char* path )
		{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QString::fromUtf8( path ), false ) : nullptr; } );
	if ( !window->binding->initialize( *m_management6cController ) )
	{
		window->binding->shutdown();
		window->binding.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window->context );
		window->nativeWindow->doneCurrent();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window->designer = createDetachedUiDesigner( *detached, "management6c-" + std::to_string(diplomacyView ? 1 : 0) );
#endif
	m_management6cController->addViewPort( *window->binding );
	windowSlot = std::move( window );
	auto& added = *windowSlot;
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
    auto* binding = added.binding.get();
    added.binding->setRouteCloseHandler([this, diplomacyView](ingnomia::ui::RouteId route, ingnomia::ui::FocusToken focus) {
        hideDetachedManagement6C(diplomacyView ? 1 : 0);
        if(m_workbenchCoordinator) (void)m_workbenchCoordinator->close(route, focus);
    });
    added.nativeWindow->setCloseHandler([this, binding, diplomacyView] {
        if(diplomacyView) binding->closeDiplomacy(); else binding->closeMilitary();
        hideDetachedManagement6C(diplomacyView ? 1 : 0);
    });
	added.nativeWindow->setResizeMinimumSize( sheetView ? sheetSize : QSize( 720, 460 ) );
	if ( sheetView ) added.nativeWindow->setResizable( false );
	added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( nativeSize );
	const QPoint desired = position() + QPoint( 96, 128 );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		const int x = qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 );
		const int y = qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 );
		added.nativeWindow->setPosition( x, y );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

void MainWindow::hideDetachedManagement6A(int view)
{
    for(auto* window : {m_management6aWindow.get(), m_stockpileWindow.get(), m_agricultureWindow.get()})
    {
        if(!window || (view && window->view != view)) continue;
	if ( window->nativeWindow )
	{
	#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window->designer.get() );
	#endif
		window->nativeWindow->stopRendering();
		window->nativeWindow->hide();
	}
}
}

void MainWindow::hideDetachedManagement6B(int surface)
{
    for (int index = 0; index < 2; ++index) {
        if(surface >= 0 && surface != index) continue;
        auto* window = index ? m_inventoryWindow.get() : m_management6bWindow.get();
	if ( window && window->nativeWindow )
	{
	#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window->designer.get() );
	#endif
		window->nativeWindow->stopRendering();
		window->nativeWindow->hide();
	}
}
}

void MainWindow::hideDetachedManagement6C(int surface)
{
    for (int index = 0; index < 2; ++index) {
        if(surface >= 0 && surface != index) continue;
        auto* window = index ? m_diplomacyWindow.get() : m_management6cWindow.get();
	if ( window && window->nativeWindow )
	{
	#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window->designer.get() );
	#endif
		window->nativeWindow->stopRendering();
		window->nativeWindow->hide();
	}
}
}

void MainWindow::hideDetachedOrdersTools()
{
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->nativeWindow )
		{
		#if defined( INGNOMIA_UI_DESIGNER )
			deactivateUiDesignerSurface( window->designer.get() );
		#endif
			window->nativeWindow->stopRendering();
			window->nativeWindow->hide();
		}
}

void MainWindow::hideDetachedOrdersTools( std::string_view panelKey )
{
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->panelKey == panelKey && window->nativeWindow )
		{
		#if defined( INGNOMIA_UI_DESIGNER )
			deactivateUiDesignerSurface( window->designer.get() );
		#endif
			persistPalettePosition( QStringLiteral( "UiWindow.orders.%1.position" ).arg( QString::fromLatin1( panelKey.data(), static_cast<int>( panelKey.size() ) ) ), window->nativeWindow->position() );
			window->nativeWindow->stopRendering();
			window->nativeWindow->hide();
			return;
		}
}

void MainWindow::hideDetachedManagementWindows()
{
	hideDetachedOrdersTools();
	hideDetachedManagement6A();
	hideDetachedManagement6B();
	hideDetachedManagement6C();
}

void MainWindow::closeDetachedManagement6B()
{
	if ( m_management6bWindow && m_management6bWindow->binding )
		m_management6bWindow->binding->closeRoute();
	else
		hideDetachedManagement6B();
}

void MainWindow::closeDetachedManagement6C()
{
	if ( m_management6cWindow && m_management6cWindow->binding )
		m_management6cWindow->binding->closeRoute();
	else
		hideDetachedManagement6C();
}

void MainWindow::destroyDetachedManagementWindows()
{
	if ( !m_rmlUiHost ) return;

	for ( auto& windowPtr : m_ordersToolsWindows )
	{
		if ( !windowPtr ) continue;
		auto& window = *windowPtr;
		if ( window.nativeWindow )
		{
			window.nativeWindow->stopRendering();
			window.nativeWindow->setCloseHandler( {} );
			window.nativeWindow->hide();
		}
		const bool detachedCurrent = window.nativeWindow && window.nativeWindow->makeCurrent();
#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window.designer.get() );
		if ( window.designer )
		{
			window.designer->shutdown();
			window.designer.reset();
		}
#endif
		if ( window.binding )
		{
			if ( m_hudController ) m_hudController->removeViewPort( *window.binding );
			window.binding->setCloseHandler( {} );
			window.binding->setDocumentLoader( {} );
			window.binding->shutdown();
			window.binding.reset();
		}
		const bool destroyed = detachedCurrent && m_rmlUiHost->destroyDetachedContext( window.context );
		if ( detachedCurrent && window.nativeWindow ) window.nativeWindow->doneCurrent();
		if ( !destroyed || !m_context || !m_context->makeCurrent( this ) )
			qFatal( "RmlUi detached Orders & tools shutdown failed" );
		if ( window.nativeWindow ) window.nativeWindow->attachContext( nullptr );
	}
	m_ordersToolsWindows.clear();

	for (auto* slot : {&m_management6aWindow, &m_stockpileWindow, &m_agricultureWindow})
	{
		if (!*slot) continue;
        auto& window = **slot;
		if ( window.nativeWindow )
		{
			window.nativeWindow->stopRendering();
			window.nativeWindow->setCloseHandler( {} );
			window.nativeWindow->hide();
		}
		const bool detachedCurrent = window.nativeWindow && window.nativeWindow->makeCurrent();
#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window.designer.get() );
		if ( window.designer )
		{
			window.designer->shutdown();
			window.designer.reset();
		}
#endif
		if ( window.binding )
		{
			if ( m_management6a && m_management6a->controller(static_cast<ingnomia::ui::management6a::ManagementView>(window.view)) )
				m_management6a->controller(static_cast<ingnomia::ui::management6a::ManagementView>(window.view))->removeViewPort( *window.binding );
			window.binding->setCloseHandler( {} );
			window.binding->shutdown();
			window.binding.reset();
		}
		const bool destroyed = detachedCurrent && m_rmlUiHost->destroyDetachedContext( window.context );
		if ( detachedCurrent && window.nativeWindow ) window.nativeWindow->doneCurrent();
		if ( !destroyed || !m_context || !m_context->makeCurrent( this ) )
			qFatal( "RmlUi detached production management shutdown failed" );
		if ( window.nativeWindow ) window.nativeWindow->attachContext( nullptr );
		slot->reset();
	}

	for(auto* slot : {&m_management6bWindow, &m_inventoryWindow})
	{
		if(!*slot) continue;
        auto& window = **slot;
		if ( window.nativeWindow )
		{
			window.nativeWindow->stopRendering();
			window.nativeWindow->setCloseHandler( {} );
			window.nativeWindow->hide();
		}
		const bool detachedCurrent = window.nativeWindow && window.nativeWindow->makeCurrent();
#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window.designer.get() );
		if ( window.designer )
		{
			window.designer->shutdown();
			window.designer.reset();
		}
#endif
		if ( window.binding )
		{
			if ( m_management6bController ) m_management6bController->removeViewPort( *window.binding );
			window.binding->setRouteCloseHandler( {} );
			window.binding->shutdown();
			window.binding.reset();
		}
		const bool destroyed = detachedCurrent && m_rmlUiHost->destroyDetachedContext( window.context );
		if ( detachedCurrent && window.nativeWindow ) window.nativeWindow->doneCurrent();
		if ( !destroyed || !m_context || !m_context->makeCurrent( this ) )
			qFatal( "RmlUi detached population management shutdown failed" );
		if ( window.nativeWindow ) window.nativeWindow->attachContext( nullptr );
		slot->reset();
	}

	for(auto* slot : {&m_management6cWindow, &m_diplomacyWindow})
	{
		if(!*slot) continue;
        auto& window = **slot;
		if ( window.nativeWindow )
		{
			window.nativeWindow->stopRendering();
			window.nativeWindow->setCloseHandler( {} );
			window.nativeWindow->hide();
		}
		const bool detachedCurrent = window.nativeWindow && window.nativeWindow->makeCurrent();
#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window.designer.get() );
		if ( window.designer )
		{
			window.designer->shutdown();
			window.designer.reset();
		}
#endif
		if ( window.binding )
		{
			if ( m_management6cController ) m_management6cController->removeViewPort( *window.binding );
			window.binding->setRouteCloseHandler( {} );
			window.binding->shutdown();
			window.binding.reset();
		}
		const bool destroyed = detachedCurrent && m_rmlUiHost->destroyDetachedContext( window.context );
		if ( detachedCurrent && window.nativeWindow ) window.nativeWindow->doneCurrent();
		if ( !destroyed || !m_context || !m_context->makeCurrent( this ) )
			qFatal( "RmlUi detached military management shutdown failed" );
		if ( window.nativeWindow ) window.nativeWindow->attachContext( nullptr );
		slot->reset();
	}
}

bool MainWindow::openDetachedCreatureInspector( const ingnomia::ui::inspector::CreatureInspectorState& creature,
	std::optional<ingnomia::ui::WorldPosition> position )
{
	if ( !m_rmlUiHost || !m_context || !m_renderer || !creature.id ) return false;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	// Inspectors are Windows 98 palette windows: 384 x 380 px at 1x, drawn at a whole-number scale (Stage 16).
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( devicePixelRatio() * userScale + 0.5 ) ) );
	const QSize inspectorSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	const ingnomia::ui::WorldEpoch epoch{ m_uiWorldEpoch ? m_uiWorldEpoch : 1 };
	for ( const auto& window : m_creatureInspectorWindows )
		if ( window.controller && window.controller->state().creature
			&& window.controller->state().creature->id == creature.id )
			return true;

	// Parked inspectors retain their RmlUi and Qt OpenGL contexts after the
	// player closes them. Reusing one avoids destroying a shared context while
	// another inspector and the main canvas are still rendering.
	for ( auto& window : m_creatureInspectorWindows )
	{
		if ( window.slot <= 0 || !window.nativeWindow || !window.context || !window.binding || !window.controller
			|| window.nativeWindow->isVisible()
			|| window.controller->state().kind != ingnomia::ui::inspector::InspectorKind::None )
			continue;
		window.nativeWindow->setTitle( QString::fromStdString( creature.name ) + QStringLiteral( " Properties" ) );
		window.controller->beginWorld( epoch );
		window.controller->showCreature( creature, position );
		if ( !m_creatureProfessionChoices.empty() ) window.controller->setProfessionChoices( m_creatureProfessionChoices );
		// A parked context can retain compiled RmlUi geometry from the previous
		// expanded state. Rebuild only the document on reuse; keep the native
		// window and shared GL context alive so multiple inspectors remain safe.
		// A hidden QWindow is not a current OpenGL surface on Windows. Expose it
		// while rendering is stopped so the parked context can safely rebuild its
		// document before the inspector is shown again.
		window.nativeWindow->show();
		if ( !window.nativeWindow->makeCurrent() )
		{
			qWarning() << "Could not make parked inspector current for document reset";
			return false;
		}
		const bool documentReloaded = window.binding->reloadDocument( *window.controller );
#if defined( INGNOMIA_UI_DESIGNER )
		const bool designerReloaded = !window.designer || window.designer->reloadDocument();
#else
		const bool designerReloaded = true;
#endif
		window.nativeWindow->doneCurrent();
		if ( !documentReloaded || !designerReloaded )
		{
			qWarning() << "Could not reload parked inspector document";
			return false;
		}
		window.nativeWindow->setResizable( false );
		window.nativeWindow->resetView( inspectorSize );
        // Keep a reused inspector owned by the game window as well. On Windows,
		// the transient-parent relationship keeps this native tool window above
		// its owner when the game canvas regains focus, without making it
		// permanently topmost over unrelated applications.
		window.nativeWindow->setTransientParent( this );
        window.nativeWindow->showAndActivate();
		(void)m_context->makeCurrent( this );
		return true;
	}

	int slot = 0;
	for ( int candidate = 1; candidate < MainWindowRenderer::cameraPreviewSlotCount; ++candidate )
	{
		const bool inUse = std::any_of( m_creatureInspectorWindows.begin(), m_creatureInspectorWindows.end(),
			[candidate]( const CreatureInspectorWindow& window ) { return window.slot == candidate; } );
		if ( !inUse ) { slot = candidate; break; }
	}
	if ( slot == 0 )
	{
		qWarning() << "No camera preview slots remain for a detached creature inspector";
		return false;
	}

	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) )
	{
		qWarning() << "Could not make the main OpenGL context current for a detached inspector";
		return false;
	}

	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		QString::fromStdString( creature.name ) + QStringLiteral( " Properties" ), inspectorSize );
	// Make the inspector an owned/transient tool window. It remains independently
	// movable and each inspector keeps its own native window, while the OS keeps
	// it above the game instead of allowing it to disappear behind the canvas.
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() ) return false;
	if ( !nativeWindow->makeCurrent() )
	{
		qWarning() << "Could not make the detached OpenGL context current for inspector creation";
		return false;
	}

	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-creature-inspector-%1" ).arg( slot ), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );

	const std::string cameraSource = "camera-preview://slot-" + std::to_string( slot );
	m_rmlUiHost->setCameraPreviewTexture( cameraSource, m_renderer->cameraPreviewTexture( slot ),
		m_renderer->cameraPreviewWidth(), m_renderer->cameraPreviewHeight() );

	CreatureInspectorWindow window;
	window.slot = slot;
	window.context = std::move( detachedContext );
	window.nativeWindow = std::move( nativeWindow );
	window.binding = new ingnomia::ui::inspector::InspectorRmlBinding( *window.context->context(), slot, 0, true );
	// Detached contexts must load through the host so the shared system
	// interface is pointed at this native window while RmlUi resolves and
	// constructs the document. Loading directly from the binding leaves the
	// window with only its clear color on some platforms.
	if ( auto* detached = window.context.get() )
	{
		window.binding->setDocumentLoader( [this, detached]
			{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached,
				QStringLiteral( "screens/inspector.rml" ), false ) : nullptr; } );
	}
	window.commands = std::make_unique<ingnomia::ui::inspector::InspectorQtCommandPort>( Global::eventConnector );
	window.controller = std::make_unique<ingnomia::ui::inspector::InspectorController>( *window.commands, *window.binding );
	if ( !window.binding->initialize( *window.controller ) )
	{
		delete window.binding;
		window.binding = nullptr;
		window.controller.reset();
		window.commands.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window.context );
		window.nativeWindow->doneCurrent();
		window.nativeWindow.reset();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window.designer = createDetachedUiDesigner( *window.context, "creature-inspector/" + std::to_string( slot ) );
#endif
	window.commands->setWorld( epoch, true );
    window.controller->beginWorld( epoch );
    window.controller->showCreature( creature, position );
    if ( !m_creatureProfessionChoices.empty() ) window.controller->setProfessionChoices( m_creatureProfessionChoices );
    // Store the record before wiring close callbacks: native WM_CLOSE and the
	// RmlUi close button both remove the same stable slot from this collection.
	m_creatureInspectorWindows.push_back( std::move( window ) );
	auto& added = m_creatureInspectorWindows.back();
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
	added.binding->setCloseHandler( [this, slot]
		{ if ( const auto it = std::ranges::find_if( m_creatureInspectorWindows,
			[slot]( const CreatureInspectorWindow& value ) { return value.slot == slot; } ); it != m_creatureInspectorWindows.end() )
			it->nativeWindow->requestClose(); } );
	added.binding->setExpertiseOpenedHandler([]{ Global::eventConnector->onTutorialFact( static_cast<unsigned int>( TutorialFact::InspectGnome ) ); });
	added.nativeWindow->setResizeMinimumSize( inspectorSize );
	added.nativeWindow->setResizable( false );
	added.nativeWindow->resetView( inspectorSize );
	added.nativeWindow->setCloseHandler( [this, slot]
		{
			// The native window X does not pass through the RmlUi close button.
			// Clear the shared creature selection first so the periodic creature
			// refresh cannot recreate the inspector after it has been closed.
			if ( const auto it = std::ranges::find_if( m_creatureInspectorWindows,
				[slot]( const CreatureInspectorWindow& value ) { return value.slot == slot; } );
				it != m_creatureInspectorWindows.end() && it->controller
				&& it->controller->state().kind == ingnomia::ui::inspector::InspectorKind::Creature )
				it->controller->close();
			QTimer::singleShot( 0, this, [this, slot] { closeDetachedCreatureInspector( slot ); } );
		} );

	const QPoint desired = palettePosition( inspectorPositionKey( slot ), this->position() + QPoint( 48 + ( slot - 1 ) * 28, 80 + ( slot - 1 ) * 28 ) );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		const int x = qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 );
		const int y = qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 );
		added.nativeWindow->setPosition( x, y );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

bool MainWindow::openDetachedLiveTileInspector()
{
	if ( !m_rmlUiHost || !m_context || !Global::eventConnector ) return false;
	constexpr int liveTileSlot = -2;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( devicePixelRatio() * userScale + 0.5 ) ) );
	const QSize inspectorSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	const ingnomia::ui::WorldEpoch epoch{ m_uiWorldEpoch ? m_uiWorldEpoch : 1 };

	for ( auto& window : m_creatureInspectorWindows )
	{
		if ( window.slot != liveTileSlot || !window.nativeWindow || !window.context || !window.binding || !window.controller || !window.commands ) continue;
		if ( window.nativeWindow->isVisible() )
		{
			window.nativeWindow->raise();
			window.nativeWindow->requestActivate();
			return true;
		}
		// Windows cannot make a parked, hidden OpenGL surface current. Expose it
		// before rebuilding the inspector document, as the other native inspectors do.
		window.nativeWindow->show();
		if ( !window.nativeWindow->makeCurrent() ) return false;
		window.commands->setWorld( epoch, true );
		window.controller->beginWorld( epoch );
		const bool reloaded = window.binding->reloadDocument( *window.controller );
		if ( reloaded ) window.binding->setLiveInspection( true );
		window.nativeWindow->doneCurrent();
		if ( !reloaded ) return false;
		window.nativeWindow->resetView( inspectorSize );
		window.nativeWindow->showAndActivate();
		(void)m_context->makeCurrent( this );
		return true;
	}

	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;
	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		QStringLiteral( "Tile Properties" ), inspectorSize );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;
	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-live-tile-inspector" ), physicalSize, ratio );
	if ( !detachedContext ) { nativeWindow->doneCurrent(); return false; }
	nativeWindow->setUserUiScale( userScale );
	CreatureInspectorWindow window;
	window.slot = liveTileSlot;
	window.context = std::move( detachedContext );
	window.nativeWindow = std::move( nativeWindow );
	window.binding = new ingnomia::ui::inspector::InspectorRmlBinding( *window.context->context(), 0, 0, true );
	if ( auto* detached = window.context.get() )
		window.binding->setDocumentLoader( [this, detached]
			{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QStringLiteral( "screens/inspector.rml" ), false ) : nullptr; } );
	window.commands = std::make_unique<ingnomia::ui::inspector::InspectorQtCommandPort>( Global::eventConnector );
	window.controller = std::make_unique<ingnomia::ui::inspector::InspectorController>( *window.commands, *window.binding );
	if ( !window.binding->initialize( *window.controller ) )
	{
		delete window.binding;
		window.binding = nullptr;
		window.controller.reset();
		window.commands.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window.context );
		window.nativeWindow->doneCurrent();
		return false;
	}
	window.commands->setWorld( epoch, true );
	window.controller->beginWorld( epoch );
	window.binding->setLiveInspection( true );
	m_creatureInspectorWindows.push_back( std::move( window ) );
	auto& added = m_creatureInspectorWindows.back();
	added.nativeWindow->attachContext( added.context.get() );
	added.binding->setCloseHandler( [this] { setTileInspection( false ); } );
	added.binding->setReplaceFloorHandler( [this]
		{ QTimer::singleShot( 0, this, [this] { setTileInspection( false ); (void)openDetachedOrdersTools( "hud_build_floor" ); } ); } );
	added.nativeWindow->setResizeMinimumSize( inspectorSize );
	added.nativeWindow->setResizable( false );
	added.nativeWindow->resetView( inspectorSize );
	added.nativeWindow->setCloseHandler( [this]
		{ QTimer::singleShot( 0, this, [this] { setTileInspection( false ); } ); } );
	const QPoint desired = palettePosition( inspectorPositionKey( -2 ), this->position() + QPoint( 184, 64 ) );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		added.nativeWindow->setPosition(
			qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 ),
			qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 ) );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

bool MainWindow::openDetachedBlueprintInspector( const ingnomia::ui::inspector::TileInspectorState& tile )
{
	if ( !m_rmlUiHost || !m_context || !tile.id ) return false;
	constexpr int blueprintSlot = -1;
	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const int sheetScale = qMax( 1, static_cast<int>( std::floor( devicePixelRatio() * userScale + 0.5 ) ) );
	const QSize inspectorSize( static_cast<int>( std::ceil( 384.0 * sheetScale / devicePixelRatio() ) ), static_cast<int>( std::ceil( 380.0 * sheetScale / devicePixelRatio() ) ) );
	const ingnomia::ui::WorldEpoch epoch{ m_uiWorldEpoch ? m_uiWorldEpoch : 1 };

	for ( auto& window : m_creatureInspectorWindows )
	{
		if ( window.slot != blueprintSlot || !window.nativeWindow || !window.context || !window.binding || !window.controller ) continue;
		window.controller->showTile( tile );
		if ( window.nativeWindow->isVisible() )
		{
			window.nativeWindow->raise();
			window.nativeWindow->requestActivate();
			return true;
		}
		window.nativeWindow->show();
		if ( !window.nativeWindow->makeCurrent() )
		{
			qWarning() << "Could not make parked blueprint inspector current";
			return false;
		}
		window.controller->beginWorld( epoch );
		window.controller->showTile( tile );
		const bool reloaded = window.binding->reloadDocument( *window.controller );
#if defined( INGNOMIA_UI_DESIGNER )
		const bool designerReloaded = !window.designer || window.designer->reloadDocument();
#else
		const bool designerReloaded = true;
#endif
		window.nativeWindow->doneCurrent();
		if ( !reloaded || !designerReloaded ) return false;
		window.nativeWindow->resetView( inspectorSize );
		window.nativeWindow->setTransientParent( this );
		window.nativeWindow->showAndActivate();
		(void)m_context->makeCurrent( this );
		return true;
	}

	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) )
	{
		qWarning() << "Could not make the main OpenGL context current for blueprint inspector creation";
		return false;
	}
	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		QStringLiteral( "Blueprint Properties" ), inspectorSize );
	nativeWindow->setTransientParent( this );
	if ( !nativeWindow->initializeOpenGL() || !nativeWindow->makeCurrent() ) return false;
	const float ratio = static_cast<float>( devicePixelRatio() ) * userScale;
	const QSize physicalSize( qMax( 1, qRound( nativeWindow->width() * nativeWindow->devicePixelRatio() ) ),
		qMax( 1, qRound( nativeWindow->height() * nativeWindow->devicePixelRatio() ) ) );
	auto detachedContext = m_rmlUiHost->createDetachedContext( nativeWindow.get(),
		QStringLiteral( "ingnomia-blueprint-inspector" ), physicalSize, ratio );
	if ( !detachedContext )
	{
		nativeWindow->doneCurrent();
		return false;
	}
	nativeWindow->setUserUiScale( userScale );
	CreatureInspectorWindow window;
	window.slot = blueprintSlot;
	window.context = std::move( detachedContext );
	window.nativeWindow = std::move( nativeWindow );
	window.binding = new ingnomia::ui::inspector::InspectorRmlBinding( *window.context->context(), 0, 0, true );
	if ( auto* detached = window.context.get() )
		window.binding->setDocumentLoader( [this, detached]
			{ return m_rmlUiHost ? m_rmlUiHost->loadDocument( *detached, QStringLiteral( "screens/inspector.rml" ), false ) : nullptr; } );
	window.commands = std::make_unique<ingnomia::ui::inspector::InspectorQtCommandPort>( Global::eventConnector );
	window.controller = std::make_unique<ingnomia::ui::inspector::InspectorController>( *window.commands, *window.binding );
	if ( !window.binding->initialize( *window.controller ) )
	{
		delete window.binding;
		window.binding = nullptr;
		window.controller.reset();
		window.commands.reset();
		(void)m_rmlUiHost->destroyDetachedContext( window.context );
		window.nativeWindow->doneCurrent();
		return false;
	}
#if defined( INGNOMIA_UI_DESIGNER )
	window.designer = createDetachedUiDesigner( *window.context, "blueprint-inspector" );
#endif
	window.commands->setWorld( epoch, true );
	window.controller->beginWorld( epoch );
	window.controller->showTile( tile );
	m_creatureInspectorWindows.push_back( std::move( window ) );
	auto& added = m_creatureInspectorWindows.back();
	added.nativeWindow->attachContext( added.context.get() );
#if defined( INGNOMIA_UI_DESIGNER )
	wireDetachedUiDesigner( *added.nativeWindow, added.designer.get() );
#endif
	added.binding->setCloseHandler( [this] { closeDetachedCreatureInspector( -1 ); } );
	added.nativeWindow->setResizeMinimumSize( inspectorSize );
	added.nativeWindow->setResizable( false );
	added.nativeWindow->resetView( inspectorSize );
	added.nativeWindow->setCloseHandler( [this]
		{
			if ( const auto it = std::ranges::find_if( m_creatureInspectorWindows,
				[]( const CreatureInspectorWindow& value ) { return value.slot == -1; } );
				it != m_creatureInspectorWindows.end() && it->controller ) it->controller->close();
			QTimer::singleShot( 0, this, [this] { closeDetachedCreatureInspector( -1 ); } );
		} );
	const QPoint desired = palettePosition( inspectorPositionKey( -1 ), this->position() + QPoint( 48, 80 ) );
	if ( QScreen* screen = QGuiApplication::screenAt( desired ) )
	{
		const QRect bounds = screen->availableGeometry();
		added.nativeWindow->setPosition(
			qBound( bounds.left() + 8, desired.x(), bounds.right() - added.nativeWindow->width() - 8 ),
			qBound( bounds.top() + 8, desired.y(), bounds.bottom() - added.nativeWindow->height() - 8 ) );
	}
	added.nativeWindow->showAndActivate();
	(void)m_context->makeCurrent( this );
	return true;
}

void MainWindow::closeDetachedCreatureInspector( int slot )
{
	const auto it = std::ranges::find_if( m_creatureInspectorWindows,
		[slot]( const CreatureInspectorWindow& window ) { return window.slot == slot; } );
	if ( it == m_creatureInspectorWindows.end() ) return;
	if ( it->nativeWindow ) persistPalettePosition( inspectorPositionKey( slot ), it->nativeWindow->position() );
	// The close event already hides the native surface. Keep the RmlUi context,
	// renderer, and shared Qt OpenGL context parked for reuse; destroying one
	// while another inspector is live can invalidate the remaining windows and
	// the main canvas on drivers that share the GL resource group.
	if ( it->controller && it->controller->state().kind != ingnomia::ui::inspector::InspectorKind::None )
		it->controller->close();
	#if defined( INGNOMIA_UI_DESIGNER )
	deactivateUiDesignerSurface( it->designer.get() );
	#endif
	if ( it->nativeWindow )
	{
		it->nativeWindow->stopRendering();
		// Keep the parked surface at its visible size. Shrinking it to the old
		// compact inspector dimensions makes the next opening appear clipped.
		it->nativeWindow->resetView( it->nativeWindow->size() );
		it->nativeWindow->hide();
	}
	if ( m_rmlUiHost ) m_rmlUiHost->setSystemWindow( this );
	if ( m_context && m_context->makeCurrent( this ) )
	{
		const int pixelWidth = qMax( 1, qRound( width() * devicePixelRatio() ) );
		const int pixelHeight = qMax( 1, qRound( height() * devicePixelRatio() ) );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glViewport( 0, 0, pixelWidth, pixelHeight );
	}
	m_pendingUpdate = false;
	redraw();
}

void

MainWindow::shutdownRmlUi()

{

	if ( !m_rmlUiHost )

		return;

	if ( !m_context || ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) )

		qFatal( "Cannot safely shut down RmlUi without its owning Qt OpenGL context" );

	destroyDetachedManagementWindows();

	if ( m_management6a )

		m_management6a->shutdown();

	m_management6a.reset();

	m_workbenchCoordinator.reset();

	if ( m_management6cBridge )

		m_management6cBridge->detach();

	if ( m_management6cBinding )

		m_management6cBinding->shutdown();
#if defined( INGNOMIA_DEVELOPER_UI )
	m_debugBinding = nullptr;
	m_debugController.reset();
	m_debugCommands.reset();
	m_debugData.reset();
#endif

	m_management6cBridge.reset();

	m_management6cController.reset();

	m_management6cBinding.reset();

	m_management6cCommands.reset();

	m_management6bController.reset();

	m_management6bCommands.reset();

	m_management6bData.reset();
	m_management6bBinding = nullptr;

	m_inspectorController.reset();
	for ( auto& window : m_creatureInspectorWindows )
	{
		if ( window.nativeWindow )
		{
			window.nativeWindow->stopRendering();
			window.nativeWindow->setCloseHandler( {} );
			window.nativeWindow->hide();
		}
		const bool detachedCurrent = window.nativeWindow && window.nativeWindow->makeCurrent();
#if defined( INGNOMIA_UI_DESIGNER )
		deactivateUiDesignerSurface( window.designer.get() );
		if ( window.designer )
		{
			window.designer->shutdown();
			window.designer.reset();
		}
#endif
		if ( window.binding )
		{
			window.binding->setCloseHandler( {} );
			window.binding->shutdown();
			delete window.binding;
			window.binding = nullptr;
		}
		window.controller.reset();
		window.commands.reset();
        // RmlUi destroys the detached RenderManager and its GL objects as part
        // of RemoveContext. Keep that detached context current until the host
        // has removed the context and released its renderer; switching to the
        // primary context first leaves shared GL resources with the wrong owner.
        const bool destroyed = detachedCurrent && m_rmlUiHost
            && m_rmlUiHost->destroyDetachedContext( window.context );
        if ( detachedCurrent && window.nativeWindow ) window.nativeWindow->doneCurrent();
        if ( !destroyed || !m_context || !m_context->makeCurrent( this ) )
            qFatal( "RmlUi detached inspector shutdown failed" );
		if ( window.nativeWindow ) window.nativeWindow->attachContext( nullptr );
	}
	m_creatureInspectorWindows.clear();
	m_creatureProfessionChoices.clear();

#if defined( INGNOMIA_UI_DESIGNER )
	deactivateUiDesignerSurface( m_uiDesigner.get() );
#endif
	m_inspectorCommands.reset();

	m_hudController.reset();

	m_hudCommands.reset();
	m_hudBinding = nullptr;
	m_inspectorBinding = nullptr;

	m_shellController.reset();

	m_shellCommands.reset();
	m_shellBinding = nullptr;

#if defined( INGNOMIA_UI_DESIGNER )
	m_uiDesigner.reset();
	m_uiDesignerActive = nullptr;
#endif

	if (!m_context->makeCurrent(this)) qFatal("Cannot restore main context for RmlUi shutdown");
	m_whatsThis.reset();
	m_windowFrame.reset();
	if ( !m_rmlUiHost->shutdown() )

		qFatal( "RmlUi MainWindow shutdown failed" );

	m_rmlUiHost.reset();
}

void

MainWindow::resizeRmlUi()

{

	if ( !rmlUiActive() )

		return;

	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) )

	{

		qCritical() << "Cannot resize RmlUi without its owning Qt OpenGL context";

		return;
	}

	const QSize physicalSize( qMax( 1, qRound( width() * devicePixelRatio() ) ), qMax( 1, qRound( height() * devicePixelRatio() ) ) );

	const float ratio = static_cast<float>( devicePixelRatio() ) * qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );

	if ( !m_rmlUiHost->resize( physicalSize, ratio ) )

		qCritical() << "RmlUi resize/DPI update failed" << physicalSize << ratio;
}

bool MainWindow::beginRmlWindowDrag( QPointF position )
{
	if ( !rmlUiActive() || !m_rmlUiHost->context() ) return false;

	const qreal dpr = qMax<qreal>( 0.01, devicePixelRatio() );
	const auto point = Rml::Vector2f( static_cast<float>( std::lround( position.x() * dpr ) ),
		static_cast<float>( std::lround( position.y() * dpr ) ) );
	auto* element = m_rmlUiHost->context()->GetElementAtPoint( point );
	Rml::Element* handle = nullptr;
	for ( auto* current = element; current; current = current->GetParentNode() )
	{
		// Action buttons are siblings of the handle. Keep this guard so nested
		// button content cannot accidentally turn a button press into a drag.
		if ( current->GetTagName() == "button" ) return false;
		if ( current->GetTagName() == "handle" )
		{
			handle = current;
			break;
		}
	}
	if ( !handle ) return false;

	const auto targetId = handle->GetAttribute<Rml::String>( "move_target", "" );
	if ( targetId.empty() || !handle->GetOwnerDocument() ) return false;
	auto* target = handle->GetOwnerDocument()->GetElementById( targetId );
	if ( !target || !target->GetParentNode() || !target->IsVisible( true ) ) return false;

	const auto offset = target->GetRelativeOffset( Rml::BoxArea::Border );
	// Draggable windows may be initially docked with right/bottom RCSS insets.
	// Once the user grabs the title bar, convert that docked position into an
	// explicit top-left position. Otherwise a later layout pass reapplies the
	// right/bottom anchor and makes SetOffset appear to have no effect.
	target->SetProperty( "right", "auto" );
	target->SetProperty( "bottom", "auto" );
	target->SetProperty( "left", std::to_string( offset.x ) + "px" );
	target->SetProperty( "top", std::to_string( offset.y ) + "px" );
	target->SetOffset( offset, target->GetParentNode(), false );
	m_rmlWindowDragTarget = target;
	m_rmlWindowDragParent = target->GetParentNode();
	m_rmlWindowDragStartPointer = position;
	m_rmlWindowDragStartOffset = QPointF( offset.x, offset.y );
	m_rmlWindowDragging = true;
	setCursor( Qt::ClosedHandCursor );
	return true;
}

void MainWindow::updateRmlWindowDrag( QPointF position )
{
	if ( !m_rmlWindowDragging || !m_rmlWindowDragTarget || !m_rmlWindowDragParent ) return;

	const qreal dpr = qMax<qreal>( 0.01, devicePixelRatio() );
	const QPointF delta = ( position - m_rmlWindowDragStartPointer ) * dpr;
	const auto parentSize = m_rmlWindowDragParent->GetBox().GetSize();
	const auto targetSize = m_rmlWindowDragTarget->GetBox().GetSize();
	const float maxX = qMax( 0.0f, parentSize.x - targetSize.x );
	const float maxY = qMax( 0.0f, parentSize.y - targetSize.y );
	const float x = qBound( 0.0f, static_cast<float>( m_rmlWindowDragStartOffset.x() + delta.x() ), maxX );
	const float y = qBound( 0.0f, static_cast<float>( m_rmlWindowDragStartOffset.y() + delta.y() ), maxY );
	m_rmlWindowDragTarget->SetProperty( "left", std::to_string( x ) + "px" );
	m_rmlWindowDragTarget->SetProperty( "top", std::to_string( y ) + "px" );
	m_rmlWindowDragTarget->SetOffset( Rml::Vector2f( x, y ), m_rmlWindowDragParent, false );
	redraw();
}

void MainWindow::endRmlWindowDrag()
{
	if ( !m_rmlWindowDragging ) return;
	m_rmlWindowDragging = false;
	m_rmlWindowDragTarget = nullptr;
	m_rmlWindowDragParent = nullptr;
	setCursor( Qt::ArrowCursor );
}

bool MainWindow::reloadRmlUiDocuments()
{
	if ( !rmlUiActive() || QOpenGLContext::currentContext() != m_context ) return false;
	bool success = true;
	const auto record = [&]( bool result ) { success = result && success; };
	m_rmlUiHost->setSystemWindow( this );
	if ( m_shellBinding ) record( m_shellBinding->reloadDocuments() );
	if ( m_hudBinding ) record( m_hudBinding->reloadDocument() );
	if ( m_inspectorBinding && m_inspectorController ) record( m_inspectorBinding->reloadDocument( *m_inspectorController, true ) );
	if ( m_management6bBinding ) record( m_management6bBinding->reloadDocuments() );
	if ( m_management6a && m_management6a->binding() ) record( m_management6a->binding()->reloadDocuments() );
	if ( m_management6cBinding ) record( m_management6cBinding->reloadDocuments() );
#if defined( INGNOMIA_UI_DESIGNER )
	if ( m_uiDesigner ) record( m_uiDesigner->reloadDocument() );
#endif
#if defined( INGNOMIA_DEVELOPER_UI )
	if ( m_debugBinding ) record( m_debugBinding->reloadDocument() );
#endif
	for ( auto& window : m_creatureInspectorWindows )
		if ( window.nativeWindow && window.binding && window.controller )
		{
			m_rmlUiHost->setSystemWindow( window.nativeWindow.get() );
			record( window.binding->reloadDocument( *window.controller, true ) );
		#if defined( INGNOMIA_UI_DESIGNER )
			if ( window.designer ) record( window.designer->reloadDocument() );
		#endif
		}
	for (auto* window : {m_management6aWindow.get(), m_stockpileWindow.get(), m_agricultureWindow.get()})
	{
        if (!window || !window->nativeWindow || !window->binding) continue;
		m_rmlUiHost->setSystemWindow( window->nativeWindow.get() );
		record( window->binding->reloadDocuments() );
	#if defined( INGNOMIA_UI_DESIGNER )
		if ( window->designer ) record( window->designer->reloadDocument() );
	#endif
	}
	for(auto* window : {m_management6bWindow.get(), m_inventoryWindow.get()})
	{
        if(!window || !window->nativeWindow || !window->binding) continue;
		m_rmlUiHost->setSystemWindow( window->nativeWindow.get() );
		record( window->binding->reloadDocuments() );
	#if defined( INGNOMIA_UI_DESIGNER )
		if ( window->designer ) record( window->designer->reloadDocument() );
	#endif
	}
	for(auto* window : {m_management6cWindow.get(), m_diplomacyWindow.get()})
	{
        if(!window || !window->nativeWindow || !window->binding) continue;
		m_rmlUiHost->setSystemWindow( window->nativeWindow.get() );
		record( window->binding->reloadDocuments() );
	#if defined( INGNOMIA_UI_DESIGNER )
		if ( window->designer ) record( window->designer->reloadDocument() );
	#endif
	}
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->nativeWindow && window->binding )
		{
			m_rmlUiHost->setSystemWindow( window->nativeWindow.get() );
			record( window->binding->reloadDocument() );
		#if defined( INGNOMIA_UI_DESIGNER )
			if ( window->designer ) record( window->designer->reloadDocument() );
		#endif
		}
	m_rmlUiHost->setSystemWindow( this );
	redraw();
	return success;
}

bool

MainWindow::rmlUiActive() const

{

	return m_rmlUiHost && m_rmlUiHost->initialized();
}

/// @brief Calculates the gameplay frame interval from the active monitor or manual cap.
/// @return Timer interval in milliseconds; menu rendering remains fixed at 60 FPS.
int MainWindow::frameTimerIntervalMs() const
{
	if( !m_renderer || m_renderer->isInMenu() ) return 16;

	const bool followMonitor = Global::cfg->get( "followMonitorRefresh" ).toBool();
	double framesPerSecond = 60.0;
	if( followMonitor )
	{
		if( const auto* activeScreen = screen(); activeScreen && activeScreen->refreshRate() > 1.0 )
			framesPerSecond = activeScreen->refreshRate();
	}
	else
	{
		framesPerSecond = qBound( 30, Global::cfg->get( "frameRateLimit" ).toInt(), 240 );
	}

	return qMax( 1, qRound( 1000.0 / framesPerSecond ) );
}

/// @brief Restarts the precise frame timer after a monitor or settings change.
void MainWindow::restartFrameTimer()
{
	if( !m_timer ) return;
	m_timer->setTimerType( Qt::PreciseTimer );
	m_timer->start( frameTimerIntervalMs() );
}

/// @brief Menu-mode frame tick (16 ms via m_timer). Requests an UpdateRequest so the

///        window repaints while the simulation is idle.

void

MainWindow::idleRenderTick()

{ // Check for ongoing keyboard movement
	keyboardMove();
	if ( !m_pendingUpdate )
	{

		m_pendingUpdate = true;

		requestUpdate();
	}
}

/// @brief Qt event loop dispatch: handles QEvent::UpdateRequest by calling paintGL(), else

///        delegates to the base QWindow.

/// @param event Incoming Qt event.

/// @return true if the event was handled.

bool

MainWindow::event( QEvent* event )

{

	if ( rmlUiActive() && event->type() == QEvent::InputMethod )

	{

		auto* inputEvent = static_cast<QInputMethodEvent*>( event );

		m_uiCompositionActive = !inputEvent->preeditString().isEmpty();

		if ( !inputEvent->commitString().isEmpty() && m_rmlUiHost->input().committedText( inputEvent->commitString() ).uiConsumed )

		{

			event->accept();

			redraw();

			return true;
		}
	}
#if QT_VERSION >= QT_VERSION_CHECK( 6, 6, 0 )
	if ( rmlUiActive() && event->type() == QEvent::DevicePixelRatioChange )
	{
		resizeRmlUi();
		redraw();
	}
#endif

	if ( event->type() == QEvent::UpdateRequest )

	{

		if ( isExposed() && m_glInitialized )

		{

			paintGL();
		}

		return true;
	}

	return QWindow::event( event );
}

/// @brief Qt expose override: first expose triggers lazy GL context creation and initializeGL().

/// @param event Incoming expose event.

void

MainWindow::exposeEvent( QExposeEvent* event )

{

	Q_UNUSED( event );

	if ( isExposed() )

	{

		if ( !m_glInitialized )

		{

			initializeGL();

			m_glInitialized = true;
		}

		paintGL();
	}
}

/// @brief Qt resize override: forwards the new size to resizeGL() once GL is initialised.

/// @param event Incoming resize event.

void

MainWindow::resizeEvent( QResizeEvent* event )

{

	QWindow::resizeEvent( event );

	if ( m_glInitialized )

	{

		resizeGL( event->size().width(), event->size().height() );
	}
}

/// @brief Main paint function. Makes the GL context current, ticks the RmlUi host, asks

///        the MainWindowRenderer to draw the world, composites the RmlUi view on top, and

///        swaps buffers.

void

MainWindow::paintGL()

{

	if ( !m_context )

		return;

	if ( !m_context->makeCurrent( this ) )
	{
		// A detached native window can briefly own the thread's GL context while
		// it is being closed. Do not issue world/UI GL calls against no context;
		// retry the complete frame after Qt finishes the surface transition.
		qWarning() << "Main OpenGL context was unavailable for a frame; retrying";
		m_pendingUpdate = false;
		QTimer::singleShot( 50, this, [this] { redraw(); } );
		return;
	}
	// Detached inspectors use a separate Qt GL context but RmlUi's system
	// interface is process-global. Reassert the primary window at the frame
	// boundary so the HUD cannot inherit an auxiliary window's services or
	// dimensions after an inspector event.
	if ( m_rmlUiHost ) m_rmlUiHost->setSystemWindow( this );
	keyboardMove(); // Apply latest position
	// Get the GPU busy

	m_renderer->paintWorld();
	// The world pass owns its own framebuffer/viewport. Restore the primary
	// surface before RmlUi composes the HUD. Offscreen inspector camera passes
	// are intentionally scheduled after this composition below: even a small
	// driver-specific state leak from an auxiliary framebuffer must never be
	// able to change which compiled HUD text is drawn into the primary surface.
	const int primaryPixelWidth = qMax( 1, qRound( width() * devicePixelRatio() ) );
	const int primaryPixelHeight = qMax( 1, qRound( height() * devicePixelRatio() ) );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glViewport( 0, 0, primaryPixelWidth, primaryPixelHeight );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_STENCIL_TEST );
	// Start the primary RmlUi pass from a neutral GL binding boundary. The
	// world and preview passes use their own VAO/program/SSBO state; letting
	// that state flow into RmlUi makes its text batches consume a stale index
	// buffer on some drivers, which is visible as top-rail captions replacing
	// sidebar button labels after a creature is selected.
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glUseProgram( 0 );
	glActiveTexture( GL_TEXTURE0 );
	if ( m_windowFrame ) m_windowFrame->sync();
	if ( m_rmlUiHost ) (void)m_rmlUiHost->processHotReload();
    const bool uiUpdated = m_rmlUiHost && m_rmlUiHost->update();
	const bool uiRendered = uiUpdated && m_rmlUiHost->render();
	static bool uiFailureReported = false;
	if ( !uiRendered )
	{
		if ( !uiFailureReported )
		{
			uiFailureReported = true;
			qCritical() << "RmlUi production frame failed"
				<< "updated" << uiUpdated
				<< "mainContextCurrent" << ( QOpenGLContext::currentContext() == m_context )
				<< "rendererInMenu" << ( m_renderer && m_renderer->isInMenu() );
		}
		// Do not swap the fallback clear over a valid front buffer when the
		// detached-window handoff temporarily prevents the primary UI from
		// rendering. Keep the last complete frame visible and retry after the
		// platform has finished restoring the owner surface.
		m_pendingUpdate = false;
		QTimer::singleShot( 25, this, [this] { redraw(); } );
		return;
	}
	uiFailureReported = false;
	++m_uiFrameCount;
	const auto captureFrame = std::max<std::uint32_t>( 1u, qEnvironmentVariable( "INGNOMIA_UI_CAPTURE_FRAME", "1" ).toUInt() );
	if ( !m_uiCaptureDone && m_uiFrameCount >= captureFrame && qEnvironmentVariableIsSet( "INGNOMIA_UI_CAPTURE" ) )
	{
		const auto output = qEnvironmentVariable( "INGNOMIA_UI_CAPTURE" );
		const int pixelWidth = qMax( 1, qRound( width() * devicePixelRatio() ) );
		const int pixelHeight = qMax( 1, qRound( height() * devicePixelRatio() ) );
		std::vector<unsigned char> pixels( static_cast<std::size_t>( pixelWidth ) * static_cast<std::size_t>( pixelHeight ) * 4 );
		glReadPixels( 0, 0, pixelWidth, pixelHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
		QImage frame( pixels.data(), pixelWidth, pixelHeight, QImage::Format_RGBA8888 );
		if ( frame.mirrored( false, true ).save( output ) ) qInfo() << "UI capture saved:" << output;
		else qWarning() << "UI capture failed:" << output;
		m_uiCaptureDone = true;
	}

	// Update the private inspector camera textures only after the primary HUD
	// has been completely rendered and captured. Detached windows sample the
	// last completed texture, so this one-frame latency is preferable to
	// allowing the preview's offscreen GL work to share a live draw boundary
	// with the main RmlUi document.
	MainWindowRenderer::CameraPreviewTarget previewTarget;
	const MainWindowRenderer::CameraPreviewTarget* previewTargetPtr = nullptr;
	if ( m_inspectorController && m_inspectorController->state().kind == ingnomia::ui::inspector::InspectorKind::Creature &&
		m_inspectorController->state().creature && !m_inspectorController->state().creatureDetailsOpen &&
		m_inspectorController->state().selected && m_inspectorController->state().selected->position )
	{
		const auto& position = *m_inspectorController->state().selected->position;
		previewTarget.position = Position( position.x, position.y, position.z );
		previewTarget.creatureID = m_inspectorController->state().selected->id;
		previewTargetPtr = &previewTarget;
	}
	m_renderer->paintCameraPreview( previewTargetPtr, 0 );
	for ( auto& window : m_creatureInspectorWindows )
	{
		if ( !window.controller || window.controller->state().kind != ingnomia::ui::inspector::InspectorKind::Creature ||
			!window.controller->state().creature || window.controller->state().creatureDetailsOpen ||
			!window.controller->state().selected || !window.controller->state().selected->position )
			continue;
		const auto& position = *window.controller->state().selected->position;
		const MainWindowRenderer::CameraPreviewTarget target {
			Position( position.x, position.y, position.z ),
			window.controller->state().selected->id
		};
		m_renderer->paintCameraPreview( &target, window.slot );
	}

	m_context->swapBuffers( this ); // Use slower tick rate in menu (no game world to render)
	restartFrameTimer();
	m_pendingUpdate = false;
}

/// @brief GL-side resize: updates the viewport, forwards the new size to RmlUi, and

///        calls MainWindowRenderer::onResize to rebuild framebuffers as needed.

/// @param w New width in pixels.

/// @param h New height in pixels.

void

MainWindow::resizeGL( int w, int h )

{

	if ( !m_isFullScreen )

	{

		Global::cfg->set( "WindowWidth", w );

		Global::cfg->set( "WindowHeight", h );
	}

	makeCurrent();
	m_renderer->resize( this->width(), this->height() );

	glViewport( 0, 0, this->width() * devicePixelRatio(), this->height() * devicePixelRatio() );

	resizeRmlUi();

	emit signalWindowSize( this->width(), this->height() );

	requestUpdate();
}

/// @brief Slot: explicit size set from the debug window.

/// @param width  New width in pixels.

/// @param height New height in pixels.

void

MainWindow::onSetWindowSize( int width, int height )

{

	this->resize( width, height );
}

void

MainWindow::onUiSetViewLevel( int level )

{

	// The loaded world owns the authoritative z extent.  The legacy config may
	// not contain dimensionZ (especially for older saves), in which case its
	// default is zero and would clamp every requested level back to level 0.
	const int configuredDimZ = Global::cfg->get( "dimensionZ" ).toInt();
	const int dimZ = Global::dimZ > 0 ? Global::dimZ : configuredDimZ;

	level = qBound( 0, level, qMax( 0, dimZ - 1 ) );

	if ( m_renderer )

		m_renderer->setViewLevel( level );
	else
		GameState::viewLevel = level;

	emit signalViewLevel( GameState::viewLevel );

	pushRenderParams();

	redraw();
}

/// @brief Requests a redraw by queuing a QEvent::UpdateRequest on the window. Coalesces

///        multiple requests within one frame using m_pendingUpdate.

void

MainWindow::redraw()

{

	if ( !m_pendingUpdate )

	{ // Trigger rendering

		m_pendingUpdate = true;

		requestUpdate();
	}
}

/// @brief One-time GL bring-up: creates the QOpenGLContext, loads GLAD function pointers,

///        constructs the MainWindowRenderer, initialises the RmlUi host, and

///        starts the idle frame timer.

void

MainWindow::initializeGL()

{ // Create and initialize OpenGL context
	m_context = new QOpenGLContext( this );
	m_context->setFormat( requestedFormat() );
	if ( !m_context->create() )
	{

		qCritical() << "Failed to create OpenGL context";

		return;
	}
	makeCurrent();
	// Initialize GLAD
	auto gladLoader = []( const char* name ) -> GLADapiproc
	{
		return reinterpret_cast<GLADapiproc>( QOpenGLContext::currentContext()->getProcAddress( name ) );
	};
	if ( !gladLoadGL( gladLoader ) )
	{

		qCritical() << "Failed to initialize GLAD";

		return;
	}
	m_renderer = new MainWindowRenderer( this );
	m_renderer->initializeGL();
	if ( !initializeRmlUi() )
		qFatal( "RmlUi MainWindow initialization failed" );
	// Trigger initial resize — QOpenGLWindow did this automatically, QWindow does not
	resizeGL( width(), height() );
	m_timer = new QTimer( this );
	m_timer->setTimerType( Qt::PreciseTimer );
	connect( m_timer, &QTimer::timeout, this, &MainWindow::idleRenderTick );
	requestUpdate();
}

/// @brief Returns the owned MainWindowRenderer pointer.

/// @return The renderer, or nullptr if initializeGL has not run yet.
MainWindowRenderer*

MainWindow::renderer()

{

	return m_renderer;
}


bool MainWindow::showManagementWorkshopFixture()
{
 using namespace ingnomia::ui;using namespace ingnomia::ui::management6a;
 if(!m_management6a || !m_management6a->controller(ManagementView::Workshop))return false;
 m_management6a->beginWorld(WorldEpoch{0xC0DEu});
 auto* controller=m_management6a->controller(ManagementView::Workshop);
 WorkshopSnapshot ws;ws.id={100};ws.name="Carpenter";ws.maxPriority=5;ws.canLinkStockpile=true;
 ws.stockpiles={{{1},"Raw wood",true},{{2},"Building materials",false},{{3},"Supplies",false}};
 WorkshopProductRow product;product.id=CatalogId{"Plank"};WorkshopComponentRow component;component.item=CatalogId{"RawWood"};component.amount=1;component.materials={{CatalogId{"Oak"},12},{CatalogId{"Pine"},0}};product.components={component};ws.products={product};
 CraftQueueRow job;job.id={101};job.craft=product.id;job.count=5;job.materials={CatalogId{"Oak"}};ws.queue={job};job.id={102};job.mode=CraftRepeatMode::Maintain;job.count=20;ws.queue.push_back(job);
 const auto mode=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGE10");
 if(mode=="trade" || mode=="review") {ws.subtype="TradingPost";ws.name="Trading post";}
 if(mode=="butcher")ws.subtype="Butcher";
 if(mode=="fisher")ws.subtype="Fisher";
 controller->showWorkshop(ws,Revision{1},WorldPosition{50,50,92});
 if(!ensureDetachedManagement6A(static_cast<int>(ManagementView::Workshop)))return false;
 QTimer::singleShot(250,this,[this,mode,controller]{
  if(mode=="settings" || mode=="butcher" || mode=="fisher")controller->setWorkshopPane(WorkshopPane::Settings);
  if(mode=="queue"){controller->setWorkshopPane(WorkshopPane::Queue);controller->selectWorkshopJob({102});}
  if(mode=="trade" || mode=="review") {
   controller->setWorkshopPane(WorkshopPane::Trade);
   TradeRow seller{{TradeParty::Trader,CatalogId{"Plank"},CatalogId{"Oak"},2},"Oak plank",12,2,10};
   TradeRow buyer{{TradeParty::Player,CatalogId{"RawWood"},CatalogId{"Pine"},2},"Pine logs",20,5,5};
   controller->setTradeSnapshot(WorkshopId{100},42,1,{seller},{buyer},20,25);controller->selectTradeRow(buyer.id);
   if(mode=="review")controller->executeTrade();
  }
  requestManagementCaptureForProbe("workshop",qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH"));
 });
 return true;
}
