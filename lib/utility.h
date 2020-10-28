#ifndef UTILITY_H
#define UTILITY_H

namespace sds
{

	/* Constants */
        const bool      EMPTY		=       false;
        const bool      NON_EMPTY       =       true;

	/* Data types */
	template <typename T>
	struct elem
	{
		T		value;
		elem<T>		*next;
	};

	template <typename T>
	class list
	{
	public:
		list();
		~list();
		elem<T>	*front();
		elem<T> *rear();
                uint64_t size();
		void reset();
		void assign(list<T> &);
		void insert(const T &);
		void insert(elem<T> *);
		bool remove(T &);
		bool remove(elem<T> *&);
		void remove(elem<T> *, elem<T> *);
		void print();

	private:
		elem<T>		*head;
		elem<T>		*tail;
		uint64_t	len;
	};

	template <typename T>
	class stack
	{
	public:
		stack();
		~stack();
		void push(const T &);
		bool pop(T *);

		//debugging
		//void print();

	private:
		elem<T>		*top;
	};

	/* Functions */
	template <typename T>
	inline void swap(T &, T &);

	template <typename T>
	inline void heap_sort(T *, const uint64_t &);

	template <typename T>
	inline void remove_duplicates(T *, uint64_t &);

	template <typename T>
	inline bool binary_search(const T &, T *, const uint64_t &);

} /* namespace sds */

template <typename T>
sds::list<T>::list()
{
	head = tail = nullptr;
	len = 0;
}

template <typename T>
sds::list<T>::~list()
{
	while (head != nullptr)
	{
		tail = head;
		head = head->next;
		delete tail;
	}
	tail = nullptr;
}

template <typename T>
sds::elem<T> *sds::list<T>::front()
{
	return head;
}

template <typename T>
sds::elem<T> *sds::list<T>::rear()
{
	return tail;
}

template <typename T>
uint64_t sds::list<T>::size()
{
        return len;
}

template <typename T>
void sds::list<T>::reset()
{
	head = tail = nullptr;
	len = 0;
}

template <typename T>
void sds::list<T>::assign(sds::list<T> &aList)
{
	head = aList.front();
	tail = aList.rear();
	len = aList.size();
	aList.reset();
}

template <typename T>
void sds::list<T>::insert(const T &value)
{
	elem<T> *temp = new elem<T>;

	if (temp == nullptr)
	{
		printf("ERROR: Local memory runs out!\n");
		return;
	}

	temp->value = value;
	temp->next = nullptr;
	if (tail == nullptr)
	{
		tail = temp;
		head = tail;
	}
	else //if (tail != nullptr)
	{
		tail->next = temp;
		tail = temp;
	}

	++len;
}

template <typename T>
void sds::list<T>::insert(elem<T> *temp)
{
	if (temp == nullptr)
		return;

	temp->next = nullptr;
	if (tail == nullptr)
	{
		tail = temp;
		head = tail;
	}
	else //if (tail != nullptr)
	{
		tail->next = temp;
		tail = temp;
	}

	++len;
}

template <typename T>
bool sds::list<T>::remove(T &value)
{
	if (head == nullptr)
		return EMPTY;

	elem<T> *temp = head;
	value = temp->value;
	head = head->next;
	if (head == nullptr)
		tail = nullptr;
	delete temp;

	--len;
	return NON_EMPTY;
}

template <typename T>
bool sds::list<T>::remove(elem<T> *&temp)
{
        if (head == nullptr)
        {
                temp = nullptr;
                return EMPTY;
        }

	temp = head;
	head = head->next;
	if (head == nullptr)
		tail = nullptr;

	--len;
	return NON_EMPTY;
}

template <typename T>
void sds::list<T>::remove(elem<T> *prev, elem<T> *curr)
{
	if (curr == nullptr)
		return;

	if (prev == nullptr)
		head = curr->next;
	else //if (prev != nullptr)
		prev->next = curr->next;

	if (curr->next == nullptr)
		tail = nullptr;

	--len;
}

template <typename T>
void sds::list<T>::print()
{
	for (elem<T> *temp = head; temp != NULL; temp = temp->next)
		temp->value.print();
}
/**/

template <typename T>
sds::stack<T>::stack()
{
	top = nullptr;
}

template <typename T>
sds::stack<T>::~stack()
{
	elem<T> *temp;
	while (top != nullptr)
	{
		temp = top;
		top = top->next;
		delete temp;
	}
}

template <typename T>
void sds::stack<T>::push(const T &value)
{
	elem<T> *temp = new elem<T>;
        if (temp == nullptr)
        {
                printf("ERROR: Local memory runs out!\n");
                return;
        }
	temp->value = value;
	temp->next = top;
	top = temp;
}

template <typename T>
bool sds::stack<T>::pop(T *value)
{
        if (top == nullptr)
        {
                value = nullptr;
                return EMPTY;
        }
        else //if (head != nullptr)
        {
		elem<T> *temp = top;
		top = top->next;
		*value = temp->value;
		free(temp);
		return NON_EMPTY;
	}
}

/*template <typename T>
void sds::stack<T>::print()
{
	for (elem<T> *temp = top; temp != NULL; temp = temp->next)
		temp.value.print();
}
/**/

template <typename T>
inline void sds::swap(T &a, T &b)
{
        T temp = a;
        a = b;
        b = temp;
}
/**/

template <typename T>
inline void sds::heap_sort(T *array, const uint64_t &size)
{
	int64_t 	index,
			i,
			j;

	//Build the max heap
	for (i = 1; i < size; ++i)
		if (array[i] > array[(i - 1) / 2])
		{
			j = i;
			while (array[j] > array[(j - i) / 2])
			{
				sds::swap(array[j], array[(j - 1) / 2]);
				j = (j - 1) / 2;
			}
		}

	//Sort the max heap
	for (i = size - 1; i > 0; --i)
	{
		sds::swap(array[0], array[i]);
		j = 0;
		do {
			index = (2 * j + 1);
			if (array[index] < array[index + 1] && index < (i - 1))
				++index;
			if (array[j] < array[index] && index < i)
				sds::swap(array[j], array[index]);
			j = index;
		} while (index < i);
	}
}
/**/

template <typename T>
inline void sds::remove_duplicates(T *array, uint64_t &size)
{
	if (size <= 0)
		return;

	uint64_t	temp = 1;

	for (uint64_t i = 0; i < size - 1; ++i)
		if (array[i] != array[i + 1])
			array[temp++] = array[i + 1];
	size = temp;
}
/**/

template <typename T>
inline bool sds::binary_search(const T &addr, T *array, const uint64_t &size)
{
	int64_t 	bot = 0,
			top = size - 1,
			mid;

	while (bot <= top)
	{
		mid = (bot + top) / 2;
		if (addr == array[mid])
			return true;
		else if (addr < array[mid])
			top = mid - 1;
		else //if (addr > array[mid])
			bot = mid + 1;
	}

	return false;
}
/**/

#endif /* UTILITY_H */
