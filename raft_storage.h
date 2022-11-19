#ifndef raft_storage_h
#define raft_storage_h

#include "raft_protocol.h"
#include <fcntl.h>
#include <fstream>
#include <mutex>

template <typename command>
class raft_storage {
public:
    raft_storage(const std::string &file_dir);
    ~raft_storage();
    // Lab3: Your code here
    int persist(log_entry<command> &log_entry);
private:
    std::mutex mtx;
    // Lab3: Your code here
};

template <typename command>
raft_storage<command>::raft_storage(const std::string &dir) {
    // Lab3: Your code here

}

template <typename command>
raft_storage<command>::~raft_storage() {
    // Lab3: Your code here
}

#endif // raft_storage_h