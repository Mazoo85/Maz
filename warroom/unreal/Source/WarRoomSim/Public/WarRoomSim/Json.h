// The one place the JSON library is included. Unreal builds without exceptions,
// so the library is told never to throw: a bad parse yields a "discarded" value.
#pragma once
#ifdef UE_BUILD_SHIPPING
#include "HAL/Platform.h"   // THIRD_PARTY_INCLUDES_START/END
#endif
#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702 4996 4267 4244 4456 4457 4459 4505 4800)
#endif
#ifdef THIRD_PARTY_INCLUDES_START
THIRD_PARTY_INCLUDES_START
#endif
#include "../../ThirdParty/nlohmann/json.hpp"
#ifdef THIRD_PARTY_INCLUDES_END
THIRD_PARTY_INCLUDES_END
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
namespace warroom { using Json = nlohmann::json; }
