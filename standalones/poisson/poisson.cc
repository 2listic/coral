/* ---------------------------------------------------------------------
 *
 * Standalone Poisson solver.
 *
 * A self-contained deal.II program that solves a Poisson problem. It is NOT
 * linked against coral and does not reuse the backend headers: the small amount
 * of code it needs (the PoissonSolver class and a grid reader) is copied in
 * here so that this standalone can diverge freely from the backend versions in
 * backends/dealii/include.
 *
 * All parameters are read from a JSON file (nlohmann::json) whose path is passed
 * as the first command line argument:
 *
 *     ./poisson parameters.json
 *
 * The mesh is either generated with
 * GridGenerator::generate_from_name_and_arguments() or read from a file,
 * selected by the "mesh.source" field:
 *
 *   "source": "generator"  -> uses grid_generator_function/arguments
 *   "source": "file"       -> uses file_name
 *
 * --------------------------------------------------------------------- */

#include <deal.II/base/function_parser.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/solver_control.h>
#include <deal.II/lac/sparse_direct.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/vector_tools.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

using namespace dealii;
using json = nlohmann::json;

// Fixed dimension for this standalone (see the project discussion: 2D).
static constexpr int dim      = 2;
static constexpr int spacedim = 2;


// ---------------------------------------------------------------------------
// PoissonSolver
//
// Local copy of the backend's header-only solver. Assembles and solves
//     -laplace(u) = f
// with Dirichlet and (optionally) Neumann boundary conditions, and writes the
// solution as a VTU file. All function data are given as muparser expressions.
// ---------------------------------------------------------------------------
template <int dim, int spacedim = dim>
class PoissonSolver
{
public:
  PoissonSolver(
    const Triangulation<dim, spacedim> &triangulation,
    const FiniteElement<dim, spacedim> &fe,
    const std::string                  &output_file_name = "solution.vtu",
    const std::string                  &rhs_function_expression       = "1",
    const std::set<types::boundary_id> &dirichlet_boundary_ids        = {0},
    const std::string                  &dirichlet_function_expression = "0",
    const std::set<types::boundary_id> &neumann_boundary_ids          = {},
    const std::string                  &neumann_function_expression   = "0",
    const std::string                  &solver_type           = "direct",
    const double                        solver_tolerance      = 1e-8,
    const unsigned int                  solver_max_iterations = 0)
    : triangulation(triangulation)
    , fe(fe)
    , output_file_name(output_file_name)
    , rhs_function_expression(rhs_function_expression)
    , dirichlet_boundary_ids(dirichlet_boundary_ids)
    , dirichlet_function_expression(dirichlet_function_expression)
    , neumann_boundary_ids(neumann_boundary_ids)
    , neumann_function_expression(neumann_function_expression)
    , solver_type(solver_type)
    , solver_tolerance(solver_tolerance)
    , solver_max_iterations(solver_max_iterations)
    , dof_handler(triangulation)
  {}

  void
  solve()
  {
    dof_handler.distribute_dofs(fe);

    std::cout << "   Number of active cells:       "
              << triangulation.n_active_cells() << std::endl
              << "   Number of degrees of freedom: " << dof_handler.n_dofs()
              << std::endl;

    DynamicSparsityPattern dsp(dof_handler.n_dofs(), dof_handler.n_dofs());

    AffineConstraints<double> constraints;
    FunctionParser<spacedim>  dirichlet_bc(dirichlet_function_expression);
    for (const auto boundary_id : dirichlet_boundary_ids)
      VectorTools::interpolate_boundary_values(dof_handler,
                                               boundary_id,
                                               dirichlet_bc,
                                               constraints);
    constraints.close();

    DoFTools::make_sparsity_pattern(dof_handler, dsp, constraints, false);
    sparsity_pattern.copy_from(dsp);
    system_matrix.reinit(sparsity_pattern);
    solution.reinit(dof_handler.n_dofs());
    system_rhs.reinit(dof_handler.n_dofs());

    FunctionParser<spacedim> rhs(rhs_function_expression);
    FunctionParser<spacedim> neumann_bc(neumann_function_expression);

    VectorTools::create_right_hand_side(
      dof_handler, QGauss<dim>(fe.degree + 1), rhs, system_rhs, constraints);
    if constexpr (dim > 1 && dim == spacedim)
      VectorTools::create_boundary_right_hand_side(dof_handler,
                                                   QGauss<dim - 1>(fe.degree +
                                                                   1),
                                                   neumann_bc,
                                                   system_rhs,
                                                   neumann_boundary_ids);
    else
      {
        AssertThrow(false, ExcNotImplemented());
      }

    MatrixTools::create_laplace_matrix(dof_handler,
                                       QGauss<dim>(fe.degree + 1),
                                       system_matrix,
                                       rhs,
                                       system_rhs,
                                       {},
                                       constraints);

    // Direct solve (UMFPACK) or iterative solve (CG + SSOR). The tolerance /
    // max_iterations parameters are only meaningful for the iterative path; a
    // direct solve is exact up to round-off and ignores them. Both produce the
    // same solution up to the requested tolerance, so the two solver types are
    // interchangeable for this SPD problem.
    if (solver_type == "direct")
      {
        SparseDirectUMFPACK solver;
        solver.initialize(system_matrix);
        solver.vmult(solution, system_rhs);
        std::cout << "   Solved with direct solver." << std::endl;
      }
    else if (solver_type == "iterative")
      {
        const double rhs_norm = system_rhs.l2_norm();
        const double tol =
          solver_tolerance * (rhs_norm > 0. ? rhs_norm : 1.);
        const unsigned int max_it = solver_max_iterations > 0 ?
                                      solver_max_iterations :
                                      dof_handler.n_dofs();

        SolverControl            solver_control(max_it, tol);
        SolverCG<Vector<double>> solver(solver_control);

        PreconditionSSOR<SparseMatrix<double>> preconditioner;
        preconditioner.initialize(system_matrix, 1.2);

        solver.solve(system_matrix, solution, system_rhs, preconditioner);
        std::cout << "   Solved in " << solver_control.last_step()
                  << " iterations." << std::endl;
      }
    else
      {
        AssertThrow(false,
                    ExcMessage("Unknown linear_solver.type '" + solver_type +
                               "'; expected 'direct' or 'iterative'."));
      }
    constraints.distribute(solution);

    DataOut<dim, spacedim> data_out;
    DataOutBase::VtkFlags  vtk_flags;
    vtk_flags.write_higher_order_cells = true;

    data_out.set_flags(vtk_flags);
    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution, "solution");
    data_out.build_patches();

    std::ofstream output(output_file_name);
    data_out.write_vtu(output);

    std::cout << "   Output written to: " << output_file_name << std::endl;
  }

private:
  const Triangulation<dim, spacedim> &triangulation;
  const FiniteElement<dim, spacedim> &fe;
  const std::string                  &output_file_name;
  const std::string                  &rhs_function_expression;
  const std::set<types::boundary_id> &dirichlet_boundary_ids;
  const std::string                  &dirichlet_function_expression;
  const std::set<types::boundary_id> &neumann_boundary_ids;
  const std::string                  &neumann_function_expression;
  const std::string                   solver_type;
  const double                        solver_tolerance;
  const unsigned int                  solver_max_iterations;

  DoFHandler<dim, spacedim> dof_handler;

  SparsityPattern      sparsity_pattern;
  SparseMatrix<double> system_matrix;

  Vector<double> solution;
  Vector<double> system_rhs;
};


// Read a mesh from a file into the triangulation. The format is deduced from
// the file extension by GridIn (.msh, .vtu, .vtk, ...).
static void
read_grid(const std::string &file_name, Triangulation<dim, spacedim> &tria)
{
  std::ifstream file(file_name);
  AssertThrow(file.good(), ExcMessage("Mesh file not found: " + file_name));

  GridIn<dim, spacedim> grid_in(tria);
  grid_in.read(file_name);
}


// Read a JSON array of non-negative integers into a set of boundary ids.
static std::set<types::boundary_id>
read_boundary_ids(const json &j)
{
  std::set<types::boundary_id> ids;
  for (const auto &id : j)
    ids.insert(types::boundary_id(id.get<unsigned int>()));
  return ids;
}


int
main(int argc, char *argv[])
{
  try
    {
      if (argc != 2)
        {
          std::cerr << "Usage: " << argv[0] << " <parameters.json>"
                    << std::endl;
          return EXIT_FAILURE;
        }

      const std::string parameter_file_name = argv[1];

      std::ifstream parameter_file(parameter_file_name);
      AssertThrow(parameter_file.good(),
                  ExcMessage("Could not open parameter file: " +
                             parameter_file_name));

      json prm;
      parameter_file >> prm;
      parameter_file.close();

      // --- General / output parameters ------------------------------------
      const std::string output_file_name =
        prm.value("output_file_name", std::string("solution.vtu"));

      const unsigned int fe_degree = prm.value("finite_element_degree", 1u);

      const unsigned int n_global_refinements =
        prm.value("n_global_refinements", 0u);

      // --- Physics / boundary parameters ----------------------------------
      const std::string rhs_expression =
        prm.value("rhs_expression", std::string("1"));

      const std::set<types::boundary_id> dirichlet_boundary_ids =
        prm.contains("dirichlet_boundary_ids") ?
          read_boundary_ids(prm.at("dirichlet_boundary_ids")) :
          std::set<types::boundary_id>{types::boundary_id(0)};

      const std::string dirichlet_expression =
        prm.value("dirichlet_expression", std::string("0"));

      const std::set<types::boundary_id> neumann_boundary_ids =
        prm.contains("neumann_boundary_ids") ?
          read_boundary_ids(prm.at("neumann_boundary_ids")) :
          std::set<types::boundary_id>{};

      const std::string neumann_expression =
        prm.value("neumann_expression", std::string("0"));

      // --- Linear solver parameters ---------------------------------------
      // Shared schema with poisson_mpi. The serial program defaults to a direct
      // solve (UMFPACK); "iterative" selects CG. tolerance/max_iterations are
      // only used by the iterative path.
      const json        linear_solver = prm.value("linear_solver", json::object());
      const std::string solver_type =
        linear_solver.value("type", std::string("direct"));
      const double solver_tolerance = linear_solver.value("tolerance", 1e-8);
      const unsigned int solver_max_iterations =
        linear_solver.value("max_iterations", 0u);

      // --- Mesh ------------------------------------------------------------
      // The "mesh.source" selector picks exactly one construction path; only
      // the keys relevant to the chosen source are read.
      Triangulation<dim, spacedim> triangulation;

      const json        mesh = prm.value("mesh", json::object());
      const std::string mesh_source =
        mesh.value("source", std::string("generator"));

      if (mesh_source == "file")
        {
          AssertThrow(mesh.contains("file_name"),
                      ExcMessage("mesh.source is 'file' but 'file_name' is "
                                 "missing."));
          const std::string mesh_file_name = mesh.at("file_name");
          read_grid(mesh_file_name, triangulation);
        }
      else if (mesh_source == "generator")
        {
          const std::string grid_generator_function =
            mesh.value("grid_generator_function", std::string("hyper_cube"));
          const std::string grid_generator_arguments =
            mesh.value("grid_generator_arguments", std::string("0 : 1 : true"));

          GridGenerator::generate_from_name_and_arguments(
            triangulation, grid_generator_function, grid_generator_arguments);
        }
      else
        {
          AssertThrow(false,
                      ExcMessage("Unknown mesh.source '" + mesh_source +
                                 "'; expected 'generator' or 'file'."));
        }

      if (n_global_refinements > 0)
        triangulation.refine_global(n_global_refinements);

      // --- Finite element --------------------------------------------------
      FE_Q<dim, spacedim> fe(fe_degree);

      // --- Solve -----------------------------------------------------------
      // Note: PoissonSolver stores const references to the arguments below, so
      // they must outlive the call to solve(). They all live in this scope.
      PoissonSolver<dim, spacedim> poisson(triangulation,
                                           fe,
                                           output_file_name,
                                           rhs_expression,
                                           dirichlet_boundary_ids,
                                           dirichlet_expression,
                                           neumann_boundary_ids,
                                           neumann_expression,
                                           solver_type,
                                           solver_tolerance,
                                           solver_max_iterations);
      poisson.solve();
    }
  catch (const std::exception &exc)
    {
      std::cerr << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl
                << exc.what() << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return EXIT_FAILURE;
    }
  catch (...)
    {
      std::cerr << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
