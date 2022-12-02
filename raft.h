#ifndef raft_h
#define raft_h

#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include <unordered_set>
#include <algorithm>
#include <thread>
#include <random>
#include <stdarg.h>

#include "rpc.h"
#include "raft_storage.h"
#include "raft_protocol.h"
#include "raft_state_machine.h"

template <typename state_machine, typename command>
class raft {
    static_assert(std::is_base_of<raft_state_machine, state_machine>(), "state_machine must inherit from raft_state_machine");
    static_assert(std::is_base_of<raft_command, command>(), "command must inherit from raft_command");

    friend class thread_pool;

// #define RAFT_LOG(fmt, args...) \
//     do {                       \
//     } while (0);

#define RAFT_LOG(fmt, args...)                                                                                   \
do {                                                                                                         \
    auto now =                                                                                               \
        std::chrono::duration_cast<std::chrono::milliseconds>(                                               \
            std::chrono::system_clock::now().time_since_epoch())                                             \
            .count();                                                                                        \
    printf("[%ld][%s:%d][node %d term %d] " fmt "\n", now, __FILE__, __LINE__, my_id, current_term, ##args); \
} while (0);

public:
    raft(
        rpcs *rpc_server,
        std::vector<rpcc *> rpc_clients,
        int idx,
        raft_storage<command> *storage,
        state_machine *state);
    ~raft();

    // start the raft node.
    // Please make sure all of the rpc request handlers have been registered before this method.
    void start();

    // stop the raft node.
    // Please make sure all of the background threads are joined in this method.
    // Notice: you should check whether is server should be stopped by calling is_stopped().
    //         Once it returns true, you should break all of your long-running loops in the background threads.
    void stop();

    // send a new command to the raft nodes.
    // This method returns true if this raft node is the leader that successfully appends the log.
    // If this node is not the leader, returns false.
    bool new_command(command cmd, int &term, int &index);

    // returns whether this node is the leader, you should also set the current term;
    bool is_leader(int &term);

    bool save_snapshot();
    // save a snapshot of the state machine and compact the log.
    bool handle_snapshot(int last_included_index, int last_included_term, std::string data, int data_size);

    int my_id;                       // The index of this node in rpc_clients, start from 0

private:
    std::mutex mtx; // A big lock to protect the whole data structure
    ThrPool *thread_pool;
    raft_storage<command> *storage; // To persist the raft log, it is just a tool
    state_machine *state;           // The state machine that applies the raft log, e.g. a kv store

    rpcs *rpc_server;                // RPC server to recieve and handle the RPC requests
    std::vector<rpcc *> rpc_clients; // RPC clients of all raft nodes including this node


    std::atomic_bool stopped;

    enum raft_role {
        follower,
        candidate,
        leader
    };
    raft_role role;
    int current_term;
    int leader_id;

    std::thread *background_election;
    std::thread *background_ping;
    std::thread *background_commit;
    std::thread *background_apply;

    // Your code here:
    int vote_for{-1};
    // the votes earned from all clients
    std::unordered_set<int> earned_votes; 
    // thw clock for time-out
    std::chrono::steady_clock::time_point clock{std::chrono::steady_clock::now()};
    // the local last term index
    int last_term{0};
    // the local last log index
    int last_log_index{0};
    // the index of highest log entry known to be committed
    int commit_index{0};
    // the majority log index from previous terms
    int prev_commit_index{0}; 
    // index of highest log entry applied to state machine 
    int last_applied{0};
    int client_count;
    // for each server, index of the next log entry to send to that server (initialized to leader
    // last log index + 1)
    std::vector<int> next_index;
    // for each server, index of highest log entry known to be replicated on server
    // (initialized to 0, increases monotonically
    std::vector<int> match_index;
    // log entries; each entry contains command for state machine, and term when entry was received by leader (first index is 1)
    std::vector<log_entry<command>> log;
    // the snashot data
    std::string snapshot_data;
    /* ----Persistent state on all server----  */

    /* ---- Volatile state on all server----  */

    /* ---- Volatile state on leader----  */

private:
    // RPC handlers
    int request_vote(request_vote_args arg, request_vote_reply &reply);

    int append_entries(append_entries_args<command> arg, append_entries_reply &reply);

    int install_snapshot(install_snapshot_args arg, install_snapshot_reply &reply);

    // RPC helpers
    void send_request_vote(int target, request_vote_args arg);
    void handle_request_vote_reply(int target, const request_vote_args &arg, const request_vote_reply &reply);

    void send_append_entries(int target, append_entries_args<command> arg);
    void handle_append_entries_reply(int target, const append_entries_args<command> &arg, const append_entries_reply &reply);

    void send_install_snapshot(int target, install_snapshot_args arg);
    void handle_install_snapshot_reply(int target, const install_snapshot_args &arg, const install_snapshot_reply &reply);

private:
    bool is_stopped();
    int num_nodes() {
        return rpc_clients.size();
    }

    // background workers
    void run_background_ping();
    void run_background_election();
    void run_background_commit();
    void run_background_apply();

    // Your code here:
    void init_follower();
    void init_leader();
    void send_heartbeat();
    // after get magority same index, can commit
    int get_majority_same_index();
    int get_true_index(int index);
};

template <typename state_machine, typename command>
raft<state_machine, command>::raft(rpcs *server, std::vector<rpcc *> clients, int idx, raft_storage<command> *storage, state_machine *state) :
    stopped(false),
    rpc_server(server),
    rpc_clients(clients),
    my_id(idx),
    storage(storage),
    state(state),
    background_election(nullptr),
    background_ping(nullptr),
    background_commit(nullptr),
    background_apply(nullptr),
    current_term(0),
    role(follower) {
    thread_pool = new ThrPool(32);

    // Register the rpcs.
    rpc_server->reg(raft_rpc_opcodes::op_request_vote, this, &raft::request_vote);
    rpc_server->reg(raft_rpc_opcodes::op_append_entries, this, &raft::append_entries);
    rpc_server->reg(raft_rpc_opcodes::op_install_snapshot, this, &raft::install_snapshot);

    // Your code here:
    // Do the initialization
    client_count = rpc_clients.size();

    // restore from the log
    storage->restore(log);
    storage->restore_metadata(current_term, vote_for);

    // restore the snapshot
    int data_size = 0, last_included_index = 0, last_included_term = 0;
    std::string str;
    storage->restore_snapshot(last_included_index, last_included_term, str, data_size);

    // add the dummy log if empty
    if(log.empty()) {
        RAFT_LOG("dummy!");
        command dummy_command;
        log_entry<command> new_entry(dummy_command, 0, 0);
        log.emplace_back(new_entry);
        // assume the dummy command don't need to persist
    }

    commit_index = 0;
    last_applied = 0;

    // if have snapshot
    if(data_size != 0) {
        RAFT_LOG("the data size: %d", data_size);
        commit_index = last_included_index;
        last_applied = last_included_index;
        log[0].log_index = last_included_index;
        log[0].term = last_included_term;
        RAFT_LOG("the str length: ", str.size());
        std::vector<char> apply_data(str.begin(), str.end());
        state->apply_snapshot(apply_data);
    }
    
    int upper_bound = 10000000;

    if(vote_for > upper_bound) {
        vote_for = -1;
    }

    RAFT_LOG("after restore, the current term: %d, vote_for: %d, commit_index: %d, the prev_term: %d", current_term, vote_for, commit_index, log[log.size() - 1].term);

    
}

template <typename state_machine, typename command>
raft<state_machine, command>::~raft() {
    if (background_ping) {
        delete background_ping;
    }
    if (background_election) {
        delete background_election;
    }
    if (background_commit) {
        delete background_commit;
    }
    if (background_apply) {
        delete background_apply;
    }
    delete thread_pool;
}

/******************************************************************

                        Public Interfaces

*******************************************************************/

template <typename state_machine, typename command>
void raft<state_machine, command>::stop() {
    stopped.store(true);
    background_ping->join();
    background_election->join();
    background_commit->join();
    background_apply->join();
    thread_pool->destroy();
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::is_stopped() {
    return stopped.load();
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::is_leader(int &term) {
    term = current_term;
    return role == leader;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::start() {
    // Lab3: Your code here


    RAFT_LOG("start");
    this->background_election = new std::thread(&raft::run_background_election, this);
    this->background_ping = new std::thread(&raft::run_background_ping, this);
    this->background_commit = new std::thread(&raft::run_background_commit, this);
    this->background_apply = new std::thread(&raft::run_background_apply, this);
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::new_command(command cmd, int &term, int &index) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    if(role != leader) {
        return false;
    }

    // append new command to the leader's log
    term = current_term; 
    index = log[log.size() - 1].log_index + 1;   // the log is not growing and growing 
    // index = log.size();     // the log is growing and growing
    log_entry<command> new_entry(cmd, index, term);
    log.emplace_back(new_entry);
    RAFT_LOG("new command persist log, the log size: %ld", log.size());
    storage->persist(log);

    return true;
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::save_snapshot() {
    std::unique_lock<std::mutex> lock(mtx);
    // get the lastapplied log index and term
    int last_applied_index = last_applied - log[0].log_index;
    int last_included_index = log[last_applied_index].log_index;
    int last_included_term = log[last_applied_index].term;

    log[0].log_index = last_included_index;
    log[0].term = last_included_term;
    
    auto it = log.begin() + last_applied_index + 1;
    log.assign(it, log.end());

    // persist
    std::vector<char> data = state->snapshot();
    snapshot_data.assign(data.begin(), data.end());
    storage->persist(log);
    storage->persist_snapshot(last_included_index, last_included_term, snapshot_data, snapshot_data.size());
    return true;
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::handle_snapshot(int last_included_index, int last_included_term, std::string data, int data_size) {
    // Lab3: Your code here
    // if don't use
    if(last_included_index <= log[0].log_index || last_included_term <= log[0].term) {
        return false;
    }
    // discard any existing or partial snapshot with a smaller index
    int n = log.size();
    std::vector<log_entry<command>> new_log;
    // the dummy log always reserve
    new_log.emplace_back(log[0]);
    int i = 1;

    while(log[i].log_index <= last_included_index && i < n) {
        ++i;
    }
    while(i < n) {
        new_log.emplace_back(log[i]);
        ++i;
    }
    log = new_log;

    // reuse the dummy log entry
    log[0].log_index = last_included_index;
    log[0].term = last_included_term;

    commit_index = std::max(commit_index, last_included_index);
    last_applied = std::max(last_applied, last_included_index);

    // save snapshot file
    data_size = data.size();
    RAFT_LOG("the data size in line 370: %d", data_size);
    storage->persist_snapshot(last_included_index, last_included_term, data, data_size);
    storage->persist(log);
    // reset the state machine
    std::vector<char> str_arr(data_size);
    assert(data_size == data.size());
    str_arr.assign(data.begin(), data.end());
    state->apply_snapshot(str_arr);
    return true;
}


/******************************************************************

                         RPC Related

*******************************************************************/
// reply to the voting request
template <typename state_machine, typename command>
int raft<state_machine, command>::request_vote(request_vote_args args, request_vote_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    
    // update last_log_index and last_log_term
    if(!log.empty()) {
        last_log_index = log[log.size() - 1].log_index;
        last_term = log[log.size() - 1].term;
    }

    RAFT_LOG("begin to handle request vote from %d, the args.term %d, the current term: %d, the last_log_index: %d, args.last_log_index: %d", args.candidate_id, args.term, current_term, last_log_index, args.last_log_index);

    if(args.term < current_term) {
        RAFT_LOG("the args.term of %d: %d, the current_term of %d: %d", args.candidate_id, args.term, my_id, current_term);
        reply.vote_granted = false;
        reply.term = current_term;
        return 0;
    }
    
    if(args.term > current_term) {
        current_term = args.term;
        reply.vote_granted = false;  
        init_follower();
    }
    
    // is a reply message
    bool is_up_to_date_log = (args.last_log_term > last_term) || (args.last_log_term == last_term && args.last_log_index >= last_log_index);
    reply.vote_granted = false;
    reply.term = current_term;

    if(role != follower) {
        return 0;
    }

    RAFT_LOG("the vote for: %d, the  is_updatelog: %d", vote_for, is_up_to_date_log);
    if(vote_for == args.candidate_id || (vote_for == -1 && is_up_to_date_log)) {
        reply.vote_granted = true;
        vote_for = args.candidate_id;
        // persist the meta data
        storage->persist_metadata(current_term, vote_for);
        // update timeout
        clock = std::chrono::steady_clock::now();
    }

    RAFT_LOG("the vote request result for %d: %d", args.candidate_id, reply.vote_granted);
    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(int target, const request_vote_args &arg, const request_vote_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);

    // judge candiate or follower or leader
    // this server is expired
    if(arg.term < current_term || arg.term < reply.term) {
        RAFT_LOG("the target: %d, the arg.term: %d, the current_term: %d, the reply.term: %d", target, arg.term, current_term, reply.term);
        init_follower();
        return;
    }

    RAFT_LOG("%d receive the vote from the client: %d", arg.candidate_id, target);
    // here maybe the vote from the remain minority server
    if(reply.vote_granted && role != leader) {
        earned_votes.insert(target);
        RAFT_LOG("the current earned vote count: %ld, the client size: %ld", earned_votes.size(), rpc_clients.size());

        if(earned_votes.size() >= rpc_clients.size() / 2 + 1) {
            init_leader();
        }
    }
    return;
}


template <typename state_machine, typename command>
int raft<state_machine, command>::append_entries(append_entries_args<command> arg, append_entries_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    
    if(arg.term > current_term) {
        RAFT_LOG("init follower: the current term: %d, the arg.term: %d", current_term, arg.term);
        current_term = arg.term;
        init_follower();    // votefor = -1
    }

    reply.term = current_term;
    last_log_index = log[log.size() - 1].log_index;
    last_term = log[log.size() - 1].term;

    // check whether it is heartbeat
    if(arg.entries.empty()) {
        reply.success = true;
        reply.term = -1;

        ///@note the second condition is important to avoid update commit index by mistake
        if(arg.leader_commit > commit_index && (last_log_index == arg.prev_log_index && last_term == arg.prev_log_term)) {  
            RAFT_LOG("change commit index of %d through heartbeat, arg.leader_commit: %d, log[log.size() - 1].log_index: %d", my_id, arg.leader_commit, log[log.size() - 1].log_index);
            commit_index = std::min(arg.leader_commit, log[log.size() - 1].log_index);
        }

        // update the leader if necessary
        if(leader_id == my_id && leader_id != arg.leader_id) {
            leader_id = arg.leader_id;
            init_follower();
        }
        
        RAFT_LOG("get heartbeat from leader %d", leader_id);
        clock = std::chrono::steady_clock::now();
        return 0;
    }

    if(arg.term < current_term) {
        reply.success = false;
        return 0;
    }

    clock = std::chrono::steady_clock::now();
    
    RAFT_LOG("receive append entries from %d, the arg.term: %d, the sender is: %d", arg.leader_id, arg.term, arg.leader_id);

    // Reply false if log doesn’t contain an entry at prevLogIndex
    // whose term matches prevLogTerm 
    RAFT_LOG("the arg.prev_log_index: %d, last_log_index: %d, arg.prev_log_term: %d, last_term: %d", arg.prev_log_index, last_log_index, arg.prev_log_term, last_term);
    
    int this_prev_log_term = log[get_true_index(arg.prev_log_index)].term; 
    if(arg.prev_log_index > last_log_index || arg.prev_log_term != this_prev_log_term) {
        reply.success = false;
        RAFT_LOG("log doesn’t contain an entry at prevLogIndex");
        return 0;
    }

    // If an existing entry conflicts with a new one (same index but different terms), 
    // delete the existing entry and all that follow it
    if(arg.prev_log_index < last_log_index) {
        int n = log.size() - 1;
        while(n >= 0 && log[n].log_index != arg.prev_log_index) {
            --n;
        }
        RAFT_LOG("the log's size after resize: %d\n", n);
        if(n >= 0) {
            log.resize(n + 1);
        }
    }

    // Append any new entries not already in the log
    for(auto log_entry : arg.entries) {
        RAFT_LOG(" append_entries push into log: the index %d, the value: %d", log_entry.log_index, log_entry.cmd.value);
        log.emplace_back(log_entry);
    }
    RAFT_LOG("persist the log, the log size: %d", log.size());
    storage->persist(log);   // apply to local storage

    last_log_index = log[log.size() - 1].log_index;
    // If leaderCommit > commitIndex, set commitIndex = min(leaderCommit, index of last new entry)
    RAFT_LOG("the arg.leader_commit: %d, commit_index: %d", arg.leader_commit, commit_index);
    if(arg.leader_commit > commit_index) {
        commit_index = std::min(arg.leader_commit, last_log_index);
    }

    reply.success = true;

    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(int node, const append_entries_args<command> &arg, const append_entries_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    assert(node >= 0 && node < client_count);

    RAFT_LOG("handle append entries update clock");
    clock = std::chrono::steady_clock::now();

    // if reply.term > arg.term, leader or candidate should step down
    if(reply.term > arg.term) {
        RAFT_LOG("init follower: the reply.term: %d, the arg.term: %d", reply.term, arg.term);
        init_follower();
        return;
    }

    // check whether it is heartbeat
    if(reply.term == -1) {
        return;
    }   
    
    if(role != leader || current_term != arg.term) {
        return;
    }

    // If successful: update nextIndex and matchIndex for follower
    if(reply.success) {
        match_index[node] = arg.entries[arg.entries.size() - 1].log_index;
        next_index[node] = std::max(match_index[node] + 1, next_index[node]); 
        RAFT_LOG("success from client %d, now the match_index: %d, the next_index: %d", node, match_index[node], next_index[node]);
        // judge majority index (must for current term)
        int majority_index = get_majority_same_index();
        int index_pos = get_true_index(majority_index);
        RAFT_LOG("the majority index: %d, log[majority_index].term: %d, commit_index: %d", majority_index, log[index_pos].term, commit_index);
        if(log[index_pos].term == current_term && majority_index > commit_index) {
            commit_index = majority_index;
        }
    } else {
        // If AppendEntries fails because of log inconsistency: decrement nextIndex and retry
        RAFT_LOG("failure from node %d!", node);
        int prev_index = get_true_index(arg.prev_log_index);
        while(prev_index > 0 && log[prev_index].term == arg.prev_log_term) {
            --prev_index;
        }
        next_index[node] = log[prev_index + 1].log_index;
    }
    return;
}

template <typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(install_snapshot_args args, install_snapshot_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    // Reply immediately if term < currentTerm
    if(args.term < current_term) {
        reply.term = current_term;
        return 0;
    } else {
        current_term = args.term;
        reply.term = current_term;
        bool ret = handle_snapshot(args.last_included_index, args.last_included_term, args.data, args.data_size);
        if(!ret) {
            RAFT_LOG("something wrong happened when install snapshot");
        }
    }
    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(int node, const install_snapshot_args &arg, const install_snapshot_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);
    if(reply.term > current_term) {
        current_term = reply.term;
        init_follower();
    } else {
        next_index[node] = arg.last_included_index + 1;
        match_index[node] = arg.last_included_index;
    }
    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_request_vote(int target, request_vote_args arg) {
    request_vote_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_request_vote, arg, reply) == 0) {
        handle_request_vote_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_append_entries(int target, append_entries_args<command> arg) {
    append_entries_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_append_entries, arg, reply) == 0) {
        handle_append_entries_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_install_snapshot(int target, install_snapshot_args arg) {
    install_snapshot_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_install_snapshot, arg, reply) == 0) {
        handle_install_snapshot_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

/******************************************************************

                        Background Workers

*******************************************************************/

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_election() {
    // Periodly check the liveness of the leader.
    
    // Work for followers and candidates.


    while (true) {
        if (is_stopped()) return;
        // Lab3: Your code here
        if (role == leader) {
            continue;
        }
        auto cur_time = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::unique_lock<std::mutex> lock(mtx);
        // get the timeout val
        std::random_device rd; 
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 1500);
        int timeout = dis(gen);

        // get the time passed
        auto time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - clock);
        
        if(time_gap.count() <= timeout) {
            continue;
        }

        RAFT_LOG("the follower %d begin a new election, the time gap: %ld, timeout: %d, my pre role: %d, the former leader: %d", my_id, time_gap.count(), timeout, role, leader_id);

        if(role == follower || role == candidate) {
            role = candidate;
            RAFT_LOG("the current role: %d", role);
            ++current_term;
            vote_for = my_id;
            // persist the meta data
            storage->persist_metadata(current_term, vote_for);
            earned_votes.clear();
            earned_votes.insert(my_id);

            // update last log index and last term
            int n = log.size();
            if(n != 0) {
                last_log_index = log[n - 1].log_index;
                last_term = log[n - 1].term;
            }

            // generate the request
            request_vote_args args(current_term, my_id, last_log_index, last_term);

            for(int i = 0; i < client_count; ++i) {
                if(i == my_id) {
                    continue;
                }
                thread_pool->addObjJob(this, &raft::send_request_vote, i, args);
            }
        }
        lock.unlock();
    }    

    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_commit() {
    // Periodly send logs to the follower.

    // Only work for the leader.
 
    while (true) {
        if (is_stopped()) return;
        // Lab3: Your code here
        std::unique_lock<std::mutex> lock(mtx);

        if(role == follower || role == candidate) continue;

        append_entries_args<command> args;
        args.term = current_term;
        args.leader_id = leader_id;
        args.leader_commit = commit_index;

        if(match_index.size() < client_count) {
            RAFT_LOG("the wrong leader: %d, the match_index size: %ld, the client_count: %d, the role: %d", my_id, match_index.size(), client_count, role);
            exit(0);
        }

        for(int i = 0; i < client_count; ++i) {
            if(match_index[i] - log[0].log_index >= log.size() - 1) {
                continue;
            }
            
            // should update the leader's match_index and next_index too
            if(i == my_id) {
                match_index[i] = log[log.size() - 1].log_index;
                next_index[i] = match_index[i] + 1;
                continue;
            }

            // if the follower is too far behind, send snapshot
            if(next_index[i] < log[0].log_index + 1) {
                install_snapshot_args args;
                args.term = current_term;
                args.leader_id = my_id;
                args.last_included_index = log[0].log_index;
                args.last_included_term = log[0].term;
                args.data = snapshot_data;
                args.data_size = args.data.size();
                thread_pool->addObjJob(this, &raft::send_install_snapshot, i, args);
                continue;
            }

            args.prev_log_index = next_index[i] - 1;
            args.prev_log_term = log[get_true_index(args.prev_log_index)].term;
            RAFT_LOG("the receiver: %d, the args.prev_log_index: %d, the args.prev_log_term: %d", i, args.prev_log_index, args.prev_log_term);
           

            ///@note should be the logs after the the prev_log_index
            RAFT_LOG("the to_commit_logs bias: %d", get_true_index(args.prev_log_index) + 1);
            std::vector<log_entry<command>> to_commit_logs(get_true_index(args.prev_log_index) + log.begin() + 1, log.end());
            args.entries = to_commit_logs;
            thread_pool->addObjJob(this, &raft::send_append_entries, i, args);
        }

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }    

    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {
    // Periodly apply committed logs the state machine

    // Work for all the nodes.

    while (true) {
        if (is_stopped()) return;
        // Lab3: Your code here:
        // If commitIndex > lastApplied: increment lastApplied, 
        // apply log[lastApplied] to state machine
        std::unique_lock<std::mutex> lock(mtx);

        while(commit_index > last_applied) {
            ++last_applied;
            RAFT_LOG("the client %d apply the log index: %d the term: %d, the apply value: %d", my_id, log[get_true_index(last_applied)].log_index, log[get_true_index(last_applied)].term, log[get_true_index(last_applied)].cmd.value);
            state->apply_log(log[get_true_index(last_applied)].cmd);
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } 
      
    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {
    // Periodly send empty append_entries RPC to the followers.

    // Only work for the leader.

    while (true) {
        if (is_stopped()) return;
        if (role != leader) continue;
        
        std::unique_lock<std::mutex> lock(mtx);
        send_heartbeat();
        lock.unlock();
        // wait shorter to avoid race
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        // Lab3: Your code here:
    }    


    return;
}

/******************************************************************

                        Other functions

*******************************************************************/
template <typename state_machine, typename command>
void raft<state_machine, command>::init_follower() {
    RAFT_LOG("become the follower: %d", my_id);
    role = follower;
    vote_for = -1;
    earned_votes.clear();
    next_index.clear();
    match_index.clear();

    storage->persist_metadata(current_term, vote_for);
}

template <typename state_machine, typename command>
void raft<state_machine, command>::init_leader() {
    RAFT_LOG("the id %d become leader", my_id);
    role = leader;
    earned_votes.clear();
    leader_id = my_id;
    next_index.resize(client_count, last_log_index + 1);
    match_index.resize(client_count, 0);
    
    // need to send heartbeat immediately
    send_heartbeat();
}

template <typename state_machine, typename command>
int raft<state_machine, command>::get_majority_same_index() {
    std::vector<int> raw_match_index(match_index.begin(), match_index.end());
    std::sort(raw_match_index.begin(), raw_match_index.end());
    
    int idx = raw_match_index.size() / 2;
    return raw_match_index[idx];
}


template <typename state_machine, typename command>
void raft<state_machine, command>::send_heartbeat() {
    std::vector<log_entry<command>> empty_entries;
    last_log_index = log[log.size() - 1].log_index;
    last_term = log[log.size() - 1].term;
    append_entries_args<command> heartbeat_args(current_term, leader_id, last_log_index, last_term, empty_entries, commit_index);
    
    // it should remind the follower to change the commit index
    for(int i = 0; i < client_count; ++i) {
        if(i == my_id) {
            continue;
        }
        heartbeat_args.leader_commit = commit_index;
        thread_pool->addObjJob(this, &raft::send_append_entries, i, heartbeat_args);
    }
}

template <typename state_machine, typename command>
int raft<state_machine, command>::get_true_index(int index) {
    assert(index >= log[0].log_index);
    int true_index = index - log[0].log_index;
    return true_index;
}

#endif // raft_h