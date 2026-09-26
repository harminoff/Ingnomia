/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "NativeWindowCommands.h"

#include <QWindow>

#if defined( Q_OS_WIN )
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ingnomia::ui::native_window
{
namespace
{
bool systemCommand( [[maybe_unused]] QWindow& window, [[maybe_unused]] unsigned command )
{
#if defined( Q_OS_WIN )
	const auto hwnd = reinterpret_cast<HWND>( window.winId() );
	// Posted so the system's modal move or size loop starts after the menu that chose it has closed.
	return hwnd && PostMessageW( hwnd, WM_SYSCOMMAND, command, 0 ) != 0;
#else
	return false;
#endif
}
} // namespace

bool beginKeyboardMove( QWindow& window )
{
#if defined( Q_OS_WIN )
	return systemCommand( window, SC_MOVE );
#else
	return systemCommand( window, 0 );
#endif
}

bool beginKeyboardSize( QWindow& window )
{
#if defined( Q_OS_WIN )
	return systemCommand( window, SC_SIZE );
#else
	return systemCommand( window, 0 );
#endif
}

AltSpace altSpace( [[maybe_unused]] const QByteArray& eventType, [[maybe_unused]] void* message )
{
#if defined( Q_OS_WIN )
	if ( eventType != "windows_generic_MSG" || !message ) return AltSpace::None;
	const auto* msg = static_cast<const MSG*>( message );
	if ( msg->message == WM_SYSKEYDOWN && msg->wParam == VK_SPACE ) return AltSpace::Open;
	if ( msg->message == WM_SYSCHAR && msg->wParam == L' ' ) return AltSpace::Swallow;
	if ( msg->message == WM_SYSCOMMAND && ( msg->wParam & 0xFFF0 ) == SC_KEYMENU && msg->lParam == L' ' ) return AltSpace::Swallow;
#endif
	return AltSpace::None;
}
} // namespace ingnomia::ui::native_window
