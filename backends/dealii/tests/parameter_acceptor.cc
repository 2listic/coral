#include <deal.II/base/parameter_acceptor.h>

#include <fstream>
#include <sstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using coral_test::ScopedTestOutputDir;

namespace
{
  std::string
  dump_prm()
  {
    std::ostringstream out;
    ParameterAcceptor::prm.print_parameters(out, ParameterHandler::ShortPRM);
    return out.str();
  }
} // namespace

// Two ParameterAcceptor nodes with no edge between them run concurrently and
// both write to the static ParameterAcceptor::prm from their constructors.
TEST(ParameterAcceptorRace, ConcurrentConstruction)
{
  ScopedTestOutputDir output_dir(
    "ParameterAcceptorRace_ConcurrentConstruction");
  coral::register_all_types();

  // Reference: the same two acceptors built sequentially.
  std::string reference;
  {
    coral::DomainParameters<2, 2>             d("/Domain/");
    coral::FiniteElementSpaceParameters<2, 2> s("/Space/");
    reference = dump_prm();
  }
  ParameterAcceptor::clear();

  const auto prm_file = output_dir.path() / "race.prm";
  {
    std::ofstream f(prm_file);
    f << reference; // existing file, so initialize() does not throw
  }

  std::ifstream input(SOURCE_DIR "/test_files/parameter_acceptor_race.json");
  ASSERT_TRUE(input.good());
  nlohmann::json data;
  input >> data;
  data["workflow"]["nodes"]["4"]["value"] = prm_file.string();

  coral::Network::set_threads_number(2);

  constexpr unsigned int n_iterations = 200;
  for (unsigned int i = 0; i < n_iterations; ++i)
    {
      {
        coral::Network network;
        network.set_touch_file_base_path(output_dir);
        ASSERT_NO_THROW(network.from_json(data)) << "iteration " << i;
        ASSERT_NO_THROW(network.run()) << "iteration " << i;
        EXPECT_EQ(dump_prm(), reference) << "iteration " << i;
      }
      ParameterAcceptor::clear();
    }
}
