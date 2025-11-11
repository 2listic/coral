# Minimal Working Example

Switch on the compose:
```
docker compose up -d
```

Enter the container:
```
docker compose exec coral -it bash
```

(notice that you will be inside a python venv and the variable `CMAKE_PREFIX_PATH` must be set). Inside the container, in `/app/pybind_example/mwe` build with CMake:
```
cmake -B build
cmake --build build
```
Then launch
```
python3 mwe.py
```
a `.svg` file should be produced.
