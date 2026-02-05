#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>

#include <nlohmann/json.hpp>

#include "coral.h"
#include "coral_network.h"

/** \cond INTERNAL */
namespace nlohmann
{
  template <int dim>
  struct adl_serializer<dealii::Point<dim>>
  {
    static void
    to_json(json &j, const dealii::Point<dim> &p)
    {
      j = json::array();
      for (unsigned int i = 0; i < dim; ++i)
        j.push_back(p[i]);
    }



    static void
    from_json(const json &j, dealii::Point<dim> &p)
    {
      for (unsigned int i = 0; i < dim; ++i)
        p[i] = j.at(i).get<double>();
    }
  };
} // namespace nlohmann

/** \endcond */

namespace coral
{
  using namespace dealii;

  // =========================================================================
  // Higher-Order Functions (Phase 6)
  // =========================================================================

  /**
   * @brief Apply a function to each element of a vector
   *
   * This is a type-erased implementation that works with any element type.
   * The function node's executor handles all type casting.
   *
   * @param input Vector of type-erased values
   * @param function_node Node containing the function to apply
   * @return Vector of type-erased results
   */
  inline std::vector<std::shared_ptr<entt::meta_any>>
  map(const std::vector<std::shared_ptr<entt::meta_any>> &input,
      NodeObjectPtr                                        function_node)
  {
    std::vector<std::shared_ptr<entt::meta_any>> result;
    result.reserve(input.size());

    // Extract output type from function metadata (first argument is output)
    const std::string output_type_hash =
      function_node->get_argument_type_hash(0);

    // Look up the initializer for that type
    auto &output_type_init =
      NodeObject::get_type_initializer(output_type_hash);

    for (const auto &elem_any : input)
      {
        // Create input node from element
        NodeObjectPtr input_node = std::make_shared<NodeObject>(elem_any);
        input_node->get_object() = elem_any; // Actually set the value

        // Create output node of correct type using initializer's executor
        auto          output_any  = output_type_init.executor(nullptr, {});
        NodeObjectPtr output_node = std::make_shared<NodeObject>(output_any);
        output_node->get_object() = output_any; // Set the output value

        // Set both as arguments to function (output first, then input)
        function_node->set_arguments({output_node, input_node});

        // Execute function - writes result into output_node
        (*function_node)();

        // Collect result from output node
        result.push_back(output_node->get_object());
      }

    return result;
  }

  /**
   * @brief Accumulate vector elements using a binary function
   *
   * Applies a binary function repeatedly to accumulate a result.
   * The function signature should be: R(R, T) where R is accumulator type
   * and T is element type.
   *
   * @param input Vector of type-erased values
   * @param function_node Node containing the binary function
   * @param initial Initial value for accumulation
   * @return Accumulated result as type-erased value
   */
  inline std::shared_ptr<entt::meta_any>
  reduce(const std::vector<std::shared_ptr<entt::meta_any>> &input,
         NodeObjectPtr                                        function_node,
         const std::shared_ptr<entt::meta_any> &             initial)
  {
    if (input.empty())
      return initial;

    // Get output type from function's first argument
    const std::string output_type_hash =
      function_node->get_argument_type_hash(0);
    auto &output_type_init =
      NodeObject::get_type_initializer(output_type_hash);

    // Create accumulator node with initial value
    NodeObjectPtr accumulator = std::make_shared<NodeObject>(initial);
    accumulator->get_object() = initial;

    for (const auto &elem_any : input)
      {
        // Create element node
        NodeObjectPtr elem_node = std::make_shared<NodeObject>(elem_any);
        elem_node->get_object() = elem_any;

        // Create new output node for result
        auto          output_any  = output_type_init.executor(nullptr, {});
        NodeObjectPtr output_node = std::make_shared<NodeObject>(output_any);
        output_node->get_object() = output_any;

        // Execute: output = function(accumulator, elem)
        function_node->set_arguments({output_node, accumulator, elem_node});
        (*function_node)();

        // Update accumulator for next iteration
        accumulator->get_object() = output_node->get_object();
      }

    return accumulator->get_object();
  }

  /**
   * @brief Filter vector elements using a predicate
   *
   * Selects only elements for which the predicate returns true.
   * The predicate signature should be: bool(T)
   *
   * @param input Vector of type-erased values
   * @param predicate_node Node containing the predicate function
   * @return Vector of type-erased values that passed the predicate
   */
  inline std::vector<std::shared_ptr<entt::meta_any>>
  filter(const std::vector<std::shared_ptr<entt::meta_any>> &input,
         NodeObjectPtr                                        predicate_node)
  {
    std::vector<std::shared_ptr<entt::meta_any>> result;

    // Get output type (bool) from predicate's first argument
    const std::string bool_type_hash =
      predicate_node->get_argument_type_hash(0);
    auto &bool_type_init = NodeObject::get_type_initializer(bool_type_hash);

    for (const auto &elem_any : input)
      {
        // Create element node
        NodeObjectPtr elem_node = std::make_shared<NodeObject>(elem_any);
        elem_node->get_object() = elem_any;

        // Create output node for bool result
        auto          output_any  = bool_type_init.executor(nullptr, {});
        NodeObjectPtr output_node = std::make_shared<NodeObject>(output_any);
        output_node->get_object() = output_any;

        // Execute predicate
        predicate_node->set_arguments({output_node, elem_node});
        (*predicate_node)();

        // Check if predicate returned true
        bool passed = output_node->get<bool>();
        if (passed)
          result.push_back(elem_any);
      }

    return result;
  }

  /**
   * @brief Apply a function with given arguments
   *
   * Generic function application. Takes a function node and a vector of
   * arguments, executes the function, and returns the result.
   *
   * @param function_node Node containing the function to apply
   * @param args Vector of type-erased argument values
   * @return Result as type-erased value
   */
  inline std::shared_ptr<entt::meta_any>
  apply(NodeObjectPtr                                        function_node,
        const std::vector<std::shared_ptr<entt::meta_any>> &args)
  {
    // Get output type from function's first argument
    const std::string output_type_hash =
      function_node->get_argument_type_hash(0);
    auto &output_type_init =
      NodeObject::get_type_initializer(output_type_hash);

    // Create output node
    auto          output_any  = output_type_init.executor(nullptr, {});
    NodeObjectPtr output_node = std::make_shared<NodeObject>(output_any);
    output_node->get_object() = output_any;

    // Create nodes for all arguments
    std::vector<NodeObjectPtr> arg_nodes;
    arg_nodes.push_back(output_node); // Output is first argument

    for (const auto &arg_any : args)
      {
        NodeObjectPtr arg_node = std::make_shared<NodeObject>(arg_any);
        arg_node->get_object() = arg_any;
        arg_nodes.push_back(arg_node);
      }

    // Execute function
    function_node->set_arguments(arg_nodes);
    (*function_node)();

    return output_node->get_object();
  }

  inline void
  register_non_dimensional_types()
  {
    // Canonicalize some common standard-library types across compilers.
    coral::detail::set_type_alias<std::string>("std::string");
    coral::detail::set_type_alias<std::ostream>("std::ostream");
    coral::detail::set_type_alias<std::ofstream>("std::ofstream");

    NodeObject::register_elementary_type<std::string>();
    NodeObject::register_elementary_type<unsigned int>();
    NodeObject::register_elementary_type<int>();
    NodeObject::register_elementary_type<double>();
    NodeObject::register_elementary_type<bool>();

    NodeObject::register_elementary_type<types::boundary_id>();
    NodeObject::register_elementary_type<types::subdomain_id>();
    NodeObject::register_elementary_type<types::manifold_id>();
    NodeObject::register_elementary_type<types::material_id>();

    NodeObject::register_type<GridOut>();

    NodeObject::register_derived_type<std::ostream, std::ofstream, std::string>(
      "file_name");
    Network::register_node();
  }



  template <int dim, int spacedim>
  inline void
  register_dimensional_types()
  {
    NodeObject::register_type<Triangulation<dim, spacedim>>();
    NodeObject::register_type<DoFHandler<dim, spacedim>,
                              Triangulation<dim, spacedim>>("triangulation");

    NodeObject::register_function(
      GridGenerator::generate_from_name_and_arguments<dim, spacedim>,
      {"GridGenerator::generate_from_name_and_arguments<" +
         Utilities::dim_string(dim, spacedim) + ">",
       "triangulation",
       "grid_generator_function_name",
       "grid_generator_function_arguments"});

    NodeObject::
      register_method<Triangulation<dim, spacedim>, void, unsigned int>(
        &Triangulation<dim, spacedim>::refine_global,
        {"Triangulation<" + Utilities::dim_string(dim, spacedim) +
           ">::refine_global",
         "triangulation",
         "n_refinements"});

    NodeObject::register_method<GridOut,
                                void,
                                const Triangulation<dim, spacedim> &,
                                std::ostream &>(
      &GridOut::write_vtk<dim, spacedim>,
      {"GridOut::write_vtk<" + Utilities::dim_string(dim, spacedim) + ">",
       "grid_out",
       "triangulation",
       "output_file"});
  }



  inline void
  register_all_types()
  {
    register_non_dimensional_types();
    register_dimensional_types<2, 2>();
    // register_dimensional_types<3, 3>();
    // register_dimensional_types<2, 3>();
  };

} // namespace coral
#endif
