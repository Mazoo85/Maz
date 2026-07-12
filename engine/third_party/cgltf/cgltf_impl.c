// Single translation unit that instantiates the cgltf implementation. Kept separate
// from engine code so it can be compiled with relaxed warnings (the engine builds its
// own sources with -Wall -Wextra -Wconversion -Werror, which vendored C would trip).
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
