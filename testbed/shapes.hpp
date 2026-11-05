// Built-in control cages, so the testbed works without files (and in the browser).
#pragma once

#include <tinysubdiv/tinysubdiv.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

namespace testbed
{

struct Shape
{
   std::string name;
   tinysubdiv::Mesh cage;
};

// quads as two triangles, counter-clockwise when seen from outside
inline void add_quad(tinysubdiv::Mesh& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
   mesh.triangles.push_back({a, b, c});
   mesh.triangles.push_back({a, c, d});
}

inline tinysubdiv::Mesh tetrahedron()
{
   return {{{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}}, {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}}};
}

inline tinysubdiv::Mesh cube(bool open_top = false)
{
   tinysubdiv::Mesh mesh{
       {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1}}, {}};
   add_quad(mesh, 0, 1, 2, 3);  // bottom
   if (!open_top)
   {
      add_quad(mesh, 4, 7, 6, 5);  // top
   }
   add_quad(mesh, 3, 2, 6, 7);  // front
   add_quad(mesh, 1, 0, 4, 5);  // back
   add_quad(mesh, 0, 3, 7, 4);  // left
   add_quad(mesh, 2, 1, 5, 6);  // right
   return mesh;
}

inline tinysubdiv::Mesh torus(std::uint32_t rings = 8, std::uint32_t sides = 4, float major = 1.2f, float minor = 0.5f)
{
   tinysubdiv::Mesh mesh;
   for (std::uint32_t r = 0; r < rings; ++r)
   {
      const float u = 2.0f * std::numbers::pi_v<float> * static_cast<float>(r) / static_cast<float>(rings);
      for (std::uint32_t s = 0; s < sides; ++s)
      {
         const float v = 2.0f * std::numbers::pi_v<float> * (static_cast<float>(s) + 0.5f) / static_cast<float>(sides);
         const float radius = major + minor * std::cos(v);
         mesh.positions.push_back({radius * std::cos(u), minor * std::sin(v), radius * std::sin(u)});
      }
   }
   for (std::uint32_t r = 0; r < rings; ++r)
   {
      for (std::uint32_t s = 0; s < sides; ++s)
      {
         const auto at = [&](std::uint32_t ri, std::uint32_t si)
         {
            return (ri % rings) * sides + si % sides;
         };
         add_quad(mesh, at(r, s), at(r, s + 1), at(r + 1, s + 1), at(r + 1, s));
      }
   }
   return mesh;
}

// a capital "T" extruded along z: flat sides and corners show how strongly
// Loop rounds a box-like cage
inline tinysubdiv::Mesh letter_t()
{
   // outline, clockwise seen from +z; the extra points on the top edge give the
   // bar's quads the same corners as the stem, so every edge is shared
   const std::vector<std::array<float, 2>> outline{{-1.2f, 1.0f}, {-0.3f, 1.0f}, {0.3f, 1.0f},  {1.2f, 1.0f},
                                                   {1.2f, 0.4f},  {0.3f, 0.4f},  {0.3f, -1.2f}, {-0.3f, -1.2f},
                                                   {-0.3f, 0.4f}, {-1.2f, 0.4f}};
   tinysubdiv::Mesh mesh;
   const auto n = static_cast<std::uint32_t>(outline.size());
   for (const float z : {0.3f, -0.3f})
   {
      for (const auto& [x, y] : outline)
      {
         mesh.positions.push_back({x, y, z});
      }
   }

   const std::array<std::array<std::uint32_t, 4>, 4> cap{{{9, 8, 1, 0}, {8, 5, 2, 1}, {5, 4, 3, 2}, {7, 6, 5, 8}}};
   for (const auto& [a, b, c, d] : cap)
   {
      add_quad(mesh, a, b, c, d);                  // front, counter-clockwise from +z
      add_quad(mesh, d + n, c + n, b + n, a + n);  // back, reversed
   }

   for (std::uint32_t i = 0; i < n; ++i)
   {
      const std::uint32_t j = (i + 1) % n;
      add_quad(mesh, i, j, j + n, i + n);
   }

   return mesh;
}

inline std::vector<Shape> builtin_shapes()
{
   return {{"cube", cube()},
           {"open box", cube(true)},
           {"tetrahedron", tetrahedron()},
           {"torus", torus()},
           {"letter T", letter_t()}};
}

}  // namespace testbed
