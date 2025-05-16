#pragma once

#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>
#include <cstdint> // int64_t...
#include <mpi.h>
#include <vector> // std::vector...

namespace bclx {

class rll_t {
public:
  rll_t();
  rll_t(const std::vector<int64_t> &disp);
  ~rll_t();
  MPI_Datatype get_elem_type() const;
  uint64_t get_elem_count() const;

private:
  MPI_Datatype MPI_REMOTE_LINKED_LIST;
  uint64_t elem_count;
}; /* class rll_t */

} /* namespace bclx */

bclx::rll_t::rll_t() {}

bclx::rll_t::rll_t(const std::vector<int64_t> &disp) : elem_count{disp.size()} {
  MPI_Type_create_hindexed_block(disp.size(), 1, (MPI_Aint *)disp.data(),
                                 MPI_UINT64_T, &MPI_REMOTE_LINKED_LIST);
  MPI_Type_commit(&MPI_REMOTE_LINKED_LIST);
}

bclx::rll_t::~rll_t() { MPI_Type_free(&MPI_REMOTE_LINKED_LIST); }

MPI_Datatype bclx::rll_t::get_elem_type() const {
  return MPI_REMOTE_LINKED_LIST;
}

uint64_t bclx::rll_t::get_elem_count() const { return elem_count; }
