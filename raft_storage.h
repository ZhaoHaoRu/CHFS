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
    std::string log_path;
    std::string metadata_path;
};

template <typename command>
raft_storage<command>::raft_storage(const std::string &dir) {
    // Lab3: Your code here
    log_path = dir + "/log.bin";
    metadata_path = dir + "/metadata_path.bin";
}

template <typename command>
raft_storage<command>::~raft_storage() {
    // Lab3: Your code here
}

template <typename command>
int raft_storage<command>::persist(log_entry<command> &log_entry) {
    std::unique_lock<std::mutex> lock(mtx);

    std::ofstream out_file(log_path, std::ios::out | std::ios::binary | std::ios::app);

    if(!out_file) {
        return -1;
    }
    
    // format: log index | term | cmd size | cmd
    int bias = 0;
    int cmd_size = log_entry.cmd.size();
    int total_size = sizeof(int) * 3 + cmd_size; 
    char data[total_size] = {' '};

    memcpy(data, &log_entry.log_index, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, &log_entry.term, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, &cmd_size, sizeof(int));
    bias += sizeof(int);
    char cmd_data[cmd_size] = {' '};
    log_entry.cmd.serialize(cmd_data, cmd_size);
    memcpy(data + bias, cmd_data, cmd_size);

    out_file.write(data, total_size);
    out_file.close();
    return 0;
}

#endif // raft_storage_h