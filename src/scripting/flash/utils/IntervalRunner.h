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

#ifndef SCRIPTING_FLASH_UTILS_INTERVALRUNNER_H
#define SCRIPTING_FLASH_UTILS_INTERVALRUNNER_H 1

#include "compat.h"
#include "swftypes.h"


namespace lightspark
{

class IntervalRunner : public ITickJob, public EventDispatcher
{
public:
	enum INTERVALTYPE { INTERVAL, TIMEOUT };
private:
	// IntervalRunner will delete itself in tickFence, others
	// should not call the destructor.
	~IntervalRunner();
	INTERVALTYPE type;
	uint32_t id;
	asAtomR callback;
	std::vector<asAtomR> args;
	asAtomR obj;
	const unsigned int argslen;
	uint32_t interval;
public:
	IntervalRunner(INTERVALTYPE _type, uint32_t _id, asAtomR& _callback, std::vector<asAtomR> args,
			const unsigned int _argslen, asAtomR& _obj, const uint32_t _interval);
	void tick();
	void tickFence();
	INTERVALTYPE getType() { return type; }
};


}

#endif /* SCRIPTING_FLASH_UTILS_FLASHUTILS_H */
