# Interactive viewer on OpenGL ES 3.0, so the same code runs natively (GLFW + glad)
# and in the browser on WebGL2 (Emscripten):
#
#   cmake -S . -B build -DTINYSUBDIV_BUILD_TESTBED=ON
#   emcmake cmake -S . -B build-web -DTINYSUBDIV_BUILD_TESTBED=ON

enable_language(C)

add_executable(tinysubdiv_testbed main.cpp renderer.cpp)
target_link_libraries(tinysubdiv_testbed PRIVATE tinysubdiv::tinysubdiv tinysubdiv_warnings tinysubdiv_testbed_third_party)

add_library(tinysubdiv_testbed_third_party STATIC third_party/stb/stb_image_write.c)
target_include_directories(tinysubdiv_testbed_third_party SYSTEM PUBLIC third_party/stb ${PROJECT_SOURCE_DIR}/examples)

if(EMSCRIPTEN)
   set_target_properties(tinysubdiv_testbed PROPERTIES SUFFIX ".html")
   target_link_options(
      tinysubdiv_testbed PRIVATE -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
      -sALLOW_MEMORY_GROWTH=1
   )
else()
   include(FetchContent)

   set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
   set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
   set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
   set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

   FetchContent_Declare(
      glfw
      URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
      URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
   )
   FetchContent_MakeAvailable(glfw)

   target_sources(tinysubdiv_testbed_third_party PRIVATE third_party/glad/src/gles2.c)
   target_include_directories(tinysubdiv_testbed_third_party SYSTEM PUBLIC third_party/glad/include)
   target_link_libraries(tinysubdiv_testbed_third_party PUBLIC glfw)
endif()
