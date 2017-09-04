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

#ifndef SCRIPTING_ABCUTILS_H
#define SCRIPTING_ABCUTILS_H 1

#include "smartrefs.h"
#include "errorconstants.h"
#include "asobject.h"

namespace lightspark
{

class method_info;
class ABCContext;
class ASObject;
class Class_base;
class asAtom;
class asAtom;

struct scope_entry
{
	asAtom object;
	bool considerDynamic;
	scope_entry(asAtom& o, bool c):object(o),considerDynamic(c)
	{
	}
};
class scope_entry_list: public RefCountable
{
public:
	std::vector<scope_entry> scope;
};
struct call_context
{
#include "packed_begin.h"
	struct
	{
		uint32_t stack_index;
		uint32_t exec_pos;
	} PACKED;
#include "packed_end.h"
	std::vector<asAtom> stack;
	std::vector<asAtom> locals;
	ABCContext* context;
	uint32_t locals_size;
	uint32_t max_stack;
	int32_t argarrayposition; // position of argument array in locals ( -1 if no argument array needed)
	_NR<scope_entry_list> parent_scope_stack;
	uint32_t max_scope_stack;
	uint32_t curr_scope_stack;
	std::vector<asAtom> scope_stack;
	bool* scope_stack_dynamic;
	method_info* mi;
	/* This is the function's inClass that is currently executing. It is used
	 * by {construct,call,get,set}Super
	 * */
	Class_base* inClass;
	/* Current namespace set by 'default xml namespace = ...'.
	 * Defaults to empty string according to ECMA-357 13.1.1.1
	 */
	uint32_t defaultNamespaceUri;
	asAtom returnvalue;
	bool returning;
	~call_context();
	static void handleError(int errorcode);
	inline void runtime_stack_clear()
	{
		stack.clear();
	}
};
#define RUNTIME_STACK_PUSH(context,val) \
if(context->stack_index<context->max_stack) \
	context->stack[context->stack_index++]=val; \
else context->handleError(kStackOverflowError)

inline void runtime_stack_push_ref(call_context* context, asAtom& val) {
	if(context->stack_index<context->max_stack) {
		context->stack[context->stack_index++]=val;
	} else {
		context->handleError(kStackOverflowError);
	}
}

#define RUNTIME_STACK_POP_CREATE_REF(context,ret) \
	if(!context->stack_index) \
		context->handleError(kStackUnderflowError); \
	asAtom& ret=context->stack[--context->stack_index]; \

#define RUNTIME_STACK_POP_CREATE_ASOBJECT(context,ret, sys) \
	ASObject* ret = NULL; \
	if(context->stack_index) ret=context->stack[--context->stack_index].toObject(sys); \
	else context->handleError(kStackUnderflowError);

#define RUNTIME_STACK_PEEK_CREATE_REF(context,ret) \
	asAtom& ret = context->stack[context->stack_index-1];

#define RUNTIME_STACK_POINTER_CREATE_REF(context,ret) \
	if(!context->stack_index)  \
		context->handleError(kStackUnderflowError); \
	asAtom& ret = context->stack[context->stack_index-1];

}
#endif /* SCRIPTING_ABCUTILS_H */
