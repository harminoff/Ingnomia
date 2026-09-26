#include "RmlUiQtInputAdapter.h"
#include "AccessKeys.h"
#include "ConnectedTabs.h"
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

#include <QGuiApplication>
#include <QInputMethod>

#include <algorithm>
#include <cmath>

namespace ingnomia::ui
{
RmlUiQtInputAdapter::RmlUiQtInputAdapter( Rml::Context* context ) { setContext(context); }
RmlUiQtInputAdapter::~RmlUiQtInputAdapter() { setContext(nullptr); }
void RmlUiQtInputAdapter::ProcessEvent(Rml::Event& event)
{
    if(event.GetId() == Rml::EventId::Blur)
    {
        for(auto& gesture : m_commands)
            if(gesture.focus == event.GetTargetElement())
            {
                if(gesture.target) gesture.target->SetClass("is-key-pressed", false);
                gesture.target.reset(); gesture.focus.reset();
            }
    }
    else if(event.GetId() == Rml::EventId::Click)
    {
        auto* tab = event.GetTargetElement();
        while(tab && tab->GetTagName() != "button") tab = tab->GetParentNode();
        if(auto* strip = connected_tabs::stripFor(tab); strip && connected_tabs::enabled(tab))
            connected_tabs::select(*strip, tab);
    }
    else if(event.GetId() == Rml::EventId::Keydown)
    {
        int modifiers = 0;
        if(event.GetParameter("ctrl_key", false)) modifiers |= Rml::Input::KM_CTRL;
        if(event.GetParameter("shift_key", false)) modifiers |= Rml::Input::KM_SHIFT;
        if(event.GetParameter("alt_key", false)) modifiers |= Rml::Input::KM_ALT;
        if(event.GetParameter("meta_key", false)) modifiers |= Rml::Input::KM_META;
        auto* focus=m_context->GetFocusElement();
        const int key=event.GetParameter<int>("key_identifier",0);
        if(focus && focus->GetTagName()=="input" && focus->GetAttribute<Rml::String>("type","")=="radio" && !modifiers &&
            (key==Rml::Input::KI_LEFT || key==Rml::Input::KI_RIGHT || key==Rml::Input::KI_UP || key==Rml::Input::KI_DOWN)) {
            Rml::ElementList choices;auto* group=focus->GetParentNode();
            while(group && group->GetTagName()!="form" && group!=focus->GetOwnerDocument())group=group->GetParentNode();
            if(group)group->GetElementsByTagName(choices,"input");
            const auto name=focus->GetAttribute<Rml::String>("name","");
            std::erase_if(choices,[&](auto* e){return e->GetAttribute<Rml::String>("type","")!="radio" || e->GetAttribute<Rml::String>("name","")!=name || !connected_tabs::enabled(e);});
            auto it=std::find(choices.begin(),choices.end(),focus);
            if(it!=choices.end()) {
                const int count=static_cast<int>(choices.size()),index=static_cast<int>(it-choices.begin());
                auto* next=choices[(index+((key==Rml::Input::KI_LEFT || key==Rml::Input::KI_UP)?count-1:1))%count];
                auto observer=next->GetObserverPtr();next->Click();if(observer)observer->Focus(true);
            }
            event.StopPropagation();return;
        }
        if(connected_tabs::key(*m_context, event.GetParameter("key_identifier", 0), modifiers)) event.StopPropagation();
    }
}

void RmlUiQtInputAdapter::setContext( Rml::Context* context )
{
    if(m_context == context) return;
    cancelInteraction();
    if(m_context) for(const char* event : {"click", "keydown", "blur"}) m_context->RemoveEventListener(event, this, true);
    m_context = context;
    if(m_context) for(const char* event : {"click", "keydown", "blur"}) m_context->AddEventListener(event, this, true);
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
    cancelCommands();
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

namespace
{
int commandIndex(int key) { return key == Qt::Key_Space ? 0 : key == Qt::Key_Return ? 1 : key == Qt::Key_Enter ? 2 : key == Qt::Key_Escape ? 3 : -1; }
Rml::Element* commandTarget(Rml::Context& context, int key)
{
    auto* focus = context.GetFocusElement();
    if(!focus || !focus->IsVisible(true)) return nullptr;
    if(key == Qt::Key_Escape) {
        auto* doc=focus->GetOwnerDocument();
        return doc && doc->HasAttribute("data-modal-dialog") ? doc->GetElementById("confirm-cancel") : nullptr;
    }
    if(focus->GetTagName() == "button") return focus;
    if(focus->GetTagName()=="input" && key==Qt::Key_Space) {
        auto type=focus->GetAttribute<Rml::String>("type","");
        if(type=="checkbox" || type=="radio") return focus;
    }
    if(focus->HasAttribute("data-numeric-editor")) return nullptr;
    if(key == Qt::Key_Space || focus->GetTagName() == "textarea" || focus->GetTagName() == "select") return nullptr;
    if(focus->GetTagName() == "input")
    {
        const auto type = focus->GetAttribute<Rml::String>("type", "text");
        if(type != "text" && type != "password") return nullptr;
    }
    auto* document = focus->GetOwnerDocument();
    if(!document) return nullptr;
    Rml::ElementList buttons; document->GetElementsByTagName(buttons, "button");
    for(auto* button : buttons)
        if(button->HasAttribute("data-default-action") && connected_tabs::enabled(button)) return button;
    return nullptr;
}
}
void RmlUiQtInputAdapter::cancelCommands()
{
    for(auto& gesture : m_commands)
    {
        if(gesture.target) gesture.target->SetClass("is-key-pressed", false);
        gesture.target.reset(); gesture.focus.reset();
    }
}
InputDispatch RmlUiQtInputAdapter::keyDown(int qtKey, Qt::KeyboardModifiers modifiers, bool autoRepeat)
{
    if(!m_context) return {};
    if(auto* focus=m_context->GetFocusElement();focus && focus->HasAttribute("readonly")) {
        const bool edit=qtKey==Qt::Key_Backspace || qtKey==Qt::Key_Delete || (qtKey==Qt::Key_Insert && (modifiers & Qt::ShiftModifier)) || ((modifiers & Qt::ControlModifier) && (qtKey==Qt::Key_X || qtKey==Qt::Key_V));
        if(edit) return {true,true};
    }
    if(auto* focus=m_context->GetFocusElement();autoRepeat && focus && focus->HasAttribute("data-numeric-editor") && (qtKey==Qt::Key_Return || qtKey==Qt::Key_Enter)) return {true,true};
    // Access keys: Alt+letter, or the plain letter when the focused control does not take text (Stage 20).
    if(qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z && !autoRepeat && !(modifiers & (Qt::ControlModifier | Qt::MetaModifier))
        && access_keys::activate(*m_context, static_cast<char>('a' + (qtKey - Qt::Key_A)), bool(modifiers & Qt::AltModifier)))
        return {true, true};
    const int index = commandIndex(qtKey);
    if(qtKey == Qt::Key_Escape) {
        cancelCommands();
        auto* focus=m_context->GetFocusElement();
        for(auto* node=focus;node;node=node->GetParentNode())
            if(auto* select=rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(node);select && select->IsSelectBoxVisible()) {
                auto observer=select->GetObserverPtr();select->CancelSelectBox();if(observer)observer->Focus(true);
                return {true,true};
            }
    }
    if(index >= 0 && m_commands[index].held) return {true, true};
    // Reports and matrices own Enter/Space semantics (for example Watch versus Open).
    // Dispatch to that owner once, instead of translating every row button into Click.
    if(index >= 0) if(auto* focus=m_context->GetFocusElement()) {
        const auto role=focus->GetAttribute<Rml::String>("role","");
        if(role=="row" || role=="gridcell") {
            m_commands[index].held=true;
            if(!autoRepeat && connected_tabs::enabled(focus)) m_context->ProcessKeyDown(keyIdentifier(qtKey,modifiers),keyModifiers(modifiers));
            return {true,true};
        }
    }
    if(index >= 0 && !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        if(auto* target = commandTarget(*m_context, qtKey))
        {
            auto& gesture = m_commands[index]; gesture.held = true;
            if(!autoRepeat && connected_tabs::enabled(target))
            {
                gesture.target = target->GetObserverPtr();
                gesture.focus = m_context->GetFocusElement()->GetObserverPtr();
                target->SetClass("is-key-pressed", true);
                if(index != 0) target->Click();
            }
            return {true, true};
        }
    }
    const auto key = keyIdentifier(qtKey, modifiers);
    return {key != Rml::Input::KI_UNKNOWN && !m_context->ProcessKeyDown(key, keyModifiers(modifiers))};
}
InputDispatch RmlUiQtInputAdapter::keyUp(int qtKey, Qt::KeyboardModifiers modifiers, bool autoRepeat)
{
    if(!m_context) return {};
    const int index = commandIndex(qtKey);
    if(index >= 0 && m_commands[index].held)
    {
        if(autoRepeat) return {true, true};
        auto gesture = std::move(m_commands[index]); m_commands[index] = {};
        if(gesture.target)
        {
            gesture.target->SetClass("is-key-pressed", false);
            if(index == 0 && gesture.focus == m_context->GetFocusElement() && connected_tabs::enabled(gesture.target.get())) gesture.target->Click();
        }
        return {true, true};
    }
    const auto key = keyIdentifier(qtKey, modifiers);
    return {key != Rml::Input::KI_UNKNOWN && !m_context->ProcessKeyUp(key, keyModifiers(modifiers))};
}

InputDispatch RmlUiQtInputAdapter::committedText( const QString& text )
{
    if ( !m_context || text.isEmpty() ) return {};
    if(auto* focus=m_context->GetFocusElement();focus && focus->HasAttribute("readonly")) return {true,true};
    // QKeyEvent::text() may contain the C0/C1 control code associated with an
    // editing key (Backspace is U+0008, Delete is U+007F). Those keys have
    // already been delivered through ProcessKeyDown; forwarding the control
    // code again through ProcessTextInput inserts a replacement-box glyph and
    // can undo normal text-edit behavior. Preserve printable UTF-16, including
    // surrogate pairs used by non-BMP characters, and reject controls here.
    QString printable;
    printable.reserve( text.size() );
    for ( const QChar character : text )
    {
        const auto code = character.unicode();
        if ( code < 0x20u || ( code >= 0x7fu && code <= 0x9fu ) ) continue;
        printable.append( character );
    }
    if ( printable.isEmpty() ) return {};
    const QByteArray utf8 = printable.toUtf8();
    return {!m_context->ProcessTextInput( Rml::String( utf8.constData(), static_cast<size_t>( utf8.size() ) ) )};
}

PointerOwner RmlUiQtInputAdapter::pointerOwner( Qt::MouseButton button ) const noexcept
{
    const int index = buttonIndex( button );
    return index >= 0 ? m_pointerOwners[static_cast<size_t>( index )] : PointerOwner::None;
}

void RmlUiQtInputAdapter::cancelInteraction()
{
    cancelCommands();
    for(auto& gesture : m_commands) gesture.held = false;
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
