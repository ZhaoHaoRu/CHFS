#include "chfs_state_machine.h"

chfs_command_raft::chfs_command_raft() {
    // Lab3: Your code here
    cmd_tp = chfs_command_raft::command_type::CMD_NONE;
    type = 0;
    id = 0;
    value = 0;
    res = std::make_shared<result>();
    res->start = std::chrono::system_clock::now();
}

chfs_command_raft::chfs_command_raft(const chfs_command_raft &cmd) :
    cmd_tp(cmd.cmd_tp), type(cmd.type),  id(cmd.id), buf(cmd.buf), res(cmd.res), value(0) {
    // Lab3: Your code here
}
chfs_command_raft::~chfs_command_raft() {
    // Lab3: Your code here
}

int chfs_command_raft::size() const{ 
    // Lab3: Your code here
    int size = 0;
    size += sizeof(cmd_tp);
    size += sizeof(uint32_t);
    size += sizeof(extent_protocol::extentid_t);
    size += buf.size();
    size += sizeof(uint32_t);
    // size += sizeof(extent_protocol::attr);
    // don't use value

    return size;
}

void chfs_command_raft::serialize(char *buf_out, int size) const {
    // Lab3: Your code here
    int bias = 0;
    memcpy(buf_out + bias, &cmd_tp, sizeof(command_type));
    bias += sizeof(command_type);
    memcpy(buf_out + bias, &type, sizeof(uint32_t));
    bias += sizeof(uint32_t);
    memcpy(buf_out + bias, &id, sizeof(extent_protocol::extentid_t));
    bias += sizeof(extent_protocol::extentid_t);
    // the buf length
    uint32_t length = buf.size();
    memcpy(buf_out + bias, &length, sizeof(uint32_t));
    bias += sizeof(uint32_t);
    assert(bias + length <= size);
    memcpy(buf_out + bias, buf.c_str(), buf.size());
    bias += buf.size();
    return;
}

void chfs_command_raft::deserialize(const char *buf_in, int size) {
    // Lab3: Your code here
    int bias = 0;
    memcpy(&cmd_tp, buf_in, sizeof(command_type));
    bias += sizeof(command_type);
    memcpy(&type, buf_in + bias, sizeof(uint32_t));
    bias += sizeof(uint32_t);
    memcpy(&id, buf_in + bias, sizeof(extent_protocol::extentid_t));
    bias += sizeof(extent_protocol::extentid_t);
    uint32_t length = 0;
    memcpy(&length, buf_in + bias, sizeof(uint32_t));
    bias += sizeof(uint32_t);

    buf.resize(length, ' ');
    int i = 0;
    while (bias + i < size && i < length)
    {
        buf[i] = buf_in[bias + i];
        ++i;
    }
    return;
}

marshall &operator<<(marshall &m, const chfs_command_raft &cmd) {
    // Lab3: Your code here
    m << (int)cmd.cmd_tp << cmd.type << cmd.id << cmd.buf;
    return m;
}

unmarshall &operator>>(unmarshall &u, chfs_command_raft &cmd) {
    // Lab3: Your code here
    int temp = 0;
    u >> temp >> cmd.type >> cmd.id >> cmd.buf;
    cmd.cmd_tp = chfs_command_raft::command_type(temp);
    return u;
}

void chfs_state_machine::apply_log(raft_command &cmd) {
    try{
        chfs_command_raft &chfs_cmd = dynamic_cast<chfs_command_raft &>(cmd);
         // Lab3: Your code here
        std::unique_lock<std::mutex> lock(mtx);

        extent_protocol::status ret;
        int tmp = 0;
        switch (chfs_cmd.cmd_tp) {
            case chfs_command_raft::command_type::CMD_CRT:
                ret = es.create(chfs_cmd.type, chfs_cmd.res->id);
                // printf("create type: %d, the id: %lld\n", chfs_cmd.type, chfs_cmd.res->id);
                break;
            case chfs_command_raft::command_type::CMD_PUT:
                ret = es.put(chfs_cmd.id, chfs_cmd.buf, tmp);
                // printf("put id: %lld, the content: %s\n", chfs_cmd.id, chfs_cmd.buf.c_str());
                break;
            case chfs_command_raft::command_type::CMD_GET:
                ret = es.get(chfs_cmd.id, chfs_cmd.res->buf);
                // printf("get id: %lld, the content: %s\n", chfs_cmd.id, chfs_cmd.res->buf);
                break;
            case chfs_command_raft::command_type::CMD_GETA:
                ret = es.getattr(chfs_cmd.id, chfs_cmd.res->attr);
                assert(ret == extent_protocol::OK);
                // printf("get attr id: %lld, chfs_cmd.res->attr: %d\n", chfs_cmd.id, chfs_cmd.res->attr.type);
                break;
            case chfs_command_raft::command_type::CMD_RMV:
                ret = es.remove(chfs_cmd.id, tmp);
                // printf("remote id: %lld\n", chfs_cmd.id);
                break;
            default:
                break;
        }

        if(ret == extent_protocol::OK) {
            chfs_cmd.res->done = true;
        } else {
            chfs_cmd.res->done = false;
            printf("something wrong\n");
        }
    
        chfs_cmd.res->cv.notify_all();
    } catch(std::bad_cast b) {
        printf("caught: %s\n", b.what());
    }
    
   
    return;
}


