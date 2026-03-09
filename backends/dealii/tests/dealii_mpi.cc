#include <gtest/gtest.h>

#include <filesystem>

#include "coral.h"
#include "coral_network.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using coral_test::ScopedTestOutputDir;

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

  ScopedTestOutputDir output_dir("dealiiExamplesMPI_PoissonSolver");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  auto laplace = make_node<LaplaceProblem<2>>();

  ASSERT_TRUE((*laplace)());
  ASSERT_TRUE(laplace->ready());

  auto run_method        = make_node("LaplaceProblem::run<2>");
  auto output_dir_string = make_node(std::string(output_dir.path()));
  run_method->set_arguments({laplace, output_dir_string});
  (*run_method)();
}
