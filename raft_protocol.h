#ifndef raft_protocol_h
#define raft_protocol_h

#include "rpc.h"
#include "raft_state_machine.h"

enum raft_rpc_opcodes {
    op_request_vote = 0x1212,
    op_append_entries = 0x3434,
    op_install_snapshot = 0x5656
};

enum raft_rpc_status {
    OK,
    RETRY,
    RPCERR,
    NOENT,
    IOERR
};

class request_vote_args {
public:
    // Lab3: Your code here
    int term{0};
    int candidate_id{0};
    int last_log_index{0};
    int last_log_term{0};

    request_vote_args(int t, int cid, int lli, int llt): term(t), 
        candidate_id(cid), last_log_index(lli), last_log_term(llt) {}
};

marshall &operator<<(marshall &m, const request_vote_args &args);
unmarshall &operator>>(unmarshall &u, request_vote_args &args);

class request_vote_reply {
public:
    // Lab3: Your code here
    int term{0};
    bool vote_granted{false};

    request_vote_reply(int t, bool granted): term(t), vote_granted(granted) {}
};

marshall &operator<<(marshall &m, const request_vote_reply &reply);
unmarshall &operator>>(unmarshall &u, request_vote_reply &reply);

template <typename command>
class log_entry {
public:
    // Lab3: Your code here
    command cmd;
    int log_index{0};
    int term{0};

    log_entry(command c, int log_ind, int t): cmd(c), log_index(log_ind), term(t) {}
};

template <typename command>
marshall &operator<<(marshall &m, const log_entry<command> &entry) {
    // Lab3: Your code here
    m = m << entry.cmd;
    m = m << entry.log_index;
    m = m << entry.term;
    return m;
}

template <typename command>
unmarshall &operator>>(unmarshall &u, log_entry<command> &entry) {
    // Lab3: Your code here
    u = u >> entry.cmd;
    u = u >> entry.log_index;
    u = u >> entry.term;
    return u;
}

template <typename command>
class append_entries_args {
public:
    // Your code here
    int term{0};
    int leader_id{0};
    // 紧接在新条目之前的日志条目索引
    int prev_log_index{0};
    // term of prevLogIndex entry
    int prev_log_term{0};
    // log entries to store (empty for heartbeat; may send more than one for efficiency)
    std::vector<log_entry<command>> entries;
    // leader commit index;
    int leader_commit{0};

    append_entries_args(int t, int lid, int prev_log_ind, int prev_log_tm ,std::vector<log_entry<command>> &cmds, int lcmt):
        term(t), leader_id(lid), prev_log_index(prev_log_ind), prev_log_term(prev_log_tm), entries(cmds), leader_commit(lcmt) {}
};

template <typename command>
marshall &operator<<(marshall &m, const append_entries_args<command> &args) {
    // Lab3: Your code here
    m = m << args.term;
    m = m << args.leader_id;
    m = m << args.prev_log_index;
    m = m << args.prev_log_term;

    for(auto entry : args.entries) {
        m = m << entry;
    }
    m = m << args.leader_commit;
    return m;
}

template <typename command>
unmarshall &operator>>(unmarshall &u, append_entries_args<command> &args) {
    // Lab3: Your code here
    u = u >> args.term;
    u = u >> args.leader_id;
    u = u >> args.prev_log_index;
    u = u >> args.prev_log_term;
    u = u >> args.entries;
    u = u >> args.leader_commit;
    return u;
}

class append_entries_reply {
public:
    // Lab3: Your code here
    // currentTerm, for leader to update itself
    int term{0};
    // true if follower contained entry matching prevLogIndex and prevLogTerm
    bool success{false};

    append_entries_reply(int t, bool succ): term(t), success(succ) {}
};

marshall &operator<<(marshall &m, const append_entries_reply &reply);
unmarshall &operator>>(unmarshall &m, append_entries_reply &reply);

class install_snapshot_args {
public:
    // Lab3: Your code here
};

marshall &operator<<(marshall &m, const install_snapshot_args &args);
unmarshall &operator>>(unmarshall &m, install_snapshot_args &args);

class install_snapshot_reply {
public:
    // Lab3: Your code here
};

marshall &operator<<(marshall &m, const install_snapshot_reply &reply);
unmarshall &operator>>(unmarshall &m, install_snapshot_reply &reply);

#endif // raft_protocol_h