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

#ifndef BBQUE_TG_TASK_GRAPH_H
#define BBQUE_TG_TASK_GRAPH_H

#include <iostream>
#include <map>
#include <vector>

// Boost libraries
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include "tg/pointers.h"
#include "tg/task.h"
#include "tg/mem_buffer.h"
#include "tg/event.h"

#define BBQUE_TG_FILE_PREFIX  BBQUE_APP_TG_PATH"/TG_"


namespace bbque {

using IdList_t  = std::list<uint32_t>;
using TaskMap_t = std::map<uint32_t, TaskPtr_t>;
using BufferMap_t = std::map<uint32_t, BufferPtr_t>;
using EventMap_t  = std::map<uint32_t, EventPtr_t>;

/**
 * \class TaskGraph
 * \brief A task-graph representation
 */
class TaskGraph: public Profilable {

public:

	enum class ExitCode {
		SUCCESS = 0,
		ERR_INVALID_TASK,
		ERR_INVALID_BUFFER,
		ERR_INVALID_EVENT
	};

	/**
	 * \brief Constructor
	 * \param app_id Application identification number
	 */
	TaskGraph(uint32_t app_id = 0);

	/**
	 * \brief Constructor
	 * \param tasks Map of registered tasks
	 * \param buffers Map of registered buffers
	 * \param app_id Application identification number
	 */
	TaskGraph(
		TaskMap_t const & tasks,
		BufferMap_t const & buffers,
		uint32_t app_id = 0);

	/**
	 * \brief Constructor
	 * \param tasks Map of registered tasks
	 * \param buffers Map of registered buffers
	 * \param events Map of registered events
	 * \param app_id Application identification number
	 */
	TaskGraph(
		TaskMap_t const & tasks,
		BufferMap_t const & buffers,
		EventMap_t const & events,
		uint32_t app_id = 0);

	/**
	 * \brief Destructor
	 */
	virtual ~TaskGraph() {
		tasks.clear();
		buffers.clear();
		events.clear();
	}

	/**
	 * \brief Get the application identification number
	 * \return An integer value to identify a specific application
	 */
	inline uint32_t GetApplicationId() const { return application_id; }

	/**
	 * \brief Set the application identification number
	 */
	inline void SetApplicationId(uint32_t app_id) {
		application_id = app_id;
	}


	/**
	 * \brief Check if it is successfully initialized
	 * \return True if so, false otherwise
	 */
	inline bool IsValid() const { return is_valid; }

	/**
	 * \brief Number of tasks in the task-graph
	 * \return An integer value about the number of tasks
	 */
	inline size_t TaskCount() const { return tasks.size(); }

	/**
	 * \brief Get the tasks vector
	 * \return A vector of shared pointers to Task objects
	 */
	inline const TaskMap_t & Tasks() { return tasks; }

	/**
	 * \brief Get the tasks vector
	 * \return A vector of shared pointers to Task objects
	 */
	inline const TaskMap_t & Tasks() const { return tasks; }

	/**
	 * \brief Get a task descriptor
	 * \param id The task id number
	 * \return A shared pointer to Task object in case of success,
	  a null pointer otherwise
	 */
	inline TaskPtr_t GetTask(uint32_t id) {
		auto it_task = tasks.find(id);
		if (it_task == tasks.end()) return nullptr;
		return it_task->second;
	}

	/**
	 * \brief Remove a task descriptor
	 * \param task_id Task identification number
	 */
	inline void RemoveTask(uint32_t task_id) {
		for (auto b_entry: buffers)
			b_entry.second->RemoveTask(task_id);
		tasks.erase(task_id);
	}


	/**
	 * \brief Number of buffers included in the task-graph
	 * \return An integer value with the number of buffers
	 */
	inline size_t BufferCount() const { return buffers.size(); }

	/**
	 * \brief The buffers included in the task-graph
	 * \return A map with shared pointers to all the Buffer objects
	 */
	inline const BufferMap_t & Buffers() { return buffers; }

	/**
	 * \brief The buffers included in the task-graph
	 * \return A map with shared pointers to all the Buffer objects
	 */
	inline const BufferMap_t & Buffers() const { return buffers; }

	/**
	 * \brief Get a buffer descriptor
	 * \param The buffer identification number
	 * \return A shared pointer to Buffer object in case of success,
	  a null pointer otherwise
	 */
	inline BufferPtr_t GetBuffer(uint32_t id) {
		auto buff = buffers.find(id);
		if (buff == buffers.end()) return nullptr;
		return buff->second;
	}

	/**
	 * \brief Remove a buffer descriptor
	 * \param id Buffer identification number
	 */
	void RemoveBuffer(uint32_t id);


	/**
	 * \brief Set the globale output buffer of the task-graph execution,
	 * i.e. where
	 * the final output data will be written
	 * \param id Buffer identification number
	 * \return SUCCESS for success or ERR_INVALID_BUFFER if the id
	 * provided does not correspond to any registered buffer
	 */
	ExitCode SetOutputBuffer(int32_t id);

	/**
	 * \brief Get the global output buffer
	 * \return Shared pointer to the buffer descriptor
	 */
	inline BufferPtr_t OutputBuffer() { return out_buff; }


	/**
	 * \brief The events objects used for synchronization purposes
	 * \return The map of Event object
	 */
	inline const EventMap_t & Events() { return events; }

	/**
	 * \brief Specific event object
	 * \param id The event object identification number
	 * \return A shared pointer to an Event object
	 */
	inline EventPtr_t GetEvent(uint32_t id) {
		auto ev = events.find(id);
		if (ev == events.end()) return nullptr;
		return ev->second;
	}

	void Print() const noexcept;

private:

	/*** Application identification number (PID, or other) ***/
	uint32_t application_id;

	/*** Initialization and validation status ***/
	bool is_valid = false;

	/*** Task objects ***/
	TaskMap_t tasks;

	/*** Buffer objects ***/
	BufferMap_t buffers;

	/*** Event objects ***/
	EventMap_t events;

	/*** Global output buffer ***/
	BufferPtr_t out_buff = nullptr;


	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		(void) version;
	        ar & boost::serialization::base_object<Profilable>(*this);
		ar & application_id;
		ar & is_valid;
		ar & tasks;
		ar & buffers;
		ar & events;
	}

	/**
	 * \brief Check the buffer validity
	 * \param task Task descriptor
	 * \param buffers Map of registered buffers
	 * \return true if valid, false otherwise
	 */
	bool AreBuffersValid(TaskPtr_t task, BufferMap_t const & buffers);

};

} // namespace bbque

#endif // BBQUE_TASK_GRAPH_H
