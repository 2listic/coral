#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include "coral.h"
#include "coral_network.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using coral_test::ScopedTestOutputDir;

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
private:
  using MPI_Session = Utilities::MPI::MPI_InitFinalize;

  inline static std::unique_ptr<MPI_Session> m_mpi_session = nullptr;

protected:
  void static SetUpTestSuite()
  {
    int    argc   = 0;
    char **argv   = nullptr;
    m_mpi_session = std::make_unique<MPI_Session>(argc, argv, 1);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size == 1)
      GTEST_SKIP() << "Please run MPI test with non trivial world size.";
  }

  void static TearDownTestSuite()
  {
    m_mpi_session.reset();
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

  MPIScopedTestOutputDir output_dir("dealiiExamplesMPI_PoissonSolver");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

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
