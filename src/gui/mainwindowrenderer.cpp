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
/** @file mainwindowrenderer.cpp
 *  @brief MainWindowRenderer implementation: shader/texture/VBO setup, per-frame draw
 *         ordering (compute tile update → tile draw → selection → thought bubbles → axles),
 *         and camera/view-level controls.
 */
#include "mainwindowrenderer.h"

#include "../game/game.h" //TODO only temporary

#include "../base/config.h"
#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/util.h"
#include "../base/vptr.h"
#include "../game/gamemanager.h"
#include "../game/plant.h"
#include "../game/world.h"
#include "../gfx/sprite.h"
#include "../gfx/spritefactory.h"
#include "eventconnector.h"
#include "mainwindow.h"
#include "aggregatorselection.h"

#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QTimer>
#include <QOpenGLContext>
#include <QTextStream>
#include <QVector3D>

#include <glad/gl.h>

#include <cstring>
#include <cmath>
#include <unordered_map>

namespace
{
void traceRender( const QString& message )
{
	const QString path = qEnvironmentVariable( "INGNOMIA_LOAD_TRACE_PATH" );
	if ( path.isEmpty() )
		return;
	QFile file( path );
	if ( file.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
	{
		QTextStream stream( &file );
		stream << message << Qt::endl;
	}
}

/// @brief RAII helper that opens a GL debug-output group on construction and pops it on
///        destruction, so debug tools like RenderDoc show hierarchical scopes.
class DebugScope
{
public:
	DebugScope() = delete;
	DebugScope( const char* c )
	{
		static GLuint counter = 0;
		glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, counter++, static_cast<GLsizei>( strlen( c ) ), c );
	}
	~DebugScope()
	{
		glPopDebugGroup();
	}
};
} // namespace

/// @brief Constructs the renderer and wires up the signal chain with AggregatorRenderer,
///        EventConnector, and AggregatorSelection. initializeGL() must be called after the
///        parent's GL context is current.
/// @param parent Owning MainWindow.
MainWindowRenderer::MainWindowRenderer( MainWindow* parent ) :
	QObject( parent ),
	m_parent( parent )
{
	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalWorldParametersChanged, this, &MainWindowRenderer::cleanupWorld );

	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalSimulationTick, this, &MainWindowRenderer::onSimulationTick );
	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalTileUpdates, this, &MainWindowRenderer::onTileUpdates );
	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalAxleData, this, &MainWindowRenderer::onAxelData );
	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalThoughtBubbles, this, &MainWindowRenderer::onThoughtBubbles );
	connect( Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::signalCenterCamera, this, &MainWindowRenderer::onCenterCameraPosition );
	connect( Global::eventConnector, &EventConnector::signalInMenu, this, &MainWindowRenderer::onSetInMenu );

	connect( Global::eventConnector->aggregatorSelection(), &AggregatorSelection::signalUpdateSelection, this, &MainWindowRenderer::onUpdateSelection, Qt::QueuedConnection );

	// Full polling of initial state on load
	connect( this, &MainWindowRenderer::fullDataRequired, Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::onAllTileInfo );
	connect( this, &MainWindowRenderer::fullDataRequired, Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::onThoughtBubbleUpdate );
	connect( this, &MainWindowRenderer::fullDataRequired, Global::eventConnector->aggregatorRenderer(), &AggregatorRenderer::onAxleDataUpdate );

	connect( this, &MainWindowRenderer::signalCameraPosition, Global::eventConnector, &EventConnector::onCameraPosition );

	qDebug() << "initialize GL ...";
	connect( m_parent->context(), &QOpenGLContext::aboutToBeDestroyed, this, &MainWindowRenderer::cleanup );
	connect( this, &MainWindowRenderer::redrawRequired, m_parent, &MainWindow::redraw );
}

/// @brief Destructor.
MainWindowRenderer ::~MainWindowRenderer()
{
}

/// @brief One-time GL resource setup: installs a debug log handler, builds the shared
///        vertex/index buffers for the tile quad, compiles shaders, and creates the sprite
///        atlas array textures.
void MainWindowRenderer::initializeGL()
{
	qDebug() << "[OpenGL]" << reinterpret_cast<char const*>( glGetString( GL_VENDOR ) );
	qDebug() << "[OpenGL]" << reinterpret_cast<char const*>( glGetString( GL_VERSION ) );
	qDebug() << "[OpenGL]" << reinterpret_cast<char const*>( glGetString( GL_RENDERER ) );

	qDebug() << m_parent->context()->format();

	GLDEBUGPROC logHandler = []( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam ) -> void
	{
		static const std::unordered_map<GLenum, const char*> debugTypes = {
			{ GL_DEBUG_TYPE_ERROR, "Error" },
			{ GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DeprecatedBehavior" },
			{ GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "UndefinedBehavior" },
			{ GL_DEBUG_TYPE_PORTABILITY, "Portability" },
			{ GL_DEBUG_TYPE_PERFORMANCE, "Performance" },
			{ GL_DEBUG_TYPE_MARKER, "Marker" },
			{ GL_DEBUG_TYPE_OTHER, "Other" },
			{ GL_DEBUG_TYPE_PUSH_GROUP, "Push" },
			{ GL_DEBUG_TYPE_POP_GROUP, "Pop" }
		};
		static const std::unordered_map<GLenum, const char*> severities = {
			{ GL_DEBUG_SEVERITY_LOW, "low" },
			{ GL_DEBUG_SEVERITY_MEDIUM, "medium" },
			{ GL_DEBUG_SEVERITY_HIGH, "high" },
			{ GL_DEBUG_SEVERITY_NOTIFICATION, "notify" },
		};
		if ( severity == GL_DEBUG_SEVERITY_NOTIFICATION && !Global::debugOpenGL )
			return;
		// Only want to handle these from dedicated graphic debugger
		if ( type == GL_DEBUG_TYPE_PUSH_GROUP || type == GL_DEBUG_TYPE_POP_GROUP )
			return;
		// Rate-limit GL error messages to avoid log spam
		static int glErrorCount = 0;
		if ( type == GL_DEBUG_TYPE_ERROR )
		{
			++glErrorCount;
			if ( glErrorCount <= 5 )
			{
				qDebug() << "[OpenGL]" << debugTypes.at( type ) << " " << severities.at(severity) << ":" << message;
			}
			else if ( glErrorCount == 6 )
			{
				qDebug() << "[OpenGL] Suppressing further GL_ERROR messages";
			}
			return;
		}
		qDebug() << "[OpenGL]" << debugTypes.at( type ) << " " << severities.at(severity) << ":" << message;
	};
	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( logHandler, nullptr );

	float vertices[] = {
		// Wall layer
		0.f, .8f, 1.f, // top left
		0.f, .2f, 1.f, // bottom left
		1.f, .8f, 1.f, // top right
		1.f, .2f, 1.f, // bottom right

		// floor layer
		0.f, .5f, 0.f, // top left
		0.f, .2f, 0.f, // bottom left
		1.f, .5f, 0.f, // top right
		1.f, .2f, 0.f, // bottom right
	};

	constexpr GLushort indices[] = {
		0, 1, 3, // Wall 1
		2, 0, 3, // Wall 2
		4, 5, 7, // Floor 1
		6, 4, 7, // Floor 2
		// Wall again for BTF rendering
		0, 1, 3, // Wall 1
		2, 0, 3, // Wall 2
	};

	if ( !initShaders() )
	{
		//qCritical() << "failed to init shaders - exiting";
	}

	glGenVertexArrays( 1, &m_vao );
	glBindVertexArray( m_vao );

	glGenBuffers( 1, &m_vbo );
	glBindBuffer( GL_ARRAY_BUFFER, m_vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW );

	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof( float ), (void*)0 );
	glEnableVertexAttribArray( 0 );

	glGenBuffers( 1, &m_vibo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_vibo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW );

	glBindVertexArray( 0 );

	updateRenderParams();
	initializeCameraPreview();
	initializeWaterTargets();
	initializeWaterTextures();
	m_waterClock.start();
	m_cameraPreviewLastPaintMs.fill( -1 );

	qDebug() << "initialize GL - done";
}

/// @brief Queues a shader reload for the next paintWorld() call (debug hotkey entry point).
void MainWindowRenderer::reloadShaders()
{
	m_reloadShaders = true;
}

/// @brief Releases all GL resources (buffers, shaders, textures). Invoked when the parent
///        context is about to be destroyed.
void MainWindowRenderer::cleanup()
{
	m_parent->makeCurrent();

	if ( m_worldShader ) { glDeleteProgram( m_worldShader ); m_worldShader = 0; }
	if ( m_waterShader ) { glDeleteProgram( m_waterShader ); m_waterShader = 0; }
	if ( m_worldUpdateShader ) { glDeleteProgram( m_worldUpdateShader ); m_worldUpdateShader = 0; }
	if ( m_thoughtBubbleShader ) { glDeleteProgram( m_thoughtBubbleShader ); m_thoughtBubbleShader = 0; }
	if ( m_selectionShader ) { glDeleteProgram( m_selectionShader ); m_selectionShader = 0; }
	if ( m_axleShader ) { glDeleteProgram( m_axleShader ); m_axleShader = 0; }
	if ( m_waterDuDv ) { glDeleteTextures( 1, &m_waterDuDv ); m_waterDuDv = 0; }
	if ( m_waterNormal ) { glDeleteTextures( 1, &m_waterNormal ); m_waterNormal = 0; }
	cleanupWaterTargets();
	for ( int slot = 0; slot < cameraPreviewSlotCount; ++slot )
	{
		if ( m_cameraPreviewDepth[slot] ) { glDeleteRenderbuffers( 1, &m_cameraPreviewDepth[slot] ); m_cameraPreviewDepth[slot] = 0; }
		if ( m_cameraPreviewTexture[slot] ) { glDeleteTextures( 1, &m_cameraPreviewTexture[slot] ); m_cameraPreviewTexture[slot] = 0; }
		if ( m_cameraPreviewFbo[slot] ) { glDeleteFramebuffers( 1, &m_cameraPreviewFbo[slot] ); m_cameraPreviewFbo[slot] = 0; }
	}

	glDeleteBuffers( 1, &m_vbo );
	glDeleteBuffers( 1, &m_vibo );
	if ( m_vao ) { glDeleteVertexArrays( 1, &m_vao ); m_vao = 0; }

	m_parent->doneCurrent();

	m_reloadShaders = true;
}

/// @brief Releases per-world GL resources (tile buffer objects) so a new world can be
///        loaded without leaking GPU memory. Triggered by signalWorldParametersChanged.
void MainWindowRenderer::cleanupWorld()
{
	m_parent->makeCurrent();
	glDeleteTextures( 32, m_textures );
	memset( m_textures, 0, sizeof( m_textures ) );
	glDeleteBuffers( 1, &m_tileBo );
	m_tileBo = 0;
	glDeleteBuffers( 1, &m_tileUpdateBo );
	m_tileUpdateBo     = 0;
	m_texesInitialized = false;

	m_parent->doneCurrent();

	m_pendingUpdates.clear();
	m_creatureCameraTargets.clear();
	m_cameraPreviewLastPaintMs.fill( -1 );
	m_selectionData.clear();
	m_thoughBubbles = ThoughtBubbleInfo();
	m_axleData      = AxleDataInfo();
	m_creatureMotionClock.invalidate();
	m_lastCreatureMotionStartMs = -1;
	m_creatureMotionIntervalMs = 50;
	m_creatureInterpolation = 1.0f;
	m_lastCreatureMotionTick = 0;
	m_creatureRenderTick = 0.0f;
}

/// @brief Advances the render interpolation clock at every fixed simulation boundary.
/// @param simulationTick Authoritative tick after the simulation step completed.
void MainWindowRenderer::onSimulationTick( quint64 simulationTick )
{
	// OpenRCT2 derives a render alpha from every completed fixed simulation tick,
	// then draws all tracked entities at that same point between their pre/post
	// snapshots. Tile changes are sparse in Ingnomia, so they cannot be the clock:
	// skipping quiet ticks makes the fractional render position jump when the next
	// path step dirties a tile.
	const bool isNewSimulationTick = simulationTick != 0
		&& simulationTick != m_lastCreatureMotionTick;
	if ( isNewSimulationTick )
	{
		if ( !m_creatureMotionClock.isValid() )
			m_creatureMotionClock.start();

		const qint64 now = m_creatureMotionClock.elapsed();
		if ( m_lastCreatureMotionStartMs >= 0 )
		{
			const qint64 elapsed = now - m_lastCreatureMotionStartMs;
			const quint64 tickDelta = simulationTick > m_lastCreatureMotionTick
				? simulationTick - m_lastCreatureMotionTick : 1;
			const qint64 interval = elapsed / static_cast<qint64>( tickDelta );
			// Track normal simulation cadence, but ignore long pauses and reload gaps.
			if ( interval >= 1 && interval <= 250 )
				m_creatureMotionIntervalMs = interval;
		}
		m_lastCreatureMotionStartMs = now;
		m_creatureInterpolation = 0.0f;
		m_lastCreatureMotionTick = simulationTick;
	}
	emit redrawRequired();
}

/// @brief Returns the sun's continuous visibility for the current in-game minute.
///        The simulation still uses GameState::daylight for gameplay decisions; this
///        curve is render-only so sunrise and sunset do not pop between two states.
static float scheduledDaylight()
{
	const int sunrise = GameState::sunrise;
	const int sunset  = GameState::sunset;
	if ( sunset <= sunrise )
		return GameState::daylight ? 1.0f : 0.0f;

	constexpr float twilightMinutes = 90.0f;
	const float current = static_cast<float>( GameState::hour * Global::util->minutesPerHour + GameState::minute );
	const auto smooth = []( float value ) {
		const float t = std::clamp( value, 0.0f, 1.0f );
		return t * t * ( 3.0f - 2.0f * t );
	};

	if ( current < static_cast<float>( sunrise ) - twilightMinutes )
		return 0.0f;
	if ( current < static_cast<float>( sunrise ) + twilightMinutes )
		return smooth( ( current - ( static_cast<float>( sunrise ) - twilightMinutes ) ) / ( 2.0f * twilightMinutes ) );
	if ( current < static_cast<float>( sunset ) - twilightMinutes )
		return 1.0f;
	if ( current < static_cast<float>( sunset ) + twilightMinutes )
		return 1.0f - smooth( ( current - ( static_cast<float>( sunset ) - twilightMinutes ) ) / ( 2.0f * twilightMinutes ) );
	return 0.0f;
}

/// @brief Slot: enqueues an incoming tile-update batch to be uploaded and applied next frame.
/// @param updates Batch of per-tile TileDataUpdate packets.
void MainWindowRenderer::onTileUpdates( const TileDataUpdateInfo& updates )
{
	static int traceBatches = 0;
	if ( traceBatches < 4 )
		traceRender( QString( "tile batch %1 size=%2" ).arg( ++traceBatches ).arg( updates.updates.size() ) );

	m_creatureCameraTargets = updates.creatureCameraTargets;
	m_pendingUpdates.push_back( updates.updates );
	emit redrawRequired();
}

/// @brief Slot: stores the current set of thought bubbles for the next paint.
/// @param bubbles New thought bubble batch.
void MainWindowRenderer::onThoughtBubbles( const ThoughtBubbleInfo& bubbles )
{
	m_thoughBubbles = bubbles;
	emit redrawRequired();
}

/// @brief Slot: stores the current axle power data for the next paint.
/// @param data New axle data batch.
void MainWindowRenderer::onAxelData( const AxleDataInfo& data )
{
	m_axleData = data;
	emit redrawRequired();
}

/// @brief Reads a shader source file from content/shaders/@p name and returns its contents.
/// @param name Filename without directory.
/// @return Shader source as a QString, or empty on failure.
QString MainWindowRenderer::copyShaderToString( QString name )
{
	QFile file( Global::cfg->get( "dataPath" ).toString() + "/shaders/" + name + ".glsl" );
	file.open( QIODevice::ReadOnly );
	QTextStream in( &file );
	QString code( "" );
	while ( !in.atEnd() )
	{
		code += in.readLine();
		code += "\n";
	}

	// Expand our shared lighting helper without a driver-specific GLSL include extension.
	if ( name != "lighting" && code.contains( "#include \"lighting.glsl\"" ) )
		code.replace( "#include \"lighting.glsl\"", copyShaderToString( "lighting" ) );

	return code;
}

/// @brief Compiles and links a vertex/fragment shader pair with the given base @p name
///        (<name>.vert / <name>.frag under content/shaders).
/// @param name Shader base filename (without extension).
/// @return GL program handle, or 0 on compile/link failure.
GLuint MainWindowRenderer::initShader( QString name, QString fragmentName )
{
	QString vs = copyShaderToString( name + "_v" );
	QString fs = copyShaderToString( ( fragmentName.isEmpty() ? name : fragmentName ) + "_f" );

	// Create and compile vertex shader
	GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );
	QByteArray vsBytes = vs.toUtf8();
	const char* vsSource = vsBytes.constData();
	glShaderSource( vertexShader, 1, &vsSource, nullptr );
	glCompileShader( vertexShader );

	GLint success;
	glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &success );
	if ( !success )
	{
		char infoLog[512];
		glGetShaderInfoLog( vertexShader, 512, nullptr, infoLog );
		qCritical() << "Vertex shader compilation failed for" << name << ":" << infoLog;
		glDeleteShader( vertexShader );
		return 0;
	}

	// Create and compile fragment shader
	GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	QByteArray fsBytes = fs.toUtf8();
	const char* fsSource = fsBytes.constData();
	glShaderSource( fragmentShader, 1, &fsSource, nullptr );
	glCompileShader( fragmentShader );

	glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &success );
	if ( !success )
	{
		char infoLog[512];
		glGetShaderInfoLog( fragmentShader, 512, nullptr, infoLog );
		qCritical() << "Fragment shader compilation failed for" << name << ":" << infoLog;
		glDeleteShader( vertexShader );
		glDeleteShader( fragmentShader );
		return 0;
	}

	// Create program and link shaders
	GLuint program = glCreateProgram();
	glAttachShader( program, vertexShader );
	glAttachShader( program, fragmentShader );
	glLinkProgram( program );

	glGetProgramiv( program, GL_LINK_STATUS, &success );
	if ( !success )
	{
		char infoLog[512];
		glGetProgramInfoLog( program, 512, nullptr, infoLog );
		qCritical() << "Shader program linking failed for" << name << ":" << infoLog;
		glDeleteShader( vertexShader );
		glDeleteShader( fragmentShader );
		glDeleteProgram( program );
		return 0;
	}

	// Clean up shaders (they're now in the program)
	glDeleteShader( vertexShader );
	glDeleteShader( fragmentShader );

	return program;
}

/// @brief Compiles and links a compute shader with the given base @p name (<name>.comp).
/// @param name Shader base filename (without extension).
/// @return GL program handle, or 0 on compile/link failure.
GLuint MainWindowRenderer::initComputeShader( QString name )
{
	QString cs = copyShaderToString( name + "_c" );

	// Create and compile compute shader
	GLuint computeShader = glCreateShader( GL_COMPUTE_SHADER );
	QByteArray csBytes = cs.toUtf8();
	const char* csSource = csBytes.constData();
	glShaderSource( computeShader, 1, &csSource, nullptr );
	glCompileShader( computeShader );

	GLint success;
	glGetShaderiv( computeShader, GL_COMPILE_STATUS, &success );
	if ( !success )
	{
		char infoLog[512];
		glGetShaderInfoLog( computeShader, 512, nullptr, infoLog );
		qCritical() << "Compute shader compilation failed for" << name << ":" << infoLog;
		glDeleteShader( computeShader );
		return 0;
	}

	// Create program and link shader
	GLuint program = glCreateProgram();
	glAttachShader( program, computeShader );
	glLinkProgram( program );

	glGetProgramiv( program, GL_LINK_STATUS, &success );
	if ( !success )
	{
		char infoLog[512];
		glGetProgramInfoLog( program, 512, nullptr, infoLog );
		qCritical() << "Compute shader program linking failed for" << name << ":" << infoLog;
		glDeleteShader( computeShader );
		glDeleteProgram( program );
		return 0;
	}

	// Clean up shader
	glDeleteShader( computeShader );

	return program;
}

/// @brief Loads and compiles every shader used by the renderer. Called at init and on a
///        debug-hotkey shader reload.
/// @return true if every shader compiled successfully.
bool MainWindowRenderer::initShaders()
{
	m_worldShader = initShader( "world" );
	m_worldUpdateShader = initComputeShader( "worldupdate" );
	m_thoughtBubbleShader = initShader( "thoughtbubble" );
	m_selectionShader = initShader( "selection" );
	m_axleShader = initShader( "axle" );
	m_waterShader = initShader( "water" );
	if ( !m_waterShader )
	{
		qWarning() << "Full water shader unavailable; using flat-water fallback";
		m_waterShader = initShader( "water", "water_flat" );
	}

	if ( !m_worldShader || !m_worldUpdateShader || !m_thoughtBubbleShader || !m_selectionShader || !m_axleShader || !m_waterShader )
	{
		// Can't proceed, and need to know what happened!
		abort();
		return false;
	}

	m_reloadShaders = false;

	return true;
}

/// @brief Allocates a new 32×64 RGBA8 array texture with @p depth slices on texture unit @p unit.
/// @param unit  GL texture unit index.
/// @param depth Number of array slices.
void MainWindowRenderer::createArrayTexture( int unit, int depth )
{
	glActiveTexture( GL_TEXTURE0 + unit );
	glGenTextures( 1, &m_textures[unit] );
	glBindTexture( GL_TEXTURE_2D_ARRAY, m_textures[unit] );
	glTexStorage3D(
		GL_TEXTURE_2D_ARRAY,
		1,             // No mipmaps
		GL_RGBA8,      // Internal format
		32, 64,        // width,height
		depth          // Number of layers
	);
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0 );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
}

/// @brief Uploads @p depth RGBA8 slices from @p data into an existing array texture.
/// @param unit  GL texture unit index.
/// @param depth Number of slices to upload.
/// @param data  Source pixel data (depth × 32 × 64 × 4 bytes).
void MainWindowRenderer::uploadArrayTexture( int unit, int depth, const uint8_t* data )
{
	glActiveTexture( GL_TEXTURE0 + unit );
	glTexSubImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,                // Mipmap number
		0, 0, 0,          // xoffset, yoffset, zoffset
		32, 64, depth,    // width, height, depth
		GL_RGBA,          // format
		GL_UNSIGNED_BYTE, // type
		data
	);
}

/// @brief Creates all sprite atlas array textures used by the renderer.
void MainWindowRenderer::initTextures()
{
	GLint max_layers;
	glGetIntegerv( GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers );

	qDebug() << "max array size: " << max_layers;
	qDebug() << "used " << Global::eventConnector->game()->sf()->size() << " sprites";

	int maxArrayTextures = Global::cfg->get( "MaxArrayTextures" ).toInt();

	for ( int i = 0; i < 32; ++i )
	{
		createArrayTexture( i, maxArrayTextures );
	}

	m_texesInitialized = true;
}

/// @brief Allocates the per-world tile data SSBO (m_tileBo) sized for the current world
///        dimensions. Called after a new world is generated or loaded.
void MainWindowRenderer::initWorld()
{
	QElapsedTimer timer;
	timer.start();

	glGenBuffers( 1, &m_tileBo );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, m_tileBo );
	glBufferData( GL_SHADER_STORAGE_BUFFER, TD_SIZE * sizeof( unsigned int ) * Global::eventConnector->game()->w()->world().size(), nullptr, GL_DYNAMIC_DRAW );
	const uint8_t zero = 0;
	glClearBufferData( GL_SHADER_STORAGE_BUFFER, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &zero );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ); // unbind

	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, m_tileBo );

	glGenBuffers( 1, &m_tileUpdateBo );

	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, m_tileBo );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, m_tileUpdateBo );

	m_texesInitialized = true;

	m_rotation = 0;

	emit fullDataRequired();
}

/// @brief Rebuilds the orthographic projection matrix and the RenderVolume bounds from
///        the current camera parameters.
void MainWindowRenderer::updateRenderParams()
{
	m_renderSize = qMin( Global::dimX, (int)( ( sqrt( m_width * m_width + m_height * m_height ) / 12 ) / m_scale ) );

	m_renderDepth = Global::cfg->get( "renderDepth" ).toInt();

	m_viewLevel = GameState::viewLevel;

	m_volume.min = { 0, 0, qMin( qMax( m_viewLevel - m_renderDepth, 0 ), Global::dimZ - 1 ) };
	m_volume.max = { Global::dimX - 1, Global::dimY - 1, qMin( m_viewLevel, Global::dimZ - 1 ) };

	m_lightMin = Global::cfg->get( "lightMin" ).toFloat();
	if ( m_lightMin < 0.01 )
		m_lightMin = 0.3f;

	m_debug   = Global::debugMode;

	m_projectionMatrix.setToIdentity();
	m_projectionMatrix.ortho( -m_width / 2, m_width / 2, -m_height / 2, m_height / 2, -( m_volume.max.x + m_volume.max.y + m_volume.max.z + 1 ), -m_volume.min.z );
	m_projectionMatrix.scale( m_scale, m_scale );
	m_projectionMatrix.translate( m_moveX, -m_moveY );
}

// Helper functions for setting uniforms
/// @brief Sets an int uniform by name on a GL program.
/// @param shader Program handle.
/// @param name   Uniform name.
/// @param value  Integer value.
void MainWindowRenderer::setUniformi( GLuint shader, const char* name, GLint value )
{
	GLint loc = glGetUniformLocation( shader, name );
	if ( loc >= 0 ) glUniform1i( loc, value );
}

/// @brief Sets a float uniform by name on a GL program.
/// @param shader Program handle.
/// @param name   Uniform name.
/// @param value  Float value.
void MainWindowRenderer::setUniformf( GLuint shader, const char* name, GLfloat value )
{
	GLint loc = glGetUniformLocation( shader, name );
	if ( loc >= 0 ) glUniform1f( loc, value );
}

/// @brief Sets an unsigned int uniform by name on a GL program.
/// @param shader Program handle.
/// @param name   Uniform name.
/// @param value  Unsigned integer value.
void MainWindowRenderer::setUniformui( GLuint shader, const char* name, GLuint value )
{
	GLint loc = glGetUniformLocation( shader, name );
	if ( loc >= 0 ) glUniform1ui( loc, value );
}

/// @brief Sets a mat4 uniform by name on a GL program, converting from QMatrix4x4.
/// @param shader Program handle.
/// @param name   Uniform name.
/// @param matrix Matrix value (Qt column-major).
void MainWindowRenderer::setUniformMatrix4fv( GLuint shader, const char* name, const QMatrix4x4& matrix )
{
	GLint loc = glGetUniformLocation( shader, name );
	if ( loc >= 0 ) glUniformMatrix4fv( loc, 1, GL_FALSE, matrix.constData() );
}

/// @brief Main draw function for the world view. Clears the framebuffer, optionally reloads
///        shaders, applies pending tile updates via the compute shader, then draws tiles,
///        selection preview, thought bubbles, and axles in order.
void MainWindowRenderer::paintWorld()
{
	DebugScope s( "paint world" );
	QElapsedTimer timer;
	//timer.start();
	{
		DebugScope s( "clear" );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDisable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glDisable( GL_SCISSOR_TEST );

		glDepthMask( true );
		glStencilMask( 0xFFFFFFFF );
		glClearStencil( 0 );
		glClearDepth( 1 );
		// Keep an RCT2-like park-green fallback visible while a world is empty
		// or still uploading. The world pass overwrites this when available.
		glClearColor( 0.11f, 0.16f, 0.10f, 1.0f );
		glColorMask( true, true, true, true );
		//glClearDepth( 1 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	if ( m_inMenu )
	{
		return;
	}

	if ( m_reloadShaders )
	{
		DebugScope s( "init shaders" );
		if ( !initShaders() )
		{
			return;
		}
	}

	if ( !m_texesInitialized )
	{
		DebugScope s( "init textures" );
		initWorld();
		initTextures();
		updateRenderParams();
	}
	updateWorld();

	// Rebind correct textures to texture units
	for ( auto unit = 0; unit < 32; ++unit )
	{
		glActiveTexture( GL_TEXTURE0 + unit );
		glBindTexture( GL_TEXTURE_2D_ARRAY, m_textures[unit] );
	}

	timer.start();
	updateTextures();

	glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT );

	QString msg = "render time: " + QString::number( timer.elapsed() ) + " ms";
	//emit sendOverlayMessage( 1, msg );

	if ( !m_sceneFbo || m_sceneWidth <= 0 || m_sceneHeight <= 0 )
	{
		// Keep water visible even if the compositing target cannot be allocated.
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glViewport( 0, 0, qMax( 1, qRound( m_width * m_parent->devicePixelRatio() ) ), qMax( 1, qRound( m_height * m_parent->devicePixelRatio() ) ) );
		glBindVertexArray( m_vao );
		paintTiles();
		paintWater( true );
		paintSelection();
		paintThoughtBubbles();
		if ( Global::showAxles )
			paintAxles();
		glBindVertexArray( 0 );
		return;
	}

	// Render the opaque world into a color/depth target first, then copy both
	// attachments to the window before drawing the opaque pixel-art water overlay.
	// The water shader no longer samples the scene textures; the copied depth is
	// what preserves stable ordering around trees, walls, creatures, and shorelines.
	glBindFramebuffer( GL_FRAMEBUFFER, m_sceneFbo );
	glViewport( 0, 0, m_sceneWidth, m_sceneHeight );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_STENCIL_TEST );
	glDepthMask( true );
	glClearColor( 0.11f, 0.16f, 0.10f, 1.0f );
	glColorMask( true, true, true, true );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glBindVertexArray( m_vao );
	paintTiles();
	glBindVertexArray( 0 );

	// Put the finished scene back on the window framebuffer, including depth,
	// before drawing water. The water pass can therefore use ordinary depth test.
	glBindFramebuffer( GL_READ_FRAMEBUFFER, m_sceneFbo );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
	glDrawBuffer( GL_BACK );
	glBlitFramebuffer( 0, 0, m_sceneWidth, m_sceneHeight,
		0, 0, m_sceneWidth, m_sceneHeight,
		GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glViewport( 0, 0, m_sceneWidth, m_sceneHeight );

	paintWater();
	paintSelection();
	paintThoughtBubbles();
	if ( Global::showAxles )
		paintAxles();

	//glFinish();

	bool pause = Global::cfg->get( "Pause" ).toBool();

	if ( pause != m_pause )
	{
		m_pause = pause;
	}
}

void MainWindowRenderer::cleanupWaterTargets()
{
	if ( m_sceneFbo ) { glDeleteFramebuffers( 1, &m_sceneFbo ); m_sceneFbo = 0; }
	if ( m_sceneColor ) { glDeleteTextures( 1, &m_sceneColor ); m_sceneColor = 0; }
	if ( m_sceneDepth ) { glDeleteTextures( 1, &m_sceneDepth ); m_sceneDepth = 0; }
	m_sceneWidth = m_sceneHeight = 0;
}

void MainWindowRenderer::initializeWaterTargets()
{
	resizeWaterTargets();
}

void MainWindowRenderer::resizeWaterTargets()
{
	if ( m_width <= 0 || m_height <= 0 )
		return;

	cleanupWaterTargets();
	m_sceneWidth = qMax( 1, qRound( m_width * m_parent->devicePixelRatio() ) );
	m_sceneHeight = qMax( 1, qRound( m_height * m_parent->devicePixelRatio() ) );

	glGenTextures( 1, &m_sceneColor );
	glBindTexture( GL_TEXTURE_2D, m_sceneColor );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, m_sceneWidth, m_sceneHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );

	glGenTextures( 1, &m_sceneDepth );
	glBindTexture( GL_TEXTURE_2D, m_sceneDepth );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_sceneWidth, m_sceneHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr );

	glGenFramebuffers( 1, &m_sceneFbo );
	glBindFramebuffer( GL_FRAMEBUFFER, m_sceneFbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneColor, 0 );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneDepth, 0 );
	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers( 1, drawBuffers );
	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
	{
		qWarning() << "Water scene framebuffer is incomplete";
		cleanupWaterTargets();
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		return;
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void MainWindowRenderer::initializeWaterTextures()
{
	constexpr int size = 64;
	QVector<unsigned char> dudv( size * size * 4 );
	QVector<unsigned char> normal( size * size * 4 );
	constexpr float tau = 6.28318530718f;
	for ( int y = 0; y < size; ++y )
	{
		for ( int x = 0; x < size; ++x )
		{
			const float u = static_cast<float>( x ) / size;
			const float v = static_cast<float>( y ) / size;
			const float dx = 0.55f * std::sin( tau * ( u * 3.0f + v * 2.0f ) ) + 0.45f * std::sin( tau * ( u * 7.0f - v * 5.0f ) );
			const float dy = 0.55f * std::cos( tau * ( v * 4.0f - u * 2.0f ) ) + 0.45f * std::sin( tau * ( u * 5.0f + v * 6.0f ) );
			const int offset = ( x + y * size ) * 4;
			dudv[offset] = static_cast<unsigned char>( qBound( 0, qRound( ( dx * 0.5f + 0.5f ) * 255.0f ), 255 ) );
			dudv[offset + 1] = static_cast<unsigned char>( qBound( 0, qRound( ( dy * 0.5f + 0.5f ) * 255.0f ), 255 ) );
			dudv[offset + 2] = 128;
			dudv[offset + 3] = 255;
			const QVector3D n = QVector3D( -dx * 0.32f, -dy * 0.32f, 1.0f ).normalized();
			normal[offset] = static_cast<unsigned char>( qRound( ( n.x() * 0.5f + 0.5f ) * 255.0f ) );
			normal[offset + 1] = static_cast<unsigned char>( qRound( ( n.y() * 0.5f + 0.5f ) * 255.0f ) );
			normal[offset + 2] = static_cast<unsigned char>( qRound( ( n.z() * 0.5f + 0.5f ) * 255.0f ) );
			normal[offset + 3] = 255;
		}
	}

	auto createTexture = [&]( GLuint& texture, const QVector<unsigned char>& pixels ) {
		if ( texture )
			glDeleteTextures( 1, &texture );
		glGenTextures( 1, &texture );
		glBindTexture( GL_TEXTURE_2D, texture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.constData() );
		glGenerateMipmap( GL_TEXTURE_2D );
	};
	createTexture( m_waterDuDv, dudv );
	createTexture( m_waterNormal, normal );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void MainWindowRenderer::paintWater( bool forceFlat )
{
	if ( !m_waterShader || !m_tileBo )
		return;

	DebugScope s( "paint water" );
	const int waterQuality = forceFlat || !m_waterDuDv
		? 0
		: qBound( 0, Global::cfg->get( "waterQuality" ).toInt(), 2 );
	GLboolean oldDepthMask = GL_TRUE;
	GLint oldProgram = 0;
	GLint oldVertexArray = 0;
	GLint oldActiveTexture = GL_TEXTURE0;
	glGetBooleanv( GL_DEPTH_WRITEMASK, &oldDepthMask );
	glGetIntegerv( GL_CURRENT_PROGRAM, &oldProgram );
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &oldVertexArray );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &oldActiveTexture );
	const bool blendWasEnabled = glIsEnabled( GL_BLEND );
	const bool depthWasEnabled = glIsEnabled( GL_DEPTH_TEST );
	glUseProgram( m_waterShader );
	setCommonUniforms( m_waterShader );
	setUniformi( m_waterShader, "uDuDvMap", 27 );
	setUniformi( m_waterShader, "uWaterQuality", waterQuality );
	setUniformf( m_waterShader, "uWaterTime", static_cast<float>( m_waterClock.elapsed() ) / 1000.0f );
	setUniformf( m_waterShader, "uDaylight", static_cast<float>( m_daylight ) );
	setUniformf( m_waterShader, "uLightMin", m_lightMin );
	glActiveTexture( GL_TEXTURE0 + 27 );
	glBindTexture( GL_TEXTURE_2D, m_waterDuDv );

	const Position volume = m_volume.size();
	const GLsizei tiles = volume.x * volume.y * volume.z;
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_FALSE );
	// The opaque pass and compute update pass both use binding zero, but the
	// post-process stages may legally change the active SSBO binding. Rebind the
	// tile buffer here so the water vertex shader always reads the current world.
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, m_tileBo );
	// paintTiles() releases the shared VAO before the post-process passes. The
	// water draw uses the floor indices in that VAO, so restore it explicitly
	// before issuing the indexed instanced draw.
	glBindVertexArray( m_vao );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_vibo );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	setUniformi( m_waterShader, "uWaterFace", 0 );
	glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void*)( sizeof( GLushort ) * 6 ), tiles );
	setUniformi( m_waterShader, "uWaterFace", 1 );
	glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, tiles );
	setUniformi( m_waterShader, "uWaterFace", 2 );
	glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, tiles );
	glBindVertexArray( static_cast<GLuint>( oldVertexArray ) );
	glDepthMask( oldDepthMask );
	if ( !blendWasEnabled ) glDisable( GL_BLEND );
	if ( !depthWasEnabled ) glDisable( GL_DEPTH_TEST );
	glActiveTexture( GL_TEXTURE0 + 27 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindTexture( GL_TEXTURE_2D_ARRAY, m_textures[27] );
	glActiveTexture( static_cast<GLenum>( oldActiveTexture ) );
	glUseProgram( static_cast<GLuint>( oldProgram ) );
}

void MainWindowRenderer::initializeCameraPreview()
{
	for ( int slot = 0; slot < cameraPreviewSlotCount; ++slot )
	{
		glGenTextures( 1, &m_cameraPreviewTexture[slot] );
		glBindTexture( GL_TEXTURE_2D, m_cameraPreviewTexture[slot] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, m_cameraPreviewWidth, m_cameraPreviewHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
		glBindTexture( GL_TEXTURE_2D, 0 );

		glGenRenderbuffers( 1, &m_cameraPreviewDepth[slot] );
		glBindRenderbuffer( GL_RENDERBUFFER, m_cameraPreviewDepth[slot] );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_cameraPreviewWidth, m_cameraPreviewHeight );
		glBindRenderbuffer( GL_RENDERBUFFER, 0 );

		glGenFramebuffers( 1, &m_cameraPreviewFbo[slot] );
		glBindFramebuffer( GL_FRAMEBUFFER, m_cameraPreviewFbo[slot] );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_cameraPreviewTexture[slot], 0 );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_cameraPreviewDepth[slot] );
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		{
			qWarning() << "Inspector camera framebuffer is incomplete for slot" << slot;
			glDeleteFramebuffers( 1, &m_cameraPreviewFbo[slot] );
			glDeleteRenderbuffers( 1, &m_cameraPreviewDepth[slot] );
			glDeleteTextures( 1, &m_cameraPreviewTexture[slot] );
			m_cameraPreviewFbo[slot] = m_cameraPreviewDepth[slot] = m_cameraPreviewTexture[slot] = 0;
		}
	}
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void MainWindowRenderer::paintCameraPreview( const CameraPreviewTarget* target, int slot )
{
	if ( !target || slot < 0 || slot >= cameraPreviewSlotCount || !m_cameraPreviewFbo[slot] || !m_texesInitialized || !m_worldShader ) return;

	// Keep the follow camera near 60 Hz even when the game is running on a high-
	// refresh monitor. The old 33 ms gate made both camera motion and the sampled
	// creature interpolation visibly step at 30 FPS. A 12 ms floor lands at about
	// 60-72 updates on common 60/120/144 Hz displays without submitting a second
	// world pass at the full 144/240 Hz monitor rate.
	constexpr qint64 previewIntervalMs = 12;
	const qint64 nowMs = m_waterClock.isValid() ? m_waterClock.elapsed() : 0;
	if ( m_cameraPreviewLastPaintMs[slot] >= 0
		&& nowMs - m_cameraPreviewLastPaintMs[slot] < previewIntervalMs )
		return;
	m_cameraPreviewLastPaintMs[slot] = nowMs;
	if ( qEnvironmentVariableIsSet( "INGNOMIA_LOAD_TRACE_PATH" ) )
	{
		static std::array<qint64, cameraPreviewSlotCount> cadenceWindowStartMs{};
		static std::array<int, cameraPreviewSlotCount> cadenceFrameCount{};
		if ( cadenceWindowStartMs[slot] == 0 )
			cadenceWindowStartMs[slot] = nowMs;
		++cadenceFrameCount[slot];
		const qint64 cadenceElapsedMs = nowMs - cadenceWindowStartMs[slot];
		if ( cadenceElapsedMs >= 1000 )
		{
			traceRender( QString( "camera preview cadence slot=%1 frames=%2 elapsedMs=%3 avgMs=%4" )
				.arg( slot ).arg( cadenceFrameCount[slot] ).arg( cadenceElapsedMs )
				.arg( static_cast<double>( cadenceElapsedMs ) / cadenceFrameCount[slot], 0, 'f', 2 ) );
			cadenceWindowStartMs[slot] = nowMs;
			cadenceFrameCount[slot] = 0;
		}
	}

	// Match the preview camera to the creature shader's interpolated position. The
	// inspector state keeps the tile where the creature was selected, so it becomes
	// stale as soon as that creature walks. Creature ID is the stable identity: use
	// the renderer's latest current position for both the camera and render volume,
	// and retain the selected tile only as a fallback while data is unavailable.
	Position trackedPosition = target->position;
	float cameraX = static_cast<float>( trackedPosition.x );
	float cameraY = static_cast<float>( trackedPosition.y );
	float cameraZ = static_cast<float>( trackedPosition.z );
	float cameraMotionProgress = 1.0f;
	const auto cameraTarget = m_creatureCameraTargets.constFind( target->creatureID );
	if ( cameraTarget != m_creatureCameraTargets.constEnd() )
	{
		trackedPosition = cameraTarget->currentPosition;
		cameraX = static_cast<float>( trackedPosition.x );
		cameraY = static_cast<float>( trackedPosition.y );
		cameraZ = static_cast<float>( trackedPosition.z );
		const float motionAge = std::max( 0.0f, m_creatureRenderTick - static_cast<float>( cameraTarget->motionTick ) );
		const float motionDuration = std::max( 1.0f, static_cast<float>( cameraTarget->motionDurationTicks ) );
		cameraMotionProgress = std::clamp( motionAge / motionDuration, 0.0f, 1.0f );
		const float previousWeight = 1.0f - cameraMotionProgress;
		cameraX += static_cast<float>( cameraTarget->previousPosition.x - cameraTarget->currentPosition.x ) * previousWeight;
		cameraY += static_cast<float>( cameraTarget->previousPosition.y - cameraTarget->currentPosition.y ) * previousWeight;
		cameraZ += static_cast<float>( cameraTarget->previousPosition.z - cameraTarget->currentPosition.z ) * previousWeight;

		// The runtime probe samples progress buckets instead of every frame. Besides
		// keeping traces compact, this verifies that a single pre/post segment survives
		// long enough for the follow camera to visit intermediate coordinates.
		if ( qEnvironmentVariableIsSet( "INGNOMIA_LOAD_TRACE_PATH" )
			&& cameraTarget->previousPosition != cameraTarget->currentPosition )
		{
			static std::array<quint64, cameraPreviewSlotCount> tracedMotionTicks{};
			static std::array<int, cameraPreviewSlotCount> tracedProgressBuckets{};
			const int progressBucket = qBound( 0, static_cast<int>( cameraMotionProgress * 10.0f ), 10 );
			if ( tracedMotionTicks[slot] != cameraTarget->motionTick )
			{
				tracedMotionTicks[slot] = cameraTarget->motionTick;
				tracedProgressBuckets[slot] = -1;
			}
			if ( progressBucket != tracedProgressBuckets[slot] )
			{
				traceRender( QString( "camera preview tween slot=%1 tick=%2 progress=%3 camera=%4,%5,%6 previous=%7,%8,%9 current=%10,%11,%12" )
					.arg( slot ).arg( cameraTarget->motionTick ).arg( cameraMotionProgress, 0, 'f', 3 )
					.arg( cameraX, 0, 'f', 3 ).arg( cameraY, 0, 'f', 3 ).arg( cameraZ, 0, 'f', 3 )
					.arg( cameraTarget->previousPosition.x ).arg( cameraTarget->previousPosition.y ).arg( cameraTarget->previousPosition.z )
					.arg( cameraTarget->currentPosition.x ).arg( cameraTarget->currentPosition.y ).arg( cameraTarget->currentPosition.z ) );
				tracedProgressBuckets[slot] = progressBucket;
			}
		}
	}
	// The renderer trace is opt-in. Record each distinct live tile after it diverges
	// from the originally selected tile so automated follow-camera runs can prove
	// that the preview continues tracking rather than merely looking centered once.
	static std::array<Position, cameraPreviewSlotCount> lastFollowTracePosition{};
	static std::array<bool, cameraPreviewSlotCount> hasFollowTracePosition{};
	if ( trackedPosition != target->position &&
		( !hasFollowTracePosition[slot] || lastFollowTracePosition[slot] != trackedPosition ) )
	{
		traceRender( QString( "camera preview follow slot=%1 selected=%2,%3,%4 tracked=%5,%6,%7" )
			.arg( slot )
			.arg( target->position.x ).arg( target->position.y ).arg( target->position.z )
			.arg( trackedPosition.x ).arg( trackedPosition.y ).arg( trackedPosition.z ) );
		lastFollowTracePosition[slot] = trackedPosition;
		hasFollowTracePosition[slot] = true;
	}

	// The inspector has its own camera. It renders a small target-centered slice into
	// a private texture instead of cropping the player's framebuffer, so moving or
	// rotating the main map camera cannot change what the creature window shows.
	GLint drawFramebuffer = 0, readFramebuffer = 0, viewport[4] = {}, activeTexture = 0, vertexArray = 0;
	GLint drawBuffer = 0, readBuffer = 0, depthFunction = GL_LESS, currentProgram = 0;
	GLint arrayBuffer = 0, elementArrayBuffer = 0, texture2d = 0, texture2dArray = 0;
	GLint renderbuffer = 0;
	GLboolean blend = GL_FALSE, depth = GL_FALSE, stencil = GL_FALSE, scissor = GL_FALSE, cull = GL_FALSE, depthMask = GL_TRUE;
	GLboolean framebufferSrgb = GL_FALSE;
	GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer );
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer );
	glGetIntegerv( GL_VIEWPORT, viewport );
	glGetIntegerv( GL_ACTIVE_TEXTURE, &activeTexture );
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &vertexArray );
	glGetIntegerv( GL_CURRENT_PROGRAM, &currentProgram );
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &arrayBuffer );
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer );
	glGetIntegerv( GL_DRAW_BUFFER, &drawBuffer );
	glGetIntegerv( GL_READ_BUFFER, &readBuffer );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture2d );
	glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &texture2dArray );
	glGetIntegerv( GL_RENDERBUFFER_BINDING, &renderbuffer );
	glGetIntegerv( GL_DEPTH_FUNC, &depthFunction );
	glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	blend = glIsEnabled( GL_BLEND );
	depth = glIsEnabled( GL_DEPTH_TEST );
	stencil = glIsEnabled( GL_STENCIL_TEST );
	scissor = glIsEnabled( GL_SCISSOR_TEST );
	cull = glIsEnabled( GL_CULL_FACE );
	framebufferSrgb = glIsEnabled( GL_FRAMEBUFFER_SRGB );
	glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );

	const auto savedProjection = m_projectionMatrix;
	const auto savedVolume = m_volume;
	const auto savedRenderSize = m_renderSize;
	const auto savedViewLevel = m_viewLevel;
	const auto savedRotation = m_rotation;
	const auto savedDaylight = m_daylight;

	// Render a generous target-centered volume. The projection below is anchored
	// to the selected tile, so edge-of-map targets stay centered instead of being
	// clipped by a camera that was clamped to the main map viewport.
	// The square preview only exposes roughly five isometric tiles from its center.
	// Ten tiles of XY padding safely covers tall sprites and map-edge views while
	// cutting enough hidden instances to pay for the smoother camera refresh.
	const int radius = 10;
	m_volume.min = { qMax( 0, trackedPosition.x - radius ), qMax( 0, trackedPosition.y - radius ), qMax( 0, trackedPosition.z - m_renderDepth ) };
	m_volume.max = { qMin( static_cast<int>( Global::dimX ) - 1, trackedPosition.x + radius ), qMin( static_cast<int>( Global::dimY ) - 1, trackedPosition.y + radius ), qMin( trackedPosition.z, static_cast<int>( Global::dimZ ) - 1 ) };
	m_viewLevel = trackedPosition.z;
	m_renderSize = radius * 2 + 1;
	// The preview camera deliberately uses a stable rotation. It is independent of
	// the player's camera, while retaining the same isometric projection as the map.
	m_rotation = 0;
	// Build the preview projection from its own framebuffer dimensions. Deriving XY from
	// the main projection makes the inspector inherit the player's viewport aspect, pan,
	// and zoom; that is exactly what this camera is meant to avoid.
	const float worldX = 16.f * cameraX - 16.f * cameraY + 16.f;
	const float worldY = -8.f * cameraY - 8.f * cameraX -
		( static_cast<float>( m_volume.max.z ) - cameraZ ) * 20.f - 12.f + 32.f;
	constexpr float previewZoom = 1.6f;
	const float halfWorldWidth = ( static_cast<float>( m_cameraPreviewWidth ) * 0.5f ) / previewZoom;
	const float halfWorldHeight = ( static_cast<float>( m_cameraPreviewHeight ) * 0.5f ) / previewZoom;
	const float previewNear = -( static_cast<float>( Global::dimX - 1 ) +
		static_cast<float>( Global::dimY - 1 ) + static_cast<float>( m_volume.max.z ) + 1.f );
	const float previewFar = -static_cast<float>( m_volume.min.z );
	QMatrix4x4 previewProjection;
	previewProjection.setToIdentity();
	// OpenGL framebuffers have their origin at the lower-left, while the RmlUi
	// image surface is laid out from the upper-left. Reverse the preview camera's
	// Y range here so the sampled texture is upright in every inspector window.
	previewProjection.ortho( worldX - halfWorldWidth, worldX + halfWorldWidth,
		worldY + halfWorldHeight, worldY - halfWorldHeight, previewNear, previewFar );
	m_projectionMatrix = previewProjection;

	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_cameraPreviewFbo[slot] );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, m_cameraPreviewFbo[slot] );
	glDrawBuffer( GL_COLOR_ATTACHMENT0 );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
	glViewport( 0, 0, m_cameraPreviewWidth, m_cameraPreviewHeight );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glDisable( GL_STENCIL_TEST );
	const GLenum previewDrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers( 1, previewDrawBuffers );
	glClearColor( 0.11f, 0.16f, 0.10f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	glBindVertexArray( m_vao );
	paintTiles();
	// Keep water visible in the inspector camera with the same depth-aware pass as the main map.
	paintWater();
	glBindVertexArray( 0 );
	const auto previewVolume = m_volume.size();

	// paintTiles updates daylight as part of the main world pass. The preview is a
	// read-only camera and must not advance simulation/render state a second time.
	m_daylight = savedDaylight;
	m_projectionMatrix = savedProjection;
	m_volume = savedVolume;
	m_renderSize = savedRenderSize;
	m_viewLevel = savedViewLevel;
	m_rotation = savedRotation;

	if ( blend ) glEnable( GL_BLEND ); else glDisable( GL_BLEND );
	if ( depth ) glEnable( GL_DEPTH_TEST ); else glDisable( GL_DEPTH_TEST );
	if ( stencil ) glEnable( GL_STENCIL_TEST ); else glDisable( GL_STENCIL_TEST );
	if ( scissor ) glEnable( GL_SCISSOR_TEST ); else glDisable( GL_SCISSOR_TEST );
	if ( cull ) glEnable( GL_CULL_FACE ); else glDisable( GL_CULL_FACE );
	if ( framebufferSrgb ) glEnable( GL_FRAMEBUFFER_SRGB ); else glDisable( GL_FRAMEBUFFER_SRGB );
	glDepthFunc( static_cast<GLenum>( depthFunction ) );
	glDepthMask( depthMask );
	glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );

	static bool tracedPreview = false;
	if ( !tracedPreview )
	{
		tracedPreview = true;
		traceRender( QString( "camera preview own-pass slot=%1 target=%2,%3,%4 tracked=%5,%6,%7 volume=%8,%9,%10 fbo=%11" )
			.arg( slot )
			.arg( target->position.x ).arg( target->position.y ).arg( target->position.z )
			.arg( trackedPosition.x ).arg( trackedPosition.y ).arg( trackedPosition.z )
			.arg( previewVolume.x ).arg( previewVolume.y ).arg( previewVolume.z )
			.arg( m_cameraPreviewFbo[slot] ) );
	}

	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, static_cast<GLuint>( drawFramebuffer ) );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, static_cast<GLuint>( readFramebuffer ) );
	glDrawBuffer( static_cast<GLenum>( drawBuffer ) );
	glReadBuffer( static_cast<GLenum>( readBuffer ) );
	glViewport( viewport[0], viewport[1], viewport[2], viewport[3] );
	// Restore VAO ownership before restoring its element buffer. Binding an
	// element buffer while VAO 0 is active is invalid in core GL and leaves the
	// preview VAO's index buffer active on some drivers. That stale index buffer
	// is then consumed by RmlUi's text batches, which presents as HUD captions
	// appearing inside unrelated sidebar buttons.
	glBindVertexArray( static_cast<GLuint>( vertexArray ) );
	glBindBuffer( GL_ARRAY_BUFFER, static_cast<GLuint>( arrayBuffer ) );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>( elementArrayBuffer ) );
	glActiveTexture( static_cast<GLenum>( activeTexture ) );
	glBindTexture( GL_TEXTURE_2D, static_cast<GLuint>( texture2d ) );
	glBindTexture( GL_TEXTURE_2D_ARRAY, static_cast<GLuint>( texture2dArray ) );
	glBindRenderbuffer( GL_RENDERBUFFER, static_cast<GLuint>( renderbuffer ) );
	glUseProgram( static_cast<GLuint>( currentProgram ) );
}

/// @brief Slot invoked when camera parameters change: recomputes render params and emits
///        signalCameraPosition so listeners (e.g. AggregatorSound) can react.
void MainWindowRenderer::onRenderParamsChanged()
{
	updateRenderParams();
	emit redrawRequired();
	emit signalCameraPosition(m_moveX, m_moveY, m_viewLevel, m_rotation, m_scale);

}

/// @brief Uploads the common uniforms shared by every draw shader (projection, camera,
///        view level, daylight, light floor, rotation, …) onto the given program.
/// @param shader Target shader program.
void MainWindowRenderer::setCommonUniforms( GLuint shader )
{
	GLint indexTotal = glGetUniformLocation( shader, "uWorldSize" );
	if ( indexTotal >= 0 )
	{
		glUniform3ui( indexTotal, Global::dimX, Global::dimY, Global::dimZ );
	}
	GLint indexMin = glGetUniformLocation( shader, "uRenderMin" );
	if ( indexMin >= 0 )
	{
		glUniform3ui( indexMin, m_volume.min.x, m_volume.min.y, m_volume.min.z );
	}
	GLint indexMax = glGetUniformLocation( shader, "uRenderMax" );
	if ( indexMax >= 0 )
	{
		glUniform3ui( indexMax, m_volume.max.x, m_volume.max.y, m_volume.max.z );
	}
	setUniformMatrix4fv( shader, "uTransform", m_projectionMatrix );
	setUniformi( shader, "uWorldRotation", (GLint)m_rotation );
	setUniformi( shader, "uTickNumber", (GLint)GameState::tick );
}

/// @brief Draws the main world tile layer: binds m_worldShader, the tile SSBO, and the
///        sprite array textures, then issues an instanced draw of the tile quad per tile
///        in the render volume.
void MainWindowRenderer::paintTiles()
{
	static bool tracedTiles = false;
	if ( !tracedTiles )
	{
		tracedTiles = true;
		traceRender( QString( "paintTiles shader=%1 texUsed=%2 volume=%3,%4,%5 move=%6,%7" )
			.arg( m_worldShader ).arg( m_texesUsed ).arg( m_volume.size().x ).arg( m_volume.size().y ).arg( m_volume.size().z )
			.arg( m_moveX ).arg( m_moveY ) );
		traceRender( QString( "paintTiles flags debug=%1 undiscovered=%2 water=%3 wallsLowered=%4" )
			.arg( Global::debugMode ).arg( Global::undiscoveredUID ).arg( Global::waterSpriteUID ).arg( Global::wallsLowered ) );
	}
	DebugScope s( "paint tiles" );

	glUseProgram( m_worldShader );
	setCommonUniforms( m_worldShader );

	for ( int i = 0; i < m_texesUsed; ++i )
	{
		auto texNum = "uTexture[" + QString::number( i ) + "]";
		setUniformi( m_worldShader, texNum.toStdString().c_str(), i );
	}

	//glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	setUniformi( m_worldShader, "uOverlay", Global::showDesignations ? 1 : 0 );
	setUniformi( m_worldShader, "uShowJobs", Global::showJobs ? 1 : 0 );
	setUniformi( m_worldShader, "uDebug", m_debug ? 1 : 0 );
	setUniformi( m_worldShader, "uWallsLowered", Global::wallsLowered ? 1 : 0 );
	setUniformi( m_worldShader, "uCreatureOnly", 0 );

	if ( m_lastCreatureMotionStartMs >= 0 && m_creatureMotionClock.isValid() )
	{
		const qint64 elapsed = m_creatureMotionClock.elapsed() - m_lastCreatureMotionStartMs;
		m_creatureInterpolation = qBound( 0.0f,
			static_cast<float>( elapsed ) / qMax<qint64>( 1, m_creatureMotionIntervalMs ), 1.0f );
	}
	else
	{
		m_creatureInterpolation = 1.0f;
	}
	setUniformf( m_worldShader, "uCreatureInterpolation", m_creatureInterpolation );
	float creatureRenderTick = static_cast<float>( m_lastCreatureMotionTick );
	if ( m_lastCreatureMotionStartMs >= 0 && m_creatureMotionClock.isValid() )
	{
		const qint64 elapsed = m_creatureMotionClock.elapsed() - m_lastCreatureMotionStartMs;
		creatureRenderTick += static_cast<float>( elapsed ) /
			static_cast<float>( qMax<qint64>( 1, m_creatureMotionIntervalMs ) );
	}
	m_creatureRenderTick = creatureRenderTick;
	setUniformf( m_worldShader, "uCreatureRenderTick", creatureRenderTick );
	if ( qEnvironmentVariableIsSet( "INGNOMIA_LOAD_TRACE_PATH" ) )
	{
		bool targetOk = false;
		const unsigned int requestedTarget = qEnvironmentVariable( "INGNOMIA_RENDER_TRACE_CREATURE_ID" ).toUInt( &targetOk );
		auto target = targetOk ? m_creatureCameraTargets.constFind( requestedTarget ) : m_creatureCameraTargets.constEnd();
		if ( target == m_creatureCameraTargets.constEnd() )
		{
			for ( auto it = m_creatureCameraTargets.constBegin(); it != m_creatureCameraTargets.constEnd(); ++it )
				if ( it->previousPosition != it->currentPosition ) { target = it; break; }
		}
		if ( target != m_creatureCameraTargets.constEnd()
			&& target->previousPosition != target->currentPosition )
		{
			const float duration = std::max( 1.0f, static_cast<float>( target->motionDurationTicks ) );
			const float progress = std::clamp(
				( creatureRenderTick - static_cast<float>( target->motionTick ) ) / duration, 0.0f, 1.0f );
			static unsigned int tracedCreature = 0;
			static quint64 tracedTick = 0;
			static int tracedBucket = -1;
			const int bucket = qBound( 0, static_cast<int>( progress * 10.0f ), 10 );
			if ( tracedCreature != target.key() || tracedTick != target->motionTick )
			{
				tracedCreature = target.key();
				tracedTick = target->motionTick;
				tracedBucket = -1;
			}
			if ( bucket != tracedBucket )
			{
				const float previousWeight = 1.0f - progress;
				const float renderX = static_cast<float>( target->currentPosition.x )
					+ static_cast<float>( target->previousPosition.x - target->currentPosition.x ) * previousWeight;
				const float renderY = static_cast<float>( target->currentPosition.y )
					+ static_cast<float>( target->previousPosition.y - target->currentPosition.y ) * previousWeight;
				traceRender( QString( "main creature tween id=%1 tick=%2 progress=%3 render=%4,%5 previous=%6,%7 current=%8,%9" )
					.arg( target.key() ).arg( target->motionTick ).arg( progress, 0, 'f', 3 )
					.arg( renderX, 0, 'f', 3 ).arg( renderY, 0, 'f', 3 )
					.arg( target->previousPosition.x ).arg( target->previousPosition.y )
					.arg( target->currentPosition.x ).arg( target->currentPosition.y ) );
				tracedBucket = bucket;
			}
		}
	}

	setUniformi( m_worldShader, "uUndiscoveredTex", Global::undiscoveredUID * 4 );
	setUniformi( m_worldShader, "uWaterTex", Global::waterSpriteUID * 4 );

	const float daylightTarget = scheduledDaylight();
	const qint64 daylightNowMs = m_waterClock.isValid() ? m_waterClock.elapsed() : 0;
	const float daylightDeltaSeconds = m_lastDaylightUpdateMs < 0
		? 0.0f
		: std::clamp( static_cast<float>( daylightNowMs - m_lastDaylightUpdateMs ) / 1000.0f, 0.0f, 0.25f );
	m_lastDaylightUpdateMs = daylightNowMs;
	// A short render-time settle removes one-frame jumps when loading or pausing,
	// while the in-game sunrise/twilight curve controls the actual schedule.
	const float daylightResponse = 1.0f - std::exp( -daylightDeltaSeconds / 1.5f );
	m_daylight += ( daylightTarget - m_daylight ) * daylightResponse;
	setUniformf( m_worldShader, "uDaylight", m_daylight );
	setUniformf( m_worldShader, "uLightMin", m_lightMin );
	//setUniformf( m_worldShader, "uDaylight", 1.0f );

	auto volume   = m_volume.size();
	GLsizei tiles = volume.x * volume.y * volume.z;

	// First pass is pure front-to-back of opaque blocks, alpha doesn't even work
	glDisable( GL_BLEND );
	glDepthMask( true );

	setUniformi( m_worldShader, "uPaintFrontToBack", 1 );
	glDrawElementsInstanced( GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, 0, tiles );

	//!TODO Transparency pass is too early, all the stuff rendered later is still missing
	// Second pass includes transparency
	setUniformi( m_worldShader, "uPaintFrontToBack", 0 );
	glEnable( GL_BLEND );
	glDrawElementsInstanced( GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (void*)( sizeof( GLushort ) * 6 ), tiles );

	// Creatures are composited separately so their motion can be interpolated without
	// moving walls, floors, items, or water with them.
	if ( m_paintCreatures )
	{
		setUniformi( m_worldShader, "uCreatureOnly", 1 );
		setUniformi( m_worldShader, "uPaintFrontToBack", 0 );
		glEnable( GL_BLEND );
		glDrawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0, tiles );
		setUniformi( m_worldShader, "uCreatureOnly", 0 );
	}

	// All done with depth writes, everything beyond is layered
	glDepthMask( false );

	glUseProgram( 0 );
}

/// @brief Draws the placement cursor preview grid using m_selectionShader. Optionally
///        disables depth test for stair placement previews.
void MainWindowRenderer::paintSelection()
{
	// TODO this is a workaround until some transparency solution is implemented
	GLint oldVertexArray = 0;
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &oldVertexArray );
	// The water pass is a self-contained post-process and restores the VAO that
	// was active when it started. In the normal scene path that is VAO 0, so the
	// selection draw must bind the renderer's placement quad explicitly instead
	// of relying on whichever pass happened to run immediately before it.
	glBindVertexArray( m_vao );
	if( m_selectionNoDepthTest )
	{
		glDisable( GL_DEPTH_TEST );
	}
	DebugScope s( "paint selection" );
	glUseProgram( m_selectionShader );
	setCommonUniforms( m_selectionShader );

	for ( int i = 0; i < m_texesUsed; ++i )
	{
		auto texNum = "uTexture[" + QString::number( i ) + "]";
		setUniformi( m_selectionShader, texNum.toStdString().c_str(), i );
	}

	for ( const auto& sd : m_selectionData )
	{
		GLint tile = glGetUniformLocation( m_selectionShader, "tile" );
		glUniform3ui( tile, sd.pos.x, sd.pos.y, sd.pos.z );
		setUniformi( m_selectionShader, "uSpriteID", (GLint)sd.spriteID );
		setUniformi( m_selectionShader, "uRotation", (GLint)sd.localRot );
		setUniformi( m_selectionShader, "uValid", sd.valid ? 1 : 0 );
		setUniformf( m_selectionShader, "uPreviewYOffset", sd.previewYOffset );

		glDrawArraysInstancedBaseInstance( GL_TRIANGLE_STRIP, 0, 4, 1, 0 );
	}

	glUseProgram( 0 );
	glBindVertexArray( static_cast<GLuint>( oldVertexArray ) );
	if( m_selectionNoDepthTest )
	{
		glEnable( GL_DEPTH_TEST );
	}
}

/// @brief Draws all active thought bubbles above their gnomes using m_thoughtBubbleShader.
void MainWindowRenderer::paintThoughtBubbles()
{
	DebugScope s( "paint thoughts" );

	glUseProgram( m_thoughtBubbleShader );
	setCommonUniforms( m_thoughtBubbleShader );

	setUniformi( m_thoughtBubbleShader, "uTexture0", 0 );

	for ( const auto& thoughtBubble : m_thoughBubbles.thoughtBubbles )
	{
		if ( thoughtBubble.pos.z <= m_viewLevel )
		{
			GLint tile = glGetUniformLocation( m_thoughtBubbleShader, "tile" );
			glUniform3ui( tile, thoughtBubble.pos.x, thoughtBubble.pos.y, thoughtBubble.pos.z );
			setUniformi( m_thoughtBubbleShader, "uType", (GLint)thoughtBubble.sprite );
			glDrawArraysInstancedBaseInstance( GL_TRIANGLE_STRIP, 0, 4, 1, 0 );
		}
	}

	glUseProgram( 0 );
}

/// @brief Draws the spinning axle overlays using m_axleShader, if Global::showAxles is on.
void MainWindowRenderer::paintAxles()
{
	DebugScope s( "paint axles" );

	glUseProgram( m_axleShader );
	setCommonUniforms( m_axleShader );
	setUniformi( m_axleShader, "uTickNumber", (GLint)GameState::tick );

	for ( int i = 0; i < m_texesUsed; ++i )
	{
		auto texNum = "uTexture[" + QString::number( i ) + "]";
		setUniformi( m_axleShader, texNum.toStdString().c_str(), i );
	}

	for ( const auto& ad : m_axleData.data )
	{
		if ( !ad.isVertical && ad.pos.z <= m_viewLevel )
		{
			GLint tile = glGetUniformLocation( m_axleShader, "tile" );
			glUniform3ui( tile, ad.pos.x, ad.pos.y, ad.pos.z );
			setUniformi( m_axleShader, "uSpriteID", (GLint)ad.spriteID );
			setUniformi( m_axleShader, "uRotation", (GLint)ad.localRot );
			setUniformf( m_axleShader, "uAnim", ad.anim );

			glDrawArraysInstancedBaseInstance( GL_TRIANGLE_STRIP, 0, 4, 1, 0 );
		}
	}

	glUseProgram( 0 );
}

/// @brief Slot: viewport resize handler. Updates m_width/m_height and the projection matrix.
/// @param w New width in pixels.
/// @param h New height in pixels.
void MainWindowRenderer::resize( int w, int h )
{
	m_width  = w;
	m_height = h;
	const int pixelWidth = qMax( 1, qRound( w * m_parent->devicePixelRatio() ) );
	const int pixelHeight = qMax( 1, qRound( h * m_parent->devicePixelRatio() ) );
	if ( m_sceneWidth != pixelWidth || m_sceneHeight != pixelHeight )
		resizeWaterTargets();
	onRenderParamsChanged();
}

/// @brief Rotates the camera by @p direction steps (positive = CW, negative = CCW).
///        Updates camera offsets so the centre stays fixed, then requests a redraw.
/// @param direction Rotation delta.
void MainWindowRenderer::rotate( int direction )
{
	direction  = qBound( -1, direction, 1 );
	m_rotation = ( 4 + m_rotation + direction ) % 4;

	if( direction == 1 )
	{
		updatePositionAfterCWRotation( m_moveX, m_moveY );
	}
	else
	{
		updatePositionAfterCWRotation( m_moveX, m_moveY );
		updatePositionAfterCWRotation( m_moveX, m_moveY );
		updatePositionAfterCWRotation( m_moveX, m_moveY );
	}
	onRenderParamsChanged();
}

/// @brief Sets the camera position to the given absolute world-space offsets. Used when
///        restoring a saved view on load so the renderer's leftover m_moveX/m_moveY from
///        the previous game doesn't compound the result.
void MainWindowRenderer::setMove( float x, float y )
{
	if ( !Global::dimX )
		return;
	m_moveX = x;
	m_moveY = y;
	GameState::moveX = m_moveX;
	GameState::moveY = m_moveY;
	onRenderParamsChanged();
}

/// @brief Pans the camera by (x, y) screen pixels.
/// @param x X delta in pixels.
/// @param y Y delta in pixels.
void MainWindowRenderer::move( float x, float y )
{
	if ( !Global::dimX )
		return;

	m_moveX += x / m_scale;
	m_moveY += y / m_scale;

	const auto centerY = -Global::dimX * 8.f;
	const auto centerX = 0;

	float oldX, oldY;
	do
	{
		oldX   = m_moveX;
		oldY   = m_moveY;
		const auto rangeY = Global::dimX * 8.f - abs( m_moveX - centerX ) / 2.f;
		const auto rangeX = Global::dimX * 16.f - abs( m_moveY - centerY ) * 2.f;
		m_moveX           = qBound( centerX - rangeX, m_moveX, centerX + rangeX );
		m_moveY           = qBound( centerY - rangeY, m_moveY, centerY + rangeY );
	} while ( oldX != m_moveX || oldY != m_moveY );

	GameState::moveX = m_moveX;
	GameState::moveY = m_moveY;

	onRenderParamsChanged();
}

/// @brief Multiplies the current zoom factor by @p factor (mouse wheel zoom).
/// @param factor Zoom multiplier.
void MainWindowRenderer::scale( float factor )
{
	m_scale *= factor;
	m_scale = qBound( 0.25f, m_scale, 15.f );
	GameState::scale = m_scale;
	onRenderParamsChanged();
}

/// @brief Sets the zoom factor to an absolute value.
/// @param scale New zoom factor.
void MainWindowRenderer::setScale( float scale )
{
	m_scale = qBound( 0.25f, scale, 15.f );
	onRenderParamsChanged();
}

/// @brief Sets the top z-level while keeping the active floor's grid aligned.
/// @param level New view level (z coordinate).
void MainWindowRenderer::setViewLevel( int level )
{
	// Shaders use (viewLevel - tile.z), so the active floor already has zero
	// depth offset. Keep the camera unchanged so vertically stacked stairs
	// occupy the same grid position as we browse floors. The caller clamps.
	GameState::viewLevel = level;
	m_viewLevel = level;
	onRenderParamsChanged();
}

/// @brief Uploads any queued tile updates to m_tileUpdateBo and runs the compute shader
///        to apply them into m_tileBo.
void MainWindowRenderer::updateWorld()
{
	if ( !m_pendingUpdates.empty() )
	{
		DebugScope s( "update world" );
		for ( const auto& update : m_pendingUpdates )
		{
			uploadTileData( update );
		}
		m_pendingUpdates.clear();
	}
}

/// @brief Uploads one batch of TileDataUpdate packets to m_tileUpdateBo ready for the
///        compute shader to consume.
/// @param tileData Batch of tile-update records.
void MainWindowRenderer::uploadTileData( const QVector<TileDataUpdate>& tileData )
{
	static bool tracedUpload = false;
	static bool tracedWaterUpload = false;
	if ( !tracedUpload && !tileData.isEmpty() )
	{
		tracedUpload = true;
		const auto& first = tileData.front();
		traceRender( QString( "upload tile id=%1 floor=%2 wall=%3 item=%4 creature=%5 flags=%6 levels=%7" )
			.arg( first.id ).arg( first.tile.floorSpriteUID ).arg( first.tile.wallSpriteUID )
			.arg( first.tile.itemSpriteUID ).arg( first.tile.creatureSpriteUID )
			.arg( first.tile.flags ).arg( first.tile.fluidLevel ) );
		for ( const auto& update : tileData )
		{
			if ( update.tile.floorSpriteUID || update.tile.wallSpriteUID || update.tile.itemSpriteUID || update.tile.creatureSpriteUID )
			{
				traceRender( QString( "first nonempty id=%1 floor=%2 wall=%3 item=%4 creature=%5 flags=%6" )
					.arg( update.id ).arg( update.tile.floorSpriteUID ).arg( update.tile.wallSpriteUID )
					.arg( update.tile.itemSpriteUID ).arg( update.tile.creatureSpriteUID ).arg( update.tile.flags ) );
				break;
			}
		}
	}
	if ( !tracedWaterUpload && !tileData.isEmpty() )
	{
		int waterTiles = 0;
		for ( const auto& update : tileData )
		{
			if ( ( update.tile.flags & 0x00008000u ) != 0u && update.tile.fluidLevel > 0 )
				++waterTiles;
		}
		if ( waterTiles > 0 )
		{
			tracedWaterUpload = true;
			traceRender( QString( "water tile upload count=%1 firstId=%2 firstLevel=%3 firstFlags=%4" )
				.arg( waterTiles ).arg( tileData.front().id ).arg( tileData.front().tile.fluidLevel ).arg( tileData.front().tile.flags ) );
		}
	}
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, m_tileUpdateBo );
	glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( TileDataUpdate ) * tileData.size(), tileData.data(), GL_STREAM_DRAW );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );

	glUseProgram( m_worldUpdateShader );
	setUniformi( m_worldUpdateShader, "uUpdateSize", (GLint)tileData.size() );
	glDispatchCompute( ( tileData.size() + 63 ) / 64, 1, 1 );
	glUseProgram( 0 );
}

/// @brief Re-uploads sprite atlas slices from SpriteFactory when new sprites have been
///        created (signalled via SpriteFactory::textureAdded/creatureTextureAdded).
void MainWindowRenderer::updateTextures()
{
	const bool textureDirty = Global::eventConnector->game()->sf()->textureAdded();
	const bool creatureDirty = Global::eventConnector->game()->sf()->creatureTextureAdded();
	if ( textureDirty || creatureDirty )
	{
		DebugScope s( "update textures" );

		m_texesUsed = Global::eventConnector->game()->sf()->texesUsed();
		traceRender( QString( "texture upload dirty=%1 creature=%2 used=%3 bytes0=%4" )
			.arg( textureDirty ).arg( creatureDirty ).arg( m_texesUsed )
			.arg( Global::eventConnector->game()->sf()->pixelData( 0 ).size() ) );
		if ( qEnvironmentVariableIsSet( "INGNOMIA_LOAD_TRACE_PATH" ) )
		{
			const auto pixels = Global::eventConnector->game()->sf()->pixelData( 0 );
			const auto nonZero = std::count_if( pixels.cbegin(), pixels.cend(), []( std::uint8_t value ) { return value != 0; } );
			traceRender( QString( "texture bytes nonzero=%1 first=%2" ).arg( nonZero ).arg( pixels.isEmpty() ? 0 : pixels.front() ) );
			for ( const int layer : { 0, 31 * 4, 32 * 4 } )
			{
				const auto begin = std::min<std::size_t>( pixels.size(), static_cast<std::size_t>( layer ) * 8192u );
				const auto end = std::min<std::size_t>( pixels.size(), begin + 8192u );
				const auto layerNonZero = std::count_if( pixels.cbegin() + static_cast<std::ptrdiff_t>( begin ), pixels.cbegin() + static_cast<std::ptrdiff_t>( end), []( std::uint8_t value ) { return value != 0; } );
				traceRender( QString( "texture layer=%1 nonzero=%2" ).arg( layer ).arg( layerNonZero ) );
			}
		}

		int maxArrayTextures = Global::cfg->get( "MaxArrayTextures" ).toInt();

		for ( int i = 0; i < m_texesUsed; ++i )
		{
			uploadArrayTexture( i, maxArrayTextures, Global::eventConnector->game()->sf()->pixelData( i ).constData() );
		}
	}
}

/// @brief Slot: caches the latest placement cursor data from AggregatorSelection.
/// @param data        New per-tile SelectionData map.
/// @param noDepthTest True to draw the cursor with depth test disabled.
void MainWindowRenderer::onUpdateSelection( const QMap<unsigned int, SelectionData>& data, bool noDepthTest )
{
	m_selectionData.clear();
	for( const auto& key : data.keys() )
	{
		m_selectionData.insert( key, data[key] );
	}
	m_selectionNoDepthTest = noDepthTest;
}

/// @brief Slot: centres the camera on the given world position (used by "jump to gnome"
///        and event-triggered camera moves).
/// @param target World position to centre on.
void MainWindowRenderer::onCenterCameraPosition( const Position& target )
{
	m_moveX     = 16 * (-target.x + target.y);
	m_moveY     = 8 * ( -target.x - target.y );
	m_viewLevel = target.z;
	onRenderParamsChanged();
	// Notify aggregators (selection, sound, …) that the camera moved — otherwise
	// the cursor-tile mapping in AggregatorSelection stays at its zero defaults
	// until the player nudges the wheel or resizes the window.
	if ( m_parent )
	{
		m_parent->pushRenderParams();
	}
}

/// @brief Helper used by rotate() to adjust camera offsets after a 90° clockwise rotation
///        so the centre point stays in place.
/// @param x In/out X offset.
/// @param y In/out Y offset.
void MainWindowRenderer::updatePositionAfterCWRotation( float& x, float& y )
{
	constexpr int tileHeight = 8; //tiles are assumed to be 8 pixels high (and twice as wide)
	int tmp = x;
	x = -2 * ( Global::dimX * tileHeight + y );
	y = -( Global::dimY - 2 ) * tileHeight + tmp/2;
}

/// @brief Slot: sets the in-menu flag. When true, the renderer runs at menu tick rate (16 ms)
///        instead of the uncapped gameplay loop.
/// @param value True while the main menu is active.
void MainWindowRenderer::onSetInMenu( bool value )
{
	m_inMenu = value;
	traceRender( QString( "onSetInMenu %1" ).arg( value ) );
}
