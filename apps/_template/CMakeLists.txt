add_executable(app_template main.cpp)

target_link_libraries(app_template PRIVATE maz::maz maz_warnings)

# Shaders and the font are staged into bin/ by the engine target (see engine/CMakeLists.txt).
# This is the canonical starting point for a new (mobile-ready) Maz game: callback-driven main loop
# (platform::runMainLoop) + unified touch/keyboard input (input::VirtualControls). Copy this directory
# and rename the target to start a game.
