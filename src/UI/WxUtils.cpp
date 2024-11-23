
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2024 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    WxUtils.cpp
// Description: WxWidgets-related utility functions
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

#include "App.h"
#include "Graphics/Icons.h"
#include "UI/UI.h"
#include "UI/WxUtils.h"
#include "Utility/Colour.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(String, font_monospace, "Consolas,Lucida Console", CVar::Flag::Save)
CVAR(Int, tab_style, 1, CVar::Flag::Save)


// -----------------------------------------------------------------------------
//
// WxUtils Namespace Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Creates a wxMenuItem from the given parameters, including giving it an icon
// from slade.pk3 if specified
// -----------------------------------------------------------------------------
wxMenuItem* wxutil::createMenuItem(
	wxMenu*         menu,
	int             id,
	const wxString& label,
	const wxString& help,
	const wxString& icon)
{
	auto item = new wxMenuItem(menu, id, label, help);

	if (!icon.IsEmpty())
		item->SetBitmap(icons::getIcon(icons::Any, icon.ToStdString()));

	return item;
}

// -----------------------------------------------------------------------------
// Returns a copy of the font [base] with a monospace face/family
// -----------------------------------------------------------------------------
wxFont wxutil::monospaceFont(wxFont base)
{
	wxFont font(base.GetPointSize(), wxFONTFAMILY_MODERN, base.GetStyle(), base.GetWeight());

	auto split = wxSplit(font_monospace, ',');
	for (const auto& name : split)
	{
		if (base.SetFaceName(name))
		{
			font.SetFaceName(name);
			break;
		}
	}

	return font;
}

// -----------------------------------------------------------------------------
// Creates an image list with a DPI-scaled 'small' image size (for lists, etc.)
// -----------------------------------------------------------------------------
wxImageList* wxutil::createSmallImageList()
{
	return new wxImageList(16, 16, false, 0);
}

// -----------------------------------------------------------------------------
// Adds [icon] of [icon_type] to the given image [list]
// -----------------------------------------------------------------------------
int wxutil::addImageListIcon(wxImageList* list, int icon_type, string_view icon)
{
	return list->Add(icons::getIcon(static_cast<icons::Type>(icon_type), icon).GetBitmap(list->GetSize()));
}

// -----------------------------------------------------------------------------
// Creates a wxPanel and places [control] on it, with [pad] padding around it
// -----------------------------------------------------------------------------
wxPanel* wxutil::createPadPanel(wxWindow* parent, wxWindow* control, int pad)
{
	if (pad < 0)
		pad = ui::pad(parent);

	auto panel = new wxPanel(parent);
	panel->SetSizer(new wxBoxSizer(wxVERTICAL));
	control->Reparent(panel);
	panel->GetSizer()->Add(control, 1, wxEXPAND | wxALL, parent->FromDIP(pad));

	return panel;
}

// -----------------------------------------------------------------------------
// Creates a wxSpinCtrl using the SLADE sizing for spin controls
// -----------------------------------------------------------------------------
wxSpinCtrl* wxutil::createSpinCtrl(wxWindow* parent, int value, int min, int max)
{
	return new wxSpinCtrl(
		parent,
		-1,
		"",
		wxDefaultPosition,
		parent->FromDIP(wxSize{ ui::sizePx(ui::Size::SpinCtrlWidth), -1 }),
		wxSP_ARROW_KEYS,
		min,
		max,
		value);
}

// -----------------------------------------------------------------------------
// Creates a simple horizontal box sizer with a [label] on the left and
// [widget] on the right
// -----------------------------------------------------------------------------
wxSizer* wxutil::createLabelHBox(wxWindow* parent, const wxString& label, wxWindow* widget)
{
	auto hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(new wxStaticText(parent, -1, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, parent->FromDIP(ui::pad()));
	hbox->Add(widget, 1, wxEXPAND);
	return hbox;
}

wxSizer* wxutil::createLabelHBox(wxWindow* parent, const wxString& label, wxSizer* sizer)
{
	auto hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(new wxStaticText(parent, -1, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, parent->FromDIP(ui::pad()));
	hbox->Add(sizer, 1, wxEXPAND);
	return hbox;
}

// -----------------------------------------------------------------------------
// Creates a simple vertical box sizer with a [label] on the top and [widget]
// on the bottom
// -----------------------------------------------------------------------------
wxSizer* ::wxutil::createLabelVBox(wxWindow* parent, const wxString& label, wxWindow* widget)
{
	auto vbox = new wxBoxSizer(wxVERTICAL);
	vbox->Add(new wxStaticText(parent, -1, label), 0, wxBOTTOM, parent->FromDIP(ui::padSmall()));
	vbox->Add(widget, 1, wxEXPAND);
	return vbox;
}

wxSizer* wxutil::createLabelVBox(wxWindow* parent, const wxString& label, wxSizer* sizer)
{
	auto vbox = new wxBoxSizer(wxVERTICAL);
	vbox->Add(new wxStaticText(parent, -1, label), 0, wxBOTTOM, parent->FromDIP(ui::padSmall()));
	vbox->Add(sizer, 1, wxEXPAND);
	return vbox;
}

// -----------------------------------------------------------------------------
// Creates a horizontal sizer with the given ok/cancel buttons on the right,
// ordered in the standard order for the platform:
// Windows: [btn_ok] [btn_cancel]
// Others:  [btn_cancel] [btn_ok]
// -----------------------------------------------------------------------------
wxSizer* wxutil::createDialogButtonBox(wxButton* btn_ok, wxButton* btn_cancel)
{
	auto* hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->AddStretchSpacer(1);

#ifdef __WXMSW__
	hbox->Add(btn_ok, 0, wxEXPAND | wxRIGHT, ui::pad(btn_ok));
	hbox->Add(btn_cancel, 0, wxEXPAND);
#else
	hbox->Add(btn_cancel, 0, wxEXPAND | wxRIGHT, ui::pad(btn_ok));
	hbox->Add(btn_ok, 0, wxEXPAND);
#endif

	return hbox;
}

// -----------------------------------------------------------------------------
// Shortcut function for createDialogButtonBox that creates ok/cancel buttons
// with the given [text_ok] and [text_cancel]
// -----------------------------------------------------------------------------
wxSizer* slade::wxutil::createDialogButtonBox(wxWindow* parent, const wxString& text_ok, const wxString& text_cancel)
{
	auto* btn_ok = new wxButton(parent, wxID_OK, text_ok);
	btn_ok->SetDefault();
	auto* btn_cancel = new wxButton(parent, wxID_CANCEL, text_cancel);
	return createDialogButtonBox(btn_ok, btn_cancel);
}

// -----------------------------------------------------------------------------
// Creates a section separator (a label on the left and a line on the right)
// with the given [text]
// -----------------------------------------------------------------------------
wxSizer* wxutil::createSectionSeparator(wxWindow* parent, const wxString& text)
{
	auto hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(new wxStaticText(parent, -1, text), wxSizerFlags().CenterVertical().Border(wxRIGHT, ui::pad(parent)));
	hbox->Add(new wxStaticLine(parent), wxSizerFlags(1).CenterVertical());
	return hbox;
}

// -----------------------------------------------------------------------------
// Returns a wxArrayString containing the (wx) strings in [vector]
// -----------------------------------------------------------------------------
wxArrayString wxutil::arrayString(const vector<wxString>& vector)
{
	return wxArrayString{ vector.size(), vector.data() };
}

// -----------------------------------------------------------------------------
// Returns a wxArrayString containing the (std) strings in [vector]
// -----------------------------------------------------------------------------
wxArrayString wxutil::arrayStringStd(const vector<string>& vector)
{
	wxArrayString list;
	for (const auto& str : vector)
		list.Add(str);
	return list;
}

// -----------------------------------------------------------------------------
// Sets the given [window]'s [icon]
// -----------------------------------------------------------------------------
void wxutil::setWindowIcon(wxTopLevelWindow* window, string_view icon)
{
	auto wx_icon = icons::getIcon(icons::Any, icon).GetIconFor(window);
	window->SetIcon(wx_icon);
}


// The following functions are taken from CodeLite (http://codelite.org)

wxColour wxutil::systemPanelBGColour()
{
	if (app::platform() == app::Windows && app::isDarkTheme())
		return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	else
		return wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
}

wxColour wxutil::systemMenuTextColour()
{
	return wxSystemSettings::GetColour(wxSYS_COLOUR_MENUTEXT);
}

wxColour wxutil::systemMenuBarBGColour()
{
	return wxSystemSettings::GetColour(wxSYS_COLOUR_MENU);
}

wxColour wxutil::lightColour(const wxColour& colour, float percent)
{
	if (percent == 0)
	{
		return colour;
	}

	// Convert to HSL
	ColHSL hsl = colour::rgbToHsl(ColRGBA(colour));

	// Increase luminance
	hsl.l += static_cast<float>((percent * 5.0) / 100.0);
	if (hsl.l > 1.0)
		hsl.l = 1.0;

	ColRGBA rgb = hsl.asRGB();
	return { rgb.r, rgb.g, rgb.b };
}

wxColour wxutil::darkColour(const wxColour& colour, float percent)
{
	if (percent == 0)
	{
		return colour;
	}

	// Convert to HSL
	ColHSL hsl = colour::rgbToHsl(ColRGBA(colour));

	// Decrease luminance
	hsl.l -= static_cast<float>((percent * 5.0) / 100.0);
	if (hsl.l < 0)
		hsl.l = 0;

	ColRGBA rgb = hsl.asRGB();
	return { rgb.r, rgb.g, rgb.b };
}
