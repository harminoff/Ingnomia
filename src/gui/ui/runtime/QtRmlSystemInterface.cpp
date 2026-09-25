#include "QtRmlSystemInterface.h"

#include <QClipboard>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QInputMethod>
#include <QWindow>

namespace ingnomia::ui
{
namespace
{
QString fromRml( const Rml::String& value )
{
    return QString::fromUtf8( value.data(), static_cast<qsizetype>( value.size() ) );
}

Rml::String toRml( const QString& value )
{
    const QByteArray utf8 = value.toUtf8();
    return Rml::String( utf8.constData(), static_cast<size_t>( utf8.size() ) );
}
} // namespace

QtRmlSystemInterface::QtRmlSystemInterface( QWindow* window, Translator translator ) :
    m_window( window ),
    m_translator( std::move( translator ) )
{
    m_elapsed.start();
}

void QtRmlSystemInterface::setWindow( QWindow* window )
{
    m_window = window;
}

void QtRmlSystemInterface::setMapCursor( QWindow* window, Qt::CursorShape shape )
{
    const bool changed = m_mapWindow != window || m_mapCursor != shape;
    m_mapWindow = window;
    m_mapCursor = shape;
    if ( changed && m_overMap && window ) window->setCursor( QCursor( shape ) );
}

double QtRmlSystemInterface::GetElapsedTime()
{
    return static_cast<double>( m_elapsed.nsecsElapsed() ) / 1'000'000'000.0;
}

int QtRmlSystemInterface::TranslateString( Rml::String& translated, const Rml::String& input )
{
    if ( !m_translator )
    {
        translated = input;
        return 0;
    }
    const QString source = fromRml( input );
    const QString result = m_translator( source );
    translated = toRml( result.isNull() ? source : result );
    return result.isNull() || result == source ? 0 : 1;
}

bool QtRmlSystemInterface::LogMessage( Rml::Log::Type type, const Rml::String& message )
{
    const QString text = fromRml( message );
    switch ( type )
    {
        case Rml::Log::LT_ALWAYS:
        case Rml::Log::LT_INFO: qInfo().noquote() << "[RmlUi]" << text; break;
        case Rml::Log::LT_DEBUG: qDebug().noquote() << "[RmlUi]" << text; break;
        case Rml::Log::LT_WARNING: qWarning().noquote() << "[RmlUi]" << text; break;
        case Rml::Log::LT_ERROR:
        case Rml::Log::LT_ASSERT: qCritical().noquote() << "[RmlUi]" << text; break;
        default: qWarning().noquote() << "[RmlUi unknown log level]" << text; break;
    }
    return true;
}

void QtRmlSystemInterface::SetMouseCursor( const Rml::String& cursorName )
{
    if ( !m_window ) return;
    const QString name = fromRml( cursorName );
    Qt::CursorShape cursor = Qt::ArrowCursor;
    if ( m_window == m_mapWindow ) m_overMap = name.isEmpty();
    if ( name.isEmpty() && m_window == m_mapWindow ) cursor = m_mapCursor;
    if ( name == "pointer" ) cursor = Qt::PointingHandCursor;
    else if ( name == "text" ) cursor = Qt::IBeamCursor;
    else if ( name == "cross" ) cursor = Qt::CrossCursor;
    else if ( name == "wait" ) cursor = Qt::WaitCursor;
    else if ( name == "progress" ) cursor = Qt::BusyCursor;
    else if ( name == "move" || name.startsWith( "rmlui-scroll-" ) ) cursor = Qt::SizeAllCursor;
    else if ( name == "ew-resize" || name == "e-resize" || name == "w-resize" ) cursor = Qt::SizeHorCursor;
    else if ( name == "ns-resize" || name == "n-resize" || name == "s-resize" ) cursor = Qt::SizeVerCursor;
    else if ( name == "nesw-resize" || name == "ne-resize" || name == "sw-resize" ) cursor = Qt::SizeBDiagCursor;
    else if ( name == "nwse-resize" || name == "nw-resize" || name == "se-resize" ) cursor = Qt::SizeFDiagCursor;
    else if ( name == "unavailable" || name == "not-allowed" ) cursor = Qt::ForbiddenCursor;
    m_window->setCursor( QCursor( cursor ) );
}

void QtRmlSystemInterface::SetClipboardText( const Rml::String& text )
{
    if ( QGuiApplication::clipboard() ) QGuiApplication::clipboard()->setText( fromRml( text ) );
}

void QtRmlSystemInterface::GetClipboardText( Rml::String& text )
{
    text = QGuiApplication::clipboard() ? toRml( QGuiApplication::clipboard()->text() ) : Rml::String();
}

void QtRmlSystemInterface::ActivateKeyboard( Rml::Vector2f caretPosition, float lineHeight )
{
    if ( !m_window || !QGuiApplication::inputMethod() ) return;
    const qreal dpr = qMax<qreal>( 0.01, m_window->devicePixelRatio() );
    QGuiApplication::inputMethod()->setInputItemRectangle(
        QRectF( caretPosition.x / dpr, caretPosition.y / dpr, 1.0, lineHeight / dpr ) );
    QGuiApplication::inputMethod()->show();
}

void QtRmlSystemInterface::DeactivateKeyboard()
{
    if ( QGuiApplication::inputMethod() ) QGuiApplication::inputMethod()->hide();
}
} // namespace ingnomia::ui
