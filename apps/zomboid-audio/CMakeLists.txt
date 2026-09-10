add_executable(zomboid_audio main.cpp)
target_link_libraries(zomboid_audio PRIVATE maz::zomboid maz_warnings)
set_target_properties(zomboid_audio PROPERTIES OUTPUT_NAME zomboid-audio)
