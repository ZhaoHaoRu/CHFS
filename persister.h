// #ifndef persister_h
// #define persister_h

// #include <fcntl.h>
// #include <mutex>
// #include <iostream>
// #include <fstream>
// #include "rpc.h"

// #define MAX_LOG_SZ 1024

// /*
//  * Your code here for Lab2A:
//  * Implement class chfs_command, you may need to add command types such as
//  * 'create', 'put' here to represent different commands a transaction requires. 
//  * 
//  * Here are some tips:
//  * 1. each transaction in ChFS consists of several chfs_commands.
//  * 2. each transaction in ChFS MUST contain a BEGIN command and a COMMIT command.
//  * 3. each chfs_commands contains transaction ID, command type, and other information.
//  * 4. you can treat a chfs_command as a log entry.
//  */
// class chfs_command {
// public:
//     typedef unsigned long long txid_t;
//     enum cmd_type {
//         CMD_BEGIN = 0,
//         CMD_COMMIT

//     };

//     cmd_type type = CMD_BEGIN;
//     txid_t id = 0;

//     // constructor
//     chfs_command() {}

//     uint64_t size() const {
//         uint64_t s = sizeof(cmd_type) + sizeof(txid_t);
//         return s;
//     }
// };

// /*
//  * Your code here for Lab2A:
//  * Implement class persister. A persister directly interacts with log files.
//  * Remember it should not contain any transaction logic, its only job is to 
//  * persist and recover data.
//  * 
//  * P.S. When and how to do checkpoint is up to you. Just keep your logfile size
//  *      under MAX_LOG_SZ and checkpoint file size under DISK_SIZE.
//  */
// template<typename command>
// class persister {

// public:
//     persister(const std::string& file_dir);
//     ~persister();

//     // persist data into solid binary file
//     // You may modify parameters in these functions
//     void append_log(const command& log);
//     void checkpoint();

//     // restore data from solid binary file
//     // You may modify parameters in these functions
//     void restore_logdata();
//     void restore_checkpoint();

// private:
//     std::mutex mtx;
//     std::string file_dir;
//     std::string file_path_checkpoint;
//     std::string file_path_logfile;

//     // restored log data
//     std::vector<command> log_entries;
// };

// template<typename command>
// persister<command>::persister(const std::string& dir){
//     // DO NOT change the file names here
//     file_dir = dir;
//     file_path_checkpoint = file_dir + "/checkpoint.bin";
//     file_path_logfile = file_dir + "/logdata.bin";
// }

// template<typename command>
// persister<command>::~persister() {
//     // Your code here for lab2A

// }

// template<typename command>
// void persister<command>::append_log(const command& log) {
//     // Your code here for lab2A

// }

// template<typename command>
// void persister<command>::checkpoint() {
//     // Your code here for lab2A

// }

// template<typename command>
// void persister<command>::restore_logdata() {
//     // Your code here for lab2A

// };

// template<typename command>
// void persister<command>::restore_checkpoint() {
//     // Your code here for lab2A

// };

// using chfs_persister = persister<chfs_command>;

// #endif // persister_h


#ifndef persister_h
#define persister_h

#include <fcntl.h>
#include <string.h>
#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include "rpc.h"

#define MAX_LOG_SZ 1024

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
const char tags[4] = {'B', 'C', 'R', 'P'};

class chfs_command {
public:
    typedef unsigned long long txid_t;
    enum cmd_type {
        CMD_BEGIN = 0,
        CMD_COMMIT,
        CMD_CREATE,
        CMD_PUT
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
    void restore_logdata();
    void restore_checkpoint();
    std::vector<command> get_logs();

    void clear();

private:
    std::mutex mtx;
    std::string file_dir;
    std::string file_path_checkpoint;
    std::string file_path_logfile;

    // restored log data
    std::vector<command> log_entries;
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
void persister<command>::append_log(const command& log) {
    // Your code here for lab2A
    chfs_command::cmd_type type = log.type;

    // begin:TID + tag 
    int bias = 0;
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
    }

    out_file.close();
}

template<typename command>
void persister<command>::checkpoint() {
    // Your code here for lab2A

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
        while(ind < 4) {
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
        }

        log_entries.emplace_back(new_commend);
    }

};

template<typename command>
std::vector<command> persister<command>::get_logs() {
    return log_entries;
};

template<typename command>
void persister<command>::restore_checkpoint() {
    // Your code here for lab2A

};

template<typename command>
void persister<command>::clear() {
    std::ofstream clear_file(file_path_logfile, std::ios_base::out);
}

using chfs_persister = persister<chfs_command>;

#endif // persister_h