
#include "extent_client.h"
#include "extent_server_dist.h"
#include "chfs_client.h"

#include "extent_protocol.h"
#include "rpc.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

#define NUM_NODES 3
#define FILE_NUM 3
#define LARGE_FILE_SIZE_MIN 512 * 10
#define LARGE_FILE_SIZE_MAX 512 * 200

#define iprint(msg) \
    printf("[TEST_ERROR]: %s\n", msg);

chfs_client *chfs_c;
extent_client *ec;
extent_server_dist *es_rg;
int total_score = 0;
void get_filename(std::string &filename, int len) {
    filename = "file-";
    for (int i = 0; i < len; i++) {
        filename += 'a' + rand() % 26;
    }
}

int test_persist_chfs() {
    int i;

    chfs_client::inum parent = 1;
    chfs_client::fileinfo fin;
    std::vector<std::string> filenames;
    std::vector<chfs_client::inum> inums;

    printf("========== begin test persist chfs ==========\n");

    srand((unsigned)time(NULL));

    for (i = 0; i < FILE_NUM; i++) {
        // create a file
        std::string filename;
        get_filename(filename, 10);
        filenames.push_back(filename);
        mode_t mode = 0644;
        chfs_client::inum inum;
        const char *p = filename.c_str();
        chfs_c->isdir(1);
        chfs_c->create(parent, p, mode, inum);

        if ((int)inum == 0) {
            iprint("error creating file\n");
            return 1;
        }
        if (chfs_c->getfile(inum, fin) != chfs_client::xxstatus::OK) {
            iprint("error getting attr, return not OK\n");
            return 2;
        }
        inums.push_back(inum);
        size_t bytes_written;
        chfs_c->write(inum, filename.length(), 0, filename.c_str(), bytes_written);
        if (filename.length() != bytes_written) {
            iprint("error writing size \n");
            return 3;
        }
    }

    chfs_c->unlink(1, filenames[0].c_str());

    printf("--- begin crash ---\n");
    for (int i = 0; i < 3; i++) {
        es_rg->raft_group->disable_node(i);
        es_rg->raft_group->restart(i);
    }

    mssleep(2000); // wait for election
    printf("========== begin test after crash ==========\n");
    for (i = 1; i < FILE_NUM; i++) {
        chfs_client::inum inum = inums[i];
        std::string contents;
        std::string filename = filenames[i];
        chfs_c->read(inum, filename.length(), 0, contents);
        printf("read %s;ans:%s\n", contents.c_str(), filename.c_str());
        if (filename.compare(contents) != 0) {
            iprint("error reading after crash\n");
            return 5;
        }
    }
    bool isfind = true;
    chfs_c->lookup(1, filenames[0].c_str(), isfind, inums[0]);
    if (isfind) {
        iprint("error unlink after crash\n");
        return 6;
    }
    total_score += 3;
    printf("[pass chfs persist]\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int count = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    char *count_env = getenv("RPC_COUNT");
    if (count_env != NULL) {
        count = atoi(count_env);
    }
    std::string extent_port = argv[1];

    rpcs server(stoi(extent_port), count);
    es_rg = new extent_server_dist(NUM_NODES);
    server.reg(extent_protocol::get, es_rg, &extent_server_dist::get);
    server.reg(extent_protocol::getattr, es_rg, &extent_server_dist::getattr);
    server.reg(extent_protocol::put, es_rg, &extent_server_dist::put);
    server.reg(extent_protocol::remove, es_rg, &extent_server_dist::remove);
    server.reg(extent_protocol::create, es_rg, &extent_server_dist::create);

    chfs_c = new chfs_client(extent_port);

    if (test_persist_chfs() != 0)
        goto test_finish;
  

test_finish:
    return 0;
}
