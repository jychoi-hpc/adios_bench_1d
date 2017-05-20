CXX=mpicxx
#OMPI_CXX=g++-5 
CXXFLAGS=-g -std=c++11
LDFLAGS=-g

## Set ADIOS_DIR here or before doing make
ADIOS_INC=$(shell adios_config -c)
ADIOS_LIB=$(shell adios_config -l)

default: writer reader

all: default
help: default



%.o : %.cpp 
	${CXX} ${CXXFLAGS} ${ADIOS_INC} -c $< 

writer: writer.o
	${CXX} ${LDFLAGS} -o writer $^ ${ADIOS_LIB} 

reader: reader.o
	${CXX} ${LDFLAGS} -o reader $^ ${ADIOS_LIB} 

clean:
	rm -f *.o core.* writer reader

distclean: clean
	rm -f *[0-9].txt
	rm -f *.png minmax 
	rm -rf *.bp *.bp.dir *.idx
	rm -f *.h5
	rm -f conf



