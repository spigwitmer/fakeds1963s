cli-out:
	mkdir -p cli-out

cli-out/%.o: %.c cli-out
	gcc -c -O0 -g3 $< -o $@

%.o: %.c
	gcc -c -O0 -g3 $< -o $@

testclient: testclient.o cli-out/ds2480.o cli-out/ds1963s.o
	gcc -O0 -g3 $^ -o testclient

clean:
	rm -rf cli-out testclient testclient.o
