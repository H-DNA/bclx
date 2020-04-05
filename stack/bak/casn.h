#ifndef CASN_H
#define CASN_H

namespace dds
{

namespace casn
{

	/* Constants */
	enum stat {
		UNDECIDED	= 0,
		FAILED		= 1,
		SUCCEEDED	= 2
	};

	/* Data types */
	template <typename T>
	class elem
	{
	public:
		T		*a1;
		T		o1;
		T		n1;
	};

	template <typename T>
	class RDCSS_descriptor
	{
	public:
		T 		*a1;	//control address
		T 		*a2;	//data address
		T 		o1;	//expected value
		T		o2;	//old value
		T		n2;	//new value
	};

	template <typename T>
	class CASN_descriptor
	{
	public:
		stat 		status;
		int		n;
		elem<T>		*entry;

		CASN_descriptor(int);
		~CASN_descriptor();
	}

	//Functions
	template <typename T>
	bool is_RDCSS_descriptor(void *);

	template <typename T>
	T RDCSS(RDCSS_descriptor<T> *);

	template <typename T>
	T RDCSS_read(T *);

	template <typename T>
	void complete(RDCSS_descriptor<T> *);

	template <typename T>
	is_CASN_descriptor(void *);

	template <typename T>
	bool CASN(CASN_descriptor<T> *);

	template <typename T>
	T CASN_read(T *);

} /*namespace cas2 */

} /* namespace dds */

template <typename T>
bool dds::casn::is_RDCSS_descriptor(void *d)
{
	//TODO
}

template <typename T>
T dds::casn::RDCSS(RDCSS_descriptor<T> *d)
{
	do {
		BCL::compare_and_swap(d->a2, d->o1, d, r); 	/* C1 */
		if (is_descriptor(r))
			complete(r);				/* H1 */
	} while (is_descriptor(r));				/* B1 */
	if (r == d->o2)
		complete(d);
	return r;
}

template <typename T>
T dds::casn::RDCSS_read(T *addr)
{
	do {
		r = *addr;					/* R1 */
		if (is_descriptor(r))
			complete(r);				/* H2 */
	} while (is_descriptor(r))				/* B2 */
	return r;
}

template <typename T>
void dds::casn::complete(RDCSS_descriptor<T> *d)
{
	v = *(d->a1);						/* R2 */
	if (v == d->o1)
		BCL::compare_and_swap(d->a2, d, d->n2, result);	/* C2 */
	else
		BCL::compare_and_swap(d->a2, d, d->o2, result);	/* C3 */
}
/**/

dds::casn::CASN_descriptor<T>::CASN_descriptor(int size = 2);
{
	//TODO
	entry = new elem<T>[size];
}

dds::casn::CASN_descriptor<T>::~CASN_descriptor()
{
	//TODO
	delete[] entry;
}

bool CASN(CASN_descriptor *d)
{
	int i;

	if (c->status == UNDECIDED)
	{
		/* R4 */
		//phase 1:
		status = SUCCEEDED;
		for (i = 0; i < d->n && status == SUCCEEDED; ++i)
		{
			/* L1 */
			//retry entry
			entry = d->entry[i];
			val = RDCSS(new RDCSS_descriptor(&(d->status), UNDECIDED, entry->addr, entry->old, cd)); /* X1 */
			if (is_CASN_descriptor(val))
			{
				if (val != d)
				{
					CASN(val);	/* H3 */
					goto retry entry;
				}
				else if (val != entry->old)
					status = FAILED;
			}
		}
		CAS1(&(d->status), UNDECIDED, status);		/* C4 */
	}

	//phase 2:
	succeeded = (d->status == SUCCEEDED);
	for (i = 0; i < d->n; ++i)
		CAS1(d->entry[i].addr, d, succeeded ? d->entry[i].new :: d->entry[i].old);	/* C5 */
	return succeeded;
}

T CASN_read(T *addr)
{
	do {
		r = RDCSS_read(addr);		/* R5 */
		if (is_CASN_descriptor(r))
			CASN(r);		/* H4 */
	} while (is_CASN_descriptor(r));	/* B3 */
	return r;
}
/**/

#endif /* CASN_H */
