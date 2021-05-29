#pragma once

#ifndef GIN_UNIX
#error "Don't include this"
#endif

#include "../io-unix.h"
#include "./tls.h"

namespace gin {
class TlsUnixIoStream final : public IoStream, public IFdOwner, public StreamReaderAndWriter {
public:
	TlsUnixIoStream(UnixEventPort &event_port, int file_descriptor, int fd_flags,
				 uint32_t event_mask);

	
};
}