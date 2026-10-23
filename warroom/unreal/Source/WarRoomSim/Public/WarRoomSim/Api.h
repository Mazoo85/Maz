// Export macro: Unreal's build tool defines WARROOMSIM_API for the module (DLL export
// inside it, import outside). Standalone builds get the empty fallback.
#pragma once
#ifndef WARROOMSIM_API
#define WARROOMSIM_API
#endif
