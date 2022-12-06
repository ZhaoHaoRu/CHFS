// this is the extent server

#ifndef extent_server_dist_h
#define extent_server_dist_h

#include "extent_protocol.h"
#include <map>
#include <string>
#include "raft.h"
#include "extent_server.h"
#include "raft_test_utils.h"
#include "chfs_state_machine.h"

using chfs_raft = raft<chfs_state_machine, chfs_command_raft>;
using chfs_raft_group = raft_group<chfs_state_machine, chfs_command_raft>;

class extent_server_dist {
public:
    chfs_raft_group *raft_group;
    extent_server_dist(const int num_raft_nodes = 3) {
        raft_group = new chfs_raft_group(num_raft_nodes);
    };

    chfs_raft *leader() const;

    int create(uint32_t type, extent_protocol::extentid_t &id);
    int put(extent_protocol::extentid_t id, std::string, int &);
    int get(extent_protocol::extentid_t id, std::string &);
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
    int remove(extent_protocol::extentid_t id, int &);

    ~extent_server_dist();
};

#endif
