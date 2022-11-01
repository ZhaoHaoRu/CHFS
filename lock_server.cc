// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mtx,NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2B part2 code goes here
  pthread_mutex_lock(&mtx);

  // if a client asks for a lock that the server has never seen before,
  // the server should create the lock and grant it to the client
  if(!lock_map.count(lid)) {
    lock_map[lid] = 1;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    cond_map[lid] = cond;

  } else {
    // check the lock
    while(lock_map[lid]) {
      pthread_cond_wait(&cond_map[lid], &mtx);
    }
    lock_map[lid] = 1;
  }

  // maybe it is useless
  r = ret;

  pthread_mutex_unlock(&mtx);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2B part2 code goes here
  pthread_mutex_lock(&mtx);

  // the lid is invalid
  if(!lock_map.count(lid)) {
    return lock_protocol::IOERR;
  }

  lock_map[lid] = 0;
  pthread_cond_signal(&cond_map[lid]);

  r = ret;
  pthread_mutex_unlock(&mtx);

  return ret;
}