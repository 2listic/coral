#/bin/bash

PYBIND11_CMAKE_DIR=$(python3 -m pybind11 --cmakedir) && \
echo "export CMAKE_PREFIX_PATH=${PYBIND11_CMAKE_DIR}:${CMAKE_PREFIX_PATH}" >> ~/.bashrc

