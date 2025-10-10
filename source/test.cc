#include <iostream>

#include "coral.h"
#include "coral_network.h"
#include "coral_utilities.h"
#include "register_types.h"

int main()
{
    coral::NodeObject::register_elementary_type<int>();

    std::cout << std::setw(4) << coral::NodeObject::get_registry() << std::endl;

    // std::cout << *coral::NodeObject::type_to_hash("int") << std::endl;

    auto node = coral::NodeObject(coral::build_from_type{}, "int");

    return EXIT_SUCCESS;
}
