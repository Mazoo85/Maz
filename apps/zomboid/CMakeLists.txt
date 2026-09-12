add_executable(zomboid main.cpp)

target_link_libraries(zomboid PRIVATE maz::maz maz_warnings)

# The game logic lives in game.hpp (also compiled by the unit tests); the app just renders it.
target_include_directories(zomboid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Shaders and the font are staged into bin/ by the engine target (see engine/CMakeLists.txt).
