cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(coral_gui_manipulator LANGUAGES CXX)
endif()

find_package(deal.II 9.5 QUIET
  HINTS ${deal.II_DIR} ${DEAL_II_DIR} ../.. ../../.. $ENV{DEAL_II_DIR})

if(NOT deal.II_FOUND AND NOT DEAL_II_FOUND)
  message(WARNING "deal.II not found; skipping gui/manipulator")
  return()
endif()

deal_ii_initialize_cached_variables()

if(NOT DEAL_II_WITH_VTK)
  message(WARNING
    "deal.II was found but was built without VTK support (DEAL_II_WITH_VTK=OFF); skipping gui/manipulator")
  return()
endif()

include(FetchContent)

set(_imgui_root "${CMAKE_CURRENT_LIST_DIR}/../editor/imgui-node-editor/external/imgui")
set(_imgui_impl_root "${CMAKE_CURRENT_LIST_DIR}/../editor/imgui-node-editor/examples/application/source")

if(NOT TARGET imgui)
  add_subdirectory(
    ${_imgui_root}
    ${CMAKE_CURRENT_BINARY_DIR}/imgui
    EXCLUDE_FROM_ALL)
  if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
  else()
    target_compile_options(imgui PRIVATE -w)
  endif()
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

add_executable(coral_manipulator
  source/main.cpp
  source/manipulator.cpp
  ${_imgui_impl_root}/imgui_impl_glfw.cpp
  ${_imgui_impl_root}/imgui_impl_opengl3.cpp
  ${CMAKE_CURRENT_LIST_DIR}/imgui/misc/cpp/imgui_stdlib.cpp)

target_compile_features(coral_manipulator PRIVATE cxx_std_17)

target_include_directories(coral_manipulator SYSTEM PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/source
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/imgui
  ${_imgui_root}
  ${_imgui_impl_root}
  ${PROJECT_SOURCE_DIR}/core/include)

target_link_libraries(coral_manipulator
  coral_core
  imgui
  ${_glfw_target})

deal_ii_setup_target(coral_manipulator)

if(MSVC)
  target_compile_options(coral_manipulator PRIVATE /W0)
else()
  target_compile_options(coral_manipulator PRIVATE -w)
endif()

if(TARGET OpenGL::GL)
  target_link_libraries(coral_manipulator OpenGL::GL)
else()
  target_link_libraries(coral_manipulator ${OPENGL_gl_LIBRARY})
endif()

if(APPLE)
  target_compile_definitions(coral_manipulator PRIVATE GL_SILENCE_DEPRECATION=1)
  target_link_libraries(coral_manipulator
    "-framework CoreFoundation"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo")
endif()
