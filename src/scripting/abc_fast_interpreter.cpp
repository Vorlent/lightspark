/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "abc.h"
#include "compat.h"
#include "exceptions.h"
#include "abcutils.h"
#include <string>
#include <sstream>
#include "scripting/class.h"

using namespace std;
using namespace lightspark;

struct OpcodeData
{
	union
	{
		int32_t ints[0];
		uint32_t uints[0];
		double doubles[0];
		ASObject* objs[0];
		const multiname* names[0];
		const Type* types[0];
	};
};

ASObject* ABCVm::executeFunctionFast(const SyntheticFunction* function, call_context* context, ASObject *caller)
{
	method_info* mi=function->mi;

	const char* const code=&(mi->body->code[0]);
	//This may be non-zero and point to the position of an exception handler

#if defined (PROFILING_SUPPORT) || !defined(NDEBUG)
	const uint32_t code_len=mi->body->code.size();
#endif
	uint32_t instructionPointer=context->exec_pos;

#ifdef PROFILING_SUPPORT
	if(mi->profTime.empty())
		mi->profTime.resize(code_len,0);
	uint64_t startTime=compat_get_thread_cputime_us();
#define PROF_ACCOUNT_TIME(a, b)  do{a+=b;}while(0)
#define PROF_IGNORE_TIME(a) do{ a; } while(0)
#else
#define PROF_ACCOUNT_TIME(a, b) do{ ; }while(0)
#define PROF_IGNORE_TIME(a) do{ ; } while(0)
#endif

	//Each case block builds the correct parameters for the interpreter function and call it
	while(1)
	{
		assert(instructionPointer<code_len);
		uint8_t opcode=code[instructionPointer];
		//Save ip for exception handling in SyntheticFunction::callImpl
		context->exec_pos = instructionPointer;
		instructionPointer++;
		const OpcodeData* data=reinterpret_cast<const OpcodeData*>(code+instructionPointer);

		switch(opcode)
		{
			case 0x01:
			{
				//bkpt
				LOG_CALL( _("bkpt") );
				break;
			}
			case 0x02:
			{
				//nop
				break;
			}
			case 0x03:
			{
				//throw
				_throw(context);
				break;
			}
			case 0x04:
			{
				//getsuper
				getSuper(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x05:
			{
				//setsuper
				setSuper(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x06:
			{
				//dxns
				dxns(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x07:
			{
				//dxnslate
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v,function->getSystemState());
				dxnslate(context, v);
				break;
			}
			case 0x08:
			{
				//kill
				uint32_t t=data->uints[0];
				LOG_CALL( "kill " << t);
				instructionPointer+=4;
				context->locals[t]=asAtom::fromObject(function->getSystemState()->getUndefinedRef());
				break;
			}
			case 0x0c:
			{
				//ifnlt
				uint32_t dest=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifNLT(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x0d:
			{
				//ifnle
				uint32_t dest=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifNLE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x0e:
			{
				//ifngt
				uint32_t dest=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifNGT(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x0f:
			{
				//ifnge
				uint32_t dest=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifNGE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x10:
			{
				//jump
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				assert(dest < code_len);
				instructionPointer=dest;
				break;
			}
			case 0x11:
			{
				//iftrue
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				bool cond=ifTrue(v1);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x12:
			{
				//iffalse
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				bool cond=ifFalse(v1);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x13:
			{
				//ifeq
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifEq(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x14:
			{
				//ifne
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifNE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x15:
			{
				//iflt
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifLT(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x16:
			{
				//ifle
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifLE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x17:
			{
				//ifgt
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifGT(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x18:
			{
				//ifge
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifGE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x19:
			{
				//ifstricteq
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifStrictEq(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x1a:
			{
				//ifstrictne
				uint32_t dest=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				bool cond=ifStrictNE(v1, v2);
				if(cond)
				{
					assert(dest < code_len);
					instructionPointer=dest;
				}
				break;
			}
			case 0x1b:
			{
				//lookupswitch
				uint32_t defaultdest=data->uints[0];
				LOG_CALL(_("Switch default dest ") << defaultdest);
				uint32_t count=data->uints[1];

				RUNTIME_STACK_POP_CREATE_REF(context,index_obj);
				assert_and_throw(index_obj->getObject()->getObjectType()==T_INTEGER);
				unsigned int index=index_obj->toUInt();

				uint32_t dest=defaultdest;
				if(index<=count)
					dest=data->uints[2+index];

				assert(dest < code_len);
				instructionPointer=dest;
				break;
			}
			case 0x1c:
			{
				//pushwith
				pushWith(context);
				break;
			}
			case 0x1d:
			{
				//popscope
				popScope(context);
				break;
			}
			case 0x1e:
			{
				//nextname
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				asAtomR name = nextName(v1,v2);
				runtime_stack_push_ref(context, name);
				break;
			}
			case 0x1f:
			{
				//hasnext
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				asAtomR name = nextName(v1,v2);
				runtime_stack_push_ref(context, name);
				break;
			}
			case 0x20:
			{
				//pushnull
				asAtomR value = asAtom::fromObject(pushNull());
				runtime_stack_push_ref(context,value);
				break;
			}
			case 0x21:
			{
				//pushundefined
				asAtomR value = asAtom::fromObject(pushUndefined());
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x23:
			{
				//nextvalue
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				asAtomR name = nextName(v1,v2);
				runtime_stack_push_ref(context, name);
				break;
			}
			case 0x24:
			{
				//pushbyte
				int8_t t=code[instructionPointer];
				instructionPointer++;
				asAtomR value = _MAR(asAtom((int32_t)t));
				runtime_stack_push_ref(context, value);
				pushByte(t);
				break;
			}
			case 0x25:
			{
				//pushshort
				// specs say pushshort is a u30, but it's really a u32
				// see https://bugs.adobe.com/jira/browse/ASC-4181
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				asAtomR value = _MAR(asAtom((int32_t)t));
				runtime_stack_push_ref(context, value);
				pushShort(t);
				break;
			}
			case 0x26:
			{
				//pushtrue
				RUNTIME_STACK_PUSH(context,asAtom::trueAtom);
				break;
			}
			case 0x27:
			{
				//pushfalse
				RUNTIME_STACK_PUSH(context,asAtom::falseAtom);
				break;
			}
			case 0x28:
			{
				//pushnan
				RUNTIME_STACK_PUSH(context,asAtom(Number::NaN));
				break;
			}
			case 0x29:
			{
				//pop
				pop();
				RUNTIME_STACK_POP_CREATE_REF(context,o);
				break;
			}
			case 0x2a:
			{
				//dup
				dup();
				RUNTIME_STACK_POP_CREATE_REF(context,o);
				runtime_stack_push_ref(context,o);
				runtime_stack_push_ref(context,o);
				break;
			}
			case 0x2b:
			{
				//swap
				swap();
				RUNTIME_STACK_POP_CREATE(context,v1);
				RUNTIME_STACK_POP_CREATE(context,v2);

				RUNTIME_STACK_PUSH(context,v1);
				RUNTIME_STACK_PUSH(context,v2);
				break;
			}
			case 0x2c:
			{
				//pushstring
				asAtomR value = asAtom::fromObject(pushString(context,data->uints[0]));
				runtime_stack_push_ref(context, value);
				instructionPointer+=4;
				break;
			}
			case 0x2d:
			{
				//pushint
				int32_t t=data->ints[0];
				instructionPointer+=4;
				pushInt(context, t);
				RUNTIME_STACK_PUSH(context,asAtom(t));
				break;
			}
			case 0x2e:
			{
				//pushuint
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				pushUInt(context, t);

				RUNTIME_STACK_PUSH(context,asAtom(t));
				break;
			}
			case 0x2f:
			{
				//pushdouble
				double t=data->doubles[0];
				instructionPointer+=8;
				pushDouble(context, t);

				ASObject* d=abstract_d(function->getSystemState(),t);
				RUNTIME_STACK_PUSH(context,asAtom(d));
				break;
			}
			case 0x30:
			{
				//pushscope
				pushScope(context);
				break;
			}
			case 0x31:
			{
				//pushnamespace
				asAtomR value = asAtom::fromObject(pushNamespace(context, data->uints[0]) );
				runtime_stack_push_ref(context, value);
				instructionPointer+=4;
				break;
			}
			case 0x32:
			{
				//hasnext2
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				instructionPointer+=8;

				bool ret=hasNext2(context,t,t2);
				RUNTIME_STACK_PUSH(context,asAtom(ret));
				break;
			}
			//Alchemy opcodes
			case 0x35:
			{
				//li8
				LOG_CALL( "li8");
				loadIntN<uint8_t>(context);
				break;
			}
			case 0x36:
			{
				//li16
				LOG_CALL( "li16");
				loadIntN<uint16_t>(context);
				break;
			}
			case 0x37:
			{
				//li32
				LOG_CALL( "li32");
				loadIntN<uint32_t>(context);
				break;
			}
			case 0x38:
			{
				//lf32
				LOG_CALL( "lf32");
				loadFloat(context);
				break;
			}
			case 0x39:
			{
				//lf32
				LOG_CALL( "lf64");
				loadDouble(context);
				break;
			}
			case 0x3a:
			{
				//si8
				LOG_CALL( "si8");
				storeIntN<uint8_t>(context);
				break;
			}
			case 0x3b:
			{
				//si16
				LOG_CALL( "si16");
				storeIntN<uint16_t>(context);
				break;
			}
			case 0x3c:
			{
				//si32
				LOG_CALL( "si32");
				storeIntN<uint32_t>(context);
				break;
			}
			case 0x3d:
			{
				//sf32
				LOG_CALL( "sf32");
				storeFloat(context);
				break;
			}
			case 0x3e:
			{
				//sf32
				LOG_CALL( "sf64");
				storeDouble(context);
				break;
			}
			case 0x40:
			{
				//newfunction
				asAtomR value = asAtom::fromObject(newFunction(context,data->uints[0]));
				runtime_stack_push_ref(context, value);
				instructionPointer+=4;
				break;
			}
			case 0x41:
			{
				//call
				uint32_t t=data->uints[0];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				call(context,t,&called_mi);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=4;
				break;
			}
			case 0x42:
			{
				//construct
				construct(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x43:
			{
				//callmethod
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				callMethod(context,t,t2);
				instructionPointer+=8;
				break;
			}
			case 0x44:
			{
				//callstatic
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callStatic(context,t,t2,&called_mi,true);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				break;
			}
			case 0x45:
			{
				//callsuper
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callSuper(context,t,t2,&called_mi,true);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=8;
				break;
			}
			case 0x46:
			{
				//callproperty
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callProperty(context,t,t2,&called_mi,true);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=8;
				break;
			}
			case 0x4c: 
			{
				//callproplex
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callProperty(context,t,t2,&called_mi,true);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=8;
				break;
			}
			case 0x47:
			{
				//returnvoid
				LOG_CALL(_("returnVoid"));
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				return NULL;
			}
			case 0x48:
			{
				//returnvalue
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,ret,function->getSystemState());
				LOG_CALL(_("returnValue ") << ret);
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				return ret;
			}
			case 0x49:
			{
				//constructsuper
				constructSuper(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x4a:
			{
				//constructprop
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				instructionPointer+=8;
				constructProp(context,t,t2);
				break;
			}
			case 0x4e:
			{
				//callsupervoid
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callSuper(context,t,t2,&called_mi,false);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=8;
				break;
			}
			case 0x4f:
			{
				//callpropvoid
				uint32_t t=data->uints[0];
				uint32_t t2=data->uints[1];
				method_info* called_mi=NULL;
				PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
				callProperty(context,t,t2,&called_mi,false);
				if(called_mi)
					PROF_ACCOUNT_TIME(mi->profCalls[called_mi],profilingCheckpoint(startTime));
				else
					PROF_IGNORE_TIME(profilingCheckpoint(startTime));
				instructionPointer+=8;
				break;
			}
			case 0x50:
			{
				//sxi1
				LOG_CALL( "sxi1");
				RUNTIME_STACK_POP_CREATE_REF(context,arg1);
				int32_t ret=arg1->toUInt() & 0x1;
				RUNTIME_STACK_PUSH(context,asAtom(ret));
				break;
			}
			case 0x51:
			{
				//sxi8
				LOG_CALL( "sxi8");
				RUNTIME_STACK_POP_CREATE_REF(context,arg1);
				int32_t ret=(int8_t)arg1->toUInt();
				RUNTIME_STACK_PUSH(context,asAtom(ret));
				break;
			}
			case 0x52:
			{
				//sxi16
				LOG_CALL( "sxi16");
				RUNTIME_STACK_POP_CREATE_REF(context,arg1);
				int32_t ret=(int16_t)arg1->toUInt();
				RUNTIME_STACK_PUSH(context,asAtom(ret));
				break;
			}
			case 0x53:
			{
				//constructgenerictype
				constructGenericType(context, data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x55:
			{
				//newobject
				newObject(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x56:
			{
				//newarray
				newArray(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x57:
			{
				//newactivation
				asAtomR value = asAtom::fromObject(newActivation(context, mi));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x58:
			{
				//newclass
				newClass(context,data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x59:
			{
				//getdescendants
				getDescendants(context, data->uints[0]);
				instructionPointer+=4;
				break;
			}
			case 0x5a:
			{
				//newcatch
				asAtomR value = asAtom::fromObject(newCatch(context,data->uints[0]));
				runtime_stack_push_ref(context, value);
				instructionPointer+=4;
				break;
			}
			case 0x5d:
			{
				//findpropstrict
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,context);
				asAtomR value = findPropStrictAtom(context,name);
				runtime_stack_push_ref(context, value);
				name->resetNameIfObject();
				break;
			}
			case 0x5e:
			{
				//findproperty
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,context);
				asAtomR value = findPropertyAtom(context,name);
				runtime_stack_push_ref(context, value);
				name->resetNameIfObject();
				break;
			}
			case 0x5f:
			{
				//finddef
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,context);
				LOG(LOG_NOT_IMPLEMENTED,"opcode 0x5f (finddef) not implemented:"<< *name);
				asAtomR value = asAtom::fromObject(pushNull());
				runtime_stack_push_ref(context, value);
				name->resetNameIfObject();
				break;
			}
			case 0x60:
			{
				//getlex
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				getLex(context,t);
				break;
			}
			case 0x61:
			{
				//setproperty
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,value,function->getSystemState());

				multiname* name=context->context->getMultiname(t,context);

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,obj,function->getSystemState());

				setProperty(value,obj,name);
				name->resetNameIfObject();
				break;
			}
			case 0x62:
			{
				//getlocal
				uint32_t i=data->uints[0];
				instructionPointer+=4;
				if (context->locals[i]->type == T_INVALID)
				{
					LOG_CALL( _("getLocal ") << i << " not set, pushing Undefined");
					asAtomR value = asAtom::fromObject(function->getSystemState()->getUndefinedRef());
					runtime_stack_push_ref(context, value);
					break;
				}
				LOG_CALL( _("getLocal ") << i << _(": ") << context->locals[i].toDebugString() );
				runtime_stack_push_ref(context,context->locals[i]);
				break;
			}
			case 0x63:
			{
				//setlocal
				uint32_t i=data->uints[0];
				instructionPointer+=4;
				LOG_CALL( _("setLocal ") << i );
				RUNTIME_STACK_POP_CREATE_REF(context,obj)
				if ((int)i != context->argarrayposition || obj->type == T_ARRAY)
				{
					context->locals[i]=obj;
				}
				break;
			}
			case 0x64:
			{
				//getglobalscope
				asAtomR value = asAtom::fromObject(getGlobalScope(context));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x65:
			{
				//getscopeobject
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				assert_and_throw(context->curr_scope_stack > t);
				asAtomR ret=context->scope_stack[t];
				LOG_CALL( _("getScopeObject: ") << ret.toDebugString());

				runtime_stack_push_ref(context,ret);
				break;
			}
			case 0x66:
			{
				//getproperty
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,context);

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,obj,function->getSystemState());

				ASObject* ret=getProperty(obj,name);
				name->resetNameIfObject();

				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x68:
			{
				//initproperty
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_REF(context,value);
				multiname* name=context->context->getMultiname(t,context);
				LOG_CALL("initProperty "<<*name);
				RUNTIME_STACK_POP_CREATE_REF(context,obj);
				checkDeclaredTraits(obj->toObject(context->context->root->getSystemState()));
				obj->toObject(context->context->root->getSystemState())->setVariableByMultiname(*name,value,ASObject::CONST_ALLOWED);
				name->resetNameIfObject();
				break;
			}
			case 0x6a:
			{
				//deleteproperty
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name = context->context->getMultiname(t,context);
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,obj,function->getSystemState());
				bool ret = deleteProperty(obj,name);
				name->resetNameIfObject();
				RUNTIME_STACK_PUSH(context,asAtom(ret));
				break;
			}
			case 0x6c:
			{
				//getslot
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,obj,function->getSystemState());
				ASObject* ret=getSlot(obj, t);
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x6d:
			{
				//setslot
				uint32_t t=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				setSlot(v1, v2, t);
				break;
			}
			case 0x6e:
			{
				//getglobalSlot
				uint32_t t=data->uints[0];
				instructionPointer+=4;

				Global* globalscope = getGlobalScope(context);
				asAtomR value = globalscope->getSlot(t);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x6f:
			{
				//setglobalSlot
				uint32_t t=data->uints[0];
				instructionPointer+=4;

				Global* globalscope = getGlobalScope(context);
				RUNTIME_STACK_POP_CREATE_REF(context,obj);
				globalscope->setSlot(t,obj);
				break;
			}
			case 0x70:
			{
				//convert_s
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				asAtomR value = asAtom::fromObject(convert_s(val));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x71:
			{
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				asAtomR value = asAtom::fromObject(esc_xelem(val));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x72:
			{
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				asAtomR value = asAtom::fromObject(esc_xattr(val));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x73:
			{
				//convert_i
				RUNTIME_STACK_POP_CREATE(context,val);
				val.convert_i();
				break;
			}
			case 0x74:
			{
				//convert_u
				RUNTIME_STACK_POP_CREATE(context,val);
				val.convert_u();
				break;
			}
			case 0x75:
			{
				//convert_d
				RUNTIME_STACK_POP_CREATE(context,val);
				val.convert_d();
				break;
			}
			case 0x76:
			{
				//convert_b
				RUNTIME_STACK_POP_CREATE(context,val);
				val.convert_b();
				break;
			}
			case 0x77:
			{
				//convert_o
				RUNTIME_STACK_PEEK_CREATE(context,val);
				if (val.type == T_NULL)
				{
					RUNTIME_STACK_POP_CREATE(context,ret);
					LOG(LOG_ERROR,"trying to call convert_o on null");
					throwError<TypeError>(kConvertNullToObjectError);
				}
				if (val.type == T_UNDEFINED)
				{
					RUNTIME_STACK_POP_CREATE(context,ret);
					LOG(LOG_ERROR,"trying to call convert_o on undefined");
					throwError<TypeError>(kConvertUndefinedToObjectError);
				}
				break;
			}
			case 0x78:
			{
				//checkfilter
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				asAtomR value = asAtom::fromObject(checkfilter(val));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x80:
			{
				//coerce
				const multiname* name=data->names[0];
				char* rewriteableCode = &(mi->body->code[0]);
				const Type* type = Type::getTypeFromMultiname(name, context->context);
				OpcodeData* rewritableData=reinterpret_cast<OpcodeData*>(rewriteableCode+instructionPointer);
				//Rewrite this to a coerceEarly
				rewriteableCode[instructionPointer-1]=0xfc;
				rewritableData->types[0]=type;

				LOG_CALL("coerceOnce " << *name);

				RUNTIME_STACK_POP_CREATE_REF(context,o);
				o=type->coerce(function->getSystemState(),o);
				runtime_stack_push_ref(context,o);

				instructionPointer+=8;
				break;
			}
			case 0x82:
			{
				//coerce_a
				coerce_a();
				break;
			}
			case 0x85:
			{
				//coerce_s
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				asAtomR value = asAtom::fromObject(val->is<ASString>() ? val : coerce_s(val));
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x86:
			{
				//astype
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,NULL);

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=asType(context->context, v1, name);
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x87:
			{
				//astypelate
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=asTypelate(v1, v2);
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x90:
			{
				//negate
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret;
				if ((val->is<Integer>() || val->is<UInteger>() || (val->is<Number>() && !val->as<Number>()->isfloat)) && val->toInt64() != 0 && val->toInt64() == val->toInt())
					ret=abstract_di(function->getSystemState(),negate_i(val));
				else
					ret=abstract_d(function->getSystemState(),negate(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x91:
			{
				//increment
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret;
				if (val->is<Integer>() || (val->is<Number>() && !val->as<Number>()->isfloat))
					ret=abstract_di(function->getSystemState(),increment_i(val));
				else
					ret=abstract_d(function->getSystemState(),increment(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x92:
			{
				//inclocal
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				incLocal(context, t);
				break;
			}
			case 0x93:
			{
				//decrement
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret;
				if (val->is<Integer>() || val->is<UInteger>() || (val->is<Number>() && !val->as<Number>()->isfloat))
					ret=abstract_di(function->getSystemState(),decrement_di(val));
				else
					ret=abstract_d(function->getSystemState(),decrement(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x94:
			{
				//declocal
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				decLocal(context, t);
				break;
			}
			case 0x95:
			{
				//typeof
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=typeOf(val);
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x96:
			{
				//not
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=abstract_b(function->getSystemState(),_not(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0x97:
			{
				//bitnot
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=abstract_i(function->getSystemState(),bitNot(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa0:
			{
				//add
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=add(v2, v1);
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa1:
			{
				//subtract
				//Be careful, operands in subtract implementation are swapped
				RUNTIME_STACK_POP_CREATE_REF(context,v2);
				RUNTIME_STACK_POP_CREATE_REF(context,v1);

				asAtomR ret;
				// if both values are Integers or int Numbers the result is also an int Number
				if( (v1->is<Integer>() || v1->is<UInteger>() || (v1->is<Number>() && !v1->as<Number>()->isfloat)) &&
					(v2->is<Integer>() || v2->is<UInteger>() || (v2->is<Number>() && !v2->as<Number>()->isfloat)))
				{
					int64_t num1=v1->toInt64();
					int64_t num2=v2->toInt64();
					LOG_CALL(_("subtractI ")  << num1 << '-' << num2);
					ret = asAtom::fromObject(abstract_di(function->getSystemState(), num1-num2));
				}
				else
					ret=asAtom::fromObject(abstract_d(function->getSystemState(),subtract(v2->getObject(), v1->getObject())));
				runtime_stack_push_ref(context,ret);
				break;
			}
			case 0xa2:
			{
				//multiply
				RUNTIME_STACK_POP_CREATE_REF(context,v2);
				RUNTIME_STACK_POP_CREATE_REF(context,v1);

				asAtomR ret;
				// if both values are Integers or int Numbers the result is also an int Number
				if( (v1->is<Integer>() || v1->is<UInteger>() || (v1->is<Number>() && !v1->as<Number>()->isfloat)) &&
					(v2->is<Integer>() || v2->is<UInteger>() || (v2->is<Number>() && !v2->as<Number>()->isfloat)))
				{
					int64_t num1=v1->toInt64();
					int64_t num2=v2->toInt64();
					LOG_CALL(_("multiplyI ")  << num1 << '*' << num2);
					ret = asAtom::fromObject(abstract_di(function->getSystemState(), num1*num2));
				}
				else
					ret=asAtom::fromObject(abstract_d(function->getSystemState(),multiply(v2->getObject(), v1->getObject())));
				runtime_stack_push_ref(context,ret);
				break;
			}
			case 0xa3:
			{
				//divide
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_d(function->getSystemState(),divide(v2, v1));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa4:
			{
				//modulo
			RUNTIME_STACK_POP_CREATE_REF(context,v2);
			RUNTIME_STACK_POP_CREATE_REF(context,v1);

				ASObject* ret;
				// if both values are Integers or int Numbers the result is also an int Number
				if( (v1->is<Integer>() || v1->is<UInteger>() || (v1->is<Number>() && !v1->as<Number>()->isfloat)) &&
					(v2->is<Integer>() || v2->is<UInteger>() || (v2->is<Number>() && !v2->as<Number>()->isfloat)))
				{
					int64_t num1=v1->toInt64();
					int64_t num2=v2->toInt64();
					LOG_CALL(_("moduloI ")  << num1 << '%' << num2);
					if (num2 == 0)
						ret=abstract_d(function->getSystemState(),Number::NaN);
					else
						ret = abstract_di(function->getSystemState(), num1%num2);
				}
				else
					ret=abstract_d(function->getSystemState(),modulo(v1->getObject(), v2->getObject()));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa5:
			{
				//lshift
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),lShift(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa6:
			{
				//rshift
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),rShift(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa7:
			{
				//urshift
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),urShift(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa8:
			{
				//bitand
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),bitAnd(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xa9:
			{
				//bitor
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),bitOr(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xaa:
			{
				//bitxor
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),bitXor(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xab:
			{
				//equals
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),equals(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xac:
			{
				//strictequals
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),strictEquals(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xad:
			{
				//lessthan
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),lessThan(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xae:
			{
				//lessequals
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),lessEquals(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xaf:
			{
				//greaterthan
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),greaterThan(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xb0:
			{
				//greaterequals
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),greaterEquals(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xb1:
			{
				//instanceof
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,type,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,value,function->getSystemState());
				bool ret=instanceOf(value, type);
				asAtomR retAtom = _MAR(asAtom(ret));
				runtime_stack_push_ref(context, retAtom);
				break;
			}
			case 0xb2:
			{
				//istype
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				multiname* name=context->context->getMultiname(t,NULL);

				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),isType(context->context, v1, name));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xb3:
			{
				//istypelate
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),isTypelate(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xb4:
			{
				//in
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());

				ASObject* ret=abstract_b(function->getSystemState(),in(v1, v2));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc0:
			{
				//increment_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=abstract_i(function->getSystemState(),increment_i(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc1:
			{
				//decrement_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=abstract_i(function->getSystemState(),decrement_i(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc2:
			{
				//inclocal_i
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				incLocal_i(context, t);
				break;
			}
			case 0xc3:
			{
				//declocal_i
				uint32_t t=data->uints[0];
				instructionPointer+=4;
				decLocal_i(context, t);
				break;
			}
			case 0xc4:
			{
				//negate_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,val,function->getSystemState());
				ASObject* ret=abstract_i(function->getSystemState(),negate_i(val));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc5:
			{
				//add_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),add_i(v2, v1));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc6:
			{
				//subtract_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),subtract_i(v2, v1));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xc7:
			{
				//multiply_i
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v2,function->getSystemState());
				RUNTIME_STACK_POP_CREATE_ASOBJECT(context,v1,function->getSystemState());

				ASObject* ret=abstract_i(function->getSystemState(),multiply_i(v2, v1));
				asAtomR value = asAtom::fromObject(ret);
				runtime_stack_push_ref(context, value);
				break;
			}
			case 0xd0:
			case 0xd1:
			case 0xd2:
			case 0xd3:
			{
				//getlocal_n
				int i=opcode&3;
				if (context->locals[i]->type == T_INVALID)
				{
					LOG_CALL( _("getLocal ") << i << " not set, pushing Undefined");
					asAtomR value = asAtom::fromObject(function->getSystemState()->getUndefinedRef());
					runtime_stack_push_ref(context, value);
					break;
				}
				LOG_CALL( "getLocal " << i << ": " << context->locals[i]->toDebugString() );
				runtime_stack_push_ref(context,context->locals[i]);
				break;
			}
			case 0xd4:
			case 0xd5:
			case 0xd6:
			case 0xd7:
			{
				//setlocal_n
				int i=opcode&3;
				LOG_CALL( "setLocal " << i );
				RUNTIME_STACK_POP_CREATE_REF(context,obj)
				if ((int)i != context->argarrayposition || obj->type == T_ARRAY)
				{
					context->locals[i]=obj;
				}
				break;
			}
			case 0xf2:
			{
				//bkptline
				LOG_CALL( _("bkptline") );
				instructionPointer+=4;
				break;
			}
			case 0xf3:
			{
				//timestamp
				LOG_CALL( _("timestamp") );
				instructionPointer+=4;
				break;
			}
			//lightspark custom opcodes
			case 0xfb:
			{
				//setslot_no_coerce
				uint32_t t=data->uints[0];
				instructionPointer+=4;

				RUNTIME_STACK_POP_CREATE_REF(context,value);
				RUNTIME_STACK_POP_CREATE_REF(context,obj);

				LOG_CALL("setSlotNoCoerce " << t);
				obj->getObject()->setSlotNoCoerce(t,value);
				break;
			}
			case 0xfc:
			{
				//coerceearly
				const Type* type = data->types[0];
				LOG_CALL("coerceEarly " << type);

				RUNTIME_STACK_POP_CREATE_REF(context,o);
				o=type->coerce(function->getSystemState(),o);
				runtime_stack_push_ref(context,o);

				instructionPointer+=8;
				break;
			}
			case 0xfd:
			{
				//getscopeatindex
				//This opcode is similar to getscopeobject, but it allows access to any
				//index of the scope stack
				uint32_t t=data->uints[0];
				LOG_CALL( "getScopeAtIndex " << t);
				asAtomR obj;
				uint32_t parentsize = context->parent_scope_stack.isNull() ? 0 :context->parent_scope_stack->scope.size();
				if (!context->parent_scope_stack.isNull() && t<parentsize)
					obj = context->parent_scope_stack->scope[t].object;
				else
				{
					assert_and_throw(t-parentsize <context->curr_scope_stack);
					obj=context->scope_stack[t-parentsize];
				}
				runtime_stack_push_ref(context,obj);
				instructionPointer+=4;
				break;
			}
			case 0xfe:
			{
				//getlexonce
				//This opcode execute a lookup on the application domain
				//and rewrites itself to a pushearly
				const multiname* name=data->names[0];
				LOG_CALL( "getLexOnce " << *name);
				asAtomR target;
				ASObject* obj=ABCVm::getCurrentApplicationDomain(context)->getVariableAndTargetByMultiname(*name,target);
				//The object must exists, since it was found during optimization
				assert_and_throw(obj);
				char* rewriteableCode = &(mi->body->code[0]);
				OpcodeData* rewritableData=reinterpret_cast<OpcodeData*>(rewriteableCode+instructionPointer);
				//Rewrite this to a pushearly
				rewriteableCode[instructionPointer-1]=0xff;
				rewritableData->objs[0]=obj;
				//Also push the object right away
				asAtomR value = asAtom::fromObject(obj);
				runtime_stack_push_ref(context, value);
				//Move to the next instruction
				instructionPointer+=8;
				break;
			}
			case 0xff:
			{
				//pushearly
				ASObject* o=data->objs[0];
				instructionPointer+=8;
				LOG_CALL( "pushEarly " << o);
				asAtomR value = asAtom::fromObject(o);
				runtime_stack_push_ref(context, value);
				break;
			}
			default:
				LOG(LOG_ERROR,_("Not interpreted instruction @") << instructionPointer);
				LOG(LOG_ERROR,_("dump ") << hex << (unsigned int)opcode << dec);
				throw ParseException("Not implemented instruction in fast interpreter");
		}
		PROF_ACCOUNT_TIME(mi->profTime[instructionPointer],profilingCheckpoint(startTime));
	}

#undef PROF_ACCOUNT_TIME 
#undef PROF_IGNORE_TIME
	//We managed to execute all the function
	RUNTIME_STACK_POP_CREATE_ASOBJECT(context,returnvalue,function->getSystemState());
	return returnvalue;
}
