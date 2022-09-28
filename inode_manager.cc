#include "inode_manager.h"
#include <cstring>
#include <algorithm>
#include <exception> 
#include <memory>
#include <cstdlib>
#include <time.h>
#include <assert.h>

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if(id >= BLOCK_NUM) {
    return;
  }
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  // judge the range
  if(id >= BLOCK_NUM) {
    return;
  }
  // TODO: it is not safe
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// judge whether it is a valid and using block
 bool block_manager::is_valid_block(uint32_t id) {
   if(id >= BLOCK_NUM) {
     return false;
   }
   return using_blocks[id] == 1;
 }

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  uint32_t last_inode_block = IBLOCK(INODE_NUM, sb.nblocks);
  // // printf("alloc block begin: %d\n", last_inode_block + 1);
  for(int i = last_inode_block + 1; i < BLOCK_NUM; ++i) {
    if(!using_blocks.count(i) || using_blocks[i] == 0 ) {
      using_blocks[i] = 1;
      // // printf("alloc new block, its id is %d\t", i);
      return i;
    }
  }
  // an error occur
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  if(id >= BLOCK_NUM)
    return;
  //// printf("free: %d ", id);
  char buf[BLOCK_SIZE] = {0};
  write_block(id, buf);
  assert(using_blocks[id] == 1);
  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
  // printf("behave read from block %d: %s\n", id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  // printf("behave write into block %d: %s\n", id, buf);
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  _last_alloced = 0;
  if (root_dir != 1) {
    // printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* get the indirect block which point to other blocks */
uint32_t* inode_manager::get_indirect_blocks(struct inode *node, int &block_count) {
  char *buf = new char[BLOCK_SIZE];
  bm->read_block(node->blocks[NDIRECT], buf);
  block_count = strlen(buf) / sizeof(uint32_t);
  uint32_t *blocks = (uint32_t *)buf;
  return blocks;
}

void inode_manager::free_indirect_blocks(struct inode *node, int begin_block) {
  if(node == nullptr) {
    return;
  }
  int block_count = node->size >= BLOCK_SIZE * NDIRECT ? (node->size - 1) / BLOCK_SIZE + 1 - NDIRECT : 0;
  // printf("block count: %d begin_block: %d\n", block_count, begin_block);
  char *blocks = new char[BLOCK_SIZE];
  bm->read_block(node->blocks[NDIRECT], blocks);
  for(int i = begin_block; i < block_count; ++i) {
    // printf("free block number: %d\t", *((uint32_t *)blocks + i));
    bm->free_block(*((uint32_t *)blocks + i));
    *((uint32_t *)blocks + i) = 0;
  }
  if(begin_block == 0) {
    bm->free_block(node->blocks[NDIRECT]);
    node->blocks[NDIRECT] = 0;
  } else {
    bm->write_block(node->blocks[NDIRECT], blocks);
    // printf("resave the indirect block %d after free!\n", node->blocks[NDIRECT]);
  }
  delete []blocks;
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  // create the new block
  // printf("alloc type: %d\n", type);
  struct inode *new_node = new inode();
  bzero(new_node, sizeof(struct inode));
  new_node->atime = new_node->ctime = new_node->mtime = time(NULL);
  new_node->type = type;
  memset(new_node->blocks, 0, sizeof(blockid_t)*(NDIRECT+1));

  // create the root_dir
  // if(type == extent_protocol::T_DIR && _last_alloced == 0) {
  //   put_inode(1, new_node);
  //   printf("create a root dir!!!\n");
  //   _last_alloced = 1;
  //   return 1;
  // }
  for(int i = 1; i <= INODE_NUM; ++i) {
    uint32_t to_alloc = i + _last_alloced >= INODE_NUM ? (i + _last_alloced) % INODE_NUM + 1 : i + _last_alloced;
    if(get_inode(to_alloc) == nullptr) {
      // printf("alloc new inode: %d type: %d %d\n", to_alloc, new_node.get()->type, type);
      if(to_alloc == 1) {
        printf("create a root inode!\n");
      }
      put_inode(to_alloc, new_node);
      _last_alloced = to_alloc;
      delete new_node;
      return to_alloc;
    }
  }
  // an error occur
  delete new_node;
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  // remember to write back to disk.
  // find the inode
  if(inum > INODE_NUM) {
    return;
  }
  struct inode *node = get_inode(inum);
  // the inode is already a freed one
  if(node == nullptr) {
    return;
  } 
  // clear it, and write back to disk.
  char buf[BLOCK_SIZE] = {0};
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
  delete node;
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  if(inum > INODE_NUM)
    return nullptr;
  struct inode *ino, *ino_disk;
  /* 
   * your code goes here.
   */
  char buf[BLOCK_SIZE] = {0};
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino = (struct inode*)buf + inum % IPB;
  if(ino->type == 0) {
    return nullptr;
  }
  ino_disk = new inode_t();
  *ino_disk = *ino;
  // printf("%d get inode info: %d %d %d %d %d\n", inum, ino->type, ino->size, ino->atime, ino->mtime, ino->ctime);
  return ino_disk;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  // printf("\tim: put_inode %d %d %d\n", inum, ino->type, ino->size);
  if (ino == NULL) 
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  if(inum > INODE_NUM) {
    *size = 0;
    return;
  }
  // printf("read from inode %d, the size is %d \n", inum, *size);
  struct inode *node = get_inode(inum);
  // varify the node
  if(node == nullptr) {
    return;
  }
  //!!! first alloc space for buf_out
  int block_count = node->size / BLOCK_SIZE;
  if(node->size % BLOCK_SIZE != 0 || block_count == 0) {
    ++block_count;
  }
  int remain_size = node->size % BLOCK_SIZE;
  // printf("malloc size: %d\n", block_count * BLOCK_SIZE);
  *buf_out = (char *)malloc(block_count * BLOCK_SIZE);
  // *buf_out = (char *)malloc(node->size);
  *size = node->size;
  int i = 0;
  for(;i < NDIRECT; ++i) {
    uint32_t block_id;
    block_id = node->blocks[i];
    // printf("the inode %d the block id %d %d\n", inum, i, block_id);
    if(bm->is_valid_block(block_id)) {
      char buf[BLOCK_SIZE] = {0};
      bm->read_block(block_id, buf);
      // printf("read from direct block%d, the data is %s\t", node->blocks[i], buf);
      // printf("read direct block %d %d ", i, block_id);
      // add to buf_out
      if(i == (block_count - 1) && remain_size) {
        memcpy(*buf_out + i * BLOCK_SIZE, buf, remain_size);
        break;
      }
      memcpy(*buf_out + i * BLOCK_SIZE, buf, BLOCK_SIZE);
    } 
  }
  if(i == NDIRECT && bm->is_valid_block(node->blocks[NDIRECT])) {
    // printf("read the inode %d , and the indirect block id: %d\n", inum, node->blocks[NDIRECT]); 
    char indirect_blocks[BLOCK_SIZE] = {0};
    bm->read_block(node->blocks[NDIRECT], indirect_blocks);
    // uint32_t *indirect_blocks = get_indirect_blocks(node, block_count);
    // printf("read the inode %d indirect_block count: %d\n", inum, block_count);
    for(i = 0; i < block_count - NDIRECT; ++i) {
      // printf("read the inode %d indirect_block id: %d\t", inum, *((uint32_t *)indirect_blocks + i));
      if(bm->is_valid_block(*((uint32_t *)indirect_blocks + i))) {
        char indirect_buf[BLOCK_SIZE] = {0};
        bm->read_block(*((uint32_t *)indirect_blocks + i), indirect_buf);
        // strcat(*buf_out, indirect_buf);
        if(i + NDIRECT == block_count - 1 && remain_size) {
          memcpy(*buf_out + (i + NDIRECT) * BLOCK_SIZE, indirect_buf, remain_size);
          break;
        }
        // if(i == 0) {
        //   printf("READ: %d inode the first indirect block %d char 0: %c\n", inum, *((uint32_t *)indirect_blocks + i), indirect_buf[0]);
        //   printf("READ: %d inode the first indirect block %d char 1: %c\n", inum, *((uint32_t *)indirect_blocks + i), indirect_buf[0]);
        // }        
        memcpy(*buf_out + (i + NDIRECT) * BLOCK_SIZE, indirect_buf, BLOCK_SIZE);
      }
    }
    // delete []indirect_blocks;
  }
  node->atime = time(NULL);
  put_inode(inum, node);
  // printf("read file end %d!\n", inum);
  // if(inum > 10) {
  //   int j = 1;
  // }
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  // printf("put into inode %d, the size is %d\n", inum, size);
  struct inode *node = get_inode(inum);
  if(node == nullptr) {
    // printf("the inode %d don't exist\n", inum);
    return;
  }
  int pre_block_num = node->size == 0 ? 1 : (node->size - 1) / BLOCK_SIZE + 1;
  // !!! there maybe '\0' in buf !!!
  // int length = strlen(buf);
  // size = std::min(size, length);
  char arr_for_remain[BLOCK_SIZE] = {0};
  int i = 0, pos = 0;
  // handle direct blocks
  while(pos < size && i < NDIRECT) {
    if(!bm->is_valid_block(node->blocks[i])) {
      // has to alloc new
      uint32_t new_block_id = bm->alloc_block();
      node->blocks[i] = new_block_id;
      // printf("write direct block count: %d, block id: %d\t", i, new_block_id);
    }
    if(pos + BLOCK_SIZE > size) {
      memcpy(arr_for_remain, buf + pos, size - pos);
      bm->write_block(node->blocks[i], arr_for_remain);
    }
    bm->write_block(node->blocks[i], buf + pos);
    pos += BLOCK_SIZE;
    ++i;
  }
  // handle indirect blocks
  if(pos < size && i == NDIRECT) {
    char indirect_blocks[BLOCK_SIZE] = {0};
    if(bm->is_valid_block(node->blocks[NDIRECT])) {
      bm->read_block(node->blocks[NDIRECT], indirect_blocks);
      // int block_count = 0;
      // indirect_blocks = get_indirect_blocks(node, block_count);
    } else {
      uint32_t new_block_id = bm->alloc_block();
      node->blocks[NDIRECT] = new_block_id;
      // printf("alloc new indirect super block: %d\n", new_block_id);
    }
    // printf("write the %d inode the indirect block id: %d  the begin pos: %d the total size: %d\n", inum, node->blocks[NDIRECT], pos, size);
    i = 0;
    int indirect_block_count = BLOCK_SIZE / sizeof(uint32_t);
    while(pos < size && i < indirect_block_count) {
      if(!bm->is_valid_block(*((uint32_t *)indirect_blocks + i))) {
        // has alloc new
        uint32_t new_block_id = bm->alloc_block();
        *((uint32_t *)indirect_blocks + i) = new_block_id;
      }
      if(pos + BLOCK_SIZE > size) {
        memcpy(arr_for_remain, buf + pos, size - pos);
        bm->write_block(*((uint32_t *)indirect_blocks + i), arr_for_remain);
        break;
      }
      // if(i <= 3) {
      //   //// printf("WRITE: %d inode the first first indirect block %d %d %c\n", inum, *((uint32_t *)indirect_blocks + i), i, (buf + pos)[0]);
      // }
      bm->write_block(*((uint32_t *)indirect_blocks + i), buf + pos);
      pos += BLOCK_SIZE;
      ++i;
    }
    // change the indirect block
    bm->write_block(node->blocks[NDIRECT], indirect_blocks);
  }
  // free block if needed
  // int used_block_num = size % BLOCK_SIZE == 0 ? size / BLOCK_SIZE : size / BLOCK_SIZE + 1;
  int used_block_num = (size - 1) / BLOCK_SIZE + 1;
  i = used_block_num;
  while(i < NDIRECT && i < pre_block_num) {
    // // printf("to free block id: %d, block id: %d\t", i, node->blocks[i]);
    if(bm->is_valid_block(node->blocks[i])) {
      bm->free_block(node->blocks[i]);
      node->blocks[i] = 0;
      ++i;
    } else {
      break;
    }
  }
  // free the indirect block
  if(i >= NDIRECT && pre_block_num > NDIRECT) {
    free_indirect_blocks(node, i - NDIRECT);
    if(i - NDIRECT == 0) {
      node->blocks[NDIRECT] = 0;
    }
  }
  // change the inode information and save 
  node->size = size;
  node->atime = time(NULL);
  node->ctime = time(NULL);
  node->mtime = time(NULL);
  put_inode(inum, node);
  return;
}

void
inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  if(inum > INODE_NUM)
    return;
  struct inode* node = get_inode(inum);
  if(node == nullptr)
    return;
  a.type = node->type;
  a.atime = node->atime;
  a.ctime = node->ctime;
  a.mtime = node->mtime;
  a.size = node->size;
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  // find the inode
  if(inum > INODE_NUM)
    return;
  struct inode *node = get_inode(inum);
  if(node == nullptr) {
    return;
  } 
  // free the data block
  unsigned int free_size = 0;
  int i = 0;
  for(i = 0; i < NDIRECT && free_size < node->size; ++i) {
    if(bm->is_valid_block(node->blocks[i])) {
      bm->free_block(node->blocks[i]);
      free_size += BLOCK_SIZE;
    } else {
      break;
    }
  }
  if(free_size < node->size) {
    // TODO: free indirect block
    int indirect_block_num = 0;
    uint32_t* indirect_blocks = get_indirect_blocks(node, indirect_block_num);
    for(i = 0; i < indirect_block_num && free_size < node->size; ++i) {
      if(bm->is_valid_block(indirect_blocks[i])) {
        bm->free_block(indirect_blocks[i]);
        free_size += BLOCK_SIZE;
      } else {
        break;
      }
    }
    bm->free_block(node->blocks[NDIRECT]);
  }
  free_inode(inum);
  delete node;
  return;
}
