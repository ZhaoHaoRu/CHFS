// chfs client.  implements FS operations using extent and lock server
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "chfs_client.h"
#include "extent_client.h"

/* 
 * Your code here for Lab2A:
 * Here we treat each ChFS operation(especially write operation such as 'create', 
 * 'write' and 'symlink') as a transaction, your job is to use write ahead log 
 * to achive all-or-nothing for these transactions.
 */

chfs_client::chfs_client()
{
    ec = new extent_client();

}

chfs_client::chfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

chfs_client::inum
chfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
chfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
chfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
chfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        return true;
    } 
    return false;
}

bool
chfs_client::issymlink(inum inum) {
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMBOLIC) {
        printf("isfile: %lld is a symbolic link!\n", inum);
        return true;
    } 
    return false;
}

int
chfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
chfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
// Your code here for Lab2A: add logging to ensure atomicity
int
chfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    if(ec->get(ino, buf) != extent_protocol::OK) {
        return IOERR;
    }
    size_t prev_size = buf.size();
    if(prev_size == size) {
        return r;
    } else if(prev_size < size) {
        buf = buf.substr(0, size);
    } else {
        // using '\0' to pad
        buf.resize(size, '\0');
    }
    if(ec->put(ino, buf) != extent_protocol::OK) {
        return IOERR;
    }
    return r;
}

int chfs_client::check_dir_inode(inum parent, extent_protocol::attr& a) {
    int r = OK;
     // the information stored in inode
    if(ec->getattr(parent, a) != extent_protocol::OK) {
        return IOERR;
    }
    // check whether it is a dir
    if(a.type != extent_protocol::T_DIR) {
        return IOERR;
    }
    return r;
}

// Your code here for Lab2A: add logging to ensure atomicity
int
chfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    extent_protocol::attr a;
    // find and check the parent parent
    if(check_dir_inode(parent, a) != OK) {
        return IOERR;
    }
    bool found = false;
    // check whether the file already exist
    if(lookup(parent, name, found, ino_out) != OK) {
        return IOERR;
    }
    if(found) {
        return EXIST;
    }
    // create a new file
    if(ec->create(extent_protocol::T_FILE, ino_out) != OK) {
        return IOERR;
    } 
    // Create an empty extent for ino.(how to do? I don't know)
    // Add a <name, ino> entry into @parent.
    // my format for directories: inum/name/inum/name...inum/name/
    std::string buf;
    if(ec->get(parent, buf) != extent_protocol::OK) {
        return IOERR;
    }
    std::string ino_str = filename(ino_out);
    std::string inum_name_pair = ino_str + "/" + name + "/";
    buf += inum_name_pair;
    if(ec->put(parent, buf) != extent_protocol::OK) {
        return IOERR;
    }
    return r;
}

// Your code here for Lab2A: add logging to ensure atomicity
int
chfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    extent_protocol::attr a;
    // find and check the parent parent
    if(check_dir_inode(parent, a) != OK) {
        printf("the parent inode is illegal!\n");
        return IOERR;
    }
    bool found = false;
    // The new directory should be empty (no . or ..)
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("the file name is illegal!\n");
        return IOERR;
    }
    // check whether the file already exist
    if(lookup(parent, name, found, ino_out) != OK) {
        printf("look up the file error!\n");
        return IOERR;
    }
    if(found) {
        printf("the file already exist!\n");
        return EXIST;
    }
    // create a new directory
    if(ec->create(extent_protocol::T_DIR, ino_out) != OK) {
        printf("create directory error!\n");
        return IOERR;
    } 
    // Create an empty extent for ino.(how to do? I don't know)
    // Add a <name, ino> entry into @parent.
    // my format for directories: inum/name/inum/name...inum/name/
    std::string buf;
    if(ec->get(parent, buf) != extent_protocol::OK) {
        printf("get the parent inode info error!\n");
        return IOERR;
    }
    std::string ino_str = filename(ino_out);
    std::string inum_name_pair = ino_str + "/" + name + "/";
    buf += inum_name_pair;
    if(ec->put(parent, buf) != extent_protocol::OK) {
        printf("put the parent inode info error!\n");
        return IOERR;
    }
    return r;
}

int
chfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    // get the parent directory 
    extent_protocol::attr a;
    if(check_dir_inode(parent, a) != OK) {
        return IOERR;
    }
    std::list<dirent> dirent_list;
    if(readdir(parent, dirent_list) != OK) {
        return IOERR;   // readdir error
    }
    for(auto elem : dirent_list) {
        if(elem.name == name) {    
            found = true;
            ino_out = elem.inum;
            break;
        }
    }
    return r;
}

int
chfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    /*
    * my format for directories: inum/name/inum/name...inum/name/
    */
    extent_protocol::attr a;
    // get and check the dir inode
    if(check_dir_inode(dir, a) != OK) {
        return IOERR;
    }
    // get the content of the directory
    std::string buf;
    if(ec->get(dir, buf) != extent_protocol::OK) {
        return IOERR;
    }
    int length = buf.length();
    int pos = 0;
    while(pos < length) {
        std::string::size_type slash_pos = buf.find('/', pos);
        if(slash_pos == std::string::npos) {
            printf("don't have correct inum!\n");
            return IOERR;
        }
        std::string inum_str = buf.substr(pos, slash_pos - pos);
        pos = slash_pos + 1;
        if(pos >= length) {
            printf("inum and name unpaired!\n");
            return IOERR;
        }
        slash_pos = buf.find('/', pos);
        if(slash_pos == std::string::npos) {
            printf("don't have correct name!\n");
            return IOERR;
        }
        std::string name_str = buf.substr(pos, slash_pos - pos);
        pos = slash_pos + 1;
        dirent new_dirent;
        new_dirent.inum = n2i(inum_str);
        new_dirent.name = name_str;
        list.emplace_back(new_dirent);
    }
    return r;
}

int
chfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    // verify inode is valid
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        return IOERR;
    }
    std::string buf;
    if(ec->get(ino, buf) != extent_protocol::OK) {
        return IOERR;
    }
    size_t length = buf.size();
    if((long int)length <= off) {
        data = {};
    } else if(off + size >= length) {
        data = buf.substr(off, length - off);
    } else {
        data = buf.substr(off, size);
    }
    return r;
}

// Your code here for Lab2A: add logging to ensure atomicity
int
chfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    extent_protocol::attr a;
    if (ec->getattr(ino, a) != extent_protocol::OK) {
        return IOERR;
    }
    std::string prev_buf, next_buf;
    next_buf.assign(data, size);
    if(ec->get(ino, prev_buf) != extent_protocol::OK) {
        return IOERR;
    }
    // handle the data string to write
    bytes_written = size;
    if(off > (long int)prev_buf.length()) {
        prev_buf.resize(off);
    } 
    prev_buf.replace(off, size, next_buf);
    if(ec->put(ino, prev_buf) != extent_protocol::OK) {
        printf("extent server put error!\n");
        return IOERR;
    }
    return r;
}

// Your code here for Lab2A: add logging to ensure atomicity
int chfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    // check whether parent is a valid directory
    extent_protocol::attr a;
    if(check_dir_inode(parent, a) != extent_protocol::OK) {
        return IOERR;
    }
    // find the file with this name
    bool found = false;
    inum file_ino = 0;
    if(lookup(parent, name, found, file_ino) != extent_protocol::OK) {
        return IOERR;
    }
    if(found == false) {
        printf("don't have such file!\n");
        return NOENT;
    } 
    // check whether it is a valid file name
    if(!isfile(file_ino)) {
        printf("the inum is not for a file!\n");
        return IOERR;
    }
    // free the file's extent
    if(ec->remove(file_ino) != extent_protocol::OK) {
        printf("extent client remove error!\n");
        return IOERR;
    }
    // update the parent directory content.
    std::list<dirent> list;
    if(readdir(parent, list) != extent_protocol::OK) {
        return IOERR;
    } 
    std::string new_buf;
    for (std::list<dirent>::iterator it = list.begin(); it != list.end();)
    {
        if (name == (*it).name)
            list.erase(it++);
        else{
            dirent cur = *it;
            std::string ino_str = filename(cur.inum);
            std::string inum_name_pair = ino_str + "/" + cur.name + "/";
            new_buf += inum_name_pair;
            ++it;
        }
    }
    if(ec->put(parent, new_buf) != extent_protocol::OK) {
        printf("update parent directory error!\n");
        return IOERR;
    }
    return r;
}

int chfs_client::create_symbolic_link(inum parent,const char *true_file_path, const char *link_name, inum &ino) {
    int r = OK;
    extent_protocol::attr a;
    // find and check the parent parent
    if(check_dir_inode(parent, a) != OK) {
        printf("the parent inode not valid!\n");
        return IOERR;
    }
    bool found = false;
    // check whether the symbol link already exist
    if(lookup(parent, link_name, found, ino) != OK) {
        printf("look up operation fail!\n");
        return IOERR;
    }
    if(found) {
        printf("the symbolic link already exist!\n");
        return EXIST;
    }
    // create a new symbolic link
    if(ec->create(extent_protocol::T_SYMBOLIC, ino) != OK) {
        printf("create symbolic link error!\n");
        return IOERR;
    } 
    std::string buf;
    if(ec->get(parent, buf) != extent_protocol::OK) {
        printf("get the parent inode content error!\n");
        return IOERR;
    }
    std::string ino_str = filename(ino);
    std::string inum_name_pair = ino_str + "/" + link_name + "/";
    buf += inum_name_pair;
    // set the symbolic value 
    if(ec->put(ino, std::string(true_file_path)) != extent_protocol::OK) {
        printf("set the symbolic value error!\n");
    }
    if(ec->put(parent, buf) != extent_protocol::OK) {
        printf("rewrite the parent inode fail!\n");
        return IOERR;
    }
    return r;
}

int chfs_client::parse_symbolic_link(std::string &buf, inum ino) {
    int r = OK;

    // verify the inode is valid
    if(!issymlink(ino)) {
        printf("the inode type wrong!\n");
        return IOERR;
    }
    // get the true path name
    std::string data;
    if(ec->get(ino, data) != extent_protocol::OK) {
        printf("get the symbolic link content error!\n");
        return IOERR;
    }
    buf = data;
    // parse the path name (don't need!)
    return r;
}