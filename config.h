#ifndef CONFIG_H
#define CONFIG_H

#include <cmath>

namespace dds
{

	/* Configurations */
	#define	TRACING
	//#define	DEBUGGING
	//#define	MEM_REC
	const uint64_t	ELEM_PER_UNIT	=	exp2l(15);
	const uint32_t	WORKLOAD	=	1;		//us
	const uint64_t	BK_INIT		=	exp2l(0);	//us
	const uint64_t	BK_MAX		=	exp2l(10);	//us
	const uint32_t	TSS_INTERVAL	=	1;		//us
	const uint32_t  MASTER_UNIT     =       0;

	/* Aliases */
	template <typename T>
	using gptr = BCL::GlobalPtr<T>;

        /* Constants */
	const bool	EMPTY		= 	false;
	const bool	NON_EMPTY	= 	true;

	/* Varriables */
	
	//tracing
	#ifdef  TRACING
        	uint64_t        succ_cs = 0;
		uint64_t	succ_ea = 0;
		uint64_t	fail_cs = 0;
		uint64_t	fail_ea = 0;
	#endif

} /* namespace dds */

#endif /* CONFIG_H */
