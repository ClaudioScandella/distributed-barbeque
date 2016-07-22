/*
 * Copyright (C) 2014  Politecnico di Milano
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

#include "agent_proxy.h"
#include "agent_proxy_grpc_plugin.h"
#include "bbque/plugins/static_plugin.h"

namespace bp = bbque::plugins;

extern "C"
int32_t StaticPlugin_AgentProxyGRPC_exitFunc()
{
	return 0;
}

extern "C"
PF_ExitFunc StaticPlugin_AgentProxyGRP_initPlugin(const PF_PlatformServices * params)
{
	int res = 0;

	PF_RegisterParams rp;
	rp.version.major = 1;
	rp.version.minor = 0;
	rp.programming_language = PF_LANG_CPP;

	// Registering the module
	rp.CreateFunc  = bp::AgentProxyGRPC::Create;
	rp.DestroyFunc = bp::AgentProxyGRPC::Destroy;
	res = params->RegisterObject((const char *) MODULE_NAMESPACE, &rp);
	if (res < 0)
		return NULL;


	return StaticPlugin_AgentProxyGRPC_exitFunc;
}

PLUGIN_INIT(StaticPlugin_AgentProxyGRPC_initPlugin);

