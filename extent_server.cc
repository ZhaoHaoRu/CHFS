// the extent server implementation

#include <sstream>
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
  printf("get here!\n");

  _persister->restore_logdata();
  std::vector<chfs_command> logs = _persister->get_logs();
  printf("get here!!!\n");
  for(auto log_entry : logs) {
    if(log_entry.type == chfs_command::cmd_type::CMD_BEGIN || log_entry.type == chfs_command::cmd_type::CMD_COMMIT) {
      continue;
    } else if(log_entry.type == chfs_command::cmd_type::CMD_CREATE) {
      printf("redo create type: %d\n", log_entry.inode_type);
      create(log_entry.inode_type, log_entry.inum, true);
    } else if(log_entry.type == chfs_command::cmd_type::CMD_PUT) {
      // TODO: how here integer use?
      int tmp = 0;
      printf("redo put inum: %lld buf: %ld\n", log_entry.inum, log_entry.input_string.size());
      put(log_entry.inum, log_entry.input_string, tmp, true);
    }
  }

  // _persister->clear();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id, bool is_restart)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  // add to log
  printf("begin to create!\n");
  if(!is_restart) {
    ++tid;
    chfs_command new_commend(chfs_command::cmd_type::CMD_BEGIN, tid);
    _persister->append_log(new_commend);
    chfs_command create_commend(chfs_command::cmd_type::CMD_CREATE, tid, type);
    _persister->append_log(create_commend);
    printf("create type: %d\n", type);
  }

  id = im->alloc_inode(type);
  printf("created inum: %lld\n", id);

  if(!is_restart) {
    chfs_command commit_log(chfs_command::cmd_type::CMD_COMMIT, tid);
    _persister->append_log(commit_log);
  }
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &, bool is_restart)
{
  printf("extent_server: put %lld\n", id);
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
 
  if(!is_restart) {
    printf("redo put inum: %lld buf: %s\n", id, cbuf);
    ++tid;
    // add the log
    chfs_command new_commend(chfs_command::cmd_type::CMD_BEGIN, tid);
    _persister->append_log(new_commend);
    chfs_command put_log(chfs_command::cmd_type::CMD_PUT, tid, id, buf);
    _persister->append_log(put_log);
  }
  
  im->write_file(id, cbuf, size);

  if(!is_restart) {
    chfs_command commit_log(chfs_command::cmd_type::CMD_COMMIT, tid);
    _persister->append_log(commit_log);
  }
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

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
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->get_attr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
 
  return extent_protocol::OK;
}

// the extent server implementation

// #include <sstream>
// #include <vector>
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>

// #include "extent_server.h"
// #include "persister.h"

// extent_server::extent_server() 
// {
//   im = new inode_manager();
//   _persister = new chfs_persister("log"); // DO NOT change the dir name here
//   tid = 0;
//   // Your code here for Lab2A: recover data on startup
//   _persister->restore_logdata();
//   std::vector<chfs_command> logs = _persister->get_logs();

//   for(auto log_entry : logs) {
//     if(log_entry.type == chfs_command::cmd_type::CMD_BEGIN || log_entry.type == chfs_command::cmd_type::CMD_COMMIT) {
//       continue;
//     } else if(log_entry.type == chfs_command::cmd_type::CMD_CREATE) {
//       create(log_entry.inode_type, log_entry.inum);
//     } else if(log_entry.type == chfs_command::cmd_type::CMD_PUT) {
//       // TODO: how here integer use?
//       int tmp = 0;
//       put(log_entry.inum, log_entry.input_string, tmp);
//     }
//   }
// }

// int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
// {
//   // alloc a new inode and return inum
//   printf("extent_server: create inode\n");
//   ++tid;
//   // add to log
//   chfs_command new_commend(chfs_command::cmd_type::CMD_BEGIN, tid);
//   _persister->append_log(new_commend);
//   chfs_command create_commend(chfs_command::cmd_type::CMD_CREATE, tid, type);
//   _persister->append_log(create_commend);
//   id = im->alloc_inode(type);
//   chfs_command commit_log(chfs_command::cmd_type::CMD_COMMIT, tid);
//   _persister->append_log(commit_log);
//   return extent_protocol::OK;
// }

// int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
// {
//   id &= 0x7fffffff;
  
//   const char * cbuf = buf.c_str();
//   int size = buf.size();
//   printf("extent server the size of string want to write: %d", size);
//   ++tid;
//   // add the log
//   chfs_command new_commend(chfs_command::cmd_type::CMD_BEGIN, tid);
//   _persister->append_log(new_commend);
//   chfs_command put_log(chfs_command::cmd_type::CMD_CREATE, tid, id, buf);
//   _persister->append_log(put_log);
//   im->write_file(id, cbuf, size);
//   chfs_command commit_log(chfs_command::cmd_type::CMD_COMMIT, tid);
//   _persister->append_log(commit_log);
  
//   return extent_protocol::OK;
// }

// int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
// {
//   printf("extent_server: get %lld\n", id);

//   id &= 0x7fffffff;

//   int size = 0;
//   char *cbuf = NULL;

//   im->read_file(id, &cbuf, &size);
//   if (size == 0)
//     buf = "";
//   else {
//     buf.assign(cbuf, size);
//     free(cbuf);
//   }

//   return extent_protocol::OK;
// }

// int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
// {
//   printf("extent_server: getattr %lld\n", id);

//   id &= 0x7fffffff;
  
//   extent_protocol::attr attr;
//   memset(&attr, 0, sizeof(attr));
//   im->get_attr(id, attr);
//   a = attr;

//   return extent_protocol::OK;
// }

// int extent_server::remove(extent_protocol::extentid_t id, int &)
// {
//   printf("extent_server: write %lld\n", id);

//   id &= 0x7fffffff;
//   im->remove_file(id);
 
//   return extent_protocol::OK;
// }



