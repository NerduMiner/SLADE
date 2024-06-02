
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2024 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    UI.cpp
// Description: Misc. UI-related stuff
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "UI.h"
#include "App.h"
#include "General/Console.h"
#include "UI/SplashWindow.h"
#include "Utility/StringUtils.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace slade::ui
{
unique_ptr<SplashWindow> splash_window;
bool                     splash_enabled = true;

// Pixel sizes/scale
int px_pad        = 8;
int px_pad_large  = 12;
int px_pad_xlarge = 16;
int px_pad_small  = 3;
int px_splitter   = 10;
int px_spin_width;

} // namespace slade::ui


// -----------------------------------------------------------------------------
//
// UI Namespace Functions
//
// -----------------------------------------------------------------------------
namespace slade::ui
{
// -----------------------------------------------------------------------------
// Returns true when called from the main (UI) thread
// -----------------------------------------------------------------------------
bool isMainThread()
{
	return app::mainThreadId() == std::this_thread::get_id();
}
} // namespace slade::ui

// -----------------------------------------------------------------------------
// Initialises UI metric values based on [scale]
// -----------------------------------------------------------------------------
void ui::init()
{
	splash_window = std::make_unique<SplashWindow>();

	if (app::platform() == app::Platform::Linux)
		px_spin_width = -1;
	else
		px_spin_width = 64;

	splash_window->init();
}

// -----------------------------------------------------------------------------
// Enables or disables the splash window depending on [enable]
// -----------------------------------------------------------------------------
void ui::enableSplash(bool enable)
{
	splash_enabled = enable;
}

// -----------------------------------------------------------------------------
// Shows the splash window with [message].
// If [progress] is true, the progress bar is displayed
// -----------------------------------------------------------------------------
void ui::showSplash(string_view message, bool progress, wxWindow* parent)
{
	if (!splash_enabled || !isMainThread())
		return;

	if (!splash_window)
	{
		splash_window = std::make_unique<SplashWindow>();
		splash_window->init();
	}

	splash_window->show(wxString{ message.data(), message.size() }, progress, parent);
}

// -----------------------------------------------------------------------------
// Hides the splash window
// -----------------------------------------------------------------------------
void ui::hideSplash()
{
	if (splash_window && isMainThread())
	{
		splash_window->hide();
		splash_window.reset();
	}
}

// -----------------------------------------------------------------------------
// Redraws the splash window
// -----------------------------------------------------------------------------
void ui::updateSplash()
{
	if (splash_window && isMainThread())
		splash_window->forceRedraw();
}

// -----------------------------------------------------------------------------
// Returns the current splash window progress
// -----------------------------------------------------------------------------
float ui::getSplashProgress()
{
	return splash_window ? splash_window->progress() : 0.0f;
}

// -----------------------------------------------------------------------------
// Sets the splash window [message]
// -----------------------------------------------------------------------------
void ui::setSplashMessage(string_view message)
{
	if (splash_window && isMainThread())
		splash_window->setMessage(wxString{ message.data(), message.size() });
}

// -----------------------------------------------------------------------------
// Sets the splash window progress bar [message]
// -----------------------------------------------------------------------------
void ui::setSplashProgressMessage(string_view message)
{
	if (splash_window && isMainThread())
		splash_window->setProgressMessage(wxString{ message.data(), message.size() });
}

// -----------------------------------------------------------------------------
// Sets the splash window [progress]
// -----------------------------------------------------------------------------
void ui::setSplashProgress(float progress)
{
	if (splash_window && isMainThread())
		splash_window->setProgress(progress);
}

// -----------------------------------------------------------------------------
// Sets the splash window progress to [current] out of [max]
// -----------------------------------------------------------------------------
void ui::setSplashProgress(unsigned current, unsigned max)
{
	setSplashProgress(static_cast<float>(current) / static_cast<float>(max));
}

// -----------------------------------------------------------------------------
// Sets the mouse cursor for [window]
// -----------------------------------------------------------------------------
void ui::setCursor(wxWindow* window, MouseCursor cursor)
{
	switch (cursor)
	{
	case MouseCursor::Hand:     window->SetCursor(wxCursor(wxCURSOR_HAND)); break;
	case MouseCursor::Move:     window->SetCursor(wxCursor(wxCURSOR_SIZING)); break;
	case MouseCursor::Cross:    window->SetCursor(wxCursor(wxCURSOR_CROSS)); break;
	case MouseCursor::SizeNS:   window->SetCursor(wxCursor(wxCURSOR_SIZENS)); break;
	case MouseCursor::SizeWE:   window->SetCursor(wxCursor(wxCURSOR_SIZEWE)); break;
	case MouseCursor::SizeNESW: window->SetCursor(wxCursor(wxCURSOR_SIZENESW)); break;
	case MouseCursor::SizeNWSE: window->SetCursor(wxCursor(wxCURSOR_SIZENWSE)); break;
	default:                    window->SetCursor(wxNullCursor);
	}
}

// -----------------------------------------------------------------------------
// Returns a UI metric size (eg. padding) in DPI-independent pixels.
// Use this for UI sizes like padding, spin control widths etc. to keep things
// consistent.
// If [window] is given, the size is converted to wxWidgets logical pixels based
// on the window's DPI
// -----------------------------------------------------------------------------
int ui::sizePx(Size size, const wxWindow* window)
{
	switch (size)
	{
	case Size::PadSmall:      return window ? window->FromDIP(px_pad_small) : px_pad_small;
	case Size::Pad:           return window ? window->FromDIP(px_pad) : px_pad;
	case Size::PadLarge:      return window ? window->FromDIP(px_pad_large) : px_pad_large;
	case Size::PadXLarge:     return window ? window->FromDIP(px_pad_xlarge) : px_pad_xlarge;
	case Size::Splitter:      return window ? window->FromDIP(px_splitter) : px_splitter;
	case Size::SpinCtrlWidth: return window ? window->FromDIP(px_spin_width) : px_spin_width;
	}

	return 0;
}

// -----------------------------------------------------------------------------
// Returns the standard padding size in DPI-independent pixels.
// If [window] is given, the size is converted to wxWidgets logical pixels based
// on the window's DPI
// -----------------------------------------------------------------------------
int ui::pad(const wxWindow* window)
{
	return window ? window->FromDIP(px_pad) : px_pad;
}

// -----------------------------------------------------------------------------
// Returns the standard large padding size in DPI-independent pixels.
// If [window] is given, the size is converted to wxWidgets logical pixels based
// on the window's DPI
// -----------------------------------------------------------------------------
int ui::padLarge(const wxWindow* window)
{
	return window ? window->FromDIP(px_pad_large) : px_pad_large;
}

// -----------------------------------------------------------------------------
// Returns the standard extra-large padding size in DPI-independent pixels.
// If [window] is given, the size is converted to wxWidgets logical pixels based
// on the window's DPI
// -----------------------------------------------------------------------------
int ui::padXLarge(const wxWindow* window)
{
	return window ? window->FromDIP(px_pad_xlarge) : px_pad_xlarge;
}

// -----------------------------------------------------------------------------
// Returns the standard small padding size in DPI-independent pixels.
// If [window] is given, the size is converted to wxWidgets logical pixels based
// on the window's DPI
// -----------------------------------------------------------------------------
int ui::padSmall(const wxWindow* window)
{
	return window ? window->FromDIP(px_pad_small) : px_pad_small;
}


// -----------------------------------------------------------------------------
//
// Console Commands
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Shows the splash screen with the given message, or hides it if no message is
// given
// -----------------------------------------------------------------------------
CONSOLE_COMMAND(splash, 0, false)
{
	if (args.empty())
		ui::hideSplash();
	else if (args.size() == 1)
		ui::showSplash(args[0]);
	else
	{
		ui::showSplash(args[0], true);
		float prog = strutil::asFloat(args[1]);
		ui::setSplashProgress(prog);
		ui::setSplashProgressMessage(fmt::format("Progress {}", args[1]));
	}
}