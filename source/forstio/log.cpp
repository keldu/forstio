#include "log.h"

namespace saw {
LogIo::LogIo(EventLoop &loop) : loop{loop} {}

Log::Log(LogIo &central, EventLoop &loop) : central{central}, loop{loop} {}
} // namespace saw