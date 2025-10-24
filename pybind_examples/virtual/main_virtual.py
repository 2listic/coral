from virtual import Base
from virtual import Derived
from virtual import play_method

if __name__ == "__main__":
    base = Base()
    derived = Derived()

    base.method()
    derived.method()

    play_method(base)
    play_method(derived)

    try:
        play_method(5)
    except TypeError:
        print("Ok, it should not played")
