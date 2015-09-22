obj-m += fakeds1963s.o
fakeds1963s-y := ds2480.o ds1963s.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
