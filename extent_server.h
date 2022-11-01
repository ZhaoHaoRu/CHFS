// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

#include "inode_manager.h"
#include "persister.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  chfs_persister *_persister;
  // add tid
  chfs_command::txid_t tid;

 public:
  extent_server();

  // for lab2A
  // int create(uint32_t type, extent_protocol::extentid_t &id, bool is_restart = false);
  // int put(extent_protocol::extentid_t id, std::string, int &, bool is_restart = false);
  // int remove(extent_protocol::extentid_t id, int &, bool is_restart = false);
  int create(uint32_t type, extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string, int &);
  int remove(extent_protocol::extentid_t id, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  

  // TODO: Your code here for lab2A: add logging APIs
  int begin_log();
  int commit_log();
  void create_log(uint32_t type);
  void put_log(extent_protocol::extentid_t id, std::string);
  
  ///@brief read the log when restart
  void log_restart();
};

#endif 







