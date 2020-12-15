#ifndef LOCK_INF_H
#define	LOCK_INF_H

namespace dds /* Distributed Data Structures */
{

class lock /* Lock Interface */
{
public:
	virtual void acquire() = 0;
	virtual void release() = 0;
	//virtual ~lock();

}; /* class lock */

} /* namespace dds */

#endif /* LOCK_INF_H */
