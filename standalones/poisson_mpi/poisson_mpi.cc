/* ---------------------------------------------------------------------
 *
 * Standalone MPI-parallel Poisson solver.
 *
 * This is the distributed-memory sibling of standalones/poisson. Like that
 * program it is fully self-contained (no coral, no backend headers) and reads
 * all of its parameters from a JSON file passed as an optional first argument;
 * when omitted it defaults to "parameters_generator.json":
 *
 *     mpirun -n <N> ./poisson_mpi [parameters.json]
 *
 * It solves  -laplace(u) = f  with Dirichlet and (optionally) Neumann boundary
 * conditions on a parallel::distributed::Triangulation, assembling distributed
 * PETSc/Trilinos matrices and solving with CG + algebraic multigrid. The result
 * is written as a parallel VTU/PVTU record.
 *
 * Parameters are read from the JSON file via deal.II's ParameterAcceptor /
 * ParameterHandler (no third-party JSON library); ParameterHandler deduces the
 * format from the ".json" extension. The schema matches standalones/poisson,
 * except that the "linear_solver" section defaults to an iterative solve since
 * the serial UMFPACK direct solve is replaced by an iterative solver here. If
 * the file does not exist, the program writes a ready-to-run template to that
 * path -- in deal.II's full JSON schema, as produced by
 * ParameterHandler::print_parameters() (each entry annotated with its default
 * value, documentation and pattern) -- and exits gracefully, so a missing file
 * is self-documenting. A terse value-only JSON object is equally accepted on
 * input.
 *
 * --------------------------------------------------------------------- */

#include <deal.II/base/config.h>

// This program is inherently distributed-memory: it requires deal.II to have
// been configured with MPI (for the parallel communication) and p4est (for the
// distributed triangulation). Fail loudly and early otherwise.
#if !defined(DEAL_II_WITH_MPI) || !defined(DEAL_II_WITH_P4EST)
#  error \
    "poisson_mpi requires deal.II configured with MPI and p4est (DEAL_II_WITH_MPI and DEAL_II_WITH_P4EST)."
#endif

#include <deal.II/base/conditional_ostream.h>
#include <deal.II/base/function_parser.h>
#include <deal.II/base/index_set.h>
#include <deal.II/base/parameter_acceptor.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/utilities.h>

#include <deal.II/lac/generic_linear_algebra.h>

// Select the parallel linear-algebra backend exactly the way the project's
// step-40 copy (backends/dealii/include/laplace.h) does: prefer PETSc when it
// is available with a real scalar type, otherwise fall back to Trilinos. Define
// FORCE_USE_OF_TRILINOS before compiling to override. USE_PETSC_LA is defined
// when PETSc is selected so a couple of backend-specific lines can branch.
namespace LA
{
#if defined(DEAL_II_WITH_PETSC) && !defined(DEAL_II_PETSC_WITH_COMPLEX) && \
  !(defined(DEAL_II_WITH_TRILINOS) && defined(FORCE_USE_OF_TRILINOS))
  using namespace dealii::LinearAlgebraPETSc;
#  define USE_PETSC_LA
#elif defined(DEAL_II_WITH_TRILINOS)
  using namespace dealii::LinearAlgebraTrilinos;
#else
#  error DEAL_II_WITH_PETSC or DEAL_II_WITH_TRILINOS required for poisson_mpi
#endif
} // namespace LA

// Header providing the parallel *direct* solver for the selected backend
// (PETSc's MUMPS interface, or Trilinos' Amesos-based SolverDirect).
#ifdef USE_PETSC_LA
#  include <deal.II/lac/petsc_solver.h>
#else
#  include <deal.II/lac/trilinos_solver.h>
#endif

#include <deal.II/distributed/tria.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/solver_control.h>
#include <deal.II/lac/sparsity_tools.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/vector_tools.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace dealii;

// Fixed dimension for this standalone (2D), matching standalones/poisson.
static constexpr int dim      = 2;
static constexpr int spacedim = 2;


// ---------------------------------------------------------------------------
// PoissonParameters
//
// Run-time parameters declared through ParameterAcceptor. The schema is shared
// with standalones/poisson (same entry names and subsections), so a parameter
// file is interchangeable between the two programs; the only difference is the
// default linear-solver type, which is "iterative" here. See the serial program
// for a detailed description of the layout and of how the mutually-exclusive
// mesh sources are handled.
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
                    "Linear solver: 'iterative' (CG + AMG) or 'direct' "
                    "(parallel MUMPS/Amesos)",
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

  // --- Linear solver (MPI program defaults to an iterative CG + AMG solve) ---
  std::string  solver_type           = "iterative";
  double       solver_tolerance      = 1e-8;
  unsigned int solver_max_iterations = 0;
};


// ---------------------------------------------------------------------------
// PoissonSolverMPI
//
// Distributed-memory Poisson solver. The triangulation and finite element are
// owned by the caller and passed by reference (they must outlive solve()); all
// function data are given as muparser expressions.
// ---------------------------------------------------------------------------
class PoissonSolverMPI
{
public:
  PoissonSolverMPI(
    parallel::distributed::Triangulation<dim, spacedim> &triangulation,
    const FiniteElement<dim, spacedim>                  &fe,
    const std::string                  &output_file_name = "solution.vtu",
    const std::string                  &rhs_function_expression       = "1",
    const std::set<types::boundary_id> &dirichlet_boundary_ids        = {0},
    const std::string                  &dirichlet_function_expression = "0",
    const std::set<types::boundary_id> &neumann_boundary_ids          = {},
    const std::string                  &neumann_function_expression   = "0",
    const std::string                  &solver_type           = "iterative",
    const double                        solver_tolerance      = 1e-8,
    const unsigned int                  solver_max_iterations = 0)
    : mpi_communicator(triangulation.get_communicator())
    , triangulation(triangulation)
    , fe(fe)
    , dof_handler(triangulation)
    , output_file_name(output_file_name)
    , rhs_function_expression(rhs_function_expression)
    , dirichlet_boundary_ids(dirichlet_boundary_ids)
    , dirichlet_function_expression(dirichlet_function_expression)
    , neumann_boundary_ids(neumann_boundary_ids)
    , neumann_function_expression(neumann_function_expression)
    , solver_type(solver_type)
    , solver_tolerance(solver_tolerance)
    , solver_max_iterations(solver_max_iterations)
    , pcout(std::cout,
            Utilities::MPI::this_mpi_process(mpi_communicator) == 0)
  {}

  void
  solve()
  {
    setup_system();
    assemble_system();
    solve_linear_system();
    output_results();
  }

private:
  void
  setup_system()
  {
    dof_handler.distribute_dofs(fe);

    locally_owned_dofs = dof_handler.locally_owned_dofs();
    locally_relevant_dofs =
      DoFTools::extract_locally_relevant_dofs(dof_handler);

    pcout << "   Number of active cells:       "
          << triangulation.n_global_active_cells() << std::endl
          << "   Number of degrees of freedom: " << dof_handler.n_dofs()
          << std::endl;

    locally_relevant_solution.reinit(locally_owned_dofs,
                                     locally_relevant_dofs,
                                     mpi_communicator);
    system_rhs.reinit(locally_owned_dofs, mpi_communicator);

    // Dirichlet constraints from the (muparser) boundary expression.
    FunctionParser<spacedim> dirichlet_bc(dirichlet_function_expression);
    constraints.clear();
    constraints.reinit(locally_owned_dofs, locally_relevant_dofs);
    for (const auto boundary_id : dirichlet_boundary_ids)
      VectorTools::interpolate_boundary_values(dof_handler,
                                               boundary_id,
                                               dirichlet_bc,
                                               constraints);
    constraints.close();

    DynamicSparsityPattern dsp(locally_relevant_dofs);
    DoFTools::make_sparsity_pattern(dof_handler, dsp, constraints, false);
    SparsityTools::distribute_sparsity_pattern(dsp,
                                               locally_owned_dofs,
                                               mpi_communicator,
                                               locally_relevant_dofs);

    system_matrix.reinit(locally_owned_dofs,
                         locally_owned_dofs,
                         dsp,
                         mpi_communicator);
  }

  void
  assemble_system()
  {
    const QGauss<dim>     quadrature_formula(fe.degree + 1);
    const QGauss<dim - 1> face_quadrature_formula(fe.degree + 1);

    FunctionParser<spacedim> rhs(rhs_function_expression);
    FunctionParser<spacedim> neumann_bc(neumann_function_expression);

    FEValues<dim, spacedim> fe_values(fe,
                                      quadrature_formula,
                                      update_values | update_gradients |
                                        update_quadrature_points |
                                        update_JxW_values);
    FEFaceValues<dim, spacedim> fe_face_values(fe,
                                               face_quadrature_formula,
                                               update_values |
                                                 update_quadrature_points |
                                                 update_JxW_values);

    const unsigned int dofs_per_cell = fe.n_dofs_per_cell();
    const unsigned int n_q_points    = quadrature_formula.size();
    const unsigned int n_face_q_points = face_quadrature_formula.size();

    FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
    Vector<double>     cell_rhs(dofs_per_cell);

    std::vector<double>                  rhs_values(n_q_points);
    std::vector<double>                  neumann_values(n_face_q_points);
    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);

    const bool have_neumann = !neumann_boundary_ids.empty();

    for (const auto &cell : dof_handler.active_cell_iterators())
      if (cell->is_locally_owned())
        {
          fe_values.reinit(cell);
          cell_matrix = 0.;
          cell_rhs    = 0.;

          rhs.value_list(fe_values.get_quadrature_points(), rhs_values);

          for (unsigned int q = 0; q < n_q_points; ++q)
            for (unsigned int i = 0; i < dofs_per_cell; ++i)
              {
                for (unsigned int j = 0; j < dofs_per_cell; ++j)
                  cell_matrix(i, j) += fe_values.shape_grad(i, q) *
                                       fe_values.shape_grad(j, q) *
                                       fe_values.JxW(q);

                cell_rhs(i) += rhs_values[q] *
                               fe_values.shape_value(i, q) * fe_values.JxW(q);
              }

          // Neumann contributions on the requested boundary faces.
          if (have_neumann)
            for (const auto &face : cell->face_iterators())
              if (face->at_boundary() &&
                  neumann_boundary_ids.count(face->boundary_id()) > 0)
                {
                  fe_face_values.reinit(cell, face);
                  neumann_bc.value_list(
                    fe_face_values.get_quadrature_points(), neumann_values);

                  for (unsigned int q = 0; q < n_face_q_points; ++q)
                    for (unsigned int i = 0; i < dofs_per_cell; ++i)
                      cell_rhs(i) += neumann_values[q] *
                                     fe_face_values.shape_value(i, q) *
                                     fe_face_values.JxW(q);
                }

          cell->get_dof_indices(local_dof_indices);
          constraints.distribute_local_to_global(
            cell_matrix, cell_rhs, local_dof_indices, system_matrix, system_rhs);
        }

    system_matrix.compress(VectorOperation::add);
    system_rhs.compress(VectorOperation::add);
  }

  void
  solve_linear_system()
  {
    LA::MPI::Vector completely_distributed_solution(locally_owned_dofs,
                                                    mpi_communicator);

    // Direct solve (parallel MUMPS/Amesos) or iterative solve (CG + AMG). The
    // tolerance / max_iterations parameters only apply to the iterative path; a
    // direct solve is exact up to round-off and ignores them. Both produce the
    // same solution up to the requested tolerance, so the two solver types --
    // and the JSON files selecting them -- are interchangeable.
    if (solver_type == "direct")
      {
        SolverControl solver_control;
#ifdef USE_PETSC_LA
        PETScWrappers::SparseDirectMUMPS solver(solver_control);
#else
        TrilinosWrappers::SolverDirect solver(solver_control);
#endif
        solver.solve(system_matrix,
                     completely_distributed_solution,
                     system_rhs);
        pcout << "   Solved with direct solver." << std::endl;
      }
    else if (solver_type == "iterative")
      {
        // Relative tolerance against the rhs norm, with a fallback so a
        // (near-)zero rhs does not produce a zero target tolerance.
        const double rhs_norm = system_rhs.l2_norm();
        const double tol = solver_tolerance * (rhs_norm > 0. ? rhs_norm : 1.);
        const unsigned int max_it = solver_max_iterations > 0 ?
                                      solver_max_iterations :
                                      dof_handler.n_dofs();

        SolverControl solver_control(max_it, tol);
        LA::SolverCG  solver(solver_control);

        LA::MPI::PreconditionAMG::AdditionalData data;
#ifdef USE_PETSC_LA
        data.symmetric_operator = true;
#endif
        LA::MPI::PreconditionAMG preconditioner;
        preconditioner.initialize(system_matrix, data);

        solver.solve(system_matrix,
                     completely_distributed_solution,
                     system_rhs,
                     preconditioner);

        pcout << "   Solved in " << solver_control.last_step()
              << " iterations." << std::endl;
      }
    else
      {
        AssertThrow(false,
                    ExcMessage("Unknown linear_solver.type '" + solver_type +
                               "'; expected 'direct' or 'iterative'."));
      }

    constraints.distribute(completely_distributed_solution);
    locally_relevant_solution = completely_distributed_solution;
  }

  void
  output_results()
  {
    DataOut<dim, spacedim> data_out;
    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(locally_relevant_solution, "solution");

    data_out.build_patches();

    // Write a single .vtu file collectively via MPI-I/O, so the output matches
    // the serial program (same name, same format). For very large numbers of
    // ranks, prefer write_vtu_with_pvtu_record() instead, which writes one
    // piece per rank plus a .pvtu index to avoid one huge file / I/O contention.
    data_out.write_vtu_in_parallel(output_file_name, mpi_communicator);

    pcout << "   Output written to: " << output_file_name << std::endl;
  }

  MPI_Comm mpi_communicator;

  parallel::distributed::Triangulation<dim, spacedim> &triangulation;
  const FiniteElement<dim, spacedim>                  &fe;
  DoFHandler<dim, spacedim>                            dof_handler;

  const std::string                  output_file_name;
  const std::string                  rhs_function_expression;
  const std::set<types::boundary_id> dirichlet_boundary_ids;
  const std::string                  dirichlet_function_expression;
  const std::set<types::boundary_id> neumann_boundary_ids;
  const std::string                  neumann_function_expression;
  const std::string                  solver_type;
  const double                       solver_tolerance;
  const unsigned int                 solver_max_iterations;

  IndexSet locally_owned_dofs;
  IndexSet locally_relevant_dofs;

  AffineConstraints<double> constraints;

  LA::MPI::SparseMatrix system_matrix;
  LA::MPI::Vector       locally_relevant_solution;
  LA::MPI::Vector       system_rhs;

  ConditionalOStream pcout;
};


int
main(int argc, char *argv[])
{
  // Initialize MPI for the entire lifetime of main(). It must live *outside*
  // the try block: if it were a local inside try, an exception would unwind the
  // stack and finalize MPI before the catch handlers run, so we could no longer
  // query the rank there to guard their output.
  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

  const MPI_Comm mpi_communicator = MPI_COMM_WORLD;
  const bool     is_root =
    Utilities::MPI::this_mpi_process(mpi_communicator) == 0;

  try
    {
      if (argc > 2)
        {
          if (is_root)
            std::cerr << "Usage: " << argv[0] << " [parameters.json]"
                      << std::endl
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
      // that path and exit gracefully -- rather than aborting with an exception.
      // The template is produced by ParameterHandler in its full JSON form (the
      // same schema deal.II's print_parameters() emits: each entry carries its
      // value, default_value, documentation, pattern and pattern_description),
      // so it is fully self-describing; the values are the program defaults.
      // Only rank 0 writes the file (file I/O is not rank-aware and every rank
      // would otherwise race on the same path); all ranks then exit. All ranks
      // see the same (shared) filesystem, so they agree on whether it exists.
      {
        std::ifstream parameter_file_test(parameter_file_name);
        const bool    parameter_file_exists = parameter_file_test.good();
        parameter_file_test.close();
        if (!parameter_file_exists)
          {
            if (is_root)
              {
                par.prm.print_parameters(parameter_file_name,
                                         ParameterHandler::JSON);

                std::cout << "No parameter file <" << parameter_file_name
                          << "> was found, so an example one was written with "
                             "default settings.\n"
                             "Edit it if needed, then run again:\n    mpirun -n "
                             "<N> "
                          << argv[0] << ' ' << parameter_file_name << std::endl;
              }
            return EXIT_SUCCESS;
          }
      }

      // The file exists: ParameterHandler deduces the JSON format from the
      // ".json" extension and all ranks parse it independently. Both the full
      // schema above and a terse value-only JSON object are accepted on input.
      ParameterAcceptor::initialize(parameter_file_name);

      // --- Mesh ------------------------------------------------------------
      // "mesh.source" selects exactly one construction path; only the keys
      // relevant to the chosen source are read. ParameterHandler cannot make
      // "file_name" conditionally mandatory, so we check it here.
      parallel::distributed::Triangulation<dim, spacedim> triangulation(
        mpi_communicator);

      if (par.mesh_source == "file")
        {
          AssertThrow(!par.mesh_file_name.empty(),
                      ExcMessage("mesh.source is 'file' but 'file_name' is "
                                 "missing or empty."));
          std::ifstream mesh_file(par.mesh_file_name);
          AssertThrow(mesh_file.good(),
                      ExcMessage("Mesh file not found: " + par.mesh_file_name));

          GridIn<dim, spacedim> grid_in(triangulation);
          grid_in.read(par.mesh_file_name);
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
      PoissonSolverMPI poisson(triangulation,
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
      if (is_root)
        std::cerr << std::endl
                  << "----------------------------------------------------"
                  << std::endl
                  << "Exception on processing: " << std::endl
                  << exc.what() << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
      return EXIT_FAILURE;
    }
  catch (...)
    {
      if (is_root)
        std::cerr << std::endl
                  << "----------------------------------------------------"
                  << std::endl
                  << "Unknown exception!" << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
