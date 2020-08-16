#pragma once

#include "common.h"

namespace gin {
class EventLoop;
class LogIo;
class Log {
public:
	enum class Type : uint8_t { Info, Warning, Error, Debug };

private:
	LogIo &central;
	EventLoop &loop;

public:
	Log(LogIo &central, EventLoop &loop);
};
class LogIo {
private:
	EventLoop &loop;

public:
	LogIo(EventLoop &loop);
};
} // namespace gin