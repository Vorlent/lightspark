/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2011-2013  Alessandro Pignotti (a.pignotti@sssup.it)
    Copyright (C) 2011       Matthias Gehre (M.Gehre@gmx.de)

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

#include "platforms/engineutils.h"
#include "swf.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_mixer.h>
#include "backends/input.h"
#include "backends/rendering.h"
#include "backends/lsopengl.h"
#include "platforms/engineutils.h"

//The interpretation of texture data change with the endianness
#if __BYTE_ORDER == __BIG_ENDIAN
#define GL_UNSIGNED_INT_8_8_8_8_HOST GL_UNSIGNED_INT_8_8_8_8_REV
#else
#define GL_UNSIGNED_INT_8_8_8_8_HOST GL_UNSIGNED_BYTE
#endif

using namespace std;
using namespace lightspark;

uint32_t EngineData::userevent = (uint32_t)-1;
Thread* EngineData::mainLoopThread = NULL;
bool EngineData::mainthread_running = false;
bool EngineData::sdl_needinit = true;
Semaphore EngineData::mainthread_initialized(0);
EngineData::EngineData() : currentPixelBuffer(0),currentPixelBufferOffset(0),currentPixelBufPtr(NULL),pixelBufferWidth(0),pixelBufferHeight(0),widget(0), width(0), height(0),needrenderthread(true)
{
}

EngineData::~EngineData()
{
	if (currentPixelBufPtr) {
		free(currentPixelBufPtr);
		currentPixelBufPtr = 0;
	}
}
bool EngineData::mainloop_handleevent(SDL_Event* event,SystemState* sys)
{
	if (event->type == LS_USEREVENT_INIT)
	{
		sys = (SystemState*)event->user.data1;
		setTLSSys(sys);
	}
	else if (event->type == LS_USEREVENT_EXEC)
	{
		if (event->user.data1)
			((void (*)(SystemState*))event->user.data1)(sys);
	}
	else if (event->type == LS_USEREVENT_QUIT)
	{
		setTLSSys(NULL);
		SDL_Quit();
		return true;
	}
	else
	{
		if (sys && sys->getInputThread() && sys->getInputThread()->handleEvent(event))
			return false;
		switch (event->type)
		{
			case SDL_WINDOWEVENT:
			{
				switch (event->window.event)
				{
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						if (sys && sys->getRenderThread())
							sys->getRenderThread()->requestResize(event->window.data1,event->window.data2,false);
						break;
					case SDL_WINDOWEVENT_EXPOSED:
					{
						//Signal the renderThread
						if (sys && sys->getRenderThread())
							sys->getRenderThread()->draw(sys->isOnError());
						break;
					}
						
					default:
						break;
				}
				break;
			}
			case SDL_QUIT:
				sys->setShutdownFlag();
				break;
		}
	}
	return false;
}

/* main loop handling */
static void mainloop_runner()
{
	bool sdl_available = !EngineData::sdl_needinit;
	
	if (EngineData::sdl_needinit)
	{
		if (SDL_WasInit(0)) // some part of SDL already was initialized
			sdl_available = !SDL_InitSubSystem ( SDL_INIT_VIDEO );
		else
			sdl_available = !SDL_Init ( SDL_INIT_VIDEO );
	}
	if (!sdl_available)
	{
		LOG(LOG_ERROR,"Unable to initialize SDL:"<<SDL_GetError());
		EngineData::mainthread_initialized.signal();
		return;
	}
	else
	{
		EngineData::mainthread_running = true;
		EngineData::mainthread_initialized.signal();
		SDL_Event event;
		while (SDL_WaitEvent(&event))
		{
			SystemState* sys = getSys();
			
			if (EngineData::mainloop_handleevent(&event,sys))
			{
				EngineData::mainthread_running = false;
				return;
			}
		}
	}
}
gboolean EngineData::mainloop_from_plugin(SystemState* sys)
{
	SDL_Event event;
	setTLSSys(sys);
	while (SDL_PollEvent(&event))
	{
		mainloop_handleevent(&event,sys);
	}
	setTLSSys(NULL);
	return G_SOURCE_CONTINUE;
}

/* This is not run in the linux plugin, as firefox
 * runs its own gtk_main, which we must not interfere with.
 */
bool EngineData::startSDLMain()
{
	assert(!mainLoopThread);
#ifdef HAVE_NEW_GLIBMM_THREAD_API
	mainLoopThread = Thread::create(sigc::ptr_fun(&mainloop_runner));
#else
	mainLoopThread = Thread::create(sigc::ptr_fun(&mainloop_runner),true);
#endif
	mainthread_initialized.wait();
	return mainthread_running;
}

void EngineData::initGLEW()
{
//For now GLEW does not work with GLES2
#ifndef ENABLE_GLES2
	//Now we can initialize GLEW
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		LOG(LOG_ERROR,_("Cannot initialize GLEW: cause ") << glewGetErrorString(err));
		throw RunTimeException("Rendering: Cannot initialize GLEW!");
	}

	if(!GLEW_VERSION_2_0)
	{
		LOG(LOG_ERROR,_("Video card does not support OpenGL 2.0... Aborting"));
		throw RunTimeException("Rendering: OpenGL driver does not support OpenGL 2.0");
	}
	if(!GLEW_ARB_framebuffer_object)
	{
		LOG(LOG_ERROR,"OpenGL does not support framebuffer objects!");
		throw RunTimeException("Rendering: OpenGL driver does not support framebuffer objects");
	}
#endif
}

void EngineData::showWindow(uint32_t w, uint32_t h)
{
	RecMutex::Lock l(mutex);

	assert(!widget);
	widget = createWidget(w,h);
	this->width = this->origwidth = w;
	this->height = this->origheight = h;
	
	// plugins create a hidden window that should not be shown
	if (widget && !(SDL_GetWindowFlags(widget) & SDL_WINDOW_HIDDEN))
		SDL_ShowWindow(widget);
	grabFocus();
	
}

void EngineData::showMouseCursor(SystemState* /*sys*/)
{
	SDL_ShowCursor(SDL_ENABLE);
}

void EngineData::hideMouseCursor(SystemState* /*sys*/)
{
	SDL_ShowCursor(SDL_DISABLE);
}

void EngineData::setClipboardText(const std::string txt)
{
	int ret = SDL_SetClipboardText(txt.c_str());
	if (ret == 0)
		LOG(LOG_INFO, "Copied error to clipboard");
	else
		LOG(LOG_ERROR, "copying text to clipboard failed:"<<SDL_GetError());
}

StreamCache *EngineData::createFileStreamCache(SystemState* sys)
{
	return new FileStreamCache(sys);
}

bool EngineData::getGLError(uint32_t &errorCode) const
{
	errorCode=glGetError();
	return errorCode!=GL_NO_ERROR;
}

uint8_t *EngineData::getCurrentPixBuf() const
{
#ifndef ENABLE_GLES2
	return (uint8_t*)currentPixelBufferOffset;
#else
	return currentPixelBufPtr;
#endif
	
}

uint8_t *EngineData::switchCurrentPixBuf(uint32_t w, uint32_t h)
{
#ifndef ENABLE_GLES2
	unsigned int nextBuffer = (currentPixelBuffer + 1)%2;
	exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(pixelBuffers[nextBuffer]);
	uint8_t* buf=(uint8_t*)exec_glMapBuffer_GL_PIXEL_UNPACK_BUFFER_GL_WRITE_ONLY();
	if(!buf)
		return NULL;
	uint8_t* alignedBuf=(uint8_t*)(uintptr_t((buf+15))&(~0xfL));

	currentPixelBufferOffset=alignedBuf-buf;
	currentPixelBuffer=nextBuffer;
	return alignedBuf;
#else
	//TODO See if a more elegant way of handling the non-PBO case can be found.
	//for now, each frame is uploaded one at a time synchronously to the server
	if(!currentPixelBufPtr)
		if(posix_memalign((void **)&currentPixelBufPtr, 16, w*h*4)) {
			LOG(LOG_ERROR, "posix_memalign could not allocate memory");
			return NULL;
		}
	return currentPixelBufPtr;
#endif
	
}

void EngineData::resizePixelBuffers(uint32_t w, uint32_t h)
{
	if(w<=pixelBufferWidth && h<=pixelBufferHeight)
		return;
	
	//Add enough room to realign to 16
	exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(pixelBuffers[0]);
	exec_glBufferData_GL_PIXEL_UNPACK_BUFFER_GL_STREAM_DRAW(w*h*4+16, 0);
	exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(pixelBuffers[1]);
	exec_glBufferData_GL_PIXEL_UNPACK_BUFFER_GL_STREAM_DRAW(w*h*4+16, 0);
	exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(0);
	pixelBufferWidth=w;
	pixelBufferHeight=h;
	if (currentPixelBufPtr) {
		free(currentPixelBufPtr);
		currentPixelBufPtr = 0;
	}
}

void EngineData::bindCurrentBuffer()
{
	exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(pixelBuffers[currentPixelBuffer]);
}


void EngineData::exec_glUniform1f(int location,float v0)
{
	glUniform1f(location,v0);
}

void EngineData::exec_glBindTexture_GL_TEXTURE_2D(uint32_t id)
{
	glBindTexture(GL_TEXTURE_2D, id);
}

void EngineData::exec_glVertexAttribPointer(uint32_t index,int32_t size, int32_t stride, const void* coords)
{
	glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, coords);
}

void EngineData::exec_glEnableVertexAttribArray(uint32_t index)
{
	glEnableVertexAttribArray(index);
}

void EngineData::exec_glDrawArrays_GL_TRIANGLES(int32_t first,int32_t count)
{
	glDrawArrays(GL_TRIANGLES,first,count);
}
void EngineData::exec_glDrawArrays_GL_LINE_STRIP(int32_t first,int32_t count)
{
	glDrawArrays(GL_LINE_STRIP,first,count);
}

void EngineData::exec_glDrawArrays_GL_TRIANGLE_STRIP(int32_t first, int32_t count)
{
	glDrawArrays(GL_TRIANGLE_STRIP,first,count);
}

void EngineData::exec_glDrawArrays_GL_LINES(int32_t first, int32_t count)
{
	glDrawArrays(GL_LINES,first,count);
}

void EngineData::exec_glDisableVertexAttribArray(uint32_t index)
{
	glDisableVertexAttribArray(index);
}

void EngineData::exec_glUniformMatrix4fv(int32_t location,int32_t count, bool transpose,const float* value)
{
	glUniformMatrix4fv(location, count, transpose, value);
}
void EngineData::exec_glBindBuffer_GL_PIXEL_UNPACK_BUFFER(uint32_t buffer)
{
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer);
}
uint8_t* EngineData::exec_glMapBuffer_GL_PIXEL_UNPACK_BUFFER_GL_WRITE_ONLY()
{
#ifndef ENABLE_GLES2
	return (uint8_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER,GL_WRITE_ONLY);
#else
	return NULL;
#endif
}
void EngineData::exec_glUnmapBuffer_GL_PIXEL_UNPACK_BUFFER()
{
#ifndef ENABLE_GLES2
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif
}
void EngineData::exec_glEnable_GL_TEXTURE_2D()
{
	glEnable(GL_TEXTURE_2D);
}
void EngineData::exec_glEnable_GL_BLEND()
{
	glEnable(GL_BLEND);
}

void EngineData::exec_glDisable_GL_TEXTURE_2D()
{
	glDisable(GL_TEXTURE_2D);
}
void EngineData::exec_glFlush()
{
	glFlush();
}

uint32_t EngineData::exec_glCreateShader_GL_FRAGMENT_SHADER()
{
	return glCreateShader(GL_FRAGMENT_SHADER);
}

uint32_t EngineData::exec_glCreateShader_GL_VERTEX_SHADER()
{
	return glCreateShader(GL_VERTEX_SHADER);
}

void EngineData::exec_glShaderSource(uint32_t shader, int32_t count, const char** name, int32_t* length)
{
	glShaderSource(shader,count,name,length);
}

void EngineData::exec_glCompileShader(uint32_t shader)
{
	glCompileShader(shader);
}

void EngineData::exec_glGetShaderInfoLog(uint32_t shader,int32_t bufSize,int32_t* length,char* infoLog)
{
	glGetShaderInfoLog(shader,bufSize,length,infoLog);
}

void EngineData::exec_glGetShaderiv_GL_COMPILE_STATUS(uint32_t shader,int32_t* params)
{
	glGetShaderiv(shader,GL_COMPILE_STATUS,params);
}

uint32_t EngineData::exec_glCreateProgram()
{
	return glCreateProgram();
}

void EngineData::exec_glBindAttribLocation(uint32_t program,uint32_t index, const char* name)
{
	glBindAttribLocation(program,index,name);
}

void EngineData::exec_glAttachShader(uint32_t program, uint32_t shader)
{
	glAttachShader(program,shader);
}

void EngineData::exec_glLinkProgram(uint32_t program)
{
	glLinkProgram(program);
}

void EngineData::exec_glGetProgramiv_GL_LINK_STATUS(uint32_t program,int32_t* params)
{
	glGetProgramiv(program,GL_LINK_STATUS,params);
}

void EngineData::exec_glBindFramebuffer_GL_FRAMEBUFFER(uint32_t framebuffer)
{
	glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
}

void EngineData::exec_glDeleteTextures(int32_t n,uint32_t* textures)
{
	glDeleteTextures(n,textures);
}

void EngineData::exec_glDeleteBuffers()
{
	glDeleteBuffers(2,pixelBuffers);
}

void EngineData::exec_glBlendFunc_GL_ONE_GL_ONE_MINUS_SRC_ALPHA()
{
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void EngineData::exec_glActiveTexture_GL_TEXTURE0()
{
	glActiveTexture(GL_TEXTURE0);
}

void EngineData::exec_glGenBuffers()
{
	glGenBuffers(2,pixelBuffers);
}

void EngineData::exec_glUseProgram(uint32_t program)
{
	glUseProgram(program);
}

int32_t EngineData::exec_glGetUniformLocation(uint32_t program,const char* name)
{
	return glGetUniformLocation(program,name);
}

void EngineData::exec_glUniform1i(int32_t location,int32_t v0)
{
	glUniform1i(location,v0);
}

void EngineData::exec_glGenTextures(int32_t n,uint32_t* textures)
{
	glGenTextures(n,textures);
}

void EngineData::exec_glViewport(int32_t x,int32_t y,int32_t width,int32_t height)
{
	glViewport(x,y,width,height);
}

void EngineData::exec_glBufferData_GL_PIXEL_UNPACK_BUFFER_GL_STREAM_DRAW(int32_t size,const void* data)
{
	glBufferData(GL_PIXEL_UNPACK_BUFFER,size, data,GL_STREAM_DRAW);
}

void EngineData::exec_glTexParameteri_GL_TEXTURE_2D_GL_TEXTURE_MIN_FILTER_GL_LINEAR()
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}
void EngineData::exec_glTexParameteri_GL_TEXTURE_2D_GL_TEXTURE_MAG_FILTER_GL_LINEAR()
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void EngineData::exec_glTexImage2D_GL_TEXTURE_2D_GL_UNSIGNED_BYTE(int32_t level,int32_t width, int32_t height,int32_t border, const void* pixels)
{
	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, border, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
}
void EngineData::exec_glTexImage2D_GL_TEXTURE_2D_GL_UNSIGNED_INT_8_8_8_8_HOST(int32_t level,int32_t width, int32_t height,int32_t border, const void* pixels)
{
	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, border, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_HOST, pixels);
}

void EngineData::exec_glDrawBuffer_GL_BACK()
{
	glDrawBuffer(GL_BACK);
}

void EngineData::exec_glClearColor(float red,float green,float blue,float alpha)
{
	glClearColor(red,green,blue,alpha);
}

void EngineData::exec_glClear_GL_COLOR_BUFFER_BIT()
{
	glClear(GL_COLOR_BUFFER_BIT);
}

void EngineData::exec_glPixelStorei_GL_UNPACK_ROW_LENGTH(int32_t param)
{
	glPixelStorei(GL_UNPACK_ROW_LENGTH,param);
}

void EngineData::exec_glPixelStorei_GL_UNPACK_SKIP_PIXELS(int32_t param)
{
	glPixelStorei(GL_UNPACK_SKIP_PIXELS,param);
}

void EngineData::exec_glPixelStorei_GL_UNPACK_SKIP_ROWS(int32_t param)
{
	glPixelStorei(GL_UNPACK_SKIP_ROWS,param);
}

void EngineData::exec_glTexSubImage2D_GL_TEXTURE_2D(int32_t level, int32_t xoffset, int32_t yoffset, int32_t width, int32_t height, const void* pixels, uint32_t w, uint32_t curX, uint32_t curY)
{
#ifndef ENABLE_GLES2
	glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_HOST, pixels);
#else
	//We need to copy the texture area to a contiguous memory region first,
	//as GLES2 does not support UNPACK state (skip pixels, skip rows, row_lenght).
	uint8_t *gdata = new uint8_t[4*width*height];
	for(int j=0;j<height;j++) {
		memcpy(gdata+4*j*width, ((uint8_t *)pixels)+4*w*(j+curY)+4*curX, width*4);
	}
	glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_HOST, gdata);
	delete[] gdata;
#endif
}
void EngineData::exec_glGetIntegerv_GL_MAX_TEXTURE_SIZE(int32_t* data)
{
	glGetIntegerv(GL_MAX_TEXTURE_SIZE,data);
}

void mixer_effect_ffmpeg_cb(int chan, void * stream, int len, void * udata)
{
	AudioStream *s = (AudioStream*)udata;
	if (!s)
		return;

	uint32_t readcount = 0;
	while (readcount < ((uint32_t)len))
	{
		uint32_t ret = s->getDecoder()->copyFrame((int16_t *)(((unsigned char*)stream)+readcount), ((uint32_t)len)-readcount);
		if (!ret)
			break;
		readcount += ret;
	}
}



int EngineData::audio_StreamInit(AudioStream* s)
{
	int mixer_channel = -1;

	uint32_t len = LIGHTSPARK_AUDIO_BUFFERSIZE;

	uint8_t *buf = new uint8_t[len];
	memset(buf,0,len);
	Mix_Chunk* chunk = Mix_QuickLoad_RAW(buf, len);


	mixer_channel = Mix_PlayChannel(-1, chunk, -1);
	Mix_RegisterEffect(mixer_channel, mixer_effect_ffmpeg_cb, NULL, s);
	Mix_Resume(mixer_channel);
	return mixer_channel;
}

void EngineData::audio_StreamPause(int channel, bool dopause)
{
	if (channel == -1)
		return;
	if(dopause)
		Mix_Pause(channel);
	else
		Mix_Resume(channel);
}

void EngineData::audio_StreamSetVolume(int channel, double volume)
{
	int curvolume = SDL_MIX_MAXVOLUME * volume;
	if (channel != -1)
		Mix_Volume(channel, curvolume);
}

void EngineData::audio_StreamDeinit(int channel)
{
	if (channel != -1)
		Mix_HaltChannel(channel);
}

bool EngineData::audio_ManagerInit()
{
	bool sdl_available = false;
	if (SDL_WasInit(0)) // some part of SDL already was initialized
		sdl_available = !SDL_InitSubSystem ( SDL_INIT_AUDIO );
	else
		sdl_available = !SDL_Init ( SDL_INIT_AUDIO );
	return sdl_available;
}

void EngineData::audio_ManagerCloseMixer()
{
	Mix_CloseAudio();
}

bool EngineData::audio_ManagerOpenMixer()
{
	return Mix_OpenAudio (audio_getSampleRate(), AUDIO_S16, 2, LIGHTSPARK_AUDIO_BUFFERSIZE) >= 0;
}

void EngineData::audio_ManagerDeinit()
{
	SDL_QuitSubSystem ( SDL_INIT_AUDIO );
	if (!SDL_WasInit(0))
		SDL_Quit ();
}

int EngineData::audio_getSampleRate()
{
	return MIX_DEFAULT_FREQUENCY;
}

IDrawable *EngineData::getTextRenderDrawable(const TextData &_textData, const MATRIX &_m, int32_t _x, int32_t _y, int32_t _w, int32_t _h, float _s, float _a, const std::vector<IDrawable::MaskData> &_ms)
{
	return NULL;
}
