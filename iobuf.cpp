#include<openssl/ssl.h>
#include<assert.h>
#include"iobuf.h"

std::atomic_size_t g_nblock = { 0 };
std::atomic_size_t g_blockmem = { 0 };

struct IOBuf::Block {

	std::atomic_int nshared;
	uint16_t size;
	uint16_t cap;
	Block* portal_next;
	char data[0];

	explicit Block(size_t block_size)
		: nshared(1), size(0), cap(block_size - offsetof(Block, data)), portal_next(NULL) {
			assert(block_size <= MAX_BLOCK_SIZE);
			g_nblock.fetch_add(1, std::memory_order_relaxed);
			g_blockmem.fetch_add(block_size, std::memory_order_relaxed);
		}
	
	void inc_ref() {
		nshared.fetch_add(1, std::memory_order_relaxed);
	}

	void dec_ref() {
		if (nshared.fetch_sub(1, std::memory_order_release) == 1) {
			//TODO
			g_nblock.fetch_sub(1, std::memory_order_relaxed);
			g_blockmem.fetch_sub(cap + offsetof(Block, data), std::memory_order_relaxed);
			this->~Block();
			::free(this);
		}
	}


	int ref_count() const {
		return nshared.load(std::memory_order_relaxed);
	}

	bool full() const { return size >= cap; }
	size_t left_space() const { return cap - size; }

};

inline IOBuf::Block* create_block(const size_t block_size) {
	void* mem = ::malloc(block_size);
	if(mem != NULL) {
		return new (mem) IOBuf::Block(block_size);
	}
	return NULL;
}
inline IOBuf::Block* create_block() {
	return create_block(IOBuf::DEFAULT_BLOCK_SIZE);
}

struct TLSData {
	IOBuf::Block* block_head;

	int num_blocks;
};

static TLSData g_tls_data = { NULL, 0 };

inline IOBuf::Block* acquire_tls_block() {
	TLSData& tls_data = g_tls_data;
	IOBuf::Block* b = tls_data.block_head;
	if (!b) {
		return create_block();
	}
	if (b->full()) {
		IOBuf::Block* const saved_next = b->portal_next;
		b->dec_ref();
		tls_data.block_head = saved_next;
		--tls_data.num_blocks;
		b = saved_next;
		if (!b) {
			return create_block();
		}
	}
	tls_data.block_head = b->portal_next;
	--tls_data.num_blocks;
	b->portal_next = NULL;
	return b;
}

inline void release_tls_block(IOBuf::Block* b) {
	if (!b) {
		return;
	}
	TLSData& tls_data = g_tls_data;
	if (b->full()) {
		b->dec_ref();
	} else if (tls_data.num_blocks > 8) {
	}
}

inline void release_tls_block_chain(IOBuf::Block* b) {
	TLSData& tls_data = g_tls_data;
	size_t n = 0;
	if (tls_data.num_blocks >= 8) {//TODO
	}
}

inline void reset_block_ref(IOBuf::BlockRef& ref) {
	ref.offset = 0;
	ref.length = 0;
	ref.block = NULL;
}
inline IOBuf::BlockRef* acquire_blockref_array() {
	//TODO
	return new IOBuf::BlockRef[IOBuf::INITIAL_CAP];
}
inline IOBuf::BlockRef* acquire_blockref_array(size_t cap) {
	//TODO
	return new IOBuf::BlockRef[cap];
}
inline void release_blockref_array(IOBuf::BlockRef* refs, size_t cap) {
	delete[] refs;
}

template <bool MOVE>
void IOBuf::_push_or_move_back_ref_to_smallview(const BlockRef& r) {
	BlockRef* const refs = _sv.refs;
	if (NULL == refs[0].block) {
		refs[0] = r;
		if (!MOVE) {
			r.block->inc_ref();
		}
		return;
	}
	if (NULL == refs[1].block) {
		if (refs[0].block == r.block && refs[0].offset + refs[0].length == r.offset) {
			refs[0].length += r.length;
			if (MOVE) {
				r.block->dec_ref();
			}
			return;
		}
		refs[1] = r;
		if (!MOVE) {
			r.block->inc_ref();
		}
		return;
	}
	if (refs[1].block == r.block && refs[1].offset + refs[1].length == r.offset) {
		refs[1].length += r.length;
		if (MOVE) {
			r.block->dec_ref();
		}
		return;
	}

	BlockRef* new_refs = acquire_blockref_array();
	new_refs[0] = refs[0];
	new_refs[1] = refs[1];
	new_refs[2] = r;
	const size_t new_nbytes = refs[0].length + refs[1].length + r.length;
	if (!MOVE) {
		r.block->inc_ref();
	}
	_bv.magic = -1;
	_bv.start = 0;
	_bv.refs = new_refs;
	_bv.nref = 3;
	_bv.cap_mask = INITIAL_CAP - 1;
	_bv.nbytes = new_nbytes;
}
template void IOBuf::_push_or_move_back_ref_to_smallview<true>(const BlockRef&);
template void IOBuf::_push_or_move_back_ref_to_smallview<false>(const BlockRef&);

template <bool MOVE>
void IOBuf::_push_or_move_back_ref_to_bigview(const BlockRef& r) {
	BlockRef& back = _bv.ref_at(_bv.nref - 1);
	if (back.block == r.block && back.offset + back.length == r.offset) { //merge
		back.length += r.length;
		_bv.nbytes += r.length;
		if (MOVE) {
			r.block->dec_ref();
		}
		return;
	}
	if (_bv.nref != _bv.capacity()) {
		_bv.ref_at(_bv.nref++) = r;
		_bv.nbytes += r.length;
		if(!MOVE) {
			r.block->inc_ref();
		}
		return;
	}
	const uint32_t new_cap = _bv.capacity() * 2;
	BlockRef* new_refs = acquire_blockref_array(new_cap);
	for (uint32_t i = 0; i < _bv.nref; ++i) {
		new_refs[i] = _bv.ref_at(i);
	}
	new_refs[_bv.nref++] = r;

	_bv.start = 0;
	release_blockref_array(_bv.refs, _bv.capacity());
	_bv.refs = new_refs;
	_bv.cap_mask = new_cap - 1;
	_bv.nbytes += r.length;
	if (!MOVE) {
		r.block->inc_ref();
	}
}
template void IOBuf::_push_or_move_back_ref_to_bigview<true>(const BlockRef&);
template void IOBuf::_push_or_move_back_ref_to_bigview<false>(const BlockRef&);

size_t IOBuf::copy_to(void* d, size_t n, size_t pos) const {
	const size_t nref = _ref_num();
	size_t offset = pos;
	size_t i = 0;
	for (; offset != 0 && i < nref; ++i) {
		IOBuf::BlockRef const& r = _ref_at(i);
		if (offset < (size_t)r.length) {
			break;
		}
		offset -= r.length;
	}
	size_t m = n;
	for (; m != 0 && i < nref; ++i) {
		IOBuf::BlockRef const& r = _ref_at(i);
		const size_t nc = std::min(m, (size_t)r.length - offset);
		memcpy(d, r.block->data + r.offset + offset, nc);
		offset = 0;
		d = (char*)d +nc;
		m -= nc;
	}
	return n - m;
}

size_t IOBuf::copy_to(std::string* s, size_t n, size_t pos) const {
	const size_t len = length();
	if (n + pos > len) {
		if (len <= pos) {
			return 0;
		}
		n = len - pos;
	}
	s->resize(n);
	return copy_to(&(*s)[0], n, pos);
}

void IOBuf::clear() {
	if (_small()) {
		if (_sv.refs[0].block != NULL) {
			_sv.refs[0].block->dec_ref();
			reset_block_ref(_sv.refs[0]);

			if (_sv.refs[1].block != NULL) {
				_sv.refs[1].block->dec_ref();
				reset_block_ref(_sv.refs[1]);
			}
		}
	} else {
		for (uint32_t i = 0; i < _bv.nref; ++i) {
			_bv.ref_at(i).block->dec_ref();
		}
		release_blockref_array(_bv.refs, _bv.capacity());
		new (this) IOBuf;
	}
}

inline void IOPortal::return_cached_blocks() {
	if (_block) {
		return_cached_blocks_impl(_block);
		_block = NULL;
	}
}

void IOPortal::return_cached_blocks_impl(Block* b) {
	release_tls_block_chain(b);
}

IOPortal::~IOPortal() { return_cached_blocks(); }

void IOPortal::clear() {
	IOBuf::clear();
	return_cached_blocks();
}

ssize_t IOPortal::append_from_SSL_channel(SSL* ssl, int* ssl_error) {
	if(!_block) {
		_block = acquire_tls_block();
		if(!_block) {
			//TODO
			*ssl_error = -1;
			return -1;
		}
	}
	const int nr = SSL_read(ssl, _block->data + _block->size, _block->left_space());
	*ssl_error = SSL_get_error(ssl, nr);
	if (nr > 0) {
		const IOBuf::BlockRef r = { (uint32_t)_block->size, (uint32_t)nr, _block };
		_push_back_ref(r);
		_block->size += nr;
		if(_block->full()) {//TODO
			Block* const saved_next = _block->portal_next;
			_block->dec_ref();
			_block = saved_next;
		}
	}
	return nr;
}
