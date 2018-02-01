#include<gtest/gtest.h>
#include<set>
#include "iobuf.h"

extern void* (*blockmem_allocate)(size_t);
extern void (*blockmem_deallocate)(void*);
extern IOBuf::Block* get_tls_block_head();
extern void remove_tls_block_chain();
extern IOBuf::Block* get_portal_next(IOBuf::Block const*);

static std::set<void*> s_set;

void* debug_block_allocate(size_t block_size) {
	void* b = operator new (block_size, std::nothrow);
	s_set.insert(b);
	return b;
}

void debug_block_deallocate(void* b) {
	if (1ul != s_set.erase(b)) {
		ASSERT_TRUE(false) << "Bad block=" << b;
	} else {
		operator delete(b);
	}
}

inline bool is_debug_allocator_enabled() {
	return (blockmem_allocate == debug_block_allocate);
}

void install_debug_allocator() {
	if (!is_debug_allocator_enabled()) {
		remove_tls_block_chain();
		blockmem_allocate = debug_block_allocate;
		blockmem_deallocate = debug_block_deallocate;
	}

}

static void check_memory_leak() {
	if (is_debug_allocator_enabled()) {
		IOBuf::Block* p = get_tls_block_head();
		size_t n = 0;
		while(p) {
			ASSERT_TRUE(s_set.find(p) == s_set.end()) << "Memory leak: " << p;
			p = get_portal_next(p);
			++n;
		}
		ASSERT_EQ(n, s_set.size());
		//ASSERT_EQ(n, get_tls_block_count());
	}
}

class IOBufTest : public ::testing::Test {
protected:
	IOBufTest() { };
	virtual ~IOBufTest() { };
	virtual void SetUp() { };
	virtual void TearDown() { check_memory_leak(); };
};

TEST_F(IOBufTest, pop_front) {
	install_debug_allocator();

	IOBuf buf;
	ASSERT_EQ(0UL, buf.pop_front(1));

	std::string s = "hello";
	buf.append(s.c_str());
	ASSERT_EQ(0UL, buf.pop_front(0));
	ASSERT_EQ(s, buf.to_string());
	ASSERT_EQ(1UL, buf.pop_front(1));
	s.erase(0, 1);
	ASSERT_EQ(s, buf.to_string());
	ASSERT_EQ(s.length(), buf.length());
	ASSERT_FALSE(buf.empty());
	ASSERT_EQ(s.length(), buf.pop_front(INT_MAX));
	s.clear();
	ASSERT_EQ(s, buf.to_string());
	ASSERT_EQ(0UL, buf.length());
	ASSERT_TRUE(buf.empty());	

}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
