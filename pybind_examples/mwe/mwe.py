from pathlib import Path
import sys

build_dir = Path(__file__).parent / "build"
sys.path.insert(0, build_dir.as_posix())

from coral import Triangulation_2
from coral import generate_from_name_and_arguments_2_2
from coral import GridOut

if __name__ == "__main__":
    triangulation = Triangulation_2()
    generate_from_name_and_arguments_2_2(triangulation, "hyper_cube", "0: 1: false")
    triangulation.refine_global(2)

    grid_out = GridOut()
    with open("grid.svg", "w") as file:
        grid_out.write_svg_2(triangulation, file)