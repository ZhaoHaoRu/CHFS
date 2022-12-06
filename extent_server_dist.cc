#include "extent_server_dist.h"

chfs_raft *extent_server_dist::leader() const {
    int leader = this->raft_group->check_exact_one_leader();
    if (leader < 0) {
        return this->raft_group->nodes[0];
    } else {
        return this->raft_group->nodes[leader];
    }
}

int extent_server_dist::create(uint32_t type, extent_protocol::extentid_t &id) {
    // Lab3: your code here
    chfs_raft *leader = this->leader();

    chfs_command_raft cmd;

    cmd.cmd_tp = chfs_command_raft::command_type::CMD_CRT;
    cmd.type = type;

    int term, index;
    int size = cmd.size();
    char *buf_out = new char[size];
    cmd.serialize(buf_out, size);
    leader->new_command(cmd, term, index);

    std::unique_lock<std::mutex> lock(cmd.res->mtx);
    if(!cmd.res->done) {
        ASSERT(cmd.res->cv.wait_until(
            lock, std::chrono::system_clock::now() + std::chrono::milliseconds(10000)) == std::cv_status::no_timeout,
         "extent_server_dist::create command timeout");
    }

    // TODO: I am not sure
    id = cmd.res->id;
    lock.unlock();
    delete []buf_out;

    return extent_protocol::OK;
}

int extent_server_dist::put(extent_protocol::extentid_t id, std::string buf, int &) {
    // Lab3: your code here
    chfs_raft *leader = this->leader();

    chfs_command_raft cmd;
    cmd.cmd_tp = chfs_command_raft::command_type::CMD_PUT;
    cmd.id = id;
    cmd.buf = buf;

    int term, index;
    int size = cmd.size();
    char *buf_out = new char[size];
    printf("the put buffer size: %d", size);
    cmd.serialize(buf_out, size);
    leader->new_command(cmd, term, index);

    std::unique_lock<std::mutex> lock(cmd.res->mtx);
    if(!cmd.res->done) {
        ASSERT(cmd.res->cv.wait_until(
            lock, std::chrono::system_clock::now() + std::chrono::milliseconds(10000)) == std::cv_status::no_timeout,
         "extent_server_dist::put command timeout");
    }
    printf("put down");
    lock.unlock();
    delete []buf_out;

    return extent_protocol::OK;
}

int extent_server_dist::get(extent_protocol::extentid_t id, std::string &buf) {
    // Lab3: your code here
    chfs_raft *leader = this->leader();
    chfs_command_raft cmd;
    cmd.cmd_tp = chfs_command_raft::command_type::CMD_GET;
    cmd.id = id;

    int term, index;
    int size = cmd.size();
    char *buf_out = new char[size];
    cmd.serialize(buf_out, size);
    leader->new_command(cmd, term, index);

    std::unique_lock<std::mutex> lock(cmd.res->mtx);
    if(!cmd.res->done) {
        ASSERT(cmd.res->cv.wait_until(
            lock, std::chrono::system_clock::now() + std::chrono::milliseconds(10000)) == std::cv_status::no_timeout,
         "extent_server_dist::get command timeout");
    }

    buf = cmd.res->buf;
    lock.unlock();
    delete []buf_out;
    return extent_protocol::OK;
}

int extent_server_dist::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a) {
    // Lab3: your code here
    chfs_raft *leader = this->leader();
    chfs_command_raft cmd;
    cmd.cmd_tp = chfs_command_raft::command_type::CMD_GETA;
    cmd.id = id;

    int term, index;
    int size = cmd.size();
    char *buf_out = new char[size];
    cmd.serialize(buf_out, size);
    leader->new_command(cmd, term, index);

    std::unique_lock<std::mutex> lock(cmd.res->mtx);
    if(!cmd.res->done) {
        ASSERT(cmd.res->cv.wait_until(
            lock, std::chrono::system_clock::now() + std::chrono::milliseconds(10000)) == std::cv_status::no_timeout,
         "extent_server_dist::getattr command timeout");
    }

    a = cmd.res->attr;
    lock.unlock();
    delete []buf_out;
    return extent_protocol::OK;
}

int extent_server_dist::remove(extent_protocol::extentid_t id, int &) {
    // Lab3: your code here
    chfs_raft *leader = this->leader();
    chfs_command_raft cmd;
    cmd.cmd_tp = chfs_command_raft::command_type::CMD_RMV;
    cmd.id = id;

    int term, index;
    int size = cmd.size();
    printf("the remove buffer size: %d", size);
    char *buf_out = new char[size];
    cmd.serialize(buf_out, size);
    leader->new_command(cmd, term, index);

    std::unique_lock<std::mutex> lock(cmd.res->mtx);
    if(!cmd.res->done) {
        ASSERT(cmd.res->cv.wait_until(
            lock, std::chrono::system_clock::now() + std::chrono::milliseconds(10000)) == std::cv_status::no_timeout,
         "extent_server_dist::remove command timeout");
    }

    lock.unlock();
    delete []buf_out;
    return extent_protocol::OK;
}

extent_server_dist::~extent_server_dist() {
    delete this->raft_group;
}