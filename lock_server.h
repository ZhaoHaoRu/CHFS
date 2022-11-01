// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <unordered_map>
#include <mutex>
#include <pthread.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

 protected:
  int nacquire;
  pthread_mutex_t mtx;
  // key: lock_id  value: 0: nobody hold the lock 1: somebody hold the lock   
  std::unordered_map<lock_protocol::lockid_t, int> lock_map;
  // key: lock_id  value: condition variable
  std::unordered_map<lock_protocol::lockid_t, pthread_cond_t> cond_map;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 