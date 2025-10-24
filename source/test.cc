#include <iostream>

#include "coral.h"
#include "coral_network.h"
#include "coral_utilities.h"
#include "register_types.h"

#include <deal.II/fe/fe_q.h>

int main()
{
    using namespace dealii;
    using namespace coral;

    /*
    using type = FE_Q<2>;
    NodeObject::register_elementary_type<int>();
    NodeObject::register_type<type, int>("degree");
    NodeObjectPtr fe     = make_node(coral::build_from_type, "dealii::FE_Q<2, 2>");
    NodeObjectPtr degree = make_node(1);
    fe->set_arguments({degree});
    (*fe)();

    std::cout << *NodeObject::get_type<type>() << std::endl;
    */

 





    //coral::NodeObject::register_elementary_type<int>();

    //using type = dealii::FE_Q<2>;
    //coral::NodeObject::register_type<type, int>("degree");

    //coral::NodeObject node = coral::make_node<type>(1);

    //std::cout << std::setw(4) << coral::NodeObject::get_registry() << std::endl;

    // std::cout << *coral::NodeObject::type_to_hash("int") << std::endl;

    //coral::NodeObjectPtr pnode2 = coral::make_node(coral::build_from_type, "int");
    //(*pnode2)();
    //std::cout << pnode2->ready() << std::endl;

    //coral::NodeObjectPtr node1 = coral::make_node

    //coral::NodeObjectPtr node3 = coral::make_node(coral::build_from_type, "dealii::FE_Q<2, 2>");
    //node3->set_arguments({pnode2});
    //(*node3)();
    //std::cout << node3->ready() << std::endl;

    return EXIT_SUCCESS;
}
