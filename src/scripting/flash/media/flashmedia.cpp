/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2010-2013  Alessandro Pignotti (a.pignotti@sssup.it)

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
#include "scripting/flash/media/flashmedia.h"
#include "scripting/class.h"
#include "compat.h"
#include <iostream>
#include "backends/audio.h"
#include "backends/rendering.h"
#include "backends/streamcache.h"
#include "scripting/argconv.h"
#include <unistd.h>

using namespace lightspark;
using namespace std;

SoundTransform::SoundTransform(Class_base* c): ASObject(c,T_OBJECT,SUBTYPE_SOUNDTRANSFORM),volume(1.0),pan(0.0),leftToLeft(1.0),leftToRight(0),rightToLeft(0),rightToRight(1.0)
{
}

void SoundTransform::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED | CLASS_FINAL);
	REGISTER_GETTER_SETTER(c,volume);
	REGISTER_GETTER_SETTER(c,pan);
	REGISTER_GETTER_SETTER(c,leftToLeft);
	REGISTER_GETTER_SETTER(c,leftToRight);
	REGISTER_GETTER_SETTER(c,rightToLeft);
	REGISTER_GETTER_SETTER(c,rightToRight);
}

ASFUNCTIONBODY_GETTER_SETTER(SoundTransform,volume)
ASFUNCTIONBODY_GETTER_SETTER(SoundTransform,pan)
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(SoundTransform,leftToLeft)
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(SoundTransform,leftToRight)
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(SoundTransform,rightToLeft)
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(SoundTransform,rightToRight)

ASFUNCTIONBODY(SoundTransform,_constructor)
{
	SoundTransform* th=Class<SoundTransform>::cast(obj);
	assert_and_throw(argslen<=2);
	th->volume = 1.0;
	th->pan = 0.0;
	if(0 < argslen)
		th->volume = ArgumentConversion<number_t>::toConcrete(args[0]);
	if(1 < argslen)
		th->pan = ArgumentConversion<number_t>::toConcrete(args[1]);
	return NULL;
}

void Video::sinit(Class_base* c)
{
	CLASS_SETUP(c, DisplayObject, _constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("videoWidth","",Class<IFunction>::getFunction(c->getSystemState(),_getVideoWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("videoHeight","",Class<IFunction>::getFunction(c->getSystemState(),_getVideoHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),Video::_getWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),Video::_setWidth),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),Video::_getHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),Video::_setHeight),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("attachNetStream","",Class<IFunction>::getFunction(c->getSystemState(),attachNetStream),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("clear","",Class<IFunction>::getFunction(c->getSystemState(),clear),NORMAL_METHOD,true);
	REGISTER_GETTER_SETTER(c, deblocking);
	REGISTER_GETTER_SETTER(c, smoothing);
}

ASFUNCTIONBODY_GETTER_SETTER(Video, deblocking);
ASFUNCTIONBODY_GETTER_SETTER(Video, smoothing);

void Video::buildTraits(ASObject* o)
{
}

void Video::finalize()
{
	DisplayObject::finalize();
	netStream.reset();
}

Video::Video(Class_base* c, uint32_t w, uint32_t h)
	: DisplayObject(c),width(w),height(h),videoWidth(0),videoHeight(0),
	  initialized(false),netStream(NullRef),deblocking(0),smoothing(false)
{
}

Video::~Video()
{
}

void Video::renderImpl(RenderContext& ctxt) const
{
	Mutex::Lock l(mutex);
	if(skipRender())
		return;

	//Video is especially optimized for GL rendering
	//It needs special treatment for SOFTWARE contextes
	if(ctxt.contextType != RenderContext::GL)
	{
		LOG(LOG_NOT_IMPLEMENTED, "Video::renderImpl on SOFTWARE context is not yet supported");
		return;
	}

	if(!netStream.isNull() && netStream->lockIfReady())
	{
		//All operations here should be non blocking
		//Get size
		videoWidth=netStream->getVideoWidth();
		videoHeight=netStream->getVideoHeight();

		const MATRIX totalMatrix=getConcatenatedMatrix();
		float m[16];
		totalMatrix.get4DMatrix(m);
		ctxt.lsglLoadMatrixf(m);

		//Enable YUV to RGB conversion
		//width and height will not change now (the Video mutex is acquired)
		ctxt.renderTextured(netStream->getTexture(), 0, 0, width, height,
			clippedAlpha(), RenderContext::YUV_MODE);
		
		netStream->unlock();
	}
}

bool Video::boundsRect(number_t& xmin, number_t& xmax, number_t& ymin, number_t& ymax) const
{
	xmin=0;
	xmax=width;
	ymin=0;
	ymax=height;
	return true;
}

ASFUNCTIONBODY(Video,_constructor)
{
	Video* th=Class<Video>::cast(obj);
	assert_and_throw(argslen<=2);
	if(0 < argslen)
		th->width=args[0]->toInt();
	if(1 < argslen)
		th->height=args[1]->toInt();
	return NULL;
}

ASFUNCTIONBODY(Video,_getVideoWidth)
{
	Video* th=Class<Video>::cast(obj);
	return abstract_i(obj->getSystemState(),th->videoWidth);
}

ASFUNCTIONBODY(Video,_getVideoHeight)
{
	Video* th=Class<Video>::cast(obj);
	return abstract_i(obj->getSystemState(),th->videoHeight);
}

ASFUNCTIONBODY(Video,_getWidth)
{
	Video* th=Class<Video>::cast(obj);
	return abstract_i(obj->getSystemState(),th->width);
}

ASFUNCTIONBODY(Video,_setWidth)
{
	Video* th=Class<Video>::cast(obj);
	Mutex::Lock l(th->mutex);
	assert_and_throw(argslen==1);
	th->width=args[0]->toInt();
	return NULL;
}

ASFUNCTIONBODY(Video,_getHeight)
{
	Video* th=Class<Video>::cast(obj);
	return abstract_i(obj->getSystemState(),th->height);
}

ASFUNCTIONBODY(Video,_setHeight)
{
	Video* th=Class<Video>::cast(obj);
	assert_and_throw(argslen==1);
	Mutex::Lock l(th->mutex);
	th->height=args[0]->toInt();
	return NULL;
}

ASFUNCTIONBODY(Video,attachNetStream)
{
	Video* th=Class<Video>::cast(obj);
	assert_and_throw(argslen==1);
	if(args[0]->getObjectType()==T_NULL || args[0]->getObjectType()==T_UNDEFINED) //Drop the connection
	{
		Mutex::Lock l(th->mutex);
		th->netStream=NullRef;
		return NULL;
	}

	//Validate the parameter
	if(!args[0]->getClass()->isSubClass(Class<NetStream>::getClass(obj->getSystemState())))
		throw RunTimeException("Type mismatch in Video::attachNetStream");

	//Acquire the netStream
	Mutex::Lock l(th->mutex);
	th->netStream=_IMR(Class<NetStream>::cast(args[0]));
	return NULL;
}
ASFUNCTIONBODY(Video,clear)
{
	LOG(LOG_NOT_IMPLEMENTED,"clear is not implemented");
	return NULL;
}

_NR<DisplayObject> Video::hitTestImpl(_NR<DisplayObject> last, number_t x, number_t y, DisplayObject::HIT_TYPE type)
{
	//TODO: support masks
	if(x>=0 && x<=width && y>=0 && y<=height)
		return last;
	else
		return NullRef;
}

Sound::Sound(Class_base* c)
	:EventDispatcher(c),downloader(NULL),soundData(new MemoryStreamCache(c->getSystemState())),
	 container(true),format(CODEC_NONE, 0, 0),bytesLoaded(0),bytesTotal(0),length(60*1000)
{
	subtype=SUBTYPE_SOUND;
}

Sound::Sound(Class_base* c, _R<StreamCache> data, AudioFormat _format)
	:EventDispatcher(c),downloader(NULL),soundData(data),
	 container(false),format(_format),
	 bytesLoaded(soundData->getReceivedLength()),
	 bytesTotal(soundData->getReceivedLength()),length(60*1000)
{
	subtype=SUBTYPE_SOUND;
}

Sound::~Sound()
{
	if(downloader && getSys()->downloadManager)
		getSys()->downloadManager->destroy(downloader);
}

void Sound::sinit(Class_base* c)
{
	CLASS_SETUP(c, EventDispatcher, _constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("load","",Class<IFunction>::getFunction(c->getSystemState(),load),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("play","",Class<IFunction>::getFunction(c->getSystemState(),play),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("close","",Class<IFunction>::getFunction(c->getSystemState(),close),NORMAL_METHOD,true);
	REGISTER_GETTER(c,bytesLoaded);
	REGISTER_GETTER(c,bytesTotal);
	REGISTER_GETTER(c,length);
}

void Sound::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY_ATOM(Sound,_constructor)
{
	std::vector<asAtomR> empty;
	EventDispatcher::_constructor(sys,obj, empty, 0);

	if (argslen>0)
		Sound::load(sys,obj, args, argslen);

	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(Sound,load)
{
	Sound* th=obj->as<Sound>();
	_NR<URLRequest> urlRequest;
	_NR<SoundLoaderContext> context;
	
	ARG_UNPACK_ATOM(urlRequest)(context,NullRef);
	if (!urlRequest.isNull())
	{
		th->url = urlRequest->getRequestURL();
		urlRequest->getPostData(th->postData);
	}
	_R<StreamCache> c(_MR(new MemoryStreamCache(th->getSystemState())));
	th->soundData = c;

	if(!th->url.isValid())
	{
		//Notify an error during loading
		getVm(th->getSystemState())->addEvent(_IMR(th),_MR(Class<IOErrorEvent>::getInstanceS(th->getSystemState())));
		return _MAR(asAtom::invalidAtom);
	}

	//The URL is valid so we can start the download

	if(th->postData.empty())
	{
		//This is a GET request
		//Use disk cache our downloaded files
		th->downloader=th->getSystemState()->downloadManager->download(th->url, th->soundData, th);
	}
	else
	{
		list<tiny_string> headers=urlRequest->getHeaders();
		th->downloader=th->getSystemState()->downloadManager->downloadWithData(th->url,
				th->soundData, th->postData, headers, th);
		//Clean up the postData for the next load
		th->postData.clear();
	}
	if(th->downloader->hasFailed())
	{
		getVm(th->getSystemState())->addEvent(_IMR(th),_MR(Class<IOErrorEvent>::getInstanceS(th->getSystemState())));
	}
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY(Sound,play)
{
	Sound* th=Class<Sound>::cast(obj);
	number_t startTime;
	ARG_UNPACK(startTime, 0);
	//TODO: use startTime
	if(startTime!=0)
		LOG(LOG_NOT_IMPLEMENTED,"startTime not supported in Sound::play");

	th->incRef();
	if (th->container)
		return Class<SoundChannel>::getInstanceS(obj->getSystemState(),th->soundData);
	else
		return Class<SoundChannel>::getInstanceS(obj->getSystemState(),th->soundData, th->format);
}

ASFUNCTIONBODY(Sound,close)
{
	Sound* th=Class<Sound>::cast(obj);
	if(th->downloader)
		th->downloader->stop();

	return NULL;
}

void Sound::setBytesTotal(uint32_t b)
{
	bytesTotal=b;
}

void Sound::setBytesLoaded(uint32_t b)
{
	if(b!=bytesLoaded)
	{
		bytesLoaded=b;
		getVm(getSystemState())->addEvent(_IMR(this),_MR(Class<ProgressEvent>::getInstanceS(getSystemState(),bytesLoaded,bytesTotal)));
		if(bytesLoaded==bytesTotal)
		{
			getVm(getSystemState())->addEvent(_IMR(this),_MR(Class<Event>::getInstanceS(getSystemState(),"complete")));
		}
	}
}

ASFUNCTIONBODY_GETTER(Sound,bytesLoaded);
ASFUNCTIONBODY_GETTER(Sound,bytesTotal);
ASFUNCTIONBODY_GETTER(Sound,length);

void SoundMixer::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setDeclaredMethodByQName("stopAll","",Class<IFunction>::getFunction(c->getSystemState(),stopAll),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("computeSpectrum","",Class<IFunction>::getFunction(c->getSystemState(),computeSpectrum),NORMAL_METHOD,false);
	REGISTER_GETTER_SETTER(c,bufferTime);
	REGISTER_GETTER_SETTER(c,soundTransform);
}
ASFUNCTIONBODY_GETTER_SETTER(SoundMixer,bufferTime);
ASFUNCTIONBODY_GETTER_SETTER(SoundMixer,soundTransform);

ASFUNCTIONBODY(SoundMixer,stopAll)
{
	LOG(LOG_NOT_IMPLEMENTED,"SoundMixer.stopAll does nothing");
	return NULL;
}
ASFUNCTIONBODY(SoundMixer,computeSpectrum)
{
	_NR<ByteArray> output;
	bool FFTMode;
	int stretchFactor;
	ARG_UNPACK (output) (FFTMode,false) (stretchFactor,0);
	output->setLength(0);
	output->setPosition(0);
	for (int i = 0; i < 4*512; i++) // 512 floats
		output->writeByte(0);
	output->setPosition(0);
	LOG(LOG_NOT_IMPLEMENTED,"SoundMixer.computeSpectrum not implemented");
	return NULL;
}

void SoundLoaderContext::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED);
	REGISTER_GETTER_SETTER(c,bufferTime);
	REGISTER_GETTER_SETTER(c,checkPolicyFile);
}

ASFUNCTIONBODY(SoundLoaderContext,_constructor)
{
	SoundLoaderContext* th=Class<SoundLoaderContext>::cast(obj);
	assert_and_throw(argslen<=2);
	th->bufferTime = 1000;
	th->checkPolicyFile = false;
	if(0 < argslen)
		th->bufferTime = ArgumentConversion<number_t>::toConcrete(args[0]);
	if(1 < argslen)
		th->checkPolicyFile = ArgumentConversion<bool>::toConcrete(args[1]);
	return NULL;
}

ASFUNCTIONBODY_GETTER_SETTER(SoundLoaderContext,bufferTime);
ASFUNCTIONBODY_GETTER_SETTER(SoundLoaderContext,checkPolicyFile);

SoundChannel::SoundChannel(Class_base* c, _NR<StreamCache> _stream, AudioFormat _format)
	: EventDispatcher(c),stream(_stream),stopped(false),audioDecoder(NULL),audioStream(NULL),
	format(_format),oldVolume(-1.0),soundTransform(_MR(Class<SoundTransform>::getInstanceS(c->getSystemState()))),
	leftPeak(1),position(0),rightPeak(1)
{
	subtype=SUBTYPE_SOUNDCHANNEL;
	if (!stream.isNull())
	{
		// Start playback
		this->incRef();
		getSys()->addJob(this);
	}
}

SoundChannel::~SoundChannel()
{
	threadAbort();
}

void SoundChannel::sinit(Class_base* c)
{
	CLASS_SETUP(c, EventDispatcher, _constructor, CLASS_SEALED | CLASS_FINAL);
	c->setDeclaredMethodByQName("stop","",Class<IFunction>::getFunction(c->getSystemState(),stop),NORMAL_METHOD,true);

	REGISTER_GETTER(c,leftPeak);
	REGISTER_GETTER(c,position);
	REGISTER_GETTER(c,rightPeak);
	REGISTER_GETTER_SETTER(c,soundTransform);
}

ASFUNCTIONBODY_GETTER_NOT_IMPLEMENTED(SoundChannel,leftPeak);
ASFUNCTIONBODY_GETTER(SoundChannel,position);
ASFUNCTIONBODY_GETTER_NOT_IMPLEMENTED(SoundChannel,rightPeak);
ASFUNCTIONBODY_GETTER_SETTER_CB(SoundChannel,soundTransform,validateSoundTransform);

void SoundChannel::buildTraits(ASObject* o)
{
}

void SoundChannel::finalize()
{
	EventDispatcher::finalize();
	soundTransform.reset();
}

void SoundChannel::validateSoundTransform(_NR<SoundTransform> oldValue)
{
	if (soundTransform.isNull())
	{
		soundTransform = oldValue;
		throwError<TypeError>(kNullPointerError, "soundTransform");
	}
}

ASFUNCTIONBODY_ATOM(SoundChannel, _constructor)
{
	std::vector<asAtomR> empty;
	EventDispatcher::_constructor(sys,obj, empty, 0);

	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY(SoundChannel, stop)
{
	SoundChannel* th=Class<SoundChannel>::cast(obj);
	th->threadAbort();
	return NULL;
}

void SoundChannel::execute()
{
	playStream();
}

void SoundChannel::playStream()
{
	assert(!stream.isNull());
	std::streambuf *sbuf = stream->createReader();
	istream s(sbuf);
	s.exceptions ( istream::failbit | istream::badbit );

	bool waitForFlush=true;
	StreamDecoder* streamDecoder=NULL;
	//We need to catch possible EOF and other error condition in the non reliable stream
	try
	{
#ifdef ENABLE_LIBAVCODEC
		streamDecoder=new FFMpegStreamDecoder(this->getSystemState()->getEngineData(),s,&format,stream->hasTerminated() ? stream->getReceivedLength() : -1);
		if(!streamDecoder->isValid())
			threadAbort();

		while(!ACQUIRE_READ(stopped))
		{
			bool decodingSuccess=streamDecoder->decodeNextFrame();
			if(decodingSuccess==false)
				break;

			if(audioDecoder==NULL && streamDecoder->audioDecoder)
				audioDecoder=streamDecoder->audioDecoder;

			if(audioStream==NULL && audioDecoder && audioDecoder->isValid())
				audioStream=getSystemState()->audioManager->createStream(audioDecoder,false);

			// TODO: check the position only when the getter is called
			if(audioStream)
				position=audioStream->getPlayedTime();

			if(audioStream)
			{
				//TODO: use soundTransform->pan
				if(soundTransform && soundTransform->volume != oldVolume)
				{
					audioStream->setVolume(soundTransform->volume);
					oldVolume = soundTransform->volume;
				}
			}
			
			if(threadAborting)
				throw JobTerminationException();
		}
#endif //ENABLE_LIBAVCODEC
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR, "Exception in SoundChannel " << e.cause);
		threadAbort();
		waitForFlush=false;
	}
	catch(JobTerminationException& e)
	{
		waitForFlush=false;
	}
	catch(exception& e)
	{
		LOG(LOG_ERROR, _("Exception in reading SoundChannel: ")<<e.what());
	}
	if(waitForFlush)
	{
		//Put the decoders in the flushing state and wait for the complete consumption of contents
		if(audioDecoder)
		{
			audioDecoder->setFlushing();
			audioDecoder->waitFlushed();
		}
	}

	{
		Locker l(mutex);
		audioDecoder=NULL;
		delete audioStream;
		audioStream=NULL;
	}
	delete streamDecoder;
	delete sbuf;

	if (!ACQUIRE_READ(stopped))
	{
		getVm(getSystemState())->addEvent(_IMR(this),_MR(Class<Event>::getInstanceS(getSystemState(),"soundComplete")));
	}
}


void SoundChannel::jobFence()
{
	this->decRef();
}

void SoundChannel::threadAbort()
{
	RELEASE_WRITE(stopped,true);
	Locker l(mutex);
	if(audioDecoder)
	{
		//Clear everything we have in buffers, discard all frames
		audioDecoder->setFlushing();
		audioDecoder->skipAll();
	}
}

void StageVideo::sinit(Class_base *c)
{
	CLASS_SETUP(c, EventDispatcher, _constructorNotInstantiatable, CLASS_SEALED);
	c->setDeclaredMethodByQName("videoWidth","",Class<IFunction>::getFunction(c->getSystemState(),_getVideoWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("videoHeight","",Class<IFunction>::getFunction(c->getSystemState(),_getVideoHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("attachNetStream","",Class<IFunction>::getFunction(c->getSystemState(),attachNetStream),NORMAL_METHOD,true);
}


void StageVideo::finalize()
{
	netStream.reset();
}

ASFUNCTIONBODY(StageVideo,_getVideoWidth)
{
	StageVideo* th=Class<StageVideo>::cast(obj);
	return abstract_i(obj->getSystemState(),th->videoWidth);
}

ASFUNCTIONBODY(StageVideo,_getVideoHeight)
{
	StageVideo* th=Class<StageVideo>::cast(obj);
	return abstract_i(obj->getSystemState(),th->videoHeight);
}

ASFUNCTIONBODY(StageVideo,attachNetStream)
{
	StageVideo* th=Class<StageVideo>::cast(obj);
	assert_and_throw(argslen==1);
	if(args[0]->getObjectType()==T_NULL || args[0]->getObjectType()==T_UNDEFINED) //Drop the connection
	{
		Mutex::Lock l(th->mutex);
		th->netStream=NullRef;
		return NULL;
	}

	//Validate the parameter
	if(!args[0]->getClass()->isSubClass(Class<NetStream>::getClass(obj->getSystemState())))
		throw RunTimeException("Type mismatch in StageVideo::attachNetStream");

	//Acquire the netStream
	Mutex::Lock l(th->mutex);
	th->netStream=_IMR(Class<NetStream>::cast(args[0]));
	return NULL;
}

void StageVideoAvailability::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED | CLASS_FINAL);
	c->setVariableByQName("AVAILABLE","",abstract_s(c->getSystemState(),"available"),DECLARED_TRAIT);
	c->setVariableByQName("UNAVAILABLE","",abstract_s(c->getSystemState(),"unavailable"),DECLARED_TRAIT);
}

void VideoStatus::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED | CLASS_FINAL);
	c->setVariableByQName("ACCELERATED","",abstract_s(c->getSystemState(),"accelerated"),DECLARED_TRAIT);
	c->setVariableByQName("SOFTWARE","",abstract_s(c->getSystemState(),"software"),DECLARED_TRAIT);
	c->setVariableByQName("UNAVAILABLE","",abstract_s(c->getSystemState(),"unavailable"),DECLARED_TRAIT);
}
void Microphone::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_SEALED | CLASS_FINAL);
	REGISTER_GETTER(c,isSupported);
	c->setDeclaredMethodByQName("getMicrophone","",Class<IFunction>::getFunction(c->getSystemState(),getMicrophone),NORMAL_METHOD,false);

}
ASFUNCTIONBODY_GETTER_NOT_IMPLEMENTED(Microphone,isSupported)

ASFUNCTIONBODY(Microphone,getMicrophone)
{
	LOG(LOG_NOT_IMPLEMENTED,"Microphone.getMicrophone always returns null");
	return NULL;
}
