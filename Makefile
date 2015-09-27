TARGET = fakeds1963s

obj-m += fakeds1963s.o
$(TARGET)-y := fake-ds1963s.o ds2480sim.o ds1963s.o crcutil.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
