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
    void persist(std::vector<log_entry<command>> &log_entries);
    void restore(std::vector<log_entry<command>> &logs);
    void persist_metadata(int term, int vote_for);
    void restore_metadata(int &term, int &vote_for);
    void persist_snapshot(int last_included_index, int last_included_term, std::vector<char> str, int data_size);
    void restore_snapshot(int &last_included_index, int &last_included_term, std::vector<char> &str, int &data_size);

private:
    std::mutex mtx;
    // Lab3: Your code here
    std::string log_path;
    std::string metadata_path;
    std::string snapshot_path;
};

template <typename command>
raft_storage<command>::raft_storage(const std::string &dir) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    log_path = dir + "/log.bin";
    metadata_path = dir + "/metadata.bin";
    snapshot_path = dir +"/snapshot.bin";
}

template <typename command>
raft_storage<command>::~raft_storage() {
    // Lab3: Your code here
}

template <typename command>
void raft_storage<command>::persist(std::vector<log_entry<command>> &log_entries) {
    std::ofstream out_file(log_path, std::ios::out | std::ios::binary | std::ios::trunc);

    if(!out_file) {
        return;
    }

    out_file.seekp(0, std::ios_base::beg);
    // persist the size first
    int count = log_entries.size();
    out_file.write((char*)&count, sizeof(int));
    // format: log index | term | cmd size | cmd
    for(auto &log_entry : log_entries) {
        int bias = 0;
        int cmd_size = log_entry.cmd.size();
        int total_size = sizeof(uint32_t) * 3 + cmd_size; 
        char *data = new char[total_size];
        memset(data, ' ', total_size);
        uint32_t tmp = log_entry.log_index;
        // printf("the log_entry.log_index: %d\n", log_entry.log_index);
        memcpy(data, &tmp, sizeof(uint32_t));
        bias += sizeof(uint32_t);
        tmp = log_entry.term;
        // printf("the log_entry.term: %d\n", log_entry.term);
        memcpy(data + bias, &tmp, sizeof(uint32_t));
        bias += sizeof(uint32_t);
        // printf("the cmd size: %d\n", cmd_size);
        tmp = cmd_size;
        memcpy(data + bias, &tmp, sizeof(uint32_t));
        bias += sizeof(uint32_t);
        char *cmd_data = new char[cmd_size];
        memset(cmd_data, ' ', cmd_size);
        log_entry.cmd.serialize(cmd_data, cmd_size);
        printf("the cmd data: %d\n", log_entry.cmd.value);
        memcpy(data + bias, cmd_data, cmd_size);
        // printf("the data: %s", data);
        out_file.write(data, total_size);
        delete []data;
        delete []cmd_data;
    }

    out_file.close();
    return;
}

template <typename command>
void raft_storage<command>::restore(std::vector<log_entry<command>> &logs) {
    std::unique_lock<std::mutex> lock(mtx);
    std::ifstream file(log_path, std::ios::binary);

    if(!file) {
        return;
    }

    int i = 0; 
    file.seekg(0,std::ios::beg);
    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(int));
    // fprintf(stdout, "the file path: %s, the count: %d\n", log_path.c_str(), count);
    while(!file.eof() && i < count) {
        ++i;
        log_entry<command> new_log_entry;
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&new_log_entry.log_index), sizeof(uint32_t));
        // fprintf(stdout, "the log log_index: %d\n", new_log_entry.log_index);
        file.read(reinterpret_cast<char*>(&new_log_entry.term), sizeof(uint32_t));
        fprintf(stdout, "the log term: %d\n", new_log_entry.term);
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        fprintf(stdout, "the log size: %d\n", size);
        char buf[size] = {' '};
        file.read(buf, size);
        new_log_entry.cmd.deserialize(buf, size);
        fprintf(stdout, "the cmd value: %d\n", new_log_entry.cmd.value);
        logs.emplace_back(new_log_entry);
    }

    fprintf(stdout, "the log count: %d\n", i);
    file.close();
}

template <typename command>
void raft_storage<command>::persist_metadata(int term, int vote_for) {
    
    std::ofstream out_file(metadata_path, std::ios::out | std::ios::binary);

    if(!out_file) {
        return;
    }

    int total_size = sizeof(int) * 2;
    int bias = 0;
    char data[total_size] = {' '};
    memcpy(data, &term, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, &vote_for, sizeof(int));
    out_file.write(data, total_size);

    out_file.close();
    return;
}

template <typename command>
void raft_storage<command>::restore_metadata(int &term, int &vote_for) {
    std::unique_lock<std::mutex> lock(mtx);
    std::ifstream file(metadata_path, std::ios::binary);

    if(!file) {
        return;
    }

    int tmp_term = 0, tmp_vote_for = 0;
    while(!file.eof()) {
        file.read(reinterpret_cast<char*>(&tmp_term), sizeof(int));
        if(file.eof()) {
            break;
        }
        file.read(reinterpret_cast<char*>(&tmp_vote_for), sizeof(int));
        term = tmp_term;
        vote_for = tmp_vote_for;
    }
}

template <typename command>
void raft_storage<command>::persist_snapshot(int last_included_index, int last_included_term, std::vector<char> str, int data_size) {
    std::ofstream out_file(snapshot_path, std::ios::out | std::ios::binary);

    if(!out_file) {
        return;
    }
    int total_size = sizeof(int) * 3 + data_size;
    int bias = 0;
    char data[total_size] = {' '};
    memcpy(data, &last_included_index, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, &last_included_term, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, &data_size, sizeof(int));
    bias += sizeof(int);
    memcpy(data + bias, str.data(), data_size);
    out_file.write(data, total_size);

    out_file.close();
    return;
}

template <typename command>
void raft_storage<command>::restore_snapshot(int &last_included_index, int &last_included_term, std::vector<char> &str, int &data_size) {
    std::unique_lock<std::mutex> lock(mtx);
    std::ifstream file(snapshot_path, std::ios::binary);

    if(!file) {
        return;
    }   

    file.read(reinterpret_cast<char*>(&last_included_index), sizeof(int));
    file.read(reinterpret_cast<char*>(&last_included_term), sizeof(int));
    file.read(reinterpret_cast<char*>(&data_size), sizeof(int));

    ///@note avoid segmentation fault
    str.resize(data_size);
    file.read(const_cast<char*>(str.data()), data_size);
    return;
}
#endif // raft_storage_h