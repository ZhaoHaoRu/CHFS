// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client()
{
  es = new extent_server();
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("extent client create: %d", type);
  ret = es->create(type, id);
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = es->get(eid, buf);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = es->getattr(eid, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = es->put(eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = es->remove(eid, r);
  return ret;
}



extent_protocol::status 
extent_client::begin_log() {
  int ret = es->begin_log();
  return ret;
}

extent_protocol::status 
extent_client::commit_log() {
  int ret = es->commit_log();
  return ret;
}


extent_protocol::status 
extent_client::create_log(uint32_t type) {
  // es->create_log(type);
  return 0;
}

extent_protocol::status 
extent_client::put_log(extent_protocol::extentid_t id, std::string buf) {
  // es->put_log(id, buf);
  return 0;
}