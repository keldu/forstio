#include "suite/suite.h"

#include "source/forstio/io.h"

namespace {
SAW_TEST("Io Socket Pair"){
	using namespace saw;

	auto err_or_aio = setupAsyncIo();
	SAW_EXPECT(!err_or_aio.isError(), "Async Io setup failed");
	AsyncIoContext& aio = err_or_aio.value();
	WaitScope wait_scope{aio.event_loop};

	SAW_EXPECT(aio.io, "Faulty async io context created");
	auto err_or_sp = aio.io->network().socketPair();
	SAW_EXPECT(!err_or_sp.isError(), "Couldn't create socket pair");
	
	SocketPair& sp = err_or_sp.value();
	
	uint8_t buffer_out[7] = {1,2,3,4,5,6,7};
	sp.stream[0]->write(buffer_out,7);
	uint8_t buffer_in[7] = {0,0,0,0,0,0,0};
	sp.stream[1]->read(buffer_in, 7);

	SAW_EXPECT(buffer_in[0] == 1, "Element 1 failed");
	SAW_EXPECT(buffer_in[1] == 2, "Element 2 failed");
	SAW_EXPECT(buffer_in[2] == 3, "Element 3 failed");
	SAW_EXPECT(buffer_in[3] == 4, "Element 4 failed");
	SAW_EXPECT(buffer_in[4] == 5, "Element 5 failed");
	SAW_EXPECT(buffer_in[5] == 6, "Element 6 failed");
	SAW_EXPECT(buffer_in[6] == 7, "Element 7 failed");
}
}
