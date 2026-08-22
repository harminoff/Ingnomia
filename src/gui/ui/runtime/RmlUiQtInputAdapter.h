#pragma once

#include <RmlUi/Core/Input.h>

#include <QPoint>
#include <QString>
#include <Qt>

#include <array>

namespace Rml { class Context; }

namespace ingnomia::ui
{
enum class PointerOwner : unsigned char { None, Ui, World };

struct PointerDispatch
{
    PointerOwner owner = PointerOwner::None;
    bool uiInteracting = false;
};

struct InputDispatch
{
    bool uiConsumed = false;
};

/// Normalizes RmlUi's inverted raw return values and retains gesture ownership
/// from button-down through button-up. The host remains authoritative for any
/// world action permitted by these positive-semantic results.
class RmlUiQtInputAdapter final
{
public:
    explicit RmlUiQtInputAdapter( Rml::Context* context = nullptr );

    void setContext( Rml::Context* context );
    Rml::Context* context() const noexcept;

    PointerDispatch mouseMove( QPointF logicalPosition, qreal devicePixelRatio, Qt::KeyboardModifiers modifiers );
    PointerDispatch mouseButtonDown( Qt::MouseButton button, Qt::KeyboardModifiers modifiers );
    PointerDispatch mouseButtonUp( Qt::MouseButton button, Qt::KeyboardModifiers modifiers );
    InputDispatch mouseWheel( QPoint angleDelta, QPoint pixelDelta, Qt::KeyboardModifiers modifiers );
    InputDispatch keyDown( int qtKey, Qt::KeyboardModifiers modifiers );
    InputDispatch keyUp( int qtKey, Qt::KeyboardModifiers modifiers );
    InputDispatch committedText( const QString& text );

    PointerOwner pointerOwner( Qt::MouseButton button ) const noexcept;
    void cancelInteraction();

    static Rml::Input::KeyIdentifier keyIdentifier( int qtKey, Qt::KeyboardModifiers modifiers = {} );
    static int keyModifiers( Qt::KeyboardModifiers modifiers );
    static int buttonIndex( Qt::MouseButton button );

private:
    Rml::Context* m_context = nullptr;
    std::array<PointerOwner, 5> m_pointerOwners = {};
};
} // namespace ingnomia::ui
