/*
 * Copyright (C) 2012  Politecnico di Milano
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

#include "bbque/app/application.h"

#ifdef CONFIG_TARGET_ANDROID
# include <stdint.h>
# include <math.h>
#else
# include <cstdint>
# include <cmath>
#endif
#include <limits>

#include "bbque/application_manager.h"
#include "bbque/app/working_mode.h"
#include "bbque/app/recipe.h"
#include "bbque/plugin_manager.h"
#include "bbque/resource_accounter.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/resource_assignment.h"

namespace ba = bbque::app;
namespace br = bbque::res;
namespace bp = bbque::plugins;

namespace bbque { namespace app {

char const *ApplicationStatusIF::stateStr[] = {
	"DISABLED",
	"READY",
	"SYNC",
	"RUNNING",
	"FINISHED"
};

char const *ApplicationStatusIF::syncStateStr[] = {
	"STARTING",
	"RECONF",
	"MIGREC",
	"MIGRATE",
	"BLOCKED",
	"NONE"
};

// Compare two working mode values.
// This is used to sort the list of enabled working modes.
bool AwmValueLesser(const AwmPtr_t & wm1, const AwmPtr_t & wm2) {
		return wm1->Value() < wm2->Value();
}

bool AwmIdLesser(const AwmPtr_t & wm1, const AwmPtr_t & wm2) {
		return wm1->Id() < wm2->Id();
}

Application::Application(std::string const & _name,
		AppPid_t _pid,
		uint8_t _exc_id,
		RTLIB_ProgrammingLanguage_t lang,
		bool container):
	name(_name),
	pid(_pid),
	exc_id(_exc_id),
	language(lang),
	container(container) {

	// Init the working modes vector
	awms.recipe_vect.resize(MAX_NUM_AWM);

	// Get a logger
	logger = bu::Logger::GetLogger(APPLICATION_NAMESPACE);
	assert(logger);

	// Format the EXC string identifier
	::snprintf(str_id, 16, "%05d:%6s:%02d",
			Pid(), Name().substr(0,6).c_str(), ExcId());

	// Initialized scheduling state
	schedule.state        = DISABLED;
	schedule.preSyncState = DISABLED;
	schedule.syncState    = SYNC_NONE;

	logger->Info("Built new EXC [%s]", StrId());
}

Application::~Application() {
	logger->Debug("Destroying EXC [%s]", StrId());

	// Releasing AWMs and ResourceConstraints...
	awms.recipe_vect.clear();
	awms.enabled_list.clear();
	rsrc_constraints.clear();
}

void Application::SetPriority(AppPrio_t _prio) {
	bbque::ApplicationManager &am(bbque::ApplicationManager::GetInstance());

	// If _prio value is greater then the lowest priority
	// (maximum integer value) it is trimmed to the last one.
	priority = std::min(_prio, am.LowestPriority());
}

void Application::InitWorkingModes(AppPtr_t & papp) {
	// Get the working modes from recipe and init the vector size
	AwmPtrVect_t const & rcp_awms(recipe->WorkingModesAll());

	// Init AWM range attributes (for AWM constraints)
	awms.max_id   = rcp_awms.size() - 1;
	awms.low_id   = 0;
	awms.upp_id   = awms.max_id;
	awms.curr_inv = false;
	awms.enabled_bset.set();
	logger->Debug("InitWorkingModes: max ID = %d", awms.max_id);

	// Init AWM lists
	for (int i = 0; i <= awms.max_id; ++i) {
		// Copy the working mode and set the owner (current Application)
		AwmPtr_t app_awm(new WorkingMode(*rcp_awms[i]));
		assert(!app_awm->Owner());
		app_awm->SetOwner(papp);

		// Insert the working mode into the vector
		awms.recipe_vect[app_awm->Id()] = app_awm;

		// Do not insert the hidden AWMs into the enabled list
		if (app_awm->Hidden()) {
			logger->Debug("InitWorkingModes: skipping hidden AWM %d",
					app_awm->Id());
			continue;
		}

		// Valid (not hidden) AWM: Insert it into the list
		awms.enabled_list.push_back(app_awm);
	}

	// Sort the enabled list by "value"
	awms.enabled_list.sort(AwmValueLesser);
	logger->Info("InitWorkingModes: %d enabled AWMs",
			awms.enabled_list.size());
}

void Application::InitResourceConstraints() {
	ConstrMap_t::const_iterator cons_it(recipe->ConstraintsAll().begin());
	ConstrMap_t::const_iterator end_cons(recipe->ConstraintsAll().end());

	// For each static constraint on a resource make an assertion
	for (; cons_it != end_cons; ++cons_it) {
		ResourcePathPtr_t const & rsrc_path(cons_it->first);
		ConstrPtr_t const & rsrc_constr(cons_it->second);

		// Lower bound
		if (rsrc_constr->lower > 0)
				SetResourceConstraint(
						rsrc_path,
						br::ResourceConstraint::LOWER_BOUND,
						rsrc_constr->lower);
		// Upper bound
		if (rsrc_constr->upper > 0)
				SetResourceConstraint(
						rsrc_path,
						br::ResourceConstraint::UPPER_BOUND,
						rsrc_constr->upper);
	}

	logger->Debug("%d resource constraints from the recipe",
			rsrc_constraints.size());
}

Application::ExitCode_t
Application::SetRecipe(RecipePtr_t & _recipe, AppPtr_t & papp) {
	// Safety check on recipe object
	if (!_recipe) {
		logger->Error("SetRecipe: null recipe object");
		return APP_RECP_NULL;
	}

	// Set the recipe and the static priority
	recipe   = _recipe;
	priority = recipe->GetPriority();

	// Set working modes
	InitWorkingModes(papp);
	logger->Info("%d working modes", awms.enabled_list.size());

	// Set (optional) resource constraints
	InitResourceConstraints();
	logger->Info("%d constraints in the application", rsrc_constraints.size());

	// Specific attributes (i.e. plugin specific)
	plugin_data = PluginDataMap_t(recipe->plugin_data);
	logger->Info("%d plugin-specific attributes", plugin_data.size());

	return APP_SUCCESS;
}

AwmPtrList_t::iterator Application::FindWorkingModeIter(
		AwmPtrList_t & awm_list,
		uint16_t wmId) {
	AwmPtrList_t::iterator awm_it(awm_list.begin());
	AwmPtrList_t::iterator end_awm(awm_list.end());

	for (; awm_it != end_awm; ++awm_it) {
		if ((*awm_it)->Id() == wmId)
			break;
	}

	return awm_it;
}


/*******************************************************************************
 *  EXC State and SyncState Management
 ******************************************************************************/

bool Application::_Disabled() const {
	return ((_State() == DISABLED) ||
			(_State() == FINISHED));
}

bool Application::Disabled() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Disabled();
}

bool Application::_Active() const {
	return ((schedule.state == READY) ||
			(schedule.state == RUNNING));
}

bool Application::Active() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Active();
}

bool Application::_Running() const {
	return ((schedule.state == RUNNING));
}

bool Application::Running() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Running();
}

bool Application::_Synching() const {
	return (schedule.state == SYNC);
}

bool Application::Synching() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Synching();
}

bool Application::_Starting() const {
	return (_Synching() && (_SyncState() == STARTING));
}

bool Application::Starting() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Starting();
}

bool Application::_Blocking() const {
	return (_Synching() && (_SyncState() == BLOCKED));
}

bool Application::Blocking() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _Blocking();
}

Application::State_t Application::_State() const {
	return schedule.state;
}

Application::State_t Application::State() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _State();
}

Application::State_t Application::_PreSyncState() const {
	return schedule.preSyncState;
}

Application::State_t Application::PreSyncState() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _PreSyncState();
}

Application::SyncState_t Application::_SyncState() const {
	return schedule.syncState;
}

Application::SyncState_t Application::SyncState() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _SyncState();
}

AwmPtr_t const & Application::_CurrentAWM() const {
	return schedule.awm;
}

AwmPtr_t const & Application::CurrentAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _CurrentAWM();
}

AwmPtr_t const & Application::_NextAWM() const {
	return schedule.next_awm;
}

AwmPtr_t const & Application::NextAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _NextAWM();
}

bool Application::_SwitchingAWM() const {
	if (schedule.state != SYNC)
		return false;
	if (schedule.awm->Id() == schedule.next_awm->Id())
		return false;
	return true;
}

bool Application::SwitchingAWM() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	return _SwitchingAWM();
}

// NOTE: this requires a lock on schedule.mtx
void Application::SetSyncState(SyncState_t sync) {

	logger->Debug("Changing sync state [%s, %d:%s => %d:%s]",
			StrId(),
			_SyncState(), SyncStateStr(_SyncState()),
			sync, SyncStateStr(sync));

	schedule.syncState = sync;
}

// NOTE: this requires a lock on schedule.mtx
void Application::SetState(State_t state, SyncState_t sync) {
	bbque::ApplicationManager &am(bbque::ApplicationManager::GetInstance());
	AppPtr_t papp = am.GetApplication(Uid());

	logger->Debug("Changing state [%s, %d:%s => %d:%s]",
			StrId(),
			_State(), StateStr(_State()),
			state, StateStr(state));

	// Entering a Synchronization state
	if (state == SYNC) {
		assert(sync != SYNC_NONE);

		// Save a copy of the pre-synchronization state
		schedule.preSyncState = _State();

		// Update synchronization state
		SetSyncState(sync);

		// Update queue based on current application state
		am.NotifyNewState(papp, Application::SYNC);

		// Updating state
		schedule.state = Application::SYNC;

		return;
	}


	// Entering a statble state
	assert(sync == SYNC_NONE);

	// Update queue based on current application state
	am.NotifyNewState(papp, state);

	// Updating state
	schedule.preSyncState = state;
	schedule.state = state;

	// Update synchronization state
	SetSyncState(sync);

	// Release current selected AWM
	if ((state == DISABLED) ||
			(state == READY)) {
		schedule.awm.reset();
		schedule.next_awm.reset();
	}

}

/*******************************************************************************
 *  EXC Destruction
 ******************************************************************************/

Application::ExitCode_t Application::Terminate() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);

	// This is to enforce a single removal of an EXC, indeed, due to
	// parallelized execution of commands, it could happen that (e.g. due
	// to timeout) the same command is issued a second time while it's
	// being served by BBQ
	if (_State() == FINISHED) {
		logger->Warn("Multiple termination of EXC [%s]", StrId());
		return APP_FINISHED;
	}

	// Mark the application as finished
	SetState(FINISHED);
	state_ul.unlock();

	logger->Info("EXC [%s] FINISHED", StrId());

	return APP_SUCCESS;

}


/*******************************************************************************
 *  EXC Enabling
 ******************************************************************************/

Application::ExitCode_t Application::Enable() {
	std::unique_lock<std::recursive_mutex>
		state_ul(schedule.mtx, std::defer_lock);

	// Enabling the execution context
	logger->Debug("Enabling EXC [%s]...", StrId());

	state_ul.lock();

	// Not disabled applications could not be marked as READY
	if (!_Disabled()) {
		logger->Crit("Trying to enable already enabled application [%s] "
				"(Error: possible data structure curruption?)",
				StrId());
		assert(_Disabled());
		return APP_ABORT;
	}

	// Mark the application has ready to run
	SetState(READY);

	state_ul.unlock();

	logger->Info("EXC [%s] ENABLED", StrId());

	return APP_SUCCESS;
}


/*******************************************************************************
 *  EXC Disabled
 ******************************************************************************/

Application::ExitCode_t Application::Disable() {
	std::unique_lock<std::recursive_mutex>
		state_ul(schedule.mtx, std::defer_lock);

	// Not disabled applications could not be marked as READY
	if (_Disabled()) {
		logger->Warn("Trying to disable already disabled application [%s]",
				StrId());
		return APP_SUCCESS;
	}

	// Mark the application as ready to run
	state_ul.lock();
	SetState(DISABLED);
	state_ul.unlock();

	logger->Info("EXC [%s] DISABLED", StrId());

	return APP_SUCCESS;
}


/*******************************************************************************
 *  EXC Optimization
 ******************************************************************************/

// NOTE: this requires a lock on schedule.mtx
Application::ExitCode_t Application::RequestSync(SyncState_t sync) {
	bbque::ApplicationManager &am(bbque::ApplicationManager::GetInstance());
	AppPtr_t papp = am.GetApplication(Uid());
	ApplicationManager::ExitCode_t result;

	if (!_Active()) {
		logger->Crit("Sync request FAILED (Error: wrong application status)");
		assert(_Active());
		return APP_ABORT;
	}

	logger->Debug("Request synchronization [%s, %d:%s]",
			StrId(), sync, SyncStateStr(sync));

	// Ensuring the AM has an hander for this application
	if (!papp) {
		logger->Crit("Request synchronization [%s, %d:%s] FAILED "
				"(Error: unable to get an application handler",
				StrId(), sync, SyncStateStr(sync));
		assert(papp);
		return APP_ABORT;
	}

	// Update our state
	SetState(SYNC, sync);

	// Request the application manager to synchronization this application
	// accorting to our new state
	result = am.SyncRequest(papp, sync);
	if (result != ApplicationManager::AM_SUCCESS) {
		logger->Error("Synchronization request FAILED (Error: %d)", result);
		// This is not an error on AWM scheduling but only on the notification
		// of the SynchronizationManager module. The AWM could still be
		// accepted.
	}

	logger->Info("Sync scheduled [%s, %d:%s]",
			StrId(), sync, SyncStateStr(sync));

	return APP_SUCCESS;

}

bool
Application::Reshuffling(AwmPtr_t const & next_awm) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	br::ResourceAssignmentMapPtr_t pumc = _CurrentAWM()->GetResourceBinding();
	br::ResourceAssignmentMapPtr_t puma = next_awm->GetResourceBinding();

	// NOTE: This method is intended to be called if we already know we
	// are in a RECONF state.
	assert(_CurrentAWM()->BindingSet(br::ResourceType::CPU) ==
			    next_awm->BindingSet(br::ResourceType::CPU));
	assert(_CurrentAWM()->Id() == next_awm->Id());

	if (ra.IsReshuffling(pumc, puma)) {
		logger->Notice("AWM Shuffling on [%s]", StrId());
		return true;
	}

	return false;
}

Application::SyncState_t
Application::SyncRequired(AwmPtr_t const & awm) {

	// This must be called only by running applications
	assert(_State() == RUNNING);
	assert(_CurrentAWM().get());

	// Check if the assigned operating point implies RECONF|MIGREC|MIGRATE
	if ((_CurrentAWM()->Id() != awm->Id()) &&
			(_CurrentAWM()->BindingSet(br::ResourceType::CPU) !=
			           awm->BindingSet(br::ResourceType::CPU))) {
		logger->Debug("SynchRequired: [%s] to MIGREC", StrId());
		return MIGREC;
	}

	if ((_CurrentAWM()->Id() == awm->Id()) &&
			(_CurrentAWM()->BindingChanged(br::ResourceType::CPU))) {
		logger->Debug("SynchRequired: [%s] to MIGRATE", StrId());
		return MIGRATE;
	}

	if (_CurrentAWM()->Id() != awm->Id()) {
		logger->Debug("SynchRequired: [%s] to RECONF", StrId());
		return RECONF;
	}

	// Check for inter-cluster resources re-assignement
	if (Reshuffling(awm)) {
		logger->Debug("SynchRequired: [%s] to AWM-RECONF", StrId());
		return RECONF;
	}

	logger->Debug("SynchRequired: [%s] SYNC_NONE", StrId());
	// NOTE: By default no reconfiguration is assumed to be required, thus we
	// return the SYNC_STATE_COUNT which must be read as false values
	return SYNC_NONE;
}

Application::ExitCode_t
Application::Reschedule(AwmPtr_t const & awm) {
	SyncState_t sync;

	// Ready application could be synchronized to start
	if (_State() == READY)
		return RequestSync(STARTING);

	// Otherwise, the application should be running...
	if (_State() != RUNNING) {
		logger->Crit("Rescheduling FAILED (Error: wrong application status "
				"{%s/%s})", StateStr(_State()), SyncStateStr(_SyncState()));
		assert(_State() == RUNNING);
		return APP_ABORT;
	}

	// Checking if a synchronization is required
	sync = SyncRequired(awm);
	if (sync == SYNC_NONE)
		return APP_SUCCESS;

	// Request a synchronization for the identified reconfiguration
	return RequestSync(sync);
}

Application::ExitCode_t Application::Unschedule() {

	// Ready application remain into ready state
	if (_State() == READY)
		return APP_ABORT;

	// Check if the application has been already blocked by a previous failed
	// schedule request
	if (_Blocking())
		return APP_ABORT;

	// Otherwise, the application should be running...
	if (_State() != RUNNING) {
		logger->Crit("Rescheduling FAILED (Error: wrong application status "
				"{%s/%s})", StateStr(_State()), SyncStateStr(_SyncState()));
		assert(_State() == RUNNING);
		return APP_ABORT;
	}

	// The application should be blocked
	return RequestSync(BLOCKED);
}

Application::ExitCode_t Application::ScheduleRequest(AwmPtr_t const & awm,
		br::RViewToken_t status_view, size_t b_refn) {
	std::unique_lock<std::recursive_mutex> schedule_ul(schedule.mtx);
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t booking;
	AppSPtr_t papp(awm->Owner());
	ExitCode_t result;
	logger->Info("ScheduleRequest: %s request for binding @[%d] view=%ld",
		papp->StrId(), b_refn, status_view);

	// App is SYNC/BLOCKED for a previously failed scheduling.
	// Reset state and syncState for this new attempt.
	if (_Blocking()) {
		logger->Warn("ScheduleRequest: request for blocking application");
		SetState(schedule.preSyncState, SYNC_NONE);
	}

	logger->Debug("ScheduleRequest: request for [%s] into AWM [%02d:%s]",
			papp->StrId(), awm->Id(), awm->Name().c_str());

	// Get the working mode pointer
	if (!awm) {
		logger->Crit("ScheduleRequest: request for [%s] FAILED "
				"(Error: AWM not existing)", papp->StrId());
		assert(awm);
		return APP_WM_NOT_FOUND;
	}

	if (_Disabled()) {
		logger->Debug("ScheduleRequest: request for [%s] FAILED "
				"(Error: EXC being disabled)", papp->StrId());
		return APP_DISABLED;
	}

	// Checking for resources availability
	booking = ra.BookResources(papp, awm->GetSchedResourceBinding(b_refn), status_view);

	// If resources are not available, unschedule
	if (booking != ResourceAccounter::RA_SUCCESS) {
		logger->Debug("ScheduleRequest: unscheduling [%s]...", papp->StrId());
		Unschedule();
		return APP_WM_REJECTED;
	}

	// Bind the resource set to the working mode
	awm->SetResourceBinding(status_view, b_refn);

	// Reschedule accordingly to "awm"
	logger->Debug("ScheduleRequest: rescheduling [%s] into AWM [%d:%s]...",
			papp->StrId(), awm->Id(), awm->Name().c_str());
	result = Reschedule(awm);

	// Reschedule failed. Release resources and clear resource binding
	if (result != APP_SUCCESS) {
		ra.ReleaseResources(papp, status_view);
		awm->ClearResourceBinding();
		return APP_WM_REJECTED;
	}

	// Set next awm
	schedule.next_awm = awm;
	awms.curr_inv = false;

	return APP_SUCCESS;
}

/*******************************************************************************
 *  EXC Synchronization
 ******************************************************************************/

Application::ExitCode_t Application::SetRunning() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	SetState(RUNNING);
	++schedule.count;
	logger->Debug("Scheduling count: %" PRIu64 "", schedule.count);
	schedule.awm->IncSchedulingCount();
	return APP_SUCCESS;
}

Application::ExitCode_t Application::SetBlocked() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);
	// If the application as been marked FINISHED, than it is released
	if (_State() == FINISHED)
		return APP_SUCCESS;

	// Otherwise mark it as READY to be re-scheduled when possible
	SetState(READY);
	return APP_SUCCESS;
}

Application::ExitCode_t Application::ScheduleCommit() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);

	// Ignoring applications disabled during a SYNC
	if (_Disabled()) {
		logger->Info("ScheduleCommit: synchronization completed (on disabled EXC)"
			" [%s, %d:%s]",
			StrId(), _State(), StateStr(_State()));
		return APP_SUCCESS;
	}

	assert(_State() == SYNC);

	switch(_SyncState()) {
	case STARTING:
	case RECONF:
	case MIGREC:
	case MIGRATE:
		// Reset GoalGap whether the Application has been scheduled into a AWM
		// having a value higher than the previous one
		if (schedule.awm &&
				(schedule.awm->Value() < schedule.next_awm->Value())) {
			logger->Debug("ScheduleCommit: resetting GoalGap (%d%c) on [%s]",
					ggap_percent, '%', StrId());
			ggap_percent = 0;
		}

		schedule.awm = schedule.next_awm;
		schedule.next_awm.reset();
		SetRunning();
		break;

	case BLOCKED:
		schedule.awm.reset();
		schedule.next_awm.reset();
		SetBlocked();
		break;

	default:
		logger->Crit("ScheduleCommit: synchronization failed for EXC [%s]"
				"(Error: invalid synchronization state)");
		assert(_SyncState() < Application::SYNC_NONE);
		return APP_ABORT;
	}

	logger->Info("ScheduleCommit: synchronization completed [%s, %d:%s]",
			StrId(), _State(), StateStr(_State()));

	return APP_SUCCESS;
}

void Application::ScheduleAbort() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);

	// The abort must be performed only for SYNC App/ExC
	if (!Synching()) {
		logger->Fatal("ScheduleAbort: [%s] in state [%s] (expected SYNC)",
				StrId(), StateStr(State()));
		assert(Synching());
	}

	// Set as READY;
	SetState(READY);

	// Reset working modes settings
	schedule.awm.reset();
	schedule.next_awm.reset();

	logger->Info("ScheduleAbort: completed ");
}

Application::ExitCode_t Application::ScheduleContinue() {
	std::unique_lock<std::recursive_mutex> state_ul(schedule.mtx);

	// Current AWM must be set
	assert(schedule.awm);

	// This must be called only for RUNNING App/ExC
	if (_State() != RUNNING) {
		logger->Error("ScheduleRunning: [%s] is not running. State {%s/%s}",
				StrId(), StateStr(_State()), SyncStateStr(_SyncState()));
		assert(_State() == RUNNING);
		assert(_SyncState() == SYNC_NONE);
		return APP_ABORT;
	}

	// Return if Next AWN is already blank
	if (!schedule.next_awm)
		return APP_SUCCESS;

	// AWM current and next must match
	if (schedule.awm->Id() != schedule.next_awm->Id()) {
		logger->Error("ScheduleRunning: [%s] AWMs differs. "
				"{curr=%d / next=%d}", StrId(),
				schedule.awm->Id(), schedule.next_awm->Id());
		assert(schedule.awm->Id() != schedule.next_awm->Id());
		return APP_ABORT;
	}

	// Reset next AWM (only current must be set)
	schedule.next_awm.reset();

	schedule.awm->IncSchedulingCount();
	return APP_SUCCESS;
}

/*******************************************************************************
 *  EXC Constraints Management
 ******************************************************************************/

Application::ExitCode_t Application::SetWorkingModeConstraint(
		RTLIB_Constraint & constraint) {
	// Get a lock. The assertion may invalidate the current AWM.
	std::unique_lock<std::recursive_mutex> schedule_ul(schedule.mtx);
	ExitCode_t result = APP_ABORT;

	logger->Debug("SetConstraint, AWM_ID: %d, OP: %s, TYPE: %d",
			constraint.awm,
			constraint.operation ? "ADD" : "REMOVE",
			constraint.type);

	// Check the working mode ID validity
	if (constraint.awm > awms.max_id)
		return APP_WM_NOT_FOUND;

	// Dispatch the constraint assertion
	switch (constraint.operation) {
	case CONSTRAINT_REMOVE:
		result = RemoveWorkingModeConstraint(constraint);
		break;
	case CONSTRAINT_ADD:
		result = AddWorkingModeConstraint(constraint);
		break;
	default:
		logger->Error("SetConstraint (AWMs): operation not supported");
		return result;
	}

	// If there are no changes in the enabled list return
	if (result == APP_WM_ENAB_UNCHANGED) {
		logger->Debug("SetConstraint (AWMs): nothing to change");
		return APP_SUCCESS;
	}

	// Rebuild the list of enabled working modes
	RebuildEnabledWorkingModes();

	logger->Debug("SetConstraint (AWMs): %d total working modes",
			awms.recipe_vect.size());
	logger->Debug("SetConstraint (AWMs): %d enabled working modes",
			awms.enabled_list.size());

	DB(DumpValidAWMs());

	return APP_SUCCESS;
}

void Application::DumpValidAWMs() const {
	uint8_t len = 0;
	char buff[256];

	for (int j = 0; j <= awms.max_id; ++j) {
		if (awms.enabled_bset.test(j))
			len += snprintf(buff+len, 265-len, "%d,", j);
	}
	// Remove leading comma
	buff[len-1] = 0;
	logger->Info("SetConstraint (AWMs): enabled map/list = {%s}", buff);
}

Application::ExitCode_t Application::AddWorkingModeConstraint(
		RTLIB_Constraint & constraint) {
	// Check the type of constraint to set
	switch (constraint.type) {
	case RTLIB_ConstraintType::LOWER_BOUND:
		// Return immediately if there is nothing to change
		if (constraint.awm == awms.low_id)
			return APP_WM_ENAB_UNCHANGED;

		// If the lower > upper: upper = max
		if (constraint.awm > awms.upp_id)
			awms.upp_id = awms.max_id;

		// Update the bitmap of the enabled working modes
		SetWorkingModesLowerBound(constraint);
		return APP_WM_ENAB_CHANGED;

	case RTLIB_ConstraintType::UPPER_BOUND:
		// Return immediately if there is nothing to change
		if (constraint.awm == awms.upp_id)
			return APP_WM_ENAB_UNCHANGED;

		// If the upper < lower: lower = begin
		if (constraint.awm < awms.low_id)
			awms.low_id = 0;

		// Update the bitmap of the enabled working modes
		SetWorkingModesUpperBound(constraint);
		return APP_WM_ENAB_CHANGED;

	case RTLIB_ConstraintType::EXACT_VALUE:
		// If the AWM is set yet, return
		if (awms.enabled_bset.test(constraint.awm))
			return APP_WM_ENAB_UNCHANGED;

		// Mark the corresponding bit in the enabled map
		awms.enabled_bset.set(constraint.awm);
		logger->Debug("SetConstraint (AWMs): set exact value AWM {%d}",
				constraint.awm);
		return APP_WM_ENAB_CHANGED;
	}

	return APP_WM_ENAB_UNCHANGED;
}

void Application::SetWorkingModesLowerBound(RTLIB_Constraint & constraint) {
	uint8_t hb = std::max(constraint.awm, awms.low_id);

	// Disable all the AWMs lower than the new lower bound and if the
	// previous lower bound was greater than the new one, enable the AWMs
	// lower than it
	for (int i = hb; i >= 0; --i) {
		if (i < constraint.awm)
			awms.enabled_bset.reset(i);
		else
			awms.enabled_bset.set(i);
	}

	// Save the new lower bound
	awms.low_id = constraint.awm;
	logger->Debug("SetConstraint (AWMs): set lower bound AWM {%d}",
			awms.low_id);
}

void Application::SetWorkingModesUpperBound(RTLIB_Constraint & constraint) {
	uint8_t lb = std::min(constraint.awm, awms.upp_id);

	// Disable all the AWMs greater than the new upper bound and if the
	// previous upper bound was lower than the new one, enable the AWMs
	// greater than it
	for (int i = lb; i <= awms.max_id; ++i) {
		if (i > constraint.awm)
			awms.enabled_bset.reset(i);
		else
			awms.enabled_bset.set(i);
	}

	// Save the new upper bound
	awms.upp_id = constraint.awm;
	logger->Debug("SetConstraint (AWMs): set upper bound AWM {%d}",
			awms.upp_id);
}

Application::ExitCode_t Application::RemoveWorkingModeConstraint(
		RTLIB_Constraint & constraint) {
	// Check the type of constraint to remove
	switch (constraint.type) {
	case RTLIB_ConstraintType::LOWER_BOUND:
		// Set the bit related to the AWM below the lower bound
		ClearWorkingModesLowerBound();
		return APP_WM_ENAB_CHANGED;

	case RTLIB_ConstraintType::UPPER_BOUND:
		// Set the bit related to the AWM above the upper bound
		ClearWorkingModesUpperBound();
		return APP_WM_ENAB_CHANGED;

	case RTLIB_ConstraintType::EXACT_VALUE:
		// If the AWM is not set yet, return
		if (!awms.enabled_bset.test(constraint.awm))
			return APP_WM_ENAB_UNCHANGED;

		// Reset the bit related to the AWM
		awms.enabled_bset.reset(constraint.awm);
		return APP_WM_ENAB_CHANGED;
	}

	return APP_WM_ENAB_UNCHANGED;
}

void Application::ClearWorkingModesLowerBound() {
	// Set all the bit previously unset
	for (int i = awms.low_id - 1; i >= 0; --i)
		awms.enabled_bset.set(i);

	logger->Debug("SetConstraint (AWMs): cleared lower bound AWM {%d}",
			awms.low_id);

	// Reset the lower bound
	awms.low_id = 0;
}

void Application::ClearWorkingModesUpperBound() {
	// Set all the bit previously unset
	for (int i = awms.upp_id + 1; i <= awms.max_id; ++i)
		awms.enabled_bset.set(i);

	logger->Debug("SetConstraint (AWMs): cleared upper bound AWM {%d}",
			awms.upp_id);

	// Reset the upperbound
	awms.upp_id = awms.max_id;
}

void Application::ClearWorkingModeConstraints() {
	// Reset range bounds
	awms.low_id = 0;
	awms.upp_id = awms.max_id;

	// Rebuild the list of enabled working modes
	RebuildEnabledWorkingModes();

	logger->Debug("ClearConstraint (AWMs): %d total working modes",
			awms.recipe_vect.size());
	logger->Debug("ClearConstraint (AWMs): %d enabled working modes",
			awms.enabled_list.size());
}

Application::ExitCode_t Application::SetGoalGap(int percent) {
	// A Goal-Gap could be assigned only for applications already running
	if (State() != RUNNING) {
		logger->Warn("SetGoalGap [%d] on EXC [%s] FAILED "
				"(Error: EXC not running)",
				percent, StrId());
		return APP_ABORT;
	}

	ggap_percent = percent;

	logger->Info("Setting Goal-Gap [%d] for EXC [%s]", ggap_percent, StrId());

	return APP_SUCCESS;
}

void Application::RebuildEnabledWorkingModes() {
	// Clear the list
	awms.enabled_list.clear();

	// Rebuild the enabled working modes list
	for (int j = 0; j <= awms.max_id; ++j) {
		// Skip if the related bit of the map is not set, or one of the
		// resource usage required violates a resource constraint, or
		// the AWM is hidden according to the current status of the hardware
		// resources
		if ((!awms.enabled_bset.test(j))
				|| (UsageOutOfBounds(awms.recipe_vect[j]))
				|| awms.recipe_vect[j]->Hidden())
			continue;

		// Insert the working mode
		awms.enabled_list.push_back(awms.recipe_vect[j]);
	}

	// Check current AWM and re-order the list
	FinalizeEnabledWorkingModes();
}

void Application::FinalizeEnabledWorkingModes() {
	// Check if the current AWM has been invalidated
	if (schedule.awm &&
			!awms.enabled_bset.test(schedule.awm->Id())) {
		logger->Warn("WorkingMode constraints: current AWM (""%s"" ID:%d)"
				" invalidated.", schedule.awm->Name().c_str(),
				schedule.awm->Id());
		awms.curr_inv = true;
	}

	// Sort by working mode "value
	awms.enabled_list.sort(AwmValueLesser);
}

/************************** Resource Constraints ****************************/

bool Application::UsageOutOfBounds(AwmPtr_t & awm) {
	ConstrMap_t::iterator rsrc_constr_it;
	ConstrMap_t::iterator end_rsrc_constr(rsrc_constraints.end());
	br::ResourceAssignmentMap_t::const_iterator usage_it(awm->ResourceRequests().begin());
	br::ResourceAssignmentMap_t::const_iterator end_usage(awm->ResourceRequests().end());

	// Check if there are constraints on the resource assignments
	for (; usage_it != end_usage; ++usage_it) {
		rsrc_constr_it = rsrc_constraints.find(usage_it->first);
		if (rsrc_constr_it == end_rsrc_constr)
			continue;

		// Check if the usage value is out of the constraint bounds
		br::ResourceAssignmentPtr_t const & r_assign(usage_it->second);
		if ((r_assign->GetAmount() < rsrc_constr_it->second->lower) ||
			(r_assign->GetAmount() > rsrc_constr_it->second->upper))
			return true;
	}

	return false;
}

void Application::UpdateEnabledWorkingModes() {
	// Remove AWMs violating resources constraints
	AwmPtrList_t::iterator awms_it(awms.enabled_list.begin());
	for (; awms_it != awms.enabled_list.end(); awms_it++) {
		if (!UsageOutOfBounds(*awms_it))
			continue;
		// This AWM must be removed
		awms.enabled_list.remove(*awms_it);
	}
	// Check current AWM and re-order the list
	FinalizeEnabledWorkingModes();
}

Application::ExitCode_t Application::SetResourceConstraint(
		ResourcePathPtr_t r_path,
		br::ResourceConstraint::BoundType_t b_type,
				uint64_t _value) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());

	// Check the existance of the resource
	if (!ra.ExistResource(r_path)) {
		logger->Warn("SetResourceConstraint: %s not found",
				r_path->ToString().c_str());
		return APP_RSRC_NOT_FOUND;
	}

	// Init a new constraint (if do not exist yet)
	ConstrMap_t::iterator it_con(rsrc_constraints.find(r_path));
	if (it_con == rsrc_constraints.end()) {
		rsrc_constraints.insert(ConstrPair_t(r_path,
					ConstrPtr_t(new br::ResourceConstraint)));
	}

	// Set the constraint bound value (if value exists overwrite it)
	switch(b_type) {
	case br::ResourceConstraint::LOWER_BOUND:
		rsrc_constraints[r_path]->lower = _value;
		if (rsrc_constraints[r_path]->upper < _value)
			rsrc_constraints[r_path]->upper =
				std::numeric_limits<uint64_t>::max();

		logger->Debug("SetConstraint (Resources): Set on {%s} LB = %" PRIu64,
				r_path->ToString().c_str(), _value);
		break;

	case br::ResourceConstraint::UPPER_BOUND:
		rsrc_constraints[r_path]->upper = _value;
		if (rsrc_constraints[r_path]->lower > _value)
			rsrc_constraints[r_path]->lower = 0;

		logger->Debug("SetConstraint (Resources): Set on {%s} UB = %" PRIu64,
				r_path->ToString().c_str(), _value);
		break;
	}

	// Check if there are some AWMs to disable
	UpdateEnabledWorkingModes();

	return APP_SUCCESS;
}

Application::ExitCode_t Application::ClearResourceConstraint(
		ResourcePathPtr_t r_path,
		br::ResourceConstraint::BoundType_t b_type) {
	// Lookup the constraint by resource pathname
	ConstrMap_t::iterator it_con(rsrc_constraints.find(r_path));
	if (it_con == rsrc_constraints.end()) {
		logger->Warn("ClearConstraint (Resources): failed due to unknown "
				"resource path");
		return APP_CONS_NOT_FOUND;
	}

	// Reset the constraint
	switch (b_type) {
	case br::ResourceConstraint::LOWER_BOUND :
		it_con->second->lower = 0;
		if (it_con->second->upper == std::numeric_limits<uint64_t>::max())
			rsrc_constraints.erase(it_con);
		break;

	case br::ResourceConstraint::UPPER_BOUND :
		it_con->second->upper = std::numeric_limits<uint64_t>::max();
		if (it_con->second->lower == 0)
			rsrc_constraints.erase(it_con);
		break;
	}

	// Check if there are some awms to enable
	UpdateEnabledWorkingModes();

	return APP_SUCCESS;
}


uint64_t Application::GetResourceRequestStat(
		std::string const & rsrc_path,
		ResourceUsageStatType_t stats_type) {
	uint64_t min_val  = UINT64_MAX;
	uint64_t max_val  = 0;
	uint64_t total = 0;

	// AWMs (enabled)
	for (auto const & awm: awms.enabled_list) {
		// Resources
		for (auto const & r_entry: awm->ResourceRequests()) {
			ResourcePathPtr_t const & curr_path(r_entry.first);
			uint64_t curr_amount = (r_entry.second)->GetAmount();

			// Is current resource the one required?
			br::ResourcePath key_path(rsrc_path);
			if (key_path.Compare(*(curr_path.get())) ==
					br::ResourcePath::NOT_EQUAL)
				continue;

			// Cumulate the resource usage and update min or max
			total += curr_amount;
			curr_amount < min_val ? min_val = curr_amount: min_val;
			curr_amount > max_val ? max_val = curr_amount: max_val;
		}
	}

	// Return the resource usage statistics required
	switch (stats_type) {
	case RU_STAT_MIN:
		return min_val;
	case RU_STAT_AVG:
		return total / awms.enabled_list.size();
	case RU_STAT_MAX:
		return max_val;
	};

	return 0;
}

} // namespace app

} // namespace bbque

