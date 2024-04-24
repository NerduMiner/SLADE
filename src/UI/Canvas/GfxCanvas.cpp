
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2024 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    GfxCanvas.cpp
// Description: GfxCanvas class. A canvas that displays an image and can take
//              offsets into account etc
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
#include "GfxCanvas.h"
#include "Graphics/Palette/Palette.h"
#include "Graphics/SImage/SImage.h"
#include "UI/WxUtils.h"
#include "Utility/MathStuff.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, gfx_arc)
EXTERN_CVAR(Bool, gfx_hilight_mouseover)
EXTERN_CVAR(Bool, gfx_show_border)
EXTERN_CVAR(Bool, hud_statusbar)
EXTERN_CVAR(Bool, hud_center)
EXTERN_CVAR(Bool, hud_wide)
EXTERN_CVAR(Bool, hud_bob)


// -----------------------------------------------------------------------------
//
// GfxCanvas Class Functions
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// GfxCanvas class constructor
// -----------------------------------------------------------------------------
GfxCanvas::GfxCanvas(wxWindow* parent) : wxPanel(parent)
{
	view_.setCentered(true);
	resetViewOffsets();

	SetDoubleBuffered(true);

	// Bind Events
	Bind(wxEVT_PAINT, &GfxCanvas::onPaint, this);
	Bind(wxEVT_LEFT_DOWN, &GfxCanvas::onMouseLeftDown, this);
	Bind(wxEVT_RIGHT_DOWN, &GfxCanvas::onMouseRightDown, this);
	Bind(wxEVT_LEFT_UP, &GfxCanvas::onMouseLeftUp, this);
	Bind(wxEVT_MOTION, &GfxCanvas::onMouseMovement, this);
	Bind(wxEVT_LEAVE_WINDOW, &GfxCanvas::onMouseLeaving, this);
	Bind(wxEVT_MOUSEWHEEL, &GfxCanvas::onMouseWheel, this);
	Bind(wxEVT_KEY_DOWN, &GfxCanvas::onKeyDown, this);

	// Update on resize
	Bind(
		wxEVT_SIZE,
		[this](wxSizeEvent&)
		{
			view_.setSize(GetSize().x, GetSize().y);
			Refresh();
		});

	// Update buffer when the image changes
	sc_image_changed_ = image_->signals().image_changed.connect([this]() { update_image_ = true; });
}

// -----------------------------------------------------------------------------
// GfxCanvas class destructor
// -----------------------------------------------------------------------------
GfxCanvas::~GfxCanvas() = default;

// -----------------------------------------------------------------------------
// Sets the canvas palette to [pal]
// -----------------------------------------------------------------------------
void GfxCanvas::setPalette(const Palette* pal)
{
	if (!palette_)
		palette_ = std::make_unique<Palette>(*pal);
	else
		palette_->copyPalette(pal);

	update_image_ = true;
}

// -----------------------------------------------------------------------------
// Creates a mask texture of the brush to preview its effect
// -----------------------------------------------------------------------------
void GfxCanvas::generateBrushShadow()
{
	if (brush_ == nullptr)
		return;

	// Generate image
	SImage img;
	generateBrushShadowImage(img);

	// Load it to the brush bitmap
	brush_bitmap_ = wxBitmap(wxutil::createImageFromSImage(img, palette_.get()));
}

// -----------------------------------------------------------------------------
// Draws the offset center/guide lines
// -----------------------------------------------------------------------------
void GfxCanvas::drawOffsetLines(wxGraphicsContext* gc)
{
	auto psize_thick  = 1.51 / view().scale().x;
	auto psize_normal = 1.0 / view().scale().x;

	if (view_type_ == View::Sprite)
	{
		gc->SetInterpolationQuality(wxINTERPOLATION_BEST);

		gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0, 190), psize_thick)));
		gc->StrokeLine(-999999, 0, 999999, 0);
		gc->StrokeLine(0, -999999, 0, 999999);
	}
	else if (view_type_ == View::HUD)
	{
		gc->SetInterpolationQuality(wxINTERPOLATION_BEST);

		// (320/354)x200 screen outline
		auto right  = hud_wide ? 337.0 : 320.0;
		auto left   = hud_wide ? -17.0 : 0.0;
		auto top    = 0;
		auto bottom = 200;
		gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0, 190), psize_thick)));
		gc->StrokeLine(left, top, left, bottom);
		gc->StrokeLine(left, bottom, right, bottom);
		gc->StrokeLine(right, bottom, right, top);
		gc->StrokeLine(right, top, left, top);

		// Statusbar line(s)
		gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0, 128), psize_normal)));
		if (hud_statusbar)
		{
			gc->StrokeLine(left, 168, right, 168); // Doom's status bar: 32 pixels tall
			gc->StrokeLine(left, 162, right, 162); // Hexen: 38 pixels
			gc->StrokeLine(left, 158, right, 158); // Heretic: 42 pixels
		}

		// Center lines
		if (hud_center)
		{
			gc->StrokeLine(left, 100, right, 100);
			gc->StrokeLine(160, top, 160, bottom);
		}

		// Normal screen edge guides if widescreen
		if (hud_wide)
		{
			gc->StrokeLine(0, top, 0, bottom);
			gc->StrokeLine(320, top, 320, bottom);
		}

		// Weapon bobbing guides
		if (hud_bob)
		{
			gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0, 128), psize_normal)));
			gc->StrokeLine(left - 16.0, top - 16.0, left - 16.0, bottom + 16.0);
			gc->StrokeLine(left - 16.0, bottom + 16.0, right + 16.0, bottom + 16.0);
			gc->StrokeLine(right + 16.0, bottom + 16.0, right + 16.0, top - 16.0);
			gc->StrokeLine(right + 16.0, top - 16.0, left - 16.0, top - 16.0);
		}
	}
}

// -----------------------------------------------------------------------------
// Draws the image (and offset drag preview if needed)
// -----------------------------------------------------------------------------
void GfxCanvas::drawImage(wxGraphicsContext* gc)
{
	auto dragging = drag_origin_.x > 0;
	auto hilight  = !dragging && image_hilight_ && gfx_hilight_mouseover && editing_mode_ == EditMode::None;

	// Load/update image if needed
	if (update_image_ || hilight != image_hilighted_)
	{
		// If the image change isn't caused by drawing, resize drawing mask
		if (!drawing_)
		{
			delete[] drawing_mask_;
			drawing_mask_ = new bool[image_->width() * image_->height()];
			memset(drawing_mask_, false, image_->width() * image_->height());
		}

		// Create wx image
		auto img = wxutil::createImageFromSImage(*image_, palette_.get());
		if (hilight)
			img.ChangeBrightness(0.25); // Hilight if needed

		// Create wx bitmap from image
		image_bitmap_ = wxBitmap(img);

		update_image_    = false;
		image_hilighted_ = hilight;
	}

	// Get top left coord to draw at
	Vec2i tl;
	if (view_type_ == View::Sprite || view_type_ == View::HUD)
	{
		// Apply offsets for sprite/hud view
		tl.x -= image().offset().x;
		tl.y -= image().offset().y;
	}

	// Draw image
	gc->SetInterpolationQuality(wxINTERPOLATION_NONE);
	if (dragging)
		gc->BeginLayer(0.5); // Semitransparent if dragging
	gc->DrawBitmap(image_bitmap_, tl.x, tl.y, image_bitmap_.GetWidth(), image_bitmap_.GetHeight());
	if (dragging)
		gc->EndLayer();

	// Draw brush shadow when in editing mode
	if (editing_mode_ != EditMode::None && brush_bitmap_.IsOk() && cursor_pos_ != Vec2i{ -1, -1 })
	{
		gc->BeginLayer(0.6);
		gc->DrawBitmap(brush_bitmap_, tl.x, tl.y, brush_bitmap_.GetWidth(), brush_bitmap_.GetHeight());
		gc->EndLayer();
	}

	// Draw dragging image
	if (dragging)
	{
		tl.x += math::scaleInverse(drag_pos_.x - drag_origin_.x, view().scale().x);
		tl.y += math::scaleInverse(drag_pos_.y - drag_origin_.y, view().scale().y);
		gc->DrawBitmap(image_bitmap_, tl.x, tl.y, image_bitmap_.GetWidth(), image_bitmap_.GetHeight());
	}

	// Draw outline
	if (gfx_show_border)
	{
		gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(0, 0, 0, 64), 1.0 / view().scale().x)));
		gc->SetBrush(*wxTRANSPARENT_BRUSH);
		gc->DrawRectangle(tl.x, tl.y, image_bitmap_.GetWidth(), image_bitmap_.GetHeight());
	}
}


// -----------------------------------------------------------------------------
//
// GfxCanvas Class Events
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Called when the canvas requires redrawing
// -----------------------------------------------------------------------------
void GfxCanvas::onPaint(wxPaintEvent& e)
{
	auto dc = wxPaintDC(this);

#ifdef WIN32
	// Use Direct2d on Windows instead of GDI+
	auto renderer = wxGraphicsRenderer::GetDirect2DRenderer();
	auto gc       = renderer->CreateContext(dc);
#else
	auto gc = wxGraphicsContext::Create(dc);
#endif

	// Background
	wxutil::generateCheckeredBackground(background_bitmap_, GetSize().x, GetSize().y);
	gc->DrawBitmap(background_bitmap_, 0, 0, background_bitmap_.GetWidth(), background_bitmap_.GetHeight());

	// Apply view to wxGraphicsContext
	if (view().centered())
		gc->Translate(GetSize().x * 0.5, GetSize().y * 0.5);
	gc->Scale(view().scale().x, gfx_arc ? view().scale().y * 1.2 : view().scale().y);
	gc->Translate(-view().offset().x, -view().offset().y);

	drawOffsetLines(gc);
	drawImage(gc);

	delete gc;
}
