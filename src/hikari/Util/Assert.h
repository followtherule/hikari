#pragma once

#include "hikari/Util/Logger.h"

#ifdef HKR_DEBUG
#include <filesystem>
#ifdef __GNUC__
#endif
#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#define HKR_ASSERT(condition)                                         \
  if (!(condition)) {                                                 \
    HKR_CRITICAL("Assertion '{}' failed at {}:{}: {}.", #condition,   \
                 std::filesystem::path(__FILE__).filename().string(), \
                 __LINE__, __PRETTY_FUNCTION__);                      \
    std::abort();                                                     \
  }
#else
#define HKR_ASSERT(condition)
#endif
