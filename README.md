# Build
```
$ make
```

# Run
Writer (100 integers per process):
```
$ mpirun -n 5 writer out.bp 100
```

Reader:
```
$ mpirun -n 2 reader out.bp
```
