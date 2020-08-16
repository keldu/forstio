#include "log.h"

namespace gin {
LogIo::LogIo(EventLoop &loop) : loop{loop} {}

Log::Log(LogIo &central, EventLoop &loop) : central{central}, loop{loop} {}
} // namespace gin