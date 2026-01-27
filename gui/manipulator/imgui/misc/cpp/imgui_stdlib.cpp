// imgui_stdlib.cpp
// Helper functions to use std::string with Dear ImGui.
// This is a lightly vendored copy of Dear ImGui's misc/cpp/imgui_stdlib.cpp.
// The original project is https://github.com/ocornut/imgui

#include "imgui_stdlib.h"

#include <cstring>

namespace ImGui
{
namespace
{
struct InputTextCallback_UserData
{
  std::string           *Str;
  ImGuiInputTextCallback ChainCallback;
  void                 *ChainCallbackUserData;
};

static int
InputTextCallback(ImGuiInputTextCallbackData *data)
{
  auto *user_data = static_cast<InputTextCallback_UserData *>(data->UserData);
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
      std::string *str = user_data->Str;
      IM_ASSERT(data->Buf == str->c_str());
      str->resize(static_cast<size_t>(data->BufTextLen));
      data->Buf = const_cast<char *>(str->c_str());
    }
  else if (user_data->ChainCallback)
    {
      data->UserData = user_data->ChainCallbackUserData;
      return user_data->ChainCallback(data);
    }
  return 0;
}
} // namespace

bool
InputText(const char           *label,
          std::string          *str,
          ImGuiInputTextFlags   flags,
          ImGuiInputTextCallback callback,
          void                 *user_data)
{
  IM_ASSERT(str != nullptr);

  flags |= ImGuiInputTextFlags_CallbackResize;

  InputTextCallback_UserData cb_user_data;
  cb_user_data.Str                   = str;
  cb_user_data.ChainCallback         = callback;
  cb_user_data.ChainCallbackUserData = user_data;

  return ImGui::InputText(label,
                          const_cast<char *>(str->c_str()),
                          str->capacity() + 1,
                          flags,
                          InputTextCallback,
                          &cb_user_data);
}

bool
InputTextMultiline(const char           *label,
                   std::string          *str,
                   const ImVec2         &size,
                   ImGuiInputTextFlags   flags,
                   ImGuiInputTextCallback callback,
                   void                 *user_data)
{
  IM_ASSERT(str != nullptr);

  flags |= ImGuiInputTextFlags_CallbackResize;

  InputTextCallback_UserData cb_user_data;
  cb_user_data.Str                   = str;
  cb_user_data.ChainCallback         = callback;
  cb_user_data.ChainCallbackUserData = user_data;

  return ImGui::InputTextMultiline(label,
                                   const_cast<char *>(str->c_str()),
                                   str->capacity() + 1,
                                   size,
                                   flags,
                                   InputTextCallback,
                                   &cb_user_data);
}

bool
InputTextWithHint(const char           *label,
                  const char           *hint,
                  std::string          *str,
                  ImGuiInputTextFlags   flags,
                  ImGuiInputTextCallback callback,
                  void                 *user_data)
{
  IM_ASSERT(str != nullptr);

  flags |= ImGuiInputTextFlags_CallbackResize;

  InputTextCallback_UserData cb_user_data;
  cb_user_data.Str                   = str;
  cb_user_data.ChainCallback         = callback;
  cb_user_data.ChainCallbackUserData = user_data;

  return ImGui::InputTextWithHint(label,
                                  hint,
                                  const_cast<char *>(str->c_str()),
                                  str->capacity() + 1,
                                  flags,
                                  InputTextCallback,
                                  &cb_user_data);
}
} // namespace ImGui

