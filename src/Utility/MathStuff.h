#pragma once

namespace slade::math
{
constexpr double PI = 3.1415926535897932384;

double clamp(double val, double min, double max);
int    floor(double val);
int    ceil(double val);
int    round(double val);
double distance(const Vec2d& p1, const Vec2d& p2);
double distance3d(const Vec3d& p1, const Vec3d& p2);
double lineSide(const Vec2d& point, const Seg2d& line);
Vec2d  closestPointOnLine(const Vec2d& point, const Seg2d& line);
double distanceToLine(const Vec2d& point, const Seg2d& line);
double distanceToLineFast(const Vec2d& point, const Seg2d& line);
bool   linesIntersect(const Seg2d& line1, const Seg2d& line2, Vec2d& out);
double distanceRayLine(const Vec2d& ray_origin, const Vec2d& ray_dir, const Vec2d& seg1, const Vec2d& seg2);
double angle2DRad(const Vec2d& p1, const Vec2d& p2, const Vec2d& p3);
Vec2d  rotatePoint(const Vec2d& origin, const Vec2d& point, double angle);
Vec3d  rotateVector3D(const Vec3d& vector, const Vec3d& axis, double angle);
double degToRad(double angle);
double radToDeg(double angle);
Vec2d  vectorAngle(double angle_rad);
double distanceRayPlane(const Vec3d& ray_origin, const Vec3d& ray_dir, const Plane& plane);
bool   boxLineIntersect(const Rectf& box, const Seg2d& line);
Plane  planeFromTriangle(const Vec3d& p1, const Vec3d& p2, const Vec3d& p3);
bool   colinear(double x1, double y1, double x2, double y2, double x3, double y3);

template<typename T> T scale(T value, double scale)
{
	return static_cast<T>(static_cast<double>(value) * scale);
}

template<typename T> T scaleInverse(T value, double scale)
{
	if (scale != 0.0)
		return static_cast<T>(static_cast<double>(value) / scale);

	return 0.0;
}

} // namespace slade::math
