/*
This file is part of Ingnomia https://github.com/rschurade/Ingnomia    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team    This program is free software: you can redistribute it and/or modify    it under the terms of the GNU Affero General Public License as    published by the Free Software Foundation, either version 3 of the    License, or (at your option) any later version.    This program is distributed in the hope that it will be useful,    but WITHOUT ANY WARRANTY; without even the implied warranty of    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    GNU Affero General Public License for more details.    You should have received a copy of the GNU Affero General Public License    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/

#include "mainwindow.h"

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
	return m_management6a->controller(ManagementView::Stockpile)->state().view == ManagementView::Stockpile;
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
	m_management6bController->applyInventory( { world, Revision { 1 }, std::move( rows ) } );
	m_management6bController->setInventoryCategory( "food" );
	m_management6bController->toggleInventoryExpanded( vegetableGroupId );
	m_management6bController->toggleInventoryExpanded( vegetableItem.id );
	return true;
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
bool MainWindow::clickManagementFarmCropForProbe( std::string_view crop )
{
	if ( !m_agricultureWindow || !m_agricultureWindow->nativeWindow || !m_agricultureWindow->binding ) return false;
	auto* document = m_agricultureWindow->binding->agricultureDocument();
	if ( !document || !document->IsVisible() ) return false;
	const auto id = std::string( "agriculture_farm_crop_" ) + QByteArray( crop.data(), static_cast<qsizetype>( crop.size() ) ).toHex().toStdString();
	auto* element = document->GetElementById( id );
	if ( !element || !element->IsVisible( true ) ) return false;
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
	const auto& selected = m_management6a->controller( ingnomia::ui::management6a::ManagementView::Agriculture )->state().agriculture.selectedProduct;
	return selected ? selected->value : std::string {};
}
bool MainWindow::clickInventoryDetailForProbe( std::string_view target )
{
	if ( !m_inventoryWindow || !m_inventoryWindow->nativeWindow || !m_inventoryWindow->binding || !m_inventoryWindow->context ) return false;
	auto* document = m_inventoryWindow->binding->inventoryDocument();
	if ( !document || !document->IsVisible() ) return false;
	Rml::Element* element = nullptr;
	if ( target == "back" ) element = document->GetElementById( "inventory_detail_back" );
	else
	{
		const char* containerId = target == "product" || target == "product_last" ? "inventory_detail_used_in" : target == "ingredient" ? "inventory_detail_made_by" : "inventory_detail_locations";
		const char* attribute = target == "stockpile" ? "data-stockpile-link" : target == "product" || target == "product_last" ? "data-product-link" : "data-item-link";
		if ( auto* container = document->GetElementById( containerId ) )
		{
			const auto findLink = [&]( auto&& self, Rml::Element* node ) -> Rml::Element* {
				if ( node->HasAttribute( attribute ) )
				{
					if ( target != "product_last" ) return node;
					auto* card = container->GetParentNode();
					const auto cardOffset = card->GetAbsoluteOffset( Rml::BoxArea::Border );
					const auto cardSize = card->GetBox().GetSize();
					const auto nodeOffset = node->GetAbsoluteOffset( Rml::BoxArea::Border );
					const auto nodeSize = node->GetBox().GetSize();
					const float centerY = nodeOffset.y + nodeSize.y * 0.5f;
					if ( centerY >= cardOffset.y + 4.f && centerY <= cardOffset.y + cardSize.y - 4.f ) element = node;
				}
				for ( auto* child = node->GetFirstChild(); child; child = child->GetNextSibling() )
					if ( auto* match = self( self, child ); match && target != "product_last" ) return match;
				return nullptr;
			};
			if ( target == "product_last" ) findLink( findLink, container );
			else element = findLink( findLink, container );
		}
	}
	if ( !element || !element->IsVisible( true ) ) return false;
	const auto offset = element->GetAbsoluteOffset( Rml::BoxArea::Border );
	const auto size = element->GetBox().GetSize();
	const qreal dpr = qMax<qreal>( 0.01, m_inventoryWindow->nativeWindow->devicePixelRatio() );
	const QPointF point( ( offset.x + size.x * 0.5f ) / dpr, ( offset.y + size.y * 0.5f ) / dpr );
	auto* window = m_inventoryWindow->nativeWindow.get();
	QMouseEvent move( QEvent::MouseMove, point, window->mapToGlobal( point.toPoint() ), Qt::NoButton, Qt::NoButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &move );
	QMouseEvent press( QEvent::MouseButtonPress, point, window->mapToGlobal( point.toPoint() ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &press );
	QMouseEvent release( QEvent::MouseButtonRelease, point, window->mapToGlobal( point.toPoint() ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &release );
	return true;
}
std::string MainWindow::inventoryDetailItemForProbe() const
{
	if ( !m_management6bController ) return {};
	const auto& detail = m_management6bController->state().inventoryDetail;
	return detail ? detail->item.value : std::string {};
}
int MainWindow::openLongestInventoryProductsForProbe()
{
	if ( !m_management6bController || !m_management6bController->state().inventoryOpen ) return 0;
	const auto& rows = m_management6bController->state().inventory;
	const auto row = std::ranges::max_element( rows, {}, []( const auto& value ) { return value.usedIn.size(); } );
	if ( row == rows.end() || row->usedIn.empty() ) return 0;
	m_management6bController->openInventoryDetail( row->id );
	return static_cast<int>( row->usedIn.size() );
}
bool MainWindow::scrollInventoryProductsForProbe()
{
	if ( !m_inventoryWindow || !m_inventoryWindow->nativeWindow || !m_inventoryWindow->binding ) return false;
	auto* document = m_inventoryWindow->binding->inventoryDocument();
	auto* content = document ? document->GetElementById( "inventory_detail_used_in" ) : nullptr;
	auto* card = content ? content->GetParentNode() : nullptr;
	if ( !card ) return false;
	const auto offset = card->GetAbsoluteOffset( Rml::BoxArea::Border );
	const auto size = card->GetBox().GetSize();
	const qreal dpr = qMax<qreal>( 0.01, m_inventoryWindow->nativeWindow->devicePixelRatio() );
	const QPointF point( ( offset.x + size.x * 0.5f ) / dpr, ( offset.y + size.y * 0.5f ) / dpr );
	auto* window = m_inventoryWindow->nativeWindow.get();
	QMouseEvent move( QEvent::MouseMove, point, window->mapToGlobal( point.toPoint() ), Qt::NoButton, Qt::NoButton, Qt::NoModifier );
	QCoreApplication::sendEvent( window, &move );
	QWheelEvent wheel( point, window->mapToGlobal( point.toPoint() ), QPoint {}, QPoint( 0, -960 ), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false );
	QCoreApplication::sendEvent( window, &wheel );
	return true;
}
std::string MainWindow::inventoryProductScrollStatusForProbe() const
{
	if ( !m_inventoryWindow || !m_inventoryWindow->binding ) return {};
	auto* document = m_inventoryWindow->binding->inventoryDocument();
	auto* content = document ? document->GetElementById( "inventory_detail_used_in" ) : nullptr;
	auto* card = content ? content->GetParentNode() : nullptr;
	if ( !card ) return {};
	return std::to_string( card->GetScrollTop() ) + ":" + std::to_string( card->GetScrollHeight() ) + ":" + std::to_string( card->GetClientHeight() );
}
bool MainWindow::requestManagementCaptureForProbe( std::string_view kind, const QString& path )
{
	ingnomia::ui::RmlUiDetachedWindow* native = nullptr;
	if ( kind == "population" && m_management6bWindow ) native = m_management6bWindow->nativeWindow.get();
	else if ( kind == "inventory" && m_inventoryWindow ) native = m_inventoryWindow->nativeWindow.get();
	else if ( kind == "stockpile" && m_stockpileWindow ) native = m_stockpileWindow->nativeWindow.get();
	else if ( kind == "military" && m_management6cWindow ) native = m_management6cWindow->nativeWindow.get();
	else if ( kind == "diplomacy" && m_diplomacyWindow ) native = m_diplomacyWindow->nativeWindow.get();
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
        && qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_LINKS") != "1") || !m_management6a) return {};
    const auto& state = m_management6a->controller(ingnomia::ui::management6a::ManagementView::Workshop)->state();
    const auto& value = state.workshop.value;
    return "generated=" + std::to_string(value.acceptGenerated) + " auto=" + std::to_string(value.autoCraftMissing)
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
	// Space/Pause is the global simulation toggle.  RmlUi's document-level
	// key handler can legitimately consume an otherwise unhandled space key
	// (for example when focus is on a button), which previously prevented the
	// legacy switch below from ever reaching EventConnector.  Handle the global
	// gesture at the host boundary before forwarding the key to RmlUi.
	if ( ( event->key() == Qt::Key_Space || event->key() == Qt::Key_Pause ) &&
			 !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) ) )
	{
		emit signalTogglePause();
		event->accept();
		return;
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

		ret = m_rmlUiHost->input().keyDown( qtKey, event->modifiers() ).uiConsumed;

		if ( !event->text().isEmpty() && !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) ) )

			ret = m_rmlUiHost->input().committedText( event->text() ).uiConsumed || ret;
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

	const bool uiConsumed = rmlUiActive() && m_rmlUiHost->input().keyUp( event->key(), event->modifiers() ).uiConsumed;

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
		if ( m_rmlWindowDragging )
		{
			updateRmlWindowDrag( event->position() );
			event->accept();
			return;
		}

		const auto hover = m_rmlUiHost->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
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

	config.fontFiles = { "fonts/LatoLatin-Regular.ttf" };

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


if(m_hudController)m_hudController->beginWorld(epoch);


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
				 if(m_management6bController&&m_management6bController->state().populationOpen)return;
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


if( m_hudController ) m_hudController->setSettlement( { name.toStdString(), gnomes, animals, items } ); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalHudClock, this, [this]( int minute, int hour, int day, const QString& season, int year, bool daylight, int nextSunMinute )

			 {


if( !m_hudController ) return; auto state = m_hudController->state().clock;


state.minute=static_cast<std::uint8_t>(minute); state.hour=static_cast<std::uint8_t>(hour); state.day=static_cast<std::uint16_t>(day); state.year=static_cast<std::uint16_t>(year);


const auto normalized=season.toCaseFolded(); state.season=normalized=="spring"?ingnomia::ui::Season::Spring:normalized=="summer"?ingnomia::ui::Season::Summer:normalized=="autumn"?ingnomia::ui::Season::Autumn:normalized=="winter"?ingnomia::ui::Season::Winter:ingnomia::ui::Season::Unknown;


state.daylight=daylight?ingnomia::ui::DaylightPhase::Day:ingnomia::ui::DaylightPhase::Night; state.nextSunEventMinute=static_cast<std::uint16_t>(nextSunMinute); m_hudController->setClock(state); }, Qt::QueuedConnection );

	connect( Global::eventConnector, &EventConnector::signalViewLevel, this, [this]( int level )

			 { if(m_hudController){auto s=m_hudController->state().camera;const int configuredDimZ=Global::cfg->get("dimensionZ").toInt();s.viewLevel=level;s.minLevel=0;s.maxLevel=Global::dimZ>0?Global::dimZ:configuredDimZ;m_hudController->setCamera(s);} }, Qt::QueuedConnection );

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

	qInfo() << "RmlUi MainWindow host initialized at" << config.physicalSize << "DPR/UI ratio" << config.densityIndependentPixelRatio;

	return true;
}

bool MainWindow::openDetachedOrdersTools( std::string_view elementId )
{
	if ( !ensureDetachedOrdersTools( elementId ) ) return false;
	const auto panelKey = ordersToolsPanelKeyForElement( elementId );
	for ( auto& window : m_ordersToolsWindows )
		if ( window && window->panelKey == panelKey && window->binding )
			return window->binding->activateElement( elementId );
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
	const QSize nativeSize = detachedWindowSize( sizeKey, size, userScale, this );
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
	added.nativeWindow->setResizeMinimumSize( QSize( 240, 240 ) );
	added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( nativeSize );
	const int cascade = static_cast<int>( m_ordersToolsWindows.size() - 1 ) * 32;
	const QPoint desired = position() + QPoint( 72 + cascade, 104 + cascade );
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
	const QSize minimumSize = stockpileView ? stockpileMinimumSize : standardMinimumSize;
	const QSize nativeSize = detachedWindowSize( sizeKey, stockpileView ? stockpileBaseSize : standardBaseSize, userScale, this ).expandedTo( minimumSize );
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
	if ( windowSlot && windowSlot->nativeWindow && windowSlot->binding )
	{
		windowSlot->nativeWindow->setResizable( false );
		windowSlot->nativeWindow->showAndActivate();
		return true;
	}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const QSize baseSize = inventoryView ? QSize( 720, 720 ) : QSize( 960, 640 );
	const auto sizeKey = inventoryView ? QStringLiteral( "UiWindow.population_inventory.flat_columns1" ) : QStringLiteral( "UiWindow.population.sidebar1" );
	const QSize minimumSize( 640, 420 );
	const QSize nativeSize = detachedWindowSize( sizeKey, baseSize, userScale, this ).expandedTo( minimumSize );
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
    added.nativeWindow->setCloseHandler([this, binding, inventoryView] {
        if(inventoryView) binding->closeInventory(); else binding->closePopulation();
        hideDetachedManagement6B(inventoryView ? 1 : 0);
    });
	added.nativeWindow->setResizeMinimumSize( minimumSize );
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
	if ( windowSlot && windowSlot->nativeWindow && windowSlot->binding )
	{
		windowSlot->nativeWindow->showAndActivate();
		return true;
	}
	if ( QOpenGLContext::currentContext() != m_context && !m_context->makeCurrent( this ) ) return false;

	const float userScale = qMax( 0.5f, Global::cfg->get( "uiscale" ).toFloat() );
	const QSize baseSize( 960, 640 );
	const auto sizeKey = (diplomacyView ? QStringLiteral("UiWindow.diplomacy") : QStringLiteral("UiWindow.military_diplomacy"));
	const QSize nativeSize = detachedWindowSize( sizeKey, baseSize, userScale, this );
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
	added.nativeWindow->setResizeMinimumSize( QSize( 720, 460 ) );
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
	const auto inspectorSizeKey = QStringLiteral( "UiWindow.creature_inspector.workbench.compact" );
	const QSize inspectorSize = detachedWindowSize( inspectorSizeKey,
		QSize( 508, 405 ), userScale, this );
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
		window.nativeWindow->setTitle( QStringLiteral( "Gnome inspector - " ) + QString::fromStdString( creature.name ) );
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
		window.nativeWindow->resetView( detachedWindowSize( inspectorSizeKey,
			QSize( 508, 405 ), userScale, this ) );
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

	const QString name = QStringLiteral( "Gnome inspector" );
	auto nativeWindow = std::make_unique<ingnomia::ui::RmlUiDetachedWindow>( *m_rmlUiHost, m_context,
		name + QStringLiteral( " - " ) + QString::fromStdString( creature.name ), inspectorSize );
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
	added.nativeWindow->setResizeMinimumSize( QSize( 508, 405 ) );
	added.nativeWindow->setResizeHandler( [inspectorSizeKey]( QSize resized ) { persistDetachedWindowSize( inspectorSizeKey, resized ); } );
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

	const QPoint desired = this->position() + QPoint( 48 + ( slot - 1 ) * 28, 80 + ( slot - 1 ) * 28 );
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
	const auto sizeKey = QStringLiteral( "UiWindow.live_tile_inspector" );
	const QSize inspectorSize = detachedWindowSize( sizeKey, QSize( 500, 280 ), userScale, this );
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
		QStringLiteral( "Tile inspection" ), inspectorSize );
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
	added.nativeWindow->setResizeMinimumSize( QSize( 440, 260 ) );
	added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( inspectorSize );
	added.nativeWindow->setCloseHandler( [this]
		{ QTimer::singleShot( 0, this, [this] { setTileInspection( false ); } ); } );
	const QPoint desired = this->position() + QPoint( 184, 64 );
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
	const auto sizeKey = QStringLiteral( "UiWindow.blueprint_inspector.compact" );
	const QSize inspectorSize = detachedWindowSize( sizeKey, QSize( 400, 430 ), userScale, this );
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
		QStringLiteral( "Construction blueprint" ), inspectorSize );
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
	added.nativeWindow->setResizeMinimumSize( QSize( 360, 360 ) );
	added.nativeWindow->setResizeHandler( [sizeKey]( QSize resized ) { persistDetachedWindowSize( sizeKey, resized ); } );
	added.nativeWindow->resetView( inspectorSize );
	added.nativeWindow->setCloseHandler( [this]
		{
			if ( const auto it = std::ranges::find_if( m_creatureInspectorWindows,
				[]( const CreatureInspectorWindow& value ) { return value.slot == -1; } );
				it != m_creatureInspectorWindows.end() && it->controller ) it->controller->close();
			QTimer::singleShot( 0, this, [this] { closeDetachedCreatureInspector( -1 ); } );
		} );
	const QPoint desired = this->position() + QPoint( 48, 80 );
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
