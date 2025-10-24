from coral import Triangulation_2
from coral import hyper_cube_2_2
from coral import GridOut

if __name__ == "__main__":
    triangulation = Triangulation_2()
    hyper_cube_2_2(triangulation)
    triangulation.refine_global(4)

    grid_out = GridOut()
    with open("grid.svg", "w") as file:
        grid_out.write_svg_2(triangulation, file)