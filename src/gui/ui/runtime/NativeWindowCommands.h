/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
// System window commands the window menu needs that Qt does not expose: the keyboard Move and Size modes (the pointer
// becomes the move or size pointer and the arrow keys move or size the window, PDF p.98-99), and recognizing
// Alt+Space, which opens the window menu (PDF p.113).
#include <QByteArray>

class QWindow;

namespace ingnomia::ui::native_window
{
/// Starts the system's keyboard move of the window. Returns false where the platform has no such mode.
bool beginKeyboardMove( QWindow& window );
/// Starts the system's keyboard sizing of the window. Returns false where the platform has no such mode.
bool beginKeyboardSize( QWindow& window );
/// Alt+Space opens the window menu: the key message opens it; the character and system-command messages that follow
/// are swallowed so the system does not open its own menu or beep.
enum class AltSpace { None, Open, Swallow };
AltSpace altSpace( const QByteArray& eventType, void* message );
} // namespace ingnomia::ui::native_window
