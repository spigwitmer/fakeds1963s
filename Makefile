obj-m += fakeds1963s.o
fakeds1963s-y := ds2480.o ds1963s.o

cli-out:
	mkdir -p cli-out

cli-out/%.o: %.c cli-out
	gcc -c -O0 -g3 $< -o $@

testclient: testclient.o cli-out/ds2480.o cli-out/ds1963s.o
	gcc -O0 -g3 $^ -o testclient
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -rf cli-out testclient testclient.o
