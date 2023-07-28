#pragma once

#include "OpenGL/OpenGL.h"
#include "OpenGL/View.h"
#include "Utility/Colour.h"

namespace slade
{
namespace gl
{
	class VertexBuffer2D;
}

class GLCanvas : public wxGLCanvas
{
public:
	enum class BGStyle
	{
		Colour,
		Checkered,
		System
	};

	GLCanvas(
		wxWindow*      parent,
		BGStyle        bg_style  = BGStyle::Colour,
		const ColRGBA& bg_colour = ColRGBA::BLACK,
		gl::View       view      = {});
	~GLCanvas();

	const gl::View& view() const { return view_; }
	gl::View&       view() { return view_; }
	ColRGBA         backgroundColour() const { return bg_colour_; }

	void setView(const gl::View& view) { view_ = view; }
	void setBackground(BGStyle style, ColRGBA colour)
	{
		bg_colour_ = colour;
		bg_style_  = style;
	}

	void setupMousewheelZoom();
	void setupMousePanning();

	bool activateContext();

protected:
	gl::View view_;

private:
	BGStyle                        bg_style_ = BGStyle::Colour;
	ColRGBA                        bg_colour_;
	unique_ptr<gl::VertexBuffer2D> vb_background_;
	bool                           init_done_ = false;

	// View panning
	Vec2i mouse_prev_;

	void         init();
	virtual void draw();
	void         updateBackgroundVB();
	void         drawCheckeredBackground();

	// Events
	void onPaint(wxPaintEvent& e);
};
} // namespace slade
