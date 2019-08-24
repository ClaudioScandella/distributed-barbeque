#ifndef BBQUE_DISTTEST_H_
#define BBQUE_DISTTEST_H_

#include "bbque/utils/worker.h"
#include "bbque/pp/remote_platform_proxy.h"
#include "bbque/distributed_manager.h"

#include <chrono>
#include <thread>
#include <map>

using namespace std;

namespace bbque {

class DistTest : public utils::Worker
{
public:
	static DistTest & GetInstance();
	DistTest();

private:
	virtual void Task() override final;

	DistributedManager & dism;

	map<int, std::string> discovered;

};
}

#endif