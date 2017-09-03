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

#ifndef SMARTREFS_H
#define SMARTREFS_H 1

#include <stdexcept>
#include "compat.h"
#include "logger.h"

namespace lightspark
{

static void breakpoint() {
	rand();
	rand();
	rand();
	rand();
	rand();
}

class RefCountable {
private:
	ATOMIC_INT32(ref_count);
	ACQUIRE_RELEASE_FLAG(isConstant);
protected:
	RefCountable() : ref_count(1),isConstant(false) {}

public:
	virtual ~RefCountable() {}

//#ifndef NDEBUG
	int getRefCount() const { return ref_count; }
//#endif
	inline bool isLastRef() const { return !isConstant && ref_count == 1; }
	inline void setConstant()
	{
		RELEASE_WRITE(isConstant,true);
	}
	inline bool getConstant() const { return isConstant; }

	//This object is not supposed to deleted via decRef()
	inline void incRefStatic()
	{
		if (!isConstant)
			++ref_count;
		if(ref_count > 300) {
			LOG(LOG_INFO, _("incRefStatic refcount: ") << ref_count << _(" ptr: ") << this);
			breakpoint();
		}
	}
	
	inline void incRef()
	{
		if (!isConstant)
			++ref_count;
		LOG(LOG_INFO, _("incRef refcount: ") << ref_count << _(" ptr: ") << this);
		breakpoint();
	}
	inline bool decRef()
	{
		assert(ref_count>0);
		if (!isConstant)
		{
			if (ref_count == 1)
			{
				if (destruct())
				{
					//Let's make refcount very invalid
					ref_count=-1024;
					LOG(LOG_INFO, _("decRef refcount: ") << ref_count << _(" ptr: ") << this);
					breakpoint();
					delete this;
				}
				return true;
			}
			else
				--ref_count;
			LOG(LOG_INFO, _("decRef refcount: ") << ref_count << _(" ptr: ") << this);
			breakpoint();
		}
		return false;
	}

	inline void incRefRef(int id)
	{
		if (!isConstant && ref_count > -999)
			++ref_count;
		if(ref_count > 300) {
			LOG(LOG_INFO, _("incRefRef refcount: ") << ref_count << _(" ptr: ") << this << _(" id: ") << id);
			breakpoint();
		}
	}
	inline bool decRefRef(int id)
	{
		assert(ref_count>0);
		if (!isConstant && ref_count > -888)
		{
			if (ref_count == 1)
			{
				if (destruct())
				{
					//Let's make refcount very invalid
					if(ref_count > 300) {
						LOG(LOG_INFO, _("decRefRef refcount: ") << -1024 << _(" ptr: ") << this << _(" id: ") << id);
						breakpoint();
					}
					ref_count=-1024;
					delete this;
				}
				return true;
			}
			else
				--ref_count;
			if(ref_count > 300) {
				LOG(LOG_INFO, _("decRefRef refcount: ") << ref_count << _(" ptr: ") << this << _(" id: ") << id);
				breakpoint();
			}
		}
		return false;
	}
	virtual bool destruct()
	{
		return true;
	}
};

/*
   NOTE: _Always_ define both copy constructor and assignment operator in non templated way.
   Templated versions must be added if copying should be allowed from compatible types.
   The compiler will _not_ use templated versions if the right hand type is the same 
   as the left hand type */

template<class T> class NullableRef;

template<class T>
class Ref
{
private:
	T* m;
public:
	int id;
	explicit Ref(T* o):m(o), id(-1)
	{
		assert(m);
	}
	Ref(const Ref<T>& r):m(r.m)
	{
		id = rand();
		m->incRefRef(id);
	}
	//Constructible from any compatible reference
	template<class D> Ref(const Ref<D>& r):m(r.getPtr())
	{
		id = rand();
		m->incRefRef(id);
	}
	template<class D> Ref(const NullableRef<D>& r);
	Ref<T>& operator=(const Ref<T>& r)
	{
		int old_id = id;
		id = rand();
		//incRef before decRef to make sure this works even if the pointer is the same
		r.m->incRefRef(id);

		T* old=m;
		m=r.m;

		//decRef as the very last function call, because it
		//may cause this Ref to be deleted (if old owns this Ref)
		old->decRefRef(old_id);

		return *this;
	}
	template<class D> inline Ref<T>& operator=(const Ref<D>& r)
	{
		int old_id = id;
		id = rand();
		//incRef before decRef to make sure this works even if the pointer is the same
		r.m->incRefRef(id);

		T* old=m;
		m=r.m;

		old->decRefRef(old_id);

		return *this;
	}
	template<class D> inline bool operator==(const Ref<D>& r) const
	{
		return m==r.getPtr();
	}
	template<class D> inline bool operator!=(const Ref<D>& r) const
	{
		return m!=r.getPtr();
	}
	template<class D> inline bool operator==(const NullableRef<D>&r) const;
	inline bool operator==(T* r) const
	{
		return m==r;
	}
	//Order operator for Dictionary map
	inline bool operator<(const Ref<T>& r) const
	{
		return m<r.m;
	}
	template<class D> inline Ref<D> cast() const
	{
		D* p = static_cast<D*>(m);
		return _IMR(p);
	}
	~Ref()
	{
		m->decRefRef(id);
	}
	inline T* operator->() const 
	{
		return m;
	}
	inline T* getPtr() const 
	{ 
		return m; 
	}
};

#define _R Ref

template<class T>
inline std::ostream& operator<<(std::ostream& s, const Ref<T>& r)
{
	return s << "Ref: ";
}

template<class T>
Ref<T> _MR(T* a)
{
	return Ref<T>(a);
}

class NullRef_t
{
};

static NullRef_t NullRef;

template<class T>
class NullableRef
{
private:
	T* m;
public:
	int id;
	NullableRef(): m(NULL), id(-1) {}
	explicit NullableRef(T* o):m(o), id(-1){}
	NullableRef(NullRef_t):m(NULL), id(-1){}
	NullableRef(const NullableRef& r):m(r.m)
	{
		id = rand();
		if(m)
			m->incRefRef(id);
	}
	//Constructible from any compatible nullable reference and reference
	template<class D> NullableRef(const NullableRef<D>& r):m(r.getPtr())
	{
		id = rand();
		if(m)
			m->incRefRef(id);
	}
	template<class D> NullableRef(const Ref<D>& r):m(r.getPtr())
	{
		id = rand();
		//The right hand Ref object is guaranteed to be valid
		m->incRefRef(id);
	}
	inline NullableRef<T>& operator=(const NullableRef<T>& r)
	{
		int old_id = id;
		id = rand();
		if(r.m)
			r.m->incRefRef(id);

		T* old=m;
		m=r.m;
		if(old)
			old->decRefRef(old_id);
		return *this;
	}
	template<class D> inline NullableRef<T>& operator=(const NullableRef<D>& r)
	{
		int old_id = id;
		id = rand();
		if(r.getPtr())
			r->incRefRef(id);

		T* old=m;
		m=r.getPtr();
		if(old)
			old->decRefRef(old_id);
		return *this;
	}
	template<class D> inline NullableRef<T>& operator=(const Ref<D>& r)
	{
		int old_id = id;
		id = rand();
		r.getPtr()->incRefRef(id);

		T* old=m;
		m=r.getPtr();
		if(old)
			old->decRefRef(old_id);
		return *this;
	}
	template<class D> inline bool operator==(const NullableRef<D>& r) const
	{
		return m==r.getPtr();
	}
	template<class D> inline bool operator==(const Ref<D>& r) const
	{
		return m==r.getPtr();
	}
	template<class D>
	inline bool operator==(const D* r) const
	{
		return m==r;
	}
	inline bool operator==(NullRef_t) const
	{
		return m==NULL;
	}
	template<class D> inline bool operator!=(const NullableRef<D>& r) const
	{
		return m!=r.getPtr();
	}
	template<class D> inline bool operator!=(const Ref<D>& r) const
	{
		return m!=r.getPtr();
	}
	template<class D>
	inline bool operator!=(const D* r) const
	{
		return m!=r;
	}
	inline bool operator!=(NullRef_t) const
	{
		return m!=NULL;
	}
	/*explicit*/ operator bool() const
	{
		return m != NULL;
	}
	~NullableRef()
	{
		if(m)
			m->decRefRef(id);
	}
	inline T* operator->() const
	{
		if(m != NULL)
			return m;
		else
			throw std::runtime_error("LS smart pointer: NULL pointer access");
	}
	inline T* getPtr() const { return m; }
	inline bool isNull() const { return m==NULL; }
	inline void reset()
	{
		T* old=m;
		m=NULL;
		if(old)
			old->decRefRef(id);
	}
	inline void fakeRelease()
	{
		m=NULL;
	}
	inline void forceDestruct()
	{
		assert(m->getConstant());
		if (m)
			delete m;
		m = NULL;
	}
	template<class D> inline NullableRef<D> cast() const
	{
		if(!m)
			return NullRef;
		D* p = static_cast<D*>(m);
		return _IMNR(p);
	}
};

//Shorthand notation
#define _NR NullableRef

template<class T>
inline std::ostream& operator<<(std::ostream& s, const NullableRef<T>& r)
{
	s << "NullableRef: ";
	if(r.isNull())
		return s << "null";
	else
		return s << *r.getPtr();
}

template<class T>
Ref<T> _IMR(T* a)
{
	Ref<T> r(a);
	r.id = rand();
	a->incRefRef(r.id);
	return r;
}

template<class T>
NullableRef<T> _IMNR(T* a)
{
	NullableRef<T> r(a);
	r.id = rand();
	a->incRefRef(r.id);
	return r;
}

template<class T>
Ref<T> _MR(NullableRef<T> a)
{
	return Ref<T>(a);
}

template<class T>
NullableRef<T> _MNR_(T* a)
{
	NullableRef<T> r(a);
	a->incRefRef(r.id);
	return r;
}

template<class T>
NullableRef<T> _MNR(T* a)
{
	return NullableRef<T>(a);
}

template<class T> template<class D> Ref<T>::Ref(const NullableRef<D>& r):m(r.getPtr())
{
	id = rand();
	assert(m);
	m->incRefRef(id);
}

template<class T> template<class D> bool Ref<T>::operator==(const NullableRef<D>&r) const
{
	return m==r.getPtr();
}

};

#endif /* SMARTREFS_H */
