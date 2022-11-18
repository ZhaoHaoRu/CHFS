#include "extent_protocol.h"
#include "rpc.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "extent_server_dist.h"

// Main loop of extent server raft group

int main(int argc, char *argv[]) {
    int count = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    char *count_env = getenv("RPC_COUNT");
    if (count_env != NULL) {
        count = atoi(count_env);
    }

    rpcs server(atoi(argv[1]), count);
    extent_server_dist es_rg(3); // extent server for raft group

    // You can not change or add the rpc interfaces
    printf("extent server dist started at port %d\n", atoi(argv[1]));
    server.reg(extent_protocol::get, &es_rg, &extent_server_dist::get);
    server.reg(extent_protocol::getattr, &es_rg, &extent_server_dist::getattr);
    server.reg(extent_protocol::put, &es_rg, &extent_server_dist::put);
    server.reg(extent_protocol::remove, &es_rg, &extent_server_dist::remove);
    server.reg(extent_protocol::create, &es_rg, &extent_server_dist::create);

    while (1)
        sleep(1000);
}
