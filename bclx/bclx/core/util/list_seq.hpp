#pragma once

namespace bclx
{

template<typename T>
class list_seq
{
public:
	list_seq();
	~list_seq();
	bool empty() const;
	void set(const gptr<T>& h, const uint64_t& s);
	gptr<T> pop();
	gptr<T> pop(const uint64_t& s);

private:
	gptr<T>		head;
	uint64_t	size;
};

} /* namespace bclx */

/* Implementation of list_seq */

template<typename T>
bclx::list_seq<T>::list_seq() : head{nullptr}, size{0} {}

template<typename T>
bclx::list_seq<T>::~list_seq() {}

template<typename T>
bool bclx::list_seq<T>::empty() const
{
	return (size == 0);
}

template<typename T>
void bclx::list_seq<T>::set(const gptr<T>& h, const uint64_t& s)
{
	head = h;
	size = s;
}

// Precondition: list_seq is not empty
template<typename T>
bclx::gptr<T> bclx::list_seq<T>::pop()
{
	--size;
	return head++;
}

// Precondition: list_seq is not empty
template<typename T>
bclx::gptr<T> bclx::list_seq<T>::pop(const uint64_t& s)
{
	if (size < s)
	{
		printf("[%lu]ERROR: The list contains enough elems\n", BCL::rank());
		return nullptr;
	}

	size -= s;
	gptr<T> ptr = head;
	head += s;
	return ptr;
}

/**/
