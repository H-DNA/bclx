#ifndef COMMON_H
#define COMMON_H

namespace dds
{

	/* Configuration */
	//#define	DEBUGGING
	//#define	MEM_REC
	const uint32_t	ELEM_PER_UNIT	=	10000;

	/* Aliases */
	template <typename T>
	using gptr = BCL::GlobalPtr<T>;

        /* Constants */
        const uint32_t	MASTER_UNIT     =       0;
	const bool	EMPTY		= 	false;
	const bool	NON_EMPTY	= 	true;

} /* namespace dds */

#endif /* COMMON_H */
