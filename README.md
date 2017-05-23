# Build
```
$ make
```

# Run
Writer (100 integers per process):
```
$ mpirun -n 5 writer -f out.bp -n 100
```

Reader:
```
$ mpirun -n 2 reader out.bp
```

Writer options are as follow:
```
$ writer -h
Usage: writer [OPTIONS]...

  -h, --help                Print help and exit
  -V, --version             Print version and exit
  -f, --filename=STRING     output filename  (default=`out.bp')
  -w, --writemethod=STRING  ADIOS write method  (default=`POSIX')
      --wparams=STRING      write method params
                              (default=`local-fs=1;have_metadata_file=1')
  -n, --len=LONG            array length  (default=`1000')
      --nstep=INT           number of time steps  (default=`1')
      --sleep=INT           interval time  (default=`3')
      --append              append  (default=off)
```