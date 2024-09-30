
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2024 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    View.cpp
// Description: View class - handles a 2d opengl 'view' that can be scrolled and
//              zoomed, with conversion between screen <-> canvas coordinates
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
#include "View.h"
#include "Geometry/BBox.h"
#include "Shader.h"
#include "Utility/MathStuff.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace slade;
using namespace gl;


// -----------------------------------------------------------------------------
//
// View Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns a generated projection matrix for this view with/without [y_flipped]
// -----------------------------------------------------------------------------
glm::mat4 View::projectionMatrix(bool y_flipped) const
{
	return y_flipped ? glm::ortho(0.0f, static_cast<float>(size_.x), 0.0f, static_cast<float>(size_.y), -1.0f, 1.0f)
					 : glm::ortho(0.0f, static_cast<float>(size_.x), static_cast<float>(size_.y), 0.0f, -1.0f, 1.0f);
}

// -----------------------------------------------------------------------------
// Sets the [scale], ensuring that [focus_point] (in screen coords) stays at the
// same relative screen<->canvas position
// -----------------------------------------------------------------------------
void View::setScale(const Vec2d& scale, const Vec2i& focus_point)
{
	// Get current [point] in canvas coords before scaling
	auto orig_point = canvasPosUninterpolated(focus_point);

	// Update scale
	scale_ = scale;

	// Check for zoom limits
	if (scale_.x < min_scale_)
		scale_.x = min_scale_;
	if (scale_.x > max_scale_)
		scale_.x = max_scale_;
	if (scale_.y < min_scale_)
		scale_.y = min_scale_;
	if (scale_.y > max_scale_)
		scale_.y = max_scale_;

	// Update offset so that [focus_point] stays at the same relative screen/canvas position
	offset_.x += orig_point.x - canvasXUninterpolated(focus_point.x);
	offset_.y += orig_point.y - canvasYUninterpolated(focus_point.y);

	if (!interpolated_)
	{
		offset_inter_ = offset_;
		scale_inter_  = scale_;
	}

	// Update screen limits
	updateVisibleRegion();
	updateMatrices();
}

// -----------------------------------------------------------------------------
// Resets the interpolated view values to their non-interpolated counterparts
// -----------------------------------------------------------------------------
void View::resetInter(bool x, bool y, bool scale)
{
	if (x)
		offset_inter_.x = offset_.x;

	if (y)
		offset_inter_.y = offset_.y;

	if (scale)
		scale_inter_ = scale_;

	updateMatrices();
}

// -----------------------------------------------------------------------------
// Pans the view by [x,y]
// -----------------------------------------------------------------------------
void View::pan(double x, double y)
{
	offset_.x += x;
	offset_.y += y;

	if (!interpolated_)
		offset_inter_ = offset_;

	updateVisibleRegion();
	updateMatrices();
}

// -----------------------------------------------------------------------------
// Zooms the view by [amount] towards the center of the view
// -----------------------------------------------------------------------------
void View::zoom(double amount)
{
	// Zoom view
	scale_ = scale_ * amount;

	// Check for zoom limits
	if (scale_.x < min_scale_)
		scale_.x = min_scale_;
	if (scale_.x > max_scale_)
		scale_.x = max_scale_;
	if (scale_.y < min_scale_)
		scale_.y = min_scale_;
	if (scale_.y > max_scale_)
		scale_.y = max_scale_;

	if (!interpolated_)
		scale_inter_ = scale_;

	// Update screen limits
	updateVisibleRegion();
	updateMatrices();
}

// -----------------------------------------------------------------------------
// Zooms the view by [amount] towards [point] (in screen coords)
// -----------------------------------------------------------------------------
void View::zoomToward(double amount, const Vec2i& point)
{
	setScale(scale_.x * amount, point);
}

// -----------------------------------------------------------------------------
// Zooms and offsets the view such that [bbox] fits within the current view size
// -----------------------------------------------------------------------------
void View::fitTo(const BBox& bbox, double scale_inc)
{
	// Reset zoom and set offsets to the middle of the canvas
	scale_    = { 2, 2 };
	offset_.x = bbox.min.x + ((bbox.max.x - bbox.min.x) * 0.5);
	offset_.y = bbox.min.y + ((bbox.max.y - bbox.min.y) * 0.5);

	// Now just keep zooming out until we fit the whole canvas in the view
	bool done = false;
	while (!done)
	{
		// Update screen limits
		updateVisibleRegion();

		if (bbox.min.x >= visibleRegion().tl.x && bbox.max.x <= visibleRegion().br.x
			&& bbox.min.y >= visibleRegion().tl.y && bbox.max.y <= visibleRegion().br.y)
			done = true;
		else
			scale_ = scale_ * 1.0 / scale_inc;
	}

	if (!interpolated_)
	{
		offset_inter_ = offset_;
		scale_inter_  = scale_;
	}

	updateMatrices();
}

// -----------------------------------------------------------------------------
// Updates the interpolated view values based on [mult].
// If [towards] is not nullptr, the scale interpolation will also interpolate
// offsets towards [towards]
// -----------------------------------------------------------------------------
bool View::interpolate(double mult, const Vec2d* towards)
{
	bool interpolating = false;

	// Scale
	auto diff_scale = scale_ - scale_inter_;
	if (diff_scale.x < -0.0000001 || diff_scale.x > 0.0000001)
	{
		// Get current mouse position in canvas coordinates (for zdooming towards [towards])
		double mx{}, my{};
		if (towards)
		{
			mx = canvasX(towards->x);
			my = canvasY(towards->y);
		}

		// Interpolate zoom
		scale_inter_ = scale_inter_ + diff_scale * mult;

		// Check for zoom finish
		if ((diff_scale.x < 0 && scale_inter_.x < scale_.x) || (diff_scale.x > 0 && scale_inter_.x > scale_.x))
			scale_inter_ = scale_;
		else
			interpolating = true;

		if (towards)
		{
			setOffset(offset_inter_.x + mx - canvasX(towards->x), offset_inter_.y + my - canvasY(towards->y));
			offset_inter_ = offset_;
		}
	}
	else
		scale_inter_ = scale_;

	// X offset
	double diff_xoff = offset_.x - offset_inter_.x;
	if (diff_xoff < -0.05 || diff_xoff > 0.05)
	{
		// Interpolate offset
		offset_inter_.x += diff_xoff * mult;

		// Check stuff
		if ((diff_xoff < 0 && offset_inter_.x < offset_.x) || (diff_xoff > 0 && offset_inter_.x > offset_.x))
			offset_inter_.x = offset_.x;
		else
			interpolating = true;
	}
	else
		offset_inter_.x = offset_.x;

	// Y offset
	double diff_yoff = offset_.y - offset_inter_.y;
	if (diff_yoff < -0.05 || diff_yoff > 0.05)
	{
		// Interpolate offset
		offset_inter_.y += diff_yoff * mult;

		if ((diff_yoff < 0 && offset_inter_.y < offset_.y) || (diff_yoff > 0 && offset_inter_.y > offset_.y))
			offset_inter_.y = offset_.y;
		else
			interpolating = true;
	}
	else
		offset_inter_.y = offset_.y;

	updateMatrices();

	return interpolating;
}

// -----------------------------------------------------------------------------
// Translates an x position on the screen to the corresponding x position on the
// canvas itself
// -----------------------------------------------------------------------------
double View::canvasX(int screen_x) const
{
	return centered_ ? screen_x / scale_inter_.x + offset_inter_.x - size_.x * 0.5 / scale_inter_.x
					 : screen_x / scale_inter_.x + offset_inter_.x;
}

// -----------------------------------------------------------------------------
// Translates a y position on the screen to the corresponding y position on the
// canvas itself
// -----------------------------------------------------------------------------
double View::canvasY(int screen_y) const
{
	if (y_flipped_)
		screen_y = size_.y - screen_y;

	return centered_ ? screen_y / scale_inter_.y + offset_inter_.y - size_.y * 0.5 / scale_inter_.y
					 : screen_y / scale_inter_.y + offset_inter_.y;
}

// -----------------------------------------------------------------------------
// Translates [x] from canvas coordinates to screen coordinates
// -----------------------------------------------------------------------------
int View::screenX(double canvas_x) const
{
	return centered_ ? math::round((size_.x * 0.5) + ((canvas_x - offset_inter_.x) * scale_inter_.x))
					 : math::round((canvas_x - offset_inter_.x) * scale_inter_.x);
}

// -----------------------------------------------------------------------------
// Translates [y] from canvas coordinates to screen coordinates
// -----------------------------------------------------------------------------
int View::screenY(double canvas_y) const
{
	auto y = centered_ ? math::round((size_.y * 0.5) + ((canvas_y - offset_inter_.y) * scale_inter_.y))
					   : math::round((canvas_y - offset_inter_.y) * scale_inter_.y);

	return y_flipped_ ? size_.y - y : y;
}

void View::setupShader(const Shader& shader, const glm::mat4& model) const
{
	shader.bind();
	shader.setUniform("mvp", projection_matrix_ * view_matrix_ * model);
	shader.setUniform("viewport_size", glm::vec2(size_.x, size_.y));
}

// -----------------------------------------------------------------------------
// Updates the canvas bounds member variable for the current view
// -----------------------------------------------------------------------------
void View::updateVisibleRegion()
{
	if (y_flipped_)
	{
		visible_region_.tl.x = canvasXUninterpolated(0);
		visible_region_.tl.y = canvasYUninterpolated(size_.y);
		visible_region_.br.x = canvasXUninterpolated(size_.x);
		visible_region_.br.y = canvasYUninterpolated(0);
	}
	else
	{
		visible_region_.tl.x = canvasXUninterpolated(0);
		visible_region_.tl.y = canvasYUninterpolated(0);
		visible_region_.br.x = canvasXUninterpolated(size_.x);
		visible_region_.br.y = canvasYUninterpolated(size_.y);
	}
}

void View::updateMatrices()
{
	auto size_x = static_cast<float>(size_.x);
	auto size_y = static_cast<float>(size_.y);

	// Projection --------------------------------------------------------------
	if (y_flipped_)
		projection_matrix_ = glm::ortho(0.0f, size_x, 0.0f, size_y, -1.0f, 1.0f);
	else
		projection_matrix_ = glm::ortho(0.0f, size_x, size_y, 0.0f, -1.0f, 1.0f);


	// View --------------------------------------------------------------------
	view_matrix_ = glm::mat4{ 1.0f };

	// Translate to middle of screen if centered
	if (centered_)
		view_matrix_ = glm::translate(view_matrix_, { size_.x * 0.5f, size_.y * 0.5f, 0.f });

	// Zoom
	view_matrix_ = glm::scale(view_matrix_, { scale_inter_.x, scale_inter_.y, 1. });

	// Translate to offsets
	view_matrix_ = glm::translate(view_matrix_, { -offset_inter_.x, -offset_inter_.y, 0. });
}

double View::canvasXUninterpolated(int screen_x) const
{
	return centered_ ? screen_x / scale_.x + offset_.x - size_.x * 0.5 / scale_.x : screen_x / scale_.x + offset_.x;
}

double View::canvasYUninterpolated(int screen_y) const
{
	if (y_flipped_)
		screen_y = size_.y - screen_y;

	return centered_ ? screen_y / scale_.y + offset_.y - size_.y * 0.5 / scale_.y : screen_y / scale_.y + offset_.y;
}

Vec2d View::canvasPosUninterpolated(const Vec2i& screen_pos) const
{
	return { canvasXUninterpolated(screen_pos.x), canvasYUninterpolated(screen_pos.y) };
}
