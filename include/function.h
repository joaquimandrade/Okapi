
#ifndef ___FUNC_TION___
#define ___FUNC_TION___

#include "amxxmodule.h"
#include <CVector.h>
#include <type_handler.h>
#include <allocator.h>
#include <memory_.h>

enum OkapiRet
{
	OkapiRetIgnore,
	OkapiRetOverride,
	OkapiRetSupercede
};

class Function;

struct AMX_Hook
{
	Function* function;
	int amx_hook;
	int phase;

	AMX_Hook(Function* function,int amx_hook,int phase) : function(function), amx_hook(amx_hook), phase(phase){}

	~AMX_Hook()
	{
		MF_UnregisterSPForward(amx_hook);
	}
};

class Function
{
	static CVector<Function*> functions;
	static CVector<AMX_Hook*> hooks_stack;

	static long Gate(void* ebp,void* eip,Function* func, void** stack,...);
	static float GateFloat(void* ebp,void* eip,Function* func, void** stack,...);

protected:

	void* address;
	unsigned char* trampoline;
	CVector<TypeHandler*> arguments_handlers;
	TypeHandler* return_handler;

	CVector<AMX_Hook*> amx_hooks[2];

	void** stack;
	
	long current_ret_data;
	long original_ret_data;

	cell* stack_amxx;

	unsigned char* original_code;

	bool call_hooks;

	Allocator allocator;

	Function(void* address,CVector<TypeHandler*> arguments_handlers,TypeHandler* return_handler);

	virtual ~Function()
	{
		delete[] stack_amxx;

		delete[] trampoline;
	}

	virtual unsigned char* create_trampoline() = 0;

	void clean_hooks()
	{
		for(int i=0;i<2;i++)
		{
			CVector<AMX_Hook*>& amx_hooks_phase = amx_hooks[i];

			for(size_t j=0;j<amx_hooks_phase.size();j++)
			{
				AMX_Hook* amx_hook = amx_hooks_phase[j];
				delete amx_hook;
			}

			amx_hooks_phase.clear();
		}
	}

	void** get_stack_for_arg(int n)
	{
		CVector<TypeHandler*>& handlers = this->arguments_handlers;

		int offset = 0;

		for(size_t i=0;i<(size_t)n;i++)
		{
			TypeHandler*& handler = handlers[i];
			offset += handler->stack_places();
		}

		return &stack[offset];
	}

	int get_stack_places()
	{
		int n = 0;

		CVector<TypeHandler*>& handlers = this->arguments_handlers;

		for(size_t i=0;i<handlers.size();i++)
		{
			TypeHandler*& handler = handlers[i];

			n += handler->stack_places();
		}

		return n;
	}

	void convert_to_amxx(void** stack)
	{
		CVector<TypeHandler*>& handlers = this->arguments_handlers;

		int offset = 0;

		for(size_t i=0;i<handlers.size();i++)
		{
			TypeHandler*& handler = handlers[i];

			handler->convert_to_amxx(allocator,&stack[offset],this->stack_amxx[i]);

			offset += handler->stack_places();
		}
	}

	long call(void** stack)
	{
		this->stack = stack;
		this->current_ret_data = 0;
		this->original_ret_data = 0;

		if(call_hooks)
		{
			convert_to_amxx(stack);
		}

		OkapiRet amxx_ret = OkapiRetIgnore;

		if(call_hooks)
		{
			amxx_ret = call_amxx_hooks(0);
		}

		if(amxx_ret != OkapiRetSupercede)
		{
			this->original_ret_data = call_original(stack);
		}

		if(amxx_ret == OkapiRetIgnore)
		{
			this->current_ret_data = this->original_ret_data;
		}

		if(call_hooks && (amxx_ret != OkapiRetSupercede))
		{
			call_amxx_hooks(1);
		}

		this->stack = NULL;
		allocator.clear();

		return this->current_ret_data;
	}

	long virtual call_original(void** stack) = 0;

	unsigned char* create_trampoline_generic(int stack_fix,bool pass_ecx, bool handle_jump = 1);

	OkapiRet call_amxx_hook(int hook);

	OkapiRet call_amxx_hooks(int phase)
	{
		OkapiRet ret = OkapiRetIgnore;
		
		CVector<AMX_Hook*> hooks = this->amx_hooks[phase];
		
		for(size_t i=0;i<hooks.size();i++)
		{
			AMX_Hook* hook = hooks[i];

			hooks_stack.push_back(hook);

			OkapiRet ret_hook = call_amxx_hook(hook->amx_hook);

			ret = (OkapiRet) max(ret,ret_hook);

			hooks_stack.pop_back();

			if(ret == OkapiRetSupercede)
			{
				return ret;
			}
		}
		
		return ret;
	}

public:

	AMX_Hook* add_hook(AMX* amx, char* callback, int phase);
	void del_hook(AMX_Hook* hook);

	static void clean()
	{
		clean_all_hooks();

		for(size_t i=0;i<functions.size();i++)
		{
			Function* function = functions[i];

			delete function;
		}

		functions.clear();
	}

	static void clean_all_hooks()
	{
		for(size_t i=0;i<functions.size();i++)
		{
			Function* function = functions[i];

			function->clean_hooks();
		}
	}

	static AMX_Hook* get_current_hook()
	{
		if(!hooks_stack.size())
			return NULL;

		return hooks_stack[hooks_stack.size()-1];
	}

	int get_n_args()
	{
		return this->arguments_handlers.size();
	}

	void set_arg(int n,AMX* amx,cell param)
	{
		void** stack_arg = get_stack_for_arg(n);

		this->arguments_handlers[n]->convert_from_amxx(allocator,stack_arg,amx,param);
	}

	void set_return(AMX *amx, cell *params)
	{
		this->current_ret_data = this->return_handler->convert_from_amxx(allocator,amx,params);
	}

	int get_n_ret_params()
	{
		return this->return_handler->by_ref_n_params();
	}
	
	long get_origin_return(AMX *amx, cell *params)
	{
		return this->return_handler->convert_to_amxx(amx,params,this->original_ret_data);
	}

	int call_n_params()
	{
		return this->arguments_handlers.size() + this->return_handler->by_ref_n_params();
	}

	virtual long call_amx(AMX* amx, cell* params,bool call_hooks)
	{
		Allocator allocator_c;

		int stack_places = this->get_stack_places();

		void** stack_call = allocator_c.alloc<void*>(stack_places);

		CVector<TypeHandler*>& handlers = this->arguments_handlers;

		int offset = 0;

		for(size_t i=0;i<handlers.size();i++)
		{
			TypeHandler*& handler = handlers[i];

			handler->convert_from_amxx(allocator_c,&stack_call[offset],amx,params[i]);

			offset += handler->stack_places();
		}

		this->call_hooks = call_hooks;

		long ret = this->call_original(stack_call);

		this->call_hooks = true;

		return return_handler->convert_to_amxx(amx,&params[handlers.size()],ret);
	}
};

class FunctionMethod : public Function
{
		int get_stack_dislocation();

	protected:

		unsigned char* create_trampoline();

		long call_original(void** stack);

	public:
	
		FunctionMethod(void* address,CVector<TypeHandler*> arguments_handlers,TypeHandler* return_handler);

		~FunctionMethod()
		{
			memcpy(address,original_code,8);

			delete[] original_code;
		}
};

class FunctionCdecl : public Function
{
		int get_stack_dislocation();

	protected:

		unsigned char* create_trampoline();

		long call_original(void** stack);

	public:
	
		FunctionCdecl(void* address,CVector<TypeHandler*> arguments_handlers,TypeHandler* return_handler);

		~FunctionCdecl()
		{
			memcpy(address,original_code,8);

			delete[] original_code;
		}
};


class FunctionVirtual : public Function
{
		int get_stack_dislocation()
		{
			int dislocation = 0;

		#ifdef __linux__
			size_t i = 0;
		#else
			size_t i = 1;
		#endif

			for(;i<this->arguments_handlers.size();i++)
			{
				dislocation += arguments_handlers[i]->stack_places() * sizeof(long);
			}

			return dislocation;
		}

		void* original_address;

	protected:

		unsigned char* create_trampoline()
		{
		#ifdef __linux__
			int stack_fix = 0;
		#else
			int stack_fix = this->get_stack_dislocation();
		#endif
	
		#ifdef __linux__
			bool pass_ecx = 0;
		#else
			bool pass_ecx = 1;
		#endif

			return this->create_trampoline_generic(stack_fix,pass_ecx,false);
		}

		long call_amx(AMX* amx, cell* params,bool call_hooks);

		long call_original(void** stack);

	public:
	
		FunctionVirtual(void* address,CVector<TypeHandler*> arguments_handlers,TypeHandler* return_handler) : Function(address,arguments_handlers,return_handler)
		{
			original_address = *((void**)address);

			original_code = NULL;

			this->trampoline = this->create_trampoline();

			{
				Memory m;

				int prot = m.get_memory_protection((long)address);

				m.make_writable((long)address,4);

				*((void**)address) = this->trampoline;

				m.set_memory_protection((long)address,prot);
			}
		}

		~FunctionVirtual()
		{
			Memory m;

			int prot = m.get_memory_protection((long)address);

			m.make_writable((long)address,4);

			*((void**)address) = original_address;

			m.set_memory_protection((long)address,prot);
		}
};

#endif