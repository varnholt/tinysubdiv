// The testbed's built-in cages must be valid, consistently oriented meshes.
#include "shapes.hpp"

#include <map>
#include <print>

int main()
{
   int failures = 0;
   for (const auto& [name, cage] : testbed::builtin_shapes())
   {
      tinysubdiv::validate(cage);

      // every directed edge may appear once; its reverse belongs to the neighbour
      std::map<std::pair<std::uint32_t, std::uint32_t>, int> directed;
      for (const auto& t : cage.triangles)
      {
         for (std::size_t k = 0; k < 3; ++k)
         {
            ++directed[{t[k], t[(k + 1) % 3]}];
         }
      }

      int boundary = 0;
      bool consistent = true;
      for (const auto& [edge, count] : directed)
      {
         consistent = consistent && count == 1;
         boundary += directed.contains({edge.second, edge.first}) ? 0 : 1;
      }

      const int expected_boundary = name == "open box" ? 4 : 0;
      if (!consistent || boundary != expected_boundary)
      {
         ++failures;
         std::println(stderr, "{}: consistent={} boundary edges={} (expected {})", name, consistent, boundary,
                      expected_boundary);
      }

      // outward orientation: the signed volume of a closed mesh is positive
      if (boundary == 0)
      {
         float volume = 0.0f;
         for (const auto& [a, b, c] : cage.triangles)
         {
            volume += tinysubdiv::dot(cage.positions[a], tinysubdiv::cross(cage.positions[b], cage.positions[c]));
         }
         if (volume <= 0.0f)
         {
            ++failures;
            std::println(stderr, "{}: faces point inward (volume {})", name, volume / 6.0f);
         }
      }
   }

   if (failures == 0)
   {
      std::println("all shapes valid");
   }
   return failures == 0 ? 0 : 1;
}
