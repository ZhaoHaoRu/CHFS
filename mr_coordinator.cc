#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

#include "mr_protocol.h"
#include "rpc.h"

using namespace std;

struct Task {
	int taskType;     // should be either Mapper or Reducer
	bool isAssigned;  // has been assigned to a worker
	bool isCompleted; // has been finised by a worker
	int index;        // index to the file
};

class Coordinator {
public:
	Coordinator(const vector<string> &files, int nReduce);
	mr_protocol::status askTask(int, mr_protocol::AskTaskResponse &reply);
	mr_protocol::status submitTask(int taskType, int index, bool &success);
	bool isFinishedMap();
	bool isFinishedReduce();
	void generateResult();
	bool Done();

private:
	vector<string> files;
	vector<Task> mapTasks;
	vector<Task> reduceTasks;

	mutex mtx;

	long completedMapCount;
	long completedReduceCount;
	long assignedMapCount;
	bool isFinished;
	
	string getFile(int index);
};


// Your code here -- RPC handlers for the worker to call.

mr_protocol::status Coordinator::askTask(int, mr_protocol::AskTaskResponse &reply) {
	// Lab4 : Your code goes here.

	int file_size = files.size();
	// cerr << "check asktask request" << endl;
	// check whether need map
	if (!isFinishedMap()) {
		this->mtx.lock();
		// cerr << "get line 57" << endl;
		for (int i = 0; i < file_size; ++i) {
			if (!mapTasks[i].isAssigned) {
				reply.task_type = mr_tasktype::MAP;
				reply.index = i;
				reply.file_name = files[i];
				mapTasks[i].isAssigned = true;
				++assignedMapCount;
				cout << "the assignedMapCount: " << assignedMapCount << std::endl;
				cerr << "assgin map work" << endl;
				this->mtx.unlock();
				return mr_protocol::OK;
			} 
		}
		this->mtx.unlock();
	} else if (!isFinishedReduce()) {
		this->mtx.lock();
		for (int i = 0; i < file_size; ++i) {
			if (!reduceTasks[i].isAssigned) {
				reply.task_type = mr_tasktype::REDUCE;
				reply.index = i;
				reply.file_name = files[i];
				reduceTasks[i].isAssigned = true;
				cerr << "assgin reduce work" << endl;
				this->mtx.unlock();
				return mr_protocol::OK;
			} 
		}
		this->mtx.unlock();
	}

	reply.task_type = mr_tasktype::NONE;
	return mr_protocol::OK;
}

mr_protocol::status Coordinator::submitTask(int taskType, int index, bool &success) {
	// Lab4 : Your code goes here.
	this->mtx.lock();

	int file_size = files.size();
	success = false;

	cerr << "get work " << index << "submit, " << "filesize: " << file_size << ", taskType: " << taskType << endl;

	if (index < 0 || index >= file_size) {
		return mr_protocol::OK;
	}

	
	if (taskType == mr_tasktype::MAP) {
		success = true;
		mapTasks[index].isCompleted = true;
		++completedMapCount;
		cerr << "completedMapCount: " << completedMapCount << endl;
	} else if (taskType == mr_tasktype::REDUCE) {
		success = true;
		reduceTasks[index].isCompleted = true;
		++completedReduceCount;
		if (this->completedReduceCount >= long(this->reduceTasks.size())) {
			// write to the file for test
			if(!isFinished) {
				cerr << "generate the result" << endl;
				generateResult();
				cerr << "generate the result end" << endl;
			}
			isFinished = true;
		}
		cerr << "completedReduceCount: " << completedMapCount << endl;
	}

	this->mtx.unlock();
	return mr_protocol::OK;
}

void Coordinator::generateResult() {
	map<string, uint64_t> result;
	string basedir = "../";
	string intermediate_filename, line, key, val, output_filename;
	int i = 0;
	size_t pos = 0;
	uint64_t raw_val = 0;

	while (true) {
		intermediate_filename = basedir + "mr-out-" + to_string(i);
		ifstream file(intermediate_filename);
		++i;

		if(file) {
			file.seekg(0,std::ios::beg);
			while(getline(file, line)) {
				pos = line.find(' ');

				if (pos == std::string::npos) {
					cerr << "not found" << endl;
				} else {
					key = line.substr(0, pos);
					val = line.substr(pos + 1);
					raw_val = stoul(val);
					result[key] += raw_val;
				}	
			}
			file.close();
		} else {
			file.close();
			break;
		}
	}

	// write to the output file
	string content;
	for (auto elem : result) {
		content = content + elem.first + ' ' + to_string(elem.second) + '\n';
	}

	output_filename = "./mr-out-0";
	ofstream out_file(output_filename);
	out_file << content;
	cerr << "task finish" << endl;
	out_file.close();
}

string Coordinator::getFile(int index) {
	this->mtx.lock();
	string file = this->files[index];
	this->mtx.unlock();
	return file;
}

bool Coordinator::isFinishedMap() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedMapCount >= long(this->mapTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

bool Coordinator::isFinishedReduce() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedReduceCount >= long(this->reduceTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

//
// mr_coordinator calls Done() periodically to find out
// if the entire job has finished.
//
bool Coordinator::Done() {
	bool r = false;
	this->mtx.lock();
	r = this->isFinished;
	this->mtx.unlock();
	return r;
}

//
// create a Coordinator.
// nReduce is the number of reduce tasks to use.
//
Coordinator::Coordinator(const vector<string> &files, int nReduce)
{
	this->files = files;
	this->isFinished = false;
	this->completedMapCount = 0;
	this->completedReduceCount = 0;
	this->assignedMapCount = 0;

	int filesize = files.size();
	for (int i = 0; i < filesize; i++) {
		this->mapTasks.push_back(Task{mr_tasktype::MAP, false, false, i});
	}
	for (int i = 0; i < nReduce; i++) {
		this->reduceTasks.push_back(Task{mr_tasktype::REDUCE, false, false, i});
	}
}

int main(int argc, char *argv[])
{
	int count = 0;

	if(argc < 3){
		fprintf(stderr, "Usage: %s <port-listen> <inputfiles>...\n", argv[0]);
		exit(1);
	}
	char *port_listen = argv[1];
	
	setvbuf(stdout, NULL, _IONBF, 0);

	char *count_env = getenv("RPC_COUNT");
	if(count_env != NULL){
		count = atoi(count_env);
	}

	vector<string> files;
	char **p = &argv[2];
	while (*p) {
		files.push_back(string(*p));
		++p;
	}

	rpcs server(atoi(port_listen), count);

	Coordinator c(files, REDUCER_COUNT);
	
	//
	// Lab4: Your code here.
	// Hints: Register "askTask" and "submitTask" as RPC handlers here
	// 
	server.reg(mr_protocol::rpc_numbers::asktask, &c, &Coordinator::askTask);
	server.reg(mr_protocol::rpc_numbers::submittask, &c, &Coordinator::submitTask);

	while(!c.Done()) {
		sleep(1);
	}

	return 0;
}


