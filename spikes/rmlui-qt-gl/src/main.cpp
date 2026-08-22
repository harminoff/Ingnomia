#include "IngnomiaRmlUiRenderer.h"
#include "gui/ui/screens/shell/ShellController.h"
#include "gui/ui/screens/shell/ShellDataAdapter.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include "gui/ui/screens/hud/HudController.h"
#include "gui/ui/screens/hud/HudRmlBinding.h"
#include "gui/ui/screens/inspector/InspectorController.h"
#include "gui/ui/screens/inspector/InspectorRmlBinding.h"
#include "gui/ui/screens/management6b/Management6BController.h"
#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
#include "gui/ui/screens/management6a/Management6AController.h"
#include "gui/ui/screens/management6a/Management6ARmlBinding.h"
#include "gui/ui/screens/management6c/Management6CController.h"
#include "gui/ui/screens/management6c/Management6CRmlBinding.h"
#include "gui/ui/screens/developer_ui/DebugController.h"
#include "gui/ui/screens/developer_ui/DebugRmlBinding.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include <glad/gl.h>

#include <QByteArray>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPointer>
#include <QResizeEvent>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace
{
[[noreturn]] void hudProofFailure( const char* message )
{
    QFile failure( QCoreApplication::applicationDirPath() + "/self-test-failure.log" );
    if ( failure.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) { failure.write( message ); failure.close(); }
    qCritical().noquote() << message;
    std::exit( 2 );
}

QString toQString( const Rml::String& text )
{
    return QString::fromUtf8( text.data(), static_cast<qsizetype>( text.size() ) );
}

bool ensureProofTga( const QString& path )
{
    constexpr int width = 64;
    constexpr int height = 64;
    QByteArray bytes( 18 + width * height * 4, '\0' );
    bytes[2] = 2; // Uncompressed true-colour image.
    bytes[12] = static_cast<char>( width & 0xff );
    bytes[13] = static_cast<char>( ( width >> 8 ) & 0xff );
    bytes[14] = static_cast<char>( height & 0xff );
    bytes[15] = static_cast<char>( ( height >> 8 ) & 0xff );
    bytes[16] = 32;
    bytes[17] = 0x28; // Eight alpha bits, top-left origin.
    for ( int y = 0; y < height; ++y )
    {
        for ( int x = 0; x < width; ++x )
        {
            const int dx = x - width / 2;
            const int dy = y - height / 2;
            const bool mineral = dx * dx + dy * dy < 19 * 19;
            const int offset = 18 + 4 * ( y * width + x );
            bytes[offset + 0] = static_cast<char>( mineral ? 0xb2 : 0x68 ); // B
            bytes[offset + 1] = static_cast<char>( mineral ? 0xdf : 0x6f ); // G
            bytes[offset + 2] = static_cast<char>( mineral ? 0xf3 : 0x2d ); // R
            bytes[offset + 3] = static_cast<char>( 0xff );
        }
    }
    QFile file( path );
    return file.open( QIODevice::WriteOnly | QIODevice::Truncate ) && file.write( bytes ) == bytes.size();
}

class QtSystemInterface final : public Rml::SystemInterface
{
public:
    explicit QtSystemInterface( QWindow* window ) : m_window( window ) { m_timer.start(); }

    double GetElapsedTime() override { return 0.001 * static_cast<double>( m_timer.elapsed() ); }

    bool LogMessage( Rml::Log::Type type, const Rml::String& message ) override
    {
        qInfo().noquote() << "[RmlUi" << static_cast<int>( type ) << "]" << toQString( message );
        return true;
    }

    int TranslateString( Rml::String& translated, const Rml::String& input ) override
    {
        translated = input;
        return 0;
    }

    void SetMouseCursor( const Rml::String& cursor_name ) override
    {
        if ( !m_window ) return;
        const QString name = toQString( cursor_name );
        Qt::CursorShape shape = Qt::ArrowCursor;
        if ( name == "pointer" ) shape = Qt::PointingHandCursor;
        else if ( name == "text" ) shape = Qt::IBeamCursor;
        else if ( name.contains( "resize" ) ) shape = Qt::SizeAllCursor;
        else if ( name.startsWith( "rmlui-scroll-" ) ) shape = Qt::SizeAllCursor;
        m_window->setCursor( QCursor( shape ) );
    }

    void SetClipboardText( const Rml::String& text ) override
    {
        QGuiApplication::clipboard()->setText( toQString( text ) );
    }

    void GetClipboardText( Rml::String& text ) override
    {
        text = QGuiApplication::clipboard()->text().toUtf8().toStdString();
    }

    void ActivateKeyboard( Rml::Vector2f caret_position, float line_height ) override
    {
        if ( !m_window ) return;
        const qreal dpr = m_window->devicePixelRatio();
        const QRectF logical_rect( caret_position.x / dpr, caret_position.y / dpr, 1.0, line_height / dpr );
        QGuiApplication::inputMethod()->setInputItemRectangle( logical_rect );
        QGuiApplication::inputMethod()->show();
    }

    void DeactivateKeyboard() override { QGuiApplication::inputMethod()->hide(); }

private:
    QPointer<QWindow> m_window;
    QElapsedTimer m_timer;
};

class QtFileInterface final : public Rml::FileInterface
{
    struct Handle { explicit Handle( QString path ) : file( std::move( path ) ) {} QFile file; };

public:
    explicit QtFileInterface( QString root ) : m_root( QFileInfo( std::move( root ) ).absoluteFilePath() ) {}

    Rml::FileHandle Open( const Rml::String& path ) override
    {
        const QString candidate = QFileInfo( m_root + "/" + toQString( path ) ).absoluteFilePath();
        const QString prefix = m_root.endsWith( '/' ) ? m_root : m_root + '/';
        if ( candidate != m_root && !candidate.startsWith( prefix ) )
        {
            qWarning() << "Rejected RmlUi path outside asset root:" << candidate;
            return {};
        }
        auto handle = std::make_unique<Handle>( candidate );
        if ( !handle->file.open( QIODevice::ReadOnly ) )
        {
            qWarning() << "Failed to open RmlUi asset:" << candidate;
            return {};
        }
        return reinterpret_cast<Rml::FileHandle>( handle.release() );
    }

    void Close( Rml::FileHandle file ) override { delete reinterpret_cast<Handle*>( file ); }

    size_t Read( void* buffer, size_t size, Rml::FileHandle file ) override
    {
        const qint64 read = reinterpret_cast<Handle*>( file )->file.read( static_cast<char*>( buffer ), static_cast<qint64>( size ) );
        return read > 0 ? static_cast<size_t>( read ) : 0;
    }

    bool Seek( Rml::FileHandle file, long offset, int origin ) override
    {
        QFile& stream = reinterpret_cast<Handle*>( file )->file;
        qint64 position = offset;
        if ( origin == SEEK_CUR ) position += stream.pos();
        else if ( origin == SEEK_END ) position += stream.size();
        else if ( origin != SEEK_SET ) return false;
        return stream.seek( position );
    }

    size_t Tell( Rml::FileHandle file ) override
    {
        return static_cast<size_t>( reinterpret_cast<Handle*>( file )->file.pos() );
    }

private:
    QString m_root;
};

Rml::Input::KeyIdentifier toRmlKey( int key )
{
    using namespace Rml::Input;
    if ( key >= Qt::Key_A && key <= Qt::Key_Z )
        return static_cast<KeyIdentifier>( static_cast<int>( KI_A ) + key - Qt::Key_A );
    if ( key >= Qt::Key_0 && key <= Qt::Key_9 )
        return static_cast<KeyIdentifier>( static_cast<int>( KI_0 ) + key - Qt::Key_0 );
    switch ( key )
    {
        case Qt::Key_Backspace: return KI_BACK;
        case Qt::Key_Tab: return KI_TAB;
        case Qt::Key_Return:
        case Qt::Key_Enter: return KI_RETURN;
        case Qt::Key_Escape: return KI_ESCAPE;
        case Qt::Key_Space: return KI_SPACE;
        case Qt::Key_PageUp: return KI_PRIOR;
        case Qt::Key_PageDown: return KI_NEXT;
        case Qt::Key_End: return KI_END;
        case Qt::Key_Home: return KI_HOME;
        case Qt::Key_Left: return KI_LEFT;
        case Qt::Key_Up: return KI_UP;
        case Qt::Key_Right: return KI_RIGHT;
        case Qt::Key_Down: return KI_DOWN;
        case Qt::Key_Delete: return KI_DELETE;
        default: return KI_UNKNOWN;
    }
}

int toRmlModifiers( Qt::KeyboardModifiers modifiers )
{
    int result = 0;
    if ( modifiers.testFlag( Qt::ShiftModifier ) ) result |= Rml::Input::KM_SHIFT;
    if ( modifiers.testFlag( Qt::ControlModifier ) ) result |= Rml::Input::KM_CTRL;
    if ( modifiers.testFlag( Qt::AltModifier ) ) result |= Rml::Input::KM_ALT;
    if ( modifiers.testFlag( Qt::MetaModifier ) ) result |= Rml::Input::KM_META;
    return result;
}

int toButtonIndex( Qt::MouseButton button )
{
    if ( button == Qt::LeftButton ) return 0;
    if ( button == Qt::RightButton ) return 1;
    if ( button == Qt::MiddleButton ) return 2;
    return -1;
}

struct GlState
{
    GLint framebuffer = 0;
    GLint program = 0;
    GLint vertex_array = 0;
    GLint active_texture = 0;
    GLint viewport[4] = {};
    GLboolean blend = GL_FALSE;
    GLboolean depth = GL_FALSE;
    GLboolean stencil = GL_FALSE;

    static GlState capture()
    {
        GlState state;
        glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &state.framebuffer );
        glGetIntegerv( GL_CURRENT_PROGRAM, &state.program );
        glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &state.vertex_array );
        glGetIntegerv( GL_ACTIVE_TEXTURE, &state.active_texture );
        glGetIntegerv( GL_VIEWPORT, state.viewport );
        state.blend = glIsEnabled( GL_BLEND );
        state.depth = glIsEnabled( GL_DEPTH_TEST );
        state.stencil = glIsEnabled( GL_STENCIL_TEST );
        return state;
    }

    bool compatibleWith( const GlState& other ) const
    {
        return framebuffer == other.framebuffer && program == other.program && vertex_array == other.vertex_array &&
            active_texture == other.active_texture && std::equal( std::begin( viewport ), std::end( viewport ), std::begin( other.viewport ) ) &&
            blend == other.blend && depth == other.depth && stencil == other.stencil;
    }

    void logDifferences( const GlState& other ) const
    {
        if ( framebuffer != other.framebuffer ) qWarning() << "  draw framebuffer:" << framebuffer << "->" << other.framebuffer;
        if ( program != other.program ) qWarning() << "  program:" << program << "->" << other.program;
        if ( vertex_array != other.vertex_array ) qWarning() << "  vertex array:" << vertex_array << "->" << other.vertex_array;
        if ( active_texture != other.active_texture ) qWarning() << "  active texture:" << active_texture << "->" << other.active_texture;
        if ( !std::equal( std::begin( viewport ), std::end( viewport ), std::begin( other.viewport ) ) )
            qWarning() << "  viewport changed";
        if ( blend != other.blend ) qWarning() << "  blend enable:" << blend << "->" << other.blend;
        if ( depth != other.depth ) qWarning() << "  depth enable:" << depth << "->" << other.depth;
        if ( stencil != other.stencil ) qWarning() << "  stencil enable:" << stencil << "->" << other.stencil;
    }
};

class SpikeWindow final : public QWindow
{
public:
    explicit SpikeWindow( bool selfTest, bool shellSelfTest, bool hudSelfTest, bool inspectorSelfTest, bool management6bSelfTest, bool management6aSelfTest, bool management6cSelfTest, bool developerUiSelfTest ) : m_self_test( selfTest ), m_shell_self_test( shellSelfTest ), m_hud_self_test( hudSelfTest ), m_inspector_self_test( inspectorSelfTest ), m_management6b_self_test( management6bSelfTest ), m_management6a_self_test( management6aSelfTest ), m_management6c_self_test( management6cSelfTest ), m_developer_ui_self_test( developerUiSelfTest )
    {
        QSurfaceFormat format;
        format.setRenderableType( QSurfaceFormat::OpenGL );
        format.setProfile( QSurfaceFormat::CoreProfile );
        format.setVersion( 4, 3 );
        format.setSwapBehavior( QSurfaceFormat::DoubleBuffer );
        format.setDepthBufferSize( 24 );
        format.setStencilBufferSize( 8 );
        setFormat( format );
        setSurfaceType( QWindow::OpenGLSurface );
        setTitle( "Ingnomia RmlUi Qt/OpenGL spike" );
        resize( 1200, 675 );
        connect( &m_tick, &QTimer::timeout, this, [this] { requestUpdate(); } );
        m_tick.start( 16 );
    }

    ~SpikeWindow() override { shutdown(); }

protected:
    bool event( QEvent* event ) override
    {
        if ( event->type() == QEvent::UpdateRequest )
        {
            if ( isExposed() ) render();
            return true;
        }
        if ( event->type() == QEvent::InputMethod )
        {
            auto* input = static_cast<QInputMethodEvent*>( event );
            if ( m_rml_context && !input->commitString().isEmpty() )
            {
                const bool consumed = !m_rml_context->ProcessTextInput( input->commitString().toUtf8().toStdString() );
                if ( consumed ) { event->accept(); requestUpdate(); return true; }
            }
        }
#if QT_VERSION >= QT_VERSION_CHECK( 6, 6, 0 )
        if ( event->type() == QEvent::DevicePixelRatioChange ) resizeRmlUi();
#endif
        return QWindow::event( event );
    }

    void exposeEvent( QExposeEvent* ) override
    {
        if ( isExposed() && !m_gl_context ) initialize();
        if ( isExposed() ) requestUpdate();
    }

    void closeEvent( QCloseEvent* event ) override
    {
        shutdown();
        QWindow::closeEvent( event );
    }

    void resizeEvent( QResizeEvent* event ) override
    {
        QWindow::resizeEvent( event );
        resizeRmlUi();
    }

    void keyPressEvent( QKeyEvent* event ) override
    {
        if ( event->key() == Qt::Key_F6 ) { runLifecycleProbe(); return; }
        if ( event->key() == Qt::Key_F7 )
        {
            Rml::Debugger::SetVisible( !Rml::Debugger::IsVisible() );
            requestUpdate();
            return;
        }
        if ( event->key() == Qt::Key_F8 ) { m_capture_requested = true; requestUpdate(); return; }
        if ( event->key() == Qt::Key_F9 || event->key() == Qt::Key_F10 )
        {
            m_user_scale = std::clamp( m_user_scale + ( event->key() == Qt::Key_F10 ? 0.25f : -0.25f ), 0.75f, 2.0f );
            resizeRmlUi();
            qInfo() << "UI scale:" << m_user_scale;
            return;
        }
        if ( !m_rml_context ) return;
        const int modifiers = toRmlModifiers( event->modifiers() );
        bool consumed = false;
        const auto key = toRmlKey( event->key() );
        if ( key != Rml::Input::KI_UNKNOWN ) consumed = !m_rml_context->ProcessKeyDown( key, modifiers );
        if ( !event->text().isEmpty() && !( event->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) ) )
            consumed = !m_rml_context->ProcessTextInput( event->text().toUtf8().toStdString() ) || consumed;
        if ( consumed ) event->accept();
        requestUpdate();
    }

    void keyReleaseEvent( QKeyEvent* event ) override
    {
        if ( !m_rml_context ) return;
        const auto key = toRmlKey( event->key() );
        if ( key != Rml::Input::KI_UNKNOWN && !m_rml_context->ProcessKeyUp( key, toRmlModifiers( event->modifiers() ) ) ) event->accept();
        requestUpdate();
    }

    void mouseMoveEvent( QMouseEvent* event ) override
    {
        if ( !m_rml_context ) return;
        const QPointF pixel = event->position() * devicePixelRatio();
        m_rml_context->ProcessMouseMove( std::lround( pixel.x() ), std::lround( pixel.y() ), toRmlModifiers( event->modifiers() ) );
        requestUpdate();
    }

    void mousePressEvent( QMouseEvent* event ) override
    {
        if ( !m_rml_context ) return;
        const int button = toButtonIndex( event->button() );
        if ( button < 0 ) return;
        m_ui_capture[button] = !m_rml_context->ProcessMouseButtonDown( button, toRmlModifiers( event->modifiers() ) );
        if ( m_ui_capture[button] ) event->accept();
        requestUpdate();
    }

    void mouseReleaseEvent( QMouseEvent* event ) override
    {
        if ( !m_rml_context ) return;
        const int button = toButtonIndex( event->button() );
        if ( button < 0 ) return;
        m_rml_context->ProcessMouseButtonUp( button, toRmlModifiers( event->modifiers() ) );
        if ( m_ui_capture[button] ) event->accept();
        m_ui_capture[button] = false;
        requestUpdate();
    }

    void wheelEvent( QWheelEvent* event ) override
    {
        if ( !m_rml_context ) return;
        const QPoint angle = event->angleDelta();
        const Rml::Vector2f delta( -angle.x() / 120.0f, -angle.y() / 120.0f );
        const bool unconsumed = m_rml_context->ProcessMouseWheel( delta, toRmlModifiers( event->modifiers() ) );
        if ( !unconsumed ) { event->accept(); qInfo() << "Wheel consumed by UI"; }
        else qInfo() << "Wheel reached world fallback";
        requestUpdate();
    }

    void focusOutEvent( QFocusEvent* event ) override
    {
        if ( m_rml_context ) m_rml_context->ProcessMouseLeave();
        m_ui_capture.fill( false );
        QWindow::focusOutEvent( event );
    }

private:
    class ShellProofPort final : public ingnomia::ui::shell::ShellCommandPort
    {
    public:
        ingnomia::ui::shell::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action ) override
        {
            actions.push_back( action );
            const bool pending = action.id.value.starts_with( "app." ) || action.id.value == "sim.set_paused"
                || action.id.value == "settings.set_draft" || action.id.value == "load.refresh";
            qInfo().noquote() << "Shell proof dispatch:" << QString::fromStdString( action.id.value )
                << "request" << action.request.value << "pending" << pending;
            return { ingnomia::ui::shell::CommandStatus::Accepted, std::nullopt, pending };
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
    };

    class HudProofPort final : public ingnomia::ui::hud::HudCommandPort
    {
    public:
        ingnomia::ui::hud::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action ) override
        {
            actions.push_back( action );
            qInfo().noquote() << "HUD proof dispatch:" << QString::fromStdString( action.id.value ) << "request" << action.request.value;
            return {};
        }
        void registerPrompt( ingnomia::ui::PromptInstanceId instance, std::optional<ingnomia::ui::EventResponseTargetId> target ) override
        {
            prompts.emplace_back( instance, target );
        }
        ingnomia::ui::hud::CommandResult requestBuildItems( ::BuildSelection selection, std::string_view category ) override
        {
            buildRequests.emplace_back( selection, std::string( category ) );
            return {};
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
        std::vector<std::pair<ingnomia::ui::PromptInstanceId, std::optional<ingnomia::ui::EventResponseTargetId>>> prompts;
        std::vector<std::pair<::BuildSelection, std::string>> buildRequests;
    };

    class InspectorProofPort final : public ingnomia::ui::inspector::InspectorCommandPort
    {
    public:
        ingnomia::ui::inspector::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action ) override
        {
            actions.push_back( action );
            qInfo().noquote() << "Inspector proof dispatch:" << QString::fromStdString( action.id.value ) << "request" << action.request.value;
            return {};
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
    };

    class Management6BProofPort final : public ingnomia::ui::management6b::CommandPort
    {
    public:
        ingnomia::ui::management6b::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action ) override
        {
            actions.push_back( action );
            qInfo().noquote() << "Management 6B proof dispatch:" << QString::fromStdString( action.id.value ) << "request" << action.request.value;
            return {};
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
    };

    class Management6AProofPort final : public ingnomia::ui::management6a::CommandPort
    {
    public:
        ingnomia::ui::management6a::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action, ingnomia::ui::management6a::DispatchOrigin origin ) override
        {
            actions.push_back( action ); origins.push_back( origin ); return {};
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
        std::vector<ingnomia::ui::management6a::DispatchOrigin> origins;
    };

    class Management6CProofPort final : public ingnomia::ui::management6c::CommandPort
    {
    public:
        ingnomia::ui::management6c::CommandResult dispatch( const ingnomia::ui::UiActionEnvelope& action,
            ingnomia::ui::management6c::DispatchOrigin origin ) override
        {
            actions.push_back( action ); origins.push_back( origin );
            return { ingnomia::ui::management6c::CommandStatus::Accepted, false, {} };
        }
        std::vector<ingnomia::ui::UiActionEnvelope> actions;
        std::vector<ingnomia::ui::management6c::DispatchOrigin> origins;
    };

    class DeveloperUiProofPort final : public ingnomia::ui::debug::CommandPort
    {
    public:
        bool dispatch( const ingnomia::ui::debug::DebugAction& action ) override { actions.push_back(action); return true; }
        std::vector<ingnomia::ui::debug::DebugAction> actions;
    };

    void initialize()
    {
        m_gl_context = std::make_unique<QOpenGLContext>();
        m_gl_context->setFormat( requestedFormat() );
        if ( !m_gl_context->create() || !m_gl_context->makeCurrent( this ) ) qFatal( "Failed to create/make current Qt OpenGL context" );
        const auto loader = []( const char* name ) -> GLADapiproc {
            return reinterpret_cast<GLADapiproc>( QOpenGLContext::currentContext()->getProcAddress( name ) );
        };
        if ( !gladLoadGL( loader ) ) qFatal( "Failed to initialize Ingnomia GLAD" );
        qInfo() << "GL vendor:" << reinterpret_cast<const char*>( glGetString( GL_VENDOR ) );
        qInfo() << "GL renderer:" << reinterpret_cast<const char*>( glGetString( GL_RENDERER ) );
        qInfo() << "GL version:" << reinterpret_cast<const char*>( glGetString( GL_VERSION ) );

        m_renderer = std::make_unique<ingnomia::ui::IngnomiaRmlUiRenderer>();
        if ( !*m_renderer ) qFatal( "Failed to initialize official RmlUi GL3 renderer" );
        const QString assets = QCoreApplication::applicationDirPath() + "/assets";
        if ( !ensureProofTga( assets + "/spike.tga" ) ) qFatal( "Failed to generate uncompressed TGA proof asset" );
        m_system = std::make_unique<QtSystemInterface>( this );
        m_files = std::make_unique<QtFileInterface>( assets );
        Rml::SetRenderInterface( m_renderer->interface() );
        Rml::SetSystemInterface( m_system.get() );
        Rml::SetFileInterface( m_files.get() );
        if ( !Rml::Initialise() ) qFatal( "RmlUi initialization failed" );

        const QSize pixels = physicalSize();
        m_rml_context = Rml::CreateContext( "ingnomia-qt-gl-spike", {pixels.width(), pixels.height()} );
        if ( !m_rml_context ) qFatal( "RmlUi context creation failed" );
        resizeRmlUi();
        if ( !Rml::Debugger::Initialise( m_rml_context ) ) qFatal( "RmlUi debugger initialization failed" );
        if ( !Rml::LoadFontFace( "fonts/LatoLatin-Regular.ttf" ) ) qFatal( "RmlUi font load failed" );

        if ( m_shell_self_test ) initializeShellProof();
        else if ( m_hud_self_test ) initializeHudProof();
        else if ( m_inspector_self_test ) initializeInspectorProof();
        else if ( m_management6b_self_test ) initializeManagement6BProof();
        else if ( m_management6a_self_test ) initializeManagement6AProof();
        else if ( m_management6c_self_test ) initializeManagement6CProof();
        else if ( m_developer_ui_self_test ) initializeDeveloperUiProof();
        else
        {
            auto model = m_rml_context->CreateDataModel( "spike" );
            model.Bind( "click_count", &m_click_count );
            model.Bind( "input_text", &m_input_text );
            model.BindEventCallback( "increment", [this]( Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList& ) {
                ++m_click_count;
                handle.DirtyVariable( "click_count" );
                qInfo() << "Button callback count:" << m_click_count;
                requestUpdate();
            } );
            m_model = model.GetModelHandle();
            m_document = m_rml_context->LoadDocument( "spike.rml" );
            if ( !m_document ) qFatal( "RmlUi document load failed" );
            m_document->Show();
        }
        runLifecycleProbe();
    }

    void initializeShellProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::shell;
        m_shell_binding = std::make_unique<ShellRmlBinding>( *m_rml_context );
        m_shell_controller = std::make_unique<ShellController>( m_shell_port, *m_shell_binding );
        if ( !m_shell_binding->initialize( *m_shell_controller ) ) qFatal( "Shell binding initialization failed" );
        m_shell_controller->setVersion( "shell-proof" );
        NewGameState newGame;
        newGame.status = RequestStatus::Ready;
        m_shell_controller->setNewGameState( newGame );
        m_shell_controller->setSettingsState( ShellDataAdapter::settings( { false, 1.25f, 40, 30, true } ) );
        m_shell_controller->setLoadGameState( ShellDataAdapter::saves( {}, {} ) );
    }

    void advanceShellProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::shell;
        if( !m_shell_self_test || !m_shell_controller || !m_shell_binding ) return;
        if( m_frame == 5 )
        {
            if( !m_shell_binding->activateElement( "shell-new-setup" ) || m_shell_controller->state().route.value != "shell.new_game" ) qFatal( "Shell proof main -> new failed" );
        }
        else if( m_frame == 10 )
        {
            if( !m_shell_binding->activateElement( "shell-back" ) || m_shell_controller->state().route.value != "shell.main_menu" ) qFatal( "Shell proof new -> back failed" );
        }
        else if( m_frame == 15 )
        {
            if( !m_shell_binding->activateElement( "shell-load" ) || m_shell_controller->state().route.value != "shell.load_game" || m_shell_controller->state().loadGame.kingdomsStatus != RequestStatus::Empty ) qFatal( "Shell proof load empty failed" );
        }
        else if( m_frame == 20 ) (void)m_shell_binding->activateElement( "shell-back" );
        else if( m_frame == 25 )
        {
            if( !m_shell_binding->activateElement( "shell-settings" ) || m_shell_controller->state().settings.rows.size() != 5 ) qFatal( "Shell proof supported settings failed" );
        }
        else if( m_frame == 30 ) (void)m_shell_binding->activateElement( "shell-back" );
        else if( m_frame == 35 )
        {
            (void)m_shell_binding->activateElement( "shell-exit" );
            if( !m_shell_binding->activateElement( "confirm-cancel" ) ) qFatal( "Shell proof destructive cancel failed" );
        }
        else if( m_frame == 40 )
        {
            (void)m_shell_binding->activateElement( "shell-exit" );
            if( !m_shell_binding->activateElement( "confirm-accept" ) || m_shell_port.actions.back().id.value != "app.exit" ) qFatal( "Shell proof destructive confirm failed" );
            m_shell_controller->onActionFinished( m_shell_port.actions.back().request, {} );
        }
        else if( m_frame == 45 )
        {
            m_shell_controller->setWorld( WorldEpoch{ 1 } );
            m_shell_controller->activate( ShellControl::OpenPause );
            if( m_shell_controller->state().route.value != "game.pause" || !m_shell_controller->state().pendingRequest ) qFatal( "Shell proof pause pending failed" );
        }
        else if( m_frame == 50 )
        {
            const auto pauseRequest = *m_shell_controller->state().pendingRequest;
            m_shell_controller->onActionFinished( pauseRequest, {} );
            m_shell_controller->onPauseState( true, PauseReason::Player );
            m_shell_controller->activate( ShellControl::Resume );
            if( m_shell_controller->state().route.value != "game.pause" || !m_shell_controller->state().pendingRequest ) qFatal( "Shell proof resume must remain pending until authoritative unpause" );
        }
        else if( m_frame == 55 ) qInfo() << "Shell vertical proof passed: main-new-back, load-empty, settings-5, confirm-cancel/accept, pause-resume-pending";
    }

    void initializeHudProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::hud;
        m_hud_binding = std::make_unique<HudRmlBinding>( *m_rml_context );
        m_hud_controller = std::make_unique<HudController>( m_hud_port, *m_hud_binding );
        if( !m_hud_binding->initialize( *m_hud_controller ) ) hudProofFailure( "HUD binding initialization failed" );
        m_hud_controller->beginWorld( WorldEpoch{ 41 } );
        m_hud_controller->setSettlement( { "Deepdelve", 18, 7, 432 } );
        ClockCalendarState clock; clock.hour=14; clock.minute=37; clock.day=12; clock.year=3; clock.season=Season::Autumn; clock.daylight=DaylightPhase::Day; clock.nextSunEventMinute=1080;
        m_hud_controller->setClock( clock );
        CameraState camera; camera.viewLevel=42; camera.minLevel=0; camera.maxLevel=100; camera.rotation=1; camera.zoom=1.25f; m_hud_controller->setCamera( camera );
        m_hud_controller->setOverlays( { true, false, true, false } );
        ToolState tool; tool.active=ToolId{"mine"}; tool.category=ToolCategory::Dig; tool.phase=ToolPhase::Preview; tool.canRotate=true;
        SelectionSummary selection; selection.width=4; selection.height=3; selection.depth=1; selection.validTiles=10; selection.invalidTiles=2; m_hud_controller->setTool( tool, selection );
        m_hud_controller->setWatchRows( {{ { CatalogId{"tools"},CatalogId{"weapons"},CatalogId{"axe"},{},InventoryDepth::Item }, "Axes", 9 }} );
        m_hud_controller->setBuildCatalog( {
            { CatalogId{"chair"}, "Chair", BuildKind::Item, { CatalogId{"oak"} }, "build_WoodChairFR.tga", 0, 0, 32, 36, 32, 36 },
            { CatalogId{"bench"}, "Bench", BuildKind::Item, { CatalogId{"oak"} }, "build_WoodChairFR.tga", 0, 0, 32, 36, 32, 36 },
            { CatalogId{"table"}, "Table", BuildKind::Item, { CatalogId{"oak"} }, "build_WoodChairFR.tga", 0, 0, 32, 36, 32, 36 },
            { CatalogId{"stool"}, "Stool", BuildKind::Item, { CatalogId{"oak"} }, "build_WoodChairFR.tga", 0, 0, 32, 36, 32, 36 }
        } );
    }

    void advanceHudProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::hud;
        if( !m_hud_self_test || !m_hud_controller || !m_hud_binding ) return;
        const auto activate = [this]( const char* id, std::string_view expected ) {
            const auto before=m_hud_port.actions.size(); if(!m_hud_binding->activateElement(id)||m_hud_port.actions.size()!=before+1||m_hud_port.actions.back().id.value!=expected) hudProofFailure("HUD callback proof failed");
        };
        if( m_frame==4 ) activate("hud_pause","sim.set_paused");
        else if( m_frame==7 ) activate("hud_speed_fast","sim.set_speed");
        else if( m_frame==10 ) activate("hud_level_up","view.change_level");
        else if( m_frame==13 ) activate("hud_overlay_jobs","view.set_overlay");
        else if( m_frame==16 ) activate("hud_tool_mine","tool.activate");
        else if( m_frame==19 ) activate("hud_tool_rotate","tool.rotate");
        else if( m_frame==20 ) { if(!m_hud_binding->activateElement("hud_build_furniture")||m_hud_port.buildRequests.empty()||m_hud_port.buildRequests.back().second!="Chairs")hudProofFailure("HUD furniture category proof failed"); }
        else if( m_frame==21 ) { if(!m_hud_binding->activateElement("hud_build_workshop")||m_hud_port.buildRequests.empty()||m_hud_port.buildRequests.back().second!="Wood")hudProofFailure("HUD workshop category navigation failed"); }
        else if( m_frame==23 ) { if(!m_hud_binding->activateElement("hud_build_furniture")||m_hud_port.buildRequests.empty()||m_hud_port.buildRequests.back().second!="Chairs")hudProofFailure("HUD category return failed"); }
        else if( m_frame==22 ) activate("hud_tool_cancel","tool.cancel");
        else if( m_frame==24 ) activate("hud_build_chair","tool.choose_build");
        else if( m_frame==25 ) { if(!m_hud_controller->enqueuePrompt({},"Caravan arrived","The market is open.",false,false))hudProofFailure("HUD acknowledge enqueue failed"); activate("hud_event_ack","event.respond"); if(!m_hud_controller->state().prompts.empty())hudProofFailure("HUD acknowledge FIFO failed"); }
        else if( m_frame==28 ) { if(!m_hud_controller->enqueuePrompt(EventResponseTargetId{77},"Accept migrants?","Five gnomes seek shelter.",true,true))hudProofFailure("HUD yes/no enqueue failed"); activate("hud_event_yes","event.respond"); if(!m_hud_controller->state().prompts.empty()||!m_hud_port.prompts.back().second||m_hud_port.prompts.back().second->value!=77)hudProofFailure("HUD yes/no FIFO failed"); }
        else if( m_frame==32 )
        {
            auto* button=m_hud_binding->document()->GetElementById("hud_pause"); auto* catalog=m_hud_binding->document()->GetElementById("hud_build_catalog"); if(!button||!catalog)hudProofFailure("HUD pointer target missing"); const auto p=button->GetAbsoluteOffset()+button->GetBox().GetSize()*0.5f;
            m_rml_context->ProcessMouseMove(static_cast<int>(p.x),static_cast<int>(p.y),0); const bool uiDown=!m_rml_context->ProcessMouseButtonDown(0,0); const bool uiUp=!m_rml_context->ProcessMouseButtonUp(0,0); const bool uiWheel=!m_rml_context->ProcessMouseWheel({0,-1},0);
            const auto scroll=catalog->GetAbsoluteOffset()+catalog->GetBox().GetSize()*0.5f; m_rml_context->ProcessMouseMove(static_cast<int>(scroll.x),static_cast<int>(scroll.y),0); const bool uiScrollWheel=!m_rml_context->ProcessMouseWheel({0,1},0);
            m_rml_context->ProcessMouseMove(width()-100,height()/2,0); const bool worldDown=m_rml_context->ProcessMouseButtonDown(0,0); m_rml_context->ProcessMouseButtonUp(0,0); const bool worldWheel=m_rml_context->ProcessMouseWheel({0,-1},0);
            qInfo() << "HUD ownership booleans" << uiDown << uiUp << uiWheel << uiScrollWheel << worldDown << worldWheel;
            if(!uiDown||!uiUp||!uiScrollWheel||!worldDown||!worldWheel)hudProofFailure("HUD UI/world pointer-wheel ownership failed");
            qInfo() << "HUD pointer/wheel ownership passed: UI consumed; world fallback unconsumed";
        }
        else if( m_frame==35 )
        {
            auto* panel = m_hud_binding->document()->GetElementById("hud_build_panel");
            auto* handle = m_hud_binding->document()->GetElementById("hud_build_drag_handle");
            if(!panel||!handle)hudProofFailure("HUD build panel drag targets missing");
            const auto before=panel->GetAbsoluteOffset();
            const auto start=handle->GetAbsoluteOffset()+handle->GetBox().GetSize()*0.5f;
            m_rml_context->ProcessMouseMove(static_cast<int>(start.x),static_cast<int>(start.y),0);
            m_rml_context->ProcessMouseButtonDown(0,0);
            m_rml_context->ProcessMouseMove(static_cast<int>(start.x+48),static_cast<int>(start.y+24),0);
            m_rml_context->ProcessMouseButtonUp(0,0);
            m_rml_context->Update();
            const auto after=panel->GetAbsoluteOffset();
            if(std::abs(after.x-before.x)<12.0f||std::abs(after.y-before.y)<12.0f)hudProofFailure("HUD build panel drag failed");
            auto* items = m_hud_binding->document()->GetElementById("hud_build_items");
            if(!items || items->GetNumChildren() < 3)hudProofFailure("HUD build grid rows missing");
            const auto first = items->GetChild(0)->GetAbsoluteOffset();
            const auto second = items->GetChild(1)->GetAbsoluteOffset();
            const auto third = items->GetChild(2)->GetAbsoluteOffset();
            if(std::abs(first.y-second.y)>2.0f || std::abs(first.y-third.y)>2.0f || !(first.x < second.x && second.x < third.x))hudProofFailure("HUD build grid is not three columns");
            qInfo()<<"HUD build panel drag/grid passed"<<before<<after;
        }
        else if( m_frame==36 )
        {
            if(!m_hud_binding->activateElement("hud_build_close")||!m_hud_controller->state().buildCatalog.empty())hudProofFailure("HUD build panel close failed");
            qInfo()<<"HUD build panel close passed";
        }
        else if( m_frame==55 ) qInfo() << "HUD vertical proof passed: state, pause/speed/z/overlay/tool/cancel/rotate, event FIFO, pointer/wheel ownership";
    }

    void initializeInspectorProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::inspector;
        m_inspector_binding=std::make_unique<InspectorRmlBinding>(*m_rml_context);
        m_inspector_controller=std::make_unique<InspectorController>(m_inspector_port,*m_inspector_binding);
        if(!m_inspector_binding->initialize(*m_inspector_controller))hudProofFailure("Inspector binding initialization failed");
        m_inspector_controller->beginWorld(WorldEpoch{51});
        TileInspectorState tile;tile.id=TileId{1004550};tile.position={70,42,12};tile.wall="Basalt wall";tile.floor="Rough stone floor";tile.embedded="Embedded: hematite";tile.construction="Bronze-reinforced arch";tile.canMine=true;tile.canManage=true;tile.designation=DesignationId{1001813};tile.designationName="Ember Forge";tile.creatures.push_back({CreatureId{77},"Gnome: Runa, Blacksmith",EntityKind::Creature});tile.items.push_back({"Iron ingot","iron",12});tile.hasJob=true;tile.jobName="Forge axe";tile.jobPriority="6";tile.requiredSkill="Blacksmithing";tile.requiredTool="Hammer";tile.requiredToolAvailable="Yes";m_inspector_controller->showTile(tile);
    }

    void advanceInspectorProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::inspector;
        if(!m_inspector_self_test||!m_inspector_controller||!m_inspector_binding)return;
        const auto expect=[this](const char*id,std::string_view action){const auto before=m_inspector_port.actions.size();if(!m_inspector_binding->activateElement(id)||m_inspector_port.actions.size()!=before+1||m_inspector_port.actions.back().id.value!=action)hudProofFailure("Inspector callback proof failed");};
        if(m_frame==4){CreatureInspectorState c;c.id=CreatureId{77};c.name="Runa";c.profession="Blacksmith";c.activity="Forging an axe";c.strength=12;c.dexterity=9;c.constitution=11;c.intelligence=8;c.wisdom=7;c.charisma=6;c.hunger=18;c.thirst=24;c.sleep=10;c.happiness=82;m_inspector_controller->showCreature(c,WorldPosition{70,42,12});if(m_inspector_controller->state().kind!=InspectorKind::Creature)hudProofFailure("Inspector creature route failed");}
        else if(m_frame==7){expect("inspector_back","inspect.select");}
        else if(m_frame==10){WorkshopInspectorState w;w.id=WorkshopId{15};w.name="Ember Forge";w.priority=6;w.maxPriority=9;w.acceptGenerated=true;w.linkedStockpile=true;w.productCount=8;w.queuedJobs=3;m_inspector_controller->showWorkshop(w);expect("workshop_toggle_suspended","workshop.set_basics");}
        else if(m_frame==13){StockpileInspectorState s;s.id=StockpileId{16};s.name="Ore Reserve";s.priority=5;s.maxPriority=9;s.capacity=120;s.itemCount=73;s.reserved=8;s.pullFromOthers=true;s.contents.push_back({"Iron ore","hematite",31});m_inspector_controller->showStockpile(s);expect("stockpile_toggle_suspended","stockpile.set_basics");}
        else if(m_frame==16){AgricultureInspectorState a;a.target={AgricultureKind::Farm,DesignationId{17}};a.name="Mushroom Terrace";a.product="Plump helmet";a.priority=4;a.maxPriority=9;a.plots=24;a.planted=18;a.ready=7;a.harvest=true;m_inspector_controller->showAgriculture(a);expect("agriculture_toggle_suspended","agriculture.set_basics");expect("agriculture_toggle_primary","agriculture.set_harvest_options");}
        else if(m_frame==19){m_inspector_controller->setSelectionAction("BuildWorkshop",true);expect("selection_rotate","tool.rotate");expect("selection_cancel","tool.cancel");}
        else if(m_frame==22){expect("inspector_locate","view.center_on");expect("inspector_close","inspect.clear");}
        else if(m_frame==26){initializeInspectorTileForPointerProof();}
        else if(m_frame==30){auto*button=m_inspector_binding->document()->GetElementById("inspector_close");if(!button)hudProofFailure("Inspector input target missing");const auto p=button->GetAbsoluteOffset()+button->GetBox().GetSize()*0.5f;m_rml_context->ProcessMouseMove((int)p.x,(int)p.y,0);const bool uiDown=!m_rml_context->ProcessMouseButtonDown(0,0);const bool uiUp=!m_rml_context->ProcessMouseButtonUp(0,0);m_rml_context->ProcessMouseMove(width()/2,height()/2,0);const bool mapDown=m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseButtonUp(0,0);qInfo()<<"Inspector ownership booleans"<<uiDown<<uiUp<<mapDown;if(!uiDown||!uiUp||!mapDown)hudProofFailure("Inspector UI/map click ownership failed");qInfo()<<"Inspector click ownership passed: dock consumed; map gap passed through";initializeInspectorTileForPointerProof();}
        else if(m_frame==36){m_inspector_controller->endWorld();if(m_inspector_controller->state().kind!=InspectorKind::None||m_inspector_controller->state().selected)hudProofFailure("Inspector unload clear failed");initializeInspectorTileForPointerProof();}
        else if(m_frame==55)qInfo()<<"Inspector vertical proof passed: target routes, applicable sections, workshop/stockpile/agriculture/selection mutations, locate/close, UI/map ownership, unload clear";
    }

    void initializeInspectorTileForPointerProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::inspector;
        if(!m_inspector_controller->state().world)m_inspector_controller->beginWorld(WorldEpoch{52});TileInspectorState t;t.id=TileId{1004550};t.position={70,42,12};t.wall="Basalt wall";t.floor="Rough stone floor";t.construction="Bronze-reinforced arch";t.canMine=true;t.designation=DesignationId{1001813};t.designationName="Ember Forge";t.items.push_back({"Iron ingot","iron",12});m_inspector_controller->showTile(t);
    }

    void initializeManagement6BProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::management6b;
        m_management6b_binding = std::make_unique<Management6BRmlBinding>( *m_rml_context );
        m_management6b_controller = std::make_unique<Management6BController>( m_management6b_port, *m_management6b_binding );
        if ( !m_management6b_binding->initialize( *m_management6b_controller ) ) hudProofFailure( "Management 6B binding initialization failed" );
        if ( m_management6b_controller->state().open ) hudProofFailure( "Management 6B must initialize hidden" );
        m_management6b_controller->beginWorld( WorldEpoch{61} );
        PopulationRow ada{CreatureId{7}, "Ada", ProfessionId{"Miner"}, {
            {CatalogId{"Mining"}, "Mining", "Industry", 4, .5f, true},
            {CatalogId{"Masonry"}, "Masonry", "Industry", 5, .7f, true},
            {CatalogId{"Stonecarving"}, "Stonecarving", "Industry", 5, .6f, true},
            {CatalogId{"Woodcutting"}, "Woodcutting", "Industry", 3, .4f, true},
            {CatalogId{"Carpentry"}, "Carpentry", "Industry", 4, .5f, true},
            {CatalogId{"Woodcarving"}, "Woodcarving", "Industry", 5, .6f, true},
            {CatalogId{"Smelting"}, "Smelting", "Industry", 4, .5f, true},
            {CatalogId{"Blacksmithing"}, "Blacksmithing", "Industry", 5, .7f, true},
            {CatalogId{"Metalworking"}, "Metalworking", "Industry", 3, .3f, true},
            {CatalogId{"WeaponCrafting"}, "Weapon Crafting", "Industry", 5, .7f, true},
            {CatalogId{"ArmorCrafting"}, "Armor Crafting", "Industry", 4, .5f, true},
            {CatalogId{"Gemcutting"}, "Gemcutting", "Industry", 2, .2f, false},
            {CatalogId{"JewelryMaking"}, "Jewelry Making", "Industry", 1, .1f, false},
            {CatalogId{"GlassMaking"}, "Glass Making", "Industry", 1, .1f, false},
            {CatalogId{"Weaving"}, "Weaving", "Industry", 5, .6f, true},
            {CatalogId{"Tailoring"}, "Tailoring", "Industry", 5, .6f, true}
        }};
        PopulationRow bera{CreatureId{9}, "Bera", ProfessionId{"Smith"}, {{CatalogId{"Smithing"}, "Smithing", "Industry", 6, .7f, true}}};
        m_management6b_controller->applyPopulation( {WorldEpoch{61}, Revision{1}, {ada, bera}} );
        CreatureDetail detail; detail.id=CreatureId{7}; detail.name="Ada"; detail.profession="Miner"; detail.activity="Mining in the lower cavern"; detail.strength=12; detail.dexterity=9; detail.constitution=11; detail.intelligence=8; detail.wisdom=7; detail.charisma=6; detail.hunger=18; detail.thirst=24; detail.sleep=10; detail.happiness=82;
        m_management6b_controller->applyCreature( {WorldEpoch{61}, Revision{1}, detail} );
        m_management6b_controller->applyProfessions( {WorldEpoch{61}, Revision{1}, {{ProfessionId{"Miner"}, "Miner", {CatalogId{"Mining"}}}, {ProfessionId{"Smith"}, "Smith", {CatalogId{"Smithing"}}}}} );
        ScheduleRow a; a.creature=CreatureId{7}; a.name="Ada"; ScheduleRow b; b.creature=CreatureId{9}; b.name="Bera";
        m_management6b_controller->applySchedules( {WorldEpoch{61}, Revision{1}, {a,b}} );
        InventoryRow iron{{CatalogId{"materials"},CatalogId{"metal"},CatalogId{"ingot"},CatalogId{"iron"},InventoryDepth::Material},"Iron ingot",42,2,30,4,0,6,420,"build_WoodChairFR.tga",0,0,32,36,32,36,false};
        InventoryRow copper{{CatalogId{"materials"},CatalogId{"metal"},CatalogId{"ingot"},CatalogId{"copper"},InventoryDepth::Material},"Copper ingot",21,1,15,1,0,4,168,"build_WoodChairFR.tga",0,0,32,36,32,36,false};
        InventoryRow grown{{CatalogId{"food"},CatalogId{"raw"},CatalogId{"honey"},CatalogId{"honey"},InventoryDepth::Item},"Bee honey",18,0,12,0,0,6,72,"build_Honey.tga",0,0,32,36,32,36,false};
        InventoryRow tree{{CatalogId{"grown"},CatalogId{"plants"},CatalogId{"leaves"},CatalogId{"tree"},InventoryDepth::Material},"Tree",1,0,1,0,0,0,1,"build_PineTree.tga",0,0,32,36,32,36,false};
        // Keep the native proof representative of the production path: inventory rows
        // carry the same generated DB-backed crop that the game-side adapter supplies.
        iron.spriteSheet = "build_WoodChairFR.tga";
        iron.spriteWidth = 32;
        iron.spriteHeight = 36;
        copper.spriteSheet = "build_WoodChairFR.tga";
        copper.spriteWidth = 32;
        copper.spriteHeight = 36;
          auto sectionRow = []( const char* category, const char* group, const char* item, InventoryDepth depth, const char* name )
          {
              InventoryRow row;
              row.id = { CatalogId { category }, CatalogId { group }, CatalogId { item }, CatalogId {}, depth };
              row.name = name;
              return row;
          };
          std::vector<InventoryRow> inventoryRows{
              sectionRow( "materials", "", "", InventoryDepth::Category, "Materials" ),
              sectionRow( "materials", "metal", "", InventoryDepth::Group, "Metal" ),
              sectionRow( "materials", "metal", "ingot", InventoryDepth::Item, "Ingots" ),
              sectionRow( "food", "", "", InventoryDepth::Category, "Food" ),
              sectionRow( "food", "raw", "", InventoryDepth::Group, "Raw food" ),
              sectionRow( "drinks", "", "", InventoryDepth::Category, "Drinks" ),
              sectionRow( "drinks", "beer", "", InventoryDepth::Group, "Beer" ),
              sectionRow( "drinks", "beer", "beer", InventoryDepth::Item, "Beer" ),
              sectionRow( "drinks", "beer", "barley_beer", InventoryDepth::Item, "Barley beer" ),
              sectionRow( "drinks", "beer", "millet_beer", InventoryDepth::Item, "Millet beer" ),
              sectionRow( "drinks", "beer", "oat_beer", InventoryDepth::Item, "Oat beer" ),
              sectionRow( "drinks", "beer", "wheat_beer", InventoryDepth::Item, "Wheat beer" ),
              sectionRow( "grown", "", "", InventoryDepth::Category, "Grown" ),
              sectionRow( "grown", "plants", "", InventoryDepth::Group, "Plants" ),
              sectionRow( "grown", "plants", "leaves", InventoryDepth::Item, "Leaves" ),
              iron,copper,grown,tree};
         for ( const auto& pie : { std::pair<const char*, const char*>( "FineMeatPie", "Fine meat pie" ), { "FruitPie", "Fruit pie" }, { "MeatPie", "Meat pie" }, { "ShepherdsPie", "Shepherd's pie" }, { "VeggiePie", "Veggie pie" } } )
         {
             auto row = grown;
             row.id.item = CatalogId{ pie.first };
             row.name = pie.second;
             row.spriteSheet = std::string( "build_" ) + pie.first + ".tga";
             inventoryRows.push_back( std::move( row ) );
         }
         for ( unsigned n = 0; n < 100; ++n )
         {
              auto extra = copper;
              extra.id.item = CatalogId{"resource_" + std::to_string( n )};
              extra.id.material = CatalogId{"material_" + std::to_string( n )};
              extra.id.depth = InventoryDepth::Item;
              extra.id.material = {};
             extra.name = "Resource " + std::to_string( n );
             extra.total = n;
             extra.totalValue = n * 2;
             inventoryRows.push_back( std::move( extra ) );
         }
         m_management6b_controller->applyInventory( {WorldEpoch{61}, Revision{1}, std::move( inventoryRows )} );
        m_management6b_controller->openPopulation();
    }

    void advanceManagement6BProof()
    {
        using namespace ingnomia::ui;
        using namespace ingnomia::ui::management6b;
        if(!m_management6b_self_test||!m_management6b_controller||!m_management6b_binding)return;
        if(m_frame==4){m_management6b_controller->selectCreature(CreatureId{7});m_management6b_controller->setSkill(CreatureId{7},CatalogId{"Mining"},false);m_management6b_population_queued=!m_management6b_port.actions.empty()&&m_management6b_port.actions.back().id.value=="population.set_skill";}
        else if(m_frame==8){m_management6b_controller->open(View::Schedules);m_management6b_controller->selectScheduleCell({CreatureId{7},12});auto*cell=m_management6b_binding->populationDocument()->GetElementById("schedule_7_12");auto*rows=m_management6b_binding->populationDocument()->GetElementById("schedule_rows");if(!cell||!rows)hudProofFailure("Management 6B schedule target missing");cell->Focus();Rml::Dictionary p;p["key_identifier"]=static_cast<int>(Rml::Input::KI_RIGHT);rows->DispatchEvent("keydown",p);m_management6b_schedule_keyboard=m_management6b_controller->state().selectedScheduleCell&&m_management6b_controller->state().selectedScheduleCell->hour==13;}
        else if(m_frame==12){m_management6b_controller->openInventory();m_management6b_controller->selectInventory(m_management6b_controller->state().inventory.front().id);const bool activated=m_management6b_binding->activateElement("inventory_toggle_watch");m_management6b_inventory_queued=activated&&!m_management6b_port.actions.empty()&&m_management6b_port.actions.back().id.value=="watch.set";m_rml_context->Update();auto*header=m_management6b_binding->inventoryDocument()->GetElementById("inventory_column_head");auto*rows=m_management6b_binding->inventoryDocument()->GetElementById("inventory_rows");if(!header||!rows||header->GetNumChildren()<4||rows->GetNumChildren()<1||rows->GetChild(0)->GetNumChildren()<4)hudProofFailure("Management 6B inventory columns missing");const auto headerTotal=header->GetChild(3);const auto rowTotal=rows->GetChild(0)->GetChild(3);if(headerTotal->GetBox().GetSize().x<=0.0f||rowTotal->GetBox().GetSize().x<=0.0f)hudProofFailure("Management 6B inventory numeric columns missing size");m_management6b_controller->setInventoryCategory("drinks");qInfo()<<"Management 6B inventory columns aligned";}
        else if(m_frame==14){m_management6b_controller->setInventoryCategory("grown");m_rml_context->Update();}
        else if(m_frame==16){const auto selected=m_management6b_controller->state().selectedInventory;m_management6b_controller->setInventoryCategory("");m_management6b_controller->setInventoryFilter("iron");m_management6b_selection_stable=selected==m_management6b_controller->state().selectedInventory;m_management6b_stale_epoch=!m_management6b_controller->applyInventory({WorldEpoch{60},Revision{99},{}});}
         else if(m_frame==19)m_management6b_controller->openPopulation();
         else if(m_frame==20){auto*doc=m_management6b_binding->populationDocument();auto*handle=doc?doc->GetElementById("population_drag_handle"):nullptr;auto*heading=doc?doc->GetElementById("population_heading"):nullptr;if(!handle||!heading)hudProofFailure("Management 6B drag handle missing");const auto before=heading->GetAbsoluteOffset();const auto point=handle->GetAbsoluteOffset()+handle->GetBox().GetSize()*0.5f;m_rml_context->ProcessMouseMove(static_cast<int>(point.x),static_cast<int>(point.y),0);m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseMove(static_cast<int>(point.x-64.0f),static_cast<int>(point.y-28.0f),0);m_rml_context->ProcessMouseButtonUp(0,0);m_rml_context->Update();const auto afterMouse=heading->GetAbsoluteOffset();Rml::Dictionary dragStart;dragStart["mouse_x"]=point.x;dragStart["mouse_y"]=point.y;Rml::Dictionary dragMove=dragStart;dragMove["mouse_x"]=point.x-64.0f;dragMove["mouse_y"]=point.y-28.0f;handle->DispatchEvent(Rml::EventId::Dragstart,dragStart);handle->DispatchEvent(Rml::EventId::Drag,dragMove);handle->DispatchEvent(Rml::EventId::Dragend,dragMove);m_rml_context->Update();const auto after=heading->GetAbsoluteOffset();m_management6b_dragged=std::abs(afterMouse.x-before.x)>12.0f||std::abs(afterMouse.y-before.y)>12.0f||std::abs(after.x-before.x)>12.0f||std::abs(after.y-before.y)>12.0f;}
        else if(m_frame==22){auto*input=m_management6b_binding->populationDocument()->GetElementById("population_search");if(!input)hudProofFailure("Management 6B input target missing");const auto point=input->GetAbsoluteOffset()+input->GetBox().GetSize()*0.5f;m_rml_context->ProcessMouseMove(static_cast<int>(point.x),static_cast<int>(point.y),0);const bool down=!m_rml_context->ProcessMouseButtonDown(0,0);const bool up=!m_rml_context->ProcessMouseButtonUp(0,0);const bool closed=m_management6b_binding->activateElement("population_close")&&!m_management6b_controller->state().populationOpen&&m_management6b_controller->state().inventoryOpen;m_rml_context->Update();m_rml_context->ProcessMouseMove(100,height()/2,0);const bool map=m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseButtonUp(0,0);m_management6b_input_ownership=down&&up&&closed&&map;m_management6b_controller->openPopulation();}
        else if(m_frame==38){m_management6b_controller->closeInventory();m_management6b_controller->open(View::Citizens);m_management6b_controller->selectCreature(CreatureId{7});m_management6b_binding->inventoryDocument()->Hide();m_management6b_binding->populationDocument()->Show();m_rml_context->Update();}
         else if(m_frame==50){m_management6b_controller->openInventory();m_management6b_controller->setInventoryFilter("");m_management6b_controller->setInventoryCategory("");m_management6b_controller->setInventorySort(Sort::Total);m_management6b_controller->selectInventory(m_management6b_controller->state().inventory.front().id);m_management6b_multi_window=m_management6b_binding->populationDocument()->IsVisible()&&m_management6b_binding->inventoryDocument()->IsVisible();}
         else if(m_frame==55){if(!m_management6b_population_queued)hudProofFailure("Management 6B population mutation proof failed");if(!m_management6b_inventory_queued)hudProofFailure("Management 6B inventory watch proof failed");if(!m_management6b_schedule_keyboard)hudProofFailure("Management 6B schedule keyboard proof failed");if(!m_management6b_stale_epoch)hudProofFailure("Management 6B stale epoch proof failed");if(!m_management6b_selection_stable)hudProofFailure("Management 6B stable selection proof failed");if(!m_management6b_input_ownership)hudProofFailure("Management 6B input ownership proof failed");if(!m_management6b_dragged)hudProofFailure("Management 6B title-bar drag proof failed");if(!m_management6b_multi_window)hudProofFailure("Management 6B multiple-window proof failed");if(!m_management6b_binding->inventoryDocument()->IsVisible())hudProofFailure("Management 6B final visual is hidden");qInfo()<<"MANAGEMENT_6B_SELF_TEST_PASS";}
    }

    void initializeManagement6AProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::management6a;
        m_management6a_binding = std::make_unique<Management6ARmlBinding>( *m_rml_context );
        m_management6a_controller = std::make_unique<Management6AController>( m_management6a_port, *m_management6a_binding );
        if(!m_management6a_binding->initialize(*m_management6a_controller))hudProofFailure("Management 6A binding initialization failed");
        if(m_management6a_controller->state().view!=ManagementView::None)hudProofFailure("Management 6A must initialize hidden");
        m_management6a_controller->beginWorld(WorldEpoch{62});
        WorkshopSnapshot ws;ws.id=WorkshopId{421};ws.name="Deepvein Forge";ws.subtype="TradingPost";ws.priority=2;ws.maxPriority=5;
        WorkshopProductRow product;product.id=CatalogId{"iron_pick"};WorkshopComponentRow component;component.item=CatalogId{"pick_head"};component.amount=1;component.materials={{CatalogId{"iron"},14}};product.components={component};ws.products={product};
        CraftQueueRow job;job.id=CraftJobId{7001};job.craft=CatalogId{"iron_pick"};job.item=CatalogId{"pick"};job.count=2;job.materials={CatalogId{"iron"}};ws.queue={job};
        m_management6a_controller->showWorkshop(ws,Revision{1},WorldPosition{48,32,-7});TradeRow trader;trader.id={TradeParty::Trader,CatalogId{"cloth"},CatalogId{"wool"},1};trader.name="Fine wool cloth";trader.stock=8;trader.unitValue=14;m_management6a_controller->setTradeRows(TradeParty::Trader,{trader});m_management6a_controller->setTradeValues(14,20);
    }

    void advanceManagement6AProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::management6a;
        if(!m_management6a_self_test||!m_management6a_controller||!m_management6a_binding)return;
        if(m_frame==4){auto*row=m_management6a_binding->workshopDocument()->GetElementById("m6a_product_69726f6e5f7069636b");if(!row)hudProofFailure("Management 6A stable product row missing");row->Focus();m_rml_context->ProcessKeyDown(Rml::Input::KI_RETURN,0);m_rml_context->ProcessKeyUp(Rml::Input::KI_RETURN,0);m_management6a_stable_keyboard=m_management6a_controller->state().workshop.selectedProduct==CatalogId{"iron_pick"};m_management6a_controller->queueSelectedCraftDefault();m_management6a_production_queued=!m_management6a_port.actions.empty()&&m_management6a_port.actions.back().id.value=="workshop.queue_craft";}
        else if(m_frame==8){auto*search=m_management6a_binding->workshopDocument()->GetElementById("workshop_search");search->Focus();static_cast<Rml::ElementFormControl*>(search)->SetValue("iron");search->DispatchEvent("input",Rml::Dictionary{});m_management6a_search_focus=m_rml_context->GetFocusElement()==search&&m_management6a_controller->state().workshop.search=="iron";}
        else if(m_frame==12){m_management6a_controller->executeTrade();m_rml_context->Update();auto*blocker=m_management6a_binding->workshopDocument()->GetElementById("workshop_trade_confirmation");auto*confirm=m_management6a_binding->workshopDocument()->GetElementById("workshop_trade_confirm");const auto before=m_management6a_port.actions.size();m_management6a_controller->setWorkshopBasics("blocked",1,false,false,false);const bool controllerGate=m_management6a_controller->state().workshop.tradeConfirmationRequired&&m_management6a_port.actions.size()==before;const auto point=confirm->GetAbsoluteOffset()+confirm->GetBox().GetSize()*0.5f;m_rml_context->ProcessMouseMove(static_cast<int>(point.x),static_cast<int>(point.y),0);const bool down=!m_rml_context->ProcessMouseButtonDown(0,0);const bool up=!m_rml_context->ProcessMouseButtonUp(0,0);m_management6a_confirmation_blocked=blocker&&blocker->IsVisible()&&controllerGate&&down&&up;const bool activated=!m_management6a_controller->state().workshop.tradeConfirmationRequired||m_management6a_binding->activateElement("workshop_trade_confirm");m_management6a_confirmation_origin=activated&&!m_management6a_port.origins.empty()&&m_management6a_port.origins.back()==DispatchOrigin::DestructiveConfirmation;}
        else if(m_frame==16){StockpileSnapshot sp;sp.id=StockpileId{20};sp.name="Deep stores";sp.priority=2;sp.maxPriority=5;for(unsigned n=0;n<120;++n){StockpileContentRow row;row.id={CatalogId{"item_"+std::to_string(n)},CatalogId{"stone"}};row.itemName="Item "+std::to_string(n);row.materialName="Stone";row.count=n;sp.contents.push_back(row);}StockpileFilterRowId filter{sp.id,CatalogId{"raw"},CatalogId{"stone"},CatalogId{"block"},CatalogId{"granite"},FilterDepth::Material};sp.filters.push_back({filter,"Granite",TriState::On});m_management6a_controller->showStockpile(sp,Revision{1},WorldPosition{4,5,6});m_management6a_controller->selectStockpileFilter(filter);m_management6a_controller->toggleSelectedStockpileFilter();m_management6a_stockpile_queued=!m_management6a_port.actions.empty()&&m_management6a_port.actions.back().id.value=="stockpile.set_filter";const bool paged=m_management6a_binding->activateElement("stockpile_page_next");m_management6a_large_paged=paged&&m_management6a_binding->stockpileDocument()->GetElementById("m6a_content_6974656d5f3438_73746f6e65")!=nullptr;}
        else if(m_frame==20){AgricultureSnapshot ag;ag.target={AgricultureKind::Pasture,DesignationId{30}};ag.name="Yak pasture";ag.priority=1;ag.maxPriority=5;PastureAnimalRow animal;animal.id=CreatureId{99};animal.name="Moss";animal.species=CatalogId{"yak"};ag.animals={animal};m_management6a_controller->showAgriculture(ag,Revision{1},WorldPosition{7,8,9});m_management6a_controller->selectAgricultureAnimal(CreatureId{99});m_management6a_controller->toggleSelectedAnimalButchering();m_management6a_agriculture_queued=!m_management6a_port.actions.empty()&&m_management6a_port.actions.back().id.value=="agriculture.set_butchering";StableRowPatch<CreatureId,PastureAnimalRow> patch{PatchKind::Update,CreatureId{99},animal,{}};m_management6a_stale_revision=!m_management6a_controller->patchAgricultureAnimals(Revision{0},Revision{3},{patch});}
        else if(m_frame==24){m_management6a_controller->close();m_rml_context->Update();m_rml_context->ProcessMouseMove(width()/2,height()/2,0);m_management6a_input_ownership=m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseButtonUp(0,0);initializeManagement6AProofVisible();}
        else if(m_frame==28){auto*scroll=m_management6a_binding->workshopDocument()->GetElementById("workshop_scroll");scroll->SetScrollTop(260.0f);}
        else if(m_frame==55){if(!m_management6a_production_queued)hudProofFailure("Management 6A production mutation failed");if(!m_management6a_stable_keyboard)hudProofFailure("Management 6A stable keyboard failed");if(!m_management6a_search_focus)hudProofFailure("Management 6A search focus failed");if(!m_management6a_confirmation_blocked)hudProofFailure("Management 6A confirmation block failed");if(!m_management6a_confirmation_origin)hudProofFailure("Management 6A confirmation origin failed");if(!m_management6a_large_paged)hudProofFailure("Management 6A paging failed");if(!m_management6a_stockpile_queued)hudProofFailure("Management 6A stockpile mutation failed");if(!m_management6a_agriculture_queued)hudProofFailure("Management 6A agriculture mutation failed");if(!m_management6a_stale_revision)hudProofFailure("Management 6A stale revision failed");if(!m_management6a_input_ownership)hudProofFailure("Management 6A input ownership failed");qInfo()<<"MANAGEMENT_6A_SELF_TEST_PASS";}
    }

    void initializeManagement6AProofVisible(){using namespace ingnomia::ui;using namespace ingnomia::ui::management6a;WorkshopSnapshot ws;ws.id=WorkshopId{422};ws.name="Deepvein Production Hall";ws.subtype="Production";ws.priority=3;ws.maxPriority=5;WorkshopProductRow p;p.id=CatalogId{"bronze_gear"};ws.products={p};CraftQueueRow job;job.id=CraftJobId{7101};job.craft=CatalogId{"bronze_gear"};job.item=CatalogId{"gear"};job.mode=CraftRepeatMode::Maintain;job.count=12;job.materials={CatalogId{"bronze"}};ws.queue={job};m_management6a_controller->showWorkshop(ws,Revision{2},WorldPosition{49,32,-7});auto*search=static_cast<Rml::ElementFormControl*>(m_management6a_binding->workshopDocument()->GetElementById("workshop_search"));search->Blur();search->SetValue("");m_management6a_controller->setSearch("");}

    void initializeDeveloperUiProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::debug;
        m_developer_ui_binding=std::make_unique<DebugRmlBinding>(*m_rml_context);
        m_developer_ui_controller=std::make_unique<DebugController>(true,m_developer_ui_port,*m_developer_ui_binding);
        if(!m_developer_ui_binding->initialize(*m_developer_ui_controller))hudProofFailure("Developer UI binding initialization failed");
        if(m_developer_ui_controller->state().open||m_developer_ui_binding->document()->IsVisible())hudProofFailure("Developer UI must initialize hidden");
        m_developer_ui_controller->beginWorld(WorldEpoch{65});m_developer_ui_controller->open();m_developer_ui_controller->setPage(Page::Gnomes);
        m_developer_ui_controller->applyGnomes(WorldEpoch{65},Revision{1},{{"Ada Coppervein",7},{"Bera Ironhand",9},{"Mira Emberpick",11}});
        m_developer_ui_controller->applyCatalog(WorldEpoch{65},Revision{1},{{"Weapons","Tools"},{"axe","pick"},{"iron"},{},0});
        m_developer_ui_controller->instrumentDirty(3,true,true);
    }

    void advanceDeveloperUiProof()
    {
        using namespace ingnomia::ui;using namespace ingnomia::ui::debug;
        if(!m_developer_ui_self_test||!m_developer_ui_controller||!m_developer_ui_binding)return;
        if(m_frame==4){m_developer_ui_controller->selectGnome(7);m_developer_ui_controller->moveGnomeSelection(1);m_developer_ui_keyboard=m_developer_ui_controller->state().selectedGnome==std::uint32_t{9};m_developer_ui_controller->setSearch("Bera");m_developer_ui_search=m_developer_ui_controller->visibleGnomes().size()==1&&m_developer_ui_controller->state().selectedGnome==std::uint32_t{9};}
        else if(m_frame==8){m_developer_ui_stale=!m_developer_ui_controller->applyGnomes(WorldEpoch{64},Revision{9},{});m_developer_ui_counters=m_developer_ui_controller->state().counters.dirtyBindings==3&&m_developer_ui_controller->state().counters.fullSnapshots>=2&&m_developer_ui_controller->state().counters.rowPatches==1&&m_developer_ui_controller->state().counters.staleEpochRejects==1;}
        else if(m_frame==12){m_developer_ui_controller->setSearch("");m_developer_ui_controller->spawnCreature("Gnome");m_developer_ui_action=!m_developer_ui_port.actions.empty()&&m_developer_ui_port.actions.back().kind==ActionKind::SpawnCreature;}
        else if(m_frame==18){auto*search=m_developer_ui_binding->document()->GetElementById("debug_search");const auto point=search->GetAbsoluteOffset()+search->GetBox().GetSize()*0.5f;m_rml_context->ProcessMouseMove((int)point.x,(int)point.y,0);const bool down=!m_rml_context->ProcessMouseButtonDown(0,0);const bool up=!m_rml_context->ProcessMouseButtonUp(0,0);m_developer_ui_controller->close();m_rml_context->Update();m_rml_context->ProcessMouseMove(width()/2,height()/2,0);const bool map=m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseButtonUp(0,0);m_developer_ui_input=down&&up&&map;m_developer_ui_controller->open();m_developer_ui_controller->setPage(Page::Diagnostics);}
        else if(m_frame==55){if(!m_developer_ui_keyboard||!m_developer_ui_search||!m_developer_ui_stale||!m_developer_ui_counters||!m_developer_ui_action||!m_developer_ui_input)hudProofFailure("Developer UI native proof failed");qInfo()<<"DEVELOPER_UI_SELF_TEST_PASS";}
    }

    void initializeManagement6CProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::management6c;
        m_management6c_binding = std::make_unique<Management6CRmlBinding>( *m_rml_context );
        m_management6c_controller = std::make_unique<Management6CController>( m_management6c_port, *m_management6c_binding );
        if( !m_management6c_binding->initialize( *m_management6c_controller ) ) hudProofFailure( "Management 6C binding initialization failed" );
        if( m_management6c_controller->state().open || m_management6c_binding->militaryDocument()->IsVisible()
            || m_management6c_binding->diplomacyDocument()->IsVisible() ) hudProofFailure( "Management 6C must initialize hidden" );
        m_management6c_binding->setRouteCloseHandler( [this]( RouteId route, FocusToken focus ) {
            m_management6c_route_closed = ( route.value == "workbench.military" || route.value == "workbench.diplomacy" )
                && focus == FocusToken{7001};
        } );
        m_management6c_controller->beginWorld( WorldEpoch{63} );
        initializeManagement6CProofVisible();
    }

    void initializeManagement6CProofVisible()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::management6c;
        const auto world = m_management6c_controller->state().world;
        MilitaryRoster roster;
        SquadRow guard; guard.id=SquadId{10}; guard.name="Basalt Gate Guard"; guard.canMoveDown=true;
        guard.members={{CreatureId{501},"Mira Emberpick",MilitaryRoleId{71}},{CreatureId{502},"Nia Deepdelver",MilitaryRoleId{71}}};
        guard.priorities={{CatalogId{"Goblin"},"Goblin",MilitaryAttitude::Hunt},{CatalogId{"Animal"},"Wild animal",MilitaryAttitude::Defend}};
        roster.squads={guard}; roster.unassigned={{CreatureId{503},"Ada Coppervein",std::nullopt}};
        m_management6c_controller->applyMilitary({world,Revision{1},roster});
        UniformSlotRow chest; chest.slot=UniformSlot::ChestArmor; chest.name="Chest armor"; chest.type=CatalogId{"plate"};
        chest.material=CatalogId{"iron"}; chest.possibleTypes={CatalogId{"plate"},CatalogId{"mail"}}; chest.possibleMaterials={CatalogId{"iron"},CatalogId{"bronze"}};
        m_management6c_controller->applyRoles({world,Revision{1},{{MilitaryRoleId{71},"Shieldbearer",false,{chest}}}});
        NeighborRow known; known.id=NeighborId{22}; known.discovered=true; known.name="Königshöhle"; known.distance="3 days";
        known.type="Mountain kingdom"; known.attitude="They are friendly."; known.wealth="Wealthy"; known.economy="Strong";
        known.military="About average"; known.canSpy=true; known.canSendEmissary=true; known.canRaid=true;
        NeighborRow hidden; hidden.id=NeighborId{11}; hidden.discovered=false;
        m_management6c_controller->applyNeighbors({world,Revision{1},{hidden,known}});
        m_management6c_controller->applyAvailableGnomes({world,Revision{1},{{CreatureId{501},"Mira Emberpick"},{CreatureId{503},"Ada Coppervein"}}});
        MissionRow mission; mission.id=MissionId{31}; mission.type=MissionType::Emissary; mission.action=MissionAction::InviteTrader;
        mission.step=MissionStep::Travel; mission.target=NeighborId{22}; mission.participants={CreatureId{501}}; mission.elapsedHours=18; mission.nextCheckTick=20240;
        m_management6c_controller->applyMissions({world,Revision{1},{mission}});
        if( !m_management6c_binding->openMilitary(View::Squads,FocusToken{7001}) ) hudProofFailure("Management 6C military route open failed");
    }

    void advanceManagement6CProof()
    {
        using namespace ingnomia::ui; using namespace ingnomia::ui::management6c;
        if(!m_management6c_self_test||!m_management6c_controller||!m_management6c_binding)return;
        if(m_frame==4){const auto before=m_management6c_port.actions.size();m_management6c_controller->selectSquad(SquadId{10});m_management6c_controller->setSelectedAttitude(MilitaryAttitude::Attack);m_management6c_military_queued=m_management6c_port.actions.size()==before+1&&m_management6c_port.actions.back().id.value=="military.set_attitude";}
        else if(m_frame==8){(void)m_management6c_binding->activateElement("military_tab_roles");m_management6c_controller->selectRole(MilitaryRoleId{71});auto*rows=m_management6c_binding->militaryDocument()->GetElementById("military_role_rows");auto*row=m_management6c_binding->militaryDocument()->GetElementById("military_role_71");if(!rows||!row)hudProofFailure("Management 6C keyboard role row missing");row->Focus();Rml::Dictionary p;p["key_identifier"]=static_cast<int>(Rml::Input::KI_DOWN);rows->DispatchEvent("keydown",p);m_management6c_keyboard=m_management6c_controller->state().selectedRole==MilitaryRoleId{71};}
        else if(m_frame==12){m_management6c_controller->requestRemoveSelectedRole();m_rml_context->Update();const auto modal=m_management6c_controller->state().destructive?m_management6c_controller->state().destructive->modal:ModalInstanceId{};auto*blocker=m_management6c_binding->militaryDocument()->GetElementById("military_confirm_layer");const bool visibleBefore=blocker&&blocker->IsVisible();const auto before=m_management6c_port.actions.size();m_management6c_controller->confirmDestructive(ModalInstanceId{modal.value+1});const bool wrong=m_management6c_port.actions.size()==before;(void)m_management6c_binding->activateElement("military_confirm_accept");m_management6c_confirmation=wrong&&visibleBefore&&!m_management6c_port.origins.empty()&&m_management6c_port.origins.back().kind()==DispatchOriginKind::DestructiveConfirmation&&m_management6c_port.origins.back().modal()==modal;}
        else if(m_frame==16){m_management6c_binding->openDiplomacy(View::Neighbors,FocusToken{7001});m_management6c_controller->selectNeighbor(NeighborId{22});m_management6c_controller->setMissionType(MissionType::Spy);const auto before=m_management6c_port.actions.size();m_management6c_controller->startMission();m_management6c_mission_queued=m_management6c_port.actions.size()==before+1&&m_management6c_port.actions.back().id.value=="diplomacy.start_mission";m_management6c_discovery_masked=!m_management6c_controller->state().neighbors.front().name;}
        else if(m_frame==20){auto large=m_management6c_controller->state().neighbors;for(unsigned n=100;n<220;++n){NeighborRow row;row.id=NeighborId{n};row.discovered=true;row.name="Neighbor "+std::to_string(n);large.push_back(row);}m_management6c_controller->applyNeighbors({WorldEpoch{63},Revision{2},large});m_management6c_large_paged=m_management6c_binding->activateElement("m6c_page_neighbors_next")&&m_management6c_binding->diplomacyDocument()->GetElementById("diplomacy_neighbor_162")!=nullptr;}
        else if(m_frame==24){const auto renders=m_management6c_binding->dynamicRenderCount();m_management6c_controller->setDiplomacyError("");m_management6c_no_unrelated_rebuild=m_management6c_binding->dynamicRenderCount()==renders;m_management6c_stale_epoch=!m_management6c_controller->applyNeighbors({WorldEpoch{62},Revision{99},{}});}
        else if(m_frame==28){m_management6c_binding->closeRoute();m_rml_context->Update();m_rml_context->ProcessMouseMove(width()/2,height()/2,0);const bool map=m_rml_context->ProcessMouseButtonDown(0,0);m_rml_context->ProcessMouseButtonUp(0,0);m_management6c_input_ownership=m_management6c_route_closed&&map;initializeManagement6CProofVisible();m_management6c_binding->openDiplomacy(View::Neighbors,FocusToken{7001});}
        else if(m_frame==40){m_management6c_controller->endWorld();m_management6c_controller->beginWorld(WorldEpoch{64});initializeManagement6CProofVisible();m_management6c_binding->openDiplomacy(View::Neighbors,FocusToken{7001});m_management6c_controller->selectNeighbor(NeighborId{22});}
        else if(m_frame==55){
            auto* toolbar=m_management6c_binding->diplomacyDocument()->GetElementById("diplomacy_primary_toolbar");
            auto* main=m_management6c_binding->diplomacyDocument()->GetElementById("diplomacy_main");
            auto* rows=m_management6c_binding->diplomacyDocument()->GetElementById("diplomacy_neighbor_rows");
            auto* detail=m_management6c_binding->diplomacyDocument()->GetElementById("diplomacy_neighbor_detail");
            auto* distance=m_management6c_binding->diplomacyDocument()->GetElementById("neighbor_distance");
            auto* military=m_management6c_binding->diplomacyDocument()->GetElementById("neighbor_military");
            m_management6c_layout_readable=toolbar&&main&&rows&&detail&&distance&&military&&toolbar->GetBox().GetSize().x>=700.0f&&toolbar->GetBox().GetSize().y<=60.0f&&main->GetBox().GetSize().x>=700.0f&&rows->GetBox().GetSize().x>=280.0f&&detail->GetBox().GetSize().x>=400.0f&&detail->GetAbsoluteOffset().x>rows->GetAbsoluteOffset().x+280.0f&&distance->GetInnerRML()=="3 days"&&military->GetInnerRML()=="About average";
            if(!m_management6c_military_queued||!m_management6c_mission_queued||!m_management6c_keyboard||!m_management6c_confirmation||!m_management6c_discovery_masked||!m_management6c_large_paged||!m_management6c_no_unrelated_rebuild||!m_management6c_stale_epoch||!m_management6c_input_ownership||!m_management6c_layout_readable)hudProofFailure("Management 6C native proof failed");qInfo()<<"MANAGEMENT_6C_SELF_TEST_PASS";
            if(auto* builder=m_management6c_binding->diplomacyDocument()->GetElementById("mission_builder"))builder->SetClass("is-hidden",true);
        }
        else if(m_frame==57){auto* document=m_management6c_binding->diplomacyDocument();if(auto* builder=document->GetElementById("mission_builder"))builder->SetClass("is-hidden",false);if(auto* body=document->GetElementById("diplomacy_body"))body->SetScrollTop(1000.0f);}
        else if(m_frame==59){auto* document=m_management6c_binding->diplomacyDocument();auto* gnomes=document->GetElementById("diplomacy_gnome_rows");auto* start=document->GetElementById("mission_start");m_management6c_lower_controls_visible=gnomes&&start&&gnomes->IsVisible()&&start->IsVisible()&&start->GetAbsoluteOffset().y>=130.0f&&start->GetAbsoluteOffset().y+start->GetBox().GetSize().y<=620.0f;if(!m_management6c_lower_controls_visible)hudProofFailure("Management 6C lower mission controls are not reachable");}
    }

    QSize physicalSize() const
    {
        return QSize( std::max( 1, qRound( width() * devicePixelRatio() ) ), std::max( 1, qRound( height() * devicePixelRatio() ) ) );
    }

    void resizeRmlUi()
    {
        if ( !m_gl_context || !m_rml_context || !m_renderer ) return;
        if ( !m_gl_context->makeCurrent( this ) ) return;
        const QSize pixels = physicalSize();
        m_rml_context->SetDimensions( {pixels.width(), pixels.height()} );
        m_rml_context->SetDensityIndependentPixelRatio( static_cast<float>( devicePixelRatio() ) * m_user_scale );
        m_renderer->setViewport( pixels.width(), pixels.height() );
        requestUpdate();
    }

    void paintWorld()
    {
        const QSize pixels = physicalSize();
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
        glViewport( 0, 0, pixels.width(), pixels.height() );
        glDisable( GL_DEPTH_TEST );
        glDisable( GL_STENCIL_TEST );
        glDisable( GL_BLEND );
        glDisable( GL_SCISSOR_TEST );
        // Keep the UI proof scene on a muted park-green field instead of a
        // black void, matching the production renderer's empty-world fallback.
        glClearColor( 0.11f, 0.16f, 0.10f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

        const int bar_width = std::max( 80, pixels.width() / 5 );
        const int span = std::max( 1, pixels.width() - bar_width );
        const int x = static_cast<int>( ( m_frame * 5 ) % span );
        glEnable( GL_SCISSOR_TEST );
        glScissor( x, pixels.height() / 3, bar_width, std::max( 24, pixels.height() / 18 ) );
        glClearColor( 0.84f, 0.52f, 0.16f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );
        glDisable( GL_SCISSOR_TEST );
        glUseProgram( 0 );
        glBindVertexArray( 0 );
    }

    void render()
    {
        if ( !m_gl_context || !m_rml_context || !m_renderer ) return;
        if ( !m_gl_context->makeCurrent( this ) ) return;
        ++m_frame;
        advanceShellProof();
        advanceHudProof();
        advanceInspectorProof();
        advanceManagement6BProof();
        advanceManagement6AProof();
        advanceManagement6CProof();
        advanceDeveloperUiProof();
        m_rml_context->Update();
        paintWorld();
        const GlState before = GlState::capture();
        if ( !m_renderer->beginFrame() ) qFatal( "RmlUi adapter BeginFrame failed" );
        m_rml_context->Render();
        if ( !m_renderer->endFrame() ) qFatal( "RmlUi adapter EndFrame failed" );
        if ( m_frame == 1 && !m_renderer->lastRestorationReport().empty() )
            qInfo().noquote() << "RmlUi adapter repaired upstream GL state:" << QString::fromStdString( m_renderer->lastRestorationReport() );
        const GlState after = GlState::capture();
        if ( !before.compatibleWith( after ) )
        {
            ++m_gl_warning_count;
            qWarning() << "RmlUi GL state mismatch detected";
            before.logDifferences( after );
        }
        if ( const GLenum error = glGetError(); error != GL_NO_ERROR ) { ++m_gl_warning_count; qWarning() << "OpenGL error after UI render:" << error; }
        if ( m_management6b_self_test && m_frame == 13 )
            captureFramebuffer( "/management-6b-inventory-framebuffer.bmp" );
        else if ( m_management6b_self_test && m_frame == 15 )
            captureFramebuffer( "/management-6b-grown-inventory-framebuffer.bmp" );
        else if ( m_management6b_self_test && m_frame == 17 )
            captureFramebuffer( "/management-6b-filtered-inventory-framebuffer.bmp" );
        else if ( m_management6b_self_test && m_frame == 51 )
            captureFramebuffer( "/management-6b-hierarchy-framebuffer.bmp" );
        else if ( m_hud_self_test && m_frame == 35 )
            captureFramebuffer( "/hud-build-panel-framebuffer.bmp" );
        else if ( m_management6b_self_test && m_frame == 40 )
            captureFramebuffer( "/management-6b-skills-framebuffer.bmp" );
        else if ( m_management6c_self_test && m_frame == 56 )
            captureFramebuffer( "/management-6c-framebuffer.bmp" );
        else if ( m_management6c_self_test && m_frame == 60 )
            captureFramebuffer( "/management-6c-lower-framebuffer.bmp" );
        else if ( ( m_self_test && m_frame == 60 ) || m_capture_requested )
        {
            captureFramebuffer();
            m_capture_requested = false;
        }
        m_gl_context->swapBuffers( this );
        if ( m_self_test && m_frame == 60 ) QTimer::singleShot( 0, this, &QWindow::close );
    }

    void captureFramebuffer( const QString& explicit_filename = {} )
    {
        const QSize pixels = physicalSize();
        QImage framebuffer( pixels, QImage::Format_RGBA8888 );
        glPixelStorei( GL_PACK_ALIGNMENT, 1 );
        glReadBuffer( GL_BACK );
        glReadPixels( 0, 0, pixels.width(), pixels.height(), GL_RGBA, GL_UNSIGNED_BYTE, framebuffer.bits() );
        const QString filename = explicit_filename.isEmpty() ? ( m_developer_ui_self_test ? "/developer-ui-framebuffer.bmp" : m_management6c_self_test ? "/management-6c-framebuffer.bmp" : m_management6a_self_test ? "/management-6a-framebuffer.bmp" : m_management6b_self_test ? "/management-6b-framebuffer.bmp" : m_inspector_self_test ? "/inspector-framebuffer.bmp" : m_hud_self_test ? "/hud-framebuffer.bmp" : "/spike-framebuffer.bmp" ) : explicit_filename;
        const QString path = QCoreApplication::applicationDirPath() + filename;
        if ( !framebuffer.mirrored().save( path, "BMP" ) ) qFatal( "Failed to save spike framebuffer evidence" );
        qInfo() << "Framebuffer evidence saved:" << path << pixels;
    }

    void runLifecycleProbe()
    {
        if ( !m_rml_context ) return;
        for ( int i = 0; i < 100; ++i )
        {
            Rml::ElementDocument* probe = m_rml_context->LoadDocument( "cycle.rml" );
            if ( !probe ) qFatal( "Lifecycle probe document load failed at cycle %d", i );
            m_rml_context->UnloadDocument( probe );
        }
        qInfo() << "Lifecycle probe passed: 100 load/unload cycles";
        requestUpdate();
    }

    void shutdown()
    {
        m_tick.stop();
        if ( !m_gl_context ) return;
        if ( !m_gl_context->makeCurrent( this ) )
        {
            qCritical() << "Failed to make Qt OpenGL context current for RmlUi shutdown";
            return;
        }
        if ( m_shell_binding ) m_shell_binding->shutdown();
        if ( m_shell_self_test ) { QFile proof( QCoreApplication::applicationDirPath() + "/shell-self-test.log" ); if ( proof.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) { proof.write( "SHELL_SELF_TEST_PASS\nMainNewBackLoadSettingsExit=true\nPauseResumePending=true\nListenerTeardown=true\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: " + QByteArray::number( m_gl_warning_count ) + "\n" ); proof.close(); } }
        if ( m_hud_binding ) m_hud_binding->shutdown();
        if ( m_inspector_binding ) { const auto before=m_inspector_binding->listenerCount(); m_inspector_binding->shutdown(); if(before==0||m_inspector_binding->listenerCount()!=0||m_inspector_binding->document())qFatal("Inspector listener teardown failed"); qInfo()<<"Inspector event listener teardown passed:" << before << "listeners removed"; if(m_inspector_self_test){QFile proof(QCoreApplication::applicationDirPath()+"/inspector-self-test.log");if(!proof.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Inspector proof log could not be written");proof.write("Inspector vertical proof passed\nInspector click ownership passed\nInspector event listener teardown passed\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: "+QByteArray::number(m_gl_warning_count)+"\n");proof.close();} }
        if(m_management6b_binding){const auto before=m_management6b_binding->listenerCount();m_management6b_controller->endWorld();m_management6b_binding->shutdown();const bool lifecycle=before>0&&m_management6b_binding->listenerCount()==0&&!m_management6b_binding->populationDocument()&&!m_management6b_binding->inventoryDocument();if(!lifecycle)qFatal("Management 6B listener teardown failed");if(m_management6b_self_test){QFile proof(QCoreApplication::applicationDirPath()+"/management-6b-self-test.log");if(!proof.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Management 6B proof log could not be written");proof.write("MANAGEMENT_6B_SELF_TEST_PASS\nPopulationMutationQueued="+QByteArray(m_management6b_population_queued?"true":"false")+"\nInventoryWatchQueued="+QByteArray(m_management6b_inventory_queued?"true":"false")+"\nScheduleKeyboardAlternative="+QByteArray(m_management6b_schedule_keyboard?"true":"false")+"\nStaleEpochRejected="+QByteArray(m_management6b_stale_epoch?"true":"false")+"\nSelectionStable="+QByteArray(m_management6b_selection_stable?"true":"false")+"\nInputOwnership="+QByteArray(m_management6b_input_ownership?"true":"false")+"\nTitleBarDragged="+QByteArray(m_management6b_dragged?"true":"false")+"\nMultipleWindowsVisible="+QByteArray(m_management6b_multi_window?"true":"false")+"\nLifecycleClean=true\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: "+QByteArray::number(m_gl_warning_count)+"\n");proof.close();}}
        m_management6b_controller.reset();m_management6b_binding.reset();
        if(m_management6a_binding){const auto before=m_management6a_binding->listenerCount();m_management6a_controller->endWorld();m_management6a_binding->shutdown();const bool lifecycle=before>0&&m_management6a_binding->listenerCount()==0&&!m_management6a_binding->workshopDocument()&&!m_management6a_binding->stockpileDocument()&&!m_management6a_binding->agricultureDocument();if(!lifecycle)qFatal("Management 6A listener teardown failed");if(m_management6a_self_test){QFile proof(QCoreApplication::applicationDirPath()+"/management-6a-self-test.log");if(!proof.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Management 6A proof log could not be written");proof.write("MANAGEMENT_6A_SELF_TEST_PASS\nProductionMutationQueued="+QByteArray(m_management6a_production_queued?"true":"false")+"\nStableRowKeyboard="+QByteArray(m_management6a_stable_keyboard?"true":"false")+"\nSearchFocusRestored="+QByteArray(m_management6a_search_focus?"true":"false")+"\nConfirmationBlockedInput="+QByteArray(m_management6a_confirmation_blocked?"true":"false")+"\nConfirmationOriginScoped="+QByteArray(m_management6a_confirmation_origin?"true":"false")+"\nLargeListPaged="+QByteArray(m_management6a_large_paged?"true":"false")+"\nStockpileMutationQueued="+QByteArray(m_management6a_stockpile_queued?"true":"false")+"\nAgricultureMutationQueued="+QByteArray(m_management6a_agriculture_queued?"true":"false")+"\nStaleRevisionRejected="+QByteArray(m_management6a_stale_revision?"true":"false")+"\nInputOwnership="+QByteArray(m_management6a_input_ownership?"true":"false")+"\nLifecycleClean=true\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: "+QByteArray::number(m_gl_warning_count)+"\n");proof.close();}}
        m_management6a_controller.reset();m_management6a_binding.reset();
        if(m_management6c_binding){const auto before=m_management6c_binding->listenerCount();m_management6c_controller->endWorld();m_management6c_binding->shutdown();const bool lifecycle=before>0&&m_management6c_binding->listenerCount()==0&&!m_management6c_binding->militaryDocument()&&!m_management6c_binding->diplomacyDocument();if(!lifecycle)qFatal("Management 6C listener teardown failed");if(m_management6c_self_test){QFile proof(QCoreApplication::applicationDirPath()+"/management-6c-self-test.log");if(!proof.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Management 6C proof log could not be written");proof.write("MANAGEMENT_6C_SELF_TEST_PASS\nMilitaryMutationQueued="+QByteArray(m_management6c_military_queued?"true":"false")+"\nMissionMutationQueued="+QByteArray(m_management6c_mission_queued?"true":"false")+"\nStableRowKeyboard="+QByteArray(m_management6c_keyboard?"true":"false")+"\nConfirmationBlockedAndScoped="+QByteArray(m_management6c_confirmation?"true":"false")+"\nDiscoveryMasked="+QByteArray(m_management6c_discovery_masked?"true":"false")+"\nLargeListPaged="+QByteArray(m_management6c_large_paged?"true":"false")+"\nUnrelatedDomStable="+QByteArray(m_management6c_no_unrelated_rebuild?"true":"false")+"\nStaleEpochRejected="+QByteArray(m_management6c_stale_epoch?"true":"false")+"\nRouteCloseAndInputOwnership="+QByteArray(m_management6c_input_ownership?"true":"false")+"\nReadableListDetailLayout="+QByteArray(m_management6c_layout_readable?"true":"false")+"\nLowerMissionControlsReachable="+QByteArray(m_management6c_lower_controls_visible?"true":"false")+"\nLifecycleClean=true\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: "+QByteArray::number(m_gl_warning_count)+"\n");proof.close();}}
        m_management6c_controller.reset();m_management6c_binding.reset();
        if(m_developer_ui_binding){const auto before=m_developer_ui_binding->listenerCount();m_developer_ui_controller->endWorld();m_developer_ui_binding->shutdown();const bool lifecycle=before>0&&m_developer_ui_binding->listenerCount()==0&&!m_developer_ui_binding->document();if(!lifecycle)qFatal("Developer UI listener teardown failed");if(m_developer_ui_self_test){QFile proof(QCoreApplication::applicationDirPath()+"/developer-ui-self-test.log");if(!proof.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Developer UI proof log could not be written");proof.write("DEVELOPER_UI_SELF_TEST_PASS\nTypedDebugAction="+QByteArray(m_developer_ui_action?"true":"false")+"\nStableRowKeyboard="+QByteArray(m_developer_ui_keyboard?"true":"false")+"\nSearchStable="+QByteArray(m_developer_ui_search?"true":"false")+"\nStaleEpochRejected="+QByteArray(m_developer_ui_stale?"true":"false")+"\nMeasuredCounters="+QByteArray(m_developer_ui_counters?"true":"false")+"\nInputOwnership="+QByteArray(m_developer_ui_input?"true":"false")+"\nLifecycleClean=true\nLifecycle load/unload cycles: 100\nOpenGL state/error warnings: "+QByteArray::number(m_gl_warning_count)+"\n");proof.close();}}
        m_developer_ui_controller.reset();m_developer_ui_binding.reset();
        m_inspector_controller.reset();
        m_inspector_binding.reset();
        m_hud_controller.reset();
        m_hud_binding.reset();
        m_shell_controller.reset();
        m_shell_binding.reset();
        m_document = nullptr;
        qInfo().noquote() << "Final input text:" << QString::fromStdString( m_input_text );
        m_model = {};
        if ( m_rml_context )
        {
            Rml::RemoveContext( "ingnomia-qt-gl-spike" );
            m_rml_context = nullptr;
        }
        if ( m_renderer ) Rml::Shutdown();
        m_renderer.reset();
        m_files.reset();
        m_system.reset();
        m_gl_context->doneCurrent();
        m_gl_context.reset();
    }

    QTimer m_tick;
    std::unique_ptr<QOpenGLContext> m_gl_context;
    std::unique_ptr<ingnomia::ui::IngnomiaRmlUiRenderer> m_renderer;
    std::unique_ptr<QtSystemInterface> m_system;
    std::unique_ptr<QtFileInterface> m_files;
    Rml::Context* m_rml_context = nullptr;
    Rml::ElementDocument* m_document = nullptr;
    Rml::DataModelHandle m_model;
    std::array<bool, 3> m_ui_capture = {};
    int m_click_count = 0;
    Rml::String m_input_text = "Type UTF-8 here";
    float m_user_scale = 1.0f;
    unsigned long long m_frame = 0;
    bool m_self_test = false;
    bool m_capture_requested = false;
    bool m_shell_self_test = false;
    bool m_hud_self_test = false;
    bool m_inspector_self_test = false;
    bool m_management6b_self_test = false;
    bool m_management6a_self_test = false;
    bool m_management6c_self_test = false;
    bool m_developer_ui_self_test = false;
    int m_gl_warning_count = 0;
    ShellProofPort m_shell_port;
    std::unique_ptr<ingnomia::ui::shell::ShellRmlBinding> m_shell_binding;
    std::unique_ptr<ingnomia::ui::shell::ShellController> m_shell_controller;
    HudProofPort m_hud_port;
    std::unique_ptr<ingnomia::ui::hud::HudRmlBinding> m_hud_binding;
    std::unique_ptr<ingnomia::ui::hud::HudController> m_hud_controller;
    InspectorProofPort m_inspector_port;
    std::unique_ptr<ingnomia::ui::inspector::InspectorRmlBinding> m_inspector_binding;
    std::unique_ptr<ingnomia::ui::inspector::InspectorController> m_inspector_controller;
    Management6BProofPort m_management6b_port;
    std::unique_ptr<ingnomia::ui::management6b::Management6BRmlBinding> m_management6b_binding;
    std::unique_ptr<ingnomia::ui::management6b::Management6BController> m_management6b_controller;
    bool m_management6b_population_queued{},m_management6b_inventory_queued{},m_management6b_schedule_keyboard{},m_management6b_stale_epoch{},m_management6b_selection_stable{},m_management6b_input_ownership{},m_management6b_dragged{},m_management6b_multi_window{};
    Management6AProofPort m_management6a_port;
    std::unique_ptr<ingnomia::ui::management6a::Management6ARmlBinding> m_management6a_binding;
    std::unique_ptr<ingnomia::ui::management6a::Management6AController> m_management6a_controller;
    bool m_management6a_production_queued{},m_management6a_stable_keyboard{},m_management6a_search_focus{},m_management6a_confirmation_blocked{},m_management6a_confirmation_origin{},m_management6a_large_paged{},m_management6a_stockpile_queued{},m_management6a_agriculture_queued{},m_management6a_stale_revision{},m_management6a_input_ownership{};
    Management6CProofPort m_management6c_port;
    std::unique_ptr<ingnomia::ui::management6c::Management6CRmlBinding> m_management6c_binding;
    std::unique_ptr<ingnomia::ui::management6c::Management6CController> m_management6c_controller;
    bool m_management6c_military_queued{},m_management6c_mission_queued{},m_management6c_keyboard{},m_management6c_confirmation{},m_management6c_discovery_masked{},m_management6c_large_paged{},m_management6c_no_unrelated_rebuild{},m_management6c_stale_epoch{},m_management6c_input_ownership{},m_management6c_layout_readable{},m_management6c_lower_controls_visible{},m_management6c_route_closed{};
    DeveloperUiProofPort m_developer_ui_port;
    std::unique_ptr<ingnomia::ui::debug::DebugRmlBinding> m_developer_ui_binding;
    std::unique_ptr<ingnomia::ui::debug::DebugController> m_developer_ui_controller;
    bool m_developer_ui_action{},m_developer_ui_keyboard{},m_developer_ui_search{},m_developer_ui_stale{},m_developer_ui_counters{},m_developer_ui_input{};
};
} // namespace

int main( int argc, char** argv )
{
    QCoreApplication::setAttribute( Qt::AA_ShareOpenGLContexts );
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy( Qt::HighDpiScaleFactorRoundingPolicy::PassThrough );
    QGuiApplication app( argc, argv );
    const bool selfTest = QCoreApplication::arguments().contains( "--self-test" );
    const bool shellSelfTest = QCoreApplication::arguments().contains( "--shell-self-test" );
    const bool hudSelfTest = QCoreApplication::arguments().contains( "--hud-self-test" );
    const bool inspectorSelfTest = QCoreApplication::arguments().contains( "--inspector-self-test" );
    const bool management6bSelfTest = QCoreApplication::arguments().contains( "--management-6b-self-test" );
    const bool management6aSelfTest = QCoreApplication::arguments().contains( "--management-6a-self-test" );
    const bool management6cSelfTest = QCoreApplication::arguments().contains( "--management-6c-self-test" );
    const bool developerUiSelfTest = QCoreApplication::arguments().contains( "--developer-ui-self-test" );
    SpikeWindow window( selfTest || shellSelfTest || hudSelfTest || inspectorSelfTest || management6bSelfTest || management6aSelfTest || management6cSelfTest || developerUiSelfTest, shellSelfTest, hudSelfTest, inspectorSelfTest, management6bSelfTest, management6aSelfTest, management6cSelfTest, developerUiSelfTest );
    window.show();
    return app.exec();
}
