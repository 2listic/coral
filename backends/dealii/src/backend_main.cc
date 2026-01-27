#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#define JSON_DIAGNOSTICS 1
#include <CLI11/CLI11.hpp>
#include <nlohmann/json.hpp> // JSON library

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>

#include "coral.h"
#include "coral_network.h"
#include "magic_enum/magic_enum_all.hpp"
#include "register_types.h"
#include "slog.h"
#include "taskflow/taskflow.hpp" // Taskflow library

using json   = nlohmann::json;
namespace fs = std::filesystem;

using namespace coral;

void
dump_registry(const fs::path &outpath)
{
  auto          json = coral::NodeObject::get_registry();
  std::ofstream output{outpath};
  output << std::setw(4) << json << std::endl;
}

namespace
{
  struct SlogCliConfig
  {
    std::string name        = "coral";
    std::string flags       = "all";
    int         thread_safe = 1;

    // -1 means "leave slog default".
    int n_to_screen = -1;
    int n_to_file   = -1;
    int n_keep_open = -1;
    int n_trace_tid = -1;
    int n_use_heap  = -1;
    int n_indent    = -1;
    int n_rotate    = -1;
    int n_flush     = 1;

    std::string file_name;
    std::string file_path;
    std::string separator;
    std::string date_control;
    std::string color_format;
  };

  static std::string
  to_lower_copy(std::string value)
  {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
    return value;
  }

  static uint16_t
  parse_slog_flags(const std::string &spec)
  {
    const auto lowered = to_lower_copy(spec);
    if (lowered.empty() || lowered == "all")
      return SLOG_FLAGS_ALL;
    if (lowered == "none" || lowered == "0")
      return 0;

    uint16_t           flags = 0;
    std::string        token;
    std::istringstream ss(lowered);
    while (std::getline(ss, token, ','))
      {
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        if (token.empty())
          continue;

        if (token == "all")
          {
            flags |= SLOG_FLAGS_ALL;
            continue;
          }
        if (token == "none" || token == "0")
          continue;
        if (token == "warning")
          token = "warn";

        std::string enum_name = token;
        if (enum_name.rfind("slog_", 0) != 0 &&
            enum_name.rfind("SLOG_", 0) != 0)
          enum_name = "SLOG_" + to_lower_copy(token);

        // magic_enum expects the exact enumerator name, but supports
        // case-insensitive matching.
        auto value =
          magic_enum::enum_cast<slog_flag_t>(enum_name,
                                             magic_enum::case_insensitive);
        if (!value)
          throw std::runtime_error("Unknown slog flag: '" + token + "'");

        flags |= static_cast<uint16_t>(*value);
      }
    return flags;
  }

  template <typename Enum>
  static Enum
  parse_enum_or_throw(const std::string &value, const char *what)
  {
    auto v = magic_enum::enum_cast<Enum>(value, magic_enum::case_insensitive);
    if (v)
      return *v;
    throw std::runtime_error(std::string("Unknown ") + what + ": '" + value +
                             "'");
  }

  static slog_date_ctrl_t
  parse_slog_date_control(const std::string &value)
  {
    const auto lowered = to_lower_copy(value);
    if (lowered.empty())
      return SLOG_TIME_ONLY;
    if (lowered == "disable" || lowered == "off" || lowered == "none")
      return SLOG_TIME_DISABLE;
    if (lowered == "time" || lowered == "time_only")
      return SLOG_TIME_ONLY;
    if (lowered == "date" || lowered == "full" || lowered == "date_full")
      return SLOG_DATE_FULL;
    return parse_enum_or_throw<slog_date_ctrl_t>(value, "slog date control");
  }

  static slog_coloring_t
  parse_slog_color_format(const std::string &value)
  {
    const auto lowered = to_lower_copy(value);
    if (lowered.empty())
      return SLOG_COLORING_TAG;
    if (lowered == "disable" || lowered == "off" || lowered == "none")
      return SLOG_COLORING_DISABLE;
    if (lowered == "tag")
      return SLOG_COLORING_TAG;
    if (lowered == "full")
      return SLOG_COLORING_FULL;
    return parse_enum_or_throw<slog_coloring_t>(value, "slog color format");
  }

  struct SlogGuard
  {
    bool active = false;

    void
    init_default()
    {
      slog_init("coral", SLOG_FLAGS_ALL, 1);
      slog_config_t cfg;
      slog_config_get(&cfg);
      cfg.nFlush = 1;
      slog_config_set(&cfg);
      active = true;
    }

    void
    reinit_from_cli(const SlogCliConfig &cli)
    {
      if (active)
        slog_destroy();

      const uint16_t flags = parse_slog_flags(cli.flags);
      slog_init(cli.name.c_str(),
                flags,
                static_cast<uint8_t>(cli.thread_safe != 0));

      slog_config_t cfg;
      slog_config_get(&cfg);

      auto set_bool = [](uint8_t &dest, int value) {
        if (value < 0)
          return;
        dest = static_cast<uint8_t>(value != 0);
      };

      set_bool(cfg.nToScreen, cli.n_to_screen);
      set_bool(cfg.nToFile, cli.n_to_file);
      set_bool(cfg.nKeepOpen, cli.n_keep_open);
      set_bool(cfg.nTraceTid, cli.n_trace_tid);
      set_bool(cfg.nUseHeap, cli.n_use_heap);
      set_bool(cfg.nIndent, cli.n_indent);
      set_bool(cfg.nRotate, cli.n_rotate);
      set_bool(cfg.nFlush, cli.n_flush);

      if (!cli.separator.empty())
        {
          std::snprintf(cfg.sSeparator,
                        sizeof(cfg.sSeparator),
                        "%s",
                        cli.separator.c_str());
        }
      if (!cli.file_name.empty())
        {
          std::snprintf(cfg.sFileName,
                        sizeof(cfg.sFileName),
                        "%s",
                        cli.file_name.c_str());
        }
      if (!cli.file_path.empty())
        {
          std::snprintf(cfg.sFilePath,
                        sizeof(cfg.sFilePath),
                        "%s",
                        cli.file_path.c_str());
        }

      if (!cli.date_control.empty())
        cfg.eDateControl = parse_slog_date_control(cli.date_control);
      if (!cli.color_format.empty())
        cfg.eColorFormat = parse_slog_color_format(cli.color_format);

      slog_config_set(&cfg);
      active = true;
    }

    ~SlogGuard()
    {
      if (active)
        slog_destroy();
    }
  };
} // namespace

int
main(int argc, char *argv[])
{
  SlogGuard slog_guard;
  slog_guard.init_default();

  // Parse command line arguments

  CLI::App app{"dealii-backend. A backend for coral."};

  app.failure_message([&](const CLI::App *app, const CLI::Error &error) {
    slog_error("Error: %s", error.what());
    slog_error("%s", app->help().c_str());
    return "";
  });

  app.require_subcommand(1, 1);

  SlogCliConfig slog_cli;

  auto *slog_group = app.add_option_group("slog", "SLog logging configuration");
  slog_group
    ->add_option("--slog-name",
                 slog_cli.name,
                 "SLog instance name (used for log file naming)")
    ->capture_default_str();
  slog_group
    ->add_option(
      "--slog-flags",
      slog_cli.flags,
      "Enabled slog flags (comma-separated: notag,note,info,warn,debug,trace,error,fatal,all,none)")
    ->capture_default_str();
  slog_group
    ->add_option("--slog-thread-safe",
                 slog_cli.thread_safe,
                 "Enable slog internal synchronization (0/1)")
    ->check(CLI::Range(0, 1))
    ->capture_default_str();

  slog_group
    ->add_option("--slog-to-screen",
                 slog_cli.n_to_screen,
                 "Enable screen logging (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option("--slog-to-file",
                 slog_cli.n_to_file,
                 "Enable file logging (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option("--slog-keep-open",
                 slog_cli.n_keep_open,
                 "Keep log file handle open (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option("--slog-rotate",
                 slog_cli.n_rotate,
                 "Enable log rotation (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option(
      "--slog-indent",
      slog_cli.n_indent,
      "Enable indentation formatting (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option("--slog-flush",
                 slog_cli.n_flush,
                 "Flush stdout after screen log (0/1)")
    ->check(CLI::Range(0, 1))
    ->capture_default_str();
  slog_group
    ->add_option(
      "--slog-use-heap",
      slog_cli.n_use_heap,
      "Use heap allocation in slog (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();
  slog_group
    ->add_option("--slog-trace-tid",
                 slog_cli.n_trace_tid,
                 "Trace thread id in output (0/1, default: keep slog default)")
    ->check(CLI::Range(-1, 1))
    ->capture_default_str();

  slog_group->add_option("--slog-file-name",
                         slog_cli.file_name,
                         "Log file name (when file logging is enabled)");
  slog_group->add_option(
    "--slog-file-path",
    slog_cli.file_path,
    "Log file directory path (when file logging is enabled)");
  slog_group->add_option("--slog-separator",
                         slog_cli.separator,
                         "Separator between header and message");

  slog_group->add_option(
    "--slog-date",
    slog_cli.date_control,
    "Date/time format: disable, time, date (default: keep slog default)");
  slog_group->add_option(
    "--slog-color",
    slog_cli.color_format,
    "Color format: disable, tag, full (default: keep slog default)");

  CLI::App *register_sub = app.add_subcommand("register", "Register node type");

  fs::path register_path{"node_types.json"};
  register_sub
    ->add_option("register_path",
                 register_path,
                 "Output path of node registry json")
    ->capture_default_str()
    ->type_name("PATH");

  CLI::App *run_sub = app.add_subcommand("run", "Run a certain graph");
  fs::path  input_json;
  fs::path  graph_path{"network.dot"};
  fs::path  touch_file_path{"./"};

  run_sub
    ->add_option("input_json", input_json, "Input json of the graph to run")
    ->required()
    ->check(CLI::ExistingPath.description(""))
    ->type_name("PATH");

  run_sub
    ->add_option("--register",
                 register_path,
                 "Output path of node registry json")
    ->capture_default_str()
    ->type_name("PATH")
    ->expected(0, 1)
    ->multi_option_policy(CLI::MultiOptionPolicy::Throw);

  run_sub->add_option("--graph", graph_path, "Output path of graph dot file")
    ->capture_default_str()
    ->type_name("PATH")
    ->expected(0, 1)
    ->multi_option_policy(CLI::MultiOptionPolicy::Throw);

  run_sub
    ->add_option("--touch-dir",
                 touch_file_path,
                 "Output directory for touch files (node status markers)")
    ->capture_default_str()
    ->type_name("PATH")
    ->expected(0, 1)
    ->multi_option_policy(CLI::MultiOptionPolicy::Throw);

  CLI11_PARSE(app, argc, argv);

  try
    {
      slog_guard.reinit_from_cli(slog_cli);
    }
  catch (const std::exception &e)
    {
      slog_error("Failed to configure slog: %s", e.what());
      return EXIT_FAILURE;
    }

  bool run        = run_sub->parsed();
  bool dump_reg   = register_sub->parsed() || run_sub->count("--register");
  bool dump_graph = run_sub->count("--graph");


  // do the job

  coral::register_all_types();
  slog_info("Registered all types.");

  if (dump_reg)
    {
      dump_registry(register_path);
      slog_info("Dumped registered nodes to %s.", register_path.c_str());
    }

  if (!run)
    return EXIT_SUCCESS;

  std::ifstream input{input_json};
  if (!input.good())
    {
      slog_error("Could not open %s.", input_json.c_str());
      return EXIT_FAILURE;
    }
  slog_info("File %s opened.", input_json.c_str());

  json data;
  input >> data;
  slog_info("File %s read.", input_json.c_str());


  const char  *env_th    = std::getenv("THREADS");
  const size_t n_threads = env_th ? static_cast<size_t>(std::stoull(env_th)) :
                                    std::thread::hardware_concurrency();
  slog_info("Thread pool size of %zu.", n_threads);

  coral::Network network;
  Network::set_threads_number(n_threads);
  network.from_json(data);
  slog_info("Built network from data.");

  Network::set_touch_file_base_path(touch_file_path);
  slog_info("Touch file base path: %s", touch_file_path.c_str());

  if (dump_graph)
    {
      network.output_dot(graph_path);
      slog_info("Wrote network graph to %s.", graph_path.c_str());
    }

  network.run();
  slog_info("Network run completed.");

  return EXIT_SUCCESS;
}
