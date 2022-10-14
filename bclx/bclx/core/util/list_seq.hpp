#pragma once

namespace bclx
{

namespace sds
{

template<typename T>
class list
{
public:
	list();
	~list();
	bool empty() const;
	void set(const gptr<T>& h, const uint64_t& s);
	gptr<T> pop();
	gptr<T> pop(const uint64_t& s);

private:
	gptr<T>		head;
	uint64_t	size;
};

template<typename T>
struct node
{
	node*	next;
	T	val;
};

template<typename T>
class stack
{
public:
	stack();
	~stack();
	bool empty() const;
	void push(const T& val);
	T pop();
	
private:
	node<T>* top;
};

} /* namespace sds */

} /* namespace bclx */

/* Implementation of list */

template<typename T>
bclx::sds::list<T>::list() : head{nullptr}, size{0} {}

template<typename T>
bclx::sds::list<T>::~list() {}

template<typename T>
bool bclx::sds::list<T>::empty() const
{
	return (size == 0);
}

template<typename T>
void bclx::sds::list<T>::set(const gptr<T>& h, const uint64_t& s)
{
	head = h;
	size = s;
}

// Precondition: list is not currently empty
template<typename T>
bclx::gptr<T> bclx::sds::list<T>::pop()
{
	--size;
	return head++;
}

// Precondition: list is not currently empty
template<typename T>
bclx::gptr<T> bclx::sds::list<T>::pop(const uint64_t& s)
{
	if (size < s)
	{
		printf("[%lu]ERROR: The list does not contain enough elems\n", BCL::rank());
		return nullptr;
	}

	size -= s;
	gptr<T> ptr = head;
	head += s;
	return ptr;
}

/**/

/* Implementation of stack */

template<typename T>
bclx::sds::stack<T>::stack() : top{nullptr} {}

template<typename T>
bclx::sds::stack<T>::~stack() {}

template<typename T>
bool bclx::sds::stack<T>::empty() const
{
	return (top == nullptr);
}

template<typename T>
void bclx::sds::stack<T>::push(const T& val)
{
	node<T>* tmp = new node<T>;

	if (tmp == nullptr)
	{
		printf("[%lu]ERROR: The stack is full\n", BCL::rank());
		return;
	}

	*tmp = {top, val};
	top = tmp;
}

// Precondition: stack is not currently empty
template<typename T>
T bclx::sds::stack<T>::pop()
{
	node<T>* tmp = top;
	T val = tmp->val;
	top = top->next;
	delete tmp;
	return val;
}

/**/
