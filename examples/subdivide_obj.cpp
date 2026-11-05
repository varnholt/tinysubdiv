// Subdivides an OBJ file.
//
//   tinysubdiv_obj input.obj output.obj [levels=2] [weld-epsilon=0]

#include "obj.hpp"

#include <tinysubdiv/tinysubdiv.hpp>

#include <charconv>
#include <exception>
#include <print>
#include <span>
#include <string_view>

int main(int argc, char* argv[])
{
   const std::span args(argv, static_cast<std::size_t>(argc));
   if (args.size() < 3 || args.size() > 5)
   {
      std::println(stderr, "usage: {} input.obj output.obj [levels=2] [weld-epsilon=0]", args[0]);
      return 2;
   }

   int levels = 2;
   float epsilon = 0.0f;
   if (args.size() > 3 && !obj::detail::parse(std::string_view{args[3]}, levels))
   {
      std::println(stderr, "invalid level count: {}", args[3]);
      return 2;
   }
   if (args.size() > 4 && !obj::detail::parse(std::string_view{args[4]}, epsilon))
   {
      std::println(stderr, "invalid weld epsilon: {}", args[4]);
      return 2;
   }

   const auto input = obj::read(args[1]);
   if (!input)
   {
      std::println(stderr, "{}", input.error());
      return 1;
   }

   try
   {
      const auto welded = tinysubdiv::weld(*input, epsilon);
      const auto result = tinysubdiv::loop(welded, levels);

      if (!obj::write(args[2], result))
      {
         std::println(stderr, "cannot write {}", args[2]);
         return 1;
      }

      std::println("{} vertices -> {} welded -> {} vertices, {} triangles", input->positions.size(),
                   welded.positions.size(), result.positions.size(), result.triangles.size());
   }
   catch (const std::exception& e)
   {
      std::println(stderr, "{}", e.what());
      return 1;
   }

   return 0;
}
