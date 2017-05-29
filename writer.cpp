/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 *
 *  Created on: May 12, 2017
 *      Author: pnorbert
 */

#include <iostream>
#include <vector>
#include <climits>
#include <sstream>

#include <unistd.h>
#include <mpi.h>
#include <adios.h>
#include <libgen.h>
#include <pmi.h>

#include "cmdline.h"

#define MAXTASKS 8192

using namespace std;

int main(int argc, char *argv[])
{
    setlinebuf(stdout);
    int rank = 0, nproc = 1;
    int namelength;
    char host[MPI_MAX_PROCESSOR_NAME];

    gengetopt_args_info args_info;
    if (cmdline_parser(argc, argv, &args_info) != 0)
        exit(1);

    if (args_info.inputs_num < 1)
    {
        cmdline_parser_print_help();
        exit(1);
    }

    string outputfile (args_info.inputs[0]);

    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &nproc);

    /* All tasks send their host name to task 0 */
    char *hostmap = (char *)malloc(nproc * MPI_MAX_PROCESSOR_NAME);
    MPI_Get_processor_name(host, &namelength);
    MPI_Gather(&host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hostmap,
               MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

    const unsigned long NX = args_info.len_arg;
    const int NSTEPS = args_info.nstep_arg;
    const unsigned long gnx = NX * nproc;
    const unsigned long offs = rank * NX;

#ifdef ON_CORI
    // Use PMI and do comm split along with tree structure
    int prank;
    int nid = -1;
    pmi_mesh_coord_t xyz;
    stringstream treelevel_ss;

    if (args_info.treelevel_arg > 0)
    {
        PMI_Get_rank(&prank);
        PMI_Get_nid(prank, &nid);
        PMI_Get_meshcoord((pmi_nid_t)nid, &xyz);
        printf(">>> PMI rank, nid, x, y, z = %d %d %u %u %u\n",
               prank, nid, xyz.mesh_x, xyz.mesh_y, xyz.mesh_z);

        MPI_Comm mesh_x_comm, mesh_y_comm, mesh_z_comm;
        int mesh_x_rank, mesh_y_rank, mesh_z_rank;

        MPI_Comm_split(MPI_COMM_WORLD, (int)xyz.mesh_x, rank, &mesh_x_comm);
        comm = mesh_x_comm;
        treelevel_ss << xyz.mesh_x;

        if (args_info.treelevel_arg > 1)
        {
            MPI_Comm_split(mesh_x_comm, (int)xyz.mesh_y, rank, &mesh_y_comm);
            comm = mesh_y_comm;
            treelevel_ss << "-" << xyz.mesh_y;
        }

        if (args_info.treelevel_arg > 2)
        {
            MPI_Comm_split(mesh_y_comm, (int)xyz.mesh_z, rank, &mesh_z_comm);
            comm = mesh_z_comm;
            treelevel_ss << "-" << xyz.mesh_z;
        }

        outputfile = outputfile + "." + treelevel_ss.str() + ".bp";
    }
#endif

    adios_init_noxml(MPI_COMM_WORLD);

    int64_t m_adios_group;
    adios_declare_group(&m_adios_group, "writer", "", adios_stat_no);
    adios_define_var(m_adios_group, "gnx", "", adios_unsigned_long, 0, 0, 0);
    adios_define_var(m_adios_group, "offs", "", adios_unsigned_long, 0, 0, 0);
    adios_define_var(m_adios_group, "nx", "", adios_unsigned_long, 0, 0, 0);
    adios_define_var(m_adios_group, "x", "", adios_integer, "nx", "gnx", "offs");
    adios_select_method(m_adios_group, args_info.writemethod_arg, args_info.wparams_arg, "");

    vector<int> x(NX);
    string mode = "w";

    if (rank == 0)
    {
        printf("====== Info =======\n");
        printf("%10s: %lu\n", "NX", NX);
        printf("%10s: %d\n", "Total NPs", nproc);
        printf("%10s: %.3f\n", "MBs/proc", (float)sizeof(int) * NX / 1024 / 1024);
        printf("%10s: %s\n", "Method", args_info.writemethod_arg);
        printf("%10s: %s\n", "Params", args_info.wparams_arg);
        for (int i = 0; i < nproc; i++)
            printf("%10s: %5d %s\n", "MAP", i, &hostmap[i * MPI_MAX_PROCESSOR_NAME]);
        printf("===================\n\n");
        printf(">>> %5s %5s %9s %12s %9s %12s %9s %12s\n",
               "rank", "step", "t3-t0", "(MB/s)", "t3-t1", "(MB/s)", "t3-t2", "(MB/s)");
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    for (int step = 0; step < NSTEPS; step++)
    {
        for (unsigned long i = 0; i < NX; i++)
        {
            x[i] = (step * NX * nproc + rank * NX + i) % INT_MAX;
        }

        int64_t f;
        double t[4];

        MPI_Barrier(MPI_COMM_WORLD);
        t[0] = MPI_Wtime();
        adios_open(&f, "writer", outputfile.c_str(), mode.c_str(), comm);
        t[1] = MPI_Wtime();
        adios_write(f, "gnx", &gnx);
        adios_write(f, "nx", &NX);
        adios_write(f, "offs", &offs);
        adios_write(f, "x", x.data());
        t[2] = MPI_Wtime();
        adios_close(f);
        t[3] = MPI_Wtime();

        double elap[3];
        elap[0] = t[3] - t[0];
        elap[1] = t[3] - t[1];
        elap[2] = t[3] - t[2];

        printf(">>> %5d %5d %9.03f %12.03f %9.03f %12.03f %9.03f %12.03f\n",
               rank, step,
               elap[0], (float)sizeof(int) * x.size() / elap[0] / 1024 / 1024,
               elap[1], (float)sizeof(int) * x.size() / elap[1] / 1024 / 1024,
               elap[2], (float)sizeof(int) * x.size() / elap[2] / 1024 / 1024);

        double melap[3];
        MPI_Reduce(elap, melap, 3, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0)
        {

            printf(">>> %5s %5d %9.03f %12.03f %9.03f %12.03f %9.03f %12.03f\n",
                   "ALL", step,
                   melap[0], (float)sizeof(int) * x.size() * nproc / melap[0] / 1024 / 1024,
                   melap[1], (float)sizeof(int) * x.size() * nproc / melap[1] / 1024 / 1024,
                   melap[2], (float)sizeof(int) * x.size() * nproc / melap[2] / 1024 / 1024);
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);

        if (args_info.append_flag)
            mode = "a";
        sleep(args_info.sleep_arg);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    adios_finalize(rank);
    MPI_Finalize();
    return 0;
}
