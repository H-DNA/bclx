#ifndef CONFIG_H
#define CONFIG_H

#include <cmath>

namespace dds
{

	/* Configurations */
	//#define	DEBUGGING
	#define	MEM_REC
	const uint32_t	ELEM_PER_UNIT	=	exp2(14);
	const uint32_t	WORKLOAD	=	1;		//ms
	const uint32_t  MASTER_UNIT     =       0;

	/* Aliases */
	template <typename T>
	using gptr = BCL::GlobalPtr<T>;

        /* Constants */
	const bool	EMPTY		= 	false;
	const bool	NON_EMPTY	= 	true;

	/* Varriables */
	uint64_t	failure		=	0;

} /* namespace dds */

#endif /* CONFIG_H */
