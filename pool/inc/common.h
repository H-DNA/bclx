#ifndef COMMON_H
#define COMMON_H

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
class list_seq2
{
public:
	list_seq2();
	~list_seq2();
	bool empty() const;
	void set(const gptr<header>& h, const gptr<header>& t);
	void get(gptr<header>& h, gptr<header>& t) const;
	void push(const gptr<T>& ptr);
	gptr<header> pop();
	void append(const list_seq2<T>& slist);
	void print() const;

private:
	gptr<header>		head;
	gptr<header>		tail;
};

} /* namespace dds */

/* Implementation of list_seq2 */

template<typename T>
dds::list_seq2<T>::list_seq2() : head{nullptr}, tail{nullptr} {}

template<typename T>
dds::list_seq2<T>::~list_seq2() {}

template<typename T>
bool dds::list_seq2<T>::empty() const
{
	return (head == nullptr);
}

template<typename T>
void dds::list_seq2<T>::set(const gptr<header>& h, const gptr<header>& t)
{
	head = h;
	tail = t;
}

template<typename T>
void dds::list_seq2<T>::get(gptr<header>& h, gptr<header>& t) const
{
	h = head;
	t = tail;
}

template<typename T>
void dds::list_seq2<T>::push(const gptr<T>& ptr)
{
	header tmp;
	tmp.next = {ptr.rank, ptr.ptr - sizeof(header)};
	if (head == nullptr)
	{
		header null;
		null.next = nullptr;
		bclx::store(null, tmp.next);		// local
		head = tail = tmp.next;
	}
	else // if (head != nullptr)
	{
		header tmp_head;
		tmp_head.next = head;
		bclx::store(tmp_head, tmp.next);	// local
		head = tmp.next;
	}
}

// Precondition: list_seq2 is not empty
template<typename T>
bclx::gptr<dds::header> dds::list_seq2<T>::pop()
{
	gptr<header> res = head;
	head = bclx::load(head).next;	// local
	if (head == nullptr)
		tail = nullptr;
	return res;
}

template<typename T>
void dds::list_seq2<T>::append(const list_seq2& slist)
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

template<typename T>
void dds::list_seq2<T>::print() const
{
	for (gptr<header> tmp = head; tmp != tail; tmp = bclx::load(tmp).next)
		printf("[%lu]<%u, %u>\n", BCL::rank(), tmp.rank, tmp.ptr);
	printf("[%lu]<%u, %u>\n", BCL::rank(), tail.rank, tail.ptr);
}

/**/

#endif /* COMMON_H */
