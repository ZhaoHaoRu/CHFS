// the extent server implementation

#include <sstream>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "extent_server.h"
#include "persister.h"

extent_server::extent_server() 
{
  im = new inode_manager();
  _persister = new chfs_persister("log"); // DO NOT change the dir name here
  tid = 0;
  // Your code here for Lab2A: recover data on startup

  _persister->restore_checkpoint();
  _persister->restore_logdata();
  std::vector<chfs_command> logs = _persister->get_logs();
  
  for(auto log_entry : logs) {
    if(log_entry.type == chfs_command::cmd_type::CMD_BEGIN || log_entry.type == chfs_command::cmd_type::CMD_COMMIT) {
      continue;
    } else if(log_entry.type == chfs_command::cmd_type::CMD_CREATE) {
      // printf("redo create type: %d\n", log_entry.inode_type);
      create(log_entry.inode_type, log_entry.inum, true);
    } else if(log_entry.type == chfs_command::cmd_type::CMD_PUT) {
      // TODO: how here integer use?
      int tmp = 0;
      // printf("redo put inum: %lld buf: %s\n", log_entry.inum, log_entry.input_string.c_str());
      put(log_entry.inum, log_entry.input_string, tmp, true);
    }
  }
  // log_restart();
  // _persister->clear();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id, bool is_restart)
{
  // alloc a new inode and return inum
  // printf("extent_server: create inode\n");
  // add to log
  // printf("begin to create!\n");
  
  if(!is_restart) {
    chfs_command create_commend(chfs_command::cmd_type::CMD_CREATE, tid, type);
    _persister->append_log(create_commend);
    // printf("create type: %d\n", type);
  }

  id = im->alloc_inode(type);
  // printf("created inum: %lld\n", id);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &, bool is_restart)
{
  // printf("extent_server: put %lld\n", id);
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
 
  // add log
  if(!is_restart) {
    // printf("for redo put inum: %lld buf: %s\n", id, cbuf);
    chfs_command put_log(chfs_command::cmd_type::CMD_PUT, tid, id, buf);
    _persister->append_log(put_log);
  }
  
  im->write_file(id, cbuf, size);

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->get_attr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &, bool is_restart)
{
  // printf("extent_server: write %lld\n", id);

  // add the log
  if(!is_restart) {
    printf("for remove: %lld\n", id);
    chfs_command remove_log(chfs_command::cmd_type::CMD_REMOVE, id);
    _persister->append_log(remove_log);
  }

  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}

int extent_server::begin_log() {
  ++tid;
  chfs_command new_commend(chfs_command::cmd_type::CMD_BEGIN, tid);
  _persister->append_log(new_commend);

  return extent_protocol::OK;
}

int extent_server::commit_log() {
  chfs_command commit_log(chfs_command::cmd_type::CMD_COMMIT, tid);
  _persister->append_log(commit_log);

  return extent_protocol::OK;
}

void extent_server::create_log(uint32_t type) {
  chfs_command create_commend(chfs_command::cmd_type::CMD_CREATE, tid, type);
  _persister->append_log(create_commend);
  // printf("create type: %d\n", type);
}

void extent_server::put_log(extent_protocol::extentid_t id, std::string buf) {
  // printf("put log inum: %lld buf: %s\n", id, buf.c_str());
  chfs_command put_log(chfs_command::cmd_type::CMD_PUT, tid, id, buf);
  _persister->append_log(put_log);
}

void extent_server::log_restart() {
  // printf("----------------begin restart--------------------\n");
  // _persister->restore_checkpoint();
  _persister->restore_logdata();

  std::vector<chfs_command> logs = _persister->get_logs();
  std::vector<chfs_command> cur_logs;

  // restore logs
  for(auto log_entry : logs) {
    if(log_entry.type == chfs_command::cmd_type::CMD_BEGIN) {
      // printf("get begin!\n");
      cur_logs.clear();
      continue;

    } else if(log_entry.type == chfs_command::cmd_type::CMD_COMMIT) {
      // printf("get commit!\n");
      // all or nothing: the transaction can redo now
      for(auto entry : cur_logs) {
        if(entry.type == chfs_command::cmd_type::CMD_CREATE) {
          // printf("redo create type: %d\n", entry.inode_type);
          create(entry.inode_type, entry.inum, true);

        } else if(entry.type == chfs_command::cmd_type::CMD_PUT) {
          // TODO: how here integer use?
          int tmp = 0;
          // printf("redo put inum: %lld buf: %s\n", entry.inum, entry.input_string.c_str());
          put(entry.inum, entry.input_string, tmp, true);
        
        } 
        // else if(entry.type == chfs_command::cmd_type::CMD_REMOVE) {
        //   printf("redo remove the inode: %lld\n", entry.inum);
        //   int tmp = 0;
        //   remove(entry.inum, tmp, true);
        // }
      }

      cur_logs.clear();

    } else if(log_entry.type == chfs_command::cmd_type::CMD_CREATE || log_entry.type == chfs_command::cmd_type::CMD_PUT) {
      
      if(log_entry.type == chfs_command::cmd_type::CMD_PUT) {
        // printf("not redo put inum: %lld buf: %s\n", log_entry.inum, log_entry.input_string.c_str());
      }

      cur_logs.emplace_back(log_entry);
    }  
  }
}


