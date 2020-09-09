#ifndef TA_H
#define TA_H

namespace ta
{

	class na
	{
	public:
		int 		rank;
		int 		size;
		int		*table;
		int		node_id;
		int		node_id_master;
		int		node_num;
		MPI_Comm	nodeComm;

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
	MPI_Comm	leaderComm;
	MPI_Group	group,
			nodeGroup;
	int		*temp,
			color;

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

        MPI_Comm_rank(nodeComm, &rank);
	if (rank == 0)
		color = 0;
	else //if (rank == 0)
		color = MPI_UNDEFINED;
	MPI_Comm_split(BCL::comm, color, BCL::rank(), &leaderComm);
	if (color == 0)
	{
		MPI_Comm_rank(leaderComm, &node_id);
		MPI_Comm_size(leaderComm, &node_num);
	}
	MPI_Bcast(&node_id, 1, MPI_INT, 0, nodeComm);
	MPI_Bcast(&node_num, 1, MPI_INT, 0, nodeComm);
	if (BCL::rank() == 0)
		node_id_master = node_id;
	MPI_Bcast(&node_id_master, 1, MPI_INT, 0, BCL::comm);
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
