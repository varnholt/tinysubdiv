// tinysubdiv.hpp - Loop subdivision for triangle meshes, C++23, single header
//
// https://github.com/varnholt/tinysubdiv
//
// Usage:
//    tinysubdiv::Mesh mesh{positions, triangles};
//    const auto smooth = tinysubdiv::loop(tinysubdiv::weld(mesh), 3);
//
// Rules (Loop 1987, with the usual crease extension):
// - an edge shared by exactly two triangles is smooth, any other edge
//   (open boundary or non-manifold) is sharp
// - new edge point: 3/8 (a + b) + 1/8 (c + d) on smooth edges, (a + b) / 2 on sharp edges
// - old vertex with no sharp edges: Loop's weights over its n neighbours
// - old vertex with two sharp edges: 3/4 v + 1/8 (s1 + s2), so boundaries stay curves
// - old vertex with one or more than two sharp edges: corner, keeps its position
//
// Each level turns V vertices, E edges and F triangles into V + E vertices and
// 4F triangles. The first V output vertices are the moved input vertices, the
// rest are edge points. Triangle orientation is preserved.
//
// Subdivision works on positions only. Meshes loaded from files often duplicate
// a vertex along UV or normal seams; run weld() first, otherwise every seam is
// treated as an open boundary and the surface tears apart along it.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#define TINYSUBDIV_VERSION_MAJOR 1
#define TINYSUBDIV_VERSION_MINOR 0
#define TINYSUBDIV_VERSION_PATCH 0

namespace tinysubdiv
{

struct Vec3
{
   float x = 0.0f;
   float y = 0.0f;
   float z = 0.0f;

   [[nodiscard]] friend constexpr Vec3 operator+(const Vec3& a, const Vec3& b) noexcept
   {
      return {a.x + b.x, a.y + b.y, a.z + b.z};
   }

   [[nodiscard]] friend constexpr Vec3 operator-(const Vec3& a, const Vec3& b) noexcept
   {
      return {a.x - b.x, a.y - b.y, a.z - b.z};
   }

   [[nodiscard]] friend constexpr Vec3 operator*(const Vec3& a, float s) noexcept
   {
      return {a.x * s, a.y * s, a.z * s};
   }

   [[nodiscard]] friend constexpr bool operator==(const Vec3&, const Vec3&) noexcept = default;
};

[[nodiscard]] constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept
{
   return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] constexpr float dot(const Vec3& a, const Vec3& b) noexcept
{
   return a.x * b.x + a.y * b.y + a.z * b.z;
}

using Triangle = std::array<std::uint32_t, 3>;

struct Mesh
{
   std::vector<Vec3> positions;
   std::vector<Triangle> triangles;
};

namespace detail
{

[[nodiscard]] constexpr std::uint64_t edge_key(std::uint32_t a, std::uint32_t b) noexcept
{
   const auto [lo, hi] = std::minmax(a, b);
   return (std::uint64_t{lo} << 32) | hi;
}

struct Edge
{
   std::uint32_t a;
   std::uint32_t b;
   std::array<std::uint32_t, 2> opposite;  // apex of the first two triangles using this edge
   std::uint32_t triangle_count;

   [[nodiscard]] constexpr bool sharp() const noexcept
   {
      return triangle_count != 2;
   }
};

// unique edges of a mesh, plus for each triangle the edge index of (v0,v1), (v1,v2), (v2,v0)
struct Topology
{
   std::vector<Edge> edges;
   std::vector<std::array<std::uint32_t, 3>> triangle_edges;
};

[[nodiscard]] inline Topology build_topology(const Mesh& mesh)
{
   Topology topology;
   topology.edges.reserve(mesh.triangles.size() * 3 / 2 + 1);
   topology.triangle_edges.reserve(mesh.triangles.size());

   std::unordered_map<std::uint64_t, std::uint32_t> lookup;
   lookup.reserve(mesh.triangles.size() * 2);

   for (const auto& triangle : mesh.triangles)
   {
      std::array<std::uint32_t, 3> indices{};
      for (std::size_t k = 0; k < 3; ++k)
      {
         const std::uint32_t a = triangle[k];
         const std::uint32_t b = triangle[(k + 1) % 3];
         const std::uint32_t apex = triangle[(k + 2) % 3];

         const auto [it, inserted] =
             lookup.try_emplace(edge_key(a, b), static_cast<std::uint32_t>(topology.edges.size()));
         if (inserted)
         {
            topology.edges.push_back({a, b, {apex, apex}, 1});
         }
         else
         {
            auto& edge = topology.edges[it->second];
            if (edge.triangle_count == 1)
            {
               edge.opposite[1] = apex;
            }
            ++edge.triangle_count;
         }
         indices[k] = it->second;
      }
      topology.triangle_edges.push_back(indices);
   }

   return topology;
}

// Loop's original vertex weight for valence n
[[nodiscard]] inline float loop_beta(std::size_t n) noexcept
{
   const double valence = static_cast<double>(n);
   const double c = 0.375 + 0.25 * std::cos(2.0 * std::numbers::pi / valence);
   return static_cast<float>((0.625 - c * c) / valence);
}

[[nodiscard]] inline Mesh loop_once(const Mesh& mesh)
{
   const auto vertex_count = mesh.positions.size();
   const auto topology = build_topology(mesh);
   const auto& edges = topology.edges;

   // per-vertex neighbour sums and sharp-edge bookkeeping
   std::vector<Vec3> neighbour_sum(vertex_count, Vec3{});
   std::vector<std::uint32_t> valence(vertex_count, 0);
   std::vector<Vec3> sharp_sum(vertex_count, Vec3{});
   std::vector<std::uint32_t> sharp_count(vertex_count, 0);

   for (const auto& edge : edges)
   {
      const auto& pa = mesh.positions[edge.a];
      const auto& pb = mesh.positions[edge.b];

      neighbour_sum[edge.a] = neighbour_sum[edge.a] + pb;
      neighbour_sum[edge.b] = neighbour_sum[edge.b] + pa;
      ++valence[edge.a];
      ++valence[edge.b];

      if (edge.sharp())
      {
         sharp_sum[edge.a] = sharp_sum[edge.a] + pb;
         sharp_sum[edge.b] = sharp_sum[edge.b] + pa;
         ++sharp_count[edge.a];
         ++sharp_count[edge.b];
      }
   }

   Mesh result;
   result.positions.reserve(vertex_count + edges.size());
   result.triangles.reserve(mesh.triangles.size() * 4);

   // moved input vertices
   for (std::size_t i = 0; i < vertex_count; ++i)
   {
      const auto& p = mesh.positions[i];

      if (valence[i] == 0 || (sharp_count[i] != 0 && sharp_count[i] != 2))
      {
         result.positions.push_back(p);  // isolated vertex or corner
      }
      else if (sharp_count[i] == 2)
      {
         result.positions.push_back(p * 0.75f + sharp_sum[i] * 0.125f);
      }
      else
      {
         const float beta = loop_beta(valence[i]);
         result.positions.push_back(p * (1.0f - static_cast<float>(valence[i]) * beta) + neighbour_sum[i] * beta);
      }
   }

   // edge points
   for (const auto& edge : edges)
   {
      const auto ab = mesh.positions[edge.a] + mesh.positions[edge.b];
      if (edge.sharp())
      {
         result.positions.push_back(ab * 0.5f);
      }
      else
      {
         const auto cd = mesh.positions[edge.opposite[0]] + mesh.positions[edge.opposite[1]];
         result.positions.push_back(ab * 0.375f + cd * 0.125f);
      }
   }

   // each triangle becomes three corner triangles and one centre triangle
   const auto first_edge_point = static_cast<std::uint32_t>(vertex_count);
   for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
   {
      const auto& [v0, v1, v2] = mesh.triangles[t];
      const std::uint32_t e01 = first_edge_point + topology.triangle_edges[t][0];
      const std::uint32_t e12 = first_edge_point + topology.triangle_edges[t][1];
      const std::uint32_t e20 = first_edge_point + topology.triangle_edges[t][2];

      result.triangles.push_back({v0, e01, e20});
      result.triangles.push_back({v1, e12, e01});
      result.triangles.push_back({v2, e20, e12});
      result.triangles.push_back({e01, e12, e20});
   }

   return result;
}

}  // namespace detail

// Throws std::invalid_argument if a triangle references a missing vertex or
// uses the same vertex twice.
inline void validate(const Mesh& mesh)
{
   const auto vertex_count = mesh.positions.size();
   if (vertex_count > std::numeric_limits<std::uint32_t>::max())
   {
      throw std::invalid_argument("tinysubdiv: too many vertices for 32-bit indices");
   }

   for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
   {
      const auto& [a, b, c] = mesh.triangles[t];
      if (a >= vertex_count || b >= vertex_count || c >= vertex_count)
      {
         throw std::invalid_argument(std::format("tinysubdiv: triangle {} references a missing vertex", t));
      }
      if (a == b || b == c || c == a)
      {
         throw std::invalid_argument(std::format("tinysubdiv: triangle {} is degenerate", t));
      }
   }
}

// Applies `levels` steps of Loop subdivision. Throws std::invalid_argument for an
// invalid mesh (see validate()) or negative levels.
[[nodiscard]] inline Mesh loop(const Mesh& mesh, int levels = 1)
{
   if (levels < 0)
   {
      throw std::invalid_argument("tinysubdiv: levels must not be negative");
   }

   validate(mesh);

   Mesh result = mesh;
   for (int level = 0; level < levels; ++level)
   {
      result = detail::loop_once(result);
   }
   return result;
}

// Merges vertices whose positions are within `epsilon` of each other (exactly
// equal for epsilon == 0), drops triangles that collapse, and removes unused
// vertices. Run this on meshes loaded from files before subdividing.
[[nodiscard]] inline Mesh weld(const Mesh& mesh, float epsilon = 0.0f)
{
   validate(mesh);

   std::vector<std::uint32_t> remap(mesh.positions.size());
   std::vector<Vec3> unique;
   unique.reserve(mesh.positions.size());

   if (epsilon <= 0.0f)
   {
      // exact match on the bit pattern, with -0 folded into +0
      struct Hash
      {
         std::size_t operator()(const Vec3& p) const noexcept
         {
            std::size_t h = 0;
            for (const float f : {p.x, p.y, p.z})
            {
               h = h * 1'000'003u ^ std::hash<float>{}(f == 0.0f ? 0.0f : f);
            }
            return h;
         }
      };

      std::unordered_map<Vec3, std::uint32_t, Hash> lookup;
      lookup.reserve(mesh.positions.size());
      for (std::size_t i = 0; i < mesh.positions.size(); ++i)
      {
         const auto [it, inserted] = lookup.try_emplace(mesh.positions[i], static_cast<std::uint32_t>(unique.size()));
         if (inserted)
         {
            unique.push_back(mesh.positions[i]);
         }
         remap[i] = it->second;
      }
   }
   else
   {
      // grid of cell size epsilon; a match can only be in the 27 surrounding cells
      using Cell = std::array<std::int64_t, 3>;
      struct Hash
      {
         std::size_t operator()(const Cell& c) const noexcept
         {
            return static_cast<std::size_t>(c[0] * 73856093 ^ c[1] * 19349663 ^ c[2] * 83492791);
         }
      };

      const auto cell_of = [epsilon](const Vec3& p)
      {
         return Cell{static_cast<std::int64_t>(std::floor(p.x / epsilon)),
                     static_cast<std::int64_t>(std::floor(p.y / epsilon)),
                     static_cast<std::int64_t>(std::floor(p.z / epsilon))};
      };

      std::unordered_map<Cell, std::vector<std::uint32_t>, Hash> grid;
      const float epsilon_squared = epsilon * epsilon;

      for (std::size_t i = 0; i < mesh.positions.size(); ++i)
      {
         const auto& p = mesh.positions[i];
         const auto cell = cell_of(p);
         std::int64_t found = -1;

         for (std::int64_t dx = -1; dx <= 1 && found < 0; ++dx)
         {
            for (std::int64_t dy = -1; dy <= 1 && found < 0; ++dy)
            {
               for (std::int64_t dz = -1; dz <= 1 && found < 0; ++dz)
               {
                  const auto it = grid.find({cell[0] + dx, cell[1] + dy, cell[2] + dz});
                  if (it == grid.end())
                  {
                     continue;
                  }
                  for (const auto candidate : it->second)
                  {
                     const auto d = unique[candidate] - p;
                     if (dot(d, d) <= epsilon_squared)
                     {
                        found = candidate;
                        break;
                     }
                  }
               }
            }
         }

         if (found < 0)
         {
            found = static_cast<std::int64_t>(unique.size());
            unique.push_back(p);
            grid[cell].push_back(static_cast<std::uint32_t>(found));
         }
         remap[i] = static_cast<std::uint32_t>(found);
      }
   }

   Mesh result;
   result.triangles.reserve(mesh.triangles.size());
   std::vector<std::uint32_t> compact(unique.size(), std::numeric_limits<std::uint32_t>::max());

   for (const auto& triangle : mesh.triangles)
   {
      Triangle welded{remap[triangle[0]], remap[triangle[1]], remap[triangle[2]]};
      if (welded[0] == welded[1] || welded[1] == welded[2] || welded[2] == welded[0])
      {
         continue;
      }
      for (auto& index : welded)
      {
         if (compact[index] == std::numeric_limits<std::uint32_t>::max())
         {
            compact[index] = static_cast<std::uint32_t>(result.positions.size());
            result.positions.push_back(unique[index]);
         }
         index = compact[index];
      }
      result.triangles.push_back(welded);
   }

   return result;
}

// Area-weighted vertex normals, unit length (zero for vertices without area).
[[nodiscard]] inline std::vector<Vec3> vertex_normals(const Mesh& mesh)
{
   validate(mesh);

   std::vector<Vec3> normals(mesh.positions.size(), Vec3{});
   for (const auto& [a, b, c] : mesh.triangles)
   {
      const auto& pa = mesh.positions[a];
      const auto face = cross(mesh.positions[b] - pa, mesh.positions[c] - pa);
      for (const auto index : {a, b, c})
      {
         normals[index] = normals[index] + face;
      }
   }

   for (auto& n : normals)
   {
      const float length = std::sqrt(dot(n, n));
      if (length > 0.0f)
      {
         n = n * (1.0f / length);
      }
   }
   return normals;
}

// Unique edges as index pairs, e.g. for drawing a wireframe.
[[nodiscard]] inline std::vector<std::array<std::uint32_t, 2>> edges(const Mesh& mesh)
{
   validate(mesh);

   const auto topology = detail::build_topology(mesh);
   std::vector<std::array<std::uint32_t, 2>> result;
   result.reserve(topology.edges.size());
   for (const auto& edge : topology.edges)
   {
      result.push_back({edge.a, edge.b});
   }
   return result;
}

}  // namespace tinysubdiv
