include(FetchContent)

set(_node_editor_root "${CMAKE_CURRENT_LIST_DIR}/imgui-node-editor")

add_subdirectory(
  ${_node_editor_root}/external/imgui
  ${CMAKE_CURRENT_BINARY_DIR}/imgui
  EXCLUDE_FROM_ALL)

if(TARGET imgui)
  if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
  else()
    target_compile_options(imgui PRIVATE -w)
  endif()
endif()

add_library(coral_imgui_node_editor STATIC
  ${_node_editor_root}/crude_json.cpp
  ${_node_editor_root}/imgui_canvas.cpp
  ${_node_editor_root}/imgui_node_editor.cpp
  ${_node_editor_root}/imgui_node_editor_api.cpp)

target_include_directories(coral_imgui_node_editor SYSTEM PUBLIC
  ${_node_editor_root}
  ${_node_editor_root}/external/imgui)

target_link_libraries(coral_imgui_node_editor PUBLIC imgui)

if(MSVC)
  target_compile_options(coral_imgui_node_editor PRIVATE /W0)
else()
  target_compile_options(coral_imgui_node_editor PRIVATE -w)
endif()

find_package(glfw3 QUIET)
if(NOT glfw3_FOUND AND NOT TARGET glfw AND NOT TARGET glfw3)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
  )
  FetchContent_MakeAvailable(glfw)
endif()

find_package(OpenGL REQUIRED)

if(TARGET glfw)
  set(_glfw_target glfw)
elseif(TARGET glfw3)
  set(_glfw_target glfw3)
else()
  set(_glfw_target glfw)
endif()

if(TARGET ${_glfw_target})
  if(MSVC)
    target_compile_options(${_glfw_target} PRIVATE /W0)
  else()
    target_compile_options(${_glfw_target} PRIVATE -w)
  endif()
endif()

add_executable(coral_node_editor
  source/node_editor_main.cc
  ${_node_editor_root}/examples/application/source/imgui_impl_glfw.cpp
  ${_node_editor_root}/examples/application/source/imgui_impl_opengl3.cpp)

target_compile_features(coral_node_editor PRIVATE cxx_std_17)

target_include_directories(coral_node_editor SYSTEM PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/../../core/include
  ${_node_editor_root}
  ${_node_editor_root}/external/imgui
  ${_node_editor_root}/examples/application/source)

target_link_libraries(coral_node_editor PRIVATE
  coral_imgui_node_editor
  imgui
  ${_glfw_target})

if(MSVC)
  target_compile_options(coral_node_editor PRIVATE /W0)
else()
  target_compile_options(coral_node_editor PRIVATE -w)
endif()

if(TARGET OpenGL::GL)
  target_link_libraries(coral_node_editor PRIVATE OpenGL::GL)
else()
  target_link_libraries(coral_node_editor PRIVATE ${OPENGL_gl_LIBRARY})
endif()

if(APPLE)
  target_compile_definitions(coral_node_editor PRIVATE GL_SILENCE_DEPRECATION=1)
  target_link_libraries(coral_node_editor PRIVATE
    "-framework CoreFoundation"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo")
endif()
