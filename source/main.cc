#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp> // JSON library

#include "coral.h"
#include "coral_network.h"
#include "coral_utilities.h"
#include "register_types.h"
#include "taskflow/taskflow.hpp" // Taskflow library

using json = nlohmann::json;

using namespace coral;

void
dump_registry(const std::string& outname) {
    auto json = coral::NodeObject::get_registry();
    std::ofstream output{outname};
    output << std::setw(4) << json << std::endl;
}

int
main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cerr << "USAGE\n"
                  << argv[0] << "\n\tProduce node_types.json\n\n"
                  << argv[0] << " input.json\n\tLoad and execute a graph"
                  << std::endl;

        return EXIT_FAILURE;
    }

    coral::register_all_types();
    std::cout << "Registered all types." << std::endl;

    std::string outfile{"node_types.json"};
    dump_registry(outfile);
    std::cout << "Dumped registered node to " << outfile << "." << std::endl;

    if (argc == 1)
        return EXIT_SUCCESS;

    std::ifstream input{argv[1]};
    if (!input.good()) {
        std::cerr << "Could not open " << argv[1] << "." << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "File " << argv[1] << " opened." << std::endl;

    json data;
    input >> data;
    std::cout << "File " << argv[1] << " read." << std::endl;

    json fixed_data = coral::fix_hashes(data);
    std::cout << "Hashes fixed." << std::endl;

    coral::Network network;
    network.from_json(fixed_data);
    std::cout << "Build network from data." << std::endl;

    std::string network_filename{"network.dot"};
    network.output_dot(network_filename);
    std::cout << "Network graph " << network_filename << "." << std::endl;

    network.run();
    std::cout << "Network run." << std::endl;

  return EXIT_SUCCESS;
}
