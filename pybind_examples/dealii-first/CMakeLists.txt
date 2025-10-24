set(TARGET "dealII_coral")
set(TARGET_SRC "register.cpp")
set(PYBIND11_FINDPYTHON ON)

cmake_minimum_required(VERSION 3.22)

find_package(deal.II 9.5 REQUIRED 
    HINTS ${deal.II_DIR} ${DEAL_II_DIR} ../ ../../ $ENV{DEAL_II_DIR})
find_package(pybind11 REQUIRED)

deal_ii_initialize_cached_variables()

project(${TARGET})

deal_ii_setup_target(${TARGET})
pybind11_add_module(${TARGET} ${TARGET_SRC})