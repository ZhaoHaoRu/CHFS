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

#define RAFT_LOG(fmt, args...) \
    do {                       \
    } while (0);

    // #define RAFT_LOG(fmt, args...)                                                                                   \
//     do {                                                                                                         \
//         auto now =                                                                                               \
//             std::chrono::duration_cast<std::chrono::milliseconds>(                                               \
//                 std::chrono::system_clock::now().time_since_epoch())                                             \
//                 .count();                                                                                        \
//         printf("[%ld][%s:%d][node %d term %d] " fmt "\n", now, __FILE__, __LINE__, my_id, current_term, ##args); \
//     } while (0);

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

    // save a snapshot of the state machine and compact the log.
    bool save_snapshot();

private:
    std::mutex mtx; // A big lock to protect the whole data structure
    ThrPool *thread_pool;
    raft_storage<command> *storage; // To persist the raft log
    state_machine *state;           // The state machine that applies the raft log, e.g. a kv store

    rpcs *rpc_server;                // RPC server to recieve and handle the RPC requests
    std::vector<rpcc *> rpc_clients; // RPC clients of all raft nodes including this node
    int my_id;                       // The index of this node in rpc_clients, start from 0

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
    term = current_term;
    return true;
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::save_snapshot() {
    // Lab3: Your code here
    return true;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::init_follower() {
    role = follower;
    vote_for = -1;
    earned_votes.clear();
    next_index.clear();
    match_index.clear();
}

template <typename state_machine, typename command>
void raft<state_machine, command>::init_leader() {
    role = leader;
    earned_votes.clear();
    leader_id = my_id;
    vote_for = -1;
    next_index.resize(client_count, last_log_index + 1);
    match_index.resize(client_count, 0);
}

/******************************************************************

                         RPC Related

*******************************************************************/
// reply to the voting request
template <typename state_machine, typename command>
int raft<state_machine, command>::request_vote(request_vote_args args, request_vote_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);

    if(args.term < current_term) {
        reply.vote_granted = false;
        reply.term = current_term;
        return -1;
    }
    
    if(args.term > current_term) {
        current_term = args.term;
        vote_for = -1;
        reply.vote_granted = false;  // TODO: I am not sure
        if(role == leader || role == candidate) {
            // TODO: need to step down
        }
    }
    
    // is a reply message
    bool is_up_to_date_log = (args.last_log_term > last_term) || (args.last_log_term == last_term && args.last_log_index > last_log_index);
    reply.vote_granted = false;
    reply.term = current_term;
    if(vote_for == args.candidate_id || (vote_for == -1 && is_up_to_date_log)) {
        reply.vote_granted = true;
        vote_for = args.candidate_id;
        // update timeout
        clock = std::chrono::steady_clock::now();
    }

    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(int target, const request_vote_args &arg, const request_vote_reply &reply) {
    // Lab3: Your code here
    std::unique_lock<std::mutex> lock(mtx);

    // judge candiate or follower or leader
    // this server is expired
    if(arg.term < current_term || arg.term < reply.term) {
        init_follower();
        return;
    }

    if(reply.vote_granted) {
        earned_votes.insert(target);
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
    last_log_index = log[log.size() - 1].log_index;
    last_term = log[log.size() - 1].term;

    if(arg.term > current_term && is_leader(current_term)) {
        current_term = arg.term;
        init_follower();
    }

    reply.term = current_term;
    if(arg.term < current_term) {
        reply.success = false;
        return 0;
    }

    // Reply false if log doesn’t contain an entry at prevLogIndex
    // whose term matches prevLogTerm 
    if(arg.prev_log_index > last_log_index || arg.prev_log_term != last_term) {
        reply.success = false;
        return 0;
    }

    // If an existing entry conflicts with a new one (same index but different terms), 
    // delete the existing entry and all that follow it
    if(arg.prev_log_index < last_log_index) {
        int n = log.size() - 1;
        while(n >= 0) {
            if(log[n].log_index == arg.prev_log_index && log[n].term != arg.prev_log_term) {
                break;
            }
        }
        if(n >= 0) {
            log.resize(n);
        }
    }

    // Append any new entries not already in the log
    

    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(int node, const append_entries_args<command> &arg, const append_entries_reply &reply) {
    // Lab3: Your code here
    return;
}

template <typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(install_snapshot_args args, install_snapshot_reply &reply) {
    // Lab3: Your code here
    return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(int node, const install_snapshot_args &arg, const install_snapshot_reply &reply) {
    // Lab3: Your code here
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
        
        // get the timeout val
        std::random_device rd; 
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(150, 300);
        int timeout = dis(gen);

        // get the time passed
        auto cur_time = std::chrono::steady_clock::now();
        auto time_gap = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - clock);
        
        if(time_gap < timeout) {
            continue;
        }

        if(role == follower || role == candidate) {
            std::unique_lock<std::mutex> lock(mtx);
            role = candidate;
            ++current_term;
            vote_for = my_id;
            earned_votes.clear();
            earned_votes.insert(my_id);

            // generate the request
            request_vote_args args(current_term, my_id, last_log_index, last_term);
            lock.unlock();

            for(int i = 0; i < client_count; ++i) {
                if(i == my_id) {
                    continue;
                }
                thread_pool->addObjJob(this, &raft::send_request_vote, i, args);
            }
        }
    }    

    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_commit() {
    // Periodly send logs to the follower.

    // Only work for the leader.

    /*
        while (true) {
            if (is_stopped()) return;
            // Lab3: Your code here
        }    
        */

    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {
    // Periodly apply committed logs the state machine

    // Work for all the nodes.

    /*
    while (true) {
        if (is_stopped()) return;
        // Lab3: Your code here:
    }    
    */
    return;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {
    // Periodly send empty append_entries RPC to the followers.

    // Only work for the leader.

    /*
    while (true) {
        if (is_stopped()) return;
        // Lab3: Your code here:
    }    
    */

    return;
}

/******************************************************************

                        Other functions

*******************************************************************/

#endif // raft_h