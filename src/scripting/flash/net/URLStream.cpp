/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2011-2013  Alessandro Pignotti (a.pignotti@sssup.it)

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

#include <map>
#include "backends/security.h"
#include "scripting/abc.h"
#include "scripting/flash/net/URLStream.h"
#include "scripting/flash/net/flashnet.h"
#include "scripting/flash/utils/ByteArray.h"
#include "scripting/argconv.h"

/**
 * TODO: This whole class shares a lot of code with URLLoader - unify!
 * TODO: This whole class is quite untested
 */

using namespace std;
using namespace lightspark;

URLStreamThread::URLStreamThread(_R<URLRequest> request, _R<URLStream> ldr, _R<ByteArray> bytes)
  : DownloaderThreadBase(_IMR(request.getPtr()), ldr.getPtr()), loader(ldr), data(bytes),streambuffer(NULL),timestamp_last_progress(0),bytes_total(0)
{
}

void URLStreamThread::setBytesTotal(uint32_t b)
{
	bytes_total = b;
}
void URLStreamThread::setBytesLoaded(uint32_t b)
{
	uint32_t curlen = data->getLength();
	if(b>curlen && streambuffer)
	{
		data->append(streambuffer,b - curlen);
		uint64_t cur=compat_get_thread_cputime_us();
		if (cur > timestamp_last_progress+ 40*1000)
		{
			timestamp_last_progress = cur;
			getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<ProgressEvent>::getInstanceS(loader->getSystemState(),b,bytes_total));
		}
	}
}

void URLStreamThread::execute()
{
	timestamp_last_progress = compat_get_thread_cputime_us();
	assert(!downloader);

	//TODO: support httpStatus

	_R<MemoryStreamCache> cache(_MR(new MemoryStreamCache(loader->getSystemState())));
	if(!createDownloader(cache, loader,this))
		return;
	data->setLength(0);

	bool success=false;
	if(!downloader->hasFailed())
	{
		getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<Event>::getInstanceS(loader->getSystemState(),"open"));
		streambuffer = cache->createReader();
		getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<ProgressEvent>::getInstanceS(loader->getSystemState(),0,bytes_total));
		cache->waitForTermination();
		if(!downloader->hasFailed() && !threadAborting)
		{
			/*
			std::streambuf *sbuf = cache->createReader();
			istream s(sbuf);
			uint8_t* buf=new uint8_t[downloader->getLength()];
			//TODO: avoid this useless copy
			s.read((char*)buf,downloader->getLength());
			//TODO: test binary data format
			data->acquireBuffer(buf,downloader->getLength());
			//The buffers must not be deleted, it's now handled by the ByteArray instance
			delete sbuf;
			*/
			success=true;
		}
	}

	// Don't send any events if the thread is aborting
	if(success && !threadAborting)
	{
		getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<ProgressEvent>::getInstanceS(loader->getSystemState(),downloader->getLength(),downloader->getLength()));
		//Send a complete event for this object
		getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<Event>::getInstanceS(loader->getSystemState(),"complete"));
	}
	else if(!success && !threadAborting)
	{
		//Notify an error during loading
		getVm(loader->getSystemState())->addEvent(_IMR(loader.getPtr()),Class<IOErrorEvent>::getInstanceS(loader->getSystemState()));
	}

	{
		//Acquire the lock to ensure consistency in threadAbort
		SpinlockLocker l(downloaderLock);
		getSys()->downloadManager->destroy(downloader);
		downloader = NULL;
	}
}

void URLStream::sinit(Class_base* c)
{
	CLASS_SETUP(c, EventDispatcher, _constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("load","",Class<IFunction>::getFunction(c->getSystemState(),load),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("close","",Class<IFunction>::getFunction(c->getSystemState(),close),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("bytesAvailable","",Class<IFunction>::getFunction(c->getSystemState(),bytesAvailable),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("endian","",Class<IFunction>::getFunction(c->getSystemState(),_getEndian),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("endian","",Class<IFunction>::getFunction(c->getSystemState(),_setEndian),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_getObjectEncoding),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_setObjectEncoding),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("readBoolean","",Class<IFunction>::getFunction(c->getSystemState(),readBoolean),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readByte","",Class<IFunction>::getFunction(c->getSystemState(),readByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readBytes","",Class<IFunction>::getFunction(c->getSystemState(),readBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readDouble","",Class<IFunction>::getFunction(c->getSystemState(),readDouble),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readFloat","",Class<IFunction>::getFunction(c->getSystemState(),readFloat),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readInt","",Class<IFunction>::getFunction(c->getSystemState(),readInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readMultiByte","",Class<IFunction>::getFunction(c->getSystemState(),readMultiByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readObject","",Class<IFunction>::getFunction(c->getSystemState(),readObject),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readShort","",Class<IFunction>::getFunction(c->getSystemState(),readShort),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedByte","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedInt","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedShort","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedShort),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUTF","",Class<IFunction>::getFunction(c->getSystemState(),readUTF),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUTFBytes","",Class<IFunction>::getFunction(c->getSystemState(),readUTFBytes),NORMAL_METHOD,true);
	REGISTER_GETTER(c,connected);

	c->addImplementedInterface(InterfaceClass<IDataInput>::getClass(c->getSystemState()));
	IDataInput::linkTraits(c);
}

ASFUNCTIONBODY_GETTER(URLStream,connected);

void URLStream::buildTraits(ASObject* o)
{
}

void URLStream::threadFinished(IThreadJob* finishedJob)
{
	// If this is the current job, we are done. If these are not
	// equal, finishedJob is a job that was cancelled when load()
	// was called again, and we have to still wait for the correct
	// job.
	SpinlockLocker l(spinlock);
	if(finishedJob==job)
		job=NULL;

	delete finishedJob;
}

ASFUNCTIONBODY_ATOM(URLStream,_constructor)
{
	std::vector<asAtom> empty;
	return EventDispatcher::_constructor(sys,obj,empty,0);
}

ASFUNCTIONBODY(URLStream,load)
{
	URLStream* th=obj->as<URLStream>();
	_NR<URLRequest> urlRequest;
	ARG_UNPACK (urlRequest);

	{
		SpinlockLocker l(th->spinlock);
		if(th->job)
			th->job->threadAbort();
	}

	th->url=urlRequest->getRequestURL();
	if(!th->url.isValid())
	{
		//Notify an error during loading
		getSys()->currentVm->addEvent(_IMR(th),Class<IOErrorEvent>::getInstanceS(obj->getSystemState()));
		return NULL;
	}

	//TODO: support the right events (like SecurityErrorEvent)
	//URLLoader ALWAYS checks for policy files, in contrast to NetStream.play().
	SecurityManager::checkURLStaticAndThrow(th->url, ~(SecurityManager::LOCAL_WITH_FILE),
		SecurityManager::LOCAL_WITH_FILE | SecurityManager::LOCAL_TRUSTED, true);

	URLStreamThread *job=new URLStreamThread(urlRequest, _IMR(th), th->data);
	getSys()->addJob(job);
	th->job=job;
	th->connected = true;
	return NULL;
}

ASFUNCTIONBODY(URLStream,close)
{
	URLStream* th=static_cast<URLStream*>(obj);
 	SpinlockLocker l(th->spinlock);
	if(th->job)
		th->job->threadAbort();
	th->connected = false;

	return NULL;
}

void URLStream::finalize()
{
	EventDispatcher::finalize();
	data.reset();
}

URLStream::URLStream(Class_base *c):EventDispatcher(c),data(Class<ByteArray>::getInstanceS(c->getSystemState())),job(NULL),connected(false)
{
}

ASFUNCTIONBODY(URLStream,bytesAvailable) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::_getBytesAvailable(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,_getEndian) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::_getEndian(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,_setEndian) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::_setEndian(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,_getObjectEncoding) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::_getObjectEncoding(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,_setObjectEncoding) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::_setObjectEncoding(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readBoolean) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readBoolean(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readByte) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readByte(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readBytes) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readBytes(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readDouble) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readDouble(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readFloat) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readFloat(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readInt) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readInt(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readMultiByte) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readMultiByte(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY_ATOM(URLStream,readObject) {
	URLStream* th=obj.as<URLStream>();
	asAtom v = asAtom::fromObject(th->data.getPtr());
	return ByteArray::readObject(sys,v, args, argslen);
}

ASFUNCTIONBODY(URLStream,readShort) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readShort(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readUnsignedByte) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readUnsignedByte(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readUnsignedInt) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readUnsignedInt(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readUnsignedShort) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readUnsignedShort(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readUTF) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readUTF(th->data.getPtr(), args, argslen);
}

ASFUNCTIONBODY(URLStream,readUTFBytes) {
	URLStream* th=static_cast<URLStream*>(obj);
	return ByteArray::readUTFBytes(th->data.getPtr(), args, argslen);
}
