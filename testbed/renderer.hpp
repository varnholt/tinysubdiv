#pragma once

#include "gl.hpp"
#include "math.hpp"

#include <tinysubdiv/tinysubdiv.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

namespace testbed
{

// owns one GL object name and frees it with the matching glDelete* call
template <void (*Delete)(GLuint)>
class GlObject
{
public:
   GlObject() = default;
   explicit GlObject(GLuint name) : name_(name)
   {
   }
   GlObject(GlObject&& other) noexcept : name_(std::exchange(other.name_, 0))
   {
   }
   GlObject& operator=(GlObject&& other) noexcept
   {
      std::swap(name_, other.name_);
      return *this;
   }
   GlObject(const GlObject&) = delete;
   GlObject& operator=(const GlObject&) = delete;
   ~GlObject()
   {
      if (name_ != 0)
      {
         Delete(name_);
      }
   }

   [[nodiscard]] GLuint get() const noexcept
   {
      return name_;
   }

private:
   GLuint name_ = 0;
};

void delete_buffer(GLuint name);
void delete_vertex_array(GLuint name);
void delete_program(GLuint name);
void delete_framebuffer(GLuint name);
void delete_renderbuffer(GLuint name);

using Buffer = GlObject<delete_buffer>;
using VertexArray = GlObject<delete_vertex_array>;
using Program = GlObject<delete_program>;
using Framebuffer = GlObject<delete_framebuffer>;
using Renderbuffer = GlObject<delete_renderbuffer>;

struct View
{
   float yaw = 0.6f;
   float pitch = 0.45f;
   float distance = 5.0f;
   bool wireframe = true;
   bool cage = true;
   std::array<float, 3> surface_color{0.86f, 0.52f, 0.28f};
};

class Renderer
{
public:
   Renderer();

   // uploads the control cage and its subdivided surface
   void set_meshes(const tinysubdiv::Mesh& cage, const tinysubdiv::Mesh& surface);

   void draw(int width, int height, const View& view) const;

   // renders one frame offscreen with 4x MSAA and writes it as PNG;
   // `overlay` draws on top of the scene, e.g. the ImGui panel
   bool screenshot(const std::filesystem::path& path, int width, int height, const View& view,
                   const std::function<void()>& overlay = {}) const;

   [[nodiscard]] float radius() const noexcept
   {
      return radius_;
   }

private:
   struct Geometry
   {
      VertexArray vao;
      Buffer vertices;
      Buffer triangles;
      Buffer lines;
      GLsizei triangle_index_count = 0;
      GLsizei line_index_count = 0;
   };

   static Geometry upload(const tinysubdiv::Mesh& mesh, bool with_normals);
   void draw_geometry(const Geometry& geometry, GLenum mode, const Mat4& mvp, const Mat4& model, float r, float g,
                      float b, bool lit) const;

   Program program_;
   GLint mvp_location_ = -1;
   GLint model_location_ = -1;
   GLint color_location_ = -1;
   GLint lit_location_ = -1;

   Geometry cage_;
   Geometry surface_;
   tinysubdiv::Vec3 center_{};
   float radius_ = 1.0f;
};

}  // namespace testbed
