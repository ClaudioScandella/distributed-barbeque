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

#include <iostream>
#include "tg/task_graph.h"

namespace bbque {


TaskGraph::TaskGraph(uint32_t app_id):
		application_id(app_id) {
}

TaskGraph::TaskGraph(
		TaskMap_t const & tasks,
		BufferMap_t const & buffers,
		uint32_t app_id):
	application_id(app_id) {

	bool buff_valid = false;
	for (auto & t_entry: tasks) {
		auto t = t_entry.second;
		buff_valid = AreBuffersValid(t, buffers);
		if (!buff_valid) return;
	}

	this->tasks    = tasks;
	this->buffers  = buffers;
	this->is_valid = true;
}

TaskGraph::TaskGraph(
		TaskMap_t const & tasks,
		BufferMap_t const & buffers,
		EventMap_t const & events,
		uint32_t app_id):
	application_id(app_id) {
	bool buff_valid = false;
	for (auto & t_entry: tasks) {
		auto & t(t_entry.second);
		// buffer validation
		buff_valid = AreBuffersValid(t, buffers);
		if (!buff_valid) return;
		// Add event validation
//		auto evit = events.find(t->GetEvent());
//		if (evit == events.end()) return;
	}

	this->tasks    = tasks;
	this->buffers  = buffers;
	this->events   = events;
	this->is_valid = true;
}


void TaskGraph::Print() const noexcept {
	std::cerr << "Tasks: ";
	for (auto & t_entry: tasks ) {
		auto & t(t_entry.second);
		std::cerr << t->Id() << "[unit=" << t->GetMappedProcessor() << "] ";
	}
	std::cerr << std::endl;
	std::cerr << "Buffers: ";
	for (auto & b_entry: buffers ) {
		auto & b(b_entry.second);
		std::cerr << b->Id() << " ";
	}
	std::cerr << std::endl;

	std::string outb_str("unset");
	if (out_buff)
		outb_str = std::to_string(out_buff->Id());
	std::cerr << "Output buffer: " << outb_str << std::endl;
	std::cerr << "Events: ";
	for (auto & e_entry: events ) {
		auto & e(e_entry.second);
		std::cerr << e->Id() << " ";
	}
	std::cerr << std::endl;
}

bool TaskGraph::AreBuffersValid(TaskPtr_t task, BufferMap_t const & buffers) {
	for (auto id: task->InputBuffers()) {
		auto b = buffers.find(id);
		if (b == buffers.end()) return false;
	}
	for (auto id: task->OutputBuffers()) {
		auto b = buffers.find(id);
		if (b == buffers.end()) return false;
	}
	return true;
}


/* ========================== Buffers ===================== */


TaskGraph::ExitCode TaskGraph::SetOutputBuffer(int32_t id) {
	auto outb = buffers.find(id);
	if (outb == buffers.end())
		return ExitCode::ERR_INVALID_BUFFER;
	this->out_buff = outb->second;
	return ExitCode::SUCCESS;
}


} // namespace bbque
