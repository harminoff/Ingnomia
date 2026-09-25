#pragma once

#include "RmlUiHost.h"

#include <QPoint>
#include <QSize>
#include <QString>
#include <QWindow>

#include <functional>
#include <memory>

class QCloseEvent;
class QExposeEvent;
class QFocusEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QOpenGLContext;
class QResizeEvent;
class QTimer;
class QWheelEvent;

namespace ingnomia::ui
{

/// Native, independently movable host for a secondary RmlUi context.
/// The native surface uses the game's existing OpenGL context. RmlUi contexts
/// therefore share one render manager and one GL resource boundary while Qt
/// still presents each inspector in its own movable top-level window.
class RmlUiDetachedWindow final : public QWindow
{
public:
    RmlUiDetachedWindow( RmlUiHost&, QOpenGLContext* shareContext, QString title, QSize initialSize );
    ~RmlUiDetachedWindow() override;

    RmlUiDetachedWindow( const RmlUiDetachedWindow& ) = delete;
    RmlUiDetachedWindow& operator=( const RmlUiDetachedWindow& ) = delete;

    bool initializeOpenGL();
    bool makeCurrent();
    void doneCurrent();
    void attachContext( RmlUiDetachedContext* );
    void setUserUiScale( float scale ) noexcept { m_userUiScale = scale; }
    void setResizable( bool value );
    void setResizeMinimumSize( QSize size );
    void setResizeHandler( std::function<void( QSize )> handler );
    void setCloseGuard(std::function<bool()> guard) { m_closeGuard=std::move(guard); }
    void setCloseHandler( std::function<void()> );
    void setDesignerFocusHandler( std::function<void()> handler );
    void setDesignerKeyHandler( std::function<bool( int, Qt::KeyboardModifiers )> handler );
    void setDesignerMousePressHandler( std::function<bool( QPointF, Qt::MouseButton, Qt::KeyboardModifiers )> handler );
    void showAndActivate();
    void stopRendering() noexcept;
    void resetView( QSize logicalSize );
    void requestAutomationCapture( QString path );
    void requestClose();

protected:
    bool event( QEvent* ) override;
    void closeEvent( QCloseEvent* ) override;
    void exposeEvent( QExposeEvent* ) override;
    void resizeEvent( QResizeEvent* ) override;
    void focusOutEvent( QFocusEvent* ) override;
    void focusInEvent( QFocusEvent* ) override;
    void keyPressEvent( QKeyEvent* ) override;
    void keyReleaseEvent( QKeyEvent* ) override;
    void mouseMoveEvent( QMouseEvent* ) override;
    void mousePressEvent( QMouseEvent* ) override;
    void mouseReleaseEvent( QMouseEvent* ) override;
    void wheelEvent( QWheelEvent* ) override;

private:
    bool isNativeDragHandle( QPointF position ) const;
    Qt::Edges resizeEdgesAt( QPointF position ) const;
    void updateResize( QPointF position );
    void updateResizeCursor( QPointF position );
    void updateMove( QPointF globalPosition );
    void queueRenderFrame();
    void renderFrame();
    void resizeUi();
    void setChromeActive( bool active );

    RmlUiHost& m_host;
    QOpenGLContext* m_shareContext = nullptr;
    QOpenGLContext* m_glContext = nullptr; // Non-owning primary game context.
    RmlUiDetachedContext* m_detachedContext = nullptr;
    QTimer* m_timer = nullptr;
    std::function<void()> m_closeHandler;
    std::function<void( QSize )> m_resizeHandler;
    std::function<void()> m_designerFocusHandler;
    std::function<bool( int, Qt::KeyboardModifiers )> m_designerKeyHandler;
    std::function<bool( QPointF, Qt::MouseButton, Qt::KeyboardModifiers )> m_designerMousePressHandler;
    Qt::Edges m_resizeEdges = Qt::Edges();
    QPointF m_resizeStart;
    QRect m_resizeGeometry;
    QPoint m_moveStartGlobal;
    QPoint m_moveStartWindow;
    QSize m_resizeMinimumSize = QSize( 240, 240 );
    bool m_resizing = false;
    bool m_moving = false;
    bool m_resizable = false;
    float m_userUiScale = 1.0f;
    bool m_closeQueued = false;
    bool m_automationCaptureDone = false;
    QString m_automationCapturePath;
    bool m_inspectorLayoutTraceDone = false;
    int m_automationCaptureSkipFrames = 0;
    std::function<bool()> m_closeGuard;
    bool m_renderingEnabled = false;
    bool m_frameQueued = false;
    bool m_rendering = false;
};

} // namespace ingnomia::ui
