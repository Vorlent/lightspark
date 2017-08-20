/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2012-2013  Alessandro Pignotti (a.pignotti@sssup.it)

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
#include "compat.h"
#include "swf.h"
#include "scripting/flash/display/DisplayObject.h"
#include "backends/rendering.h"
#include "backends/input.h"
#include "scripting/argconv.h"
#include "scripting/flash/geom/flashgeom.h"
#include "scripting/flash/accessibility/flashaccessibility.h"
#include "scripting/flash/display/BitmapData.h"
#include "scripting/flash/geom/flashgeom.h"

using namespace lightspark;
using namespace std;

ATOMIC_INT32(DisplayObject::instanceCount);

Vector2f DisplayObject::getXY()
{
	Vector2f ret;
	ret.x = getMatrix().getTranslateX();
	ret.y = getMatrix().getTranslateY();
	return ret;
}

bool DisplayObject::getBounds(number_t& xmin, number_t& xmax, number_t& ymin, number_t& ymax, const MATRIX& m) const
{
	if(!isConstructed())
		return false;

	bool ret=boundsRect(xmin,xmax,ymin,ymax);
	if(ret)
	{
		number_t tmpX[4];
		number_t tmpY[4];
		m.multiply2D(xmin,ymin,tmpX[0],tmpY[0]);
		m.multiply2D(xmax,ymin,tmpX[1],tmpY[1]);
		m.multiply2D(xmax,ymax,tmpX[2],tmpY[2]);
		m.multiply2D(xmin,ymax,tmpX[3],tmpY[3]);
		auto retX=minmax_element(tmpX,tmpX+4);
		auto retY=minmax_element(tmpY,tmpY+4);
		xmin=*retX.first;
		xmax=*retX.second;
		ymin=*retY.first;
		ymax=*retY.second;
	}
	return ret;
}

number_t DisplayObject::getNominalWidth()
{
	number_t xmin, xmax, ymin, ymax;

	if(!isConstructed())
		return 0;

	bool ret=boundsRect(xmin,xmax,ymin,ymax);
	return ret?(xmax-xmin):0;
}

number_t DisplayObject::getNominalHeight()
{
	number_t xmin, xmax, ymin, ymax;

	if(!isConstructed())
		return 0;

	bool ret=boundsRect(xmin,xmax,ymin,ymax);
	return ret?(ymax-ymin):0;
}

void DisplayObject::Render(RenderContext& ctxt)
{
	if(!isConstructed() || skipRender())
		return;

	renderImpl(ctxt);
}

DisplayObject::DisplayObject(Class_base* c):EventDispatcher(c),matrix(Class<Matrix>::getInstanceS(c->getSystemState())),tx(0),ty(0),rotation(0),
	sx(1),sy(1),alpha(1.0),isLoadedRoot(false),maskOf(),parent(),constructed(false),useLegacyMatrix(true),onStage(false),
	visible(true),mask(),invalidateQueueNext(),loaderInfo(),filters(Class<Array>::getInstanceSNoArgs(c->getSystemState())),hasChanged(true),cacheAsBitmap(false)
{
	subtype=SUBTYPE_DISPLAYOBJECT;
//	name = tiny_string("instance") + Integer::toString(ATOMIC_INCREMENT(instanceCount));
}

DisplayObject::~DisplayObject() {}

void DisplayObject::finalize()
{
	EventDispatcher::finalize();
	maskOf.reset();
	parent.reset();
	mask.reset();
	matrix.reset();
	loaderInfo.reset();
	invalidateQueueNext.reset();
	accessibilityProperties.reset();
	hasChanged = true;
}

void DisplayObject::sinit(Class_base* c)
{
	CLASS_SETUP(c, EventDispatcher, _constructorNotInstantiatable, CLASS_SEALED);
	c->setDeclaredMethodByQName("loaderInfo","",Class<IFunction>::getFunction(c->getSystemState(),_getLoaderInfo),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),_getWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),_setWidth),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleX","",Class<IFunction>::getFunction(c->getSystemState(),_getScaleX),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleX","",Class<IFunction>::getFunction(c->getSystemState(),_setScaleX),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleY","",Class<IFunction>::getFunction(c->getSystemState(),_getScaleY),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleY","",Class<IFunction>::getFunction(c->getSystemState(),_setScaleY),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleZ","",Class<IFunction>::getFunction(c->getSystemState(),_getScaleZ),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("scaleZ","",Class<IFunction>::getFunction(c->getSystemState(),_setScaleZ),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("x","",Class<IFunction>::getFunction(c->getSystemState(),_getX),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("x","",Class<IFunction>::getFunction(c->getSystemState(),_setX),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("y","",Class<IFunction>::getFunction(c->getSystemState(),_getY),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("y","",Class<IFunction>::getFunction(c->getSystemState(),_setY),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("z","",Class<IFunction>::getFunction(c->getSystemState(),_getZ),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("z","",Class<IFunction>::getFunction(c->getSystemState(),_setZ),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),_getHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),_setHeight),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("visible","",Class<IFunction>::getFunction(c->getSystemState(),_getVisible),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("visible","",Class<IFunction>::getFunction(c->getSystemState(),_setVisible),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("rotation","",Class<IFunction>::getFunction(c->getSystemState(),_getRotation),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("rotation","",Class<IFunction>::getFunction(c->getSystemState(),_setRotation),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("name","",Class<IFunction>::getFunction(c->getSystemState(),_getName),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("name","",Class<IFunction>::getFunction(c->getSystemState(),_setName),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("parent","",Class<IFunction>::getFunction(c->getSystemState(),_getParent),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("root","",Class<IFunction>::getFunction(c->getSystemState(),_getRoot),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("blendMode","",Class<IFunction>::getFunction(c->getSystemState(),_getBlendMode),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("blendMode","",Class<IFunction>::getFunction(c->getSystemState(),_setBlendMode),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("scale9Grid","",Class<IFunction>::getFunction(c->getSystemState(),_getScale9Grid),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("scale9Grid","",Class<IFunction>::getFunction(c->getSystemState(),undefinedFunction),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("stage","",Class<IFunction>::getFunction(c->getSystemState(),_getStage),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("mask","",Class<IFunction>::getFunction(c->getSystemState(),_getMask),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("mask","",Class<IFunction>::getFunction(c->getSystemState(),_setMask),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("alpha","",Class<IFunction>::getFunction(c->getSystemState(),_getAlpha),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("alpha","",Class<IFunction>::getFunction(c->getSystemState(),_setAlpha),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("opaqueBackground","",Class<IFunction>::getFunction(c->getSystemState(),undefinedFunction),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("opaqueBackground","",Class<IFunction>::getFunction(c->getSystemState(),undefinedFunction),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("getBounds","",Class<IFunction>::getFunction(c->getSystemState(),_getBounds),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getRect","",Class<IFunction>::getFunction(c->getSystemState(),_getBounds),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("mouseX","",Class<IFunction>::getFunction(c->getSystemState(),_getMouseX),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("mouseY","",Class<IFunction>::getFunction(c->getSystemState(),_getMouseY),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("localToGlobal","",Class<IFunction>::getFunction(c->getSystemState(),localToGlobal),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("globalToLocal","",Class<IFunction>::getFunction(c->getSystemState(),globalToLocal),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("hitTestObject","",Class<IFunction>::getFunction(c->getSystemState(),hitTestObject),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("hitTestPoint","",Class<IFunction>::getFunction(c->getSystemState(),hitTestPoint),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("transform","",Class<IFunction>::getFunction(c->getSystemState(),_getTransform),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("transform","",Class<IFunction>::getFunction(c->getSystemState(),_setTransform),SETTER_METHOD,true);
	REGISTER_GETTER_SETTER(c,accessibilityProperties);
	REGISTER_GETTER_SETTER(c,cacheAsBitmap);
	REGISTER_GETTER_SETTER(c,filters);
	REGISTER_GETTER_SETTER(c,scrollRect);
	REGISTER_GETTER_SETTER(c, rotationX);
	REGISTER_GETTER_SETTER(c, rotationY);

	c->addImplementedInterface(InterfaceClass<IBitmapDrawable>::getClass(c->getSystemState()));
	IBitmapDrawable::linkTraits(c);
}

ASFUNCTIONBODY_GETTER_SETTER(DisplayObject,accessibilityProperties);
//TODO: Use a callback for the cacheAsBitmap getter, since it should use computeCacheAsBitmap
ASFUNCTIONBODY_GETTER_SETTER(DisplayObject,cacheAsBitmap);
ASFUNCTIONBODY_GETTER_SETTER(DisplayObject,filters);
ASFUNCTIONBODY_GETTER_SETTER(DisplayObject,scrollRect);
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(DisplayObject, rotationX);
ASFUNCTIONBODY_GETTER_SETTER_NOT_IMPLEMENTED(DisplayObject, rotationY);

bool DisplayObject::computeCacheAsBitmap() const
{
	return cacheAsBitmap || (!filters.isNull() && filters->size()!=0);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getTransform)
{
	DisplayObject* th=obj->as<DisplayObject>();
	
	return asAtom::fromObject(Class<Transform>::getInstanceS(sys,_IMR(th)));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setTransform)
{
	DisplayObject* th=obj->as<DisplayObject>();
	_NR<Transform> trans;
	ARG_UNPACK_ATOM(trans);
	if (!trans.isNull())
	{
		th->setMatrix(trans->owner->matrix);
		th->colorTransform = trans->owner->colorTransform;
	}
	return _MAR(asAtom::invalidAtom);
}

void DisplayObject::buildTraits(ASObject* o)
{
}

void DisplayObject::setMatrix(_NR<Matrix> m)
{
	bool mustInvalidate=false;
	if (m.isNull())
	{
		if (!matrix.isNull())
			mustInvalidate=true;
		matrix= NullRef;
	}
	else
	{
		SpinlockLocker locker(spinlock);
		if (matrix.isNull())
			matrix= _MR(Class<Matrix>::getInstanceS(this->getSystemState()));
		if(matrix->matrix!=m->matrix)
		{
			matrix->matrix=m->matrix;
			extractValuesFromMatrix();
			mustInvalidate=true;
		}
	}
	if(mustInvalidate && onStage)
	{
		hasChanged=true;
		requestInvalidation(getSystemState());
	}
}

void DisplayObject::setLegacyMatrix(const lightspark::MATRIX& m)
{
	if(!useLegacyMatrix)
		return;
	bool mustInvalidate=false;
	{
		SpinlockLocker locker(spinlock);
		if (matrix.isNull())
			matrix= _MR(Class<Matrix>::getInstanceS(this->getSystemState()));
		if(matrix->matrix!=m)
		{
			matrix->matrix=m;
			extractValuesFromMatrix();
			mustInvalidate=true;
		}
	}
	if(mustInvalidate && onStage)
	{
		hasChanged=true;
		requestInvalidation(getSystemState());
	}
}

void DisplayObject::becomeMaskOf(_NR<DisplayObject> m)
{
	maskOf=m;
}

void DisplayObject::setMask(_NR<DisplayObject> m)
{
	bool mustInvalidate=(mask!=m) && onStage;

	if(!mask.isNull())
	{
		//Remove previous mask
		mask->becomeMaskOf(NullRef);
	}

	mask=m;
	if(!mask.isNull())
	{
		//Use new mask
		mask->becomeMaskOf(_IMR(this));
	}

	if(mustInvalidate && onStage)
	{
		hasChanged=true;
		requestInvalidation(getSystemState());
	}
}

MATRIX DisplayObject::getConcatenatedMatrix() const
{
	if(parent.isNull())
		return getMatrix();
	else
		return parent->getConcatenatedMatrix().multiplyMatrix(getMatrix());
}

/* Return alpha value between 0 and 1. (The stored alpha value is not
 * necessary bounded.) */
float DisplayObject::clippedAlpha() const
{
	float a = alpha;
	if (!this->colorTransform.isNull())
	{
		if (alpha != 1.0 && this->colorTransform->alphaMultiplier != 1.0)
			a = alpha * this->colorTransform->alphaMultiplier /256.;
		if (this->colorTransform->alphaOffset != 0)
			a = alpha + this->colorTransform->alphaOffset /256.;
	}
	return dmin(dmax(a, 0.), 1.);
}

float DisplayObject::getConcatenatedAlpha() const
{
	if(parent.isNull())
		return clippedAlpha();
	else
		return parent->getConcatenatedAlpha()*clippedAlpha();
}

MATRIX DisplayObject::getMatrix() const
{
	SpinlockLocker locker(spinlock);
	//Start from the residual matrix and construct the whole one
	MATRIX ret;
	if (!matrix.isNull())
		ret=matrix->matrix;
	ret.scale(sx,sy);
	ret.rotate(rotation*M_PI/180);
	ret.translate(tx,ty);
	return ret;
}

void DisplayObject::extractValuesFromMatrix()
{
	//Extract the base components from the matrix and leave in
	//it only the residual components
	assert(!matrix.isNull());
	tx=matrix->matrix.getTranslateX();
	ty=matrix->matrix.getTranslateY();
	sx=matrix->matrix.getScaleX();
	sy=matrix->matrix.getScaleY();
	rotation=matrix->matrix.getRotation();
	//Deapply translation
	matrix->matrix.translate(-tx,-ty);
	//Deapply rotation
	matrix->matrix.rotate(-rotation*M_PI/180);
	//Deapply scaling
	matrix->matrix.scale(1.0/sx,1.0/sy);
}

bool DisplayObject::skipRender() const
{
	return visible==false || clippedAlpha()==0.0 || ClipDepth;
}

void DisplayObject::defaultRender(RenderContext& ctxt) const
{
	// TODO: use scrollRect

	const CachedSurface& surface=ctxt.getCachedSurface(this);
	/* surface is only modified from within the render thread
	 * so we need no locking here */
	if(!surface.tex.isValid())
		return;

	ctxt.lsglLoadIdentity();
	ctxt.renderTextured(surface.tex, surface.xOffset, surface.yOffset,
			surface.tex.width, surface.tex.height,
			surface.alpha, RenderContext::RGB_MODE);
}

void DisplayObject::computeBoundsForTransformedRect(number_t xmin, number_t xmax, number_t ymin, number_t ymax,
		int32_t& outXMin, int32_t& outYMin, uint32_t& outWidth, uint32_t& outHeight,
		const MATRIX& m) const
{
	//As the transformation is arbitrary we have to check all the four vertices
	number_t coords[8];
	m.multiply2D(xmin,ymin,coords[0],coords[1]);
	m.multiply2D(xmin,ymax,coords[2],coords[3]);
	m.multiply2D(xmax,ymax,coords[4],coords[5]);
	m.multiply2D(xmax,ymin,coords[6],coords[7]);
	//Now find out the minimum and maximum that represent the complete bounding rect
	number_t minx=coords[6];
	number_t maxx=coords[6];
	number_t miny=coords[7];
	number_t maxy=coords[7];
	for(int i=0;i<6;i+=2)
	{
		if(coords[i]<minx)
			minx=coords[i];
		else if(coords[i]>maxx)
			maxx=coords[i];
		if(coords[i+1]<miny)
			miny=coords[i+1];
		else if(coords[i+1]>maxy)
			maxy=coords[i+1];
	}
	outXMin=minx;
	outYMin=miny;
	outWidth=ceil(maxx-minx);
	outHeight=ceil(maxy-miny);
}

IDrawable* DisplayObject::invalidate(DisplayObject* target, const MATRIX& initialMatrix)
{
	//Not supposed to be called
	throw RunTimeException("DisplayObject::invalidate");
}

void DisplayObject::requestInvalidation(InvalidateQueue* q)
{
	//Let's invalidate also the mask
	if(!mask.isNull())
		mask->requestInvalidation(q);
}
//TODO: Fix precision issues, Adobe seems to do the matrix mult with twips and rounds the results, 
//this way they have less pb with precision.
void DisplayObject::localToGlobal(number_t xin, number_t yin, number_t& xout, number_t& yout) const
{
	getMatrix().multiply2D(xin, yin, xout, yout);
	if(!parent.isNull())
		parent->localToGlobal(xout, yout, xout, yout);
}
//TODO: Fix precision issues
void DisplayObject::globalToLocal(number_t xin, number_t yin, number_t& xout, number_t& yout) const
{
	getConcatenatedMatrix().getInverted().multiply2D(xin, yin, xout, yout);
}

void DisplayObject::setOnStage(bool staged)
{
	//TODO: When removing from stage released the cachedTex
	if(staged!=onStage)
	{
		//Our stage condition changed, send event
		onStage=staged;
		if(staged==true)
		{
			hasChanged=true;
			requestInvalidation(getSystemState());
		}
		if(getVm(getSystemState())==NULL)
			return;
		/*NOTE: By tests we can assert that added/addedToStage is dispatched
		  immediately when addChild is called. On the other hand setOnStage may
		  be also called outside of the VM thread (for example by Loader::execute)
		  so we have to check isVmThread and act accordingly. If in the future
		  asynchronous uses of setOnStage are removed the code can be simplified
		  by removing the !isVmThread case.
		*/
		if(onStage==true)
		{
			_R<Event> e=_MR(Class<Event>::getInstanceS(getSystemState(),"addedToStage"));
			if(isVmThread())
				ABCVm::publicHandleEvent(_IMR(this),e);
			else
				getVm(getSystemState())->addEvent(_IMR(this),e);
		}
		else if(onStage==false)
		{
			_R<Event> e=_MR(Class<Event>::getInstanceS(getSystemState(),"removedFromStage"));
			if(isVmThread())
				ABCVm::publicHandleEvent(_IMR(this),e);
			else
				getVm(getSystemState())->addEvent(_IMR(this),e);
		}
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setAlpha)
{
	DisplayObject* th=obj->as<DisplayObject>();
	number_t val;
	ARG_UNPACK_ATOM (val);
	
	/* The stored value is not clipped, _getAlpha will return the
	 * stored value even if it is outside the [0, 1] range. */
	if(th->alpha != val)
	{
		th->alpha=val;
		th->hasChanged=true;
		if(th->onStage)
			th->requestInvalidation(sys);
	}
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getAlpha)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->alpha));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getMask)
{
	DisplayObject* th=obj->as<DisplayObject>();
	if(th->mask.isNull())
		return _MAR(asAtom::nullAtom);

	return asAtom::fromObject(th->mask.getPtr());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setMask)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	if(args[0]->is<DisplayObject>())
	{
		//We received a valid mask object
		DisplayObject* newMask=args[0]->as<DisplayObject>();
		th->setMask(_IMR(newMask));
	}
	else
		th->setMask(NullRef);

	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getScaleX)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->sx));
}

void DisplayObject::setScaleX(number_t val)
{
	//Apply the difference
	if(sx!=val)
	{
		sx=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setScaleX)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	//Stop using the legacy matrix
	if(th->useLegacyMatrix)
		th->useLegacyMatrix=false;
	th->setScaleX(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getScaleY)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->sy));
}

void DisplayObject::setScaleY(number_t val)
{
	//Apply the difference
	if(sy!=val)
	{
		sy=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setScaleY)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	//Stop using the legacy matrix
	if(th->useLegacyMatrix)
		th->useLegacyMatrix=false;
	th->setScaleY(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getScaleZ)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->sz));
}

void DisplayObject::setScaleZ(number_t val)
{
	//Apply the difference
	if(sz!=val)
	{
		sz=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setScaleZ)
{
	LOG(LOG_NOT_IMPLEMENTED,"DisplayObject.scaleZ is set, but not used anywhere");
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	//Stop using the legacy matrix
	if(th->useLegacyMatrix)
		th->useLegacyMatrix=false;
	th->setScaleZ(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getX)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->tx));
}

void DisplayObject::setX(number_t val)
{
	//Stop using the legacy matrix
	if(useLegacyMatrix)
		useLegacyMatrix=false;
	//Apply translation, it's trivial
	if(tx!=val)
	{
		tx=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

void DisplayObject::setY(number_t val)
{
	//Stop using the legacy matrix
	if(useLegacyMatrix)
		useLegacyMatrix=false;
	//Apply translation, it's trivial
	if(ty!=val)
	{
		ty=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

void DisplayObject::setZ(number_t val)
{
	LOG(LOG_NOT_IMPLEMENTED,"setting DisplayObject.z has no effect");
	
	//Stop using the legacy matrix
	if(useLegacyMatrix)
		useLegacyMatrix=false;
	//Apply translation, it's trivial
	if(tz!=val)
	{
		tz=val;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setX)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	th->setX(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getY)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->ty));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setY)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	th->setY(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getZ)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->tz));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setZ)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	th->setZ(val);
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getBounds)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);

	if(args[0]->is<Undefined>() || args[0]->is<Null>())
		return asAtom::fromObject(Class<Rectangle>::getInstanceS(sys));
	if (!args[0]->is<DisplayObject>())
		LOG(LOG_ERROR,"DisplayObject.getBounds invalid type:"<<args[0]->toDebugString());
	assert_and_throw(args[0]->is<DisplayObject>());
	DisplayObject* target=args[0]->as<DisplayObject>();
	//Compute the transformation matrix
	MATRIX m;
	DisplayObject* cur=th;
	while(cur!=NULL && cur!=target)
	{
		m = cur->getMatrix().multiplyMatrix(m);
		cur=cur->parent.getPtr();
	}
	if(cur==NULL)
	{
		//We crawled all the parent chain without finding the target
		//The target is unrelated, compute it's transformation matrix
		const MATRIX& targetMatrix=target->getConcatenatedMatrix();
		//If it's not invertible just use the previous computed one
		if(targetMatrix.isInvertible())
			m = targetMatrix.getInverted().multiplyMatrix(m);
	}

	Rectangle* ret=Class<Rectangle>::getInstanceS(sys);
	number_t x1,x2,y1,y2;
	bool r=th->getBounds(x1,x2,y1,y2, m);
	if(r)
	{
		//Bounds are in the form [XY]{min,max}
		//convert it to rect (x,y,width,height) representation
		ret->x=x1;
		ret->width=x2-x1;
		ret->y=y1;
		ret->height=y2-y1;
	}
	else
	{
		ret->x=0;
		ret->width=0;
		ret->y=0;
		ret->height=0;
	}
	return asAtom::fromObject(ret);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getLoaderInfo)
{
	DisplayObject* th=obj->as<DisplayObject>();

	/* According to tests returning root.loaderInfo is the correct
	 * behaviour, even though the documentation states that only
	 * the main class should have non-null loaderInfo. */
	_NR<RootMovieClip> r=th->getRoot();
	if (r.isNull())
	{
		// if this DisplayObject is not yet added to the stage we just use the mainclip
		r = sys->mainClip;
	}
	if(r.isNull() || r->loaderInfo.isNull())
		return _MAR(asAtom::undefinedAtom);
	
	return asAtom::fromObject(r->loaderInfo.getPtr());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getStage)
{
	DisplayObject* th=obj->as<DisplayObject>();
	_NR<Stage> ret=th->getStage();
	if(ret.isNull())
		return _MAR(asAtom::nullAtom);

	return asAtom::fromObject(ret.getPtr());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getScale9Grid)
{
	//DisplayObject* th=obj->as<DisplayObject>();
	LOG(LOG_NOT_IMPLEMENTED,"DispalyObject.Scale9Grid");
	return _MAR(asAtom::undefinedAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getBlendMode)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return asAtom::fromString(sys,th->blendMode);
}
ASFUNCTIONBODY_ATOM(DisplayObject,_setBlendMode)
{
	DisplayObject* th=obj->as<DisplayObject>();
	tiny_string val;
	ARG_UNPACK_ATOM(val);

	if (
			val != "add" &&
			val != "alpha" &&
			val != "darken" &&
			val != "difference" &&
			val != "erase" &&
			val != "hardlight" &&
			val != "invert" &&
			val != "invert" &&
			val != "layer" &&
			val != "lighten" &&
			val != "multiply" &&
			val != "normal" &&
			val != "overlay" &&
			val != "screen" &&
			val != "subtract"
			)
			val = "normal";
	LOG(LOG_NOT_IMPLEMENTED, "blendmode is set but is not respected during drawing:"<<val);
	th->blendMode = val;
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,localToGlobal)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen == 1);

	Point* pt=args[0]->as<Point>();

	number_t tempx, tempy;

	th->localToGlobal(pt->getX(), pt->getY(), tempx, tempy);

	return asAtom::fromObject(Class<Point>::getInstanceS(sys,tempx, tempy));
}

ASFUNCTIONBODY_ATOM(DisplayObject,globalToLocal)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen == 1);

	Point* pt=args[0]->as<Point>();

	number_t tempx, tempy;

	th->globalToLocal(pt->getX(), pt->getY(), tempx, tempy);

	return asAtom::fromObject(Class<Point>::getInstanceS(sys,tempx, tempy));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setRotation)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	number_t val=args[0]->toNumber();
	//Stop using the legacy matrix
	if(th->useLegacyMatrix)
		th->useLegacyMatrix=false;
	//Apply the difference
	if(th->rotation!=val)
	{
		th->rotation=val;
		th->hasChanged=true;
		if(th->onStage)
			th->requestInvalidation(sys);
	}
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setName)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	th->name=args[0]->toString();
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getName)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return asAtom::fromObject(abstract_s(sys,th->name));
}

void DisplayObject::setParent(_NR<DisplayObjectContainer> p)
{
	if(parent!=p)
	{
		parent=p;
		hasChanged=true;
		if(onStage)
			requestInvalidation(getSystemState());
	}
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getParent)
{
	DisplayObject* th=obj->as<DisplayObject>();
	if(th->parent.isNull())
		return _MAR(asAtom::undefinedAtom);

	return asAtom::fromObject(th->parent.getPtr());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getRoot)
{
	DisplayObject* th=obj->as<DisplayObject>();
	_NR<DisplayObject> ret;
	
	if (th->isLoadedRootObject())
		ret = _NR<DisplayObject>(th);
	else if (th->is<Stage>())
		// according to spec, the root of the stage is the stage itself
		ret = _NR<DisplayObject>(th);
	else
		ret =th->getRoot();
	if(ret.isNull())
		return _MAR(asAtom::undefinedAtom);

	return asAtom::fromObject(ret.getPtr());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getRotation)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->rotation));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setVisible)
{
	DisplayObject* th=obj->as<DisplayObject>();
	assert_and_throw(argslen==1);
	th->visible=args[0]->Boolean_concrete();
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getVisible)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->visible));
}

number_t DisplayObject::computeHeight()
{
	number_t x1,x2,y1,y2;
	bool ret=getBounds(x1,x2,y1,y2,getMatrix());

	return (ret)?(y2-y1):0;
}

number_t DisplayObject::computeWidth()
{
	number_t x1,x2,y1,y2;
	bool ret=getBounds(x1,x2,y1,y2,getMatrix());

	return (ret)?(x2-x1):0;
}

_NR<RootMovieClip> DisplayObject::getRoot()
{
	if(parent.isNull())
		return NullRef;

	return parent->getRoot();
}

_NR<Stage> DisplayObject::getStage()
{
	if(parent.isNull())
		return NullRef;

	return parent->getStage();
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getWidth)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->computeWidth()));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setWidth)
{
	DisplayObject* th=obj->as<DisplayObject>();
	number_t newwidth=args[0]->toNumber();

	number_t xmin,xmax,y1,y2;
	if(!th->boundsRect(xmin,xmax,y1,y2))
		return _MAR(asAtom::invalidAtom);

	number_t width=xmax-xmin;
	if(width==0) //Cannot scale, nothing to do (See Reference)
		return _MAR(asAtom::invalidAtom);
	
	if(width*th->sx!=newwidth) //If the width is changing, calculate new scale
	{
		if(th->useLegacyMatrix)
			th->useLegacyMatrix=false;
		th->setScaleX(newwidth/width);
	}
	return _MAR(asAtom::invalidAtom);
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getHeight)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->computeHeight()));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_setHeight)
{
	DisplayObject* th=obj->as<DisplayObject>();
	number_t newheight=args[0]->toNumber();

	number_t x1,x2,ymin,ymax;
	if(!th->boundsRect(x1,x2,ymin,ymax))
		return _MAR(asAtom::invalidAtom);

	number_t height=ymax-ymin;
	if(height==0) //Cannot scale, nothing to do (See Reference)
		return _MAR(asAtom::invalidAtom);

	if(height*th->sy!=newheight) //If the height is changing, calculate new scale
	{
		if(th->useLegacyMatrix)
			th->useLegacyMatrix=false;
		th->setScaleY(newheight/height);
	}
	return _MAR(asAtom::invalidAtom);
}

Vector2f DisplayObject::getLocalMousePos()
{
	return getConcatenatedMatrix().getInverted().multiply2D(getSystemState()->getInputThread()->getMousePos());
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getMouseX)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->getLocalMousePos().x));
}

ASFUNCTIONBODY_ATOM(DisplayObject,_getMouseY)
{
	DisplayObject* th=obj->as<DisplayObject>();
	return _MAR(asAtom(th->getLocalMousePos().y));
}

_NR<DisplayObject> DisplayObject::hitTest(_NR<DisplayObject> last, number_t x, number_t y, HIT_TYPE type)
{
	if(!(visible || type == GENERIC_HIT_INVISIBLE) || !isConstructed())
		return NullRef;

	//First check if there is any mask on this object, if so the point must be inside the mask to go on
	if(!mask.isNull())
	{
		//First compute the global coordinates from the local ones
		//TODO: we may also pass the global coordinates to all the calls
		const MATRIX& thisMatrix = this->getConcatenatedMatrix();
		number_t globalX, globalY;
		thisMatrix.multiply2D(x,y,globalX,globalY);
		//Now compute the coordinates local to the mask
		const MATRIX& maskMatrix = mask->getConcatenatedMatrix();
		if(!maskMatrix.isInvertible())
		{
			//If the matrix is not invertible the mask as collapsed to zero size
			//If the mask is zero sized then the object is not visible
			return NullRef;
		}
		number_t maskX, maskY;
		maskMatrix.getInverted().multiply2D(globalX,globalY,maskX,maskY);
		if(mask->hitTest(last, maskX, maskY, type)==false)
			return NullRef;
	}

	return hitTestImpl(last, x,y, type);
}

/* Display objects have no children in general,
 * so we skip to calling the constructor, if necessary.
 * This is called in vm's thread context */
void DisplayObject::initFrame()
{
	if(!isConstructed() && getClass())
	{
		asAtomR o = asAtom::fromObject(this);
		std::vector<asAtomR> emtpy;
		getClass()->handleConstruction(o,emtpy,0,true);

		/*
		 * Legacy objects have their display list properties set on creation, but
		 * the related events must only be sent after the constructor is sent.
		 * This is from "Order of Operations".
		 */
		if(!parent.isNull())
		{
			_R<Event> e=_MR(Class<Event>::getInstanceS(getSystemState(),"added"));
			ABCVm::publicHandleEvent(_IMR(this),e);
		}
		if(onStage)
		{
			_R<Event> e=_MR(Class<Event>::getInstanceS(getSystemState(),"addedToStage"));
			ABCVm::publicHandleEvent(_IMR(this),e);
		}
	}
}

void DisplayObject::constructionComplete()
{
	RELEASE_WRITE(constructed,true);
	if(!loaderInfo.isNull())
	{
		loaderInfo->objectHasLoaded(_IMR(this));
	}
	hasChanged=true;
	if(onStage)
		requestInvalidation(getSystemState());
}

void DisplayObject::gatherMaskIDrawables(std::vector<IDrawable::MaskData>& masks) const
{
	if(mask.isNull())
		return;

	//If the mask is hard we need the drawable for each child
	//If the mask is soft we need the rendered final result
	IDrawable::MASK_MODE maskMode = IDrawable::HARD_MASK;
	//For soft masking to work, both the object and the mask must be
	//cacheAsBitmap and the mask must be on the stage
	if(this->computeCacheAsBitmap() && mask->computeCacheAsBitmap() && mask->isOnStage())
		maskMode = IDrawable::SOFT_MASK;

	if(maskMode == IDrawable::HARD_MASK)
	{
		SoftwareInvalidateQueue queue;
		mask->requestInvalidation(&queue);
		for(auto it=queue.queue.begin();it!=queue.queue.end();it++)
		{
			DisplayObject* target=(*it).getPtr();
			//Get the drawable from each of the added objects
			IDrawable* drawable=target->invalidate(NULL, MATRIX());
			if(drawable==NULL)
				continue;
			masks.emplace_back(drawable, maskMode);
		}
	}
	else
	{
		IDrawable* drawable=NULL;
		if(mask->is<DisplayObjectContainer>())
		{
			//HACK: use bitmap temporarily
			number_t xmin,xmax,ymin,ymax;
			bool ret=mask->getBounds(xmin,xmax,ymin,ymax,mask->getConcatenatedMatrix());
			if(ret==false)
				return;
			_R<BitmapData> data(Class<BitmapData>::getInstanceS(getSystemState(),xmax-xmin,ymax-ymin));
			//Forge a matrix. It must contain the right rotation and scaling while translation
			//only compensate for the xmin/ymin offset
			MATRIX m=mask->getConcatenatedMatrix();
			m.x0 -= xmin;
			m.y0 -= ymin;
			data->drawDisplayObject(mask.getPtr(), m);
			_R<Bitmap> bmp(Class<Bitmap>::getInstanceS(getSystemState(),data));

			//The created bitmap is already correctly scaled and rotated
			//Just apply the needed offset
			MATRIX m2(1,1,0,0,xmin,ymin);
			drawable=bmp->invalidate(NULL, m2);
		}
		else
			drawable=mask->invalidate(NULL, MATRIX());

		if(drawable==NULL)
			return;
		masks.emplace_back(drawable, maskMode);
	}
}

void DisplayObject::computeMasksAndMatrix(DisplayObject* target, std::vector<IDrawable::MaskData>& masks, MATRIX& totalMatrix) const
{
	const DisplayObject* cur=this;
	bool gatherMasks = true;
	while(cur && cur!=target)
	{
		totalMatrix=cur->getMatrix().multiplyMatrix(totalMatrix);
		//Get an IDrawable for all the hierarchy of each mask.
		if(gatherMasks)
		{
			if(cur->maskOf.isNull())
				cur->gatherMaskIDrawables(masks);
			else
			{
				//Stop gathering masks if any level of the hierarchy it's a mask
				masks.clear();
				masks.shrink_to_fit();
				gatherMasks=false;
			}
		}
		cur=cur->getParent().getPtr();
	}
}

// Compute the minimal, axis aligned bounding box in global
// coordinates
bool DisplayObject::boundsRectGlobal(number_t& xmin, number_t& xmax, number_t& ymin, number_t& ymax) const
{
	number_t x1, x2, y1, y2;
	if (!boundsRect(x1, x2, y1, y2))
		return abstract_b(getSystemState(),false);

	localToGlobal(x1, y1, x1, y1);
	localToGlobal(x2, y2, x2, y2);

	// Mapping to global may swap min and max values (for example,
	// rotation by 180 degrees)
	xmin = dmin(x1, x2);
	xmax = dmax(x1, x2);
	ymin = dmin(y1, y2);
	ymax = dmax(y1, y2);

	return true;
}

ASFUNCTIONBODY_ATOM(DisplayObject,hitTestObject)
{
	DisplayObject* th=obj->as<DisplayObject>();
	_NR<DisplayObject> another;
	ARG_UNPACK_ATOM(another);

	number_t xmin, xmax, ymin, ymax;
	if (!th->boundsRectGlobal(xmin, xmax, ymin, ymax))
		return _MAR(asAtom::falseAtom);

	number_t xmin2, xmax2, ymin2, ymax2;
	if (!another->boundsRectGlobal(xmin2, xmax2, ymin2, ymax2))
		return _MAR(asAtom::falseAtom);

	number_t intersect_xmax = dmin(xmax, xmax2);
	number_t intersect_xmin = dmax(xmin, xmin2);
	number_t intersect_ymax = dmin(ymax, ymax2);
	number_t intersect_ymin = dmax(ymin, ymin2);

	return _MAR(asAtom((intersect_xmax > intersect_xmin) &&
			  (intersect_ymax > intersect_ymin)));
}

ASFUNCTIONBODY_ATOM(DisplayObject,hitTestPoint)
{
	DisplayObject* th=obj->as<DisplayObject>();
	number_t x;
	number_t y;
	bool shapeFlag;
	ARG_UNPACK_ATOM (x) (y) (shapeFlag, false);

	number_t xmin, xmax, ymin, ymax;
	if (!th->boundsRectGlobal(xmin, xmax, ymin, ymax))
		return _MAR(asAtom::falseAtom);

	bool insideBoundingBox = (xmin <= x) && (x < xmax) && (ymin <= y) && (y < ymax);

	if (!shapeFlag)
	{
		return _MAR(asAtom(insideBoundingBox));
	}
	else
	{
		if (!insideBoundingBox)
			return _MAR(asAtom::falseAtom);

		number_t localX;
		number_t localY;
		th->globalToLocal(x, y, localX, localY);

		// Hmm, hitTest will also check the mask, is this the
		// right thing to do?
		_NR<DisplayObject> hit = th->hitTest(_IMR(th), localX, localY,
						     HIT_TYPE::GENERIC_HIT_INVISIBLE);

		return _MAR(asAtom(hit.getPtr() == th));
	}
}
