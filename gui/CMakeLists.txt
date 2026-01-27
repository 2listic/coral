cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(coral_gui LANGUAGES CXX)
endif()

option(CORAL_GUI_BUILD_EDITOR "Build the node editor (registry-driven)." ON)
option(CORAL_GUI_BUILD_MANIPULATOR "Build the VTK-based manipulator." ON)

if(CORAL_GUI_BUILD_EDITOR)
  add_subdirectory(editor)
endif()

if(CORAL_GUI_BUILD_MANIPULATOR)
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/manipulator/CMakeLists.txt")
    add_subdirectory(manipulator)
  else()
    message(WARNING
      "CORAL_GUI_BUILD_MANIPULATOR is ON, but gui/manipulator has no CMakeLists.txt; skipping.")
  endif()
endif()
