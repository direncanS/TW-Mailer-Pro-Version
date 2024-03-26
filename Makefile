#############################################################################################
# Makefile
#############################################################################################
# GCC is the GNU Compiler Collection and includes the standard C compiler
CC=gcc

# Compiler Flags: https://linux.die.net/man/1/gcc
#############################################################################################
# -g: produces debugging information (for gdb)
# -Wall: enables all the warnings
# -Wextra: further warnings
# -O: Optimizer turned on
# -I: Add the directory dir to the list of directories to be searched for header files.
# -c: says not to run the linker
# -pthread: Add support for multithreading with the POSIX threads library.
#############################################################################################
CFLAGS=-g -Wall -Wextra -O -I/usr/include -pthread
LIBS=-lldap -llber -lncurses
GTEST=/usr/local/lib/libgtest.a

# Targets for cleaning, compiling, and linking
rebuild: clean all

all: bin/server bin/client

clean:
	clear
	rm -f bin/* obj/*

# Ensure the 'bin' and 'obj' directories exist
directories:
	mkdir -p bin obj

# Target for server object file
obj/server.o: server.c | directories
	$(CC) $(CFLAGS) -o $@ $< -c

# Target for client object file
obj/client.o: ldap_client.c | directories
	$(CC) $(CFLAGS) -o $@ $< -c

# Target for server executable
bin/server: obj/server.o | directories
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Target for client executable
bin/client: obj/client.o | directories
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

