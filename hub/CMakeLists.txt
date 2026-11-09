# MC2Recomp hub: builds MC2Recomp.exe into the MC2Recomp folder (beside
# the toolkit folder) and copies games.txt there. settings.ini and
# covers/ are created beside it at run time. Use build.cmd, or:
#   cmake -S hub -B hub/build -G Ninja && cmake --build hub/build
cmake_minimum_required(VERSION 3.20)
project(MC2RecompHub C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_sources hub.c)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icon/hub.ico")
    enable_language(RC)
    list(APPEND _sources hub.rc)
endif()
add_executable(MC2Recomp WIN32 ${_sources})
target_compile_definitions(MC2Recomp PRIVATE UNICODE _UNICODE)
target_link_options(MC2Recomp PRIVATE -municode -static)
target_link_libraries(MC2Recomp PRIVATE gdiplus msimg32 urlmon comdlg32)
set_target_properties(MC2Recomp PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_root}")

# The game list; edit hub/games.txt and rebuild to add a game.
configure_file(games.txt "${_root}/games.txt" COPYONLY)
