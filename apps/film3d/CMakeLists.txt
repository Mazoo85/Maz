add_executable(film3d main.cpp)
target_link_libraries(film3d PRIVATE maz::maz maz_warnings)

add_executable(film3d_demo showcase.cpp)
target_link_libraries(film3d_demo PRIVATE maz::maz maz_warnings)
