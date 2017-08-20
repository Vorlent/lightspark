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

#ifndef SCRIPTING_FLASH_NET_FLASHNET_H
#define SCRIPTING_FLASH_NET_FLASHNET_H 1

#include "compat.h"
#include "asobject.h"
#include "scripting/flash/events/flashevents.h"
#include "thread_pool.h"
#include "backends/netutils.h"
#include "timer.h"
#include "backends/decoder.h"
#include "backends/audio.h"
#include "NetStreamInfo.h"
#include "scripting/flash/utils/ByteArray.h"

namespace lightspark
{

class URLRequest: public ASObject
{
private:
	enum METHOD { GET=0, POST };
	METHOD method;
	tiny_string url;
	_NR<ASObject> data;
	tiny_string digest;
	tiny_string validatedContentType() const;
	tiny_string getContentTypeHeader() const;
	void validateHeaderName(const tiny_string& headerName) const;
	ASPROPERTY_GETTER_SETTER(tiny_string,contentType);
	//TODO ASPROPERTY_GETTER_SETTER(_R<Array>,requestHeaders);
	ASPROPERTY_GETTER_SETTER(asAtomR,requestHeaders);
public:
	URLRequest(Class_base* c);
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(_getURL);
	ASFUNCTION(_setURL);
	ASFUNCTION(_getMethod);
	ASFUNCTION(_setMethod);
	ASFUNCTION(_setData);
	ASFUNCTION_ATOM(_getData);
	ASFUNCTION(_getDigest);
	ASFUNCTION(_setDigest);
	URLInfo getRequestURL() const;
	std::list<tiny_string> getHeaders();
	void getPostData(std::vector<uint8_t>& data) const;
};

class URLRequestMethod: public ASObject
{
public:
	URLRequestMethod(Class_base* c):ASObject(c){}
	static void sinit(Class_base*);
};

class URLVariables: public ASObject
{
private:
	void decode(const tiny_string& s);
	tiny_string toString_priv();
public:
	URLVariables(Class_base* c):ASObject(c){}
	URLVariables(Class_base* c,const tiny_string& s);
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(decode);
	ASFUNCTION(_toString);
	tiny_string toString();
};

class URLLoaderDataFormat: public ASObject
{
public:
	URLLoaderDataFormat(Class_base* c):ASObject(c){}
	static void sinit(Class_base*);
};


class SharedObjectFlushStatus: public ASObject
{
public:
	SharedObjectFlushStatus(Class_base* c):ASObject(c){}
	static void sinit(Class_base*);
};

class SharedObject: public EventDispatcher
{
private:
	static std::map<tiny_string, ASObject* > sharedobjectmap;
public:
	SharedObject(Class_base* c);
	static void sinit(Class_base*);
	ASFUNCTION_ATOM(getLocal);
	ASFUNCTION(getRemote);
	ASFUNCTION(flush);
	ASFUNCTION(clear);
	ASFUNCTION(close);
	ASFUNCTION(connect);
	ASFUNCTION(setProperty);
	ASPROPERTY_GETTER_SETTER(asAtomR,client);
	ASPROPERTY_GETTER(_NR<ASObject>,data);
	ASFUNCTION(_getDefaultObjectEncoding);
	ASFUNCTION(_setDefaultObjectEncoding);
	ASPROPERTY_SETTER(number_t,fps);
	ASPROPERTY_GETTER_SETTER(uint32_t,objectEncoding);
	ASFUNCTION(_getPreventBackup);
	ASFUNCTION(_setPreventBackup);
	ASFUNCTION(_getSize);
};

class ObjectEncoding: public ASObject
{
public:
	ObjectEncoding(Class_base* c):ASObject(c){}
	enum ENCODING { AMF0=0, AMF3=3, DEFAULT=3 };
	static void sinit(Class_base*);
};

class URLLoader;

class URLLoaderThread : public DownloaderThreadBase
{
private:
	_R<URLLoader> loader;
	void execute();
public:
	URLLoaderThread(_R<URLRequest> _request, _R<URLLoader> _loader);
};

class URLLoader: public EventDispatcher, public IDownloaderThreadListener, public ILoadable
{
private:
	tiny_string dataFormat;
	_NR<ASObject> data;
	Spinlock spinlock;
	URLLoaderThread *job;
	uint64_t timestamp_last_progress;
public:
	URLLoader(Class_base* c);
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	void threadFinished(IThreadJob *job);
	void setData(_NR<ASObject> data);
	tiny_string getDataFormat();
	void setDataFormat(const tiny_string& newFormat);
	void setBytesTotal(uint32_t b);
	void setBytesLoaded(uint32_t b);
	ASFUNCTION_ATOM(_constructor);
	ASFUNCTION_ATOM(load);
	ASFUNCTION(close);
	ASFUNCTION(_getDataFormat);
	ASFUNCTION_ATOM(_getData);
	ASFUNCTION(_setData);
	ASFUNCTION(_setDataFormat);
	ASPROPERTY_GETTER_SETTER(uint32_t, bytesLoaded);
	ASPROPERTY_GETTER_SETTER(uint32_t, bytesTotal);
};

class Responder: public ASObject
{
private:
	asAtomR result;
	asAtomR status;
public:
	Responder(Class_base* c):ASObject(c){}
	static void sinit(Class_base*);
	void finalize();
	ASFUNCTION_ATOM(_constructor);
	ASFUNCTION_ATOM(onResult);
};

class NetConnection: public EventDispatcher, public IThreadJob
{
friend class NetStream;
private:
	enum PROXY_TYPE { PT_NONE, PT_HTTP, PT_CONNECT_ONLY, PT_CONNECT, PT_BEST };
	//Indicates whether the application is connected to a server through a persistent RMTP connection/HTTP server with Flash Remoting
	bool _connected;
	tiny_string protocol;
	URLInfo uri;
	//Data for remoting support (NetConnection::call)
	// The message data to be sent asynchronously
	std::vector<uint8_t> messageData;
	Spinlock downloaderLock;
	Downloader* downloader;
	_NR<Responder> responder;
	uint32_t messageCount;
	//The connection is to a flash media server
	ObjectEncoding::ENCODING objectEncoding;
	PROXY_TYPE proxyType;
	//IThreadJob interface
	void execute();
	void threadAbort();
	void jobFence();
public:
	NetConnection(Class_base* c);
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION_ATOM(_constructor);
	ASFUNCTION(connect);
	ASFUNCTION(call);
	ASFUNCTION(_getConnected);
	ASFUNCTION(_getConnectedProxyType);
	ASFUNCTION(_getDefaultObjectEncoding);
	ASFUNCTION(_setDefaultObjectEncoding);
	ASFUNCTION(_getObjectEncoding);
	ASFUNCTION(_setObjectEncoding);
	ASFUNCTION(_getProtocol);
	ASFUNCTION(_getProxyType);
	ASFUNCTION(_setProxyType);
	ASFUNCTION(_getURI);
	ASFUNCTION(close);
	ASPROPERTY_GETTER_SETTER(NullableRef<ASObject>,client);
};

class NetStreamAppendBytesAction: public ASObject
{
public:
	NetStreamAppendBytesAction(Class_base* c):ASObject(c){}
	static void sinit(Class_base*);
};


class SoundTransform;
class NetStream: public EventDispatcher, public IThreadJob, public ITickJob
{
private:
	bool tickStarted;
	//Indicates whether the NetStream is paused
	bool paused;
	//Indicates whether the NetStream has been closed/threadAborted. This is reset at every play() call.
	//We initialize this value to true, so we can check that play() hasn't been called without being closed first.
	volatile bool closed;

	uint32_t streamTime;
	URLInfo url;
	double frameRate;
	//The NetConnection used by this NetStream
	_NR<NetConnection> connection;
	Downloader* downloader;
	VideoDecoder* videoDecoder;
	AudioDecoder* audioDecoder;
	AudioStream *audioStream;
	// only used when in DataGenerationMode
	StreamCache* datagenerationfile;
	bool datagenerationthreadstarted;
	Mutex mutex;
	Mutex countermutex;
	//IThreadJob interface for long jobs
	void execute();
	void threadAbort();
	void jobFence();
	//ITickJob interface to frame advance
	void tick();
	void tickFence();
	bool isReady() const;
	_NR<ASObject> client;

	ASPROPERTY_GETTER_SETTER(NullableRef<SoundTransform>,soundTransform);
	number_t oldVolume;

	enum CONNECTION_TYPE { CONNECT_TO_FMS=0, DIRECT_CONNECTIONS };
	CONNECTION_TYPE peerID;

	bool checkPolicyFile;
	bool rawAccessAllowed;

	uint32_t framesdecoded;
	uint32_t prevstreamtime;
	number_t playbackBytesPerSecond;
	number_t maxBytesPerSecond;

	struct bytespertime {
		uint64_t timestamp;
		uint32_t bytesread;
	};
	std::deque<bytespertime> currentBytesPerSecond;
	enum DATAGENERATION_EXPECT_TYPE { DATAGENERATION_HEADER=0,DATAGENERATION_PREVTAG,DATAGENERATION_FLVTAG };
	DATAGENERATION_EXPECT_TYPE datagenerationexpecttype;
	_NR<ByteArray> datagenerationbuffer;
public:
	NetStream(Class_base* c);
	~NetStream();
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION_ATOM(_constructor);
	ASFUNCTION(play);
	ASFUNCTION(play2);
	ASFUNCTION(resume);
	ASFUNCTION(pause);
	ASFUNCTION(togglePause);
	ASFUNCTION(close);
	ASFUNCTION(seek);
	ASFUNCTION(_getBytesLoaded);
	ASFUNCTION(_getBytesTotal);
	ASFUNCTION(_getTime);
	ASFUNCTION(_getCurrentFPS);
	ASFUNCTION_ATOM(_getClient);
	ASFUNCTION(_setClient);
	ASFUNCTION(_getCheckPolicyFile);
	ASFUNCTION(_setCheckPolicyFile);
	ASFUNCTION(attach);
	ASFUNCTION(appendBytes);
	ASFUNCTION(appendBytesAction);
	ASFUNCTION(_getInfo);
	ASPROPERTY_GETTER(number_t, backBufferLength);
	ASPROPERTY_GETTER_SETTER(number_t, backBufferTime);
	ASPROPERTY_GETTER(number_t, bufferLength);
	ASPROPERTY_GETTER_SETTER(number_t, bufferTime);
	ASPROPERTY_GETTER_SETTER(number_t, bufferTimeMax);
	ASPROPERTY_GETTER_SETTER(number_t, maxPauseBufferTime);
	ASPROPERTY_GETTER_SETTER(bool, useHardwareDecoder);

	void sendClientNotification(const tiny_string& name, std::list<asAtomR> &arglist);

	//Interface for video
	/**
	  	Get the frame width

		@pre lock on the object should be acquired and object should be ready
		@return the frame width
	*/
	uint32_t getVideoWidth() const;
	/**
	  	Get the frame height

		@pre lock on the object should be acquired and object should be ready
		@return the frame height
	*/
	uint32_t getVideoHeight() const;
	/**
	  	Get the frame rate

		@pre lock on the object should be acquired and object should be ready
		@return the frame rate
	*/
	double getFrameRate();
	/**
	  	Get the texture containing the current video Frame
		@pre lock on the object should be acquired and object should be ready
		@return a TextureChunk ready to be blitted
	*/
	const TextureChunk& getTexture() const;
	/**
	  	Get the stream time

		@pre lock on the object should be acquired and object should be ready
		@return the stream time
	*/
	uint32_t getStreamTime();
	/**
	  	Get the length of loaded data

		@pre lock on the object should be acquired and object should be ready
		@return the length of loaded data
	*/
	uint32_t getReceivedLength();
	/**
	  	Get the length of loaded data

		@pre lock on the object should be acquired and object should be ready
		@return the total length of the data
	*/
	uint32_t getTotalLength();
	/**
	  	Acquire the mutex to guarantee validity of data

		@return true if the lock has been acquired
	*/
	bool lockIfReady();
	/**
	  	Release the lock

		@pre the object should be locked
	*/
	void unlock();
};

class LocalConnection: public EventDispatcher
{
public:
	LocalConnection(Class_base* c);
	static void sinit(Class_base*);
	ASFUNCTION_ATOM(_constructor);
	ASPROPERTY_GETTER(bool,isSupported);
	ASFUNCTION(allowDomain);
	ASFUNCTION(allowInsecureDomain);
	ASFUNCTION(send);
	ASFUNCTION(connect);
	ASFUNCTION(close);
	ASFUNCTION(domain);
	ASPROPERTY_GETTER_SETTER(_NR<ASObject>,client);
};

class NetGroup: public EventDispatcher
{
public:
	NetGroup(Class_base* c);
	static void sinit(Class_base*);
	ASFUNCTION_ATOM(_constructor);
};

class FileReference: public EventDispatcher
{
public:
	FileReference(Class_base* c);
	static void sinit(Class_base*);
	ASFUNCTION_ATOM(_constructor);
};
class ASSocket: public EventDispatcher, IDataInput, IDataOutput
{
public:
	ASSocket(Class_base* c);
	static void sinit(Class_base*);
	ASFUNCTION_ATOM(_constructor);
};

class DRMManager: public EventDispatcher
{
public:
	DRMManager(Class_base* c);
	static void sinit(Class_base*);
	ASPROPERTY_GETTER(bool,isSupported);
};

ASObject* navigateToURL(ASObject* obj,ASObject* const* args, const unsigned int argslen);
ASObject* sendToURL(ASObject* obj,ASObject* const* args, const unsigned int argslen);
ASObject* registerClassAlias(ASObject* obj,ASObject* const* args, const unsigned int argslen);
asAtomR getClassByAlias(SystemState* sys, asAtomR& obj, std::vector<asAtomR>& args, const unsigned int argslen);

}

#endif /* SCRIPTING_FLASH_NET_FLASHNET_H */
