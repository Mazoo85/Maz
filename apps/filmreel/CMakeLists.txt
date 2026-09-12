add_executable(filmreel main.cpp)
find_package(Threads REQUIRED)
target_link_libraries(filmreel PRIVATE maz::maz maz_warnings Threads::Threads)
