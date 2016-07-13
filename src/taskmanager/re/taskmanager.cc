#include <base/printf.h>
#include <stdint.h>
#include <timer_session/connection.h>
#include <base/env.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>

#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <os/config.h>
#include <base/sleep.h>

static char const *state_name(Genode::Trace::CPU_info::State state)
{
	switch (state) {
	case Genode::Trace::CPU_info::INVALID:  return "INVALID";
	case Genode::Trace::CPU_info::UNTRACED: return "UNTRACED";
	case Genode::Trace::CPU_info::TRACED:   return "TRACED";
	case Genode::Trace::CPU_info::FOREIGN:  return "FOREIGN";
	case Genode::Trace::CPU_info::ERROR:    return "ERROR";
	case Genode::Trace::CPU_info::DEAD:     return "DEAD";
	}
	return "undefined";
}

using namespace Genode;


class Trace_buffer_monitor
{
	private:

		enum { MAX_ENTRY_BUF = 256 };
		char                  _buf[MAX_ENTRY_BUF];

		Trace::Subject_id     _id;
		Trace::Buffer        *_buffer;
		Trace::Buffer::Entry _curr_entry;

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';

			return _buf;
		}

	public:

		Trace_buffer_monitor(Trace::Subject_id id, Dataspace_capability ds_cap)
		:
			_id(id),
			_buffer(env()->rm_session()->attach(ds_cap)),
			_curr_entry(_buffer->first())
		{
			PLOG("monitor subject:%d buffer:0x%lx", _id.id, (addr_t)_buffer);
		}

		~Trace_buffer_monitor()
		{
			if (_buffer)
				env()->rm_session()->detach(_buffer);
		}

		Trace::Subject_id id() { return _id; };

		void dump()
		{
			PLOG("overflows: %u", _buffer->wrapped());

			PLOG("read all remaining events");
			for (; !_curr_entry.is_last(); _curr_entry = _buffer->next(_curr_entry)) {
				/* omit empty entries */
				if (_curr_entry.length() == 0)
					continue;

				const char *data = _terminate_entry(_curr_entry);
				if (data)
					PLOG("%s", data);
			}

			/* reset after we read all available entries */
			_curr_entry = _buffer->first();
		}
};


int trace()
{
	using namespace Genode;

	static Genode::Trace::Connection trace1(1024*4096, 64*4096, 0);

	static Timer::Connection timer;

	static Trace_buffer_monitor *test_monitor[3] = {0,0,0};

	Genode::Trace::Policy_id policy_id;
	bool                     policy_set[10]={false,false,false,false,false,false,false,false,false,false};

	char                     policy_label[64][100];
	int			 policy_counter=0;
	char                     policy_module[64];
	Rom_dataspace_capability policy_module_rom_ds;

	Trace::CPU_info last_step[32];

	Trace::Subject_id subjects[32];
	
	size_t last_step_num_subjects = trace1.subjects(subjects, 32);

	for (size_t i = 0; i < last_step_num_subjects; i++) {
		last_step[i] = trace1.cpu_info(subjects[i]);
	}
	
	try {
		Xml_node policy = config()->xml_node().sub_node("trace_policy");
		for (;; policy = policy.next("trace_policy")) {
			try {
				policy.attribute("label").value(policy_label[policy_counter], sizeof (policy_label[0]));
				policy.attribute("module").value(policy_module, sizeof (policy_module));

				static Rom_connection policy_rom(policy_module);
				policy_module_rom_ds = policy_rom.dataspace();

				size_t rom_size = Dataspace_client(policy_module_rom_ds).size();

				policy_id = trace1.alloc_policy(rom_size);
				Dataspace_capability ds_cap = trace1.policy(policy_id);

				if (ds_cap.valid()) {
					void *ram = env()->rm_session()->attach(ds_cap);
					void *rom = env()->rm_session()->attach(policy_module_rom_ds);
					memcpy(ram, rom, rom_size);

					env()->rm_session()->detach(ram);
					env()->rm_session()->detach(rom);
				}
			} catch (...) {
				PERR("could not load module '%s' for label '%s'", policy_module, policy_label[policy_counter]);
			}

			PINF("load module: '%s' for label: '%s'", policy_module, policy_label[policy_counter]);
			
			policy_counter++;

			if (policy.is_last("trace_policy")) break;
		}

	} catch (...) { }

	while(true) {

		int timestamp = 1000;

		timer.msleep(timestamp);

		Trace::Subject_id subjects[32];
		size_t num_subjects = trace1.subjects(subjects, 32);

		Trace::RAM_info init;

		unsigned long long idle;

		printf("%zd tracing subjects present\n", num_subjects);

		for (size_t i = 0; i < num_subjects; i++) {
			Trace::CPU_info info = trace1.cpu_info(subjects[i]);
			Trace::RAM_info ram_info = trace1.ram_info(subjects[i]);
			if(strcmp(info.session_label().string(), "init")!=0&&strcmp(info.session_label().string(), "init -> idle")!=0) {
			printf("ID:%d %lld prio:%d %s name:%s %dKB %dKB\n",
			       subjects[i].id,
			       (info.execution_time().value-last_step[i].execution_time().value)/(timestamp*10),
				info.prio(), 
			       info.session_label().string(),
				info.thread_name().string(),
				ram_info.ram_quota()/1024,
				ram_info.ram_used()/1024);
			}
			if(strcmp(info.session_label().string(), "init")==0) {
				init=ram_info;
			}
			if(strcmp(info.thread_name().string(), "idle")==0) {
				idle=100-((info.execution_time().value-last_step[i].execution_time().value)/(timestamp*10));
			}

			last_step[i]=info;

			/* enable tracing */
			
			/*for(int j=0; j<policy_counter; j++) {

			num_subjects = trace1.subjects(subjects, 32);

			if (!policy_set[j] && 
				strcmp(info.session_label().string(), policy_label[j]) == 0
			    && (
			       strcmp(info.thread_name().string(), "taskmanager") == 0)
			    ) {
				try {
					PINF("enable tracing for thread:'%s' with policy:%d",
					     info.thread_name().string(), policy_id.id);

					trace1.trace(subjects[i].id, policy_id, 16384U);

					Dataspace_capability ds_cap = trace1.buffer(subjects[i].id);
					test_monitor[j] = new (env()->heap()) Trace_buffer_monitor(subjects[i].id, ds_cap);

				} catch (Trace::Source_is_dead) { PERR("source is dead"); }

				policy_set[j] = true;
			}
			}*/

			/* read events from trace buffer */
			
			/*for(int k=0; k<policy_counter; k++){
				if (test_monitor[k]) {
					if (subjects[i].id == test_monitor[k]->id().id)
						test_monitor[k]->dump();
				}
			}*/
		}

		printf("Load:%lld RAM avail:%dMB\n", idle, init.ram_quota()/1048576);
	}
	/*for(int k=0; k<policy_counter; k++){
		if (test_monitor[k])
			destroy(env()->heap(), test_monitor[k]);
	}*/

	printf("--- test-trace finished ---\n");
	return 0;
}

int main(void)
{
		trace();
}


























