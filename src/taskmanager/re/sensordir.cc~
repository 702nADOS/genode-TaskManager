/*
 * \brief  Main program of the Hello server
 * \author Björn Döbel
 * \date   2008-03-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <sensordir_session/sensordir_session.h>
#include <base/rpc_server.h>
#include <dataspace/client.h>
#include <sensors/scalar.h>
#include <sensordir.h>

#include <sensordir_session/client.h>
#include <sensordir_session/connection.h>

Genode::Dataspace_capability ds [100];
int numberOfSensors = 0;

Genode::Dataspace_capability _create(uint16_t typ) {
				Genode::Dataspace_capability s1_cap = Genode::env()->ram_session()->alloc(sizeof(ferret_scalar_t));
				ds [numberOfSensors]=s1_cap;
				numberOfSensors++;
				ferret_scalar_t *f1 = Genode::env()->rm_session()->attach(s1_cap);
				f1->header.ds_cap=s1_cap;
				return s1_cap;
}

Genode::Dataspace_capability _lookup(uint16_t maj,  uint16_t min,  uint16_t inst) {
	bool found=true;
	int counter=0;
	while(found){
		Genode::Dataspace_capability s1_cap=ds [counter];
		ferret_scalar_t *f1 = Genode::env()->rm_session()->attach(s1_cap);
		if(f1) {
			if(f1->header.major==maj&&f1->header.minor==min&&f1->header.instance==inst) {
				found=false;
				return s1_cap;
			}
		}
		counter++;
	}
}

namespace Ferret {
	struct Session_component : Genode::Rpc_object<Session> 
	{
		Genode::Dataspace_capability create(uint16_t typ) {
			Genode::Dataspace_capability s1_cap=_create(typ);
			Genode::env()->rm_session()->attach(s1_cap);
			return s1_cap;
		}
		Genode::Dataspace_capability lookup(uint16_t maj,  uint16_t min,  uint16_t inst) {
			Genode::Dataspace_capability s1_cap=_lookup(maj, min, inst);
			Genode::env()->rm_session()->attach(s1_cap);
			return s1_cap;
		}
	};

	class Root_component : public Genode::Root_component<Session_component>
	{
		protected:

			Ferret::Session_component *_create_session(const char *args)
			{
				return new (md_alloc()) Session_component();
			}

		public:

			Root_component(Genode::Rpc_entrypoint *ep,
			               Genode::Allocator *allocator)
			: Genode::Root_component<Session_component>(ep, allocator)
			{				
			}
	};
}


using namespace Genode;



int main(void)
{	
	
	Cap_connection cap;

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "ferret_ep");
	static Ferret::Root_component ferret_root(&ep, &sliced_heap);
	env()->parent()->announce(ep.manage(&ferret_root));	
	
	sleep_forever();
		
	return 0;
}




























