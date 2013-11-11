#ifndef XARR_CHUNK_HPP__
#define XARR_CHUNK_HPP__

#include <cstddef>

template <typename T>
struct Chunk {
	T       *ch_ptr;
	size_t   ch_cnt; // number of Ts starting from @ch_ptr
};

#endif /* XARR_CHUNKS_HPP__ */
