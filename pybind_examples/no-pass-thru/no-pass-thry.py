from pathlib import Path
import sys

build_dir = Path(__file__).parent / "build"
sys.path.insert(0, build_dir.as_posix())

from no_pass_thru import Instrumented
from no_pass_thru import get_and_set


if __name__ == "__main__":
    inst = Instrumented(42)

    old_val, updated_instance = get_and_set(inst, 99)

    print(f"old value: {old_val}")
    print(f"update instance value: {updated_instance.get_value()}")
    print(f"original instance value: {inst.get_value()}")
    print(f"instance equality: {updated_instance is inst}")

