/*
 * Copyright (C) 2019  Politecnico di Milano
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

#include "bbque/app/schedulable.h"
#include "bbque/app/working_mode.h"


namespace bbque { namespace app {


Schedulable::State_t Schedulable::_State() const {
	return schedule.state;
}

Schedulable::State_t Schedulable::State() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _State();
}

Schedulable::State_t Schedulable::_PreSyncState() const {
	return schedule.preSyncState;
}

Schedulable::State_t Schedulable::PreSyncState() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _PreSyncState();
}

Schedulable::SyncState_t Schedulable::_SyncState() const {
	return schedule.syncState;
}

Schedulable::SyncState_t Schedulable::SyncState() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _SyncState();
}


bool Schedulable::_Disabled() const {
	return ((_State() == DISABLED) ||
			(_State() == FINISHED));
}

bool Schedulable::Disabled() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Disabled();
}

bool Schedulable::_Active() const {
	return ((schedule.state == READY) ||
			(schedule.state == RUNNING));
}

bool Schedulable::Active() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Active();
}

bool Schedulable::_Running() const {
	return ((schedule.state == RUNNING));
}

bool Schedulable::Running() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Running();
}

bool Schedulable::_Synching() const {
	return (schedule.state == SYNC);
}

bool Schedulable::Synching() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Synching();
}

bool Schedulable::_Starting() const {
	return (_Synching() && (_SyncState() == STARTING));
}

bool Schedulable::Starting() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Starting();
}

bool Schedulable::_Blocking() const {
	return (_Synching() && (_SyncState() == BLOCKED));
}

bool Schedulable::Blocking() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Blocking();
}

AwmPtr_t const & Schedulable::_CurrentAWM() const {
	return schedule.awm;
}

AwmPtr_t const & Schedulable::CurrentAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _CurrentAWM();
}

AwmPtr_t const & Schedulable::_NextAWM() const {
	return schedule.next_awm;
}

AwmPtr_t const & Schedulable::NextAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _NextAWM();
}

bool Schedulable::_SwitchingAWM() const {
	if (schedule.state != SYNC)
		return false;
	if (schedule.awm->Id() == schedule.next_awm->Id())
		return false;
	return true;
}

bool Schedulable::SwitchingAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _SwitchingAWM();
}

uint64_t Schedulable::ScheduleCount() const noexcept {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return schedule.count;
}

} // namespace app

} // namespace bbque

