#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <unordered_map>
#include <dirent.h>

#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "rpc.h"
#include "mr_protocol.h"

using namespace std;

#define DEBUG

#ifdef DEBUG
  #define LOG(format, args...) do {   \
    FILE* debug_log = fopen("worker.log", "a+");  \
    fprintf(debug_log, "%d, %s: ", __LINE__, __func__); \
    fprintf(debug_log, format, ##args); \
    fclose(debug_log);\
  } while (0)
#endif

struct KeyVal {
    string key;
    string val;
};

//
// The map function is called once for each file of input. The first
// argument is the name of the input file, and the second is the
// file's complete contents. You should ignore the input file name,
// and look only at the contents argument. The return value is a slice
// of key/value pairs.
//
vector<KeyVal> Map(const string &filename, const string &content)
{
	// Copy your code from mr_sequential.cc here.
	vector<KeyVal> result;
    unordered_map<std::string, uint64_t> word_map;

    std::size_t begin_pos = 0;
    bool in_word = false;
    std::string word;

    // suppose the file is not too long
    std::size_t file_size = content.size();

    // different from paper, also do some reduce work
    for (std::size_t i = 0; i < file_size; ++i) {
        if (isalpha(content[i])) {
            if (!in_word) {
                in_word = true;
                begin_pos = i;
            }
        } else {
            if (in_word) {
                in_word = false;
                word = content.substr(begin_pos, i - begin_pos);
                begin_pos = i + 1;

                if(!word.empty()) {
                    if (word_map.count(word)) {
                        word_map[word] += 1;
                    } else {
                        word_map[word] = 1;
                    }
                }
            }
        }
    } 

    // handle the tail word
    if (begin_pos != file_size && in_word) {
        word = content.substr(begin_pos);

        if (!word.empty() && isalpha(word[0])) {
            if (word_map.count(word)) {
                word_map[word] += 1;
            } else {
                word_map[word] = 1;
            }
        }
    }

    // generate the result
    for (auto elem : word_map) {
        KeyVal new_key_val;
        new_key_val.key = elem.first;
        new_key_val.val = to_string(elem.second);

        result.emplace_back(new_key_val);
    }

    return result;
}

//
// The reduce function is called once for each key generated by the
// map tasks, with a list of all the values created for that key by
// any map task.
//
string Reduce(const string &key, const vector < string > &values)
{
    // Copy your code from mr_sequential.cc here.
	uint64_t count = 0;
    for (auto val : values) {
        count += std::stoul(val);
    }

    return to_string(count);
}


typedef vector<KeyVal> (*MAPF)(const string &key, const string &value);
typedef string (*REDUCEF)(const string &key, const vector<string> &values);

class Worker {
public:
	Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf);

	void doWork();

private:
	void doMap(int index, vector<string> &filenames);
	void doReduce(int index);
	void doSubmit(mr_tasktype taskType, int index);
	int hash(string str);

	mutex mtx;
	int id;

	rpcc *cl;
	std::string basedir;
	MAPF mapf;
	REDUCEF reducef;
	std::string rawdir;
	std::string intermediate;
};


Worker::Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf)
{
	this->basedir = dir;
	this->rawdir = "../";
	this->mapf = mf;
	this->reducef = rf;

	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	this->cl = new rpcc(dstsock);
	if (this->cl->bind() < 0) {
		printf("mr worker: call bind error\n");
	}
}

int Worker::hash(string str)
{
	std::size_t str_hash = std::hash<std::string>{}(str);
	return str_hash % REDUCER_COUNT;
}

void Worker::doMap(int index, vector<string> &filenames)
{
	// Lab4: Your code goes here.
	string content;
	
	// cerr << "Read the whole file into the buffer, the filename: " << filename << endl;
	for(auto filename : filenames) {
		// Read the whole file into the buffer
		getline(ifstream(filename), content, '\0');
		vector<KeyVal> ret = mapf(filename, content);
		
		for(auto elem : ret) {
			int hash_val = this->hash(elem.key);
			assert(hash_val >= 0 && hash_val < REDUCER_COUNT);
			intermediate = intermediate + elem.key + ' ' + elem.val + '\n';
		}	

		// do the reduce job
		cout << "the filename: " << filename << endl;
		string output_filename = rawdir + "mr-out-" + to_string(index);
		ofstream out_file(output_filename);
		out_file << intermediate;
		intermediate.clear();
		out_file.close();
		cout << "(write end) the filename: " << filename << endl;
	}

	cerr << "finish domap" << endl;
}

void Worker::doReduce(int index)
{
	// Lab4: Your code goes here.
	return;
}

void Worker::doSubmit(mr_tasktype taskType, int index)
{
	bool b;
	mr_protocol::status ret = this->cl->call(mr_protocol::submittask, taskType, index, b);
	if (ret != mr_protocol::OK) {
		fprintf(stderr, "submit task failed: ret %d\n", ret);
		exit(-1);
	}
}

void Worker::doWork()
{
	for (;;) {

		//
		// Lab4: Your code goes here.
		// Hints: send asktask RPC call to coordinator
		// if mr_tasktype::MAP, then doMap and doSubmit
		// if mr_tasktype::REDUCE, then doReduce and doSubmit
		// if mr_tasktype::NONE, meaning currently no work is needed, then sleep
		//
		mr_protocol::AskTaskResponse response;
		mr_protocol::status ret = this->cl->call(mr_protocol::asktask, 0, response);
		if (ret != mr_protocol::OK) {
			continue; 
		}

		if (response.task_type == mr_tasktype::MAP) {
			vector<string> filenames;
			filenames.emplace_back(response.file_name);

			doMap(response.index, filenames);
			doSubmit(mr_tasktype::MAP, response.index);
		} else if (response.task_type == mr_tasktype::REDUCE) {
			doReduce(response.index);
			doSubmit(mr_tasktype::REDUCE, response.index);
		} else {
			sleep(1);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <coordinator_listen_port> <intermediate_file_dir> \n", argv[0]);
		exit(1);
	}

	MAPF mf = Map;
	REDUCEF rf = Reduce;
	
	Worker w(argv[1], argv[2], mf, rf);
	w.doWork();

	return 0;
}
