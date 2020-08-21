#ifndef TA_H
#define TA_H

namespace ta
{

	class na
	{
	public:
		int 	size;
		int	*table;

		na();
		~na();
		void print();
	};

	class sa
	{
		//TODO
	};

} /* namespace ta */

ta::na::na()
{
	MPI_Comm	nodeComm;
	MPI_Group	group,
			nodeGroup;
	int		*temp;

	MPI_Comm_split_type(BCL::comm, MPI_COMM_TYPE_SHARED, BCL::rank(), BCL::info, &nodeComm);
	MPI_Comm_size(nodeComm, &size);
	table = new int [size];
	temp = new int [size];
	for (int i = 0; i < size; ++i)
                temp[i] = i;
	MPI_Comm_group(BCL::comm, &group);
	MPI_Comm_group(nodeComm, &nodeGroup);
        MPI_Group_translate_ranks(nodeGroup, size, temp, group, table);
	delete[] temp;
}

ta::na::~na()
{
	delete[] table;
}

void ta::na::print()
{
	for (int i = 0; i < size; ++i)
		printf("[%lu]table[%d] = %d\n", BCL::rank(), i, table[i]);
}

#endif /* TA_H */
