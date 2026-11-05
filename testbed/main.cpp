// tinysubdiv testbed: shows a control cage and its Loop subdivision surface.
//
//   tinysubdiv_testbed [mesh.obj ...] [--shape NAME] [--level N]
//                      [--screenshot out.png] [--size WxH] [--no-wireframe] [--no-cage]
//
// Mouse: drag to orbit, wheel to zoom.
// Keys:  Up/Down level, Space/Left/Right shape, W wireframe, C cage, R reset view, Esc quit.

#include "gl.hpp"
#include "obj.hpp"
#include "renderer.hpp"
#include "shapes.hpp"

#include <tinysubdiv/tinysubdiv.hpp>

#include <algorithm>
#include <charconv>
#include <exception>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace
{

constexpr int max_level = 6;

struct App
{
   GLFWwindow* window = nullptr;
   std::optional<testbed::Renderer> renderer;
   std::vector<testbed::Shape> shapes;
   std::size_t shape = 0;
   int level = 3;
   testbed::View view;
   std::size_t triangle_count = 0;

   bool dragging = false;
   double last_x = 0.0;
   double last_y = 0.0;

   void rebuild()
   {
      const auto& cage = shapes[shape].cage;
      const auto surface = tinysubdiv::loop(cage, level);
      triangle_count = surface.triangles.size();
      renderer->set_meshes(cage, surface);
      update_title();
   }

   void update_title() const
   {
      const auto title =
          std::format("tinysubdiv testbed - {} - level {} - {} triangles", shapes[shape].name, level, triangle_count);
      glfwSetWindowTitle(window, title.c_str());
   }

   void reset_view()
   {
      view.yaw = 0.6f;
      view.pitch = 0.45f;
      view.distance = 3.2f;
   }
};

App* app_from(GLFWwindow* window)
{
   return static_cast<App*>(glfwGetWindowUserPointer(window));
}

void on_key(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
   if (action == GLFW_RELEASE)
   {
      return;
   }

   auto& app = *app_from(window);
   const auto shape_count = app.shapes.size();

   switch (key)
   {
      case GLFW_KEY_UP:
      case GLFW_KEY_KP_ADD:
      case GLFW_KEY_EQUAL:
         app.level = std::min(app.level + 1, max_level);
         app.rebuild();
         break;
      case GLFW_KEY_DOWN:
      case GLFW_KEY_KP_SUBTRACT:
      case GLFW_KEY_MINUS:
         app.level = std::max(app.level - 1, 0);
         app.rebuild();
         break;
      case GLFW_KEY_SPACE:
      case GLFW_KEY_RIGHT:
         app.shape = (app.shape + 1) % shape_count;
         app.rebuild();
         break;
      case GLFW_KEY_LEFT:
         app.shape = (app.shape + shape_count - 1) % shape_count;
         app.rebuild();
         break;
      case GLFW_KEY_W: app.view.wireframe = !app.view.wireframe; break;
      case GLFW_KEY_C: app.view.cage = !app.view.cage; break;
      case GLFW_KEY_R: app.reset_view(); break;
      case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
      default: break;
   }
}

void on_mouse_button(GLFWwindow* window, int button, int action, int /*mods*/)
{
   auto& app = *app_from(window);
   if (button == GLFW_MOUSE_BUTTON_LEFT)
   {
      app.dragging = action == GLFW_PRESS;
      glfwGetCursorPos(window, &app.last_x, &app.last_y);
   }
}

void on_cursor(GLFWwindow* window, double x, double y)
{
   auto& app = *app_from(window);
   if (app.dragging)
   {
      app.view.yaw += static_cast<float>(x - app.last_x) * 0.01f;
      app.view.pitch = std::clamp(app.view.pitch + static_cast<float>(y - app.last_y) * 0.01f, -1.55f, 1.55f);
   }
   app.last_x = x;
   app.last_y = y;
}

void on_scroll(GLFWwindow* window, double /*x*/, double y)
{
   auto& app = *app_from(window);
   app.view.distance = std::clamp(app.view.distance * (y > 0 ? 0.9f : 1.1f), 1.2f, 20.0f);
}

void frame(void* user)
{
   auto& app = *static_cast<App*>(user);
   int width = 0;
   int height = 0;
   glfwGetFramebufferSize(app.window, &width, &height);
   app.renderer->draw(width, height, app.view);
   glfwSwapBuffers(app.window);
   glfwPollEvents();
}

struct Options
{
   std::vector<std::string> obj_files;
   std::optional<std::string> shape;
   std::optional<int> level;
   std::optional<std::string> screenshot;
   int width = 1280;
   int height = 720;
   bool wireframe = true;
   bool cage = true;
};

std::optional<Options> parse_options(std::span<char*> args)
{
   Options options;
   for (std::size_t i = 1; i < args.size(); ++i)
   {
      const std::string_view arg = args[i];
      const auto value = [&]() -> std::optional<std::string_view>
      {
         if (i + 1 < args.size())
         {
            return std::string_view{args[++i]};
         }
         std::println(stderr, "{} needs a value", arg);
         return std::nullopt;
      };

      if (arg == "--shape")
      {
         const auto v = value();
         if (!v)
         {
            return std::nullopt;
         }
         options.shape = std::string{*v};
      }
      else if (arg == "--level")
      {
         const auto v = value();
         int level = 0;
         if (!v || !obj::detail::parse(*v, level) || level < 0 || level > max_level)
         {
            std::println(stderr, "--level expects 0..{}", max_level);
            return std::nullopt;
         }
         options.level = level;
      }
      else if (arg == "--screenshot")
      {
         const auto v = value();
         if (!v)
         {
            return std::nullopt;
         }
         options.screenshot = std::string{*v};
      }
      else if (arg == "--size")
      {
         const auto v = value();
         const auto x = v ? v->find('x') : std::string_view::npos;
         if (x == std::string_view::npos || !obj::detail::parse(v->substr(0, x), options.width) ||
             !obj::detail::parse(v->substr(x + 1), options.height) || options.width <= 0 || options.height <= 0)
         {
            std::println(stderr, "--size expects WIDTHxHEIGHT");
            return std::nullopt;
         }
      }
      else if (arg == "--no-wireframe")
      {
         options.wireframe = false;
      }
      else if (arg == "--no-cage")
      {
         options.cage = false;
      }
      else if (arg.starts_with("--"))
      {
         std::println(stderr, "unknown option {}", arg);
         return std::nullopt;
      }
      else
      {
         options.obj_files.emplace_back(arg);
      }
   }
   return options;
}

int run(const Options& options)
{
   App app;
   app.shapes = testbed::builtin_shapes();

   // loaded files go first, welded so seams do not tear
   std::vector<testbed::Shape> loaded;
   for (const auto& file : options.obj_files)
   {
      const auto mesh = obj::read(file);
      if (!mesh)
      {
         std::println(stderr, "{}: {}", file, mesh.error());
         return 1;
      }
      loaded.push_back({std::filesystem::path(file).filename().string(), tinysubdiv::weld(*mesh)});
   }
   app.shapes.insert(app.shapes.begin(), loaded.begin(), loaded.end());

   if (options.shape)
   {
      const auto it = std::ranges::find(app.shapes, *options.shape, &testbed::Shape::name);
      if (it == app.shapes.end())
      {
         std::println(stderr, "unknown shape '{}'", *options.shape);
         return 1;
      }
      app.shape = static_cast<std::size_t>(it - app.shapes.begin());
   }
   app.level = options.level.value_or(app.level);
   app.view.wireframe = options.wireframe;
   app.view.cage = options.cage;
   app.reset_view();

   if (glfwInit() == GLFW_FALSE)
   {
      std::println(stderr, "glfwInit failed");
      return 1;
   }

   // OpenGL ES 3.0 natively, WebGL2 in the browser
   glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
   glfwWindowHint(GLFW_SAMPLES, 4);
   if (options.screenshot)
   {
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
   }

   app.window = glfwCreateWindow(options.width, options.height, "tinysubdiv testbed", nullptr, nullptr);
   if (app.window == nullptr)
   {
      std::println(stderr, "could not create an OpenGL ES 3.0 window");
      glfwTerminate();
      return 1;
   }
   glfwMakeContextCurrent(app.window);

#ifndef __EMSCRIPTEN__
   if (gladLoadGLES2(glfwGetProcAddress) == 0)
   {
      std::println(stderr, "could not load OpenGL ES 3.0 functions");
      glfwTerminate();
      return 1;
   }
   glfwSwapInterval(1);
#endif

   int exit_code = 0;
   try
   {
      app.renderer.emplace();
      app.rebuild();

      if (options.screenshot)
      {
         if (!app.renderer->screenshot(*options.screenshot, options.width, options.height, app.view))
         {
            std::println(stderr, "could not write {}", *options.screenshot);
            exit_code = 1;
         }
      }
      else
      {
         glfwSetWindowUserPointer(app.window, &app);
         glfwSetKeyCallback(app.window, on_key);
         glfwSetMouseButtonCallback(app.window, on_mouse_button);
         glfwSetCursorPosCallback(app.window, on_cursor);
         glfwSetScrollCallback(app.window, on_scroll);

#ifdef __EMSCRIPTEN__
         // hands control to the browser; App lives on because main never returns
         emscripten_set_main_loop_arg(frame, &app, 0, true);
#else
         while (glfwWindowShouldClose(app.window) == GLFW_FALSE)
         {
            frame(&app);
         }
#endif
      }
   }
   catch (const std::exception& e)
   {
      std::println(stderr, "{}", e.what());
      exit_code = 1;
   }

   app.renderer.reset();  // GL objects go before the context
   glfwDestroyWindow(app.window);
   glfwTerminate();
   return exit_code;
}

}  // namespace

int main(int argc, char* argv[])
{
   const auto options = parse_options(std::span(argv, static_cast<std::size_t>(argc)));
   if (!options)
   {
      return 2;
   }
   return run(*options);
}
