#pragma once

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>

namespace bclx
{

/* Interfaces */

struct block
{
	gptr<block> next;
};

struct header
{
	gptr<gptr<block>>	head_ptr;
	uint64_t		size_class;
};

class list_seq
{
public:
	list_seq();
	~list_seq();
	uint64_t size() const;
	bool empty() const;
	void set_obj_size(const uint64_t& os);
	void push(const gptr<void>& h, const uint64_t& s);
	gptr<void> pop();

private:
	gptr<void>	head;
	uint64_t	len;
	uint64_t	obj_size;
}; /* class list_seq */

class list_seq2
{
public:
	list_seq2();
	~list_seq2();
	bool empty() const;
	void set(const gptr<block>& h, const gptr<block>& t);
	void get(gptr<block>& h, gptr<block>& t) const;
	void push(const gptr<block>& h, const gptr<block>& t);
	void push(const list_seq2& list);
	gptr<void> pop();

private:
	gptr<block>	head;
	gptr<block>	tail;
}; /* class list_seq2 */

class list_seq3
{
public:
	list_seq3();
	~list_seq3();
	uint64_t size() const;
	bool empty() const;
	void set_batch_num(const uint64_t& bn);
	void push(const gptr<void>& ptr);
	void push(const list_seq2& list);
	gptr<void> pop();

private:
	std::vector<gptr<void>>	processed;
	list_seq2		unprocessed;
	uint64_t		batch_num;
}; /* class list_seq3 */

} /* namespace bclx */

/* Implementation of class list_seq */

bclx::list_seq::list_seq()
	: head{nullptr}, len{0} {}

bclx::list_seq::~list_seq() {}

uint64_t bclx::list_seq::size() const
{
	return len;
}

bool bclx::list_seq::empty() const
{
	return (len == 0);
}

void bclx::list_seq::set_obj_size(const uint64_t& os)
{
	obj_size = os;
}

void bclx::list_seq::push(const gptr<void>&	h,
				const uint64_t& s)
{
	head = h;
	len = s;
}

// Precondition: The list is not currently empty
bclx::gptr<void> bclx::list_seq::pop()
{
	gptr<void> res = head;
	--len;
	head.ptr += obj_size;
	return res;
}

/**/

/* Implementation of class list_seq2 */

bclx::list_seq2::list_seq2()
	: head{nullptr}, tail{nullptr} {}

bclx::list_seq2::~list_seq2() {}

bool bclx::list_seq2::empty() const
{
	return (head == nullptr);
}

void bclx::list_seq2::set(const gptr<block>&		h,
				const gptr<block>&	t)
{
	head = h;
	tail = t;
}

void bclx::list_seq2::get(gptr<block>& 	h,
			gptr<block>&	t) const
{
	h = head;
	t = tail;
}

// Precondition: the input list is not currently empty
void bclx::list_seq2::push(const gptr<block>&	h,
			const gptr<block>& 	t)
{
	if (head == nullptr)
	{
		head = h;
		tail = t;
	}
	else // if (head != nullptr)
	{
		block tmp;
		tmp.next = h;
		bclx::store(tmp, tail);     // local access
		tail = t;
	}
}

void bclx::list_seq2::push(const list_seq2& list)
{
	gptr<block> h, t;
	list.get(h, t);
	push(h, t);
}

// Precondition: the list is not currently empty 
bclx::gptr<void> bclx::list_seq2::pop()
{
	gptr<void> res = {head.rank, head.ptr};
	if (head == tail)
		head = tail = nullptr;
	else // if (head != tail)
		head = bclx::load(head).next;	// local access
	return res;
}

/**/

/* Implementation of class list_seq3 */

bclx::list_seq3::list_seq3() {}

bclx::list_seq3::~list_seq3() {}

uint64_t bclx::list_seq3::size() const
{
	return processed.size();
}

bool bclx::list_seq3::empty() const
{
	return (processed.empty() && unprocessed.empty());
}

void bclx::list_seq3::set_batch_num(const uint64_t& bn)
{
	batch_num = bn;
}

// Precondition: @ptr is not a null gptr
void bclx::list_seq3::push(const gptr<void>& ptr)
{
	processed.push_back(ptr);
}

// Precondition: @list is not currently empty
void bclx::list_seq3::push(const list_seq2& list)
{
        gptr<block> head, tail;
        list.get(head, tail);

        for (uint64_t i = 0; i < batch_num - 1; ++i)
        {
                processed.push_back({head.rank, head.ptr});
                head = bclx::load(head).next;	// local access
        }
	processed.push_back({head.rank, head.ptr});

        if (head != tail)
                unprocessed.push(bclx::load(head).next, tail);	// local access
}

// Precondition: the list is not currently empty
bclx::gptr<void> bclx::list_seq3::pop()
{
	if (!unprocessed.empty())
		return unprocessed.pop();

	gptr<void> res = processed.back();
	processed.pop_back();
	return res;
}

/**/

