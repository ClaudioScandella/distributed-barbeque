#include "bbque/test.h"

using namespace std;

namespace bbque {

DistTest::DistTest() : dism(DistributedManager::GetInstance()) {}

DistTest & DistTest::GetInstance() {
	static DistTest instance;
	return instance;
}

void DistTest::Task() {

	cout << "Waiting for 10 seconds" << endl;
	this_thread::sleep_for(chrono::seconds(10));

	cout << "Starting my job" << endl;

	bbque::agent::ExitCode_t exit_code;

	while (!done) {

		cout << "Getting discovered instances" << endl;
		discovered = dism.GetInstancesID();

		cout << "I found: ";
		for(auto it = discovered.begin(); it != discovered.end(); ++it)
		{
			cout << '[' << it->first << "]: " << it->second << " - ";
		}
		cout << "" << endl;

		cout << "Getting remote platform proxy" << endl;
		PlatformManager & plm = PlatformManager::GetInstance();
		pp::RemotePlatformProxy* rpp = plm.GetRemotePlatformProxy();

		if(discovered.count(1))
		{
			cout << "Trying to get resource status of sys0.mem0 from id 1" << endl;

			string path = "sys0.mem0";
			agent::ResourceStatus r_status;
			exit_code = rpp->GetResourceStatus(1, path, r_status);

			if(exit_code != bbque::agent::ExitCode_t::OK)
				cout << "Request failed" << endl;
			else
			{
				cout << "I received:" << endl;
				cout << "\ttotal: " << r_status.total << endl;
				cout << "\tused: " << r_status.used << endl;
			}

			/**************************************************/

			cout << "Trying to get workload status of id 1" << endl;

			agent::WorkloadStatus w_status;
			exit_code = rpp->GetWorkloadStatus(1, w_status);

			if(exit_code != bbque::agent::ExitCode_t::OK)
				cout << "Request failed" << endl;
			else
			{
				cout << "I received:" << endl;
				cout << "\tnr_ready: " << w_status.nr_ready << endl;
				cout << "\tnr_running: " << w_status.nr_running << endl;
			}

			/**************************************************/

			cout << "Trying to get channel status of id 1" << endl;

			agent::ChannelStatus c_status;
			exit_code = rpp->GetChannelStatus(1, c_status);

			if(exit_code != bbque::agent::ExitCode_t::OK)
				cout << "Request failed" << endl;
			else
			{
				cout << "I received:" << endl;
				cout << "\tconnected: " << c_status.connected << endl;
				cout << "\tlatency_ms: " << c_status.latency_ms << endl;
			}

		}

		cout << "Waiting for 10 seconds" << endl;
		this_thread::sleep_for(chrono::seconds(10));
	}
	cout << "" << endl;
}

}


// struct ResourceStatus {
// 	uint64_t total;
// 	uint64_t used;
// 	int32_t power_mw;
// 	int32_t temperature;
// 	int16_t degradation;
// 	int32_t load;
// };

