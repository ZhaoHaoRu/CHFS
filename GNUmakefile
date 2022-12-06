LAB=4
SOL=0
RPC=./rpc
LAB1GE=$(shell expr $(LAB) \>\= 1)
LAB2GE=$(shell expr $(LAB) \>\= 2)
LAB2AGE=$(shell expr $(LAB) \>\= 2a)
LAB2BGE=$(shell expr $(LAB) \>\= 2b)
LAB2CGE=$(shell expr $(LAB) \>\= 2c)
LAB3GE=$(shell expr $(LAB) \>\= 3)
LAB4GE=$(shell expr $(LAB) \>\= 4)
LAB5GE=$(shell expr $(LAB) \>\= 5)

CXXFLAGS =  -g -MMD -Wall -I. -I$(RPC) -DLAB=$(LAB) -DSOL=$(SOL) -D_FILE_OFFSET_BITS=64
FUSEFLAGS= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25 -I/usr/local/include/fuse -I/usr/include/fuse
RPCLIB=librpc.a

ifeq ($(shell uname -s),Darwin)
  MACFLAGS= -D__FreeBSD__=10
else
  MACFLAGS=
endif
LDFLAGS = -L. -L/usr/local/lib
LDLIBS = -lpthread 
ifeq ($(LAB1GE),1)
  ifeq ($(shell uname -s),Darwin)
    ifeq ($(shell sw_vers -productVersion | sed -e "s/.*\(10\.[0-9]\).*/\1/"),10.6)
      LDLIBS += -lfuse_ino64
    else
      LDLIBS += -lfuse
    endif
  else
    LDLIBS += -lfuse
  endif
endif
LDLIBS += $(shell test -f `gcc -print-file-name=librt.so` && echo -lrt)
LDLIBS += $(shell test -f `gcc -print-file-name=libdl.so` && echo -ldl)
CC = g++
CXX = g++

lab:  lab$(LAB)
lab1: part1_tester chfs_client
lab2a: chfs_client 
lab2b: lock_server lock_tester lock_demo chfs_client extent_server test-lab2b-part1-g test-lab2b-part2-a test-lab2b-part2-b 
lab3: raft_test chfs_client test-lab3-part5-b extent_server_dist 
lab4: raft_test chfs_client extent_server_dist mr_coordinator mr_worker mr_sequential

rpclib=rpc/rpc.cc rpc/connection.cc rpc/pollmgr.cc rpc/thr_pool.cc rpc/jsl_log.cc gettime.cc
rpc/librpc.a: $(patsubst %.cc,%.o,$(rpclib))
	rm -f $@
	ar cq $@ $^
	ranlib rpc/librpc.a

rpc/rpctest=rpc/rpctest.cc
rpc/rpctest: $(patsubst %.cc,%.o,$(rpctest)) rpc/$(RPCLIB)

chfs_client=chfs_client.cc extent_client.cc fuse.cc extent_server.cc inode_manager.cc

chfs_client : $(patsubst %.cc,%.o,$(chfs_client)) rpc/$(RPCLIB)

extent_server=extent_server.cc extent_smain.cc inode_manager.cc
extent_server : $(patsubst %.cc,%.o,$(extent_server)) rpc/$(RPCLIB)

extent_server_dist= extent_server_dist.cc extent_sdist_main.cc extent_server.cc inode_manager.cc chfs_state_machine.cc  raft_protocol.cc raft_test_utils.cc 
extent_server_dist: $(patsubst %.cc,%.o,$(extent_server_dist)) rpc/$(RPCLIB)

test-lab3-part5-b= extent_server_dist.cc test-lab3-part5-b.cc extent_server.cc inode_manager.cc chfs_state_machine.cc raft_protocol.cc raft_test_utils.cc chfs_client.cc extent_client.cc
test-lab3-part5-b: $(patsubst %.cc,%.o,$(test-lab3-part5-b)) rpc/$(RPCLIB)

raft_test=raft_protocol.cc raft_test_utils.cc raft_test.cc
raft_test : $(patsubst %.cc,%.o,$(raft_test)) rpc/$(RPCLIB)

mr_sequential=mr_sequential.cc
mr_sequential : $(patsubst %.cc,%.o,$(mr_sequential))

mr_coordinator=mr_coordinator.cc
mr_coordinator : $(patsubst %.cc,%.o,$(mr_coordinator)) rpc/$(RPCLIB)

mr_worker=mr_worker.cc
mr_worker : $(patsubst %.cc,%.o,$(mr_worker)) rpc/$(RPCLIB)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

fuse.o: fuse.cc
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(MACFLAGS) $<

# mklab.inc is needed by 6.824 staff only. Just ignore it.
-include mklab.inc

-include *.d
-include rpc/*.d

clean_files=rpc/rpctest rpc/*.o rpc/*.d *.o *.d chfs_client extent_server extent_server_dist lock_server lock_tester lock_demo rpctest test-lab2b-part1-g test-lab2b-part2-a test-lab2b-part2-b demo_client demo_server raft_test raft_temp raft_chfs_test test-lab3-part5-b mr_coordinator mr_worker mr_sequential rpc/$(RPCLIB)
.PHONY: clean handin
clean: 
	rm $(clean_files) -rf 

handin_ignore=$(clean_files) core* *log .git
handin_file=lab$(LAB).tgz
labdir=$(shell basename $(PWD))
handin: 
	@bash -c "cd ../; tar -X <(tr ' ' '\n' < <(echo '$(handin_ignore)')) -czvf $(handin_file) $(labdir); mv $(handin_file) $(labdir); cd $(labdir)"
	@echo Please modify lab4.tgz to lab4_[your student id].tgz and upload it to Canvas \(https://oc.sjtu.edu.cn/courses/49245\)
	@echo Thanks!

rpcdemo: demo_server demo_client

demo_client:
	$(CXX) $(CXXFLAGS) demo_client.cc rpc/$(RPCLIB) $(LDFLAGS) $(LDLIBS) -o demo_client

demo_server:
	$(CXX) $(CXXFLAGS) demo_server.cc rpc/$(RPCLIB) $(LDFLAGS) $(LDLIBS) -o demo_server