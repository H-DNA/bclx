#pragma once

#include <bcl/bcl.hpp>

//#include <bclx/core/except.hpp>
#include <bclx/core/gptr.hpp>
//#include <bclx/core/malloc.hpp>
//#include <bclx/core/alloc.hpp>

#ifdef SHMEM
  #include <bclx/backends/shmem/backend.hpp>
#elif GASNET_EX
  #include <bclx/backends/gasnet-ex/backend.hpp>
#elif UPCXX
  #include <bclx/backends/upcxx/backend.hpp>
#else
  #include <bclx/backends/mpi/backend.hpp>
#endif

#include <bclx/core/comm.hpp>
#include <bclx/core/util/timer.hpp>

