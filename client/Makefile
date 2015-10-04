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
ow/lib/userial/link/Simulated/simlink.c

# symlink these from the main project
ds2480sim_objects := ds2480sim.c ds1963s.c crcutil.c sha1.c

all: lib1w.a test1w
lib1w.a: $(lib1w_objects:.c=.o)
	ar rcs $@ $^

ds2480sim.a: $(ds2480sim_objects:.c=.o)
	ar rcs $@ $^

$(ds2480sim_objects):
	[ -e ds2480sim.c ] || ln -s ../ds2480sim.c .
	[ -e ds1963s.c ] || ln -s ../ds1963s.c .
	[ -e crcutil.c ] || ln -s ../extern/crcutil.c .
	[ -e sha1.c ] || ln -s ../extern/sha1.c .

%.o: %.c
	gcc -fPIC -I.. -I../extern/ -Iow/common/ -Iow/lib/userial/shared/ -c -O0 -g3 $^ -o $@

test1w: test1w.o lib1w.a ds2480sim.a
	gcc -fPIC -lm -O0 -g3 lib1w.a ds2480sim.a $^ -o $@

clean:
	rm -f $(lib1w_objects:.c=.o) $(ds2480sim_objects:.c=.o) lib1w.a ds2480sim.a test1w test1w.o
