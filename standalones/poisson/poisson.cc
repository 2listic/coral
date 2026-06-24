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
 * All parameters are read from a JSON file via deal.II's ParameterAcceptor /
 * ParameterHandler (no third-party JSON library). The file path is passed as an
 * optional first command line argument; when omitted it defaults to
 * "parameters_generator.json":
 *
 *     ./poisson [parameters.json]
 *
 * The file is parsed by ParameterHandler, which deduces the format from the
 * ".json" extension. If the file does not exist, the program writes a
 * ready-to-run template to that path -- in deal.II's full JSON schema, as
 * produced by ParameterHandler::print_parameters(), so each entry is annotated
 * with its default value, documentation and pattern -- and exits gracefully, so
 * a missing file is self-documenting. A terse value-only JSON object (just the
 * keys mapped to values) is equally accepted on input. Unlike the previous
 * nlohmann-based reader, keys that are not declared below are rejected rather
 * than silently ignored.
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
#include <deal.II/base/parameter_acceptor.h>
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

using namespace dealii;

// Fixed dimension for this standalone (see the project discussion: 2D).
static constexpr int dim      = 2;
static constexpr int spacedim = 2;


// ---------------------------------------------------------------------------
// PoissonParameters
//
// All run-time parameters, declared through ParameterAcceptor. Each member is
// connected to an entry of the parameter file by add_parameter(); the entry
// names match the keys of the JSON file 1:1, and "mesh"/"linear_solver" are
// subsections (nested JSON objects). The top-level section is "/" so that the
// global parameters live at the root of the JSON object rather than under a
// section named after this class.
//
// Every entry is optional in the file: a key that is absent keeps the default
// value assigned below. The two mesh sources are mutually exclusive and are
// selected by "mesh.source"; only the keys relevant to the chosen source are
// read in main(). ParameterHandler has no notion of conditionally-required
// keys, so this mutual exclusivity is expressed by the Patterns::Selection on
// "source" plus the branch in main(), not by the schema itself.
// ---------------------------------------------------------------------------
class PoissonParameters : public ParameterAcceptor
{
public:
  PoissonParameters()
    : ParameterAcceptor("/")
  {
    add_parameter("output_file_name", output_file_name);
    add_parameter("finite_element_degree",
                  finite_element_degree,
                  "FE_Q polynomial degree",
                  this->prm,
                  Patterns::Integer(1));
    add_parameter("n_global_refinements", n_global_refinements);

    add_parameter("rhs_expression", rhs_expression);
    add_parameter("dirichlet_boundary_ids", dirichlet_boundary_ids);
    add_parameter("dirichlet_expression", dirichlet_expression);
    add_parameter("neumann_boundary_ids", neumann_boundary_ids);
    add_parameter("neumann_expression", neumann_expression);

    enter_subsection("mesh");
    {
      add_parameter("source",
                    mesh_source,
                    "Mesh construction path: 'generator' or 'file'",
                    this->prm,
                    Patterns::Selection("generator|file"));
      add_parameter("file_name", mesh_file_name);
      add_parameter("grid_generator_function", grid_generator_function);
      add_parameter("grid_generator_arguments", grid_generator_arguments);
    }
    leave_subsection();

    enter_subsection("linear_solver");
    {
      add_parameter("type",
                    solver_type,
                    "Linear solver: 'direct' (UMFPACK) or 'iterative' (CG)",
                    this->prm,
                    Patterns::Selection("direct|iterative"));
      add_parameter("tolerance", solver_tolerance);
      add_parameter("max_iterations", solver_max_iterations);
    }
    leave_subsection();
  }

  // --- General / output ---
  std::string  output_file_name      = "solution.vtu";
  unsigned int finite_element_degree = 1;
  unsigned int n_global_refinements  = 0;

  // --- Physics / boundary ---
  std::string                  rhs_expression = "1";
  std::set<types::boundary_id> dirichlet_boundary_ids = {types::boundary_id(0)};
  std::string                  dirichlet_expression   = "0";
  std::set<types::boundary_id> neumann_boundary_ids   = {};
  std::string                  neumann_expression     = "0";

  // --- Mesh ---
  std::string mesh_source              = "generator";
  std::string mesh_file_name           = "";
  std::string grid_generator_function  = "hyper_cube";
  std::string grid_generator_arguments = "0 : 1 : true";

  // --- Linear solver (serial program defaults to a direct UMFPACK solve) ---
  std::string  solver_type           = "direct";
  double       solver_tolerance      = 1e-8;
  unsigned int solver_max_iterations = 0;
};


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


int
main(int argc, char *argv[])
{
  try
    {
      if (argc > 2)
        {
          std::cerr << "Usage: " << argv[0] << " [parameters.json]" << std::endl
                    << "  (defaults to \"parameters_generator.json\" when no "
                       "file is given)"
                    << std::endl;
          return EXIT_FAILURE;
        }

      // With no argument we fall back to a default file name.
      const std::string parameter_file_name =
        (argc == 2) ? argv[1] : "parameters_generator.json";

      // Declare all parameters up front so we can emit a complete template if
      // the file is missing (see below) and parse into the same object if it
      // exists.
      PoissonParameters par;

      // If the parameter file does not exist, write a ready-to-run template to
      // that path and exit gracefully -- rather than aborting with an exception
      // -- so the user has a self-documenting file to start from. The template
      // is produced by ParameterHandler in its full JSON form (the same schema
      // deal.II's print_parameters() emits: each entry carries its value,
      // default_value, documentation, pattern and pattern_description), so it is
      // fully self-describing. The values are the program defaults.
      {
        std::ifstream parameter_file(parameter_file_name);
        if (!parameter_file.good())
          {
            parameter_file.close();
            par.prm.print_parameters(parameter_file_name,
                                     ParameterHandler::JSON);

            std::cout << "No parameter file <" << parameter_file_name
                      << "> was found, so an example one was written with "
                         "default settings.\n"
                         "Edit it if needed, then run again:\n    "
                      << argv[0] << ' ' << parameter_file_name << std::endl;
            return EXIT_SUCCESS;
          }
      }

      // The file exists: ParameterHandler deduces the JSON format from the
      // ".json" extension. Both the full schema above and a terse value-only
      // JSON object are accepted on input.
      ParameterAcceptor::initialize(parameter_file_name);

      // --- Mesh ------------------------------------------------------------
      // The "mesh.source" selector picks exactly one construction path; only
      // the keys relevant to the chosen source are read. ParameterHandler
      // cannot make "file_name" conditionally mandatory, so we check it here.
      Triangulation<dim, spacedim> triangulation;

      if (par.mesh_source == "file")
        {
          AssertThrow(!par.mesh_file_name.empty(),
                      ExcMessage("mesh.source is 'file' but 'file_name' is "
                                 "missing or empty."));
          read_grid(par.mesh_file_name, triangulation);
        }
      else if (par.mesh_source == "generator")
        {
          GridGenerator::generate_from_name_and_arguments(
            triangulation,
            par.grid_generator_function,
            par.grid_generator_arguments);
        }
      else
        {
          AssertThrow(false,
                      ExcMessage("Unknown mesh.source '" + par.mesh_source +
                                 "'; expected 'generator' or 'file'."));
        }

      if (par.n_global_refinements > 0)
        triangulation.refine_global(par.n_global_refinements);

      // --- Finite element --------------------------------------------------
      FE_Q<dim, spacedim> fe(par.finite_element_degree);

      // --- Solve -----------------------------------------------------------
      // Note: PoissonSolver stores const references to the arguments below, so
      // they must outlive the call to solve(). They all live in `par`, which
      // outlives this call.
      PoissonSolver<dim, spacedim> poisson(triangulation,
                                           fe,
                                           par.output_file_name,
                                           par.rhs_expression,
                                           par.dirichlet_boundary_ids,
                                           par.dirichlet_expression,
                                           par.neumann_boundary_ids,
                                           par.neumann_expression,
                                           par.solver_type,
                                           par.solver_tolerance,
                                           par.solver_max_iterations);
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
