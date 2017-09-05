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

#include "scripting/toplevel/Array.h"
#include "scripting/toplevel/Number.h"
#include "scripting/toplevel/Integer.h"
#include "scripting/toplevel/UInteger.h"
#include "scripting/toplevel/toplevel.h"
#include "scripting/flash/system/flashsystem.h"
#include "compat.h"
#include "asobject.h"
#include "swf.h"

#ifndef SCRIPTING_CLASS_H
#define SCRIPTING_CLASS_H 1

namespace lightspark
{

template<typename T>
class ClassName
{
public:
	static const char* ns;
	static const char* name;
	static unsigned int id;
};

class Class_inherit:public Class_base
{
private:
	asAtom getInstance(bool construct, std::vector<asAtom>& args, const unsigned int argslen, Class_base* realClass);
	DictionaryTag const* tag;
	bool bindedToRoot;
	void recursiveBuild(ASObject* target) const;
	const traits_info* classtrait;
public:
	Class_inherit(const QName& name, MemoryAccount* m,const traits_info* _classtrait);
	bool destruct()
	{
		class_scope.clear();
		return Class_base::destruct();
	}
	void buildInstanceTraits(ASObject* o) const;
	void setupDeclaredTraits(ASObject *target) const;
	void bindToTag(DictionaryTag const* t)
	{
		tag=t;
	}
	void bindToRoot()
	{
		bindedToRoot=true;
	}
	bool isBinded() const
	{
		return tag || bindedToRoot;
	}
	//Closure stack
	std::vector<scope_entry> class_scope;
	virtual void describeClassMetadata(pugi::xml_node &root) const;

};

/* helper function: does Class<ASObject>::getInstances(), but solves forward declaration problem */
ASObject* new_asobject(SystemState *sys);
Prototype* new_objectPrototype(SystemState *sys);
Prototype* new_functionPrototype(Class_base* functionClass, _NR<Prototype> p);
Function_object* new_functionObject(_NR<ASObject> p);
ObjectConstructor* new_objectConstructor(Class_base* cls,uint32_t length);

template<class T,std::size_t N>
struct newWithOptionalClass
{
	template<class F, typename... Args>
	static T* doNew(Class_base* c, const F& f, Args&&... args)
	{
		return newWithOptionalClass<T, N-1>::doNew(c, std::forward<Args>(args)..., f);
	}
};
template<class T>
struct newWithOptionalClass<T, 1>
{
	template<class F, typename... Args>
	static T* doNew(Class_base* c, const F& f, Args&&... args)
	{
		//Last parameter is not a class pointer
		return new (c->memoryAccount) T(c, std::forward<Args>(args)..., f);
	}
	template<typename... Args>
	static T* doNew(Class_base* c, Class_base* f, Args&&... args)
	{
		//Last parameter is a class pointer
		return new (f->memoryAccount) T(f, std::forward<Args>(args)...);
	}
};

template<class T>
struct newWithOptionalClass<T, 0>
{
	static T* doNew(Class_base* c)
	{
		//Last parameter is not a class pointer
		return new (c->memoryAccount) T(c);
	}
};

template< class T>
class Class: public Class_base
{
protected:
	Class(const QName& name, MemoryAccount* m):Class_base(name, m){}
	//This function is instantiated always because of inheritance
	asAtom getInstance(bool construct, std::vector<asAtom>& args, const unsigned int argslen, Class_base* realClass=NULL)
	{
		if(realClass==NULL)
			realClass=this;
		asAtom ret = asAtom::fromObject(realClass->freelist[0].getObjectFromFreeList());
		if (ret.type == T_INVALID)
			ret=asAtom::fromObject(new (realClass->memoryAccount) T(realClass));
		if(construct)
			handleConstruction(ret,args,argslen,true);
		return ret;
	}
public:
	/*
	template<typename... Args>
	static T* getInstanceS(Args&&... args)
	{
		Class<T>* c=Class<T>::getClass();
		T* ret=newWithOptionalClass<T, sizeof...(Args)>::doNew(c, std::forward<Args>(args)...);
		c->handleConstruction(ret,NULL,0,true);
		return ret;
	}
	*/
	template<typename... Args>
	static T* getInstanceSRaw(SystemState* sys, Args&&... args)
	{
		Class<T>* c=static_cast<Class<T>*>(sys->builtinClasses[ClassName<T>::id]);
		if (!c)
			c = getClass(sys);
		T* ret=newWithOptionalClass<T, sizeof...(Args)>::doNew(c, std::forward<Args>(args)...);
		c->setupDeclaredTraits(ret);
		ret->constructionComplete();
		ret->setConstructIndicator();
		return ret;
	}
	template<typename... Args>
	static Ref<T> getInstanceS(SystemState* sys, Args&&... args)
	{
		Class<T>* c=static_cast<Class<T>*>(sys->builtinClasses[ClassName<T>::id]);
		if (!c)
			c = getClass(sys);
		T* ret=newWithOptionalClass<T, sizeof...(Args)>::doNew(c, std::forward<Args>(args)...);
		c->setupDeclaredTraits(ret);
		ret->constructionComplete();
		ret->setConstructIndicator();
		return _MR(ret);
	}
	// constructor without arguments
	inline static T* getInstanceSNoArgs(SystemState* sys)
	{
		Class<T>* c=static_cast<Class<T>*>(sys->builtinClasses[ClassName<T>::id]);
		if (!c)
			c = getClass(sys);
		T* ret = c->freelist[0].getObjectFromFreeList()->as<T>();
		if (!ret)
		{
			ret=new (c->memoryAccount) T(c);
			assert_and_throw(ret);
		}
		ret->setIsInitialized();
		ret->constructionComplete();
		ret->setConstructIndicator();
		return ret;
	}
	inline static Class<T>* getClass(SystemState* sys)
	{
		uint32_t classId=ClassName<T>::id;
		Class<T>* ret=NULL;
		Class_base** retAddr= &sys->builtinClasses[classId];
		if(*retAddr==NULL)
		{
			//Create the class
			QName name(sys->getUniqueStringId(ClassName<T>::name),sys->getUniqueStringId(ClassName<T>::ns));
			ret=new (sys->unaccountedMemory) Class<T>(name, sys->unaccountedMemory);
			ret->setSystemState(sys);
			ret->incRefStatic();
			*retAddr=ret;
			ret->prototype = _MNR(new_objectPrototype(sys));
			T::sinit(ret);

			ret->initStandardProps();
		}
		else
			ret=static_cast<Class<T>*>(*retAddr);

		return ret;
	}
	static _R<Class<T>> getRef(SystemState* sys)
	{
		Class<T>* ret = getClass(sys);
		return _IMR(ret);
	}
	static T* cast(ASObject* o)
	{
		return static_cast<T*>(o);
	}
	static T* dyncast(ASObject* o)
	{
		return dynamic_cast<T*>(o);
	}
	void buildInstanceTraits(ASObject* o) const
	{
		T::buildTraits(o);
	}
	asAtom generator(std::vector<asAtom>& args, const unsigned int argslen)
	{
		asAtom ret=T::generator(getSystemState(), asAtom::invalidAtom, args, argslen);
		return ret;
	}
	asAtom coerce(SystemState* sys, asAtom& o) const
	{
		return Class_base::coerce(sys,o);
	}
};

template<>
asAtom Class<Global>::getInstance(bool construct, std::vector<asAtom>& args, const unsigned int argslen, Class_base* realClass);

template<>
inline asAtom Class<Number>::coerce(SystemState* sys, asAtom& o) const
{
	switch (o.type)
	{
		case T_NUMBER:
		case T_INTEGER:
		case T_UINTEGER:
			return o;
		default:
		{
			number_t n = o.toNumber();
			return asAtom(n);
		}
	}
}

template<>
inline asAtom Class<UInteger>::coerce(SystemState* sys, asAtom& o) const
{
	if (o.type == T_UINTEGER)
		return o;
	uint32_t n = o.toUInt();
	return asAtom(n);
}

template<>
inline asAtom Class<Integer>::coerce(SystemState* sys, asAtom& o) const
{
	if (o.type == T_INTEGER)
		return o;
	int32_t n = o.toInt();
	return asAtom(n);
}

template<>
inline asAtom Class<Boolean>::coerce(SystemState* sys,asAtom& o) const
{
	if (o.type == T_BOOLEAN)
		return o;
	bool n = o.Boolean_concrete();
	return asAtom(n);
}

template<>
class Class<ASObject>: public Class_base
{
private:
	Class<ASObject>(const QName& name, MemoryAccount* m):Class_base(name, m){}
	//This function is instantiated always because of inheritance
	asAtom getInstance(bool construct, std::vector<asAtom>& args, const unsigned int argslen, Class_base* realClass=NULL);
public:
	static ASObject* getInstanceS(SystemState* sys)
	{
		Class<ASObject>* c=Class<ASObject>::getClass(sys);
		ASObject* ret = c->freelist[0].getObjectFromFreeList();
		if (!ret)
			ret=new (c->memoryAccount) ASObject(c);
		c->setupDeclaredTraits(ret);
		ret->constructionComplete();
		ret->setConstructIndicator();
		return ret;
		//return c->getInstance(true,NULL,0);
	}

	static Class<ASObject>* getClass(SystemState* sys);
	static _R<Class<ASObject>> getRef(SystemState* sys)
	{
		Class<ASObject>* ret = getClass(sys);
		return _IMR(ret);
	}
	static ASObject* cast(ASObject* o)
	{
		return static_cast<ASObject*>(o);
	}
	void buildInstanceTraits(ASObject* o) const
	{
		ASObject::buildTraits(o);
	}
	asAtom generator(std::vector<asAtom>& args, const unsigned int argslen)
	{
		if(argslen==0 || args[0].is<Null>() || args[0].is<Undefined>()) {
			return asAtom::fromObject(Class<ASObject>::getInstanceS(getSys()));
		}
		else
		{
			return args[0];
		}
	}
};

/* InterfaceClass implements interfaces. E.g., if you declare a variable of type IEventDispatcher in AS3,
 * then the type in our code will be InterfaceClass<IEventDispatcher>.
 * All classes that implement an interface in AS3 do so in the C++ code, i.e.
 * EventDispatcher derives from IEventDispatcher both in AS3 and C++ code.
 * This makes it possible to use C++ casts similar to their AS3 counterparts.
 *
 * TODO: InterfaceClass should derive from Type, not from Class_base!
 */
void lookupAndLink(Class_base* c, const tiny_string& name, const tiny_string& interfaceNs);
template<class T>
class InterfaceClass: public Class_base
{
	virtual ~InterfaceClass() { }
	void buildInstanceTraits(ASObject*) const {}
	asAtom getInstance(bool, std::vector<asAtom>&, unsigned int, Class_base* realClass)
	{
		assert(false);
		return asAtom::invalidAtom;
	}
	asAtom generator(std::vector<asAtom>& args, const unsigned int argslen)
	{
		assert(argslen == 1);
		return args[0];
	}
	InterfaceClass(const QName& name, MemoryAccount* m):Class_base(name, m) { }
public:
	static InterfaceClass<T>* getClass(SystemState* sys)
	{
		uint32_t classId=ClassName<T>::id;
		InterfaceClass<T>* ret=NULL;
		Class_base** retAddr=&sys->builtinClasses[classId];
		if(*retAddr==NULL)
		{
			//Create the class
			QName name(sys->getUniqueStringId(ClassName<T>::name),sys->getUniqueStringId(ClassName<T>::ns));
			ret=new (sys->unaccountedMemory) InterfaceClass<T>(name, sys->unaccountedMemory);
			ret->isInterface = true;
			ret->incRefStatic();
			*retAddr=ret;
		}
		else
			ret=static_cast<InterfaceClass<T>*>(*retAddr);

		return ret;
	}
	static _R<InterfaceClass<T>> getRef(SystemState* sys)
	{
		InterfaceClass<T>* ret = getClass(sys);
		return _IMR(ret);
	}
	void linkInterface(Class_base* c) const
	{
		T::linkTraits(c);
	}
};

/* This is a class which was instantiated from a Template<T> */
template<class T>
class TemplatedClass : public Class<T>
{
private:
	/* the Template<T>* this class was generated from */
	const Template_base* templ;
	std::vector<const Type*> types;
public:
	TemplatedClass(const QName& name, const std::vector<const Type*>& _types, Template_base* _templ, MemoryAccount* m)
		: Class<T>(name, m), templ(_templ), types(_types)
	{
	}

	asAtom getInstance(bool construct, std::vector<asAtom>& args, const unsigned int argslen, Class_base* realClass=NULL)
	{
		if(realClass==NULL)
			realClass=this;
		asAtom ret=asAtom::fromObject(new (realClass->memoryAccount) T(realClass));
		ret.as<T>()->setTypes(types);
		if(construct)
			this->handleConstruction(ret,args,argslen,true);
		return ret;
	}

	/* This function is called for as3 code like v = Vector.<String>(["Hello", "World"])
	 * this->types will be Class<ASString> on entry of this function.
	 */
	asAtom generator(std::vector<asAtom>& args, const unsigned int argslen)
	{
		asAtom th = asAtom::fromObject(this);
		asAtom ret = T::generator(this->getSystemState(),th,args,argslen);
		return ret;
	}

	const Template_base* getTemplate() const
	{
		return templ;
	}

	std::vector<const Type*> getTypes() const
	{
		return types;
	}
	void addType(const Type* type)
	{
		types.push_back(type);
	}

	asAtom coerce(SystemState* sys,asAtom& o) const
	{
		if (o.type == T_UNDEFINED)
		{;
			return asAtom::nullAtom;
		}
		else if ((o.getObject() && o.getObject()->is<T>() && o.as<T>()->sameType(this)) ||
				 o.type ==T_NULL)
		{
			// Vector.<x> can be coerced to Vector.<y>
			// only if x and y are the same type
			return o;
		}
		else
		{
			tiny_string clsname = o.getObject() ? o.getObject()->getClassName() : "";
			throwError<TypeError>(kCheckTypeFailedError, clsname,
								  Class<T>::getQualifiedClassName());
			return asAtom::invalidAtom; // not reached
		}
	}
};

/* this is modeled closely after the Class/Class_base pattern */
template<class T>
class Template : public Template_base
{
public:
	Template(QName name) : Template_base(name) {}

	QName getQName(SystemState* sys, const std::vector<const Type*>& types)
	{
		//This is the naming scheme that the ABC compiler uses,
		//and we need to stay in sync here
		tiny_string name = ClassName<T>::name;
		for(size_t i=0;i<types.size();++i)
		{
			name += "$";
			name += types[i]->getName();
		}
		QName ret(sys->getUniqueStringId(name),sys->getUniqueStringId(ClassName<T>::ns));
		return ret;
	}

	Class_base* applyType(const std::vector<const Type*>& types,_NR<ApplicationDomain> applicationDomain)
	{
		_NR<ApplicationDomain> appdomain = applicationDomain;
		
		// if type is a builtin class, it is handled in the systemDomain
		if (appdomain.isNull() || (types.size() > 0 && !dynamic_cast<const Class_inherit*>(types[0])))
			appdomain = getSys()->systemDomain;
		QName instantiatedQName = getQName(appdomain->getSystemState(),types);

		std::map<QName, Class_base*>::iterator it=appdomain->instantiatedTemplates.find(instantiatedQName);
		Class<T>* ret=NULL;
		if(it==appdomain->instantiatedTemplates.end()) //This class is not yet in the map, create it
		{
			ret=new (appdomain->getSystemState()->unaccountedMemory) TemplatedClass<T>(instantiatedQName,types,this,appdomain->getSystemState()->unaccountedMemory);
			appdomain->instantiatedTemplates.insert(std::make_pair(instantiatedQName,ret));
			ret->prototype = _MNR(new_objectPrototype(appdomain->getSystemState()));
			T::sinit(ret);
			if(ret->super)
				ret->prototype->prevPrototype=ret->super->prototype;
			ret->addPrototypeGetter();
		}
		else
		{
			TemplatedClass<T>* tmp = static_cast<TemplatedClass<T>*>(it->second);
			if (tmp->getTypes().size() == 0)
				tmp->addType(types[0]);
			ret= tmp;
		}

		ret->incRef();
		return ret;
	}
	Class_base* applyTypeByQName(const QName& qname,_NR<ApplicationDomain> applicationDomain)
	{
		const std::vector<const Type*> types;
		_NR<ApplicationDomain> appdomain = applicationDomain;
		std::map<QName, Class_base*>::iterator it=appdomain->instantiatedTemplates.find(qname);
		Class<T>* ret=NULL;
		if(it==appdomain->instantiatedTemplates.end()) //This class is not yet in the map, create it
		{
			ret=new (appdomain->getSystemState()->unaccountedMemory) TemplatedClass<T>(qname,types,this,appdomain->getSystemState()->unaccountedMemory);
			appdomain->instantiatedTemplates.insert(std::make_pair(qname,ret));
			ret->prototype = _MNR(new_objectPrototype(appdomain->getSystemState()));
			T::sinit(ret);
			if(ret->super)
				ret->prototype->prevPrototype=ret->super->prototype;
			ret->addPrototypeGetter();
		}
		else
			ret=static_cast<TemplatedClass<T>*>(it->second);

		return ret;
	}

	static Ref<Class_base> getTemplateInstance(SystemState* sys,const Type* type,_NR<ApplicationDomain> appdomain)
	{
		std::vector<const Type*> t(1,type);
		Ref<Template<T>> templ=getTemplate(sys);
		Ref<Class_base> ret=_MR(templ->applyType(t, appdomain));
		return ret;
	}

	static Ref<Class_base> getTemplateInstance(SystemState* sys,const QName& qname, ABCContext* context,_NR<ApplicationDomain> appdomain)
	{
		Ref<Template<T>> templ=getTemplate(sys);
		Ref<Class_base> ret=_IMR(templ->applyTypeByQName(qname,appdomain));
		ret->context = context;
		return ret;
	}
	static asAtom getInstanceS(SystemState* sys,const Type* type,_NR<ApplicationDomain> appdomain)
	{
		std::vector<asAtom> empty;
		return getTemplateInstance(sys,type,appdomain).getPtr()->getInstance(true,empty,0);
	}

	static Ref<Template<T>> getTemplate(SystemState* sys,const QName& name)
	{
		std::map<QName, Template_base*>::iterator it=sys->templates.find(name);
		Template<T>* ret=NULL;
		if(it==sys->templates.end()) //This class is not yet in the map, create it
		{
			ret=new (sys->unaccountedMemory) Template<T>(name);
			sys->templates.insert(std::make_pair(name,ret));
		}
		else
			ret=static_cast<Template<T>*>(it->second);

		return _IMR(ret);
	}

	static Ref<Template<T>> getTemplate(SystemState* sys)
	{
		return getTemplate(sys,QName(sys->getUniqueStringId(ClassName<T>::name),sys->getUniqueStringId(ClassName<T>::ns)));
	}
};

};
#endif /* SCRIPTING_CLASS_H */
