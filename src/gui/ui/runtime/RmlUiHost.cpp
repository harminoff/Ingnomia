#include "RmlUiHost.h"

#include "IngnomiaRmlUiRenderer.h"
#include "QtRmlFileInterface.h"
#include "QtRmlSystemInterface.h"
#include "../screens/shell/ShellRmlBinding.h"
#include "../screens/hud/HudRmlBinding.h"
#include "../screens/inspector/InspectorRmlBinding.h"
#include "../screens/management6b/Management6BRmlBinding.h"
#if defined(INGNOMIA_DEVELOPER_UI)
#include "../screens/developer_ui/DebugRmlBinding.h"
#endif

#include <RmlUi/Core.h>
#if defined(INGNOMIA_RMLUI_DEBUGGER)
#include <RmlUi/Debugger.h>
#endif

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QThread>
#include <QWindow>

#include <algorithm>

namespace ingnomia::ui
{
namespace
{
RmlUiHost* activeHost = nullptr;

Rml::String toRml( const QString& value )
{
    const QByteArray bytes = value.toUtf8();
    return Rml::String( bytes.constData(), static_cast<size_t>( bytes.size() ) );
}
} // namespace

RmlUiDetachedContext::~RmlUiDetachedContext() = default;

RmlUiDetachedContext::RmlUiDetachedContext( QWindow* window, QString contextName, Rml::Context* context ) :
    m_window( window ),
    m_contextName( std::move( contextName ) ),
    m_context( context ),
    m_input( context )
{
}

RmlUiHost::RmlUiHost() = default;

RmlUiHost::~RmlUiHost()
{
    if ( initialized() && !shutdown() )
    {
        qCritical() << "RmlUiHost left initialized at destruction; explicit shutdown with the owning GL context current is required";
        // Avoid invoking the GL3 destructor without its owning context. This is
        // an intentional fail-safe leak on a broken process-shutdown path.
        m_renderer.release();
    }
}

bool RmlUiHost::initialize( const Config& config )
{
    if ( initialized() )
    {
        qCritical() << "RmlUiHost initialize called more than once";
        return false;
    }
    if ( activeHost && activeHost != this )
    {
        qCritical() << "RmlUi supports one process-global host in this integration";
        return false;
    }
    if ( !requireGuiThread( "initialize" ) || !requireCurrentContext( "initialize" ) ) return false;
    if ( !config.window )
    {
        qCritical() << "RmlUiHost requires its Qt window owner";
        return false;
    }
    if ( config.contextName.isEmpty() )
    {
        qCritical() << "RmlUiHost requires a non-empty unique context name";
        return false;
    }

    activeHost = this;
    m_ownerWindow = config.window;
    m_contextName = config.contextName;
    m_system = std::make_unique<QtRmlSystemInterface>( config.window );
    m_files = std::make_unique<QtRmlFileInterface>( config.assetRoot );
    if ( !m_files->valid() ) return shutdown() && false;
    m_renderer = std::make_unique<IngnomiaRmlUiRenderer>();
    if ( !*m_renderer )
    {
        qCritical() << "Official RmlUi GL3 renderer initialization failed";
        return shutdown() && false;
    }

    Rml::SetSystemInterface( m_system.get() );
    Rml::SetFileInterface( m_files.get() );
    Rml::SetRenderInterface( m_renderer->interface() );
    if ( !Rml::Initialise() )
    {
        qCritical() << "RmlUi core initialization failed";
        return shutdown() && false;
    }
    m_coreInitialized = true;

    const QSize size( std::max( 1, config.physicalSize.width() ), std::max( 1, config.physicalSize.height() ) );
    m_context = Rml::CreateContext( toRml( m_contextName ), {size.width(), size.height()} );
    if ( !m_context )
    {
        qCritical() << "RmlUi context creation failed:" << m_contextName;
        return shutdown() && false;
    }
    m_input.setContext( m_context );
    if ( !resize( size, config.densityIndependentPixelRatio ) ) return shutdown() && false;

#if defined(INGNOMIA_RMLUI_DEBUGGER)
    if ( config.enableDebugger && !Rml::Debugger::Initialise( m_context ) )
    {
        qCritical() << "RmlUi debugger initialization failed";
        return shutdown() && false;
    }
#else
    if ( config.enableDebugger ) qWarning() << "RmlUi debugger requested in a build that omits debugger support";
#endif

    for ( const QString& font : config.fontFiles )
    {
        if ( Rml::LoadFontFace( toRml( font ) ) ) continue;
        qCritical() << "RmlUi font load failed:" << font;
        return shutdown() && false;
    }

    qInfo().noquote() << "RmlUi host initialized, version" << QString::fromStdString( Rml::GetVersion() )
                      << "context" << m_contextName << "asset root" << m_files->assetRoot();
    return update();
}

bool RmlUiHost::shutdown()
{
    if ( !activeHost && !m_system && !m_files && !m_renderer && !m_coreInitialized && !m_context ) return true;
    if ( !requireGuiThread( "shutdown" ) || !requireCurrentContext( "shutdown" ) ) return false;

    if ( !m_detachedContexts.empty() )
    {
        qCritical() << "RmlUi host shutdown requested while detached contexts are still alive";
        return false;
    }

    m_input.cancelInteraction();
    m_input.setContext( nullptr );
#if defined(INGNOMIA_DEVELOPER_UI)
    if ( m_debugBinding )
    {
        m_debugBinding->shutdown();
        m_debugBinding.reset();
    }
#endif
    if ( m_management6bBinding )
    {
        m_management6bBinding->shutdown();
        m_management6bBinding.reset();
    }
    for ( auto& binding : m_creatureInspectorBindings )
    {
        if ( binding ) binding->shutdown();
    }
    m_creatureInspectorBindings.clear();
    if ( m_inspectorBinding )
    {
        m_inspectorBinding->shutdown();
        m_inspectorBinding.reset();
    }
    if ( m_hudBinding )
    {
        m_hudBinding->shutdown();
        m_hudBinding.reset();
    }
    if ( m_shellBinding )
    {
        m_shellBinding->shutdown();
        m_shellBinding.reset();
    }
    if ( m_context )
    {
        m_context->UnloadAllDocuments();
        if ( !Rml::RemoveContext( toRml( m_contextName ) ) )
            qWarning() << "RmlUi context was already absent during shutdown:" << m_contextName;
        m_context = nullptr;
    }
    if ( m_coreInitialized )
    {
        Rml::Shutdown();
        m_coreInitialized = false;
    }

    Rml::SetRenderInterface( nullptr );
    Rml::SetFileInterface( nullptr );
    Rml::SetSystemInterface( nullptr );
    m_renderer.reset();
    m_files.reset();
    m_system.reset();
    m_contextName.clear();
    m_ownerWindow.clear();
    if ( activeHost == this ) activeHost = nullptr;
    qInfo() << "RmlUi host shutdown completed";
    return true;
}

bool RmlUiHost::initialized() const noexcept { return m_coreInitialized && m_context && m_renderer; }

bool RmlUiHost::resize( QSize physicalSize, float densityIndependentPixelRatio )
{
    if ( !initialized() || !requireCurrentContext( "resize" ) ) return false;
    const int width = std::max( 1, physicalSize.width() );
    const int height = std::max( 1, physicalSize.height() );
    const float ratio = std::clamp( densityIndependentPixelRatio, 0.25f, 8.0f );
    m_context->SetDimensions( {width, height} );
    m_context->SetDensityIndependentPixelRatio( ratio );
    m_renderer->setViewport( width, height );
    return true;
}

std::unique_ptr<RmlUiDetachedContext> RmlUiHost::createDetachedContext( QWindow* window,
    const QString& contextName, QSize physicalSize, float densityIndependentPixelRatio )
{
    if ( !initialized() || !window || contextName.isEmpty() || !requireCurrentContext( "createDetachedContext" ) )
        return nullptr;
    if ( std::any_of( m_detachedContexts.begin(), m_detachedContexts.end(),
        [&]( const RmlUiDetachedContext* value ) { return value && value->name() == contextName; } ) )
    {
        qCritical() << "RmlUi detached context name is already in use:" << contextName;
        return nullptr;
    }

    const int width = std::max( 1, physicalSize.width() );
    const int height = std::max( 1, physicalSize.height() );
    // Keep all contexts on the primary render interface. RmlUi's core then
    // gives them one RenderManager, so font/geometry resources cannot cross
    // renderer instances while each native window remains independently
    // movable and is rendered on the same GUI thread.
    Rml::Context* context = Rml::CreateContext( toRml( contextName ), { width, height }, m_renderer->interface() );
    if ( !context )
    {
        qCritical() << "RmlUi detached context creation failed:" << contextName;
        return nullptr;
    }
    context->SetDensityIndependentPixelRatio( std::clamp( densityIndependentPixelRatio, 0.25f, 8.0f ) );
    auto detached = std::unique_ptr<RmlUiDetachedContext>( new RmlUiDetachedContext( window, contextName, context ) );
    m_detachedContexts.push_back( detached.get() );
    setSystemWindow( m_ownerWindow.data() );
    return detached;
}

bool RmlUiHost::destroyDetachedContext( std::unique_ptr<RmlUiDetachedContext>& detached )
{
    if ( !detached ) return true;
    if ( !initialized() || !requireCurrentContext( "destroyDetachedContext" ) ) return false;
    setSystemWindow( detached->window() );
    if ( detached->context() )
    {
        detached->input().cancelInteraction();
        detached->context()->UnloadAllDocuments();
        if ( !Rml::RemoveContext( toRml( detached->name() ) ) )
            qWarning() << "RmlUi detached context was already absent during shutdown:" << detached->name();
    }
    std::erase( m_detachedContexts, detached.get() );
    detached.reset();
    setSystemWindow( m_ownerWindow.data() );
    return true;
}

bool RmlUiHost::resizeDetached( RmlUiDetachedContext& detached, QSize physicalSize, float densityIndependentPixelRatio )
{
    if ( !initialized() || !detached.context() || !requireCurrentContext( "resizeDetached" ) ) return false;
    const int width = std::max( 1, physicalSize.width() );
    const int height = std::max( 1, physicalSize.height() );
    detached.context()->SetDimensions( { width, height } );
    detached.context()->SetDensityIndependentPixelRatio( std::clamp( densityIndependentPixelRatio, 0.25f, 8.0f ) );
    if ( !m_renderer ) return false;
    m_renderer->setViewport( width, height );
    return true;
}

bool RmlUiHost::updateDetached( RmlUiDetachedContext& detached )
{
    if ( !initialized() || !detached.context() || !requireCurrentContext( "updateDetached" ) ) return false;
    setSystemWindow( detached.window() );
    // Context::Update performs layout and input processing, not rasterization.
    // Keep the host's single render/resource boundary untouched here; the
    // native surface is selected explicitly by renderDetached() below.
    const bool result = detached.context()->Update();
    setSystemWindow( m_ownerWindow.data() );
    return result;
}

bool RmlUiHost::renderDetached( RmlUiDetachedContext& detached )
{
    if ( !initialized() || !detached.context() || !requireCurrentContext( "renderDetached" ) ) return false;
    setSystemWindow( detached.window() );
    auto* renderer = m_renderer.get();
    const auto dimensions = detached.context()->GetDimensions();
    if ( renderer ) renderer->setViewport( dimensions.x, dimensions.y );
    if ( !renderer || !renderer->beginFrame() )
    {
        setSystemWindow( m_ownerWindow.data() );
        return false;
    }
    detached.context()->Render();
    const bool result = renderer->endFrame();
    setSystemWindow( m_ownerWindow.data() );
    return result;
}

Rml::ElementDocument* RmlUiHost::loadDocument( RmlUiDetachedContext& detached, const QString& logicalPath, bool show )
{
    if ( !initialized() || !detached.context() || !requireCurrentContext( "loadDetachedDocument" ) ) return nullptr;
    setSystemWindow( detached.window() );
    Rml::ElementDocument* document = detached.context()->LoadDocument( toRml( logicalPath ) );
    if ( !document )
    {
        qCritical() << "RmlUi detached document load failed:" << logicalPath;
        setSystemWindow( m_ownerWindow.data() );
        return nullptr;
    }
    if ( show ) document->Show();
    setSystemWindow( m_ownerWindow.data() );
    return document;
}

void RmlUiHost::setSystemWindow( QWindow* window )
{
    if ( m_system ) m_system->setWindow( window ? window : m_ownerWindow.data() );
}

void RmlUiHost::setCameraPreviewTexture( unsigned int texture, int width, int height )
{
    if ( m_renderer ) m_renderer->setCameraPreviewTexture( texture, width, height );
}

void RmlUiHost::setCameraPreviewTexture( const std::string& source, unsigned int texture, int width, int height )
{
    if ( m_renderer ) m_renderer->setCameraPreviewTexture( source, texture, width, height );
}

bool RmlUiHost::update()
{
    if ( !initialized() ) return false;
    setSystemWindow( m_ownerWindow.data() );
    return m_context->Update();
}

bool RmlUiHost::render()
{
    if ( !initialized() || !requireCurrentContext( "render" ) ) return false;
    setSystemWindow( m_ownerWindow.data() );
    const auto dimensions = m_context->GetDimensions();
    m_renderer->setViewport( dimensions.x, dimensions.y );
    if ( !m_renderer->beginFrame() ) return false;
    m_context->Render();
    return m_renderer->endFrame();
}

double RmlUiHost::nextUpdateDelay() const
{
    return initialized() ? m_context->GetNextUpdateDelay() : -1.0;
}

Rml::ElementDocument* RmlUiHost::loadDocument( const QString& logicalPath, bool show )
{
    if ( !initialized() || !requireCurrentContext( "loadDocument" ) ) return nullptr;
    Rml::ElementDocument* document = m_context->LoadDocument( toRml( logicalPath ) );
    if ( !document )
    {
        qCritical() << "RmlUi document load failed:" << logicalPath;
        return nullptr;
    }
    if ( show ) document->Show();
    return document;
}

bool RmlUiHost::unloadDocument( Rml::ElementDocument* document )
{
    if ( !initialized() || !document || !requireCurrentContext( "unloadDocument" ) ) return false;
    m_context->UnloadDocument( document );
    return true;
}

Rml::Context* RmlUiHost::context() const noexcept { return m_context; }
RmlUiQtInputAdapter& RmlUiHost::input() noexcept { return m_input; }

shell::ShellRmlBinding* RmlUiHost::createShellBinding()
{
    if ( !initialized() || m_shellBinding ) return nullptr;
    m_shellBinding = std::make_unique<shell::ShellRmlBinding>( *m_context );
    return m_shellBinding.get();
}

hud::HudRmlBinding* RmlUiHost::createHudBinding()
{
	if( !m_context || m_hudBinding ) return m_hudBinding.get();
	m_hudBinding = std::make_unique<hud::HudRmlBinding>( *m_context );
	return m_hudBinding.get();
}

inspector::InspectorRmlBinding* RmlUiHost::createInspectorBinding()
{
	if( !m_context || m_inspectorBinding ) return m_inspectorBinding.get();
	m_inspectorBinding = std::make_unique<inspector::InspectorRmlBinding>( *m_context );
	return m_inspectorBinding.get();
}

inspector::InspectorRmlBinding* RmlUiHost::createCreatureInspectorBinding( int cameraSlot, int windowIndex )
{
    if ( !m_context ) return nullptr;
    m_creatureInspectorBindings.push_back( std::make_unique<inspector::InspectorRmlBinding>( *m_context, cameraSlot, windowIndex ) );
    return m_creatureInspectorBindings.back().get();
}

management6b::Management6BRmlBinding* RmlUiHost::createManagement6BBinding()
{
	if( !m_context || m_management6bBinding ) return m_management6bBinding.get();
	m_management6bBinding = std::make_unique<management6b::Management6BRmlBinding>( *m_context );
	return m_management6bBinding.get();
}

#if defined(INGNOMIA_DEVELOPER_UI)
debug::DebugRmlBinding* RmlUiHost::createDebugBinding()
{
	if( !m_context || m_debugBinding ) return m_debugBinding.get();
	m_debugBinding = std::make_unique<debug::DebugRmlBinding>( *m_context );
	return m_debugBinding.get();
}
#endif

bool RmlUiHost::requireGuiThread( const char* operation ) const
{
    if ( !QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread() ) return true;
    qCritical() << "RmlUiHost operation must run on the Qt GUI thread:" << operation;
    return false;
}

bool RmlUiHost::requireCurrentContext( const char* operation ) const
{
    if ( QOpenGLContext::currentContext() ) return true;
    qCritical() << "RmlUiHost operation requires the owning Qt OpenGL context to be current:" << operation;
    return false;
}
} // namespace ingnomia::ui
