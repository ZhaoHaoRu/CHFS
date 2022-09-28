#ifndef chfs_client_h
#define chfs_client_h

#include <string>
//#include "chfs_protocol.h"
#include "extent_client.h"
#include <vector>


class chfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    chfs_client::inum inum;
  };
  struct dirent_n {
    char name[BLOCK_SIZE/2];
    chfs_client::inum inum;
    size_t len;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

 public:
  chfs_client();
  chfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  
  /** you may need to add symbolic link related methods here.*/
  int symlink(inum parent, const char* name, const char* link, inum& ino_out);
  int readlink(inum ino, std::string &data);
};

#endif 