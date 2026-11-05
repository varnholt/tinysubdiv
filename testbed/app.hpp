#pragma once

#include "gl.hpp"
#include "renderer.hpp"
#include "shapes.hpp"

#include <tinysubdiv/tinysubdiv.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace testbed
{

constexpr int max_level = 6;

struct Stats
{
   std::size_t cage_vertices = 0;
   std::size_t cage_edges = 0;
   std::size_t cage_triangles = 0;
   std::size_t surface_vertices = 0;
   std::size_t surface_triangles = 0;
   double milliseconds = 0.0;
};

struct App
{
   GLFWwindow* window = nullptr;
   std::optional<Renderer> renderer;
   std::vector<Shape> shapes;
   std::size_t shape = 0;
   int level = 3;
   View view;
   Stats stats;
   std::string status;  // last message shown in the panel, e.g. a saved screenshot

   bool dragging = false;
   double last_x = 0.0;
   double last_y = 0.0;

   void rebuild()
   {
      const auto& cage = shapes[shape].cage;

      const auto start = std::chrono::steady_clock::now();
      const auto surface = tinysubdiv::loop(cage, level);
      const auto elapsed = std::chrono::steady_clock::now() - start;

      stats = {cage.positions.size(),    tinysubdiv::edges(cage).size(),
               cage.triangles.size(),    surface.positions.size(),
               surface.triangles.size(), std::chrono::duration<double, std::milli>(elapsed).count()};

      renderer->set_meshes(cage, surface);

      const auto title = std::format("tinysubdiv testbed - {} - level {} - {} triangles", shapes[shape].name, level,
                                     stats.surface_triangles);
      glfwSetWindowTitle(window, title.c_str());
   }

   void reset_view()
   {
      view.yaw = 0.6f;
      view.pitch = 0.45f;
      view.distance = 3.2f;
   }
};

// the ImGui control panel
void draw_ui(App& app);

}  // namespace testbed
