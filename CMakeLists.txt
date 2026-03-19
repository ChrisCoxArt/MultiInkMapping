# Multi Ink Mapping
# Created March 17, 2026

cmake_minimum_required (VERSION 3.8)
project( MultiInkMapping CXX)

set( CMAKE_CXX_STANDARD 20)
set( CMAKE_INCLUDE_CURRENT_DIR ON)
set( CMAKE_POSITION_INDEPENDENT_CODE TRUE)


# Currently no platform specific config needed

#
# App definitions
#

set( SOURCES
MiniICC.cpp
MiniTIFF.cpp
Options.cpp
MultiInkMapping.cpp
)

set( PROJECT_HEADERS
MiniICC.hpp
MiniTIFF.hpp
Options.hpp
json.hpp
MultiInkMapping.hpp
)

add_executable( MultiInkMapping
${SOURCES}
${PUBLIC_HEADERS}
${PROJECT_HEADERS}
${PRIVATE_HEADERS}
)
