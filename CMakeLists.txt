cmake_minimum_required(VERSION 3.21)

project(
   tinysubdiv
   VERSION 1.0.0
   DESCRIPTION "Loop subdivision for triangle meshes, C++23, single header"
   HOMEPAGE_URL "https://github.com/varnholt/tinysubdiv"
   LANGUAGES CXX
)

option(TINYSUBDIV_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(TINYSUBDIV_BUILD_EXAMPLES "Build the OBJ command line example" ${PROJECT_IS_TOP_LEVEL})
option(TINYSUBDIV_BUILD_TESTBED "Build the OpenGL ES 3 / WebGL2 testbed (fetches GLFW)" OFF)
option(TINYSUBDIV_INSTALL "Generate the install target" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)

if(EMSCRIPTEN)
   # tinysubdiv reports bad input with exceptions; Emscripten disables them by default
   add_compile_options(-fexceptions)
   add_link_options(-fexceptions)
endif()

add_library(tinysubdiv INTERFACE)
add_library(tinysubdiv::tinysubdiv ALIAS tinysubdiv)

target_include_directories(
   tinysubdiv INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(tinysubdiv INTERFACE cxx_std_23)

if(TINYSUBDIV_BUILD_TESTS OR TINYSUBDIV_BUILD_EXAMPLES OR TINYSUBDIV_BUILD_TESTBED)
   add_library(tinysubdiv_warnings INTERFACE)
   if(MSVC)
      target_compile_options(tinysubdiv_warnings INTERFACE /W4 /WX /permissive- /utf-8)
   else()
      target_compile_options(tinysubdiv_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
   endif()
endif()

if(TINYSUBDIV_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()

if(TINYSUBDIV_BUILD_TESTBED)
   add_subdirectory(testbed)
endif()

if(TINYSUBDIV_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

if(TINYSUBDIV_INSTALL)
   include(CMakePackageConfigHelpers)

   install(TARGETS tinysubdiv EXPORT tinysubdivTargets)
   install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
   install(
      EXPORT tinysubdivTargets
      NAMESPACE tinysubdiv::
      FILE tinysubdivConfig.cmake
      DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/tinysubdiv
   )

   write_basic_package_version_file(
      ${PROJECT_BINARY_DIR}/tinysubdivConfigVersion.cmake COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT
   )
   install(FILES ${PROJECT_BINARY_DIR}/tinysubdivConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/tinysubdiv)
endif()
