#include <iostream>

#include "coral.h"
#include "coral_network.h"
#include "coral_utilities.h"
#include "register_types.h"

#include <deal.II/fe/fe_q.h>

int main()
{
    coral::NodeObject::register_elementary_type<int>();
    coral::NodeObject::register_type<dealii::FE_Q<2>, int>("degree");

    std::cout << std::setw(4) << coral::NodeObject::get_registry() << std::endl;

    // std::cout << *coral::NodeObject::type_to_hash("int") << std::endl;

    coral::NodeObject node1 = coral::NodeObject(coral::build_from_type, "int");
    coral::NodeObjectPtr pnode2 = coral::make_node(coral::build_from_type, "int");

    coral::NodeObject node3 = coral::NodeObject(coral::build_from_type, "dealii::FE_Q<2, 2>");

    return EXIT_SUCCESS;
}
