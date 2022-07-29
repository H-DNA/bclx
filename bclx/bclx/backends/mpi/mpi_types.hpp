#pragma once

namespace bclx
{

MPI_Datatype	MPI_REMOTE_LINKED_LIST;

struct rll_t
{
	rll_t();
	rll_t(const std::vector<int64_t>& disp);
	~rll_t();
	MPI_Datatype get() const;
};

} /* namespace bclx */

bclx::rll_t::rll_t() {}

bclx::rll_t::rll_t(const std::vector<int64_t>& disp)
{
	MPI_Type_create_hindexed_block(disp.size(), 1, (MPI_Aint*)disp.data(),
					MPI_UINT64_T, &MPI_REMOTE_LINKED_LIST);
	MPI_Type_commit(&MPI_REMOTE_LINKED_LIST);
}

bclx::rll_t::~rll_t()
{
	MPI_Type_free(&MPI_REMOTE_LINKED_LIST);
}

MPI_Datatype bclx::rll_t::get() const
{
	return MPI_REMOTE_LINKED_LIST;
}

