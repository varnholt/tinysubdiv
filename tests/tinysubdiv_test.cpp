#include <tinysubdiv/tinysubdiv.hpp>

#include <cmath>
#include <map>
#include <print>
#include <source_location>
#include <string_view>

using tinysubdiv::Mesh;
using tinysubdiv::Vec3;

namespace
{

int failures = 0;

void check(bool ok, std::string_view what, const std::source_location& where = std::source_location::current())
{
   if (!ok)
   {
      ++failures;
      std::println(stderr, "{}:{}: FAILED: {}", where.file_name(), where.line(), what);
   }
}

bool near(const Vec3& a, const Vec3& b, float eps = 1e-5f)
{
   return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
}

// how many triangles use each edge
std::map<std::pair<std::uint32_t, std::uint32_t>, int> edge_use(const Mesh& mesh)
{
   std::map<std::pair<std::uint32_t, std::uint32_t>, int> use;
   for (const auto& t : mesh.triangles)
   {
      for (std::size_t k = 0; k < 3; ++k)
      {
         const auto [lo, hi] = std::minmax(t[k], t[(k + 1) % 3]);
         ++use[{lo, hi}];
      }
   }
   return use;
}

bool closed(const Mesh& mesh)
{
   for (const auto& [edge, count] : edge_use(mesh))
   {
      if (count != 2)
      {
         return false;
      }
   }
   return true;
}

long euler(const Mesh& mesh)
{
   return static_cast<long>(mesh.positions.size()) - static_cast<long>(edge_use(mesh).size()) +
          static_cast<long>(mesh.triangles.size());
}

Mesh tetrahedron()
{
   return {{{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}}, {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}}};
}

Mesh cube()
{
   return {{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},
           {{0, 2, 1},
            {0, 3, 2},
            {4, 5, 6},
            {4, 6, 7},
            {0, 1, 5},
            {0, 5, 4},
            {2, 3, 7},
            {2, 7, 6},
            {1, 2, 6},
            {1, 6, 5},
            {0, 4, 7},
            {0, 7, 3}}};
}

// n x n quads in the z = 0 plane, two triangles each
Mesh grid(std::uint32_t n)
{
   Mesh mesh;
   for (std::uint32_t y = 0; y <= n; ++y)
   {
      for (std::uint32_t x = 0; x <= n; ++x)
      {
         mesh.positions.push_back({static_cast<float>(x), static_cast<float>(y), 0.0f});
      }
   }
   for (std::uint32_t y = 0; y < n; ++y)
   {
      for (std::uint32_t x = 0; x < n; ++x)
      {
         const std::uint32_t i = y * (n + 1) + x;
         mesh.triangles.push_back({i, i + 1, i + n + 2});
         mesh.triangles.push_back({i, i + n + 2, i + n + 1});
      }
   }
   return mesh;
}

void test_counts_and_topology()
{
   const auto cube_mesh = cube();
   check(closed(cube_mesh) && euler(cube_mesh) == 2, "cube input is a closed sphere");

   const auto one = tinysubdiv::loop(cube_mesh);
   check(one.positions.size() == 8 + 18, "V' = V + E");
   check(one.triangles.size() == 12 * 4, "F' = 4F");
   check(closed(one), "closed mesh stays closed");
   check(euler(one) == 2, "Euler characteristic is preserved");

   const auto three = tinysubdiv::loop(cube_mesh, 3);
   check(three.triangles.size() == 12 * 64, "three levels give 64x the triangles");
   check(closed(three) && euler(three) == 2, "closed after three levels");

   check(tinysubdiv::loop(cube_mesh, 0).triangles == cube_mesh.triangles, "level 0 is a copy");
}

void test_symmetry_and_shrinking()
{
   // regular tetrahedron centred at the origin: every vertex is equivalent,
   // so all moved vertices keep the same distance and the centroid stays put
   const auto result = tinysubdiv::loop(tetrahedron(), 2);
   Vec3 centroid{};
   for (const auto& p : result.positions)
   {
      centroid = centroid + p;
   }
   centroid = centroid * (1.0f / static_cast<float>(result.positions.size()));
   check(near(centroid, {}), "centroid stays at the origin");

   const auto radius = [](const Vec3& p)
   {
      return std::sqrt(tinysubdiv::dot(p, p));
   };
   const float r0 = radius(result.positions[0]);
   for (std::size_t i = 1; i < 4; ++i)
   {
      check(std::abs(radius(result.positions[i]) - r0) < 1e-5f, "original vertices move symmetrically");
   }
   check(r0 < std::sqrt(3.0f), "Loop subdivision shrinks a convex cage");
}

void test_flat_grid_stays_flat()
{
   const auto result = tinysubdiv::loop(grid(4), 3);
   bool flat = true;
   for (const auto& p : result.positions)
   {
      flat = flat && p.z == 0.0f;
   }
   check(flat, "planar mesh stays planar");

   // a vertex in the middle of a straight boundary stays where it is
   check(near(result.positions[2], Vec3{2, 0, 0}), "straight boundary stays straight");
}

void test_boundary_rules()
{
   // a single triangle: all edges are open, all vertices have two sharp edges
   const Mesh triangle{{{0, 0, 0}, {4, 0, 0}, {0, 4, 0}}, {{0, 1, 2}}};
   const auto result = tinysubdiv::loop(triangle);

   check(near(result.positions[0], {0.5f, 0.5f, 0}), "boundary vertex: 3/4 v + 1/8 (a + b)");
   check(near(result.positions[1], {3.0f, 0.5f, 0}), "boundary vertex: 3/4 v + 1/8 (a + b)");

   // edge points of open edges are the midpoints
   bool midpoints = true;
   for (std::size_t i = 3; i < result.positions.size(); ++i)
   {
      const auto& p = result.positions[i];
      midpoints = midpoints && (near(p, {2, 0, 0}) || near(p, {2, 2, 0}) || near(p, {0, 2, 0}));
   }
   check(midpoints, "open edges split at the midpoint");
}

void test_interior_rules()
{
   // regular valence-6 vertex in the middle of a hexagon fan, lifted by 1
   Mesh fan;
   fan.positions.push_back({0, 0, 1});
   for (int i = 0; i < 6; ++i)
   {
      const float a = static_cast<float>(i) * std::numbers::pi_v<float> / 3.0f;
      fan.positions.push_back({std::cos(a), std::sin(a), 0});
   }
   for (std::uint32_t i = 0; i < 6; ++i)
   {
      fan.triangles.push_back({0, 1 + i, 1 + (i + 1) % 6});
   }

   const auto result = tinysubdiv::loop(fan);
   // valence 6: beta = 1/16, so the centre moves to (1 - 6/16) * 1 = 0.625
   check(std::abs(result.positions[0].z - 0.625f) < 1e-6f, "valence-6 vertex uses beta = 1/16");

   // the spoke from the centre to vertex 1 is smooth: 3/8 (c + v1) + 1/8 (v2 + v6)
   const Vec3 expected =
       (fan.positions[0] + fan.positions[1]) * 0.375f + (fan.positions[2] + fan.positions[6]) * 0.125f;
   bool found = false;
   for (std::size_t i = fan.positions.size(); i < result.positions.size(); ++i)
   {
      found = found || near(result.positions[i], expected);
   }
   check(found, "smooth edge point uses 3/8 and 1/8 weights");
}

void test_weld_fixes_seams()
{
   // the cube with every face using its own four vertices, like an OBJ export
   // with per-face normals; without welding every edge is a boundary
   const auto source = cube();
   Mesh split;
   for (const auto& t : source.triangles)
   {
      const auto base = static_cast<std::uint32_t>(split.positions.size());
      for (const auto i : t)
      {
         split.positions.push_back(source.positions[i]);
      }
      split.triangles.push_back({base, base + 1, base + 2});
   }

   check(!closed(split), "split cube is open");

   const auto welded = tinysubdiv::weld(split);
   check(welded.positions.size() == 8, "weld merges duplicates");
   check(closed(welded), "welded cube is closed");

   const auto a = tinysubdiv::loop(welded, 2);
   const auto b = tinysubdiv::loop(source, 2);
   check(a.triangles.size() == b.triangles.size(), "welded and indexed cube subdivide alike");
   check(closed(a), "no tears after welding");

   // near-duplicates within epsilon
   auto noisy = split;
   for (std::size_t i = 0; i < noisy.positions.size(); ++i)
   {
      noisy.positions[i].x += static_cast<float>(i % 3) * 1e-4f;
   }
   check(tinysubdiv::weld(noisy, 1e-3f).positions.size() == 8, "weld with epsilon merges near-duplicates");
   check(tinysubdiv::weld(noisy).positions.size() > 8, "exact weld keeps near-duplicates apart");
}

void test_normals_and_edges()
{
   const auto normals = tinysubdiv::vertex_normals(cube());
   const auto& n = normals[6];  // corner (1, 1, 1)
   check(n.x > 0 && n.y > 0 && n.z > 0, "corner normal points outward");
   check(std::abs(tinysubdiv::dot(n, n) - 1.0f) < 1e-5f, "normals have unit length");
   check(tinysubdiv::edges(cube()).size() == 18, "cube has 18 edges");
}

void test_validation()
{
   const auto throws = [](auto&& f)
   {
      try
      {
         f();
      }
      catch (const std::invalid_argument&)
      {
         return true;
      }
      return false;
   };

   check(throws(
             []
             {
                (void)tinysubdiv::loop({{{0, 0, 0}}, {{0, 1, 2}}});
             }),
         "missing vertex throws");
   check(throws(
             []
             {
                (void)tinysubdiv::loop({{{0, 0, 0}, {1, 0, 0}}, {{0, 0, 1}}});
             }),
         "degenerate triangle throws");
   check(throws(
             []
             {
                (void)tinysubdiv::loop(cube(), -1);
             }),
         "negative levels throw");
   check(tinysubdiv::loop(Mesh{}).positions.empty(), "empty mesh is fine");
}

}  // namespace

int main()
{
   test_counts_and_topology();
   test_symmetry_and_shrinking();
   test_flat_grid_stays_flat();
   test_boundary_rules();
   test_interior_rules();
   test_weld_fixes_seams();
   test_normals_and_edges();
   test_validation();

   if (failures > 0)
   {
      std::println(stderr, "{} check(s) failed", failures);
      return 1;
   }

   std::println("all checks passed");
   return 0;
}
