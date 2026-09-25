#pragma once

#include <RmlUi/Core/SystemInterface.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <Qt>

#include <functional>

class QWindow;

namespace ingnomia::ui
{
class QtRmlSystemInterface final : public Rml::SystemInterface
{
public:
    using Translator = std::function<QString( const QString& )>;

    explicit QtRmlSystemInterface( QWindow* window, Translator translator = {} );

    void setWindow( QWindow* window );
    /// Pointer shown where no RmlUi element is under the pointer in @p window (the map). An armed tool sets a
    /// cross here, so the mode shows only over the area where it applies (PDF p.356).
    void setMapCursor( QWindow* window, Qt::CursorShape shape );

    double GetElapsedTime() override;
    int TranslateString( Rml::String& translated, const Rml::String& input ) override;
    bool LogMessage( Rml::Log::Type type, const Rml::String& message ) override;
    void SetMouseCursor( const Rml::String& cursorName ) override;
    void SetClipboardText( const Rml::String& text ) override;
    void GetClipboardText( Rml::String& text ) override;
    void ActivateKeyboard( Rml::Vector2f caretPosition, float lineHeight ) override;
    void DeactivateKeyboard() override;

private:
    QPointer<QWindow> m_window;
    Translator m_translator;
    QElapsedTimer m_elapsed;
    QPointer<QWindow> m_mapWindow;
    Qt::CursorShape m_mapCursor = Qt::ArrowCursor;
    bool m_overMap = true;
};
} // namespace ingnomia::ui
