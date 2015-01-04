all:phptrace_unittest

INCS=-I ../common -I ./ -I ../cmdtool -I ../phpext

CFLAGS=$(INCS) -g -O0 -Wall
CXXFLAGS=$(INCS) -g -O0 -Wall

protocol_unittest_deps= phptrace_protocol_unittest.o phptrace_protocol.o
mmap_unittest_deps= phptrace_mmap_unittest.o phptrace_mmap.o
ctrl_unittest_deps= phptrace_ctrl_unittest.o phptrace_ctrl.o

sds.o: ../common/sds/sds.c
	$(CC) $(CFLAGS) -c -o sds.o ../common/sds/sds.c

phptrace_protocol_unittest.o:phptrace_protocol_unittest.cc
phptrace_protocol.o: ../common/phptrace_protocol.c
	$(CC) $(CFLAGS) -c -o phptrace_protocol.o ../common/phptrace_protocol.c

phptrace_mmap_unittest.o: phptrace_mmap_unittest.cc
phptrace_mmap.o: ../common/phptrace_mmap.c
	$(CC) $(CFLAGS) -c -o phptrace_mmap.o ../common/phptrace_mmap.c

phptrace_ctrl_unittest.o: phptrace_ctrl_unittest.cc
phptrace_ctrl.o: ../common/phptrace_ctrl.c 
	$(CC) $(CFLAGS) -c -o phptrace_ctrl.o ../common/phptrace_ctrl.c


phptrace_unittest: $(protocol_unittest_deps) $(mmap_unittest_deps) $(ctrl_unittest_deps) sds.o
	$(CXX) -o phptrace_unittest $(protocol_unittest_deps) \
								$(mmap_unittest_deps)     \
								$(ctrl_unittest_deps)     \
								sds.o                     \
		$(CXXFLAGS) -lgtest -lgtest_main -lpthread

clean:
	rm -f *.o phptrace_unittest
