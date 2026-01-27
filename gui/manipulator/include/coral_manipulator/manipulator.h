#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace coral::manipulator
{
class ManipulatorApp
{
public:
  ManipulatorApp();
  ~ManipulatorApp();

  ManipulatorApp(const ManipulatorApp &)            = delete;
  ManipulatorApp &operator=(const ManipulatorApp &) = delete;

  void initialize(GLFWwindow *glfw_window);
  void set_framebuffer_size(int width, int height);

  // GLFW callback entry points (call these from your GLFW callbacks, and also
  // forward the event to ImGui's GLFW backend callbacks).
  void on_glfw_cursor_pos(GLFWwindow *glfw_window, double xpos, double ypos);
  void on_glfw_mouse_button(GLFWwindow *glfw_window,
                            int        button,
                            int        action,
                            int        mods);
  void on_glfw_scroll(GLFWwindow *glfw_window, double xoffset, double yoffset);

  void process_input(GLFWwindow *glfw_window);
  void render_frame();
  void draw_ui();

  std::string status_text() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
} // namespace coral::manipulator
