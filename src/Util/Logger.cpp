#include "hikari/Util/Logger.h"

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

namespace hkr {

std::shared_ptr<spdlog::logger> Logger::sEngineLogger = nullptr;

void Logger::Init() {
  auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  sink->set_pattern("%^[%T] %n: %v%$");
  sEngineLogger = std::make_shared<spdlog::logger>("HKR", sink);
  sEngineLogger->set_level(spdlog::level::trace);
  sEngineLogger->flush_on(spdlog::level::trace);
}

}  // namespace hkr
