#ifndef CORAL_DEALII_PARAMETERS_H
#define CORAL_DEALII_PARAMETERS_H

#include <deal.II/base/parameter_acceptor.h>

#include <string>

namespace coral
{
  template <int dim, int spacedim = dim>
  class DomainParameters : public dealii::ParameterAcceptor
  {
  public:
    explicit DomainParameters(const std::string &subsection = "/Domain/")
      : dealii::ParameterAcceptor(subsection)
    {
      add_parameter("Grid generator", name_of_grid);
      add_parameter("Grid generator arguments", arguments_for_grid);
      add_parameter("Grid scale", grid_scale);
      add_parameter("Initial refinement", initial_refinement);
      enter_subsection("VTK support");
      add_parameter("Read fields", read_vtk_fields);
      add_parameter("Boundary id field", vtk_boundary_id_field);
      leave_subsection();
    }

    std::string  name_of_grid          = "hyper_cube";
    std::string  arguments_for_grid    = "-1: 1: false";
    double       grid_scale            = 1.;
    unsigned int initial_refinement    = 0;
    bool         read_vtk_fields       = false;
    std::string  vtk_boundary_id_field = "boundary_id";
  };

  template <int dim, int spacedim = dim>
  class FiniteElementSpaceParameters : public dealii::ParameterAcceptor
  {
  public:
    explicit FiniteElementSpaceParameters(
      const std::string &subsection = "/Space/")
      : dealii::ParameterAcceptor(subsection)
    {
      add_parameter("Finite element", fe_name);
      add_parameter("Polynomial degree", degree);
      enter_subsection("Quadrature");
      add_parameter("Extra points", extra_quadrature_points);
      leave_subsection();
    }

    std::string  fe_name                 = "FE_Q";
    unsigned int degree                  = 1;
    unsigned int extra_quadrature_points = 0;
  };

  inline void
  initialize_parameters(dealii::ParameterAcceptor &,
                        dealii::ParameterAcceptor &,
                        const std::string &file_name)
  {
    dealii::ParameterAcceptor::initialize(file_name);
  }
} // namespace coral
#endif
