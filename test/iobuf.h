#ifndef _IOBUF_H
#define _IOBUF_H

#include<atomic>
#include<string>
#include<openssl/ssl.h>

class IOBuf {
public:
	static const size_t DEFAULT_BLOCK_SIZE = 8192;
	static const size_t MAX_BLOCK_SIZE = (1 << 16);
	static const size_t BLOCK_SIZE = DEFAULT_BLOCK_SIZE;
	static const size_t INITIAL_CAP = 32;

	struct Block;

	struct BlockRef {
		uint32_t offset; //BigView::magic
		uint32_t length;
		Block* block;
	};

	// buf is a tiny queue usually
	struct SmallView {
		BlockRef refs[2];
	};

	struct BigView {
		int32_t magic;
		uint32_t start;
		BlockRef* refs;
		uint32_t nref;
		uint32_t cap_mask;
		size_t nbytes;

		const BlockRef& ref_at(uint32_t i) const {
			return refs[(start + i) & cap_mask];
		}
		BlockRef& ref_at(uint32_t i) {
			return refs[(start + i) & cap_mask];
		}
		
		uint32_t capacity() const { return cap_mask + 1; }
	};


	IOBuf();

	void clear();
	bool empty() const { return _small() ? !_sv.refs[0].block : !_bv.nbytes; }
	size_t length() const { return _small() ? (_sv.refs[0].length + _sv.refs[1].length) : _bv.nbytes; }
	size_t size() const { return length(); }

	size_t pop_front(size_t n);

	int append(void const* data, size_t count);
	int append(char const* s);

	ssize_t cut_into_SSL_channel(SSL* ssl, int* ssl_error);

	size_t cutn(IOBuf* out, size_t n);
	size_t cutn(void* out, size_t n);
	size_t cutn(std::string* out, size_t n);
	size_t copy_to(void* buf, size_t n = (size_t)-1L, size_t pos = 0) const;
	size_t copy_to(std::string* s, size_t n = (size_t)-1L, size_t pos = 0) const;

	std::string to_string() const { std::string s; copy_to(&s); return s; }
protected:
	bool _small() const { return _bv.magic >= 0; }

	template <bool MOVE>
	void _push_or_move_back_ref_to_smallview(const BlockRef&);
	template <bool MOVE>
	void _push_or_move_back_ref_to_bigview(const BlockRef&);

	void _push_back_ref(const BlockRef& r) {
		if (_small()) {
			return _push_or_move_back_ref_to_smallview<false>(r);
		} else {
			return _push_or_move_back_ref_to_bigview<false>(r);
		}
	}

	size_t _ref_num () const { return _small() ? (!!_sv.refs[0].block + !!_sv.refs[1].block) : _bv.nref; }

	BlockRef& _ref_at(size_t i) { return _small() ? _sv.refs[i] : _bv.ref_at(i); }
	const BlockRef& _ref_at(size_t i) const { return _small() ? _sv.refs[i] : _bv.ref_at(i); }

	int _pop_front_ref();
	BlockRef& _front_ref() { return _small() ? _sv.refs[0] : _bv.ref_at(_bv.start); }
private:
	union {
		BigView _bv;
		SmallView _sv;
	};
};

class IOPortal : public IOBuf {
public:
	IOPortal() : _block(NULL) { }
	//IOPortal(const IOPortal& rhs) : IOBuf(rhs), _block(NULL) { }
	~IOPortal();

	ssize_t append_from_file_descriptor(int fd, size_t max_count);

	ssize_t append_from_SSL_channel(SSL* ssl, int* ssl_error);
	
	void clear();

	void return_cached_blocks();

private:
	static void return_cached_blocks_impl(Block*);

	Block* _block;
};

#endif
