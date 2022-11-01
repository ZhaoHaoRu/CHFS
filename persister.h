#ifndef persister_h
#define persister_h

#include <fcntl.h>
#include <string.h>
#include <string>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include "rpc.h"

#define MAX_LOG_SZ 131072

/*
 * Your code here for Lab2A:
 * Implement class chfs_command, you may need to add command types such as
 * 'create', 'put' here to represent different commands a transaction requires. 
 * 
 * Here are some tips:
 * 1. each transaction in ChFS consists of several chfs_commands.
 * 2. each transaction in ChFS MUST contain a BEGIN command and a COMMIT command.
 * 3. each chfs_commands contains transaction ID, command type, and other information.
 * 4. you can treat a chfs_command as a log entry.
 */

const int begin_length = 9;
const int commit_length = 9;
const int create_length = 13;
const int put_basic_length = 21;
const int remove_length = 17;
const char tags[5] = {'B', 'C', 'R', 'P', 'D'};

class chfs_command {
public:
    typedef unsigned long long txid_t;
    enum cmd_type {
        CMD_BEGIN = 0,
        CMD_COMMIT,
        CMD_CREATE,
        CMD_PUT,
        CMD_REMOVE
    };

    cmd_type type = CMD_BEGIN;
    txid_t id = 0;
    // add info
    std::string input_string;
    extent_protocol::extentid_t inum;
    uint32_t inode_type;

    // constructor
    chfs_command() {}

    chfs_command(cmd_type cmd_ty, txid_t new_id):type(cmd_ty), id(new_id) {}

    chfs_command(cmd_type cmd_ty, txid_t new_id,
        uint32_t inode_ty): type(cmd_ty), id(new_id), inode_type(inode_ty) {}

    chfs_command(cmd_type cmd_ty, txid_t new_id, extent_protocol::extentid_t new_inum,
        std::string &input): type(cmd_ty), id(new_id), input_string(input), inum(new_inum) {}

    chfs_command(cmd_type cmd_ty, txid_t new_id, extent_protocol::extentid_t new_inum)
        :type(cmd_ty), id(new_id), inum(new_inum) {}

    uint64_t size() const {
        uint64_t s = sizeof(cmd_type) + sizeof(txid_t);
        return s;
    }
};

/*
 * Your code here for Lab2A:
 * Implement class persister. A persister directly interacts with log files.
 * Remember it should not contain any transaction logic, its only job is to 
 * persist and recover data.
 * 
 * P.S. When and how to do checkpoint is up to you. Just keep your logfile size
 *      under MAX_LOG_SZ and checkpoint file size under DISK_SIZE.
 */
template<typename command>
class persister {
private:
    chfs_command::txid_t tid;

public:
    persister(const std::string& file_dir);
    ~persister();

    // persist data into solid binary file
    // You may modify parameters in these functions
    ///@param id for inum
    ///@param buf for written data
    ///@param type for inum type
    void append_log(const command& log);
    void checkpoint();

    // restore data from solid binary file
    // You may modify parameters in these functions
    // TODO: add beautify
    // void restore_data(std::string file_name);
    void restore_logdata();
    void restore_checkpoint();
    std::vector<command> get_logs();
    std::vector<command> get_checkpoint();
    bool is_log_too_long(int length);

    void clear();

private:
    std::mutex mtx;
    std::string file_dir;
    std::string file_path_checkpoint;
    std::string file_path_logfile;

    // restored log data
    std::vector<command> log_entries;
    // restored the checkpoint
    std::vector<command> checkpoint_entries;
};

template<typename command>
persister<command>::persister(const std::string& dir){
    // DO NOT change the file names here
    file_dir = dir;
    file_path_checkpoint = file_dir + "/checkpoint.bin";
    file_path_logfile = file_dir + "/logdata.bin";
}

template<typename command>
persister<command>::~persister() {
    // Your code here for lab2A

}

template<typename command>
bool persister<command>::is_log_too_long(int length) {
    std::ofstream out_file(file_path_logfile, std::ios::out | std::ios::binary | std::ios::app);
    out_file.seekp(0, std::ios::end);
    int file_size = out_file.tellp();
    if(file_size + length > MAX_LOG_SZ) {
        return true;
    }
    return false;
}

template<typename command>
void persister<command>::append_log(const command& log) {
    // Your code here for lab2A
    chfs_command::cmd_type type = log.type;

    // begin:TID + tag 
    int bias = 0;
    
    // check whether need to add checkpoint
    int str_size = log.input_string.size();
    bool need_checkpoint = is_log_too_long(put_basic_length + str_size);
    if(need_checkpoint) {
        checkpoint();
    }
    
    std::ofstream out_file(file_path_logfile, std::ios::out | std::ios::binary | std::ios::app);
    
    if(type == chfs_command::cmd_type::CMD_BEGIN) {
        char data[begin_length] = {' '};
        memcpy(data, &log.id, sizeof(chfs_command::txid_t));
        bias += sizeof(chfs_command::txid_t);
        *(data + bias) = tags[type];

        out_file.write(data, begin_length);

    } else if(type == chfs_command::cmd_type::CMD_COMMIT) {
        // commit: TID + tag
        char data[commit_length] = {' '};
        memcpy(data, &log.id, sizeof(chfs_command::txid_t));
        bias = sizeof(chfs_command::txid_t);
        *(data + bias) = tags[type];

        out_file.write(data, commit_length);

    } else if(type == chfs_command::cmd_type::CMD_CREATE) {
        // create: TID + tag + inode_type
        char data[create_length] = {' '};
        memcpy(data, &log.id, sizeof(chfs_command::txid_t));
        bias = sizeof(chfs_command::txid_t);
        *(data + bias) = tags[type];
        std::cout << "append log type: " << log.inode_type << std::endl;
        bias++;
        memcpy(data + bias, &log.inode_type, sizeof(uint32_t));

        out_file.write(data, create_length);

    }  else if(type == chfs_command::cmd_type::CMD_PUT) {
        // put: TID + tag + inum + size + str
        uint32_t size = log.input_string.size();
        char data[size + put_basic_length] = {' '};;
        memcpy(data, &log.id, sizeof(chfs_command::txid_t));
        bias = sizeof(chfs_command::txid_t);
        *(data + bias) = tags[type];
        bias++;
        memcpy(data + bias, &log.inum, sizeof(extent_protocol::extentid_t));
        bias += sizeof(extent_protocol::extentid_t);
        memcpy(data + bias, &size, sizeof(uint32_t));
        bias += sizeof(uint32_t);
        memcpy(data + bias, log.input_string.c_str(), size);

        out_file.write(data, size + put_basic_length);

    } else if(type == chfs_command::cmd_type::CMD_REMOVE) {
        // remove: TID + tag + inum
        char data[remove_length] = {' '};;
        memcpy(data, &log.id, sizeof(chfs_command::txid_t));
        bias = sizeof(chfs_command::txid_t);
        *(data + bias) = tags[type];
        bias++;
        memcpy(data + bias, &log.inum, sizeof(extent_protocol::extentid_t));

        out_file.write(data, remove_length);
    }

    out_file.close();
}

template<typename command>
void persister<command>::checkpoint() {
    // Your code here for lab2A
    // get all log entries in logdata.bin
    log_entries.clear();
    restore_logdata();
    std::vector<command> logs = get_logs();
    
   
    // if put several times, only save the last time
    int n = logs.size();
    // key: inum, val: last put pos
    std::unordered_map<extent_protocol::extentid_t, int> dict;

    std::vector<bool> added(n, true);
    for(int i = 0; i < n; ++i) {
        if(logs[i].type == chfs_command::cmd_type::CMD_PUT) {
            if(!dict.count(logs[i].inum)) {
                dict[logs[i].inum] = i;
            } else {
                // not save the prev put
                int last_pos = dict[logs[i].inum];
                assert(last_pos >= 0 && last_pos < n);
                added[last_pos] = false;
                dict[logs[i].inum] = i;
            }
        }
    } 

    // third, put the valid entry to the checkpoint file
    std::ofstream out_file(file_path_checkpoint, std::ios::out | std::ios::binary | std::ios::app);
    for(int i = 0; i < n; ++i) {
        if(!added[i]) {
            continue;
        }

        // add put/create/remove/begin/commit
        // TODO: the code has duplicate parts ...
        int bias = 0;
        chfs_command::cmd_type type = logs[i].type;
        
        if(type == chfs_command::cmd_type::CMD_BEGIN) {
            // TODO: multi begin?
            if(i > 0 && logs[i - 1].type == chfs_command::cmd_type::CMD_BEGIN) {
                continue;
            }
            char data[begin_length] = {' '};
            memcpy(data, &logs[i].id, sizeof(chfs_command::txid_t));
            bias += sizeof(chfs_command::txid_t);
            *(data + bias) = tags[type];

            out_file.write(data, begin_length);
            // printf("checkpoint begin!\n");

        } else if(type == chfs_command::cmd_type::CMD_COMMIT) {
            // commit: TID + tag
            char data[commit_length] = {' '};
            memcpy(data, &logs[i].id, sizeof(chfs_command::txid_t));
            bias = sizeof(chfs_command::txid_t);
            *(data + bias) = tags[type];

            out_file.write(data, commit_length);
            // printf("checkpoint commit!\n");

        }else if(type == chfs_command::cmd_type::CMD_CREATE) {
            // create: TID + tag + inode_type
            char data[create_length] = {' '};
            memcpy(data, &logs[i].id, sizeof(chfs_command::txid_t));
            bias = sizeof(chfs_command::txid_t);
            *(data + bias) = tags[type];
            bias++;
            memcpy(data + bias, &logs[i].inode_type, sizeof(uint32_t));

            out_file.write(data, create_length);

        } else if(type == chfs_command::cmd_type::CMD_PUT) {
            // put: TID + tag + inum + size + str
            uint32_t size = logs[i].input_string.size();
            char data[size + put_basic_length] = {' '};;
            memcpy(data, &logs[i].id, sizeof(chfs_command::txid_t));
            bias = sizeof(chfs_command::txid_t);
            *(data + bias) = tags[type];
            bias++;
            memcpy(data + bias, &logs[i].inum, sizeof(extent_protocol::extentid_t));
            bias += sizeof(extent_protocol::extentid_t);
            memcpy(data + bias, &size, sizeof(uint32_t));
            bias += sizeof(uint32_t);
            memcpy(data + bias, logs[i].input_string.c_str(), size);

            out_file.write(data, size + put_basic_length);
            // printf("checkpoint put!\n");

        } else if(type == chfs_command::cmd_type::CMD_REMOVE) {
            // remove: TID + tag + inum
            char data[remove_length] = {' '};;
            memcpy(data, &logs[i].id, sizeof(chfs_command::txid_t));
            bias = sizeof(chfs_command::txid_t);
            *(data + bias) = tags[type];
            bias++;
            memcpy(data + bias, &logs[i].inum, sizeof(extent_protocol::extentid_t));

            out_file.write(data, remove_length);
        }
    }

    out_file.close();

    // clean the logdata
    clear();
}


template<typename command>
void persister<command>::restore_logdata() {
    // Your code here for lab2A
    // open the file
    std::ifstream file(file_path_logfile, std::ios::binary);
    if(!file) {
        std::cout << "not such file!" << std::endl;
        return;
    }
    // read the data and push it into log_entries
    // file.seekg(0,std::ios::beg);
    while(!file.eof()) {
        command new_commend;
        char cur_tag = ' ';
        file.read(reinterpret_cast<char*>(&new_commend.id), sizeof(chfs_command::txid_t));
        file.read(&cur_tag, 1);

        int ind = 0;
        while(ind < 5) {
            if(cur_tag == tags[ind]) {
                break;
            }
            ++ind;
        }

        if(ind == chfs_command::cmd_type::CMD_COMMIT) {
            new_commend.type = chfs_command::cmd_type::CMD_COMMIT; 
            
        } else if(ind == chfs_command::cmd_type::CMD_CREATE) {
            new_commend.type = chfs_command::cmd_type::CMD_CREATE;
            // inode type
            file.read(reinterpret_cast<char*>(&new_commend.inode_type), sizeof(uint32_t));
        
        } else if(ind == chfs_command::cmd_type::CMD_PUT) {
            new_commend.type = chfs_command::cmd_type::CMD_PUT;
            file.read(reinterpret_cast<char*>(&new_commend.inum), sizeof(extent_protocol::extentid_t));
            uint32_t size = 0;
            // size
            file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            // input string
            new_commend.input_string.resize(size);
            file.read(const_cast<char*>(new_commend.input_string.data()), size);

        } else if(ind == chfs_command::cmd_type::CMD_REMOVE) {
            new_commend.type = chfs_command::cmd_type::CMD_REMOVE;
            file.read(reinterpret_cast<char*>(&new_commend.inum), sizeof(extent_protocol::extentid_t));
        }

        log_entries.emplace_back(new_commend);
    }

};

template<typename command>
std::vector<command> persister<command>::get_logs() {
    return log_entries;
};

template<typename command>
std::vector<command> persister<command>::get_checkpoint() {
    return checkpoint_entries;
}

template<typename command>
void persister<command>::restore_checkpoint() {
    // Your code here for lab2A
    // open the file
    std::ifstream file(file_path_checkpoint, std::ios::binary);
    log_entries.clear();
    if(!file) {
        std::cout << "not such file!" << std::endl;
        return;
    }

    // read the data and push it into log_entries
    // file.seekg(0,std::ios::beg);
    int prev_ind = -1;
    while(!file.eof()) {
        command new_commend;
        char cur_tag = ' ';
        file.read(reinterpret_cast<char*>(&new_commend.id), sizeof(chfs_command::txid_t));
        file.read(&cur_tag, 1);

        int ind = 0;
        while(ind < 5) {
            if(cur_tag == tags[ind]) {
                break;
            }
            ++ind;
        }
        
        if(ind == chfs_command::cmd_type::CMD_COMMIT) {
            printf("read checkpoint type: commit! %d\n", ind);
            new_commend.type = chfs_command::cmd_type::CMD_COMMIT; 
            
        } else if(ind == chfs_command::cmd_type::CMD_CREATE) {
            new_commend.type = chfs_command::cmd_type::CMD_CREATE;
            // inode type
            file.read(reinterpret_cast<char*>(&new_commend.inode_type), sizeof(uint32_t));
            printf("read checkpoint type: create! %d \n", new_commend.inode_type);

        } else if(ind == chfs_command::cmd_type::CMD_PUT) {
            new_commend.type = chfs_command::cmd_type::CMD_PUT;
            file.read(reinterpret_cast<char*>(&new_commend.inum), sizeof(extent_protocol::extentid_t));
            uint32_t size = 0;
            // size
            file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            // input string
            new_commend.input_string.resize(size);
            printf("read checkpoint type: put! %s %d\n", new_commend.input_string.data(), size);
            file.read(const_cast<char*>(new_commend.input_string.data()), size);

        } else if(ind == chfs_command::cmd_type::CMD_REMOVE) {
            new_commend.type = chfs_command::cmd_type::CMD_REMOVE;
            file.read(reinterpret_cast<char*>(&new_commend.inum), sizeof(extent_protocol::extentid_t));
        } else {
            if(prev_ind == ind) {
                printf("read checkpoint type: multiple begin! %d\n", ind);
                prev_ind = ind;
            }
            printf("read checkpoint type: begin! %d\n", ind);
        }

        prev_ind = ind;
        log_entries.emplace_back(new_commend);
    }

};

template<typename command>
void persister<command>::clear() {
    std::ofstream clear_file(file_path_logfile, std::ios_base::out);
    clear_file.close();
    std::ofstream out_file(file_path_logfile, std::ios::out | std::ios::binary | std::ios::app);
    out_file.seekp(0, std::ios::end);
    int file_size = out_file.tellp();
    assert(file_size == 0);
}

using chfs_persister = persister<chfs_command>;

#endif // persister_h