#pragma once

#include <spdlog/logger.h>

#include <memory>

// #include <glm/gtx/string_cast.hpp>

namespace hkr {

class Logger {
public:
  static void Init();
  static std::shared_ptr<spdlog::logger> sEngineLogger;
};

}  // namespace hkr

#ifdef HKR_DEBUG
// Engine log
// NOLINTBEGIN
#define HKR_TRACE(...) hkr::Logger::sEngineLogger->trace(__VA_ARGS__)
#define HKR_INFO(...) hkr::Logger::sEngineLogger->info(__VA_ARGS__)
#define HKR_WARN(...) hkr::Logger::sEngineLogger->warn(__VA_ARGS__)
#define HKR_ERROR(...) hkr::Logger::sEngineLogger->error(__VA_ARGS__)
#define HKR_CRITICAL(...) hkr::Logger::sEngineLogger->critical(__VA_ARGS__)
// NOLINTEND

// Game log
// #define GAME_TRACE(...) Engine::Logger::sGameLogger->trace(__VA_ARGS__)
// #define GAME_INFO(...) Engine::Hazel::Logger::sGameLogger->info(__VA_ARGS__)
// #define GAME_WARN(...) Engine::Hazel::Logger::sGameLogger->warn(__VA_ARGS__)
// #define GAME_ERROR(...) Engine::Logger::sGameLogger->error(__VA_ARGS__)
// #define GAME_CRITICAL(...) Engine::Logger::sGameLogger->critical(__VA_ARGS__)
#else
// Engine log
#define HKR_TRACE(...)
#define HKR_INFO(...)
#define HKR_WARN(...)
#define HKR_ERROR(...)
#define HKR_FATAL(...)
#define HKR_CRITICAL(...)

// Game log
// #define GAME_TRACE(...)
// #define GAME_INFO(...)
// #define GAME_WARN(...)
// #define GAME_ERROR(...)
// #define GAME_FATAL(...)
// #define GAME_CRITICAL(...)
#endif

// template<glm::length_t L, typename T, glm::qualifier Q>
// struct fmt::formatter<glm::vec<L, T, Q>> : fmt::formatter<std::string>
// {
//   auto format(const glm::vec<L, T, Q> &vector, format_context &ctx) const
//   {
//     return fmt::formatter<std::string>::format(glm::to_string(vector), ctx);
//   }
// };
//
// template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
// struct fmt::formatter<glm::mat<C, R, T, Q>> : fmt::formatter<std::string>
// {
//   auto format(const glm::mat<C, R, T, Q> &matrix, format_context &ctx) const
//   {
//     return fmt::formatter<std::string>::format(glm::to_string(matrix), ctx);
//   }
// };
//
// template<typename T, glm::qualifier Q> struct fmt::formatter<glm::qua<T, Q>>
// : fmt::formatter<std::string>
// {
//   auto format(const glm::qua<T, Q> &quaternion, format_context &ctx) const
//   {
//     return fmt::formatter<std::string>::format(glm::to_string(quaternion),
//     ctx);
//   }
// };
