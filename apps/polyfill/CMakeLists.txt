add_executable(polyfill main.cpp)
target_link_libraries(polyfill PRIVATE maz::maz maz_warnings)
