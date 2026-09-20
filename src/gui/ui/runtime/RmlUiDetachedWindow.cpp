#include "RmlUiDetachedWindow.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>

#include <glad/gl.h>

#include <QCloseEvent>
#include <QCursor>
#include <QDebug>
#include <QDateTime>
#include <QExposeEvent>
#include <QFile>
#include <QFocusEvent>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QResizeEvent>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTextStream>
#include <QWheelEvent>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Context.h>

#include <algorithm>
#include <vector>

namespace ingnomia::ui
{

RmlUiDetachedWindow::RmlUiDetachedWindow( RmlUiHost& host, QOpenGLContext* shareContext,
    QString title, QSize initialSize ) :
    QWindow(),
    m_host( host ),
    m_shareContext( shareContext )
{
    setSurfaceType( QWindow::OpenGLSurface );
    setFlags( Qt::Window | Qt::Tool | Qt::FramelessWindowHint );
    setTitle( std::move( title ) );
    setResizeMinimumSize( QSize( 240, 240 ) );
    resize( initialSize.expandedTo( m_resizeMinimumSize ) );

    m_timer = new QTimer( this );
    // Present the already-rendered preview texture at a steady 60-ish FPS. The
    // previous coarse 33 ms timer sampled smooth creature interpolation only at
    // 30 FPS, so the camera still looked stepped even when the main world was
    // rendering at the monitor refresh rate.
    m_timer->setTimerType( Qt::PreciseTimer );
    QObject::connect( m_timer, &QTimer::timeout, this, [this] { queueRenderFrame(); } );
}

RmlUiDetachedWindow::~RmlUiDetachedWindow()
{
    m_closeHandler = {};
    if ( m_timer ) m_timer->stop();
    if ( m_glContext && m_glContext->makeCurrent( this ) )
    {
        m_glContext->doneCurrent();
    }
    m_glContext = nullptr;
}

bool RmlUiDetachedWindow::initializeOpenGL()
{
    if ( m_glContext ) return true;
    if ( !m_shareContext )
    {
        qCritical() << "Detached RmlUi window requires a share context";
        return false;
    }

	QSurfaceFormat format = m_shareContext->format();
	// The detached surface is larger than the RmlUi panel so the panel can
	// retain its in-game layout. Request alpha for the unused host area and
	// leave those pixels transparent instead of carrying the grey clear color
	// with the panel when it is moved outside the game window.
	format.setAlphaBufferSize( 8 );
	setFormat( format );
    create();
    // A single context is intentional: RmlUi's GL3 backend keeps renderer
    // resources per interface, while the game and detached views must not
    // compete across separate shared-context resource/state boundaries.
    m_glContext = m_shareContext;
    return true;
}

bool RmlUiDetachedWindow::makeCurrent()
{
    return m_glContext && m_glContext->makeCurrent( this );
}

void RmlUiDetachedWindow::doneCurrent()
{
    if ( m_glContext ) m_glContext->doneCurrent();
}

void RmlUiDetachedWindow::attachContext( RmlUiDetachedContext* context )
{
    m_detachedContext = context;
    m_renderingEnabled = false;
    m_frameQueued = false;
    resizeUi();
}

void RmlUiDetachedWindow::setCloseHandler( std::function<void()> handler )
{
    m_closeHandler = std::move( handler );
}

void RmlUiDetachedWindow::setResizeMinimumSize( QSize size )
{
    m_resizeMinimumSize = size.expandedTo( QSize( 240, 240 ) );
    setMinimumSize( m_resizeMinimumSize );
}

void RmlUiDetachedWindow::setResizeHandler( std::function<void( QSize )> handler )
{
    m_resizeHandler = std::move( handler );
}

void RmlUiDetachedWindow::showAndActivate()
{
    m_closeQueued = false;
    m_renderingEnabled = true;
    if ( m_timer && !m_timer->isActive() ) m_timer->start( 16 );
    show();
    raise();
    requestActivate();
    // Defer the first frame until Qt returns to the event loop. show() can
    // synchronously deliver expose events while the main GL context is still
    // being restored after detached-window creation.
    queueRenderFrame();
}

void RmlUiDetachedWindow::stopRendering() noexcept
{
    m_renderingEnabled = false;
    if ( m_timer ) m_timer->stop();
}

void RmlUiDetachedWindow::resetView( QSize logicalSize )
{
    stopRendering();
    m_frameQueued = false;
    m_rendering = false;
    m_automationCaptureDone = false;
    m_inspectorLayoutTraceDone = false;
    m_automationCaptureSkipFrames = qMax( 0, qEnvironmentVariableIntValue( "INGNOMIA_AUTOMATE_DETACHED_CAPTURE_DELAY_FRAMES" ) );

    if ( logicalSize.isValid() ) resize( logicalSize );
    // A resize event is not guaranteed while a parked QWindow is hidden. Apply
    // the RmlUi dimensions explicitly so a reopened inspector cannot retain the
    // expanded document viewport from its previous lifetime. Keep both resize
    // and layout on the detached context; RmlUi document updates can dirty
    // resources which must be owned by the same GL context as the render pass.
    if ( !makeCurrent() ) return;
    const QSize physicalSize( std::max( 1, qRound( width() * devicePixelRatio() ) ),
        std::max( 1, qRound( height() * devicePixelRatio() ) ) );
    if ( m_detachedContext )
    {
        (void)m_host.resizeDetached( *m_detachedContext, physicalSize,
            static_cast<float>( devicePixelRatio() ) * m_userUiScale );
        (void)m_host.updateDetached( *m_detachedContext );
    }
    const int physicalWidth = std::max( 1, qRound( width() * devicePixelRatio() ) );
    const int physicalHeight = std::max( 1, qRound( height() * devicePixelRatio() ) );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    glViewport( 0, 0, physicalWidth, physicalHeight );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_STENCIL_TEST );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_CULL_FACE );
    glUseProgram( 0 );
    glBindVertexArray( 0 );
    glActiveTexture( GL_TEXTURE0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
    doneCurrent();
    m_host.setSystemWindow( nullptr );
}

void RmlUiDetachedWindow::requestClose()
{
    close();
}

bool RmlUiDetachedWindow::event( QEvent* event )
{
    if ( event && event->type() == QEvent::InputMethod && m_detachedContext )
    {
        auto* input = static_cast<QInputMethodEvent*>( event );
        m_host.setSystemWindow( this );
        m_detachedContext->input().committedText( input->commitString() );
        // RmlUi exposes one process-wide SystemInterface. Do not leave it
        // pointing at an auxiliary window between events; the next primary
        // HUD update must always resolve input/cursor services against the
        // game's window.
        m_host.setSystemWindow( nullptr );
        input->accept();
        return true;
    }
    return QWindow::event( event );
}

void RmlUiDetachedWindow::closeEvent( QCloseEvent* event )
{
    // A close can be deferred while the owner context is restored. Stop the
    // detached frame loop immediately so a hidden inspector cannot continue
    // submitting OpenGL work during that handoff.
    m_renderingEnabled = false;
    if ( m_timer ) m_timer->stop();
    event->accept();
    if ( m_closeQueued || !m_closeHandler ) return;
    m_closeQueued = true;
    QTimer::singleShot( 0, this, [this]
    {
        m_closeQueued = false;
        if ( m_closeHandler ) m_closeHandler();
    } );
}

void RmlUiDetachedWindow::exposeEvent( QExposeEvent* event )
{
    QWindow::exposeEvent( event );
    if ( isExposed() ) queueRenderFrame();
}

void RmlUiDetachedWindow::resizeEvent( QResizeEvent* event )
{
    QWindow::resizeEvent( event );
    resizeUi();
    if ( m_resizeHandler && event->size() != event->oldSize() ) m_resizeHandler( event->size() );
    if ( isExposed() ) queueRenderFrame();
}

void RmlUiDetachedWindow::focusOutEvent( QFocusEvent* event )
{
    if ( m_moving )
    {
        m_moving = false;
        setMouseGrabEnabled( false );
    }
    if ( m_detachedContext ) m_detachedContext->input().cancelInteraction();
    QWindow::focusOutEvent( event );
}

void RmlUiDetachedWindow::keyPressEvent( QKeyEvent* event )
{
    if ( !m_detachedContext ) return;
    m_host.setSystemWindow( this );
    const auto key = m_detachedContext->input().keyDown( event->key(), event->modifiers() );
    const auto text = m_detachedContext->input().committedText( event->text() );
    m_host.setSystemWindow( nullptr );
    if ( key.uiConsumed || text.uiConsumed ) event->accept();
    else event->ignore();
}

void RmlUiDetachedWindow::keyReleaseEvent( QKeyEvent* event )
{
    if ( !m_detachedContext ) return;
    m_host.setSystemWindow( this );
    const auto result = m_detachedContext->input().keyUp( event->key(), event->modifiers() );
    m_host.setSystemWindow( nullptr );
    if ( result.uiConsumed ) event->accept();
    else event->ignore();
}

void RmlUiDetachedWindow::mouseMoveEvent( QMouseEvent* event )
{
    if ( m_resizing )
    {
        updateResize( event->position() );
        event->accept();
        return;
    }
    if ( m_moving )
    {
        updateMove( event->globalPosition() );
        event->accept();
        return;
    }
    updateResizeCursor( event->position() );
    if ( !m_detachedContext ) return;
    m_host.setSystemWindow( this );
    m_detachedContext->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
    m_host.setSystemWindow( nullptr );
    event->accept();
}

void RmlUiDetachedWindow::mousePressEvent( QMouseEvent* event )
{
    if ( !m_detachedContext ) return;
    if ( event->button() == Qt::LeftButton )
    {
        const auto edges = resizeEdgesAt( event->position() );
        if ( edges )
        {
            m_resizing = true;
            m_resizeEdges = edges;
            m_resizeStart = event->position();
            m_resizeGeometry = geometry();
            event->accept();
            return;
        }
    }
    if ( event->button() == Qt::LeftButton && isNativeDragHandle( event->position() ) )
    {
        m_moving = true;
        m_moveStartGlobal = event->globalPosition().toPoint();
        m_moveStartWindow = position();
        setMouseGrabEnabled( true );
        event->accept();
        return;
    }
    m_host.setSystemWindow( this );
    m_detachedContext->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
    m_detachedContext->input().mouseButtonDown( event->button(), event->modifiers() );
    m_host.setSystemWindow( nullptr );
    event->accept();
}

bool RmlUiDetachedWindow::isNativeDragHandle( QPointF position ) const
{
    if ( !m_detachedContext || !m_detachedContext->context() ) return false;
    const qreal dpr = std::max<qreal>( 0.01, devicePixelRatio() );
    const auto point = Rml::Vector2f( static_cast<float>( position.x() * dpr ),
        static_cast<float>( position.y() * dpr ) );
    auto* element = m_detachedContext->context()->GetElementAtPoint( point );
    while ( element )
    {
        if ( element->GetTagName() == "button" ) return false;
        if ( element->GetTagName() == "handle" ) return true;
        element = element->GetParentNode();
    }
    // The detached orders document is rendered at a density-independent
    // scale. If a platform reports the pointer in a coordinate space that
    // misses the RmlUi handle, retain the same title-bar contract from the
    // native surface. Reserve the right-side controls so Close/Deconstruct
    // remain ordinary buttons.
    if ( auto* document = m_detachedContext->context()->GetDocument( 0 ) )
    {
        if ( document->GetElementById( "hud_orders_tools_panel" ) )
        {
            // If RmlUi misses the custom <handle> during a density/layout
            // transition, use the laid-out title bar as the native drag zone.
            // This keeps the fixed-size tool window movable without making the
            // close button draggable.
            for ( const char* id : { "hud_mine_drag_handle", "hud_agriculture_drag_handle",
                "hud_designations_drag_handle", "hud_jobs_drag_handle", "hud_build_drag_handle" } )
            {
                if ( auto* header = document->GetElementById( id ) )
                {
                    const auto offset = header->GetAbsoluteOffset( Rml::BoxArea::Border );
                    const auto size = header->GetBox().GetSize();
                    if ( point.x >= offset.x && point.x <= offset.x + size.x
                        && point.y >= offset.y && point.y <= offset.y + size.y )
                        return true;
                }
            }
            return position.y() <= 72.0 && position.x() < width() - 110.0;
        }
    }
    return false;
}

void RmlUiDetachedWindow::mouseReleaseEvent( QMouseEvent* event )
{
    if ( !m_detachedContext ) return;
    if ( m_resizing && event->button() == Qt::LeftButton )
    {
        m_resizing = false;
        m_resizeEdges = Qt::Edges();
        updateResizeCursor( event->position() );
        event->accept();
        return;
    }
    if ( m_moving && event->button() == Qt::LeftButton )
    {
        m_moving = false;
        setMouseGrabEnabled( false );
        event->accept();
        return;
    }
    m_host.setSystemWindow( this );
    m_detachedContext->input().mouseMove( event->position(), devicePixelRatio(), event->modifiers() );
    m_detachedContext->input().mouseButtonUp( event->button(), event->modifiers() );
    m_host.setSystemWindow( nullptr );
    event->accept();
}

Qt::Edges RmlUiDetachedWindow::resizeEdgesAt( QPointF position ) const
{
    if ( !m_resizable ) return Qt::Edges();
    constexpr qreal border = 12.0;
    Qt::Edges edges;
    if ( position.x() <= border ) edges |= Qt::LeftEdge;
    if ( position.x() >= width() - border ) edges |= Qt::RightEdge;
    if ( position.y() <= border ) edges |= Qt::TopEdge;
    if ( position.y() >= height() - border ) edges |= Qt::BottomEdge;
    return edges;
}

void RmlUiDetachedWindow::updateResizeCursor( QPointF position )
{
    const auto edges = m_resizing ? m_resizeEdges : resizeEdgesAt( position );
    Qt::CursorShape shape = Qt::ArrowCursor;
    if ( ( edges & ( Qt::LeftEdge | Qt::RightEdge ) ) && ( edges & ( Qt::TopEdge | Qt::BottomEdge ) ) )
        shape = ( edges & ( Qt::LeftEdge | Qt::BottomEdge ) ) || ( edges & ( Qt::RightEdge | Qt::TopEdge ) ) ? Qt::SizeBDiagCursor : Qt::SizeFDiagCursor;
    else if ( edges & ( Qt::LeftEdge | Qt::RightEdge ) ) shape = Qt::SizeHorCursor;
    else if ( edges & ( Qt::TopEdge | Qt::BottomEdge ) ) shape = Qt::SizeVerCursor;
    setCursor( QCursor( shape ) );
}

void RmlUiDetachedWindow::updateResize( QPointF position )
{
    if ( !m_resizing || !m_resizeEdges ) return;
    const QPoint delta( qRound( position.x() - m_resizeStart.x() ), qRound( position.y() - m_resizeStart.y() ) );
    QRect next = m_resizeGeometry;
    if ( m_resizeEdges & Qt::LeftEdge ) next.setLeft( qMin( m_resizeGeometry.left() + delta.x(), m_resizeGeometry.right() - m_resizeMinimumSize.width() + 1 ) );
    if ( m_resizeEdges & Qt::RightEdge ) next.setRight( qMax( m_resizeGeometry.right() + delta.x(), m_resizeGeometry.left() + m_resizeMinimumSize.width() - 1 ) );
    if ( m_resizeEdges & Qt::TopEdge ) next.setTop( qMin( m_resizeGeometry.top() + delta.y(), m_resizeGeometry.bottom() - m_resizeMinimumSize.height() + 1 ) );
    if ( m_resizeEdges & Qt::BottomEdge ) next.setBottom( qMax( m_resizeGeometry.bottom() + delta.y(), m_resizeGeometry.top() + m_resizeMinimumSize.height() - 1 ) );
    setGeometry( next );
}

void RmlUiDetachedWindow::updateMove( QPointF globalPosition )
{
    if ( !m_moving ) return;
    const QPoint delta = globalPosition.toPoint() - m_moveStartGlobal;
    setPosition( m_moveStartWindow + delta );
}

void RmlUiDetachedWindow::wheelEvent( QWheelEvent* event )
{
    if ( !m_detachedContext ) return;
    m_host.setSystemWindow( this );
    m_detachedContext->input().mouseWheel( event->angleDelta(), event->pixelDelta(), event->modifiers() );
    m_host.setSystemWindow( nullptr );
    event->accept();
}

void RmlUiDetachedWindow::resizeUi()
{
    if ( !m_detachedContext || !makeCurrent() ) return;
    const QSize physicalSize( std::max( 1, qRound( width() * devicePixelRatio() ) ),
        std::max( 1, qRound( height() * devicePixelRatio() ) ) );
    m_host.resizeDetached( *m_detachedContext, physicalSize, static_cast<float>( devicePixelRatio() ) * m_userUiScale );
    m_host.setSystemWindow( nullptr );
    doneCurrent();
}

void RmlUiDetachedWindow::queueRenderFrame()
{
    if ( !m_renderingEnabled || m_frameQueued || !m_detachedContext ) return;
    m_frameQueued = true;
    QTimer::singleShot( 0, this, [this]
    {
        m_frameQueued = false;
        renderFrame();
    } );
}

void RmlUiDetachedWindow::renderFrame()
{
    if ( !m_renderingEnabled || m_rendering || !m_detachedContext || !isExposed() || !makeCurrent() ) return;
    m_rendering = true;

    const int physicalWidth = std::max( 1, qRound( width() * devicePixelRatio() ) );
    const int physicalHeight = std::max( 1, qRound( height() * devicePixelRatio() ) );

    glViewport( 0, 0, physicalWidth, physicalHeight );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
    const bool updated = m_host.updateDetached( *m_detachedContext );
    if ( updated && !m_inspectorLayoutTraceDone && qEnvironmentVariableIsSet( "INGNOMIA_TRACE_INSPECTOR_SKILLS" ) )
    {
        if ( auto* document = m_detachedContext->context()->GetDocument( 0 ) )
            if ( auto* panel = document->GetElementById( "creature_preview_expertise_panel" ); panel && panel->IsVisible( true ) )
                if ( auto* rows = document->GetElementById( "creature_preview_skills" ) )
                {
                    const auto offset = rows->GetAbsoluteOffset();
                    qInfo() << "Inspector skill layout" << title()
                        << "children" << rows->GetNumChildren()
                        << "rows" << rows->GetOffsetWidth() << rows->GetOffsetHeight()
                        << "offset" << offset.x << offset.y
                        << "panel" << panel->GetOffsetWidth() << panel->GetOffsetHeight();
                    if ( rows->GetNumChildren() > 0 )
                    {
                        if ( auto* child = rows->GetChild( 0 ) )
                        {
                            const auto childOffset = child->GetAbsoluteOffset();
                            qInfo() << "Inspector first skill layout"
                                << child->GetInnerRML().c_str()
                                << "visible" << child->IsVisible( true )
                                << "size" << child->GetOffsetWidth() << child->GetOffsetHeight()
                                << "offset" << childOffset.x << childOffset.y;
                        }
                    }
                    const auto tracePath = qEnvironmentVariable( "INGNOMIA_TRACE_INSPECTOR_SKILLS_PATH" );
                    if ( !tracePath.isEmpty() )
                    {
                        QFile trace( tracePath );
                        if ( trace.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
                        {
                            QTextStream stream( &trace );
                            stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs )
                                << " layout title=" << title()
                                << " children=" << rows->GetNumChildren()
                                << " rows=" << rows->GetOffsetWidth() << 'x' << rows->GetOffsetHeight()
                                << " offset=" << offset.x << ',' << offset.y
                                << " panel=" << panel->GetOffsetWidth() << 'x' << panel->GetOffsetHeight();
                            if ( rows->GetNumChildren() > 0 )
                            {
                                if ( auto* child = rows->GetChild( 0 ) )
                                {
                                    const auto childOffset = child->GetAbsoluteOffset();
                                    stream << " firstVisible=" << ( child->IsVisible( true ) ? "true" : "false" )
                                        << " firstSize=" << child->GetOffsetWidth() << 'x' << child->GetOffsetHeight()
                                        << " firstOffset=" << childOffset.x << ',' << childOffset.y;
                                }
                            }
                            stream << '\n';
                        }
                    }
                    m_inspectorLayoutTraceDone = true;
                }
    }
    const bool rendered = m_host.renderDetached( *m_detachedContext );
    if ( !updated || !rendered )
        qWarning() << "Detached RmlUi frame failed for" << title();

    // Keep detached-window rendering independently testable. The normal game
    // path never reads pixels back; the opt-in probe captures this window's
    // actual OpenGL surface rather than the main game's framebuffer.
    const QString capturePath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH" );
    if ( !m_automationCaptureDone && !capturePath.isEmpty() )
    {
        if ( m_automationCaptureSkipFrames > 0 )
        {
            --m_automationCaptureSkipFrames;
        }
        else
        {
        std::vector<unsigned char> pixels( static_cast<std::size_t>( physicalWidth )
            * static_cast<std::size_t>( physicalHeight ) * 4 );
        glReadPixels( 0, 0, physicalWidth, physicalHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
        const QImage frame( pixels.data(), physicalWidth, physicalHeight, QImage::Format_RGBA8888 );
        if ( frame.mirrored( false, true ).save( capturePath ) )
            qInfo() << "Detached RmlUi capture saved:" << capturePath;
        else
            qWarning() << "Detached RmlUi capture failed:" << capturePath;
        const QString layoutPath = qEnvironmentVariable( "INGNOMIA_AUTOMATE_DETACHED_LAYOUT_PATH" );
        if( !layoutPath.isEmpty() )
        {
            QJsonArray elements;
            std::function<void(Rml::Element*, const QString&)> visit = [&]( Rml::Element* element, const QString& parent ) {
                if( !element ) return;
                const auto offset = element->GetAbsoluteOffset( Rml::BoxArea::Border );
                const QString id = QString::fromStdString( element->GetId() );
                QJsonObject row;
                row["id"] = id;
                row["parent"] = parent;
                row["tag"] = QString::fromStdString( element->GetTagName() );
                row["visible"] = element->IsVisible( true );
                row["x"] = offset.x; row["y"] = offset.y;
                row["width"] = element->GetOffsetWidth(); row["height"] = element->GetOffsetHeight();
                row["children"] = element->GetNumChildren();
                row["rml"] = QString::fromStdString( element->GetInnerRML() ).left( 512 );
                elements.append( row );
                for( int child = 0; child < element->GetNumChildren(); ++child ) visit( element->GetChild( child ), id.isEmpty() ? parent : id );
            };
            auto* context = m_detachedContext->context();
            for( int index = 0; index < context->GetNumDocuments(); ++index )
                if( auto* document = context->GetDocument( index ); document && document->IsVisible() ) visit( document, {} );
            QFile layoutFile( layoutPath );
            if( layoutFile.open( QIODevice::WriteOnly ) ) layoutFile.write( QJsonDocument( elements ).toJson() );
        }
        m_automationCaptureDone = true;
        }
    }
    m_glContext->swapBuffers( this );
    doneCurrent();
    // swapBuffers/doneCurrent can return control to Qt while the detached
    // surface is still the last RmlUi window touched. Restore the owner after
    // the native handoff as well as inside the host's update/render methods.
    m_host.setSystemWindow( nullptr );
    m_rendering = false;
}

} // namespace ingnomia::ui
