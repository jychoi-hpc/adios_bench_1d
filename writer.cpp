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
#include <mpi.h>
#include <adios.h>

int main(int argc, char *argv[])
{
    int rank = 0, nproc = 1;
    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &nproc);

    if (argc < 2)
    {
        std::cout << "Not enough arguments: need an input file" << std::endl;
        std::cout << "Usage: writer filename [NX]" << std::endl;
        return 1;
    }
    const char *outputfile = argv[1];
    const unsigned long NX = argc>2? atol(argv[2]) : 10;
    std::cout << "NX: " << NX << " (" << (float)NX*4/1024/1024 << " MB)" << std::endl;

    const int NSTEPS = 5;
    const unsigned long gnx = NX*nproc;
    const unsigned long offs = rank*NX;

    adios_init("writer.xml", comm);

    std::vector<int> x(NX);
    std::string mode = "w";

    for (int step = 0; step < NSTEPS; step++)
    {
        std::cout << "Step:" << step << std::endl;
        for (int i = 0; i < NX; i++)
        {
            x[i] = step * NX * nproc * 1.0 + rank * NX * 1.0 + i;
        }

        std::cout << "Writing ... " << std::endl;
        int64_t f;
        adios_open(&f, "writer", outputfile, mode.c_str(), comm);
        adios_write(f, "gnx", &gnx);
        adios_write(f, "nx", &NX);
        adios_write(f, "offs", &offs);
        adios_write(f, "x", x.data());
        adios_close(f);

        mode = "a";
    }

    MPI_Barrier(comm);
    adios_finalize(rank);
    MPI_Finalize();
    return 0;
}
