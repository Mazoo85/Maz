add_executable(zomboid_game main.cpp)
target_link_libraries(zomboid_game PRIVATE maz::zomboid maz_warnings)
set_target_properties(zomboid_game PROPERTIES OUTPUT_NAME zomboid)
