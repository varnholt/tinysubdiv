// Minimal Wavefront OBJ reading and writing for the examples and the testbed.
// Reads positions and faces (polygons are fanned into triangles, negative
// indices are supported); normals, texture coordinates and groups are ignored.

#pragma once

#include <tinysubdiv/tinysubdiv.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace obj
{

namespace detail
{

inline std::string_view next_token(std::string_view& line)
{
   const auto start = line.find_first_not_of(" \t\r");
   if (start == std::string_view::npos)
   {
      line = {};
      return {};
   }
   line.remove_prefix(start);
   const auto end = std::min(line.find_first_of(" \t\r"), line.size());
   const auto token = line.substr(0, end);
   line.remove_prefix(end);
   return token;
}

template <class T>
bool parse(std::string_view text, T& value)
{
   const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
   return error == std::errc{} && end == text.data() + text.size();
}

}  // namespace detail

inline std::expected<tinysubdiv::Mesh, std::string> read(std::istream& stream)
{
   tinysubdiv::Mesh mesh;
   std::string line_buffer;
   std::size_t line_number = 0;

   while (std::getline(stream, line_buffer))
   {
      ++line_number;
      std::string_view line = line_buffer;
      const auto keyword = detail::next_token(line);

      if (keyword == "v")
      {
         tinysubdiv::Vec3 p;
         for (float* component : {&p.x, &p.y, &p.z})
         {
            if (!detail::parse(detail::next_token(line), *component))
            {
               return std::unexpected(std::format("line {}: bad vertex", line_number));
            }
         }
         mesh.positions.push_back(p);
      }
      else if (keyword == "f")
      {
         std::vector<std::uint32_t> polygon;
         for (auto token = detail::next_token(line); !token.empty(); token = detail::next_token(line))
         {
            std::int64_t index = 0;
            if (!detail::parse(token.substr(0, token.find('/')), index) || index == 0)
            {
               return std::unexpected(std::format("line {}: bad face index '{}'", line_number, token));
            }

            const auto count = static_cast<std::int64_t>(mesh.positions.size());
            const auto resolved = index > 0 ? index - 1 : count + index;
            if (resolved < 0 || resolved >= count)
            {
               return std::unexpected(std::format("line {}: face index {} out of range", line_number, index));
            }
            polygon.push_back(static_cast<std::uint32_t>(resolved));
         }

         for (std::size_t i = 2; i < polygon.size(); ++i)
         {
            mesh.triangles.push_back({polygon[0], polygon[i - 1], polygon[i]});
         }
      }
   }

   return mesh;
}

inline std::expected<tinysubdiv::Mesh, std::string> read(const std::filesystem::path& path)
{
   std::ifstream stream(path);
   if (!stream)
   {
      return std::unexpected(std::format("cannot open {}", path.string()));
   }
   return read(stream);
}

inline bool write(const std::filesystem::path& path, const tinysubdiv::Mesh& mesh)
{
   std::ofstream stream(path);
   for (const auto& p : mesh.positions)
   {
      stream << std::format("v {} {} {}\n", p.x, p.y, p.z);
   }
   for (const auto& [a, b, c] : mesh.triangles)
   {
      stream << std::format("f {} {} {}\n", a + 1, b + 1, c + 1);
   }
   return static_cast<bool>(stream);
}

}  // namespace obj
