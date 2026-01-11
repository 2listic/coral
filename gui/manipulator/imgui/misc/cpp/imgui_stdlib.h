// imgui_stdlib.h
// Helper functions to use std::string with Dear ImGui.
// This is a lightly vendored copy of Dear ImGui's misc/cpp/imgui_stdlib.h.
// The original project is https://github.com/ocornut/imgui

#pragma once

#include <imgui.h>

#include <string>

namespace ImGui
{
IMGUI_API bool
InputText(const char        *label,
          std::string       *str,
          ImGuiInputTextFlags flags = 0,
          ImGuiInputTextCallback callback = nullptr,
          void              *user_data = nullptr);

IMGUI_API bool
InputTextMultiline(const char        *label,
                   std::string       *str,
                   const ImVec2      &size = ImVec2(0, 0),
                   ImGuiInputTextFlags flags = 0,
                   ImGuiInputTextCallback callback = nullptr,
                   void              *user_data = nullptr);

IMGUI_API bool
InputTextWithHint(const char        *label,
                  const char        *hint,
                  std::string       *str,
                  ImGuiInputTextFlags flags = 0,
                  ImGuiInputTextCallback callback = nullptr,
                  void              *user_data = nullptr);
} // namespace ImGui

