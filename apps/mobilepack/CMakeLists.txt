add_executable(mobilepack main.cpp)

target_link_libraries(mobilepack PRIVATE maz::maz maz_warnings)

# mobilepack is the command-line mobile exporter: it drives io::planMobileBundle to stage a complete
# Android/iOS bundle layout (game .so/.app, assets, generated manifest) from a desktop build tree, then
# verifies every planned file landed. Used by tools/package_mobile.sh; --selftest is a headless ctest.
