#pragma once

#include "hikari/Util/Logger.h"

#ifdef HKR_DEBUG
#include <filesystem>
#ifdef __GNUC__
#endif
#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#define VK_CHECK(...)                 \
  do {                                \
    VkResult result = __VA_ARGS__;    \
    HKR_ASSERT(result == VK_SUCCESS); \
  } while (0)
#define VK_LABEL(type, object, name)                                           \
  do {                                                                         \
    VkDebugUtilsObjectNameInfoEXT name_info = {                                \
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};                   \
    name_info.objectType = type;                                               \
    name_info.objectHandle = (uint64_t)object;                                 \
    name_info.pObjectName = name;                                              \
    auto vkSetDebugUtilsObjectNameEXT =                                        \
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(                    \
            vkGetInstanceProcAddr(mInstance, "vkSetDebugUtilsObjectNameEXT")); \
    vkSetDebugUtilsObjectNameEXT(mDevice, &name_info);                         \
  } while (0)

#else
#define VK_CHECK(...) __VA_ARGS__;
#define VK_LABEL(type, object, name)
#endif
