#ifndef mr_protocol_h_
#define mr_protocol_h_

#include <string>
#include <vector>

#include "rpc.h"

using namespace std;

#define REDUCER_COUNT 4

enum mr_tasktype {
	NONE = 0, // this flag means no task needs to be performed at this point
	MAP,
	REDUCE
};

class mr_protocol {
public:
	typedef int status;
	enum xxstatus { OK, RPCERR, NOENT, IOERR };
	enum rpc_numbers {
		asktask = 0xa001,
		submittask,
	};

	struct AskTaskResponse {
		// Lab4: Your definition here.
		int index;
		int task_type;
		std::string file_name;
	};

	struct AskTaskRequest {
		// Lab4: Your definition here.
	};

	struct SubmitTaskResponse {
		// Lab4: Your definition here.
	};

	struct SubmitTaskRequest {
		// Lab4: Your definition here.
	};
};

marshall &operator<<(marshall &m, const mr_protocol::AskTaskResponse &args) {
    // Lab3: Your code here
    m = m << args.index;
    m = m << args.task_type;
	m = m << args.file_name;
    return m;
}

unmarshall &operator>>(unmarshall &u, mr_protocol::AskTaskResponse &args) {
    // Lab3: Your code here
	u = u >> args.index;
	u = u >> args.task_type;
	u = u >> args.file_name;
    return u;
}

#endif

