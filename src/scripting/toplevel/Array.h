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

#ifndef SCRIPTING_TOPLEVEL_ARRAY_H
#define SCRIPTING_TOPLEVEL_ARRAY_H 1

#include "asobject.h"
#include <unordered_map>

namespace lightspark
{

struct sorton_field
{
	bool isNumeric;
	bool isCaseInsensitive;
	bool isDescending;
	multiname fieldname;
	sorton_field(const multiname& sortfieldname):isNumeric(false),isCaseInsensitive(false),isDescending(false),fieldname(sortfieldname){}
};

class Array: public ASObject
{
friend class ABCVm;
protected:
	uint64_t currentsize;
	// data is split into a vector for the first ARRAY_SIZE_THRESHOLD indexes, and a map for bigger indexes
	std::vector<asAtom> data_first;
	std::unordered_map<uint32_t,asAtom> data_second;
	
	void outofbounds(unsigned int index) const;
	~Array();
private:
	class sortComparatorDefault
	{
	private:
		bool isNumeric;
		bool isCaseInsensitive;
		bool isDescending;
	public:
		sortComparatorDefault(bool n, bool ci, bool d):isNumeric(n),isCaseInsensitive(ci),isDescending(d){}
		bool operator()(asAtom& d1, asAtom& d2);
	};
	class sortComparatorWrapper
	{
	private:
		asAtom comparator;
	public:
		sortComparatorWrapper(asAtom& c):comparator(c){}
		bool operator()(asAtom& d1, asAtom& d2);
	};
	class sortOnComparator
	{
	private:
		std::vector<sorton_field> fields;
	public:
		sortOnComparator(const std::vector<sorton_field>& sf):fields(sf){}
		bool operator()(asAtom& d1, asAtom& d2);
	};
	void constructorImpl(std::vector<asAtom> &args, const unsigned int argslen);
	tiny_string toString_priv(bool localized=false);
	int capIndex(int i);
public:
	static bool isIntegerWithoutLeadingZeros(const tiny_string& value);
	enum SORTTYPE { CASEINSENSITIVE=1, DESCENDING=2, UNIQUESORT=4, RETURNINDEXEDARRAY=8, NUMERIC=16 };
	Array(Class_base* c);
	bool destruct()
	{
		data_first.clear();
		data_second.clear();
		currentsize=0;
		return ASObject::destruct();
	}
	
	//These utility methods are also used by ByteArray
	static bool isValidMultiname(SystemState* sys,const multiname& name, uint32_t& index);
	static bool isValidQName(const tiny_string& name, const tiny_string& ns, unsigned int& index);

	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);

	ASFUNCTION_ATOM(_constructor);
	ASFUNCTION_ATOM(generator);
	ASFUNCTION_ATOM(_push);
	ASFUNCTION_ATOM(_push_as3);
	ASFUNCTION_ATOM(_concat);
	ASFUNCTION_ATOM(_pop);
	ASFUNCTION_ATOM(join);
	ASFUNCTION_ATOM(shift);
	ASFUNCTION_ATOM(unshift);
	ASFUNCTION_ATOM(splice);
	ASFUNCTION_ATOM(_sort);
	ASFUNCTION_ATOM(sortOn);
	ASFUNCTION_ATOM(filter);
	ASFUNCTION_ATOM(indexOf);
	ASFUNCTION_ATOM(_getLength);
	ASFUNCTION_ATOM(_setLength);
	ASFUNCTION_ATOM(forEach);
	ASFUNCTION_ATOM(_reverse);
	ASFUNCTION_ATOM(lastIndexOf);
	ASFUNCTION_ATOM(_map);
	ASFUNCTION_ATOM(_toString);
	ASFUNCTION_ATOM(_toLocaleString);
	ASFUNCTION_ATOM(slice);
	ASFUNCTION_ATOM(every);
	ASFUNCTION_ATOM(some);
	ASFUNCTION_ATOM(insertAt);
	ASFUNCTION_ATOM(removeAt);

	asAtom at(unsigned int index);
	void set(unsigned int index, asAtom& o, bool checkbounds = true);
	uint64_t size();
	void push(asAtom& o);
	void resize(uint64_t n);
	asAtom getVariableByMultiname(const multiname& name, GET_VARIABLE_OPTION opt);
	int32_t getVariableByMultiname_i(const multiname& name);
	void setVariableByMultiname(const multiname& name, asAtom& o, CONST_ALLOWED_FLAG allowConst);
	bool deleteVariableByMultiname(const multiname& name);
	void setVariableByMultiname_i(const multiname& name, int32_t value);
	bool hasPropertyByMultiname(const multiname& name, bool considerDynamic, bool considerPrototype);
	tiny_string toString();
	uint32_t nextNameIndex(uint32_t cur_index);
	asAtom nextName(uint32_t index);
	asAtom nextValue(uint32_t index);
	//Serialization interface
	void serialize(ByteArray* out, std::map<tiny_string, uint32_t>& stringMap,
				std::map<const ASObject*, uint32_t>& objMap,
				std::map<const Class_base*, uint32_t>& traitsMap);
	virtual tiny_string toJSON(std::vector<ASObject *> &path, asAtom& replacer, const tiny_string &spaces, const tiny_string& filter);
};


}
#endif /* SCRIPTING_TOPLEVEL_ARRAY_H */
