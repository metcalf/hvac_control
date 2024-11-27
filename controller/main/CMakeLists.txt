# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB app_sources *.c *.cpp)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS
)
