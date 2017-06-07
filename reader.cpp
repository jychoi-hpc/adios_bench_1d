#include <cstdint>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
//#include <math.h>
//#include <memory>
//#include <stdexcept>
#include <string>
#include <vector>
#include <mpi.h>
#include <adios_read.h>
#include <climits>
#include "reader_cmdline.h"

using namespace std;

void printData(std::vector<int> x, int steps, uint64_t nelems,
        uint64_t offset, int rank);
void summarizeData(std::vector<int> x, unsigned long gnx,  int steps, uint64_t nelems,
        uint64_t offset, int rank);

int main(int argc, char *argv[])
{
    setlinebuf(stdout);    
    int rank = 0, nproc = 1;
    int namelength;
    char host[MPI_MAX_PROCESSOR_NAME];

    gengetopt_args_info args_info;
    if (cmdline_parser (argc, argv, &args_info) != 0)
        exit(1);

    if (args_info.inputs_num < 1)
    {
        cmdline_parser_print_help();
        exit(1);
    }
    const char *inputfile = args_info.inputs[0];

    enum ADIOS_READ_METHOD adios_read_method = ADIOS_READ_METHOD_BP;
    if (args_info.readmethod_given)
    {
        if (string(args_info.readmethod_arg) == "BP") {
            adios_read_method = ADIOS_READ_METHOD_BP;
        } else if (string(args_info.readmethod_arg) == "ICEE") {
            adios_read_method = ADIOS_READ_METHOD_ICEE;
        } else if (string(args_info.readmethod_arg) == "DIMES") {
            adios_read_method = ADIOS_READ_METHOD_DIMES;
        } else if (string(args_info.readmethod_arg) == "DATASPACES") {
            adios_read_method = ADIOS_READ_METHOD_DATASPACES;
        } else if (string(args_info.readmethod_arg) == "FLEXPATH") {
            adios_read_method = ADIOS_READ_METHOD_FLEXPATH;
        } else {
            fprintf(stderr, "Unknown read method: %s\n", args_info.readmethod_arg);
        }
    }

    float timeout_sec = args_info.timeout_arg;

    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &nproc);

    /* All tasks send their host name to task 0 */
    char *hostmap = (char*) malloc(nproc * MPI_MAX_PROCESSOR_NAME);
    MPI_Get_processor_name(host, &namelength);
    MPI_Gather(&host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hostmap,
             MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

    adios_read_init_method(adios_read_method, comm, args_info.rparams_arg);

    ADIOS_FILE *f;
    double t[5];

    MPI_Barrier(comm);
    t[0] = MPI_Wtime();

    if (args_info.stream_flag)
        f = adios_read_open(inputfile, adios_read_method, comm,
                            ADIOS_LOCKMODE_ALL, timeout_sec);
    else
        f = adios_read_open_file(inputfile, adios_read_method, comm);
    
    if (f == NULL)
    {
        std::cout << adios_errmsg() << std::endl;
        return -1;
    }

    t[1] = MPI_Wtime();
    ADIOS_VARINFO *vgnx = adios_inq_var(f, "gnx");
    unsigned long gnx = *(unsigned long *)vgnx->value;
    int nsteps = vgnx->nsteps;

    // 1D decomposition of the columns, which is inefficient for reading!
    uint64_t readsize = gnx / nproc;
    uint64_t offset = rank * readsize;
    if (rank == nproc - 1)
    {
        // last process should read all the rest of columns
        readsize = gnx - readsize * (nproc - 1);
    }

    //printf("rank %d reads %d columns from offset %d\n", rank, readsize, offset);
    std::vector<int> x(nsteps * readsize) ;

    // Create a 2D selection for the subset
    ADIOS_SELECTION *sel = adios_selection_boundingbox(1, &offset, &readsize);

    // Arrays are read by scheduling one or more of them
    // and performing the reads at once
    t[2] = MPI_Wtime();
    adios_schedule_read(f, sel, "x", 0, nsteps, x.data());
    adios_perform_reads(f, 1);
    t[3] = MPI_Wtime();

    adios_read_close(f);
    t[4] = MPI_Wtime();

    double elap[3];
    elap[0] = t[4] - t[0] - (t[2] - t[1]);
    elap[1] = t[4] - t[2];
    elap[2] = t[4] - t[3];
    
    if (rank == 0)
    {
        printf("====== Info =======\n");
        printf("%10s: %lu\n", "gnx", gnx);
        printf("%10s: %d\n", "nsteps", nsteps);
        printf("%10s: %d\n", "Total NPs", nproc);
        printf("%10s: %.3f\n", "MBs/proc", (float) sizeof(int)*gnx/nproc/1024/1024);
        for (int i=0; i<nproc; i++)
            printf("%10s: %5d %s\n", "MAP", i, &hostmap[i*MPI_MAX_PROCESSOR_NAME]);
        printf("===================\n\n");
        printf(">>> %5s %9s %12s %9s %12s %9s %12s\n",
               "rank", "t3-t0", "(MB/s)", "t3-t1", "(MB/s)", "t3-t2", "(MB/s)");
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    
    printf(">>> %5d %9.03f %12.03f %9.03f %12.03f %9.03f %12.03f\n",
            rank, 
            elap[0], (float)sizeof(int) * nsteps * readsize / elap[0] / 1024 / 1024,
            elap[1], (float)sizeof(int) * nsteps * readsize / elap[1] / 1024 / 1024,
            elap[2], (float)sizeof(int) * nsteps * readsize / elap[2] / 1024 / 1024);

    double melap[3];
    MPI_Reduce(elap, melap, 3, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (rank == 0)
    {

        printf(">>> %5s %9.03f %12.03f %9.03f %12.03f %9.03f %12.03f\n",
                "ALL", 
                melap[0], (float)sizeof(int) * nsteps * gnx / melap[0] / 1024 / 1024,
                melap[1], (float)sizeof(int) * nsteps * gnx / melap[1] / 1024 / 1024,
                melap[2], (float)sizeof(int) * nsteps * gnx / melap[2] / 1024 / 1024);
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    //printData(x, nsteps, readsize, offset, rank);
    summarizeData(x, gnx, nsteps, readsize, offset, rank);
    adios_free_varinfo(vgnx);
    adios_selection_delete(sel);
    adios_read_finalize_method(adios_read_method);
    MPI_Barrier(comm);
    MPI_Finalize();
    return 0;
}

void printData(std::vector<int> x, int steps, uint64_t nelems,
        uint64_t offset, int rank)
{
    std::ofstream myfile;
    // The next line does not work with PGI compiled code
    //std::string filename = "reader." + std::to_string(rank)+".txt";
    std::stringstream ss;
    ss << "reader." << rank << ".txt";
    std::string filename = ss.str();

    myfile.open(filename);
    myfile << "rank=" << rank << " columns=" << nelems
           << " offset=" << offset
           << " steps=" << steps << std::endl;
    myfile << " time   columns " << offset << "..."
           << offset+nelems-1 << std::endl;

    myfile << "             ";
    for (uint64_t j = 0; j < nelems; j++)
        myfile << std::setw(5) << offset + j;
    myfile << "\n-------------";
    for (uint64_t j = 0; j < nelems; j++)
        myfile << "-----";
    myfile << std::endl;


    for (int step = 0; step < steps; step++)
    {
        myfile << std::setw(5) << step << "        ";
        for (uint64_t i = 0; i < nelems; i++)
        {
            myfile << std::setw(5) << x[step*nelems + i] << " ";
        }
        myfile << std::endl;
    }
    myfile.close();
}

void summarizeData(std::vector<int> x, unsigned long gnx,  int steps, uint64_t nelems,
        uint64_t offset, int rank)
{
    for (int step = 0; step < steps; step++)
    {
        int ok = 1;
        uint64_t i;
        for (i = 0; i < nelems; i++)
        {
            if (x[step*nelems + i] != (gnx*step + offset + i)%INT_MAX)
            {
                ok = 0;
                break;
            }
        }
        if (ok)
        {
            printf ("rank: %d step:%d ... PASS\n", rank, step);
        }
        else
        {
            printf ("rank: %d step:%d ... ERROR: %d %d\n", rank, step, x[step*nelems + i], (int)(gnx*step + offset + i));
        }
    }
}