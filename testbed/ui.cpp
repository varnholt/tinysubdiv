#include "app.hpp"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>

namespace testbed
{

namespace
{

void text(const std::string& line)
{
   ImGui::TextUnformatted(line.c_str());
}

}  // namespace

void draw_ui(App& app)
{
   ImGui::SetNextWindowPos({12.0f, 12.0f}, ImGuiCond_FirstUseEver);
   ImGui::SetNextWindowBgAlpha(0.85f);
   ImGui::Begin("tinysubdiv", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

   // mesh
   bool rebuild = false;
   if (ImGui::BeginCombo("shape", app.shapes[app.shape].name.c_str()))
   {
      for (std::size_t i = 0; i < app.shapes.size(); ++i)
      {
         const bool selected = i == app.shape;
         if (ImGui::Selectable(app.shapes[i].name.c_str(), selected))
         {
            rebuild = rebuild || i != app.shape;
            app.shape = i;
         }
         if (selected)
         {
            ImGui::SetItemDefaultFocus();
         }
      }
      ImGui::EndCombo();
   }
   rebuild = ImGui::SliderInt("level", &app.level, 0, max_level) || rebuild;
   if (rebuild)
   {
      app.rebuild();
   }

   // display
   ImGui::Checkbox("wireframe", &app.view.wireframe);
   ImGui::SameLine();
   ImGui::Checkbox("cage", &app.view.cage);
   ImGui::ColorEdit3("surface", app.view.surface_color.data(), ImGuiColorEditFlags_NoInputs);
   ImGui::SliderFloat("zoom", &app.view.distance, 1.2f, 20.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
   if (ImGui::Button("reset view"))
   {
      app.reset_view();
   }

#ifndef __EMSCRIPTEN__
   ImGui::SameLine();
   if (ImGui::Button("screenshot"))
   {
      int width = 0;
      int height = 0;
      glfwGetFramebufferSize(app.window, &width, &height);
      auto file = std::format("tinysubdiv_{}_level{}.png", app.shapes[app.shape].name, app.level);
      std::ranges::replace(file, ' ', '_');
      app.status = app.renderer->screenshot(file, width, height, app.view) ? "saved " + file : "could not save " + file;
   }
#endif

   // numbers
   ImGui::SeparatorText("mesh");
   const auto& s = app.stats;
   text(std::format("cage     {:>7} V {:>7} E {:>7} F", s.cage_vertices, s.cage_edges, s.cage_triangles));
   text(std::format("surface  {:>7} V {:>17} F", s.surface_vertices, s.surface_triangles));
   text(std::format("loop()   {:.2f} ms", s.milliseconds));
   if (app.show_fps)
   {
      text(std::format("frame    {:.1f} fps", ImGui::GetIO().Framerate));
   }

   if (!app.status.empty())
   {
      ImGui::TextDisabled("%s", app.status.c_str());
   }

   if (ImGui::CollapsingHeader("controls"))
   {
      text("drag        orbit");
      text("wheel       zoom");
      text("up / down   level");
      text("space       next shape");
      text("w / c       wireframe / cage");
      text("r           reset view");
   }

   ImGui::End();
}

}  // namespace testbed
