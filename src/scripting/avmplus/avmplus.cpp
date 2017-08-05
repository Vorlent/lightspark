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

#include "scripting/avmplus/avmplus.h"
#include "scripting/flash/utils/ByteArray.h"
#include "scripting/class.h"
#include "scripting/argconv.h"
#include "abc.h"

using namespace lightspark;

avmplusFile::avmplusFile(Class_base* c):
	ASObject(c)
{
}

void avmplusFile::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_SEALED);
	c->setDeclaredMethodByQName("exists","",Class<IFunction>::getFunction(c->getSystemState(),exists),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("read","",Class<IFunction>::getFunction(c->getSystemState(),read),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("write","",Class<IFunction>::getFunction(c->getSystemState(),write),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("readByteArray","",Class<IFunction>::getFunction(c->getSystemState(),readByteArray),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("writeByteArray","",Class<IFunction>::getFunction(c->getSystemState(),writeByteArray),NORMAL_METHOD,false);
}

ASFUNCTIONBODY(avmplusFile,exists)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.File.exists is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusFile,read)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.File.read is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusFile,write)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.File.write is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusFile,readByteArray)
{
	//avmplusFile* th=static_cast<avmplusFile*>(obj);
	tiny_string filename;
	ARG_UNPACK(filename);
	
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.File.readByteArray is unimplemented."));
	return Class<ByteArray>::getInstanceS(obj->getSystemState());
}
ASFUNCTIONBODY(avmplusFile,writeByteArray)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.File.writeByteArray is unimplemented."));
	return NULL;
}

avmplusSystem::avmplusSystem(Class_base* c):
	ASObject(c)
{
}

void avmplusSystem::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_SEALED);
	c->setDeclaredMethodByQName("getFeatures","",Class<IFunction>::getFunction(c->getSystemState(),getFeatures),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("queueCollection","",Class<IFunction>::getFunction(c->getSystemState(),queueCollection),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("forceFullCollection","",Class<IFunction>::getFunction(c->getSystemState(),forceFullCollection),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("getAvmplusVersion","",Class<IFunction>::getFunction(c->getSystemState(),getAvmplusVersion),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("pauseForGCIfCollectionImminent","",Class<IFunction>::getFunction(c->getSystemState(),pauseForGCIfCollectionImminent),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("getRunmode","",Class<IFunction>::getFunction(c->getSystemState(),getRunmode),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("isDebugger","",Class<IFunction>::getFunction(c->getSystemState(),isDebugger),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("isGlobal","",Class<IFunction>::getFunction(c->getSystemState(),isGlobal),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("freeMemory","",Class<IFunction>::getFunction(c->getSystemState(),_freeMemory),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("totalMemory","",Class<IFunction>::getFunction(c->getSystemState(),_totalMemory),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("privateMemory","",Class<IFunction>::getFunction(c->getSystemState(),_privateMemory),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("argv","",Class<IFunction>::getFunction(c->getSystemState(),argv),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("exec","",Class<IFunction>::getFunction(c->getSystemState(),exec),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("write","",Class<IFunction>::getFunction(c->getSystemState(),write),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("exit","",Class<IFunction>::getFunction(c->getSystemState(),exit),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("trace","",Class<IFunction>::getFunction(c->getSystemState(),lightspark::trace),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("canonicalizeNumber","",Class<IFunction>::getFunction(c->getSystemState(),canonicalizeNumber),NORMAL_METHOD,false);
}

ASFUNCTIONBODY(avmplusSystem,getFeatures)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.getFeatures is unimplemented."));
	return abstract_s(getSys(),"");
}
ASFUNCTIONBODY(avmplusSystem,getRunmode)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.getRunmode is unimplemented."));
	return abstract_s(getSys(),"jit");
}

ASFUNCTIONBODY(avmplusSystem,queueCollection)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.queueCollection is unimplemented."));
	return NULL;
}

ASFUNCTIONBODY(avmplusSystem,forceFullCollection)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.forceFullCollection is unimplemented."));
	return NULL;
}

ASFUNCTIONBODY(avmplusSystem,getAvmplusVersion)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.getAvmplusVersion is unimplemented."));
	return abstract_s(getSys(),"0");
}
ASFUNCTIONBODY(avmplusSystem,pauseForGCIfCollectionImminent)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.pauseForGCIfCollectionImminent is unimplemented."));
	return NULL;
}

ASFUNCTIONBODY(avmplusSystem,isDebugger)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.isDebugger is unimplemented."));
	return abstract_b(obj->getSystemState(),false);
}
ASFUNCTIONBODY(avmplusSystem,isGlobal)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.isDebugger is unimplemented."));
	return abstract_b(obj->getSystemState(),false);
}
ASFUNCTIONBODY(avmplusSystem,_freeMemory)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.freeMemory is unimplemented."));
	return abstract_d(obj->getSystemState(),1024);
}
ASFUNCTIONBODY(avmplusSystem,_totalMemory)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.totalMemory is unimplemented."));
	return abstract_d(obj->getSystemState(),1024);
}
ASFUNCTIONBODY(avmplusSystem,_privateMemory)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.privateMemory is unimplemented."));
	return abstract_d(obj->getSystemState(),1024);
}
ASFUNCTIONBODY(avmplusSystem,argv)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.argv is unimplemented."));
	return Class<Array>::getInstanceS(getSys());
}
ASFUNCTIONBODY(avmplusSystem,exec)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.exec is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusSystem,write)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.write is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusSystem,exit)
{
	LOG(LOG_NOT_IMPLEMENTED, _("avmplus.System.exit is unimplemented."));
	return NULL;
}
ASFUNCTIONBODY(avmplusSystem,canonicalizeNumber)
{
	_NR<ASObject> o;
	ARG_UNPACK(o);
	switch(o->getObjectType())
	{
		case T_NUMBER:
		case T_INTEGER:
		case T_BOOLEAN:
		case T_UINTEGER:
		case T_NULL:
		case T_UNDEFINED:
			return abstract_d(o->getSystemState(),o->toNumber());
		case T_QNAME:
		case T_NAMESPACE:
			return abstract_d(o->getSystemState(),Number::NaN);
		default:
			o->incRef();
			return o.getPtr();
	}
}

avmplusDomain::avmplusDomain(Class_base* c):
	ASObject(c)
{
}

void avmplusDomain::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject,_constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("currentDomain","",Class<IFunction>::getFunction(c->getSystemState(),_getCurrentDomain),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("MIN_DOMAIN_MEMORY_LENGTH","",Class<IFunction>::getFunction(c->getSystemState(),_getMinDomainMemoryLength),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("load","",Class<IFunction>::getFunction(c->getSystemState(),load),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("loadBytes","",Class<IFunction>::getFunction(c->getSystemState(),loadBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getClass","",Class<IFunction>::getFunction(c->getSystemState(),getClass),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("domainMemory","",Class<IFunction>::getFunction(c->getSystemState(),_getDomainMemory),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("domainMemory","",Class<IFunction>::getFunction(c->getSystemState(),_setDomainMemory),SETTER_METHOD,true);
}
ASFUNCTIONBODY(avmplusDomain,_constructor)
{
	_NR<avmplusDomain> parentDomain;
	ARG_UNPACK(parentDomain);
	avmplusDomain* th = Class<avmplusDomain>::cast(obj);
	if (parentDomain.isNull())
		th->appdomain = ABCVm::getCurrentApplicationDomain(getVm(getSys())->currentCallContext);
	else
		th->appdomain = _NR<ApplicationDomain>(Class<ApplicationDomain>::getInstanceS(getSys(),parentDomain->appdomain));
	return NULL;
}

ASFUNCTIONBODY(avmplusDomain,_getCurrentDomain)
{
	avmplusDomain* ret = Class<avmplusDomain>::getInstanceSNoArgs(getSys());
	ret->appdomain = ABCVm::getCurrentApplicationDomain(getVm(getSys())->currentCallContext);
	return ret;
}
ASFUNCTIONBODY(avmplusDomain,_getMinDomainMemoryLength)
{
	return abstract_ui(obj->getSystemState(),MIN_DOMAIN_MEMORY_LIMIT);
}
ASFUNCTIONBODY(avmplusDomain,load)
{
	tiny_string filename;
	LOG(LOG_NOT_IMPLEMENTED, "avmplus.Domain.load is unimplemented.");
	ARG_UNPACK(filename);
	throwError<ASError>(kFileOpenError,filename);
	return NULL;
}
ASFUNCTIONBODY(avmplusDomain,loadBytes)
{
	avmplusDomain* th = Class<avmplusDomain>::cast(obj);
	_NR<ByteArray> bytes;
	uint32_t swfversion;
	ARG_UNPACK (bytes)(swfversion, 0);

	if (swfversion != 0)
		LOG(LOG_NOT_IMPLEMENTED,"Domain.loadBytes ignores parameter swfVersion");
	MemoryStreamCache mc(obj->getSystemState());
	mc.append(bytes->getBuffer(bytes->getLength(),false),bytes->getLength());
	std::streambuf *sbuf = mc.createReader();
	std::istream s(sbuf);
	
	RootMovieClip* root=getVm(getSys())->currentCallContext->context->root.getPtr();
	_NR<ApplicationDomain> origdomain = root->applicationDomain;
	root->applicationDomain = th->appdomain;
	ABCContext context(_IAMR(root), s, getVm(root->getSystemState()));
	context.exec(false);
	root->applicationDomain = origdomain;
	delete sbuf;
	return NULL;
}
ASFUNCTIONBODY(avmplusDomain,getClass)
{
	return getDefinitionByName(obj,args,argslen);
}
ASFUNCTIONBODY(avmplusDomain,_getDomainMemory)
{
	avmplusDomain* th = Class<avmplusDomain>::cast(obj);
	if (th->appdomain->domainMemory.isNull())
		return NULL;
	th->appdomain->domainMemory->incRef();
	return th->appdomain->domainMemory.getPtr();
}
ASFUNCTIONBODY(avmplusDomain,_setDomainMemory)
{
	_NR<ByteArray> b;
	ARG_UNPACK(b);
	avmplusDomain* th = Class<avmplusDomain>::cast(obj);
	if (b.isNull())
	{
		th->appdomain->domainMemory = b;
		return NULL;
	}
		
	if (b->getLength() < MIN_DOMAIN_MEMORY_LIMIT)
		throwError<RangeError>(kEndOfFileError);
	th->appdomain->domainMemory = b;
	return NULL;
}
