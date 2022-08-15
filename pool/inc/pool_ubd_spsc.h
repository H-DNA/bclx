#ifndef POOL_UBD_SPSC_H
#define POOL_UBD_SPSC_H

#include <vector>		// std::vector...
#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...

namespace dds
{

using namespace bclx;

struct header
{
	gptr<header>	next;
};

template<typename T>
struct block
{
	header	head;
	T	data;
};

template<typename T>
class list_seq
{
public:
	list_seq();
	~list_seq();
	bool empty() const;
	void set(const gptr<T>& h, const uint64_t& s);
	gptr<T> pop_front();
	gptr<T> pop_front(const uint64_t& s);

private:
	gptr<T>		head;
	uint64_t	size;
};

class list_seq2
{
public:
	list_seq2();
	~list_seq2();
	bool empty() const;
	void set(const gptr<header>& h, const gptr<header>& t);
	void get(gptr<header>& h, gptr<header>& t) const;
	gptr<header> pop_front();
	void append(const list_seq2& slist);
	void print() const;

private:
	gptr<header>	head;
	gptr<header>	tail;
};

template<typename T>
struct list_seq3
{
	list_seq<T>	contig;
	list_seq2	ncontig;
};

template<typename T>
class pool_ubd_spsc
{
public:
	pool_ubd_spsc(const uint64_t& host);
	~pool_ubd_spsc();
	void clear();
	void put(const std::vector<gptr<T>>& vals);
	bool get(list_seq2& slist);

private:
	const header		NULL_PTR;
	const uint64_t		HOST;
	gptr<gptr<header>>	head_ptr;
	gptr<header>		head_addr;
	gptr<block<T>>		dummy;
};

} /* namespace dds */

/* Implementation of list_seq */

template<typename T>
dds::list_seq<T>::list_seq() : head{nullptr}, size{0} {}

template<typename T>
dds::list_seq<T>::~list_seq() {}

template<typename T>
bool dds::list_seq<T>::empty() const
{
	return (size == 0);
}

template<typename T>
void dds::list_seq<T>::set(const gptr<T>& h, const uint64_t& s)
{
	head = h;
	size = s;
}

// Precondition: list_seq is not empty
template<typename T>
bclx::gptr<T> dds::list_seq<T>::pop_front()
{
	--size;
	return head++;
}

// Precondition: list_seq is not empty
template<typename T>
bclx::gptr<T> dds::list_seq<T>::pop_front(const uint64_t& s)
{
	if (size < s)
	{
		printf("[%lu]ERROR: MEMORY RUNS OUT\n", BCL::rank());
		return nullptr;
	}

	size -= s;
	gptr<T> ptr = head;
	head += s;
	return ptr;
}

/**/

/* Implementation of list_seq2 */

dds::list_seq2::list_seq2() : head{nullptr}, tail{nullptr} {}

dds::list_seq2::~list_seq2() {}

bool dds::list_seq2::empty() const
{
	return (head == nullptr);
}

void dds::list_seq2::set(const gptr<header>& h, const gptr<header>& t)
{
	head = h;
	tail = t;
}

void dds::list_seq2::get(gptr<header>& h, gptr<header>& t) const
{
	h = head;
	t = tail;
}

// Precondition: list_seq2 is not empty
bclx::gptr<dds::header> dds::list_seq2::pop_front()
{
	gptr<header> res = head;
	head = bclx::load(head).next;
	if (head == nullptr)
		tail = nullptr;
	return res;
}

void dds::list_seq2::append(const list_seq2& slist)
{
	gptr<header> slist_head, slist_tail;
	slist.get(slist_head, slist_tail);

	if (head == nullptr)
	{
		head = slist_head;
		tail = slist_tail;
	}
	else // if (head != nullptr)
	{
		header tmp;
		tmp.next = slist_head;
		bclx::store(tmp, tail);	// local
		tail = slist_tail;
	}
}

void dds::list_seq2::print() const
{
	for (gptr<header> tmp = head; tmp != tail; tmp = bclx::load(tmp).next)
		printf("[%lu]<%u, %u>\n", BCL::rank(), tmp.rank, tmp.ptr);
	printf("[%lu]<%u, %u>\n", BCL::rank(), tail.rank, tail.ptr);
}

/**/

/* Implementation of pool_ubd_spsc */

template<typename T>
dds::pool_ubd_spsc<T>::pool_ubd_spsc(const uint64_t& host)
				: HOST{host}
{
	// initialize the pool locally
	if (BCL::rank() == HOST)
	{
		head_ptr = BCL::alloc<gptr<header>>(1);
		dummy = BCL::alloc<block<T>>(1);
		head_addr = {dummy.rank, dummy.ptr};
		bclx::store(head_addr, head_ptr);
	}

	// broadcast	
	head_addr = BCL::broadcast(head_addr, HOST);
	head_ptr = BCL::broadcast(head_ptr, HOST);
}

template<typename T>
dds::pool_ubd_spsc<T>::~pool_ubd_spsc() {}

template<typename T>
void dds::pool_ubd_spsc<T>::clear()
{
	if (BCL::rank() == HOST)
	{
		BCL::dealloc<block<T>>(dummy);
		BCL::dealloc<gptr<header>>(head_ptr);
	}
}

template<typename T>
void dds::pool_ubd_spsc<T>::put(const std::vector<gptr<T>>& vals)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<header>	buffer;
	header			tmp;
	for (uint64_t i = 0; i < vals.size(); ++i)
	{
		disp.push_back(int64_t(vals[i].ptr) - int64_t(vals[0].ptr));
		if (i != vals.size() - 1)
			tmp.next = {vals[i + 1].rank, vals[i + 1].ptr - sizeof(header)};
		else // if (i == vals.size() - 1)
			tmp.next = head_addr;
		buffer.push_back(tmp);
	}

	// Link the remote global pointers and the head pointer of the pool together
	gptr<rll_t>	base = {vals[0].rank, vals[0].ptr - sizeof(header)};
	rll_t		rll(disp);
	bclx::rput_sync(buffer, base, rll);	// remote

	// Cache the head address of the pool
	head_addr = {base.rank, base.ptr};

	// Update the head pointer of the pool
	bclx::aput_sync(head_addr, head_ptr);	// remote
}

template<typename T>
bool dds::pool_ubd_spsc<T>::get(list_seq2& slist)
{
	// Get the current head address of the pool
	gptr<header> curr = bclx::aget_sync(head_ptr);	// local

	// Check if the head has been changed by the producer
	if (curr == head_addr)
		return false;	// the queue is empty now

	// Update the list
	slist.set(bclx::load(curr).next, head_addr);	// local

	// Cache the head of the pool
	head_addr = curr;

	return true;
}

/**/

#endif /* POOL_UBD_SPSC_H */
