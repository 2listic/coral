#ifndef CORAL_BACKENDS_DEALII_POISSON_H
#define CORAL_BACKENDS_DEALII_POISSON_H

#include <deal.II/base/function.h>
#include <deal.II/base/function_parser.h>
#include <deal.II/base/parameter_acceptor.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/sparse_direct.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/vector_tools.h>

using namespace dealii;

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
    const std::string                  &neumann_function_expression   = "0")
    : triangulation(triangulation)
    , fe(fe)
    , output_file_name(output_file_name)
    , rhs_function_expression(rhs_function_expression)
    , dirichlet_boundary_ids(dirichlet_boundary_ids)
    , dirichlet_function_expression(dirichlet_function_expression)
    , neumann_boundary_ids(neumann_boundary_ids)
    , neumann_function_expression(neumann_function_expression)
    , dof_handler(triangulation)
  {
    dof_handler.distribute_dofs(fe);

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
    VectorTools::create_boundary_right_hand_side(dof_handler,
                                                 QGauss<dim - 1>(fe.degree + 1),
                                                 neumann_bc,
                                                 system_rhs,
                                                 neumann_boundary_ids);

    MatrixTools::create_laplace_matrix(dof_handler,
                                       QGauss<dim>(fe.degree + 1),
                                       system_matrix,
                                       rhs,
                                       system_rhs,
                                       {},
                                       constraints);

    SparseDirectUMFPACK solver;
    solver.initialize(system_matrix);
    solver.vmult(solution, system_rhs);
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

  DoFHandler<dim, spacedim> dof_handler;

  SparsityPattern      sparsity_pattern;
  SparseMatrix<double> system_matrix;

  Vector<double> solution;
  Vector<double> system_rhs;
};


#endif