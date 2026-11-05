#include "renderer.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <print>
#include <stdexcept>
#include <string>

namespace testbed
{

void delete_buffer(GLuint name)
{
   glDeleteBuffers(1, &name);
}

void delete_vertex_array(GLuint name)
{
   glDeleteVertexArrays(1, &name);
}

void delete_program(GLuint name)
{
   glDeleteProgram(name);
}

void delete_framebuffer(GLuint name)
{
   glDeleteFramebuffers(1, &name);
}

void delete_renderbuffer(GLuint name)
{
   glDeleteRenderbuffers(1, &name);
}

namespace
{

constexpr const char* vertex_shader = R"(#version 300 es
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
uniform mat4 u_mvp;
uniform mat4 u_model;
out vec3 v_normal;
void main()
{
   v_normal = mat3(u_model) * a_normal;
   gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

constexpr const char* fragment_shader = R"(#version 300 es
precision mediump float;
in vec3 v_normal;
uniform vec3 u_color;
uniform bool u_lit;
out vec4 o_color;
void main()
{
   if (!u_lit)
   {
      o_color = vec4(u_color, 1.0);
      return;
   }
   // two-sided, so open meshes look right from inside too
   vec3 n = normalize(gl_FrontFacing ? v_normal : -v_normal);
   vec3 l = normalize(vec3(0.4, 0.7, 0.6));
   float diffuse = max(dot(n, l), 0.0);
   float specular = pow(max(dot(n, normalize(l + vec3(0.0, 0.0, 1.0))), 0.0), 48.0);
   float rim = pow(1.0 - max(n.z, 0.0), 3.0) * 0.25;
   o_color = vec4(u_color * (0.22 + 0.78 * diffuse) + vec3(0.35 * specular + rim), 1.0);
}
)";

GLuint compile(GLenum type, const char* source)
{
   const GLuint shader = glCreateShader(type);
   glShaderSource(shader, 1, &source, nullptr);
   glCompileShader(shader);

   GLint ok = 0;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
   if (ok == 0)
   {
      std::string log(1024, '\0');
      glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
      glDeleteShader(shader);
      throw std::runtime_error("shader: " + log);
   }
   return shader;
}

}  // namespace

Renderer::Renderer()
{
   const GLuint vs = compile(GL_VERTEX_SHADER, vertex_shader);
   const GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_shader);

   program_ = Program{glCreateProgram()};
   glAttachShader(program_.get(), vs);
   glAttachShader(program_.get(), fs);
   glLinkProgram(program_.get());
   glDeleteShader(vs);
   glDeleteShader(fs);

   GLint ok = 0;
   glGetProgramiv(program_.get(), GL_LINK_STATUS, &ok);
   if (ok == 0)
   {
      throw std::runtime_error("shader program failed to link");
   }

   mvp_location_ = glGetUniformLocation(program_.get(), "u_mvp");
   model_location_ = glGetUniformLocation(program_.get(), "u_model");
   color_location_ = glGetUniformLocation(program_.get(), "u_color");
   lit_location_ = glGetUniformLocation(program_.get(), "u_lit");
}

Renderer::Geometry Renderer::upload(const tinysubdiv::Mesh& mesh, bool with_normals)
{
   Geometry geometry;
   GLuint name = 0;

   glGenVertexArrays(1, &name);
   geometry.vao = VertexArray{name};
   glBindVertexArray(name);

   // interleaved position + normal
   const auto normals = with_normals ? tinysubdiv::vertex_normals(mesh) : std::vector<tinysubdiv::Vec3>{};
   std::vector<tinysubdiv::Vec3> interleaved;
   interleaved.reserve(mesh.positions.size() * 2);
   for (std::size_t i = 0; i < mesh.positions.size(); ++i)
   {
      interleaved.push_back(mesh.positions[i]);
      interleaved.push_back(with_normals ? normals[i] : tinysubdiv::Vec3{});
   }

   glGenBuffers(1, &name);
   geometry.vertices = Buffer{name};
   glBindBuffer(GL_ARRAY_BUFFER, name);
   glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(tinysubdiv::Vec3)),
                interleaved.data(), GL_STATIC_DRAW);

   constexpr auto stride = static_cast<GLsizei>(2 * sizeof(tinysubdiv::Vec3));
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(sizeof(tinysubdiv::Vec3)));

   glGenBuffers(1, &name);
   geometry.triangles = Buffer{name};
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, name);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.triangles.size() * sizeof(tinysubdiv::Triangle)),
                mesh.triangles.data(), GL_STATIC_DRAW);
   geometry.triangle_index_count = static_cast<GLsizei>(mesh.triangles.size() * 3);

   const auto edges = tinysubdiv::edges(mesh);
   glGenBuffers(1, &name);
   geometry.lines = Buffer{name};
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, name);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(edges.size() * sizeof(edges[0])), edges.data(),
                GL_STATIC_DRAW);
   geometry.line_index_count = static_cast<GLsizei>(edges.size() * 2);

   glBindVertexArray(0);
   return geometry;
}

void Renderer::set_meshes(const tinysubdiv::Mesh& cage, const tinysubdiv::Mesh& surface)
{
   cage_ = upload(cage, false);
   surface_ = upload(surface, true);

   // frame the cage: centre of its bounding box, radius of the farthest vertex
   tinysubdiv::Vec3 lo{1e30f, 1e30f, 1e30f};
   tinysubdiv::Vec3 hi{-1e30f, -1e30f, -1e30f};
   for (const auto& p : cage.positions)
   {
      lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
      hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
   }
   center_ = (lo + hi) * 0.5f;
   radius_ = 1e-3f;
   for (const auto& p : cage.positions)
   {
      const auto d = p - center_;
      radius_ = std::max(radius_, std::sqrt(tinysubdiv::dot(d, d)));
   }
}

void Renderer::draw_geometry(const Geometry& geometry, GLenum mode, const Mat4& mvp, const Mat4& model, float r,
                             float g, float b, bool lit) const
{
   glUniformMatrix4fv(mvp_location_, 1, GL_FALSE, mvp.data());
   glUniformMatrix4fv(model_location_, 1, GL_FALSE, model.data());
   glUniform3f(color_location_, r, g, b);
   glUniform1i(lit_location_, lit ? 1 : 0);

   glBindVertexArray(geometry.vao.get());
   if (mode == GL_TRIANGLES)
   {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry.triangles.get());
      glDrawElements(GL_TRIANGLES, geometry.triangle_index_count, GL_UNSIGNED_INT, nullptr);
   }
   else
   {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry.lines.get());
      glDrawElements(GL_LINES, geometry.line_index_count, GL_UNSIGNED_INT, nullptr);
   }
}

void Renderer::draw(int width, int height, const View& view) const
{
   glViewport(0, 0, width, height);
   glClearColor(0.118f, 0.118f, 0.125f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_LEQUAL);

   if (surface_.triangle_index_count == 0)
   {
      return;
   }

   const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
   const Mat4 model = rotation_x(view.pitch) * rotation_y(view.yaw) * translation(-center_.x, -center_.y, -center_.z);
   const Mat4 projection = perspective(0.7f, aspect, radius_ * 0.05f, radius_ * 20.0f);
   const Mat4 mvp = projection * translation(0.0f, 0.0f, -view.distance * radius_) * model;

   glUseProgram(program_.get());

   // push the filled surface back a little so its own wireframe wins the depth test
   glEnable(GL_POLYGON_OFFSET_FILL);
   glPolygonOffset(1.0f, 1.0f);
   draw_geometry(surface_, GL_TRIANGLES, mvp, model, 0.86f, 0.52f, 0.28f, true);
   glDisable(GL_POLYGON_OFFSET_FILL);

   if (view.wireframe)
   {
      draw_geometry(surface_, GL_LINES, mvp, model, 0.45f, 0.24f, 0.10f, false);
   }
   if (view.cage)
   {
      draw_geometry(cage_, GL_LINES, mvp, model, 0.35f, 0.75f, 1.0f, false);
   }

   glBindVertexArray(0);
}

bool Renderer::screenshot(const std::filesystem::path& path, int width, int height, const View& view) const
{
   GLuint name = 0;

   // multisampled target to draw into
   glGenRenderbuffers(1, &name);
   const Renderbuffer msaa_color{name};
   glBindRenderbuffer(GL_RENDERBUFFER, name);
   glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, width, height);

   glGenRenderbuffers(1, &name);
   const Renderbuffer msaa_depth{name};
   glBindRenderbuffer(GL_RENDERBUFFER, name);
   glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width, height);

   glGenFramebuffers(1, &name);
   const Framebuffer msaa{name};
   glBindFramebuffer(GL_FRAMEBUFFER, name);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaa_color.get());
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaa_depth.get());

   // single-sampled target to resolve into and read back
   glGenRenderbuffers(1, &name);
   const Renderbuffer resolved_color{name};
   glBindRenderbuffer(GL_RENDERBUFFER, name);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);

   glGenFramebuffers(1, &name);
   const Framebuffer resolved{name};
   glBindFramebuffer(GL_FRAMEBUFFER, name);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolved_color.get());

   glBindFramebuffer(GL_FRAMEBUFFER, msaa.get());
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
   {
      std::println(stderr, "offscreen framebuffer is incomplete");
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return false;
   }

   draw(width, height, view);

   glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa.get());
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.get());
   glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

   glBindFramebuffer(GL_READ_FRAMEBUFFER, resolved.get());
   std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
   glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   // GL rows start at the bottom
   stbi_flip_vertically_on_write(1);
   return stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

}  // namespace testbed
