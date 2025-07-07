CFLAGS+=-O
CFLAGS+=-fPIC
LDFLAGS=
#CHECK=valgrind --leak-check=full --show-leak-kinds=all
#CHECK=ddd
#CHECK=gdb
#CHECK=time

.PHONY: all
all: README.md libmap.a libmap.so test_map libtimer.a libtimer.so test_timer

#### Examples
.PHONY: test_map
test_map: libmap.so examples/test_map
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. $(CHECK) ./examples/test_map
	@echo "*********************"

examples/test_map: CFLAGS+=-std=c23
examples/test_map: CPPFLAGS+=-I.
examples/test_map: LDFLAGS+=-L.
examples/test_map: LDLIBS=-lmap
examples/test_map: examples/test_map.c

.PHONY: test_timer
test_timer: libmap.so libtimer.so examples/test_timer
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. $(CHECK) ./examples/test_timer
	@echo "*********************"

examples/test_timer: CFLAGS+=-std=c23
examples/test_timer: CPPFLAGS+=-I.
examples/test_timer: LDFLAGS+=-L.
examples/test_timer: LDLIBS=-ltimer
examples/test_timer: examples/test_timer.c

#### Libraries
#C11 compliant, since <threads.h> is required.
%.o: CFLAGS+=-std=c11

libtimer.a: ARFLAGS=rcs
libtimer.a: timer.o map.o
	rm -f -- "$@"
	$(AR) $(ARFLAGS) -- "$@" $^
	@nm -A -g --defined-only -- "$@"


libtimer.so: LDFLAGS+=-shared
libtimer.so: timer.o map.o
	$(CC) $(LDFLAGS) -o "$@" $^

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(LDFLAGS) -o "$@" "$^"

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	rm -f -- "$@"
	$(AR) $(ARFLAGS) -- "$@" "$^"
	@nm -A -g --defined-only -- "$@"

.INTERMEDIATE: timer.o map.o

#### Documentation
.PHONY: doc
doc: README_map.html README_timer.html README.md
.INTERMEDIATE: README_map.md README_timer.md

README.md: README_map.md README_timer.md
	cat README_map.md README_timer.md >| README.md

README_%.md: %.h ./h2md
	chmod +x ./h2md
	./h2md "$<" >| "$@"

