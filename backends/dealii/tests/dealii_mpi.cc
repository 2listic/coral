#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include "coral.h"
#include "coral_network.h"
#include "coral_plugin.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using coral_test::ScopedTestOutputDir;
using MPI_Session = Utilities::MPI::MPI_InitFinalize;

/**
 * MPI-aware wrapper around ScopedTestOutputDir.
 *
 * Only the master rank (rank 0) constructs and destructs the underlying
 * ScopedTestOutputDir, so all filesystem operations (create/delete) happen
 * exactly once. MPI barriers ensure:
 *  - the directory exists before non-master ranks proceed after construction,
 *  - all ranks have finished writing before master rank deletes on destruction.
 */
class MPIScopedTestOutputDir
{
public:
  MPIScopedTestOutputDir(
    const std::string           &test_name,
    const std::filesystem::path &base_dir = "./test_output")
    : path_(base_dir / test_name)
  {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0)
      scoped_dir_ = std::make_unique<ScopedTestOutputDir>(test_name, base_dir);
    MPI_Barrier(MPI_COMM_WORLD); // ensure dir exists before all ranks proceed
  }

  ~MPIScopedTestOutputDir()
  {
    MPI_Barrier(MPI_COMM_WORLD); // wait for all ranks to finish writing
    // scoped_dir_ destructs only on rank 0; other ranks hold nullptr
  }

  const std::filesystem::path &
  path() const
  {
    return path_;
  }

  operator const std::filesystem::path &() const
  {
    return path_;
  }

private:
  std::filesystem::path                path_;
  std::unique_ptr<ScopedTestOutputDir> scoped_dir_;
};

class DealiiMPITest : public ::testing::Test
{
protected:
  void static SetUpTestSuite()
  {
    // if (Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD) == 1)
    //   GTEST_SKIP() << "Please run MPI test with non trivial world size.";

    std::ifstream plugin_json_raw{SOURCE_DIR "/test_files/plugin.json"};
    ASSERT_TRUE(plugin_json_raw.good());
    json plugin_json;
    plugin_json_raw >> plugin_json;
    ASSERT_TRUE(plugin_json.contains("plugin"));
    ASSERT_EQ(coral_load_plugin(plugin_json["plugin"].dump().c_str()), 0);
  }

  void static TearDownTestSuite()
  {
    coral_unload_plugin();
  }
};

TEST_F(DealiiMPITest, Setup)
{
  int size;
  ASSERT_EQ(MPI_Comm_size(MPI_COMM_WORLD, &size), MPI_SUCCESS);
  ASSERT_GE(size, 0);

  int rank;
  ASSERT_EQ(MPI_Comm_rank(MPI_COMM_WORLD, &rank), MPI_SUCCESS);
  ASSERT_GE(rank, 0);
}

TEST_F(DealiiMPITest, LaplaceProblem)
{
  MARK_LONG_TEST();

  MPIScopedTestOutputDir output_dir("DealiiMPITest_PoissonSolver");

  auto laplace  = make_node<LaplaceProblem<2>>();
  auto mpi_init = make_node(false);
  laplace->set_arguments({mpi_init});
  ASSERT_TRUE((*laplace)());
  ASSERT_TRUE(laplace->ready());

  auto run_method        = make_node("LaplaceProblem::run<2>");
  auto output_dir_string = make_node(std::string(output_dir.path()));
  run_method->set_arguments({laplace, output_dir_string});
  (*run_method)();
}

TEST_F(DealiiMPITest, LaplaceProblemNetwork)
{
  MARK_LONG_TEST();

  MPIScopedTestOutputDir output_dir("dealiiMPITest_LaplaceTransformNetwork");

  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  std::ifstream file(SOURCE_DIR "/test_files/laplace.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open laplace.json file.";

  nlohmann::json json_data;
  file >> json_data;
  file.close();

  ASSERT_FALSE(json_data.empty()) << "JSON data is empty.";

  // Update the initialize constant
  // Node 5 contains the bool value
  if (json_data["workflow"]["nodes"].contains("5") &&
      json_data["workflow"]["nodes"]["5"].contains("value"))
    {
      json_data["workflow"]["nodes"]["5"]["value"] = "false";
    }

  // Update the output file path to use the test output directory
  // Node 2 contains the output filename
  if (json_data["workflow"]["nodes"].contains("2") &&
      json_data["workflow"]["nodes"]["2"].contains("value"))
    {
      json_data["workflow"]["nodes"]["2"]["value"] = output_dir.path().string();
    }

  network.from_json(json_data);

  slog_debug("Network has %u nodes and %u connections",
             network.n_nodes(),
             network.n_connections());

  network.run();
}
