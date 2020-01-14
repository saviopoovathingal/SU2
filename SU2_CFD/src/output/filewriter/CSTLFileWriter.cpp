/*!
 * \file CSTLFileWriter.cpp
 * \brief STL Writer output class
 * \author T. Kattmann, T. Albring
 * \version 7.0.0 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2019, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../include/output/filewriter/CSTLFileWriter.hpp"
#include "../../../include/output/filewriter/CParallelDataSorter.hpp"
#include <iomanip> /*--- Used for ofstream.precision(int). ---*/


const string CSTLFileWriter::fileExt = ".stl";


CSTLFileWriter::CSTLFileWriter(vector<string> fields,
                               unsigned short nDim,
                               string fileName,
                               CParallelDataSorter *dataSorter) :
                CFileWriter(std::move(fields),
                            std::move(fileName),
                            dataSorter,
                            fileExt,
                            nDim){}


CSTLFileWriter::~CSTLFileWriter(){}


void CSTLFileWriter::ReprocessElementConnectivity(){

  /*--- Re-process the element connectivity information and store that appropriatly such that
    this information can be accessed in step 2. It is copied from CTecplotBinaryFileWriter.cpp +162 and mildly modified. ---*/

  unsigned long nParallel_Tria = dataSorter->GetnElem(TRIANGLE),
                nParallel_Quad = dataSorter->GetnElem(QUADRILATERAL);
  unsigned short iVar;

  num_nodes_to_receive.resize(size, 0);
  values_to_receive_displacements.resize(size);
  halo_nodes.clear();
  sorted_halo_nodes.clear();

  /* We output a single, partitioned zone where each rank outputs one partition. */
  vector<int> partition_owners;
  partition_owners.reserve(size);
  for (int iRank = 0; iRank < size; ++iRank)
    partition_owners.push_back(iRank);

  /* Gather a list of nodes we refer to but are not outputting, because they are not present on this rank. */
  for (unsigned long i = 0; i < nParallel_Tria * N_POINTS_TRIANGLE; ++i)
    if (dataSorter->FindProcessor(dataSorter->GetElem_Connectivity(TRIANGLE, 0, i)-1) != rank)
      halo_nodes.insert(dataSorter->GetElem_Connectivity(TRIANGLE, 0, i)-1);

  for (unsigned long i = 0; i < nParallel_Quad * N_POINTS_QUADRILATERAL; ++i)
    if (dataSorter->FindProcessor(dataSorter->GetElem_Connectivity(QUADRILATERAL, 0, i)-1) != rank)
      halo_nodes.insert(dataSorter->GetElem_Connectivity(QUADRILATERAL, 0, i)-1);


  /* Sorted list of halo nodes for this MPI rank. */
  sorted_halo_nodes.assign(halo_nodes.begin(), halo_nodes.end());

  /*--- We effectively tack the halo nodes onto the end of the node list for this partition.
    TecIO will later replace them with references to nodes in neighboring partitions. */
  num_halo_nodes = sorted_halo_nodes.size();
  vector<long> neighbor_partitions(max(static_cast<unsigned long>(1), num_halo_nodes));
  vector<long>      neighbor_nodes(max(static_cast<unsigned long>(1), num_halo_nodes));

  for(unsigned long i = 0; i < num_halo_nodes; ++i) {
    int owning_rank = dataSorter->FindProcessor(sorted_halo_nodes[i]);
    unsigned long node_number = sorted_halo_nodes[i] - dataSorter->GetNodeBegin(owning_rank);
    neighbor_partitions[i] = owning_rank; /* Partition numbers are 1-based, i.e. start with 1! */
    neighbor_nodes[i] = static_cast<long>(node_number);
  }

  /* Gather halo node data. First, tell each rank how many nodes' worth of data we need from them. */
  for (unsigned long i = 0; i < num_halo_nodes; ++i)
    ++num_nodes_to_receive[neighbor_partitions[i]];
  num_nodes_to_send.resize(size);
  SU2_MPI::Alltoall(&num_nodes_to_receive[0], 1, MPI_INT, &num_nodes_to_send[0], 1, MPI_INT, MPI_COMM_WORLD);

  /* Now send the global node numbers whose data we need,
     and receive the same from all other ranks.
     Each rank has globally consecutive node numbers,
     so we can just parcel out sorted_halo_nodes for send. */
  nodes_to_send_displacements.resize(size);
  nodes_to_receive_displacements.resize(size);
  nodes_to_send_displacements[0] = 0;
  nodes_to_receive_displacements[0] = 0;

  for(int iRank = 1; iRank < size; ++iRank) {
    nodes_to_send_displacements[iRank]    = nodes_to_send_displacements[iRank - 1]    + num_nodes_to_send[iRank - 1];
    nodes_to_receive_displacements[iRank] = nodes_to_receive_displacements[iRank - 1] + num_nodes_to_receive[iRank - 1];
  }

  int total_num_nodes_to_send = nodes_to_send_displacements[size - 1] + num_nodes_to_send[size - 1];

  nodes_to_send.resize(max(1, total_num_nodes_to_send));
  /* The terminology gets a bit confusing here. We're sending the node numbers
     (sorted_halo_nodes) whose data we need to receive, and receiving
     lists of nodes whose data we need to send. */
  if (sorted_halo_nodes.empty()) sorted_halo_nodes.resize(1); /* Avoid crash. */
  SU2_MPI::Alltoallv(&sorted_halo_nodes[0], &num_nodes_to_receive[0], &nodes_to_receive_displacements[0], MPI_UNSIGNED_LONG,
                     &nodes_to_send[0],     &num_nodes_to_send[0],    &nodes_to_send_displacements[0],    MPI_UNSIGNED_LONG,
                     MPI_COMM_WORLD);

  /* Now actually send and receive the data */
  data_to_send.resize(max(1, total_num_nodes_to_send * (int)fieldnames.size()));
  halo_var_data.resize(max(static_cast<unsigned long>(1), fieldnames.size() * num_halo_nodes));
  num_values_to_send.resize(size);
  values_to_send_displacements.resize(size);
  num_values_to_receive.resize(size);
  unsigned long index = 0;

  for(int iRank = 0; iRank < size; ++iRank) {

    /* We send and receive GlobalField_Counter values per node. */
    num_values_to_send[iRank]              = num_nodes_to_send[iRank] * fieldnames.size();
    values_to_send_displacements[iRank]    = nodes_to_send_displacements[iRank] * fieldnames.size();
    num_values_to_receive[iRank]           = num_nodes_to_receive[iRank] * fieldnames.size();
    values_to_receive_displacements[iRank] = nodes_to_receive_displacements[iRank] * fieldnames.size();

    for(iVar = 0; iVar < fieldnames.size(); ++iVar) {
      for(int iNode = 0; iNode < num_nodes_to_send[iRank]; ++iNode) {
        unsigned long node_offset = nodes_to_send[nodes_to_send_displacements[iRank] + iNode] - dataSorter->GetNodeBegin(rank);
        data_to_send[index++] = SU2_TYPE::GetValue(dataSorter->GetData(iVar,node_offset));
      }
    }

  }

  SU2_MPI::Alltoallv(&data_to_send[0],  &num_values_to_send[0],    &values_to_send_displacements[0],    MPI_DOUBLE,
                     &halo_var_data[0], &num_values_to_receive[0], &values_to_receive_displacements[0], MPI_DOUBLE,
                     MPI_COMM_WORLD);
}


void CSTLFileWriter::GatherCoordData(){
  /*--- 2. Load the coordinate data succesively in an array. 3coordinates*3nodes = 9 consecutive
    su2doubles form a triangle ---*/

  /*--- Routine to write the surface STL files (ASCII). We
   assume here that, as an ASCII file, it is safer to merge the
   surface data onto the master rank for writing for 2 reasons:
   (a) as a surface file, the amount of data should be much less
   than the volume solution, and (b) writing ASCII files in parallel
   requires serializing the IO calls with barriers, which ruins
   the performance at moderate to high rank counts. ---*/

  unsigned short iPoint,
                 iVar;

  unsigned long iProcessor,
                nProcessor = size,
                iElem,
                nLocalTria,
                nLocalQuad,
                nLocalTriaAll = 0,
                global_node_number;

  su2double *bufD_Send = NULL;

  vector<unsigned short> Nodelist = {0,1,3, 1,2,3}; // for Quad2Tri, assumes clockwise or counterclockwise rotation

  /*--- Find the max number of surface vertices among all
   partitions so we can set up buffers. The master node will handle
   the writing of the STL file after gathering all of the data. ---*/

  nLocalTria = dataSorter->GetnElem(TRIANGLE);
  nLocalQuad = dataSorter->GetnElem(QUADRILATERAL);
  nLocalTriaAll = nLocalTria + nLocalQuad*2; // Quad splitted into 2 tris

  if (rank == MASTER_NODE) Buffer_Recv_nTriaAll = new unsigned long[nProcessor];

  /*--- Communicate the maximum of local triangles on any process to each partition and the number of local vertices on each partition
   to the master node with collective calls. ---*/

  SU2_MPI::Allreduce(&nLocalTriaAll, &MaxLocalTriaAll, 1,
                     MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);

  SU2_MPI::Gather(&nLocalTriaAll,       1, MPI_UNSIGNED_LONG,
                  Buffer_Recv_nTriaAll, 1, MPI_UNSIGNED_LONG,
                  MASTER_NODE, MPI_COMM_WORLD);

  /*--- Allocate buffer for send/recv of the coordinate data. Only the master rank allocates buffers for the recv. ---*/
  bufD_Send = new su2double[MaxLocalTriaAll*3*3](); /* Triangle has 3 Points with 3 coords each */
  if (rank == MASTER_NODE)
    bufD_Recv = new su2double[nProcessor*MaxLocalTriaAll*3*3];

  /*--- Load send buffers with the local data on this rank. ---*/
  /*--- Write triangle element coordinate data into send buffer---*/
  unsigned long index = 0;
  for (iElem = 0; iElem < nLocalTria; iElem++) {
    for (iPoint = 0; iPoint < 3; iPoint++) {

      global_node_number = dataSorter->GetElem_Connectivity(TRIANGLE, iElem, iPoint) - 1; // (var, GlobalPointindex)
      unsigned long local_node_number = global_node_number - dataSorter->GetNodeBegin(rank);

      for (iVar = 0; iVar < 3; iVar++){
        if (dataSorter->FindProcessor(global_node_number) == rank) {
          bufD_Send[index] = dataSorter->GetData(iVar, local_node_number);
        } else {
          bufD_Send[index] = GetHaloNodeValue(global_node_number, iVar);
        }

        index++;
      }//iVar
    }//iPoint
  }//iElem

  /*--Write quadrilateral element coordinate data into send buffer:
    Each quad is split into two triangles after a fixed node order. ---*/
  for (iElem = 0; iElem < nLocalQuad; iElem++) {
    for (iPoint = 0; iPoint < Nodelist.size(); iPoint++) {
      global_node_number = dataSorter->GetElem_Connectivity(QUADRILATERAL,iElem,Nodelist[iPoint]) - 1; // (var, GlobalPointindex)
      unsigned long local_node_number = global_node_number - dataSorter->GetNodeBegin(rank);

      for (iVar = 0; iVar < 3; iVar++){
        if (dataSorter->FindProcessor(global_node_number) == rank) {
          bufD_Send[index] = dataSorter->GetData(iVar, local_node_number);
        } else {
          bufD_Send[index] = GetHaloNodeValue(global_node_number, iVar);
        }

        index++;
      }//iVar
    }//iPoint
  }//iElem

  /*--- Collective comms of the solution data and global IDs. ---*/
  SU2_MPI::Gather(bufD_Send, static_cast<int>(MaxLocalTriaAll*3*3), MPI_DOUBLE,
                  bufD_Recv, static_cast<int>(MaxLocalTriaAll*3*3), MPI_DOUBLE,
                  MASTER_NODE, MPI_COMM_WORLD);

  /*--- Free temporary memory. ---*/
  if(bufD_Send != NULL) delete [] bufD_Send;
}


void CSTLFileWriter::Write_Data(){

  /*--- This Write_Data routine has 3 major parts where the first two are transfered in external functions:
    1. Prerequisite info: The parallel data-sorter distributes nodes of the primal mesh onto the processes
    (i.e. this step is only important for parallel excecution) and not elements. For the STL file the connectivity
    of the elements is broken/lost especially on the node-borders between processes. This information is
    re-processed in this first (cumbersome) step.
    2. The coordinate data for each node in a Triangle is successively written into a local array.
    Quadrilateral surface elements are split into 2 triangles. All local arrays are gathered on the MASTER_NODE.
    3. The MASTER_NODE only writes the data into a .stl file.
  ---*/

  /*--- 1. Re-process the element connectivity information and store that appropriatly such that
    this information can be accessed in step 2. It is copied from CTecplotBinaryFileWriter.cpp:WriteData() l.162 and mildly modified. ---*/
  ReprocessElementConnectivity();

  /*--- 2. Load the coordinate data succesively in an array. 3coordinates*3nodes = 9 consecutive
    su2doubles form a triangle ---*/
  GatherCoordData();

  /*--- 3. The master rank alone writes the surface STL file. ---*/
  unsigned short iPoint,
                 iVar;

  unsigned long iProcessor,
                nProcessor = size,
                iElem,
                index;

  ofstream Surf_file;

  if (rank == MASTER_NODE) {

    /*--- For information how an ASCII .stl file is structured: https://en.wikipedia.org/wiki/STL_(file_format)  ---*/
    /*--- Open the STL file and write the header line. ---*/
    Surf_file.precision(6);
    Surf_file.open(fileName.c_str(), ios::out);
    Surf_file << "solid SU2_output" << endl;

    /*--- Loop through all of the collected data and write each node's coordinate values. ---*/
    for (iProcessor = 0; iProcessor < nProcessor; iProcessor++) {
      for (iElem = 0; iElem < Buffer_Recv_nTriaAll[iProcessor]; iElem++) { // loops over nLocalTriaAll

        /*--- Write the coordinate data for each node in a triangle. ---*/
        /*--- Every tested software recomputes the normal anyway,
              such that this arbitray face normal does not matter.  ---*/
        Surf_file << "facet normal " << 1 << " " << 2 << " " << 3 << endl;
        Surf_file << "    outer loop" << endl; // 4 leading whitespaces

        for(iPoint = 0; iPoint < 3; iPoint++) {
          Surf_file << "        vertex"; // 8 leading whitespaces
          index = iProcessor*MaxLocalTriaAll*3*3 + iElem*3*3 + iPoint*3;

          for (iVar = 0; iVar < 3; iVar++) {
            Surf_file << " " <<  bufD_Recv[index + iVar];
          }
          Surf_file << endl;
        }//iPoint
        Surf_file << "    endloop" << endl; // 4 leading whitespaces
        Surf_file << "endfacet" << endl;
      }//iElem
    }//iProcessor

    /*--- Close the file. ---*/
    Surf_file << "endsolid SU2_output" << endl;
    Surf_file.close();

  }

  /*--- Free temporary memory. ---*/
  if(bufD_Recv != NULL) delete [] bufD_Recv;
  if(Buffer_Recv_nTriaAll != NULL) delete [] Buffer_Recv_nTriaAll;
}


double CSTLFileWriter::GetHaloNodeValue(unsigned long global_node_number, unsigned short iVar) {

  vector<unsigned long>::iterator it = lower_bound(sorted_halo_nodes.begin(), sorted_halo_nodes.end(), global_node_number);
  int offset = distance(sorted_halo_nodes.begin(), it);
  int id = 0;

  for (int iRank = 0; iRank < size; ++iRank) {
    for (int i = 0; i < num_nodes_to_receive[iRank]; i++){
      int displ = values_to_receive_displacements[iRank] + num_nodes_to_receive[iRank]*iVar;
      if (id == offset)
        return halo_var_data[displ+i];
      id++;
    }
  }

  SU2_MPI::Error("STL File-Writer: Halo node not found.", CURRENT_FUNCTION);
  return 0.0;
}