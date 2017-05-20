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
#include <unistd.h>
#include <mpi.h>
#include <adios.h>

#define MAXTASKS 8192
#define MAXNSTEP 1

int main(int argc, char *argv[])
{
    setlinebuf(stdout);
    int rank = 0, nproc = 1;
    int namelength;
    char host[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &nproc);

    /* All tasks send their host name to task 0 */
    char *hostmap = (char*) malloc(nproc * MPI_MAX_PROCESSOR_NAME);
    MPI_Get_processor_name(host, &namelength);
    MPI_Gather(&host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hostmap,
             MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (argc < 2)
    {
        std::cout << "Not enough arguments: need an input file" << std::endl;
        std::cout << "Usage: writer filename [NX]" << std::endl;
        return 1;
    }
    const char *outputfile = argv[1];
    const unsigned long NX = argc > 2 ? atol(argv[2]) : 10;

    const int NSTEPS = MAXNSTEP;
    const unsigned long gnx = NX * nproc;
    const unsigned long offs = rank * NX;

    adios_init("writer.xml", comm);

    std::vector<int> x(NX);
    std::string mode = "w";

    if (rank == 0)
    {
        printf("====== Info =======\n");
        printf("%10s: %lu\n", "NX", NX);
        printf("%10s: %d\n", "Total NPs", nproc);
        printf("%10s: %.3f\n", "MBs/proc", (float) sizeof(int)*NX/1024/1024);
        for (int i=0; i<nproc; i++)
            printf("%10s: %5d %s\n", "MAP", i, &hostmap[i*MPI_MAX_PROCESSOR_NAME]);
        printf("===================\n\n");
        printf(">>> %5s %5s %9s %9s %9s %9s %9s %9s\n",
               "rank", "step", "t3-t0", "(MB/s)", "t3-t1", "(MB/s)", "t3-t2", "(MB/s)");
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    for (int step = 0; step < NSTEPS; step++)
    {
        for (int i = 0; i < NX; i++)
        {
            x[i] = step * NX * nproc * 1.0 + rank * NX * 1.0 + i;
        }

        int64_t f;
        double t[4];

        MPI_Barrier(comm);
        t[0] = MPI_Wtime();
        adios_open(&f, "writer", outputfile, mode.c_str(), comm);
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

        printf(">>> %5d %5d %9.03f %9.03e %9.03f %9.03e %9.03f %9.03e\n",
               rank, step,
               elap[0], (float)sizeof(int) * x.size() / elap[0] / 1024 / 1024,
               elap[1], (float)sizeof(int) * x.size() / elap[1] / 1024 / 1024,
               elap[2], (float)sizeof(int) * x.size() / elap[2] / 1024 / 1024);

        double melap[3];
        MPI_Reduce(elap, melap, 3, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0)
        {

            printf(">>> %5s %5d %9.03f %9.03e %9.03f %9.03e %9.03f %9.03e\n",
                   "ALL", step,
                   melap[0], (float)sizeof(int) * x.size() * nproc / melap[0] / 1024 / 1024,
                   melap[1], (float)sizeof(int) * x.size() * nproc / melap[1] / 1024 / 1024,
                   melap[2], (float)sizeof(int) * x.size() * nproc / melap[2] / 1024 / 1024);
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);

        mode = "a";
        sleep(3);
    }

    MPI_Barrier(comm);
    adios_finalize(rank);
    MPI_Finalize();
    return 0;
}
