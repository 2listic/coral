#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdio>

#ifdef IMGUI_IMPL_OPENGL_ES2
#  include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h>

#include "coral_manipulator/manipulator.h"

namespace
{
static coral::manipulator::ManipulatorApp *g_app = nullptr;

static void
glfw_error_callback(int error, const char *description)
{
  (void)error;
  std::fprintf(stderr, "glfw-err: %s\n", description);
}

static void
glfw_cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
  if (g_app)
    g_app->on_glfw_cursor_pos(window, xpos, ypos);
}

static void
glfw_mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
  ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
  if (g_app)
    g_app->on_glfw_mouse_button(window, button, action, mods);
}

static void
glfw_scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
  ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
  if (g_app)
    g_app->on_glfw_scroll(window, xoffset, yoffset);
}
} // namespace

int
main(int argc, const char *argv[])
{
  (void)argc;
  (void)argv;

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

#if defined(IMGUI_IMPL_OPENGL_ES2)
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow *window =
    glfwCreateWindow(1280, 720, "CORAL Manipulator (VTK)", nullptr, nullptr);
  if (!window)
    return 1;

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  ImGui::StyleColorsDark();

  coral::manipulator::ManipulatorApp app;
  g_app = &app;
  app.initialize(window);

  // Override ImGui's installed callbacks with wrappers that also forward events
  // to the manipulator (for VTK camera controls). The wrappers call ImGui's
  // callbacks explicitly.
  glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
  glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
  glfwSetScrollCallback(window, glfw_scroll_callback);

  while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      int display_w = 0;
      int display_h = 0;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      app.set_framebuffer_size(display_w, display_h);

      app.draw_ui();
      app.process_input(window);

      ImGui::Render();

      app.render_frame();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(window);
    }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
