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
#include "parsing/amf3_generator.h"
#include "scripting/toplevel/toplevel.h"
#include "scripting/toplevel/Array.h"
#include "scripting/toplevel/ASString.h"
#include "scripting/toplevel/Date.h"
#include "scripting/toplevel/Vector.h"
#include "scripting/class.h"
#include "scripting/flash/xml/flashxml.h"
#include "toplevel/XML.h"
#include <iostream>
#include <fstream>
#include "scripting/flash/utils/ByteArray.h"
#include "scripting/flash/utils/Dictionary.h"

using namespace std;
using namespace lightspark;

asAtomR Amf3Deserializer::readObject() const
{
	vector<tiny_string> stringMap;
	vector<asAtomR> objMap;
	vector<TraitsRef> traitsMap;
	return parseValue(stringMap, objMap, traitsMap);
}

asAtomR Amf3Deserializer::parseInteger() const
{
	uint32_t tmp;
	if(!input->readU29(tmp))
		throw ParseException("Not enough data to parse integer");
	return _MAR(asAtom(tmp));
}

asAtomR Amf3Deserializer::parseDouble() const
{
	union
	{
		uint64_t dummy;
		double val;
	} tmp;
	uint8_t* tmpPtr=reinterpret_cast<uint8_t*>(&tmp.dummy);

	for(uint32_t i=0;i<8;i++)
	{
		if(!input->readByte(tmpPtr[i]))
			throw ParseException("Not enough data to parse double");
	}
	tmp.dummy=GINT64_FROM_BE(tmp.dummy);
	return _MAR(asAtom(tmp.val));
}

asAtomR Amf3Deserializer::parseDate() const
{
	union
	{
		uint64_t dummy;
		double val;
	} tmp;
	uint8_t* tmpPtr=reinterpret_cast<uint8_t*>(&tmp.dummy);

	for(uint32_t i=0;i<8;i++)
	{
		if(!input->readByte(tmpPtr[i]))
			throw ParseException("Not enough data to parse date");
	}
	tmp.dummy=GINT64_FROM_BE(tmp.dummy);
	Date* dt = Class<Date>::getInstanceSRaw(input->getSystemState());
	dt->MakeDateFromMilliseconds((int64_t)tmp.val);
	return asAtom::fromObject(dt);
}

tiny_string Amf3Deserializer::parseStringVR(std::vector<tiny_string>& stringMap) const
{
	uint32_t strRef;
	if(!input->readU29(strRef))
		throw ParseException("Not enough data to parse string");

	if((strRef&0x01)==0)
	{
		//Just a reference
		if(stringMap.size() <= (strRef >> 1))
			throw ParseException("Invalid string reference in AMF3 data");
		return stringMap[strRef >> 1];
	}

	uint32_t strLen=strRef>>1;
	string retStr;
	for(uint32_t i=0;i<strLen;i++)
	{
		uint8_t c;
		if(!input->readByte(c))
			throw ParseException("Not enough data to parse string");
		retStr.push_back(c);
	}
	//Add string to the map, if it's not the empty one
	if(retStr.size())
		stringMap.emplace_back(retStr);
	return retStr;
}

asAtomR Amf3Deserializer::parseArray(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t arrayRef;
	if(!input->readU29(arrayRef))
		throw ParseException("Not enough data to parse AMF3 array");

	if((arrayRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (arrayRef >> 1))
			throw ParseException("Invalid object reference in AMF3 data");
		asAtomR ret=objMap[arrayRef >> 1];
		return ret;
	}

	Array* ret=Class<lightspark::Array>::getInstanceSRaw(input->getSystemState());
	//Add object to the map
	objMap.push_back(asAtom::fromObject(ret));

	int32_t denseCount = arrayRef >> 1;

	//Read name, value pairs
	while(1)
	{
		const tiny_string& varName=parseStringVR(stringMap);
		if(varName=="")
			break;
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->setVariableAtomByQName(varName,nsNameAndKind(BUILTIN_NAMESPACES::EMPTY_NS),value, DYNAMIC_TRAIT);
	}

	//Read the dense portion
	for(int32_t i=0;i<denseCount;i++)
	{
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->push(value);
	}
	return asAtom::fromObject(ret);
}

asAtomR Amf3Deserializer::parseVector(uint8_t marker, std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t vectorRef;
	if(!input->readU29(vectorRef))
		throw ParseException("Not enough data to parse AMF3 vector");
	
	if((vectorRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (vectorRef >> 1))
			throw ParseException("Invalid object reference in AMF3 data");
		asAtomR ret=objMap[vectorRef >> 1];
		return ret;
	}

	uint8_t b;
	if (!input->readByte(b))
		throw ParseException("Not enough data to parse AMF3 vector");
	const Type* type =NULL;
	switch (marker)
	{
		case vector_int_marker:
			type = Class<Integer>::getClass(input->getSystemState());
			break;
		case vector_uint_marker:
			type = Class<UInteger>::getClass(input->getSystemState());
			break;
		case vector_double_marker:
			type = Class<Number>::getClass(input->getSystemState());
			break;
		case vector_object_marker:
		{
			tiny_string vectypename;
			vectypename = parseStringVR(stringMap);
			multiname m(NULL);
			m.name_type=multiname::NAME_STRING;
			m.name_s_id=input->getSystemState()->getUniqueStringId(vectypename);
			m.ns.push_back(nsNameAndKind(input->getSystemState(),"",NAMESPACE));
			m.isAttribute = false;
			type = Type::getTypeFromMultiname(&m,getVm(input->getSystemState())->currentCallContext->context);
			if (type == NULL)
			{
				LOG(LOG_ERROR,"unknown vector type during deserialization:"<<m);
				type = input->getSystemState()->getObjectClassRef();
			}
			break;
		}
		default:
			LOG(LOG_ERROR,"invalid marker during deserialization of vector:"<<marker);
			throw ParseException("invalid marker in AMF3 vector");
			
	}
	Vector* ret= Template<Vector>::getInstanceS(input->getSystemState(),type,ABCVm::getCurrentApplicationDomain(getVm(input->getSystemState())->currentCallContext))->as<Vector>();
	//Add object to the map
	objMap.push_back(asAtom::fromObject(ret));

	
	int32_t count = vectorRef >> 1;

	for(int32_t i=0;i<count;i++)
	{
		switch (marker)
		{
			case vector_int_marker:
			{
				uint32_t value = 0;
				if (!input->readUnsignedInt(value))
					throw ParseException("Not enough data to parse AMF3 vector");
				asAtomR v = _MAR(asAtom((int32_t)value));
				ret->append(v);
				break;
			}
			case vector_uint_marker:
			{
				uint32_t value = 0;
				if (!input->readUnsignedInt(value))
					throw ParseException("Not enough data to parse AMF3 vector");
				asAtomR v = _MAR(asAtom(value));
				ret->append(v);
				break;
			}
			case vector_double_marker:
			{
				asAtomR v = parseDouble();
				ret->append(v);
				break;
			}
			case vector_object_marker:
			{
				asAtomR value=parseValue(stringMap, objMap, traitsMap);
				ret->append(value);
				break;
			}
		}
	}
	// set fixed at last to avoid rangeError
	ret->setFixed(b == 0x01);
	return asAtom::fromObject(ret);
}


asAtomR Amf3Deserializer::parseDictionary(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t dictRef;
	if(!input->readU29(dictRef))
		throw ParseException("Not enough data to parse AMF3 dictionary");
	
	if((dictRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (dictRef >> 1))
			throw ParseException("Invalid object reference in AMF3 data");
		asAtomR ret=objMap[dictRef >> 1];
		return ret;
	}

	uint8_t weakkeys;
	if (!input->readByte(weakkeys))
		throw ParseException("Not enough data to parse AMF3 vector");
	if (weakkeys)
		LOG(LOG_NOT_IMPLEMENTED,"handling of weak keys in Dictionary");
	Dictionary* ret=Class<Dictionary>::getInstanceSRaw(input->getSystemState());
	//Add object to the map
	objMap.push_back(asAtom::fromObject(ret));

	
	int32_t count = dictRef >> 1;

	for(int32_t i=0;i<count;i++)
	{
		asAtomR key=parseValue(stringMap, objMap, traitsMap);
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		multiname name(NULL);
		name.name_type=multiname::NAME_OBJECT;
		name.name_o = key->toObject(getSys());
		name.ns.push_back(nsNameAndKind(input->getSystemState(),"",NAMESPACE));
		ret->setVariableByMultiname(name,value,ASObject::CONST_ALLOWED);
	}
	return asAtom::fromObject(ret);
}

asAtomR Amf3Deserializer::parseByteArray(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t bytearrayRef;
	if(!input->readU29(bytearrayRef))
		throw ParseException("Not enough data to parse AMF3 bytearray");
	
	if((bytearrayRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (bytearrayRef >> 1))
			throw ParseException("Invalid object reference in AMF3 data");
		asAtomR ret=objMap[bytearrayRef >> 1];
		return ret;
	}

	ByteArray* ret=Class<ByteArray>::getInstanceSRaw(input->getSystemState());
	//Add object to the map
	objMap.push_back(asAtom::fromObject(ret));

	
	int32_t count = bytearrayRef >> 1;

	for(int32_t i=0;i<count;i++)
	{
		uint8_t b;
		if (!input->readByte(b))
			throw ParseException("Not enough data to parse AMF3 bytearray");
		ret->writeByte(b);
	}
	return asAtom::fromObject(ret);
}

asAtomR Amf3Deserializer::parseObject(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t objRef;
	if(!input->readU29(objRef))
		throw ParseException("Not enough data to parse AMF3 object");
	if((objRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (objRef >> 1))
			throw ParseException("Invalid object reference in AMF3 data");
		asAtomR ret=objMap[objRef >> 1];
		return ret;
	}

	if((objRef&0x07)==0x07)
	{
		//Custom serialization
		const tiny_string& className=parseStringVR(stringMap);
		assert_and_throw(!className.empty());
		const auto it=input->getSystemState()->aliasMap.find(className);
		assert_and_throw(it!=input->getSystemState()->aliasMap.end());

		Class_base* type=it->second.getPtr();
		traitsMap.push_back(TraitsRef(type));

		std::vector<asAtomR> empty;
		asAtomR ret= type->getInstance(true, empty, 0);
		//Invoke readExternal
		multiname readExternalName(NULL);
		readExternalName.name_type=multiname::NAME_STRING;
		readExternalName.name_s_id=input->getSystemState()->getUniqueStringId("readExternal");
		readExternalName.ns.push_back(nsNameAndKind(input->getSystemState(),"",NAMESPACE));
		readExternalName.isAttribute = false;

		asAtomR o=ret->getObject()->getVariableByMultiname(readExternalName,ASObject::SKIP_IMPL);
		assert_and_throw(o->type==T_FUNCTION);
		std::vector<asAtomR> tmpArg;
		tmpArg.reserve(1);
		tmpArg.push_back(asAtom::fromObject(input));
		o->callFunction(ret, tmpArg, 1,false);
		return ret;
	}

	TraitsRef traits(NULL);
	if((objRef&0x02)==0)
	{
		uint32_t traitsRef=objRef>>2;
		if(traitsMap.size() <= traitsRef)
			throw ParseException("Invalid traits reference in AMF3 data");
		traits=traitsMap[traitsRef];
	}
	else
	{
		traits.dynamic = objRef&0x08;
		uint32_t traitsCount=objRef>>4;
		const tiny_string& className=parseStringVR(stringMap);
		//Add the type to the traitsMap
		for(uint32_t i=0;i<traitsCount;i++)
			traits.traitsNames.emplace_back(parseStringVR(stringMap));

		const auto it=input->getSystemState()->aliasMap.find(className);
		if(it!=input->getSystemState()->aliasMap.end())
			traits.type=it->second.getPtr();
		traitsMap.emplace_back(traits);
	}

	std::vector<asAtomR> empty;
	asAtomR ret=(traits.type)?traits.type->getInstance(true, empty, 0):
	asAtom::fromObject(Class<ASObject>::getInstanceS(input->getSystemState()));
	//Add object to the map
	objMap.push_back(ret);

	for(uint32_t i=0;i<traits.traitsNames.size();i++)
	{
		asAtomR value=parseValue(stringMap, objMap, traitsMap);

		multiname name(NULL);
		name.name_type=multiname::NAME_STRING;
		name.name_s_id=input->getSystemState()->getUniqueStringId(traits.traitsNames[i]);
		name.ns.push_back(nsNameAndKind(input->getSystemState(),"",NAMESPACE));
		name.isAttribute=false;
			
		ret->getObject()->setVariableByMultiname(name,value,ASObject::CONST_ALLOWED,traits.type);
	}

	//Read dynamic name, value pairs
	while(traits.dynamic)
	{
		const tiny_string& varName=parseStringVR(stringMap);
		if(varName=="")
			break;
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->getObject()->setVariableAtomByQName(varName,nsNameAndKind(BUILTIN_NAMESPACES::EMPTY_NS),value,DYNAMIC_TRAIT);
	}
	return ret;
}

asAtomR Amf3Deserializer::parseXML(std::vector<asAtomR>& objMap, bool legacyXML) const
{
	uint32_t xmlRef;
	if(!input->readU29(xmlRef))
		throw ParseException("Not enough data to parse XML");

	if((xmlRef&0x01)==0)
	{
		//Just a reference
		if(objMap.size() <= (xmlRef >> 1))
			throw ParseException("Invalid XML reference in AMF3 data");
		asAtomR xmlObj = objMap[xmlRef >> 1];
		return xmlObj;
	}

	uint32_t strLen=xmlRef>>1;
	string xmlStr;
	for(uint32_t i=0;i<strLen;i++)
	{
		uint8_t c;
		if(!input->readByte(c))
			throw ParseException("Not enough data to parse string");
		xmlStr.push_back(c);
	}

	ASObject *xmlObj;
	if(legacyXML)
		xmlObj=Class<XMLDocument>::getInstanceSRaw(input->getSystemState(),xmlStr);
	else
		xmlObj=XML::createFromString(input->getSystemState(),xmlStr);
	objMap.push_back(asAtom::fromObject(xmlObj));
	return asAtom::fromObject(xmlObj);
}


asAtomR Amf3Deserializer::parseValue(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	//Read the first byte as it contains the object marker
	uint8_t marker;
	if(!input->readByte(marker))
		throw ParseException("Not enough data to parse AMF3 object");
	if (input->getCurrentObjectEncoding() == ObjectEncoding::AMF3)
	{
		switch(marker)
		{
			case null_marker:
				return _MAR(asAtom::nullAtom);
			case undefined_marker:
				return _MAR(asAtom::undefinedAtom);
			case false_marker:
				return _MAR(asAtom::falseAtom);
			case true_marker:
				return _MAR(asAtom::trueAtom);
			case integer_marker:
				return parseInteger();
			case double_marker:
				return parseDouble();
			case date_marker:
				return parseDate();
			case string_marker:
				return asAtom::fromObject(abstract_s(input->getSystemState(),parseStringVR(stringMap)));
			case xml_doc_marker:
				return parseXML(objMap, true);
			case array_marker:
				return parseArray(stringMap, objMap, traitsMap);
			case object_marker:
				return parseObject(stringMap, objMap, traitsMap);
			case xml_marker:
				return parseXML(objMap, false);
			case byte_array_marker:
				return parseByteArray(stringMap, objMap, traitsMap);
			case vector_int_marker:
			case vector_uint_marker:
			case vector_double_marker:
			case vector_object_marker:
				return parseVector(marker, stringMap, objMap, traitsMap);
			case dictionary_marker:
				return parseDictionary(stringMap, objMap, traitsMap);
			default:
				LOG(LOG_ERROR,"Unsupported marker " << (uint32_t)marker);
				throw UnsupportedException("Unsupported marker");
		}
	
	}
	else
	{
		switch(marker)
		{
			case amf0_number_marker:
				return parseDouble();
			case amf0_boolean_marker:
				return _MAR(asAtom((bool)input->readByte(marker)));
			case amf0_string_marker:
				return asAtom::fromObject(abstract_s(input->getSystemState(),parseStringAMF0()));
			case amf0_object_marker:
				return parseObjectAMF0(stringMap,objMap,traitsMap);
			case amf0_null_marker:
				return _MAR(asAtom::nullAtom);
			case amf0_undefined_marker:
				return _MAR(asAtom::undefinedAtom);
			case amf0_reference_marker:
				LOG(LOG_ERROR,"unimplemented marker " << (uint32_t)marker);
				throw UnsupportedException("unimplemented marker");
			case amf0_ecma_array_marker:
				return parseECMAArrayAMF0(stringMap,objMap,traitsMap);
			case amf0_strict_array_marker:
				return parseStrictArrayAMF0(stringMap,objMap,traitsMap);
			case amf0_date_marker:
				LOG(LOG_ERROR,"unimplemented marker " << (uint32_t)marker);
				throw UnsupportedException("unimplemented marker");
			case amf0_long_string_marker:
				LOG(LOG_ERROR,"unimplemented marker " << (uint32_t)marker);
				throw UnsupportedException("unimplemented marker");
			case amf0_xml_document_marker:
				return parseXML(objMap, false);
			case amf0_typed_object_marker:
				LOG(LOG_ERROR,"unimplemented marker " << (uint32_t)marker);
				throw UnsupportedException("unimplemented marker");
			case amf0_avmplus_object_marker:
				input->setCurrentObjectEncoding(ObjectEncoding::AMF3);
				return parseValue(stringMap, objMap, traitsMap);
			default:
				LOG(LOG_ERROR,"Unsupported marker " << (uint32_t)marker);
				throw UnsupportedException("Unsupported marker");
		}
	}
}
tiny_string Amf3Deserializer::parseStringAMF0() const
{
	uint16_t strLen;
	if(!input->readShort(strLen))
		throw ParseException("Not enough data to parse integer");
	
	string retStr;
	for(uint32_t i=0;i<strLen;i++)
	{
		uint8_t c;
		if(!input->readByte(c))
			throw ParseException("Not enough data to parse string");
		retStr.push_back(c);
	}
	return retStr;
}
asAtomR Amf3Deserializer::parseECMAArrayAMF0(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t count;
	if(!input->readUnsignedInt(count))
		throw ParseException("Not enough data to parse AMF3 array");

	asAtomR ret=asAtom::fromObject(Class<ASObject>::getInstanceS(input->getSystemState()));

	//Read name, value pairs
	while(true)
	{
		tiny_string varName = parseStringAMF0();
		if (varName == "")
		{
			uint8_t marker = 0;
			input->readByte(marker);
			if (marker == amf0_object_end_marker )
				break;
			throw ParseException("empty key in AMF0 ECMA array");
		}
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->getObject()->setVariableAtomByQName(varName,nsNameAndKind(BUILTIN_NAMESPACES::EMPTY_NS),value,DYNAMIC_TRAIT);
	}
	return ret;
}
asAtomR Amf3Deserializer::parseStrictArrayAMF0(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	uint32_t count;
	if(!input->readUnsignedInt(count))
		throw ParseException("Not enough data to parse AMF3 strict array");

	lightspark::Array* ret=Class<lightspark::Array>::getInstanceSRaw(input->getSystemState());
	//Add object to the map
	objMap.push_back(asAtom::fromObject(ret));

	while(count)
	{
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->push(value);
		count--;
	}
	return asAtom::fromObject(ret);
}

asAtomR Amf3Deserializer::parseObjectAMF0(std::vector<tiny_string>& stringMap,
			std::vector<asAtomR>& objMap,
			std::vector<TraitsRef>& traitsMap) const
{
	asAtomR ret=asAtom::fromObject(Class<ASObject>::getInstanceS(input->getSystemState()));

	while (true)
	{
		tiny_string varName = parseStringAMF0();
		if (varName == "")
		{
			uint8_t marker = 0;
			input->readByte(marker);
			if (marker == amf0_object_end_marker )
				return ret;
			throw ParseException("empty key in AMF0 object");
		}
		asAtomR value=parseValue(stringMap, objMap, traitsMap);
		ret->getObject()->setVariableAtomByQName(varName,nsNameAndKind(BUILTIN_NAMESPACES::EMPTY_NS),value,DYNAMIC_TRAIT);
	}
	return ret;
}

