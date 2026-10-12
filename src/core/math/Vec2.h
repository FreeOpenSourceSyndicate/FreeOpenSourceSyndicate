/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cmath>

namespace foss::core
{

// Engine-agnostic 2D vector. Never use engine types in core code.
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

// Integer tile coordinate.
struct TilePos
{
    int x = 0;
    int y = 0;
};

inline bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Vec2& a, const Vec2& b) { return !(a == b); }

inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(const Vec2& a, float s) { return Vec2{a.x * s, a.y * s}; }
inline Vec2 operator*(float s, const Vec2& a) { return Vec2{a.x * s, a.y * s}; }

inline bool operator==(const TilePos& a, const TilePos& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const TilePos& a, const TilePos& b) { return !(a == b); }

inline float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// Returns a unit vector, or (0, 0) when the input has zero length.
inline Vec2 normalized(const Vec2& v)
{
    const float len = length(v);
    if (len <= 0.0f)
    {
        return Vec2{0.0f, 0.0f};
    }
    return Vec2{v.x / len, v.y / len};
}

} // namespace foss::core
