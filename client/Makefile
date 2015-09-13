lib1w_objects := ow/common/sha18.c \
ow/common/shadebit.c \
ow/common/findtype.c \
ow/common/swt1C.c \
ow/common/mbscrex.c \
ow/common/mbee77.c \
ow/common/rawmem.c \
ow/common/sha33.c \
ow/common/swt3A.c \
ow/common/mbeewp.c \
ow/common/atod26.c \
ow/common/mbscrcrc.c \
ow/common/owcache.c \
ow/common/swt05.c \
ow/common/mbscrx77.c \
ow/common/mbee.c \
ow/common/humutil.c \
ow/common/swt1f.c \
ow/common/swt29.c \
ow/common/crcutil.c \
ow/common/swt12.c \
ow/common/mbsha.c \
ow/common/owprgm.c \
ow/common/mbeprom.c \
ow/common/mbnv.c \
ow/common/weather.c \
ow/common/pw77.c \
ow/common/mbscree.c \
ow/common/mbshaee.c \
ow/common/mbappreg.c \
ow/common/mbnvcrc.c \
ow/common/atod20.c \
ow/common/temp10.c \
ow/common/owfile.c \
ow/common/ioutil.c \
ow/common/owerr.c \
ow/common/cnt1d.c \
ow/common/mbscr.c \
ow/common/shaib.c \
ow/common/screenio.c \
ow/common/owpgrw.c \
ow/lib/userial/shared/ownetu.c \
ow/lib/userial/shared/owtrnu.c \
ow/lib/userial/shared/ds2480ut.c \
ow/lib/userial/shared/owsesu.c \
ow/lib/userial/shared/owllu.c \
ow/lib/userial/link/Linux/linuxlnk.c

all: lib1w.so test1w
lib1w.so: $(lib1w_objects:.c=.o)
	gcc -shared -fPIC -O0 -g3 $^ -o $@

%.o: %.c
	gcc -fPIC -DDEBUG -Iow/common/ -Iow/lib/userial/shared/ -c -O0 -g3 $^ -o $@

test1w: test1w.o lib1w.so
	gcc -fPIC -L$(shell pwd) -l1w -lm -O0 -g3 $< -o $@

clean:
	rm -f $(lib1w_objects:.c=.o) lib1w.so test1w test1w.o
