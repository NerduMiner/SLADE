#pragma once

#include "MapObjectList.h"

namespace slade
{
class MapVertex;

class VertexList : public MapObjectList<MapVertex>
{
public:
	MapVertex* nearest(const Vec2d& point, double min = 64) const;
	MapVertex* vertexAt(double x, double y) const;
	MapVertex* firstCrossed(const Seg2d& line) const;
};
} // namespace slade
