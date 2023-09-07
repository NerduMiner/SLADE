
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2022 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapRenderer2D.cpp
// Description: MapRenderer2D class - handles all rendering related stuff for
//              the map in 2d
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
#include "MapRenderer2D.h"
#include "App.h"
#include "Game/Configuration.h"
#include "General/ColourConfiguration.h"
#include "MapEditor/Edit/ObjectEdit.h"
#include "MapEditor/ItemSelection.h"
#include "MapEditor/MapEditContext.h"
#include "MapEditor/MapEditor.h"
#include "MapEditor/MapTextureManager.h"
#include "OpenGL/Draw2D.h"
#include "OpenGL/GLTexture.h"
#include "OpenGL/LineBuffer.h"
#include "OpenGL/OpenGL.h"
#include "OpenGL/PointSpriteBuffer.h"
#include "OpenGL/Shader.h"
#include "OpenGL/VertexBuffer2D.h"
#include "OpenGL/View.h"
#include "SLADEMap/SLADEMap.h"
#include "ThingBuffer2D.h"
#include "Utility/Polygon2D.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(Bool, vertex_round, true, CVar::Flag::Save)
CVAR(Int, vertex_size, 7, CVar::Flag::Save)
CVAR(Float, line_width, 1.5f, CVar::Flag::Save)
CVAR(Bool, line_smooth, true, CVar::Flag::Save)
CVAR(Int, thing_shape, 0, CVar::Flag::Save)
CVAR(Bool, thing_sprites, true, CVar::Flag::Save)
CVAR(Int, thing_drawtype, 1, CVar::Flag::Save)
CVAR(Bool, thing_force_dir, false, CVar::Flag::Save)
CVAR(Bool, thing_overlay_square, false, CVar::Flag::Save)
CVAR(Bool, thing_preview_lights, true, CVar::Flag::Save)
CVAR(Float, thing_light_intensity, 0.5f, CVar::Flag::Save)
CVAR(Float, flat_brightness, 0.8f, CVar::Flag::Save)
CVAR(Bool, flat_ignore_light, false, CVar::Flag::Save)
CVAR(Float, thing_shadow, 0.5f, CVar::Flag::Save)
CVAR(Bool, sector_hilight_fill, true, CVar::Flag::Save)
CVAR(Bool, sector_selected_fill, true, CVar::Flag::Save)
CVAR(Bool, map_animate_hilight, true, CVar::Flag::Save)
CVAR(Bool, map_animate_selection, false, CVar::Flag::Save)
CVAR(Bool, map_animate_tagged, true, CVar::Flag::Save)
CVAR(Float, arrow_alpha, 1.0f, CVar::Flag::Save)
CVAR(Bool, arrow_colour, false, CVar::Flag::Save)
CVAR(Bool, flats_use_vbo, true, CVar::Flag::Save)
CVAR(Int, halo_width, 5, CVar::Flag::Save)
CVAR(Float, arrowhead_angle, 0.7854f, CVar::Flag::Save)
CVAR(Float, arrowhead_length, 25.f, CVar::Flag::Save)
CVAR(Bool, action_lines, true, CVar::Flag::Save)
CVAR(Bool, test_ssplit, false, CVar::Flag::Save)


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, use_zeth_icons)


// -----------------------------------------------------------------------------
//
//  Functions
//
// -----------------------------------------------------------------------------
namespace
{
// -----------------------------------------------------------------------------
// Sets up the given things [buffer] for drawing things of type [tt]
// -----------------------------------------------------------------------------
void setupThingBuffer(gl::ThingBuffer2D& buffer, const game::ThingType& tt)
{
	// Determine texture
	auto tex    = 0;
	auto sprite = false;

	// Sprite if we are drawing them
	if (thing_sprites)
	{
		tex    = mapeditor::textureManager().sprite(tt.sprite(), tt.translation(), tt.palette()).gl_id;
		sprite = true;
	}

	// If no sprite found, use editor icon
	if (!tex && !tt.icon().empty())
	{
		tex    = mapeditor::textureManager().editorImage(fmt::format("thing/{}", tt.icon())).gl_id;
		sprite = false;
	}

	// Setup buffer
	buffer.setup(tt);
	buffer.setTexture(tex, sprite);
}

// -----------------------------------------------------------------------------
// Returns the colour for [line]
// -----------------------------------------------------------------------------
ColRGBA lineColour(const MapLine* line, bool ignore_filter = false)
{
	ColRGBA col;

	if (line)
	{
		// Check for special line
		if (line->special() > 0)
			col.set(colourconfig::colour("map_line_special"));
		else if (line->s1())
			col.set(colourconfig::colour("map_line_normal"));
		else
			col.set(colourconfig::colour("map_line_invalid"));

		// Check for two-sided line
		if (line->s2())
			col.a *= 0.6f;

		// Check if filtered
		if (line->isFiltered() && !ignore_filter)
			col.a *= 0.25f;
	}

	return col;
}
} // namespace


// -----------------------------------------------------------------------------
//
// MapRenderer2D Class Functions
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// MapRenderer2D class constructor
// -----------------------------------------------------------------------------
MapRenderer2D::MapRenderer2D(SLADEMap* map, gl::View* view) : map_{ map }, view_{ view }
{
	temp_lines_buffer_          = std::make_unique<gl::LineBuffer>();
	thing_overlay_buffer_       = std::make_unique<gl::PointSpriteBuffer>();
	thing_light_preview_buffer_ = std::make_unique<gl::VertexBuffer2D>();
	temp_things_buffer_         = std::make_unique<gl::ThingBuffer2D>();
}

// -----------------------------------------------------------------------------
// MapRenderer2D class destructor
// -----------------------------------------------------------------------------
MapRenderer2D::~MapRenderer2D() = default;

// -----------------------------------------------------------------------------
// Returns the texture to use for vertices (or vertex [overlay]s)
// -----------------------------------------------------------------------------
unsigned MapRenderer2D::vertexTexture(bool overlay)
{
	if (overlay)
		return mapeditor::textureManager().editorImage(vertex_round ? "vertex/hilight_r" : "vertex/hilight_s").gl_id;

	return mapeditor::textureManager().editorImage(vertex_round ? "vertex/round" : "vertex/square").gl_id;
}

// -----------------------------------------------------------------------------
// Returns the radius (in map scale) of vertices based on the current view
// -----------------------------------------------------------------------------
float MapRenderer2D::vertexRadius(float scale) const
{
	auto vscale = view_->scale(true).x;
	auto size   = vertex_size / vscale;

	if (vscale < 1.0f)
		size *= vscale;

	auto min = 4.0f / vscale;
	if (size < min)
		size = min;

	return size * 0.5f * scale;
}

// -----------------------------------------------------------------------------
// Renders map vertices
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertices(float alpha)
{
	// Check there are any vertices to render
	if (map_->nVertices() == 0)
		return;

	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	// Update vertices buffer if required
	if (!vertices_buffer_ || map_->nVertices() != n_vertices_ || map_->geometryUpdated() > vertices_updated_)
		updateVerticesBuffer();

	// Setup rendering options
	vertices_buffer_->setColour(colourconfig::colour("map_vertex").ampf(1.0f, 1.0f, 1.0f, alpha).asVec4());
	vertices_buffer_->setPointRadius(vertexRadius());
	gl::setBlend(gl::Blend::Normal);

	// Set texture
	glEnable(GL_TEXTURE_2D);
	gl::Texture::bind(vertexTexture(false));

	// Render vertices
	vertices_buffer_->draw(gl::PointSpriteType::Textured, view_);
}

// -----------------------------------------------------------------------------
// Renders the vertex hilight overlay for vertex [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertexHilight(int index, float fade) const
{
	// Check hilight
	auto vertex = map_->vertex(index);
	if (!vertex)
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Setup rendering options
	vertices_buffer_->setColour(colourconfig::colour("map_hilight").ampf(1.0f, 1.0f, 1.0f, fade).asVec4());
	vertices_buffer_->setPointRadius(vertexRadius(1.8f + 0.6f * fade));
	gl::setBlend(colourconfig::colDef("map_hilight").blendMode());

	// Set texture
	glEnable(GL_TEXTURE_2D);
	gl::Texture::bind(vertexTexture(true));

	// Render
	vertices_buffer_->draw(gl::PointSpriteType::Textured, view_, index, 1);
}

// -----------------------------------------------------------------------------
// Renders the vertex selection overlay for vertex indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertexSelection(gl::draw2d::Context& dc, const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Build vertex position list
	vector<Vec2f> vertices;
	for (const auto& item : selection)
		if (auto v = item.asVertex(*map_); v)
			vertices.emplace_back(v->position());

	// Draw as point sprites
	dc.setColourFromConfig("map_selection", fade);
	dc.texture            = vertexTexture(true);
	dc.pointsprite_type   = gl::PointSpriteType::Textured;
	dc.pointsprite_radius = 1.8f * vertexRadius();
	dc.drawPointSprites(vertices);
}

// -----------------------------------------------------------------------------
// Renders map lines, with direction tabs if [show_direction] is true
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLines(bool show_direction, float alpha)
{
	// Check there are any lines to render
	if (map_->nLines() == 0)
		return;

	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	// Update lines buffer if needed
	if (!lines_buffer_ || lines_buffer_->buffer().empty() || show_direction != lines_dirs_ || map_->nLines() != n_lines_
		|| map_->geometryUpdated() > lines_updated_
		|| map_->mapData().modifiedSince(lines_updated_, MapObject::Type::Line))
		updateLinesBuffer(show_direction);

	// Render lines buffer
	lines_buffer_->setWidthMult(line_width);
	lines_buffer_->draw(view_, { 1.0f, 1.0f, 1.0f, alpha });
}

// -----------------------------------------------------------------------------
// Renders the line hilight overlay for line [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLineHilight(gl::draw2d::Context& dc, int index, float fade) const
{
	// Check hilight
	auto line = map_->line(index);
	if (!line)
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Render line hilight (+ direction tab)
	auto mid = line->getPoint(MapObject::Point::Mid);
	auto tab = line->dirTabPoint();
	dc.setColourFromConfig("map_hilight", fade);
	dc.line_thickness = line_width * (colourconfig::lineHilightWidth() * fade);
	dc.drawLines({ { line->start(), line->end() }, { mid, tab } });
}

// -----------------------------------------------------------------------------
// Renders the line selection overlay for line indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLineSelection(gl::draw2d::Context& dc, const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Build lines list
	vector<Rectf> lines;
	for (const auto& item : selection)
	{
		if (auto line = item.asLine(*map_); line)
		{
			auto mid = line->getPoint(MapObject::Point::Mid);
			auto tab = line->dirTabPoint();
			lines.emplace_back(line->x1(), line->y1(), line->x2(), line->y2());
			lines.emplace_back(mid.x, mid.y, tab.x, tab.y);
		}
	}

	// Render lines
	dc.setColourFromConfig("map_selection", fade);
	dc.line_thickness = line_width * colourconfig::lineSelectionWidth();
	dc.drawLines(lines);
}

// -----------------------------------------------------------------------------
// Renders the tagged line overlay for lines in [lines]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedLines(gl::draw2d::Context& dc, const vector<MapLine*>& lines, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Build list of lines & arrows to render
	vector<Rectf> r_lines;
	vector<Rectf> r_arrows;
	auto          object = mapeditor::editContext().selection().hilightedObject();
	for (auto line : lines)
	{
		// Line
		auto mid = line->getPoint(MapObject::Point::Mid);
		auto tab = line->dirTabPoint();
		r_lines.emplace_back(line->x1(), line->y1(), line->x2(), line->y2());
		r_lines.emplace_back(mid.x, mid.y, tab.x, tab.y);

		// Action lines
		if (object && action_lines)
		{
			auto op = object->getPoint(MapObject::Point::Within);
			auto lp = line->getPoint(MapObject::Point::Within);
			r_arrows.emplace_back(op.x, op.y, lp.x, lp.y);
		}
	}

	// Render tagged lines
	dc.setColourFromConfig("map_tagged", fade);
	dc.line_thickness = line_width * colourconfig::lineHilightWidth();
	dc.drawLines(r_lines);

	// Render action lines
	dc.line_thickness    = line_width * 1.5f;
	dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
	dc.drawLines(r_arrows);
	dc.line_arrow_length = 0.0f;
}

// -----------------------------------------------------------------------------
// Renders the tagging line overlay for lines in [lines]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggingLines(gl::draw2d::Context& dc, const vector<MapLine*>& lines, float fade) const
{
	// Reset fade if tagging animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Build list of lines & arrows to render
	vector<Rectf> r_lines;
	vector<Rectf> r_arrows;
	auto          object = mapeditor::editContext().selection().hilightedObject();
	for (auto line : lines)
	{
		// Line
		auto mid = line->getPoint(MapObject::Point::Mid);
		auto tab = line->dirTabPoint();
		r_lines.emplace_back(line->x1(), line->y1(), line->x2(), line->y2());
		r_lines.emplace_back(mid.x, mid.y, tab.x, tab.y);

		// Action lines
		if (object && action_lines)
		{
			auto op = object->getPoint(MapObject::Point::Within);
			auto lp = line->getPoint(MapObject::Point::Within);
			r_arrows.emplace_back(lp.x, lp.y, op.x, op.y);
		}
	}

	// Render tagging lines
	dc.setColourFromConfig("map_tagged", fade);
	dc.line_thickness = line_width * colourconfig::lineHilightWidth();
	dc.drawLines(r_lines);

	// Render action lines
	dc.line_thickness    = line_width * 1.5f;
	dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
	dc.drawLines(r_arrows);
	dc.line_arrow_length = 0.0f;
}

// -----------------------------------------------------------------------------
// Renders overlays for the given [things], adding [radius_extra] to each radius
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingOverlays(
	gl::draw2d::Context&     dc,
	const vector<MapThing*>& things,
	float                    radius_extra,
	const Vec2d&             offset) const
{
	// Check if we want square overlays
	if (thing_overlay_square)
	{
		dc.texture = 0;
		dc.colour.a /= 2;

		for (const auto thing : things)
		{
			const auto& tt     = game::configuration().thingType(thing->type());
			float       radius = tt.shrinkOnZoom() ? scaledRadius(tt.radius()) : tt.radius();

			// Draw simple rect
			dc.drawRect({ thing->xPos() + offset.x - radius,
						  thing->yPos() + offset.y - radius,
						  thing->xPos() + offset.x + radius,
						  thing->yPos() + offset.y + radius });
		}

		dc.colour.a *= 2;

		return;
	}

	// Otherwise, setup the thing overlay buffer with values from the dc
	gl::setBlend(dc.blend);
	thing_overlay_buffer_->setColour(dc.colour.asVec4());
	thing_overlay_buffer_->setFillOpacity(0.25f);
	thing_overlay_buffer_->setOutlineWidth(std::min(3.0f / (float)view_->scale().x, 4.0f));
	thing_overlay_buffer_->setPointRadius(dc.pointsprite_radius);

	// Populate thing overlay buffer
	for (const auto thing : things)
	{
		const auto& tt     = game::configuration().thingType(thing->type());
		float       radius = tt.shrinkOnZoom() ? scaledRadius(tt.radius()) : tt.radius();

		thing_overlay_buffer_->add(
			glm::vec2{ thing->xPos() + offset.x, thing->yPos() + offset.y }, radius + 4.0f + radius_extra);
	}
	thing_overlay_buffer_->push();

	// Draw the buffer
	thing_overlay_buffer_->draw(
		thing_shape == 1 ? gl::PointSpriteType::RoundedSquareOutline : gl::PointSpriteType::CircleOutline, dc.view);
}

// -----------------------------------------------------------------------------
// Renders all map things
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThings(float alpha, bool force_dir)
{
	// Don't bother if (practically) invisible
	if (alpha <= 0.01f || map_->nThings() == 0)
		return;

	things_angles_ = force_dir;

	if (thing_buffers_.empty() || map_->thingsUpdated() > things_updated_)
		updateThingBuffers();

	// Draw thing buffers
	gl::setBlend(gl::Blend::Normal);
	for (auto& buffer : thing_buffers_)
		buffer->draw(view_, glm::vec4{ 1.0f, 1.0f, 1.0f, alpha }, thing_shape == 1, force_dir);
}

// -----------------------------------------------------------------------------
// Renders given [things] including an optional [offset]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThings(const vector<MapThing*>& things, float alpha, const Vec2d& offset) const
{
	gl::setBlend(gl::Blend::Normal);

	// Render given things (by type)
	vector<int> types_rendered;
	unsigned    index = 0;
	while (index < things.size())
	{
		// Ignore if things of this type were already rendered
		auto ttype = things[index]->type();
		if (VECTOR_EXISTS(types_rendered, ttype))
		{
			++index;
			continue;
		}

		// Setup buffer with thing type properties from game configuration
		setupThingBuffer(*temp_things_buffer_, game::configuration().thingType(ttype));

		// Add all subsequent things of same type to buffer
		for (unsigned i = index; i < things.size(); ++i)
			if (things[i]->type() == ttype)
				temp_things_buffer_->add(
					things[i]->xPos() + offset.x, things[i]->yPos() + offset.y, things[i]->angle());
		temp_things_buffer_->push();

		// Render
		temp_things_buffer_->draw(view_, glm::vec4{ 1.0f, 1.0f, 1.0f, alpha }, thing_shape == 1);

		// Continue
		types_rendered.push_back(ttype);
		++index;
	}
}

// -----------------------------------------------------------------------------
// Renders the thing hilight overlay for thing [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingHilight(gl::draw2d::Context& dc, int index, float fade) const
{
	// Check hilight
	auto thing = map_->thing(index);
	if (!thing)
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set hilight colour
	dc.setColourFromConfig("map_hilight", fade);

	// Check if we want square overlays
	if (thing_overlay_square)
	{
		// Get thing info
		auto& tt = game::configuration().thingType(thing->type());
		float x  = thing->xPos();
		float y  = thing->yPos();

		// Get thing radius
		float radius = tt.radius();

		// Draw simple rect+outline
		Rectf rect{ x - radius, y - radius, x + radius, y + radius };
		dc.texture        = 0;
		dc.line_thickness = 3.0f;
		dc.drawRectOutline(rect);
		dc.colour.a /= 2;
		dc.drawRect(rect);

		return;
	}

	// Otherwise draw point sprite overlay
	renderThingOverlays(dc, { map_->thing(index) }, 4.0f * fade);
}

// -----------------------------------------------------------------------------
// Renders the thing selection overlay for thing indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingSelection(gl::draw2d::Context& dc, const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Build list of things
	vector<MapThing*> things;
	for (const auto& item : selection)
		if (auto thing = item.asThing(*map_))
			things.push_back(thing);

	// Render overlays
	dc.setColourFromConfig("map_selection", fade);
	renderThingOverlays(dc, things);
}

// -----------------------------------------------------------------------------
// Renders the tagged thing overlay for things in [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedThings(gl::draw2d::Context& dc, const vector<MapThing*>& things, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Render overlays
	dc.setColourFromConfig("map_tagged", fade);
	renderThingOverlays(dc, things);

	// Action lines
	auto object = mapeditor::editContext().selection().hilightedObject();
	if (object && action_lines)
	{
		// Build list of lines (arrow: hilighted object -> thing)
		vector<Rectf> r_arrows;
		for (auto thing : things)
			r_arrows.emplace_back(
				object->getPoint(MapObject::Point::Within), thing->getPoint(MapObject::Point::Within));

		// Render action lines
		dc.line_thickness    = line_width * 1.5f;
		dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
		dc.drawLines(r_arrows);
		dc.line_arrow_length = 0.0f;
	}
}

// -----------------------------------------------------------------------------
// Renders the tagging thing overlay for things in [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggingThings(gl::draw2d::Context& dc, const vector<MapThing*>& things, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Render overlays
	dc.setColourFromConfig("map_tagged", fade);
	renderThingOverlays(dc, things);

	// Action lines
	auto object = mapeditor::editContext().selection().hilightedObject();
	if (object && action_lines)
	{
		// Build list of lines (arrow: thing -> hilighted object)
		vector<Rectf> r_arrows;
		for (auto thing : things)
			r_arrows.emplace_back(
				thing->getPoint(MapObject::Point::Within), object->getPoint(MapObject::Point::Within));

		// Render action lines
		dc.line_thickness    = line_width * 1.5f;
		dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
		dc.drawLines(r_arrows);
		dc.line_arrow_length = 0.0f;
	}
}

// -----------------------------------------------------------------------------
// Renders thing pathing lines/arrows for [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderPathedThings(gl::draw2d::Context& dc, const vector<MapThing*>& things)
{
	// Skip if action lines are not desired, or if there's nothing to do
	if (!action_lines || things.empty())
		return;

	// Check if paths need updating
	bool update = false;
	if (thing_paths_.empty())
		update = true;
	else if (map_->thingsUpdated() > thing_paths_updated_)
	{
		for (auto& thing : things)
		{
			if (thing->modifiedTime() > thing_paths_updated_)
			{
				update = true;
				break;
			}
		}
		if (!update)
			thing_paths_updated_ = app::runTimer();
	}
	if (update)
		updateThingPaths(things);

	// Build line lists
	vector<Rectf> lines_path;
	vector<Rectf> lines_dragon;
	for (auto& thing_path : thing_paths_)
	{
		if (thing_path.from_index == thing_path.to_index)
			continue;

		auto from = map_->thing(thing_path.from_index);

		if (from && ((from->arg(3) | (from->arg(4) << 8)) > 0))
		{
			auto to = map_->thing(thing_path.to_index);
			if (!to)
				continue;

			if (thing_path.type == PathType::DragonBoth || thing_path.type == PathType::Dragon)
				lines_dragon.emplace_back(from->getPoint(MapObject::Point::Mid), to->getPoint(MapObject::Point::Mid));
			else
				lines_path.emplace_back(from->getPoint(MapObject::Point::Mid), to->getPoint(MapObject::Point::Mid));
		}
	}

	// Draw path lines
	dc.line_thickness    = line_width * 1.5f;
	dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
	if (!lines_path.empty())
	{
		dc.setColourFromConfig("map_thing_path");
		dc.drawLines(lines_path);
	}

	// Draw dragon path lines
	if (!lines_dragon.empty())
	{
		dc.setColourFromConfig("map_thing_path_dragon");
		dc.drawLines(lines_dragon);
	}

	dc.line_arrow_length = 0.0f;
}

// -----------------------------------------------------------------------------
// Renders point light previews
// -----------------------------------------------------------------------------
void MapRenderer2D::renderPointLightPreviews(gl::draw2d::Context& dc, float alpha, int hilight_index) const
{
	if (!thing_preview_lights)
		return;

	// Build light preview buffer
	glm::vec2 hl_position;
	glm::vec4 hl_colour;
	float     hl_radius = 0.0f;
	for (const auto& thing : map_->things())
	{
		const auto& ttype = game::configuration().thingType(thing->type());

		// Not a point light
		if (ttype.pointLight().empty())
			continue;

		auto light_col    = glm::vec4{ 1.0f };
		auto light_radius = 0.0f;

		// ZDoom point light
		if (ttype.pointLight() == "zdoom")
		{
			light_col.r  = thing->arg(0) / 255.0f;
			light_col.g  = thing->arg(1) / 255.0f;
			light_col.b  = thing->arg(2) / 255.0f;
			light_radius = thing->arg(3);
		}

		// Vavoom point light
		else if (ttype.pointLight() == "vavoom")
		{
			light_col.r  = thing->arg(1) / 255.0f;
			light_col.g  = thing->arg(2) / 255.0f;
			light_col.b  = thing->arg(3) / 255.0f;
			light_radius = thing->arg(0);
		}

		// Vavoom white light
		else if (ttype.pointLight() == "vavoom_white")
		{
			light_radius = thing->arg(0);
		}

		light_radius *= 2; // Doubling the radius value matches better with in-game results

		// Add to buffer
		thing_light_preview_buffer_->addQuadTriangles(
			{ thing->xPos() - light_radius, thing->yPos() - light_radius },
			{ thing->xPos() + light_radius, thing->yPos() + light_radius },
			light_col);

		// Set hilight infor if hilighted
		if (thing->index() == hilight_index)
		{
			hl_colour   = light_col;
			hl_position = glm::vec2{ thing->xPos(), thing->yPos() };
			hl_radius   = light_radius;
		}
	}
	thing_light_preview_buffer_->push();

	// Setup rendering
	const auto& shader = gl::draw2d::defaultShader();
	dc.texture         = mapeditor::textureManager().editorImage("thing/light_preview").gl_id;
	dc.colour.set(255, 255, 255, static_cast<uint8_t>(alpha * (thing_light_intensity * 255.f)));
	dc.blend = gl::Blend::Additive;
	dc.setupToDraw(shader);

	// Draw buffer
	thing_light_preview_buffer_->draw(gl::Primitive::Triangles);

	// Draw hilight ring if needed
	if (hl_radius > 0.0f)
	{
		dc.pointsprite_type          = gl::PointSpriteType::CircleOutline;
		dc.pointsprite_radius        = hl_radius;
		dc.pointsprite_fill_opacity  = 0.0f;
		dc.pointsprite_outline_width = std::min(2.0f / (float)view_->scale().x, 4.0f);
		dc.colour.set(hl_colour.r * 255, hl_colour.g * 255, hl_colour.b * 255, alpha * 255);
		dc.drawPointSprites(vector{ Vec2f{ hl_position.x, hl_position.y } });
		dc.pointsprite_radius = 1.0f;
	}
}


// -----------------------------------------------------------------------------
// Renders map flats (sectors)
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlats(int type, bool texture, float alpha)
{
	using game::Feature;
	using game::UDMFFeature;

	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	// Apply flat alpha from theme
	if (texture)
		alpha *= colourconfig::flatAlpha();

	// Re-init flats texture list if invalid
	if ((texture && tex_flats_.size() != map_->nSectors()) || last_flat_type_ != type)
	{
		tex_flats_.clear();
		for (unsigned a = 0; a < map_->nSectors(); a++)
			tex_flats_.push_back(0);

		last_flat_type_ = type;
	}

	// Create vertex buffer if necessary
	if (!flats_buffer_)
		updateFlatsBuffer();

	// Check if any polygon vertex data has changed (in this case we need to refresh the entire buffer)
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		auto poly = map_->sector(a)->polygon();
		if (poly && poly->vboUpdate() > 1)
		{
			updateFlatsBuffer();
			break;
		}
	}

	// Setup shader
	const auto& shader = gl::draw2d::defaultShader(texture);
	view_->setupShader(shader);
	if (flat_ignore_light)
		shader.setUniform("colour", glm::vec4{ flat_brightness, flat_brightness, flat_brightness, alpha });

	// Go through sectors
	unsigned tex_last = 0;
	unsigned tex      = 0;
	unsigned update   = 0;
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		auto sector = map_->sector(a);

		// Skip if sector is out of view
		if (vis_s_[a] > 0)
			continue;

		const MapTextureManager::Texture* map_tex_props = nullptr;
		if (texture)
		{
			if (!tex_flats_[a] || sector->modifiedTime() > flats_updated_ - 100)
			{
				// Get the sector texture
				bool mix_tex_flats = game::configuration().featureSupported(Feature::MixTexFlats);
				if (type <= 1)
					map_tex_props = &mapeditor::textureManager().flat(sector->floor().texture, mix_tex_flats);
				else
					map_tex_props = &mapeditor::textureManager().flat(sector->ceiling().texture, mix_tex_flats);

				tex           = map_tex_props->gl_id;
				tex_flats_[a] = tex;
			}
			else
				tex = tex_flats_[a];
		}

		// Setup polygon texture info if needed
		auto poly = sector->polygon();
		if (texture && poly->texture() != tex)
		{
			poly->setTexture(tex); // Set polygon texture

			// Get scaling/offset info
			double ox  = 0.;
			double oy  = 0.;
			double sx  = map_tex_props ? map_tex_props->scale.x : 1.;
			double sy  = map_tex_props ? map_tex_props->scale.y : 1.;
			double rot = 0.;
			// Check for various UDMF extensions
			if (mapeditor::editContext().mapDesc().format == MapFormat::UDMF)
			{
				// Floor
				if (type <= 1)
				{
					if (game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningfloor");
						oy = sector->floatProperty("ypanningfloor");
					}
					if (game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscalefloor"));
						sy *= (1.0 / sector->floatProperty("yscalefloor"));
					}
					if (game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationfloor");
				}
				// Ceiling
				else
				{
					if (game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningceiling");
						oy = sector->floatProperty("ypanningceiling");
					}
					if (game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscaleceiling"));
						sy *= (1.0 / sector->floatProperty("yscaleceiling"));
					}
					if (game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationceiling");
				}
			}
			// Scaling applies to offsets as well.
			// Note for posterity: worldpanning only applies to textures, not flats
			ox /= sx;
			oy /= sy;

			poly->updateTextureCoords(sx, sy, ox, oy, rot);
		}

		// Update polygon VBO data if needed
		if (poly->vboUpdate() > 0)
		{
			poly->updateVBData(*flats_buffer_);
			update++;
			if (update > 200)
				break;
		}

		// Bind the texture if needed
		if (texture && tex != tex_last)
			gl::Texture::bind(tex);
		tex_last = tex;

		// Render the polygon
		if (!flat_ignore_light)
		{
			auto col = sector->colourAt(type).ampf(flat_brightness, flat_brightness, flat_brightness, alpha);
			shader.setUniform("colour", col.asVec4());
		}
		poly->render(*flats_buffer_);
	}
}

// -----------------------------------------------------------------------------
// Renders the flat hilight overlay for sector [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatHilight(gl::draw2d::Context& dc, int index, float fade) const
{
	// Check hilight
	if (!map_->sector(index))
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set render options
	dc.setColourFromConfig("map_hilight", fade);
	dc.line_thickness = line_width * (colourconfig::lineHilightWidth() * fade);

	// Fill if cvar is set
	if (sector_hilight_fill)
	{
		const auto& shader = gl::draw2d::defaultShader(false);
		shader.setUniform("colour", dc.colour.ampf(1.0f, 1.0f, 1.0f, 0.2f).asVec4());
		dc.view->setupShader(shader);
		map_->sector(index)->polygon()->render(*flats_buffer_);
		dc.line_thickness *= 0.75f;
	}

	// Get all lines belonging to the hilighted sector
	vector<MapLine*> lines;
	map_->sector(index)->putLines(lines);

	// Build list of lines to render
	vector<Rectf> render_lines;
	for (auto line : lines)
		if (line)
			render_lines.emplace_back(line->x1(), line->y1(), line->x2(), line->y2());

	// Render lines
	dc.drawLines(render_lines);

	//// Draw sector split lines
	// if (test_ssplit)
	//{
	//	glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * 0.5f);
	//	glLineWidth(1.0f);
	//	map_->sector(index)->polygon()->renderWireframe();
	// }

	//// TEST draw text point
	// glPointSize(8.0f);
	// glBegin(GL_POINTS);
	// glVertex2d(map->getSector(index)->getPoint(MapObject::Point::Within).x,
	// map->getSector(index)->getPoint(MapObject::Point::Within).y); glEnd();
}

// -----------------------------------------------------------------------------
// Renders flat overlays for the given [sectors]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatOverlays(const gl::draw2d::Context& dc, const vector<MapSector*>& sectors) const
{
	// Setup shader (for fill)
	const auto& shader = gl::draw2d::defaultShader(false);
	shader.setUniform("colour", dc.colour.ampf(1.0f, 1.0f, 1.0f, 0.2f).asVec4());
	dc.view->setupShader(shader);

	// Go through selection, render fill (if needed) and build list of lines to render (for outline)
	vector<uint8_t> lines_added(map_->nLines(), 0);
	vector<Rectf>   render_lines;
	for (const auto sector : sectors)
	{
		// Don't draw if outside screen (but still draw if it's small)
		if (vis_s_[sector->index()] > 0 && vis_s_[sector->index()] != VIS_SMALL)
			continue;

		// Render fill if needed
		if (sector_selected_fill && sector->polygon()->hasPolygon())
			sector->polygon()->render(*flats_buffer_);

		// Go through sides
		for (const auto side : sector->connectedSides())
		{
			// Get line
			auto line = side->parentLine();
			if (lines_added[line->index()] > 0)
				continue; // Ignore if already added

			// Add line to render list
			render_lines.emplace_back(line->x1(), line->y1(), line->x2(), line->y2());
			lines_added[line->index()] = 1;
		}
	}

	// Render lines
	dc.drawLines(render_lines);
}

// -----------------------------------------------------------------------------
// Renders the flat selection overlay for sectors in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatSelection(gl::draw2d::Context& dc, const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Set render options
	dc.setColourFromConfig("map_selection", fade);
	dc.line_thickness = line_width * 2.0f;

	// Render flat overlays for selection
	vector<MapSector*> sectors;
	for (const auto& item : selection)
		sectors.push_back(item.asSector(*map_));
	renderFlatOverlays(dc, sectors);
}

// -----------------------------------------------------------------------------
// Renders the tagged flat overlay for sectors in [sectors]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedFlats(gl::draw2d::Context& dc, const vector<MapSector*>& sectors, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Setup render options
	dc.setColourFromConfig("map_tagged", fade);
	dc.line_thickness = line_width * 2.0f;

	// Render overlays for tagged sectors
	renderFlatOverlays(dc, sectors);

	// Action Lines
	auto object = mapeditor::editContext().selection().hilightedObject();
	if (action_lines && object)
	{
		vector<Rectf> lines;
		for (const auto sector : sectors)
		{
			// Skip if the tagged sector is adjacent
			if (object->objType() == MapObject::Type::Line)
			{
				auto line = dynamic_cast<MapLine*>(object);
				if (line->frontSector() == sector || line->backSector() == sector)
					continue;
			}

			lines.emplace_back(object->getPoint(MapObject::Point::Within), sector->getPoint(MapObject::Point::Within));
		}

		// Render action lines
		dc.line_thickness    = line_width * 1.5f;
		dc.line_arrow_length = 24.0f / dc.view->scale(true).x;
		dc.drawLines(lines);
		dc.line_arrow_length = 0.0f;
	}
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for vertex indices in [vertices], to show movement
// by [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingVertices(
	gl::draw2d::Context&           dc,
	const vector<mapeditor::Item>& vertices,
	const Vec2d&                   move_vec) const
{
	vector<uint8_t> lines_drawn(map_->nLines(), 0);

	// Determine what lines need drawing (and which of their vertices are being moved)
	for (const auto& item : vertices)
	{
		if (auto v = item.asVertex(*map_))
		{
			for (const auto& line : v->connectedLines())
			{
				if (line->v1() == v)
					lines_drawn[line->index()] |= 1;
				if (line->v2() == v)
					lines_drawn[line->index()] |= 2;
			}
		}
	}

	// Update moving lines buffer with lines attached to moving vertices
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		auto line  = map_->line(a);
		auto drawn = lines_drawn[line->index()];

		// Skip if not attached to any moving vertices
		if (drawn == 0)
			continue;

		// First vertex
		auto v1 = (drawn & 1) ? glm::vec2{ line->x1() + move_vec.x, line->y1() + move_vec.y } :
								glm::vec2{ line->x1(), line->y1() };

		// Second vertex
		auto v2 = (drawn & 2) ? glm::vec2{ line->x2() + move_vec.x, line->y2() + move_vec.y } :
								glm::vec2{ line->x2(), line->y2() };

		// Add to buffer
		temp_lines_buffer_->add2d(v1.x, v1.y, v2.x, v2.y, lineColour(line, true).asVec4());
	}
	temp_lines_buffer_->push();

	// Draw moving lines
	gl::setBlend(gl::Blend::Normal);
	temp_lines_buffer_->setWidthMult(line_width);
	temp_lines_buffer_->draw(view_);

	// Get list of moving vertex points
	vector<Vec2f> points;
	for (const auto& item : vertices)
		if (auto v = item.asVertex(*map_))
			points.emplace_back(v->xPos() + move_vec.x, v->yPos() + move_vec.y);

	// Draw moving vertices
	dc.setColourFromConfig("map_moving");
	dc.pointsprite_type   = gl::PointSpriteType::Textured;
	dc.pointsprite_radius = vertexRadius(1.5f);
	dc.texture            = vertexTexture();
	dc.drawPointSprites(points);
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for line indices in [lines], to show movement by
// [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingLines(
	gl::draw2d::Context&           dc,
	const vector<mapeditor::Item>& lines,
	const Vec2d&                   move_vec) const
{
	vector<uint8_t> lines_drawn(map_->nLines(), 0);

	// Determine what lines need drawing (and which of their vertices are being moved)
	for (const auto& item : lines)
	{
		if (auto line = item.asLine(*map_))
		{
			// Check first vertex
			auto v = line->v1();
			for (unsigned l = 0; l < v->nConnectedLines(); l++)
			{
				auto cline = v->connectedLine(l);

				if (cline->v1() == v)
					lines_drawn[cline->index()] |= 1;
				if (cline->v2() == v)
					lines_drawn[cline->index()] |= 2;
			}

			// Check second vertex
			v = line->v2();
			for (unsigned l = 0; l < v->nConnectedLines(); l++)
			{
				auto cline = v->connectedLine(l);

				if (cline->v1() == v)
					lines_drawn[cline->index()] |= 1;
				if (cline->v2() == v)
					lines_drawn[cline->index()] |= 2;
			}
		}
	}

	// Update moving lines buffer with lines attached to moving vertices
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		auto line  = map_->line(a);
		auto drawn = lines_drawn[line->index()];

		// Skip if not attached to any moving vertices
		if (drawn == 0)
			continue;

		// First vertex
		auto v1 = (drawn & 1) ? glm::vec2{ line->x1() + move_vec.x, line->y1() + move_vec.y } :
								glm::vec2{ line->x1(), line->y1() };

		// Second vertex
		auto v2 = (drawn & 2) ? glm::vec2{ line->x2() + move_vec.x, line->y2() + move_vec.y } :
								glm::vec2{ line->x2(), line->y2() };

		// Add to buffer
		temp_lines_buffer_->add2d(v1.x, v1.y, v2.x, v2.y, lineColour(line, true).asVec4());
	}
	temp_lines_buffer_->push();

	// Draw moving lines
	gl::setBlend(gl::Blend::Normal);
	temp_lines_buffer_->setWidthMult(line_width);
	temp_lines_buffer_->draw(view_);

	// Build list of moving lines (for overlays)
	vector<Rectf> line_overlays;
	for (const auto& item : lines)
		if (auto line = item.asLine(*map_))
			line_overlays.emplace_back(line->start() + move_vec, line->end() + move_vec);

	// Draw moving line overlays
	dc.setColourFromConfig("map_moving");
	dc.line_thickness = line_width * 3;
	dc.drawLines(line_overlays);
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for sector indices in [sectors], to show movement
// by [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingSectors(
	gl::draw2d::Context&           dc,
	const vector<mapeditor::Item>& sectors,
	const Vec2d&                   move_vec) const
{
	// Determine what lines are being moved
	vector<uint8_t> lines_moved(map_->nLines(), 0);
	for (auto item : sectors)
	{
		if (auto sector = item.asSector(*map_))
		{
			// Go through connected sides
			auto& sides = sector->connectedSides();
			for (auto& side : sides)
				lines_moved[side->parentLine()->index()] = 1; // Mark parent line as moved
		}
	}

	// Build list of moving lines
	vector<mapeditor::Item> lines;
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		if (lines_moved[a] > 0)
			lines.emplace_back((int)a, mapeditor::ItemType::Line);
	}

	// Draw moving lines
	renderMovingLines(dc, lines, move_vec);
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for thing indices in [things], to show movement by
// [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingThings(
	gl::draw2d::Context&           dc,
	const vector<mapeditor::Item>& things,
	const Vec2d&                   move_vec) const
{
	vector<MapThing*> moving_things;
	for (const auto& item : things)
		if (auto thing = item.asThing(*map_))
			moving_things.push_back(thing);

	// Render things
	renderThings(moving_things, 1.0f, move_vec);

	// Render overlays
	dc.setColourFromConfig("map_moving");
	renderThingOverlays(dc, moving_things, 0.0f, move_vec);
}

// -----------------------------------------------------------------------------
// Renders pasting overlay for [things] at [pos]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderPasteThings(gl::draw2d::Context& dc, const vector<MapThing*>& things, const Vec2d& pos) const
{
	// Render things
	renderThings(things, 1.0f, pos);

	// Render overlays
	dc.setColourFromConfig("map_linedraw");
	renderThingOverlays(dc, things, 0.0f, pos);
}

// -----------------------------------------------------------------------------
// Renders object edit group overlay for [group]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderObjectEditGroup(gl::draw2d::Context& dc, ObjectEditGroup* group) const
{
	// Simple test
	vector<Vec2d> vertex_points;
	group->putVerticesToDraw(vertex_points);
	vector<ObjectEditGroup::Line> lines;
	group->putLinesToDraw(lines);

	// Set 'drawing' colour
	colourconfig::setGLColour("map_linedraw");

	// --- Lines ---

	// Lines
	for (auto& line : lines)
	{
		temp_lines_buffer_->add2d(
			line.v1->position.x,
			line.v1->position.y,
			line.v2->position.x,
			line.v2->position.y,
			lineColour(line.map_line, true).asVec4());
	}
	temp_lines_buffer_->push();
	temp_lines_buffer_->setWidthMult(line_width);
	temp_lines_buffer_->draw(view_);

	// Edit overlay
	vector<Rectf> overlay_lines;
	for (auto& line : lines)
		if (!line.isExtra())
			overlay_lines.emplace_back(
				line.v1->position.x, line.v1->position.y, line.v2->position.x, line.v2->position.y);
	dc.setColourFromConfig("map_object_edit");
	dc.line_thickness = line_width * 3;
	dc.drawLines(overlay_lines);


	// --- Vertices ---

	dc.pointsprite_type   = gl::PointSpriteType::Textured;
	dc.pointsprite_radius = vertexRadius();
	dc.texture            = vertexTexture();
	dc.drawPointSprites(vertex_points);


	// --- Things ---

	// Get things to draw
	vector<ObjectEditGroup::Thing> things;
	group->putThingsToDraw(things);

	if (!things.empty())
	{
		// Draw things
		MapThing* thing;
		double    x, y, angle;
		for (auto& item : things)
		{
			// Get thing info
			thing = item.map_thing;
			x     = item.position.x;
			y     = item.position.y;
			angle = thing->angle();

			// Setup temp. thing buffer with type properties from game configuration
			setupThingBuffer(*temp_things_buffer_, game::configuration().thingType(thing->type()));

			// Draw thing
			temp_things_buffer_->add(x, y, angle);
			temp_things_buffer_->push();
			temp_things_buffer_->draw(view_, glm::vec4{ 1.0f }, thing_shape == 1);
		}

		// Draw thing overlays
		for (auto& item : things)
		{
			thing              = item.map_thing;
			const auto& tt     = game::configuration().thingType(thing->type());
			double      radius = tt.radius();
			if (tt.shrinkOnZoom())
				radius = scaledRadius(radius);

			thing_overlay_buffer_->add({ item.position.x, item.position.y }, radius + 4.0f);
		}
		thing_overlay_buffer_->push();
		thing_overlay_buffer_->setColour(colourconfig::colour("map_object_edit").asVec4());
		thing_overlay_buffer_->setFillOpacity(0.25f);
		thing_overlay_buffer_->setOutlineWidth(std::min(3.0f / (float)view_->scale().x, 4.0f));
		thing_overlay_buffer_->draw(
			thing_shape == 1 ? gl::PointSpriteType::RoundedSquareOutline : gl::PointSpriteType::CircleOutline, view_);
	}
}

// -----------------------------------------------------------------------------
// (Re)builds the map vertices buffer
// -----------------------------------------------------------------------------
void MapRenderer2D::updateVerticesBuffer()
{
	if (!vertices_buffer_)
		vertices_buffer_ = std::make_unique<gl::PointSpriteBuffer>();

	// Fill vertices buffer
	for (const auto vertex : map_->vertices())
		vertices_buffer_->add({ vertex->xPos(), vertex->yPos() });
	vertices_buffer_->push();

	n_vertices_       = map_->nVertices();
	vertices_updated_ = app::runTimer();
}

// -----------------------------------------------------------------------------
// (Re)builds the map lines buffer
// -----------------------------------------------------------------------------
void MapRenderer2D::updateLinesBuffer(bool show_direction)
{
	// Init buffer
	if (!lines_buffer_)
		lines_buffer_ = std::make_unique<gl::LineBuffer>();

	// Add all map lines to buffer
	for (const auto line : map_->lines())
	{
		auto col = lineColour(line);

		lines_buffer_->add2d(line->x1(), line->y1(), line->x2(), line->y2(), col.asVec4(), 1.0f);

		// Direction tab if needed
		if (show_direction)
		{
			auto mid = line->getPoint(MapObject::Point::Mid);
			auto tab = line->dirTabPoint();
			lines_buffer_->add2d(mid.x, mid.y, tab.x, tab.y, { col.fr(), col.fg(), col.fb(), col.fa() * 0.6f }, 1.0f);
		}
	}
	lines_buffer_->push();

	lines_dirs_    = show_direction;
	n_lines_       = map_->nLines();
	lines_updated_ = app::runTimer();
}

// -----------------------------------------------------------------------------
// (Re)builds the map flats buffer
// -----------------------------------------------------------------------------
void MapRenderer2D::updateFlatsBuffer()
{
	// Init buffer
	if (!flats_buffer_)
		flats_buffer_ = std::make_unique<gl::VertexBuffer2D>();

	// Write sector polygons to buffer
	for (const auto& sector : map_->sectors())
		sector->polygon()->writeToVB(*flats_buffer_);
	flats_buffer_->push();

	flats_updated_ = app::runTimer();
}

// -----------------------------------------------------------------------------
// (Re)builds the map thing buffers
// -----------------------------------------------------------------------------
void MapRenderer2D::updateThingBuffers()
{
	thing_buffers_.clear();

	std::unordered_map<int, gl::ThingBuffer2D*> buffers;

	for (const auto& thing : map_->things())
	{
		// Create buffer for thing type if needed
		gl::ThingBuffer2D* buffer = buffers[thing->type()];
		if (!buffer)
		{
			thing_buffers_.emplace_back(new gl::ThingBuffer2D());
			buffer                 = thing_buffers_.back().get();
			buffers[thing->type()] = buffer;

			// Setup buffer with thing type properties from game configuration
			setupThingBuffer(*buffer, game::configuration().thingType(thing->type()));
		}

		// Add to buffer
		buffer->add(thing->xPos(), thing->yPos(), thing->angle(), thing->isFiltered() ? 0.25f : 1.0f);
	}

	// Upload buffers
	for (auto& buffer : thing_buffers_)
		buffer->push();

	things_updated_ = app::runTimer();
}

// -----------------------------------------------------------------------------
// Updates map object visibility info depending on the current view
// -----------------------------------------------------------------------------
void MapRenderer2D::updateVisibility(const Vec2d& view_tl, const Vec2d& view_br)
{
	// Sector visibility
	if (map_->nSectors() != vis_s_.size())
	{
		// Number of sectors changed, reset array
		vis_s_.clear();
		for (unsigned a = 0; a < map_->nSectors(); a++)
			vis_s_.push_back(0);
	}
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		// Check against sector bounding box
		auto bbox = map_->sector(a)->boundingBox();
		vis_s_[a] = 0;
		if (bbox.max.x < view_tl.x)
			vis_s_[a] = VIS_LEFT;
		if (bbox.max.y < view_tl.y)
			vis_s_[a] = VIS_ABOVE;
		if (bbox.min.x > view_br.x)
			vis_s_[a] = VIS_RIGHT;
		if (bbox.min.y > view_br.y)
			vis_s_[a] = VIS_BELOW;

		// Check if the sector is worth drawing
		if ((bbox.max.x - bbox.min.x) * view_->scale().x < 4 || (bbox.max.y - bbox.min.y) * view_->scale().x < 4)
			vis_s_[a] = VIS_SMALL;
	}
}

// -----------------------------------------------------------------------------
// Updates all VBOs and other cached data
// -----------------------------------------------------------------------------
void MapRenderer2D::forceUpdate(float line_alpha)
{
	// Update variables
	tex_flats_.clear();
	thing_paths_.clear();

	// Update buffers
	updateLinesBuffer(lines_dirs_);
	updateVerticesBuffer();
	updateThingBuffers();

	renderVertices(view_->scale().x);
	renderLines(lines_dirs_, line_alpha);
}

// -----------------------------------------------------------------------------
// Returns [radius] scaled such that it stays the same size on screen at all
// zoom levels
// -----------------------------------------------------------------------------
double MapRenderer2D::scaledRadius(int radius) const
{
	if (radius > 16)
		radius = 16;

	if (view_->scale().x > 1.0)
		return radius / view_->scale().x;
	else
		return radius;
}

// -----------------------------------------------------------------------------
// Returns true if the current visibility info is valid
// -----------------------------------------------------------------------------
bool MapRenderer2D::visOK() const
{
	return map_->nSectors() == vis_s_.size();
}

// -----------------------------------------------------------------------------
// Updates the thing paths cache
// -----------------------------------------------------------------------------
void MapRenderer2D::updateThingPaths(const vector<MapThing*>& things)
{
	thing_paths_.clear();

	// Find things that need to be pathed
	for (unsigned a = 0; a < things.size(); ++a)
	{
		auto      thing = things[a];
		ThingPath path;
		path.from_index = 0;
		path.to_index   = 0;

		auto& tt = game::configuration().thingType(thing->type());

		// Dragon Path
		if (tt.flags() & game::ThingType::Flags::Dragon)
		{
			auto first = map_->things().firstWithId(thing->id());
			if (first)
			{
				path.from_index = thing->index();
				path.to_index   = first->index();
				path.type       = PathType::Dragon;
				thing_paths_.push_back(path);

				vector<MapThing*> dragon_things;
				dragon_things.clear();
				map_->putDragonTargets(first, dragon_things);
				for (unsigned d = 0; d < dragon_things.size(); ++d)
				{
					int   id1 = dragon_things[d]->id();
					int   a11 = dragon_things[d]->arg(0);
					int   a12 = dragon_things[d]->arg(1);
					int   a13 = dragon_things[d]->arg(2);
					int   a14 = dragon_things[d]->arg(3);
					int   a15 = dragon_things[d]->arg(4);
					auto& tt1 = game::configuration().thingType(dragon_things[d]->type());
					for (unsigned e = d + 1; e < dragon_things.size(); ++e)
					{
						int   id2   = dragon_things[e]->id();
						int   a21   = dragon_things[e]->arg(0);
						int   a22   = dragon_things[e]->arg(1);
						int   a23   = dragon_things[e]->arg(2);
						int   a24   = dragon_things[e]->arg(3);
						int   a25   = dragon_things[e]->arg(4);
						auto& tt2   = game::configuration().thingType(dragon_things[e]->type());
						bool  l1to2 = ((a11 == id2) || (a12 == id2) || (a13 == id2) || (a14 == id2) || (a15 == id2));
						bool  l2to1 = ((a21 == id1) || (a22 == id1) || (a23 == id1) || (a24 == id1) || (a25 == id1));
						if (!((tt1.flags() | tt2.flags()) & game::ThingType::Flags::Dragon))
						{
							ThingPath dpath;
							if (l1to2)
							{
								dpath.from_index = dragon_things[e]->index();
								dpath.to_index   = dragon_things[d]->index();
								dpath.type       = l2to1 ? PathType::DragonBoth : PathType::Dragon;
							}
							else if (l2to1)
							{
								dpath.from_index = dragon_things[d]->index();
								dpath.to_index   = dragon_things[e]->index();
								dpath.type       = PathType::Dragon;
							}
							thing_paths_.push_back(dpath);
						}
					}
				}
			}
			continue;
		}

		// Normal Path
		int tid = -1, tid2 = -1;
		int nexttype = tt.nextType();
		int nextargs = tt.nextArgs();
		if (nextargs)
		{
			int pos = nextargs % 10;
			tid     = thing->arg(pos - 1);
		}
		if (nextargs >= 10)
		{
			int pos = nextargs / 10;
			tid += (256 * thing->arg(pos - 1));
		}
		for (unsigned b = a + 1; b < things.size(); ++b)
		{
			auto thing2 = things[b];
			if (thing2->type() == nexttype)
			{
				auto& tt2 = game::configuration().thingType(thing2->type());
				nextargs  = tt2.nextArgs();
				if (nextargs)
				{
					int pos = nextargs % 10;
					tid2    = thing2->arg(pos - 1);
				}
				if (nextargs >= 10)
				{
					int pos = nextargs / 10;
					tid2 += (256 * thing2->arg(pos - 1));
				}
				if (thing2->id() == tid)
				{
					path.from_index = thing->index();
					path.to_index   = thing2->index();
					path.type       = (tid2 == thing->id()) ? PathType::NormalBoth : PathType::Normal;
				}
				else if (thing->id() == tid2)
				{
					path.from_index = thing2->index();
					path.to_index   = thing->index();
					path.type       = PathType::Normal;
				}
				thing_paths_.push_back(path);
			}
		}
	}

	thing_paths_updated_ = app::runTimer();
}
