#include "RmlUiQtInputAdapter.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

#include <QGuiApplication>
#include <QInputMethod>

#include <algorithm>
#include <cmath>

namespace ingnomia::ui
{
RmlUiQtInputAdapter::RmlUiQtInputAdapter( Rml::Context* context ) : m_context( context ) {}

void RmlUiQtInputAdapter::setContext( Rml::Context* context )
{
    if ( m_context != context ) cancelInteraction();
    m_context = context;
}

Rml::Context* RmlUiQtInputAdapter::context() const noexcept { return m_context; }

PointerDispatch RmlUiQtInputAdapter::mouseMove( QPointF logicalPosition, qreal devicePixelRatio, Qt::KeyboardModifiers modifiers )
{
    if ( !m_context ) return {};
    const qreal dpr = std::max<qreal>( 0.01, devicePixelRatio );
    const bool worldMayHandle = m_context->ProcessMouseMove(
        std::lround( logicalPosition.x() * dpr ), std::lround( logicalPosition.y() * dpr ), keyModifiers( modifiers ) );
    return {worldMayHandle ? PointerOwner::World : PointerOwner::Ui, m_context->IsMouseInteracting()};
}

PointerDispatch RmlUiQtInputAdapter::mouseButtonDown( Qt::MouseButton button, Qt::KeyboardModifiers modifiers )
{
    const int index = buttonIndex( button );
    if ( !m_context || index < 0 ) return {};
    const bool worldMayHandle = m_context->ProcessMouseButtonDown( index, keyModifiers( modifiers ) );
    const PointerOwner owner = ( !worldMayHandle || m_context->IsMouseInteracting() ) ? PointerOwner::Ui : PointerOwner::World;
    m_pointerOwners[static_cast<size_t>( index )] = owner;
    return {owner, m_context->IsMouseInteracting()};
}

PointerDispatch RmlUiQtInputAdapter::mouseButtonUp( Qt::MouseButton button, Qt::KeyboardModifiers modifiers )
{
    const int index = buttonIndex( button );
    if ( !m_context || index < 0 ) return {};
    const PointerOwner owner = m_pointerOwners[static_cast<size_t>( index )];
    m_context->ProcessMouseButtonUp( index, keyModifiers( modifiers ) );
    m_pointerOwners[static_cast<size_t>( index )] = PointerOwner::None;
    return {owner, m_context->IsMouseInteracting()};
}

InputDispatch RmlUiQtInputAdapter::mouseWheel( QPoint angleDelta, QPoint pixelDelta, Qt::KeyboardModifiers modifiers )
{
    if ( !m_context ) return {};
    // Qt precision devices supply pixel deltas; 40 physical pixels per RmlUi
    // scroll unit matches one common three-line wheel step and is isolated here
    // for platform tuning without changing call-site semantics.
    const Rml::Vector2f delta = pixelDelta.isNull()
        ? Rml::Vector2f( -angleDelta.x() / 120.0f, -angleDelta.y() / 120.0f )
        : Rml::Vector2f( -pixelDelta.x() / 40.0f, -pixelDelta.y() / 40.0f );
    return {!m_context->ProcessMouseWheel( delta, keyModifiers( modifiers ) )};
}

InputDispatch RmlUiQtInputAdapter::keyDown( int qtKey, Qt::KeyboardModifiers modifiers )
{
    if ( !m_context ) return {};
    const Rml::Input::KeyIdentifier key = keyIdentifier( qtKey, modifiers );
    return {key != Rml::Input::KI_UNKNOWN && !m_context->ProcessKeyDown( key, keyModifiers( modifiers ) )};
}

InputDispatch RmlUiQtInputAdapter::keyUp( int qtKey, Qt::KeyboardModifiers modifiers )
{
    if ( !m_context ) return {};
    const Rml::Input::KeyIdentifier key = keyIdentifier( qtKey, modifiers );
    return {key != Rml::Input::KI_UNKNOWN && !m_context->ProcessKeyUp( key, keyModifiers( modifiers ) )};
}

InputDispatch RmlUiQtInputAdapter::committedText( const QString& text )
{
    if ( !m_context || text.isEmpty() ) return {};
    const QByteArray utf8 = text.toUtf8();
    return {!m_context->ProcessTextInput( Rml::String( utf8.constData(), static_cast<size_t>( utf8.size() ) ) )};
}

PointerOwner RmlUiQtInputAdapter::pointerOwner( Qt::MouseButton button ) const noexcept
{
    const int index = buttonIndex( button );
    return index >= 0 ? m_pointerOwners[static_cast<size_t>( index )] : PointerOwner::None;
}

void RmlUiQtInputAdapter::cancelInteraction()
{
    if ( m_context ) m_context->ProcessMouseLeave();
    m_pointerOwners.fill( PointerOwner::None );
    if ( QGuiApplication::inputMethod() ) QGuiApplication::inputMethod()->hide();
}

Rml::Input::KeyIdentifier RmlUiQtInputAdapter::keyIdentifier( int qtKey, Qt::KeyboardModifiers modifiers )
{
    using namespace Rml::Input;
    const bool keypad = modifiers.testFlag( Qt::KeypadModifier );
    if ( qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z )
        return static_cast<KeyIdentifier>( static_cast<int>( KI_A ) + qtKey - Qt::Key_A );
    if ( qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9 )
    {
        const KeyIdentifier base = keypad ? KI_NUMPAD0 : KI_0;
        return static_cast<KeyIdentifier>( static_cast<int>( base ) + qtKey - Qt::Key_0 );
    }
    if ( qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24 )
        return static_cast<KeyIdentifier>( static_cast<int>( KI_F1 ) + qtKey - Qt::Key_F1 );

    switch ( qtKey )
    {
        case Qt::Key_Space: return KI_SPACE;
        case Qt::Key_Backspace: return KI_BACK;
        case Qt::Key_Tab:
        case Qt::Key_Backtab: return KI_TAB;
        case Qt::Key_Clear: return KI_CLEAR;
        case Qt::Key_Return: return keypad ? KI_NUMPADENTER : KI_RETURN;
        case Qt::Key_Enter: return KI_NUMPADENTER;
        case Qt::Key_Pause: return KI_PAUSE;
        case Qt::Key_CapsLock: return KI_CAPITAL;
        case Qt::Key_Escape: return KI_ESCAPE;
        case Qt::Key_PageUp: return KI_PRIOR;
        case Qt::Key_PageDown: return KI_NEXT;
        case Qt::Key_End: return KI_END;
        case Qt::Key_Home: return KI_HOME;
        case Qt::Key_Left: return KI_LEFT;
        case Qt::Key_Up: return KI_UP;
        case Qt::Key_Right: return KI_RIGHT;
        case Qt::Key_Down: return KI_DOWN;
        case Qt::Key_Print: return KI_SNAPSHOT;
        case Qt::Key_Insert: return KI_INSERT;
        case Qt::Key_Delete: return KI_DELETE;
        case Qt::Key_Help: return KI_HELP;
        case Qt::Key_Meta: return KI_LMETA;
        case Qt::Key_Menu: return KI_APPS;
        case Qt::Key_NumLock: return KI_NUMLOCK;
        case Qt::Key_ScrollLock: return KI_SCROLL;
        case Qt::Key_Shift: return KI_LSHIFT;
        case Qt::Key_Control: return KI_LCONTROL;
        case Qt::Key_Alt: return KI_LMENU;
        case Qt::Key_Semicolon: return KI_OEM_1;
        case Qt::Key_Equal: return keypad ? KI_OEM_NEC_EQUAL : KI_OEM_PLUS;
        case Qt::Key_Comma: return KI_OEM_COMMA;
        case Qt::Key_Minus: return keypad ? KI_SUBTRACT : KI_OEM_MINUS;
        case Qt::Key_Period: return keypad ? KI_DECIMAL : KI_OEM_PERIOD;
        case Qt::Key_Slash: return keypad ? KI_DIVIDE : KI_OEM_2;
        case Qt::Key_QuoteLeft: return KI_OEM_3;
        case Qt::Key_BracketLeft: return KI_OEM_4;
        case Qt::Key_Backslash: return KI_OEM_5;
        case Qt::Key_BracketRight: return KI_OEM_6;
        case Qt::Key_Apostrophe: return KI_OEM_7;
        case Qt::Key_Asterisk: return keypad ? KI_MULTIPLY : KI_UNKNOWN;
        case Qt::Key_Plus: return keypad ? KI_ADD : KI_OEM_PLUS;
        case Qt::Key_VolumeMute: return KI_VOLUME_MUTE;
        case Qt::Key_VolumeDown: return KI_VOLUME_DOWN;
        case Qt::Key_VolumeUp: return KI_VOLUME_UP;
        case Qt::Key_MediaNext: return KI_MEDIA_NEXT_TRACK;
        case Qt::Key_MediaPrevious: return KI_MEDIA_PREV_TRACK;
        case Qt::Key_MediaStop: return KI_MEDIA_STOP;
        case Qt::Key_MediaTogglePlayPause: return KI_MEDIA_PLAY_PAUSE;
        default: return KI_UNKNOWN;
    }
}

int RmlUiQtInputAdapter::keyModifiers( Qt::KeyboardModifiers modifiers )
{
    int result = 0;
    if ( modifiers.testFlag( Qt::ControlModifier ) ) result |= Rml::Input::KM_CTRL;
    if ( modifiers.testFlag( Qt::ShiftModifier ) ) result |= Rml::Input::KM_SHIFT;
    if ( modifiers.testFlag( Qt::AltModifier ) ) result |= Rml::Input::KM_ALT;
    if ( modifiers.testFlag( Qt::MetaModifier ) ) result |= Rml::Input::KM_META;
    return result;
}

int RmlUiQtInputAdapter::buttonIndex( Qt::MouseButton button )
{
    switch ( button )
    {
        case Qt::LeftButton: return 0;
        case Qt::RightButton: return 1;
        case Qt::MiddleButton: return 2;
        case Qt::BackButton: return 3;
        case Qt::ForwardButton: return 4;
        default: return -1;
    }
}
} // namespace ingnomia::ui
