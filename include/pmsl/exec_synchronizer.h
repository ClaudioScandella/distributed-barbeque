/*
 * Copyright (C) 2017  Politecnico di Milano
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BBQUE_EXC_SYNCHRONIZER_H_
#define BBQUE_EXC_SYNCHRONIZER_H_

#include <atomic>
#include <bitset>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "bbque/config.h"
#include "rtlib/bbque_exc.h"
#include "bbque/utils/timer.h"
#include "tg/task_graph.h"

#define BBQUE_TASKS_MAX_NUM BBQUE_APP_TG_TASKS_MAX_NUM

using namespace boost::accumulators;

namespace bbque {

/**
 * \class ExecutionSynchronizer
 *
 * \brief This class exposes some high-level member function to enable the
 * integration of a specific programming model on top of the BarbequeRTRM
 * Application Execution Model.
 */
class ExecutionSynchronizer: public bbque::rtlib::BbqueEXC {

public:

	enum class ExitCode {
		SUCCESS,
		ERR_TASK_ID,
		ERR_TASK_GRAPH_NOT_VALID,
		ERR_TASK_GRAPH_FILES,
		ERR_TASKS_IN_EXECUTION
	};


	/**
	 * \brief Constructor
	 */
	ExecutionSynchronizer(
		std::string const & name,
		std::string const & recipe,
		RTLIB_Services_t * rtlib);

	/**
	 * \brief Constructor
	 */
	ExecutionSynchronizer(
		std::string const & name,
		std::string const & recipe,
		RTLIB_Services_t * rtlib,
		std::shared_ptr<TaskGraph> tg);

	/**
	 * \brief Destructor
	 */
	virtual ~ExecutionSynchronizer() {
		tasks.runtime.clear();
		events.clear();

		if (access(tg_file_path.c_str(), F_OK) != -1)
			remove(tg_file_path.c_str());

		if (tg_sem != nullptr) {
			sem_close(tg_sem);
			sem_unlink(tg_sem_path.c_str());
		}
	}

	/**
	 * \brief Set a task-graph based description of the application
	 * \param tg Shared pointer to the TaskGraph object
	 * \return
	 */
	ExitCode SetTaskGraph(std::shared_ptr<TaskGraph> tg);

	/**
	 * \brief The task-graph based description of the application
	 * \return Shared pointer to the TaskGraph object
	 */
	inline std::shared_ptr<TaskGraph> GetTaskGraph() noexcept {
		return task_graph;
	}

	/**
	 * \brief Notify the launch of a task
	 * \param task_id The task identification number
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StartTask(uint32_t task_id) noexcept;

	/**
	 * \brief Notify the launch of a set of tasks
	 * \param tasks_id List of task identification numbers
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StartTasks(std::list<uint32_t> tasks_id) noexcept;

	/**
	 * \brief Notify the launch of all the tasks in the task-graph
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StartTasksAll() noexcept;

	/**
	 * \brief Notify the stop of a task
	 * \param task_id The task identification number
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StopTask(uint32_t task_id) noexcept;

	/**
	 * \brief Notify the stop of a set of tasks
	 * \param tasks_id List of task identification numbers
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StopTasks(std::list<uint32_t> tasks_id) noexcept;

	/**
	 * \brief Notify the stop of all the tasks in the task-graph
	 * \return SUCCESS for success. ERR_TASK_ID in case of wrong
	 * task identification number. ERR_TASK_GRAPH_NOT_VALID in case
	 * of bad formed task-graph
	 */
	ExitCode StopTasksAll() noexcept;


	/**
	 * \brief Notify (all) a registered event
	 * \param event_id the event identification number
	 */
	void NotifyEvent(uint32_t event_id) noexcept;

	/**
	 * \brief Wait for the resource manager to assign resources to
	 * the application
	 */
	void WaitForResourceAllocation() noexcept;

	/**
	 * \brief Thread-safe check of the resource assignment status
	 * \return true if the application has been scheduled, false
	 * otherwise
	 */
	inline bool IsResourceAllocationReady() noexcept {
		std::unique_lock<std::mutex> rtrm_ul(rtrm.mx);
		return rtrm.scheduled;
	}


protected:

	struct EventSync {
		std::mutex mx;
		std::condition_variable cv;
		uint32_t id;
		bool occurred = false;

		EventSync(uint32_t _id): id(_id){}
	};

	struct TaskProfiling {
		bbque::utils::Timer timer;
		accumulator_set<double, features<tag::mean, tag::variance>> acc;
	};

	struct RuntimeInfo {
		std::atomic<bool> is_running;
		std::thread  monitor_thr;
		TaskProfiling profile;

		RuntimeInfo(bool _run): is_running(_run) {}
	};


	std::string app_name;

	std::string tg_file_path;

	std::string tg_sem_path;

	std::shared_ptr<TaskGraph> task_graph;

	sem_t * tg_sem = nullptr;


	std::shared_ptr<EventSync> on_run_sync;

	std::map<uint32_t, std::shared_ptr<EventSync>> events;


	/**
	 * \struct tasks
	 * \brief Status information about tasks
	 */
	struct TasksInfo {
		std::mutex mx;
		std::condition_variable cv;
		std::queue<uint32_t>             start_queue;
		std::bitset<BBQUE_TASKS_MAX_NUM>  is_stopped;
		std::map<uint32_t, std::shared_ptr<RuntimeInfo>> runtime;
	} tasks;


	/**
	 * \struct rtrm
	 * \brief Status information the actions of the resource manager
	 */
	struct RTRMInfo {
		std::mutex mx;
		std::condition_variable cv;
		bool scheduled = false;
	} rtrm;

	/**
	 * \brief Set the path of the semaphore and the serial file
	 */
	ExitCode SetTaskGraphPaths();

	/**
	 * \brief Check the task graph provided is valid
	 * \return true if yes, false otherwise
	 */
	bool CheckTaskGraph(std::shared_ptr<TaskGraph>) noexcept;

	/**
	 * \brief Send the task graph to the resource manager
	 */
	void SendTaskGraphToRM();

	/**
	 * \brief Receive the task graph from the resource manager after
	 * the policy execution
	 */
	void RecvTaskGraphFromRM();

	/**
	 * \brief Notify the resource allocation done and thus the
	 * scheduling of the application
	 */
	void NotifyResourceAllocation() noexcept;

	/**
	 * \brief Perform the task profiling
	 * \param task_id The task identification number
	 */
	void TaskProfiler(uint32_t task_id) noexcept;

	/**
	 * \brief Send a notification to all the events associated to the buffers
	 * \param buffers A list of buffer ids
	 */
	inline void NotifyBuffersEvents(const std::list<uint32_t> & buffers) {
		for (auto buffer_id: buffers) {
			auto buffer = task_graph->GetBuffer(buffer_id);
			if (buffer != nullptr)
				NotifyEvent(buffer->Event());
		}
	}

	/**
	 * \brief Send a notification to all the events associated to the tasks
	 * \param buffers A list of task ids
	 */
	inline void NotifyTaskEvents(TaskPtr_t task) {
		NotifyBuffersEvents(task->OutputBuffers());
		NotifyBuffersEvents(task->InputBuffers());
		NotifyEvent(task->Event());
	}

	// --------------- BbqueEXC derived functions -------------------- //


	RTLIB_ExitCode_t onSetup() override;

	RTLIB_ExitCode_t onConfigure(int8_t awm_id) override;

	RTLIB_ExitCode_t onRun() override;

	RTLIB_ExitCode_t onMonitor() override;

	RTLIB_ExitCode_t onRelease() override;


	// --------------------------------------------------------------- //

	inline void enqueue_task(TaskPtr_t t) {
		if (tasks.is_stopped.test(t->Id())) {
			tasks.start_queue.push(t->Id());
			tasks.is_stopped.reset(t->Id());
			tasks.runtime[t->Id()]->is_running = true;
		}
		logger->Debug("Stopped tasks: %s", tasks.is_stopped.to_string().c_str());
		logger->Debug("Starting queue: %d", tasks.start_queue.size());
	}

	inline void dequeue_task(TaskPtr_t t) {
		if (!tasks.is_stopped.test(t->Id())) {
			tasks.is_stopped.set(t->Id());
			tasks.runtime[t->Id()]->is_running = false;
		}
	}
};

} // namespace bbque


#endif // BBQUE_EXC_SYNCHRONIZER_H_
