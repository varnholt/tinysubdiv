add_executable(tinysubdiv_obj subdivide_obj.cpp)
target_link_libraries(tinysubdiv_obj PRIVATE tinysubdiv::tinysubdiv tinysubdiv_warnings)

if(EMSCRIPTEN)
   # read and write real files when run under Node
   target_link_options(tinysubdiv_obj PRIVATE -sNODERAWFS=1)
endif()
