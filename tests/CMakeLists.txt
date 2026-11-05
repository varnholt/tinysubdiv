add_executable(tinysubdiv_test tinysubdiv_test.cpp)
target_link_libraries(tinysubdiv_test PRIVATE tinysubdiv::tinysubdiv tinysubdiv_warnings)
add_test(NAME tinysubdiv_test COMMAND tinysubdiv_test)

if(TARGET tinysubdiv_obj)
   add_test(NAME tinysubdiv_obj_roundtrip
            COMMAND tinysubdiv_obj ${CMAKE_CURRENT_SOURCE_DIR}/data/split_cube.obj ${CMAKE_CURRENT_BINARY_DIR}/out.obj 2)
   set_tests_properties(tinysubdiv_obj_roundtrip PROPERTIES PASS_REGULAR_EXPRESSION "26 vertices -> 8 welded -> 98 vertices, 192 triangles")
endif()

add_executable(tinysubdiv_shapes_test shapes_test.cpp)
target_include_directories(tinysubdiv_shapes_test PRIVATE ${PROJECT_SOURCE_DIR}/testbed)
target_link_libraries(tinysubdiv_shapes_test PRIVATE tinysubdiv::tinysubdiv tinysubdiv_warnings)
add_test(NAME tinysubdiv_shapes_test COMMAND tinysubdiv_shapes_test)
