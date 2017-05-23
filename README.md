# Build
```
$ make
```

# Run
Writer (100 integers per process):
```
$ mpirun -n 4 writer -n 100 out.bp
```

Reader:
```
$ mpirun -n 2 reader out.bp
```

Writer options are as follow:
```
$ writer -h
Usage: writer [OPTIONS]... [FILE]

  -h, --help                Print help and exit
  -V, --version             Print version and exit
  -w, --writemethod=STRING  ADIOS write method  (default=`POSIX')
      --wparams=STRING      write method params
                              (default=`local-fs=1;have_metadata_file=1')
  -n, --len=LONG            array length  (default=`1000')
      --nstep=INT           number of time steps  (default=`1')
      --sleep=INT           interval time  (default=`3')
      --append              append  (default=off)
```

# Notes

## Add command line options
[GNU Gengetopt](https://www.gnu.org/software/gengetopt/gengetopt.html) is needed. 
Change ```cmdline.ggo``` and build:
```
$ make ggo
$ make
```
