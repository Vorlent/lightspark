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

#include "scripting/abc.h"
#include "scripting/flash/utils/flashutils.h"
#include "asobject.h"
#include "scripting/class.h"
#include "compat.h"
#include "parsing/amf3_generator.h"
#include "scripting/argconv.h"
#include "scripting/flash/errors/flasherrors.h"
#include <sstream>
#include <zlib.h>
#include <glib.h>

using namespace std;
using namespace lightspark;

#define BA_CHUNK_SIZE 4096

const char* Endian::littleEndian = "littleEndian";
const char* Endian::bigEndian = "bigEndian";

void Endian::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_SEALED | CLASS_FINAL);
	c->setVariableByQName("LITTLE_ENDIAN","",abstract_s(c->getSystemState(),littleEndian),DECLARED_TRAIT);
	c->setVariableByQName("BIG_ENDIAN","",abstract_s(c->getSystemState(),bigEndian),DECLARED_TRAIT);
}

void IExternalizable::linkTraits(Class_base* c)
{
	lookupAndLink(c,"readExternal","flash.utils:IExternalizable");
	lookupAndLink(c,"writeExternal","flash.utils:IExternalizable");
}

void IDataInput::linkTraits(Class_base* c)
{
	lookupAndLink(c,"bytesAvailable","flash.utils:IDataInput");
	lookupAndLink(c,"endian","flash.utils:IDataInput");
	lookupAndLink(c,"objectEncoding","flash.utils:IDataInput");
	lookupAndLink(c,"readBoolean","flash.utils:IDataInput");
	lookupAndLink(c,"readByte","flash.utils:IDataInput");
	lookupAndLink(c,"readBytes","flash.utils:IDataInput");
	lookupAndLink(c,"readDouble","flash.utils:IDataInput");
	lookupAndLink(c,"readFloat","flash.utils:IDataInput");
	lookupAndLink(c,"readInt","flash.utils:IDataInput");
	lookupAndLink(c,"readMultiByte","flash.utils:IDataInput");
	lookupAndLink(c,"readObject","flash.utils:IDataInput");
	lookupAndLink(c,"readShort","flash.utils:IDataInput");
	lookupAndLink(c,"readUnsignedByte","flash.utils:IDataInput");
	lookupAndLink(c,"readUnsignedInt","flash.utils:IDataInput");
	lookupAndLink(c,"readUnsignedShort","flash.utils:IDataInput");
	lookupAndLink(c,"readUTF","flash.utils:IDataInput");
	lookupAndLink(c,"readUTFBytes","flash.utils:IDataInput");
}

void IDataOutput::linkTraits(Class_base* c)
{
	lookupAndLink(c,"endian","flash.utils:IDataOutput");
	lookupAndLink(c,"objectEncoding","flash.utils:IDataOutput");
	lookupAndLink(c,"writeBoolean","flash.utils:IDataOutput");
	lookupAndLink(c,"writeByte","flash.utils:IDataOutput");
	lookupAndLink(c,"writeBytes","flash.utils:IDataOutput");
	lookupAndLink(c,"writeDouble","flash.utils:IDataOutput");
	lookupAndLink(c,"writeFloat","flash.utils:IDataOutput");
	lookupAndLink(c,"writeInt","flash.utils:IDataOutput");
	lookupAndLink(c,"writeMultiByte","flash.utils:IDataOutput");
	lookupAndLink(c,"writeObject","flash.utils:IDataOutput");
	lookupAndLink(c,"writeShort","flash.utils:IDataOutput");
	lookupAndLink(c,"writeUnsignedInt","flash.utils:IDataOutput");
	lookupAndLink(c,"writeUTF","flash.utils:IDataOutput");
	lookupAndLink(c,"writeUTFBytes","flash.utils:IDataOutput");
}



ASFUNCTIONBODY(lightspark,getQualifiedClassName)
{
	//CHECK: what to do if ns is empty
	ASObject* target=args[0];
	Class_base* c;
	switch(target->getObjectType())
	{
		case T_NULL:
			return abstract_s(target->getSystemState(),"null");
		case T_UNDEFINED:
			// Testing shows that this really returns "void"!
			return abstract_s(target->getSystemState(),"void");
		case T_CLASS:
			c=static_cast<Class_base*>(target);
			break;
		case T_NUMBER:
			if (target->as<Number>()->isfloat)
				c=target->getClass();
			else if (target->toInt64() > INT32_MIN && target->toInt64()< INT32_MAX)
				c=Class<Integer>::getRef(target->getSystemState()).getPtr();
			else if (target->toInt64() > 0 && target->toInt64()< UINT32_MAX)
				c=Class<UInteger>::getRef(target->getSystemState()).getPtr();
			else 
				c=target->getClass();
			break;
		default:
			assert_and_throw(target->getClass());
			c=target->getClass();
			break;
	}

	return abstract_s(obj->getSystemState(),c->getQualifiedClassName());
}

ASFUNCTIONBODY(lightspark,getQualifiedSuperclassName)
{
	//CHECK: what to do is ns is empty
	ASObject* target=args[0];
	Class_base* c;
	if(target->getObjectType()!=T_CLASS)
	{
		assert_and_throw(target->getClass());
		c=target->getClass()->super.getPtr();
	}
	else
		c=static_cast<Class_base*>(target)->super.getPtr();

	if (!c)
		return target->getSystemState()->getNullRef();

	return abstract_s(obj->getSystemState(),c->getQualifiedClassName());
}

ASFUNCTIONBODY_ATOM(lightspark,getDefinitionByName)
{
	assert_and_throw(argslen==1);
	const tiny_string& tmp=args[0]->toString();
	multiname name(NULL);
	name.name_type=multiname::NAME_STRING;

	tiny_string nsName;
	tiny_string tmpName;
	stringToQName(tmp,tmpName,nsName);
	name.name_s_id=sys->getUniqueStringId(tmpName);
	if (nsName != "")
		name.ns.push_back(nsNameAndKind(sys,nsName,NAMESPACE));

	LOG(LOG_CALLS,_("Looking for definition of ") << name);
	ASObject* target;
	ASObject* o=ABCVm::getCurrentApplicationDomain(getVm(sys)->currentCallContext)->getVariableAndTargetByMultiname(name,target);

	if(o==NULL)
	{
		throwError<ReferenceError>(kClassNotFoundError, tmp);
	}

	assert_and_throw(o->getObjectType()==T_CLASS);

	LOG(LOG_CALLS,_("Getting definition for ") << name);
	return asAtom::fromObject(o);
}

ASFUNCTIONBODY(lightspark,describeType)
{
	assert_and_throw(argslen>=1);
	return args[0]->describeType();
}

ASFUNCTIONBODY(lightspark,getTimer)
{
	uint64_t ret=compat_msectiming() - obj->getSystemState()->startTime;
	return abstract_i(obj->getSystemState(),ret);
}


ASFUNCTIONBODY_ATOM(lightspark,setInterval)
{
	assert_and_throw(argslen >= 2 && args[0]->type==T_FUNCTION);

	//Build arguments array
	std::vector<asAtom> callbackArgs(argslen-2);
	uint32_t i;
	for(i=0; i<argslen-2; i++)
	{
		callbackArgs[i] = args[i+2];
		//incRef all passed arguments
	}

	//Add interval through manager
	uint32_t id = sys->intervalManager->setInterval(args[0], callbackArgs, argslen-2,
			asAtom::nullAtom, args[1]->toInt());
	return asAtom((int32_t)id);
}

ASFUNCTIONBODY_ATOM(lightspark,setTimeout)
{
	assert_and_throw(argslen >= 2);

	//Build arguments array
	std::vector<asAtom> callbackArgs(argslen-2);
	uint32_t i;
	for(i=0; i<argslen-2; i++)
	{
		callbackArgs[i] = args[i+2];
		//incRef all passed arguments
	}

	//incRef the function
	//Add timeout through manager
	uint32_t id = sys->intervalManager->setTimeout(args[0], callbackArgs, argslen-2,
			asAtom::nullAtom, args[1]->toInt());
	return asAtom((int32_t)id);
}

ASFUNCTIONBODY(lightspark,clearInterval)
{
	assert_and_throw(argslen == 1);
	args[0]->getSystemState()->intervalManager->clearInterval(args[0]->toInt(), IntervalRunner::INTERVAL, true);
	return NULL;
}

ASFUNCTIONBODY(lightspark,clearTimeout)
{
	assert_and_throw(argslen == 1);
	args[0]->getSystemState()->intervalManager->clearInterval(args[0]->toInt(), IntervalRunner::TIMEOUT, true);
	return NULL;
}

ASFUNCTIONBODY(lightspark,escapeMultiByte)
{
	tiny_string str;
	ARG_UNPACK (str, "undefined");
	return abstract_s(getSys(),URLInfo::encode(str, URLInfo::ENCODE_ESCAPE));
}
ASFUNCTIONBODY(lightspark,unescapeMultiByte)
{
	tiny_string str;
	ARG_UNPACK (str, "undefined");
	return abstract_s(getSys(),URLInfo::decode(str, URLInfo::ENCODE_ESCAPE));
}
