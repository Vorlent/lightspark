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

#include <cmath>
#include "parsing/amf3_generator.h"
#include "scripting/argconv.h"
#include "scripting/toplevel/UInteger.h"
#include "scripting/flash/utils/ByteArray.h"

using namespace std;
using namespace lightspark;

tiny_string UInteger::toString()
{
	return UInteger::toString(val);
}

/* static helper function */
tiny_string UInteger::toString(uint32_t val)
{
	char buf[20];
	snprintf(buf,sizeof(buf),"%u",val);
	return tiny_string(buf,true);
}

TRISTATE UInteger::isLess(ASObject* o)
{
	if(o->getObjectType()==T_UINTEGER)
	{
		uint32_t val1=val;
		uint32_t val2=o->toUInt();
		return (val1<val2)?TTRUE:TFALSE;
	}
	else if(o->getObjectType()==T_INTEGER ||
	   o->getObjectType()==T_BOOLEAN)
	{
		uint32_t val1=val;
		int32_t val2=o->toInt();
		if(val2<0)
			return TFALSE;
		else
			return (val1<(uint32_t)val2)?TTRUE:TFALSE;
	}
	else if(o->getObjectType()==T_NUMBER)
	{
		number_t val2=o->toNumber();
		if(std::isnan(val2)) return TUNDEFINED;
		return (number_t(val) < val2)?TTRUE:TFALSE;
	}
	else if(o->getObjectType()==T_NULL)
	{
		// UInteger is never less than int(null) == 0
		return TFALSE;
	}
	else if(o->getObjectType()==T_UNDEFINED)
	{
		return TUNDEFINED;
	}
	else if(o->getObjectType()==T_STRING)
	{
		double val2=o->toNumber();
		if(std::isnan(val2)) return TUNDEFINED;
		return (val<val2)?TTRUE:TFALSE;
	}
	else
	{
		double val2=o->toPrimitive()->toNumber();
		if(std::isnan(val2)) return TUNDEFINED;
		return (val<val2)?TTRUE:TFALSE;
	}
}

ASFUNCTIONBODY_ATOM(UInteger,_constructor)
{
	UInteger* th=obj->as<UInteger>();
	if(argslen==0)
	{
		//The uint is already initialized to 0
		return asAtom::invalidAtomR;
	}
	th->val=args[0]->toUInt();
	return asAtom::invalidAtomR;
}

ASFUNCTIONBODY_ATOM(UInteger,generator)
{
	if (argslen == 0)
		return _MAR(asAtom((uint32_t)0));
	return _MAR(asAtom(args[0]->toUInt()));
}

ASFUNCTIONBODY_ATOM(UInteger,_valueOf)
{
	if(Class<UInteger>::getClass(sys)->prototype->getObj() == obj->getObject())
		return _MAR(asAtom((uint32_t)0));

	if(!obj->is<UInteger>())
			throw Class<TypeError>::getInstanceS(sys,"");

	return _MAR(asAtom(obj->as<UInteger>()->val));
}

void UInteger::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED | CLASS_FINAL);
	c->isReusable = true;
	asAtom max_value = _MAR(asAtom((uint32_t)0xFFFFFFFF));
	asAtom min_value = _MAR(asAtom((uint32_t)0xFFFFFFFF));
	c->setVariableAtomByQName("MAX_VALUE",nsNameAndKind(), max_value,CONSTANT_TRAIT);
	c->setVariableAtomByQName("MIN_VALUE",nsNameAndKind(), min_value,CONSTANT_TRAIT);
	c->setDeclaredMethodByQName("toString",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toString),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("toFixed",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toFixed,1),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("toExponential",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toExponential,1),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("toPrecision",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toPrecision,1),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("valueOf",AS3,Class<IFunction>::getFunction(c->getSystemState(),_valueOf),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("toExponential","",Class<IFunction>::getFunction(c->getSystemState(),_toExponential, 1),DYNAMIC_TRAIT);
	c->prototype->setVariableByQName("toFixed","",Class<IFunction>::getFunction(c->getSystemState(),_toFixed, 1),DYNAMIC_TRAIT);
	c->prototype->setVariableByQName("toPrecision","",Class<IFunction>::getFunction(c->getSystemState(),_toPrecision, 1),DYNAMIC_TRAIT);
	c->prototype->setVariableByQName("toString","",Class<IFunction>::getFunction(c->getSystemState(),_toString),DYNAMIC_TRAIT);
	c->prototype->setVariableByQName("valueOf","",Class<IFunction>::getFunction(c->getSystemState(),_valueOf),DYNAMIC_TRAIT);
}

ASFUNCTIONBODY_ATOM(UInteger,_toString)
{
	if(Class<UInteger>::getClass(sys)->prototype->getObj() == obj->getObject())
		return asAtom::fromString(sys,"0");

	UInteger* th=obj->as<UInteger>();
	uint32_t radix;
	ARG_UNPACK_ATOM (radix,10);

	if (radix == 10)
	{
		char buf[20];
		snprintf(buf,20,"%u",th->val);
		return asAtom::fromObject(abstract_s(sys,buf));
	}
	else
	{
		tiny_string s=Number::toStringRadix((number_t)th->val, radix);
		return asAtom::fromObject(abstract_s(sys,s));
	}
}

bool UInteger::isEqual(ASObject* o)
{
	switch(o->getObjectType())
	{
		case T_INTEGER:
		case T_UINTEGER:
		case T_NUMBER:
		case T_STRING:
		case T_BOOLEAN:
			return val==o->toUInt();
		case T_NULL:
		case T_UNDEFINED:
			return false;
		default:
			return o->isEqual(this);
	}
}

ASFUNCTIONBODY_ATOM(UInteger,_toExponential)
{
	UInteger *th=obj->as<UInteger>();
	double v = (double)th->val;
	int32_t fractionDigits;
	ARG_UNPACK_ATOM(fractionDigits, 0);
	if (argslen == 0 || args[0]->is<Undefined>())
	{
		if (v == 0)
			fractionDigits = 1;
		else
			fractionDigits = imin(imax((int32_t)ceil(::log10(v)), 1), 20);
	}
	return asAtom::fromObject(abstract_s(sys,Number::toExponentialString(v, fractionDigits)));
}

ASFUNCTIONBODY_ATOM(UInteger,_toFixed)
{
	UInteger *th=obj->as<UInteger>();
	int fractiondigits;
	ARG_UNPACK_ATOM (fractiondigits, 0);
	return asAtom::fromObject(abstract_s(sys,Number::toFixedString(th->val, fractiondigits)));
}

ASFUNCTIONBODY_ATOM(UInteger,_toPrecision)
{
	UInteger *th=obj->as<UInteger>();
	if (argslen == 0 || args[0]->is<Undefined>())
		return asAtom::fromObject(abstract_s(sys,th->toString()));
	int precision;
	ARG_UNPACK_ATOM (precision);
	return asAtom::fromObject(abstract_s(sys,Number::toPrecisionString(th->val, precision)));
}

void UInteger::serialize(ByteArray* out, std::map<tiny_string, uint32_t>& stringMap,
				std::map<const ASObject*, uint32_t>& objMap,
				std::map<const Class_base*, uint32_t>& traitsMap)
{
	if (out->getObjectEncoding() == ObjectEncoding::AMF0)
	{
		out->writeByte(amf0_number_marker);
		out->serializeDouble(val);
		return;
	}
	if(val>=0x40000000)
	{
		// write as double
		out->writeByte(double_marker);
		out->serializeDouble(val);
	}
	else
	{
		out->writeByte(integer_marker);
		out->writeU29((uint32_t)val);
	}
}
