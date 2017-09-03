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

#include "scripting/toplevel/XML.h"
#include "scripting/toplevel/XMLList.h"
#include "scripting/flash/xml/flashxml.h"
#include "scripting/class.h"
#include "compat.h"
#include "scripting/argconv.h"
#include "abc.h"
#include "parsing/amf3_generator.h"
#include <unordered_set>

using namespace std;
using namespace lightspark;

static bool ignoreComments;
static bool ignoreProcessingInstructions;
static bool ignoreWhitespace;
static int32_t prettyIndent;
static bool prettyPrinting;

void setDefaultXMLSettings()
{
	ignoreComments = true;
	ignoreProcessingInstructions = true;
	ignoreWhitespace = true;
	prettyIndent = 2;
	prettyPrinting = true;
}

XML::XML(Class_base* c):ASObject(c,T_OBJECT,SUBTYPE_XML),parentNode(0),nodetype((pugi::xml_node_type)0),isAttribute(false),nodenamespace_uri(BUILTIN_STRINGS::EMPTY),nodenamespace_prefix(BUILTIN_STRINGS::EMPTY),constructed(false)
{
}

XML::XML(Class_base* c, const std::string &str):ASObject(c,T_OBJECT,SUBTYPE_XML),parentNode(0),nodetype((pugi::xml_node_type)0),isAttribute(false),nodenamespace_uri(BUILTIN_STRINGS::EMPTY),nodenamespace_prefix(BUILTIN_STRINGS::EMPTY),constructed(false)
{
	createTree(buildFromString(str, getParseMode()),false);
}

XML::XML(Class_base* c, const pugi::xml_node& _n, XML* parent, bool fromXMLList):ASObject(c,T_OBJECT,SUBTYPE_XML),parentNode(0),nodetype((pugi::xml_node_type)0),isAttribute(false),nodenamespace_uri(BUILTIN_STRINGS::EMPTY),nodenamespace_prefix(BUILTIN_STRINGS::EMPTY),constructed(false)
{
	if (parent)
	{
		parentNode = _IMNR<XML>(parent);
	}
	createTree(_n,fromXMLList);
}

bool XML::destruct()
{
	xmldoc.reset();
	parentNode.reset();
	nodetype =(pugi::xml_node_type)0;
	isAttribute = false;
	constructed = false;
	childrenlist.reset();
	nodename.clear();
	nodevalue.clear();
	nodenamespace_uri=BUILTIN_STRINGS::EMPTY;
	nodenamespace_prefix=BUILTIN_STRINGS::EMPTY;
	attributelist.reset();
	procinstlist.reset();
	namespacedefs.clear();
	return ASObject::destruct();
}

void XML::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_FINAL);
	setDefaultXMLSettings();
	c->isReusable=true;

	c->setDeclaredMethodByQName("ignoreComments","",Class<IFunction>::getFunction(c->getSystemState(),_getIgnoreComments),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("ignoreComments","",Class<IFunction>::getFunction(c->getSystemState(),_setIgnoreComments),SETTER_METHOD,false);
	c->setDeclaredMethodByQName("ignoreProcessingInstructions","",Class<IFunction>::getFunction(c->getSystemState(),_getIgnoreProcessingInstructions),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("ignoreProcessingInstructions","",Class<IFunction>::getFunction(c->getSystemState(),_setIgnoreProcessingInstructions),SETTER_METHOD,false);
	c->setDeclaredMethodByQName("ignoreWhitespace","",Class<IFunction>::getFunction(c->getSystemState(),_getIgnoreWhitespace),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("ignoreWhitespace","",Class<IFunction>::getFunction(c->getSystemState(),_setIgnoreWhitespace),SETTER_METHOD,false);
	c->setDeclaredMethodByQName("prettyIndent","",Class<IFunction>::getFunction(c->getSystemState(),_getPrettyIndent),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("prettyIndent","",Class<IFunction>::getFunction(c->getSystemState(),_setPrettyIndent),SETTER_METHOD,false);
	c->setDeclaredMethodByQName("prettyPrinting","",Class<IFunction>::getFunction(c->getSystemState(),_getPrettyPrinting),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("prettyPrinting","",Class<IFunction>::getFunction(c->getSystemState(),_setPrettyPrinting),SETTER_METHOD,false);
	c->setDeclaredMethodByQName("settings",AS3,Class<IFunction>::getFunction(c->getSystemState(),_getSettings),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("setSettings",AS3,Class<IFunction>::getFunction(c->getSystemState(),_setSettings),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("defaultSettings",AS3,Class<IFunction>::getFunction(c->getSystemState(),_getDefaultSettings),NORMAL_METHOD,false);
	// undocumented method, see http://www.docsultant.com/site2/articles/flex_internals.html#xmlNotify
	c->setDeclaredMethodByQName("setNotification",AS3,Class<IFunction>::getFunction(c->getSystemState(),setNotification),NORMAL_METHOD,true); // undocumented

	c->prototype->setVariableByQName("toString","",Class<IFunction>::getFunction(c->getSystemState(),_toString),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("toString",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toString),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("valueOf","",Class<IFunction>::getFunction(c->getSystemState(),valueOf),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("valueOf",AS3,Class<IFunction>::getFunction(c->getSystemState(),valueOf),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("toXMLString",AS3,Class<IFunction>::getFunction(c->getSystemState(),toXMLString),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("nodeKind","",Class<IFunction>::getFunction(c->getSystemState(),nodeKind),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("nodeKind",AS3,Class<IFunction>::getFunction(c->getSystemState(),nodeKind),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("child",AS3,Class<IFunction>::getFunction(c->getSystemState(),child),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("child","",Class<IFunction>::getFunction(c->getSystemState(),child),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("children",AS3,Class<IFunction>::getFunction(c->getSystemState(),children),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("childIndex",AS3,Class<IFunction>::getFunction(c->getSystemState(),childIndex),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("contains",AS3,Class<IFunction>::getFunction(c->getSystemState(),contains),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("attribute",AS3,Class<IFunction>::getFunction(c->getSystemState(),attribute),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("attribute","",Class<IFunction>::getFunction(c->getSystemState(),attribute),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("attributes",AS3,Class<IFunction>::getFunction(c->getSystemState(),attributes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("length",AS3,Class<IFunction>::getFunction(c->getSystemState(),length),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("localName",AS3,Class<IFunction>::getFunction(c->getSystemState(),localName),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("localName","",Class<IFunction>::getFunction(c->getSystemState(),localName),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("name",AS3,Class<IFunction>::getFunction(c->getSystemState(),name),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("namespace",AS3,Class<IFunction>::getFunction(c->getSystemState(),_namespace),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("namespace","",Class<IFunction>::getFunction(c->getSystemState(),_namespace),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("normalize",AS3,Class<IFunction>::getFunction(c->getSystemState(),_normalize),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("descendants",AS3,Class<IFunction>::getFunction(c->getSystemState(),descendants),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("appendChild",AS3,Class<IFunction>::getFunction(c->getSystemState(),_appendChild),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("parent",AS3,Class<IFunction>::getFunction(c->getSystemState(),parent),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("inScopeNamespaces",AS3,Class<IFunction>::getFunction(c->getSystemState(),inScopeNamespaces),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("addNamespace",AS3,Class<IFunction>::getFunction(c->getSystemState(),addNamespace),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("hasSimpleContent",AS3,Class<IFunction>::getFunction(c->getSystemState(),_hasSimpleContent),DYNAMIC_TRAIT);
	c->prototype->setVariableByQName("hasComplexContent",AS3,Class<IFunction>::getFunction(c->getSystemState(),_hasComplexContent),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("text",AS3,Class<IFunction>::getFunction(c->getSystemState(),text),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("elements",AS3,Class<IFunction>::getFunction(c->getSystemState(),elements),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("elements","",Class<IFunction>::getFunction(c->getSystemState(),elements),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("setLocalName",AS3,Class<IFunction>::getFunction(c->getSystemState(),_setLocalName),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("setName",AS3,Class<IFunction>::getFunction(c->getSystemState(),_setName),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("setNamespace",AS3,Class<IFunction>::getFunction(c->getSystemState(),_setNamespace),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("copy",AS3,Class<IFunction>::getFunction(c->getSystemState(),_copy),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("setChildren",AS3,Class<IFunction>::getFunction(c->getSystemState(),_setChildren),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("toJSON",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toJSON),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("insertChildAfter",AS3,Class<IFunction>::getFunction(c->getSystemState(),insertChildAfter),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("insertChildBefore",AS3,Class<IFunction>::getFunction(c->getSystemState(),insertChildBefore),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("namespaceDeclarations",AS3,Class<IFunction>::getFunction(c->getSystemState(),namespaceDeclarations),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("removeNamespace",AS3,Class<IFunction>::getFunction(c->getSystemState(),removeNamespace),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("comments",AS3,Class<IFunction>::getFunction(c->getSystemState(),comments),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("processingInstructions",AS3,Class<IFunction>::getFunction(c->getSystemState(),processingInstructions),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("propertyIsEnumerable",AS3,Class<IFunction>::getFunction(c->getSystemState(),_propertyIsEnumerable),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("hasOwnProperty",AS3,Class<IFunction>::getFunction(c->getSystemState(),_hasOwnProperty),DYNAMIC_TRAIT);
	c->setDeclaredMethodByQName("prependChild",AS3,Class<IFunction>::getFunction(c->getSystemState(),_prependChild),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("replace",AS3,Class<IFunction>::getFunction(c->getSystemState(),_replace),NORMAL_METHOD,true);
}

ASFUNCTIONBODY_ATOM(XML,generator)
{
	assert_and_throw(argslen<=1);
	if (argslen == 0)
	{
		return asAtom::fromObject(Class<XML>::getInstanceSNoArgs(getSys()));
	}
	else if (args[0]->is<Null>() || args[0]->is<Undefined>())
	{
		return asAtom::fromObject(Class<XML>::getInstanceSNoArgs(getSys()));
	}
	else if(args[0]->is<ASString>() ||
		args[0]->is<Number>() ||
		args[0]->is<Integer>() ||
		args[0]->is<UInteger>() ||
		args[0]->is<Boolean>())
	{
		return asAtom::fromObject(createFromString(getSys(),args[0]->toString()));
	}
	else if(args[0]->is<XML>())
	{
		return args[0];
	}
	else if(args[0]->is<XMLList>())
	{
		_R<XML> ret=args[0]->as<XMLList>()->reduceToXML();
		return asAtom::fromObject(ret.getPtr());
	}
	else if(args[0]->is<XMLNode>())
	{
		return asAtom::fromObject(createFromNode(args[0]->as<XMLNode>()->node));
	}
	else
	{
		return asAtom::fromObject(createFromString(getSys(),args[0]->toString()));
	}
}

ASFUNCTIONBODY(XML,_constructor)
{
	assert_and_throw(argslen<=1);
	XML* th=Class<XML>::cast(obj);
	if(argslen==0 && th->constructed) //If root is already set we have been constructed outside AS code
		return NULL;

	if(argslen==0 ||
	   args[0]->is<Null>() || 
	   args[0]->is<Undefined>())
	{
		th->createTree(th->buildFromString("", getParseMode()),false);
	}
	else if(args[0]->getClass()->isSubClass(Class<ByteArray>::getClass(obj->getSystemState())))
	{
		//Official documentation says that generic Objects are not supported.
		//ByteArray seems to be though (see XML test) so let's support it
		ByteArray* ba=Class<ByteArray>::cast(args[0]);
		uint32_t len=ba->getLength();
		const uint8_t* str=ba->getBuffer(len, false);
		th->createTree(th->buildFromString(std::string((const char*)str,len), getParseMode(),
					     getVm(obj->getSystemState())->getDefaultXMLNamespace()),false);
	}
	else if(args[0]->is<ASString>() ||
		args[0]->is<Number>() ||
		args[0]->is<Integer>() ||
		args[0]->is<UInteger>() ||
		args[0]->is<Boolean>())
	{
		//By specs, XML constructor will only convert to string Numbers or Booleans
		//ints are not explicitly mentioned, but they seem to work
		th->createTree(th->buildFromString(args[0]->toString(), getParseMode(),
					     getVm(obj->getSystemState())->getDefaultXMLNamespace()),false);
	}
	else if(args[0]->is<XML>())
	{
		th->createTree(th->buildFromString(args[0]->as<XML>()->toXMLString_internal(), getParseMode(),
					     getVm(obj->getSystemState())->getDefaultXMLNamespace()),false);
	}
	else if(args[0]->is<XMLList>())
	{
		XMLList *list=args[0]->as<XMLList>();
		_R<XML> reduced=list->reduceToXML();
		th->createTree(th->buildFromString(reduced->toXMLString_internal(), getParseMode()),false);
	}
	else
	{
		th->createTree(th->buildFromString(args[0]->toString(), getParseMode(),
					     getVm(obj->getSystemState())->getDefaultXMLNamespace()),false);
	}

	return NULL;
}

ASFUNCTIONBODY(XML,nodeKind)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==0);
	return abstract_s(obj->getSystemState(),th->nodekindString());
}
const char *XML::nodekindString()
{
	if (isAttribute)
		return "attribute";
	switch(nodetype)
	{
		case pugi::node_element:
			return "element";
		case pugi::node_cdata:
		case pugi::node_pcdata:
		case pugi::node_null:
			return "text";
		case pugi::node_comment:
			return "comment";
		case pugi::node_pi:
			return "processing-instruction";
		default:
		{
			LOG(LOG_ERROR,"Unsupported XML type " << nodetype);
			throw UnsupportedException("Unsupported XML node type");
		}
	}
}

ASFUNCTIONBODY(XML,length)
{
	return abstract_i(obj->getSystemState(),1);
}

ASFUNCTIONBODY(XML,localName)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==0);
	if(!th->isAttribute && (th->nodetype==pugi::node_pcdata || th->nodetype==pugi::node_comment || th->nodetype==pugi::node_null))
		return obj->getSystemState()->getNullRef();
	else
		return abstract_s(obj->getSystemState(),th->nodename);
}

ASFUNCTIONBODY(XML,name)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==0);
	//TODO: add namespace
	if(!th->isAttribute && (th->nodetype==pugi::node_pcdata || th->nodetype==pugi::node_comment || th->nodetype==pugi::node_null))
		return obj->getSystemState()->getNullRef();
	else
	{
		ASQName* ret = Class<ASQName>::getInstanceSNoArgs(obj->getSystemState());
		ret->setByXML(th);
		return ret;
	}
}

ASFUNCTIONBODY(XML,descendants)
{
	XML* th=Class<XML>::cast(obj);
	_NR<ASObject> name;
	ARG_UNPACK(name,_NR<ASObject>(abstract_s(obj->getSystemState(),"*")));
	XMLVector ret;
	multiname mname(NULL);
	name->applyProxyProperty(mname);
	th->getDescendantsByQName(name->toString(),mname.isQName() ? mname.ns[0].nsNameId : (uint32_t)BUILTIN_STRINGS::EMPTY,mname.isAttribute,ret);
	return XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),multiname(NULL));
}

ASFUNCTIONBODY_ATOM(XML,_appendChild)
{
	XML* th=obj->as<XML>();
	assert_and_throw(argslen==1);
	_NR<XML> arg;
	if(args[0].getObject()->getClass()==Class<XML>::getClass(sys))
	{
		arg=_IMR(args[0]->as<XML>());
	}
	else if(args[0].getObject()->getClass()==Class<XMLList>::getClass(sys))
	{
		XMLList* list=args[0]->as<XMLList>();
		list->appendNodesTo(th);
		return asAtom::fromObject(th);
	}
	else
	{
		//The appendChild specs says that any other type is converted to string
		//NOTE: this is explicitly different from XML constructor, that will only convert to
		//string Numbers and Booleans
		arg=_MR(createFromString(sys,"dummy"));
		//avoid interpretation of the argument, just set it as text node
		arg->setTextContent(args[0]->toString());
	}

	th->appendChild(arg);
	return asAtom::fromObject(th);
}

void XML::appendChild(_R<XML> newChild)
{
	if (newChild->constructed)
	{
		if (this == newChild.getPtr())
			throwError<TypeError>(kXMLIllegalCyclicalLoop);
		_NR<XML> node = this->parentNode;
		while (!node.isNull())
		{
			if (node == newChild)
				throwError<TypeError>(kXMLIllegalCyclicalLoop);
			node = node->parentNode;
		}
		newChild->parentNode = _IMNR<XML>(this);
		childrenlist->append(newChild);
	}
}

/* returns the named attribute in an XMLList */
ASFUNCTIONBODY(XML,attribute)
{
	XML* th = obj->as<XML>();
	tiny_string attrname;
	//see spec for QName handling
	ARG_UNPACK (attrname);
	uint32_t tmpns = BUILTIN_STRINGS::EMPTY;
	if(argslen > 0 && args[0]->is<ASQName>())
	{
		tmpns= args[0]->as<ASQName>()->getURI();
		attrname = obj->getSystemState()->getStringFromUniqueId(args[0]->as<ASQName>()->getLocalName());
			
	}

	XMLVector tmp;
	XMLList* res = XMLList::create(obj->getSystemState(),tmp,th->getChildrenlist(),multiname(NULL));
	if (!th->attributelist.isNull())
	{
		for (XMLList::XMLListVector::const_iterator it = th->attributelist->nodes.begin(); it != th->attributelist->nodes.end(); it++)
		{
			_R<XML> attr = *it;
			if (attr->nodenamespace_uri == tmpns && (attrname== "*" || attr->nodename == attrname))
			{
				res->append(attr);
			}
		}
	}
	return res;
}

ASFUNCTIONBODY_ATOM(XML,attributes)
{
	assert_and_throw(argslen==0);
	return asAtom::fromObject(obj->as<XML>()->getAllAttributes().getPtr());
}

_NR<XMLList> XML::getAllAttributes()
{
	return attributelist;
}

const tiny_string XML::toXMLString_internal(bool pretty, uint32_t defaultnsprefix, const char *indent,bool bfirst)
{
	tiny_string res;
	set<uint32_t> seen_prefix;

	if (bfirst)
	{
		uint32_t defns = getVm(getSystemState())->getDefaultXMLNamespaceID();
		XML* tmp = this;
		bool bfound = false;
		while(tmp)
		{
			for (uint32_t j = 0; j < tmp->namespacedefs.size(); j++)
			{
				bool b;
				_R<Namespace> tmpns = tmp->namespacedefs[j];
				if (tmpns->getURI() == defns)
				{
					defaultnsprefix = tmpns->getPrefix(b);
					bfound = true;
					break;
				}
			}
			if (!bfound && tmp->parentNode)
				tmp = tmp->parentNode.getPtr();
			else
				break;
		}
	}
	if (isAttribute)
		res += encodeToXML(nodevalue,true);
	else
	{
	/*
		if (!ignoreProcessingInstructions && !procinstlist.isNull())
		{

			for (uint32_t i = 0; i < procinstlist->nodes.size(); i++)
			{
				_R<XML> child= procinstlist->nodes[i];
				res += child->toXMLString_internal(pretty,defaultnsprefix,indent,false);
			LOG(LOG_INFO,"printpi:"<<res);
				if (pretty && prettyPrinting)
					res += "\n";
			}
		}
		*/
		switch (nodetype)
		{
			case pugi::node_pcdata:
				res += indent;
				res += encodeToXML(nodevalue,false);
				break;
			case pugi::node_comment:
				res += indent;
				res += "<!--";
				res += nodevalue;
				res += "-->";
				break;
			case pugi::node_declaration:
			case pugi::node_pi:
				if (ignoreProcessingInstructions)
					break;
				res += indent;
				res += "<?";
				res +=this->nodename;
				res += " ";
				res += nodevalue;
				res += "?>";
				break;
			case pugi::node_element:
			{
				uint32_t curprefix = this->nodenamespace_prefix;
				res += indent;
				res += "<";
				if (this->nodenamespace_prefix == BUILTIN_STRINGS::EMPTY)
				{
					if (defaultnsprefix != BUILTIN_STRINGS::EMPTY)
					{
						res += getSystemState()->getStringFromUniqueId(defaultnsprefix);
						res += ":";
						curprefix = defaultnsprefix;
					}
				}
				else
				{
					res += this->getSystemState()->getStringFromUniqueId(this->nodenamespace_prefix);
					res += ":";
				}
				res +=this->nodename;
				for (uint32_t i = 0; i < this->namespacedefs.size(); i++)
				{
					bool b;
					_R<Namespace> tmpns = this->namespacedefs[i];
					uint32_t tmpprefix = tmpns->getPrefix(b);
					if(tmpprefix == BUILTIN_STRINGS::EMPTY || tmpprefix==this->nodenamespace_prefix || seen_prefix.find(tmpprefix)!=seen_prefix.end())
						continue;
					seen_prefix.insert(tmpprefix);
					res += " xmlns:";
					res += getSystemState()->getStringFromUniqueId(tmpprefix);
					res += "=\"";
					res += getSystemState()->getStringFromUniqueId(tmpns->getURI());
					res += "\"";
				}
				if (this->parentNode)
				{
					if (bfirst)
					{
						XML* tmp = this->parentNode.getPtr();
						while(tmp)
						{
							for (uint32_t i = 0; i < tmp->namespacedefs.size(); i++)
							{
								bool b;
								_R<Namespace> tmpns = tmp->namespacedefs[i];
								uint32_t tmpprefix = tmpns->getPrefix(b);
								if(tmpprefix != BUILTIN_STRINGS::EMPTY && seen_prefix.find(tmpprefix)==seen_prefix.end())
								{
									seen_prefix.insert(tmpprefix);
									res += " xmlns:";
									res += getSystemState()->getStringFromUniqueId(tmpprefix);
									res += "=\"";
									res += getSystemState()->getStringFromUniqueId(tmpns->getURI());
									res += "\"";
								}
							}
							if (tmp->parentNode)
								tmp = tmp->parentNode.getPtr();
							else
								break;
						}
					}
					else if (curprefix != BUILTIN_STRINGS::EMPTY)
					{
						XML* tmp = this->parentNode.getPtr();
						bool bfound = false;
						while(tmp)
						{
							for (uint32_t i = 0; i < tmp->namespacedefs.size(); i++)
							{
								bool b;
								_R<Namespace> tmpns = tmp->namespacedefs[i];
								uint32_t tmpprefix = tmpns->getPrefix(b);
								if(tmpprefix == curprefix)
								{
									seen_prefix.insert(tmpprefix);
									bfound = true;
									break;
								}
							}
							if (!bfound && tmp->parentNode)
								tmp = tmp->parentNode.getPtr();
							else
								break;
						}
					}
				}
				if (this->nodenamespace_uri != BUILTIN_STRINGS::EMPTY && 
						((this->nodenamespace_prefix==BUILTIN_STRINGS::EMPTY && defaultnsprefix == BUILTIN_STRINGS::EMPTY) ||
						 (this->nodenamespace_prefix!=BUILTIN_STRINGS::EMPTY && seen_prefix.find(this->nodenamespace_prefix)==seen_prefix.end())))
				{
					if (this->nodenamespace_prefix!=BUILTIN_STRINGS::EMPTY)
					{
						seen_prefix.insert(this->nodenamespace_prefix);
						res += " xmlns:";
						res += getSystemState()->getStringFromUniqueId(this->nodenamespace_prefix);
					}
					else
						res += " xmlns";
					res += "=\"";
					res += getSystemState()->getStringFromUniqueId(this->nodenamespace_uri);
					res += "\"";
				}
				else if (defaultnsprefix != BUILTIN_STRINGS::EMPTY && seen_prefix.find(defaultnsprefix)==seen_prefix.end())
				{
					seen_prefix.insert(defaultnsprefix);
					res += " xmlns:";
					res += getSystemState()->getStringFromUniqueId(defaultnsprefix);
					res += "=\"";
					res += getVm(getSystemState())->getDefaultXMLNamespace();
					res += "\"";
				}
				if (!attributelist.isNull())
				{
					for (XMLList::XMLListVector::const_iterator it = attributelist->nodes.begin(); it != attributelist->nodes.end(); it++)
					{
						_R<XML> attr = *it;
						res += " ";
						if (attr->nodenamespace_prefix != BUILTIN_STRINGS::EMPTY)
						{
							res += getSystemState()->getStringFromUniqueId(attr->nodenamespace_prefix);
							res += ":";
						}
						res += attr->nodename;
						res += "=\"";
						res += encodeToXML(attr->nodevalue,true);
						res += "\"";
					}
				}
				if (childrenlist.isNull() || childrenlist->nodes.size() == 0)
				{
					res += "/>";
					break;
				}
				res += ">";
				tiny_string newindent;
				bool bindent = (pretty && prettyPrinting && prettyIndent >=0 && 
								!childrenlist.isNull() &&
								(childrenlist->nodes.size() >1 || 
								 (!childrenlist->nodes[0]->procinstlist.isNull()) ||
								 (childrenlist->nodes[0]->nodetype != pugi::node_pcdata && childrenlist->nodes[0]->nodetype != pugi::node_cdata)));
				if (bindent)
				{
					newindent = indent;
					for (int32_t j = 0; j < prettyIndent; j++)
					{
						newindent += " ";
					}
				}
				if (!childrenlist.isNull())
				{
					for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
					{
						_R<XML> child= childrenlist->nodes[i];
						tiny_string tmpres = child->toXMLString_internal(pretty,defaultnsprefix,newindent.raw_buf(),false);
						if (bindent && !tmpres.empty())
							res += "\n";
						res += tmpres;
					}
				}
				if (bindent)
				{
					res += "\n";
					res += indent;
				}
				res += "</";
				if (this->nodenamespace_prefix == BUILTIN_STRINGS::EMPTY)
				{
					if (defaultnsprefix != BUILTIN_STRINGS::EMPTY)
					{
						res += getSystemState()->getStringFromUniqueId(defaultnsprefix);
						res += ":";
					}
				}
				else
				{
					res += getSystemState()->getStringFromUniqueId(this->nodenamespace_prefix);
					res += ":";
				}
				res += this->nodename;
				res += ">";
				break;
			}
			case pugi::node_cdata:
				res += "<![CDATA[";
				res += nodevalue;
				res += "]]>";
				break;
			default:
				LOG(LOG_NOT_IMPLEMENTED,"XML::toXMLString unhandled nodetype:"<<nodetype);
				break;
		}
	}
	return res;
}

ASFUNCTIONBODY(XML,toXMLString)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==0);
	tiny_string res = th->toXMLString_internal();
	ASString* ret=abstract_s(obj->getSystemState(),res);
	return ret;
}

void XML::childrenImpl(XMLVector& ret, const tiny_string& name)
{
	if (!childrenlist.isNull())
	{
		for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
		{
			_R<XML> child= childrenlist->nodes[i];
			if(name!="*" && child->nodename != name)
				continue;
			ret.push_back(child);
		}
	}
}

void XML::childrenImpl(XMLVector& ret, uint32_t index)
{
	if (constructed && !childrenlist.isNull() && index < childrenlist->nodes.size())
	{
		_R<XML> child= childrenlist->nodes[index];
		ret.push_back(child);
	}
}

ASFUNCTIONBODY(XML,child)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==1);
	const tiny_string& arg0=args[0]->toString();
	XMLVector ret;
	uint32_t index=0;
	multiname mname(NULL);
	mname.name_s_id=obj->getSystemState()->getUniqueStringId(arg0);
	mname.name_type=multiname::NAME_STRING;
	mname.ns.emplace_back(obj->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	mname.isAttribute=false;
	if(XML::isValidMultiname(obj->getSystemState(),mname, index))
		th->childrenImpl(ret, index);
	else
		th->childrenImpl(ret, arg0);
	XMLList* retObj=XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),mname);
	return retObj;
}

ASFUNCTIONBODY(XML,children)
{
	XML* th=Class<XML>::cast(obj);
	assert_and_throw(argslen==0);
	XMLVector ret;
	th->childrenImpl(ret, "*");
	multiname mname(NULL);
	mname.name_s_id=BUILTIN_STRINGS::STRING_WILDCARD;
	mname.name_type=multiname::NAME_STRING;
	mname.ns.emplace_back(obj->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	XMLList* retObj=XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),mname);
	return retObj;
}

ASFUNCTIONBODY(XML,childIndex)
{
	XML* th=Class<XML>::cast(obj);
	if (th->parentNode && !th->parentNode->childrenlist.isNull())
	{
		XML* parent = th->parentNode.getPtr();
		for (uint32_t i = 0; i < parent->childrenlist->nodes.size(); i++)
		{
			ASObject* o= parent->childrenlist->nodes[i].getPtr();
			if (o == th)
				return abstract_i(obj->getSystemState(),i);
		}
	}
	return abstract_i(obj->getSystemState(),-1);
}

ASFUNCTIONBODY(XML,_hasSimpleContent)
{
	XML *th=static_cast<XML*>(obj);
	return abstract_b(obj->getSystemState(),th->hasSimpleContent());
}

ASFUNCTIONBODY(XML,_hasComplexContent)
{
	XML *th=static_cast<XML*>(obj);
	return abstract_b(obj->getSystemState(),th->hasComplexContent());
}

ASFUNCTIONBODY_ATOM(XML,valueOf)
{
	return obj;
}

void XML::getText(XMLVector& ret)
{
	if (childrenlist.isNull())
		return;
	for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
	{
		_R<XML> child= childrenlist->nodes[i];
		if (child->getNodeKind() == pugi::node_pcdata  ||
			child->getNodeKind() == pugi::node_cdata)
		{
			ret.push_back( child );
		}
	}
}

ASFUNCTIONBODY(XML,text)
{
	XML *th = obj->as<XML>();
	ARG_UNPACK;
	XMLVector ret;
	th->getText(ret);
	return XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),multiname(NULL));
}

ASFUNCTIONBODY(XML,elements)
{
	XMLVector ret;
	XML *th = obj->as<XML>();
	tiny_string name;
	ARG_UNPACK (name, "");
	if (name=="*")
		name="";

	th->getElementNodes(name, ret);
	return XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),multiname(NULL));
}

void XML::getElementNodes(const tiny_string& name, XMLVector& foundElements)
{
	if (childrenlist.isNull())
		return;
	for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
	{
		_R<XML> child= childrenlist->nodes[i];
		if(child->nodetype==pugi::node_element && (name.empty() || name == child->nodename))
		{
			foundElements.push_back( child );
		}
	}
}

ASFUNCTIONBODY(XML,inScopeNamespaces)
{
	XML *th = obj->as<XML>();
	Array *namespaces = Class<Array>::getInstanceSNoArgs(obj->getSystemState());
	set<uint32_t> seen_prefix;

	XML* tmp = th;
	while(tmp)
	{
		for (uint32_t i = 0; i < tmp->namespacedefs.size(); i++)
		{
			bool b;
			_R<Namespace> tmpns = tmp->namespacedefs[i];
			if(seen_prefix.find(tmpns->getPrefix(b))==seen_prefix.end())
			{
				asAtom element = asAtom::fromObject(tmpns.getPtr());
				namespaces->push(element);
				seen_prefix.insert(tmp->nodenamespace_prefix);
			}
		}
		if (tmp->parentNode)
			tmp = tmp->parentNode.getPtr();
		else
			break;
	}
	return namespaces;
}

ASFUNCTIONBODY(XML,addNamespace)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> newNamespace;
	ARG_UNPACK(newNamespace);


	uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
	uint32_t ns_prefix = BUILTIN_STRINGS::EMPTY;
	if (newNamespace->is<Namespace>())
	{
		Namespace* tmpns = newNamespace->as<Namespace>();
		bool b;
		ns_prefix = tmpns->getPrefix(b);
		ns_uri = tmpns->getURI();
	}
	else if (newNamespace->is<ASQName>())
	{
		ns_uri = newNamespace->as<ASQName>()->getURI();
	}
	else
		ns_uri = th->getSystemState()->getUniqueStringId(newNamespace->toString());
	if (th->nodenamespace_prefix == ns_prefix)
		th->nodenamespace_prefix=BUILTIN_STRINGS::EMPTY;
	for (uint32_t i = 0; i < th->namespacedefs.size(); i++)
	{
		bool b;
		_R<Namespace> tmpns = th->namespacedefs[i];
		if (tmpns->getPrefix(b) == ns_prefix)
		{
			th->namespacedefs[i] = _R<Namespace>(Class<Namespace>::getInstanceS(obj->getSystemState(),ns_uri,ns_prefix));
			return NULL;
		}
	}
	th->namespacedefs.push_back(_R<Namespace>(Class<Namespace>::getInstanceS(obj->getSystemState(),ns_uri,ns_prefix)));
	return NULL;
}

ASObject *XML::getParentNode()
{
	if (parentNode && parentNode->is<XML>())
	{
		parentNode->incRef();
		return parentNode.getPtr();
	}
	else
		return getSystemState()->getUndefinedRef();
}

ASFUNCTIONBODY(XML,parent)
{
	XML *th = obj->as<XML>();
	return th->getParentNode();
}

ASFUNCTIONBODY(XML,contains)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> value;
	ARG_UNPACK(value);
	if(!value->is<XML>())
		return abstract_b(obj->getSystemState(),false);

	return abstract_b(obj->getSystemState(),th->isEqual(value.getPtr()));
}

ASFUNCTIONBODY(XML,_namespace)
{
	XML *th = obj->as<XML>();
	tiny_string prefix;
	ARG_UNPACK(prefix, "");

	pugi::xml_node_type nodetype=th->nodetype;
	if(prefix.empty() && 
	   nodetype!=pugi::node_element && 
	   !th->isAttribute)
	{
		return obj->getSystemState()->getNullRef();
	}
	if (prefix.empty())
		return Class<Namespace>::getInstanceSRaw(obj->getSystemState(),th->nodenamespace_uri, th->nodenamespace_prefix);
		
	for (uint32_t i = 0; i < th->namespacedefs.size(); i++)
	{
		bool b;
		_R<Namespace> tmpns = th->namespacedefs[i];
		if (tmpns->getPrefix(b) == obj->getSystemState()->getUniqueStringId(prefix))
		{
			return Class<Namespace>::getInstanceSRaw(obj->getSystemState(),tmpns->getURI(), obj->getSystemState()->getUniqueStringId(prefix));
		}
	}
	return obj->getSystemState()->getUndefinedRef();
}

ASFUNCTIONBODY(XML,_setLocalName)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> newName;
	ARG_UNPACK(newName);

	if(th->nodetype==pugi::node_pcdata || th->nodetype==pugi::node_comment)
		return NULL;

	tiny_string new_name;
	if(newName->is<ASQName>())
	{
		new_name=obj->getSystemState()->getStringFromUniqueId(newName->as<ASQName>()->getLocalName());
	}
	else
	{
		new_name=newName->toString();
	}

	th->setLocalName(new_name);

	return NULL;
}

void XML::setLocalName(const tiny_string& new_name)
{
	asAtom v =asAtom::fromObject(abstract_s(getSystemState(),new_name));
	if(!isXMLName(getSystemState(),v))
	{
		throwError<TypeError>(kXMLInvalidName, new_name);
	}
	this->nodename = new_name;
}

ASFUNCTIONBODY(XML,_setName)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> newName;
	ARG_UNPACK(newName);

	if(th->nodetype==pugi::node_pcdata || th->nodetype==pugi::node_comment)
		return NULL;

	tiny_string localname;
	uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
	uint32_t ns_prefix = th->nodenamespace_prefix;
	if(newName->is<ASQName>())
	{
		ASQName *qname=newName->as<ASQName>();
		localname=obj->getSystemState()->getStringFromUniqueId(qname->getLocalName());
		ns_uri=qname->getURI();
	}
	else if (!newName->is<Undefined>())
	{
		localname=newName->toString();
		ns_prefix= BUILTIN_STRINGS::EMPTY;
	}

	th->setLocalName(localname);
	th->setNamespace(ns_uri,ns_prefix);

	return NULL;
}

ASFUNCTIONBODY(XML,_setNamespace)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> newNamespace;
	ARG_UNPACK(newNamespace);

	if(th->nodetype==pugi::node_pcdata ||
	   th->nodetype==pugi::node_comment ||
	   th->nodetype==pugi::node_pi)
		return NULL;
	uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
	uint32_t ns_prefix = BUILTIN_STRINGS::EMPTY;
	if(newNamespace->is<Namespace>())
	{
		Namespace *ns=newNamespace->as<Namespace>();
		ns_uri=ns->getURI();
		bool prefix_is_undefined=true;
		ns_prefix=ns->getPrefix(prefix_is_undefined);
	}
	else if(newNamespace->is<ASQName>())
	{
		ASQName *qname=newNamespace->as<ASQName>();
		ns_uri=qname->getURI();
		for (uint32_t i = 0; i < th->namespacedefs.size(); i++)
		{
			bool b;
			_R<Namespace> tmpns = th->namespacedefs[i];
			if (tmpns->getURI() == ns_uri)
			{
				ns_prefix = tmpns->getPrefix(b);
				break;
			}
		}
	}
	else if (!newNamespace->is<Undefined>())
	{
		ns_uri=th->getSystemState()->getUniqueStringId(newNamespace->toString());
		for (uint32_t i = 0; i < th->namespacedefs.size(); i++)
		{
			bool b;
			_R<Namespace> tmpns = th->namespacedefs[i];
			if (tmpns->getURI() == ns_uri)
			{
				ns_prefix = tmpns->getPrefix(b);
				break;
			}
		}
	}
	th->setNamespace(ns_uri, ns_prefix);
	if (th->isAttribute && th->parentNode)
	{
		XML* tmp = th->parentNode.getPtr();
		for (uint32_t i = 0; i < tmp->namespacedefs.size(); i++)
		{
			bool b;
			_R<Namespace> tmpns = tmp->namespacedefs[i];
			if (tmpns->getPrefix(b) == ns_prefix)
			{
				tmp->namespacedefs[i] = Class<Namespace>::getInstanceS(obj->getSystemState(),ns_uri,ns_prefix);
				return NULL;
			}
		}
		tmp->namespacedefs.push_back(_R<Namespace>(Class<Namespace>::getInstanceS(obj->getSystemState(),ns_uri,ns_prefix)));
	}
	
	return NULL;
}

void XML::setNamespace(uint32_t ns_uri, uint32_t ns_prefix)
{
	this->nodenamespace_prefix = ns_prefix;
	this->nodenamespace_uri = ns_uri;
}

ASFUNCTIONBODY(XML,_copy)
{
	XML* th=Class<XML>::cast(obj);
	return th->copy();
}

XML *XML::copy()
{
	return createFromString(this->getSystemState(),this->toXMLString_internal(false));
}

ASFUNCTIONBODY_ATOM(XML,_setChildren)
{
	XML* th=obj->as<XML>();
	_NR<ASObject> newChildren;
	ARG_UNPACK_ATOM(newChildren);

	th->childrenlist->clear();

	if (newChildren->is<XML>())
	{
		XML *newChildrenXML=newChildren->as<XML>();
		th->appendChild(_IMR<XML>(newChildrenXML));
	}
	else if (newChildren->is<XMLList>())
	{
		XMLList *list=newChildren->as<XMLList>();
		list->incRef();
		list->appendNodesTo(th);
	}
	else
	{
		LOG(LOG_NOT_IMPLEMENTED, "XML::setChildren supports only XMLs and XMLLists");
	}

	return asAtom::fromObject(th);
}

ASFUNCTIONBODY_ATOM(XML,_normalize)
{
	XML* th=obj->as<XML>();
	th->normalize();

	return asAtom::fromObject(th);
}

void XML::normalize()
{
	childrenlist->normalize();
}

void XML::addTextContent(const tiny_string& str)
{
	assert(getNodeKind() == pugi::node_pcdata);

	nodevalue += str;
}

void XML::setTextContent(const tiny_string& content)
{
	if (getNodeKind() == pugi::node_pcdata ||
	    isAttribute ||
	    getNodeKind() == pugi::node_comment ||
	    getNodeKind() == pugi::node_pi ||
		getNodeKind() == pugi::node_cdata)
	{
		nodevalue = content;
	}
}


bool XML::hasSimpleContent() const
{
	if (getNodeKind() == pugi::node_comment ||
		getNodeKind() == pugi::node_pi)
		return false;
	if (childrenlist.isNull())
		return true;
	for(size_t i=0; i<childrenlist->nodes.size(); i++)
	{
		if (childrenlist->nodes[i]->getNodeKind() == pugi::node_element)
			return false;
	}
	return true;
}

bool XML::hasComplexContent() const
{
	return !hasSimpleContent();
}

pugi::xml_node_type XML::getNodeKind() const
{
	return nodetype;
}


void XML::getDescendantsByQName(const tiny_string& name, uint32_t ns, bool bIsAttribute, XMLVector& ret) const
{
	if (!constructed)
		return;
	if (bIsAttribute && !attributelist.isNull())
	{
		for (uint32_t i = 0; i < attributelist->nodes.size(); i++)
		{
			_R<XML> child= attributelist->nodes[i];
			if(name=="" || name=="*" || (name == child->nodename && (ns == BUILTIN_STRINGS::STRING_WILDCARD || ns == child->nodenamespace_uri)))
			{
				ret.push_back(child);
			}
		}
	}
	if (childrenlist.isNull())
		return;
	for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
	{
		_R<XML> child= childrenlist->nodes[i];
		if(!bIsAttribute && (name=="" || name=="*" || (name == child->nodename && (ns == BUILTIN_STRINGS::STRING_WILDCARD || ns == child->nodenamespace_uri))))
		{
			ret.push_back(child);
		}
		child->getDescendantsByQName(name, ns, bIsAttribute, ret);
	}
}

XML::XMLVector XML::getAttributes()
{ 
	multiname mn(NULL);
	mn.name_type=multiname::NAME_STRING;
	mn.ns.emplace_back(getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	mn.ns.emplace_back(getSystemState(),BUILTIN_STRINGS::STRING_AS3NS,NAMESPACE);
	mn.isAttribute = true;
	return getAttributesByMultiname(mn,""); 
}
XML::XMLVector XML::getAttributesByMultiname(const multiname& name, const tiny_string& normalizedName) const
{
	XMLVector ret;
	if (attributelist.isNull())
		return ret;
	uint32_t defns = getVm(getSystemState())->getDefaultXMLNamespaceID();
	std::unordered_set<uint32_t> namespace_uri;
	bool hasDefaultNS = false;
	auto it = name.ns.cbegin();
	while (it != name.ns.cend())
	{
		switch (it->nsId)
		{
			case BUILTIN_NAMESPACES::EMPTY_NS:
				namespace_uri.insert(defns);
				hasDefaultNS=true;
				break;
			case BUILTIN_NAMESPACES::AS3_NS:
				break;
			default:
			{
				if (it->kind==NAMESPACE)
				{
					if (it->nsNameId == BUILTIN_STRINGS::EMPTY)
					{
						hasDefaultNS=true;
						namespace_uri.insert(defns);
					}
					else
						namespace_uri.insert(it->nsNameId);
				}
				break;
			}
		}
		++it;
	}
	const XMLList::XMLListVector nodes = attributelist->nodes;
	if (normalizedName.empty())
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch =(hasDefaultNS||
						  (namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	else if (normalizedName=="*")
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch =(hasDefaultNS||
						  (namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	else
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch =(normalizedName==(*child)->nodename) &&
						 ((namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.size() == 0 && childnamespace_uri == BUILTIN_STRINGS::EMPTY) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	return ret;
}
XML::XMLVector XML::getValuesByMultiname(_NR<XMLList> nodelist, const multiname& name)
{
	XMLVector ret;
	uint32_t defns = BUILTIN_STRINGS::EMPTY;
	if (nodenamespace_prefix == BUILTIN_STRINGS::EMPTY && nodenamespace_uri != BUILTIN_STRINGS::EMPTY)
		defns = nodenamespace_uri;
	else
		defns = getVm(getSystemState())->getDefaultXMLNamespaceID();
	tiny_string normalizedName= "";
	normalizedName= name.normalizedName(getSystemState());
	if (normalizedName.startsWith("@"))
		normalizedName = normalizedName.substr(1,normalizedName.end());

	std::unordered_set<uint32_t> namespace_uri;
	bool hasDefaultNS = false;
	auto it = name.ns.cbegin();
	while (it != name.ns.cend())
	{
		switch (it->nsId)
		{
			case BUILTIN_NAMESPACES::EMPTY_NS:
				namespace_uri.insert(defns);
				hasDefaultNS=true;
				break;
			case BUILTIN_NAMESPACES::AS3_NS:
				break;
			default:
			{
				if (it->kind==NAMESPACE)
				{
					if (it->nsNameId == BUILTIN_STRINGS::EMPTY)
					{
						hasDefaultNS=true;
						namespace_uri.insert(defns);
					}
					else
						namespace_uri.insert(it->nsNameId);
				}
				break;
			}
		}
		++it;
	}

	const XMLList::XMLListVector& nodes = nodelist->nodes;
	if (normalizedName.empty())
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch =(hasDefaultNS||
						  (namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	else if (normalizedName=="*")
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch =(hasDefaultNS||
						  (namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	else
	{
		for (auto child = nodes.cbegin(); child != nodes.cend(); child++)
		{
			uint32_t childnamespace_uri = (*child)->nodenamespace_uri;
			
			bool bmatch = (normalizedName==(*child)->nodename) &&
						 (hasDefaultNS||
						  (namespace_uri.find(BUILTIN_STRINGS::STRING_WILDCARD)!= namespace_uri.end()) ||
						  (namespace_uri.size() == 0 && childnamespace_uri == BUILTIN_STRINGS::EMPTY) ||
						  (namespace_uri.find(childnamespace_uri) != namespace_uri.end())
						 );
			if(bmatch)
			{
				ret.push_back(*child);
			}
		}
	}
	return ret;
}

asAtom XML::getVariableByMultiname(const multiname& name, GET_VARIABLE_OPTION opt)
{
	if((opt & SKIP_IMPL)!=0)
	{
		asAtom res=ASObject::getVariableByMultiname(name,opt);

		//If a method is not found on XML object and the
		//object is a leaf node, delegate to ASString
		if(res->type == T_INVALID && hasSimpleContent())
		{
			_NR<ASString> contentstr=_MR(abstract_s(getSystemState(),toString_priv()));
			res=contentstr->getVariableByMultiname(name, opt);
		}

		return res;
	}

	bool isAttr=name.isAttribute;
	unsigned int index=0;

	tiny_string normalizedName=name.normalizedName(getSystemState());
	if(!normalizedName.empty() && normalizedName.charAt(0)=='@')
	{
		normalizedName = normalizedName.substr(1,normalizedName.end());
		isAttr=true;
	}
	if(isAttr)
	{
		//Lookup attribute
		const XMLVector& attributes=getAttributesByMultiname(name,normalizedName);
		return asAtom::fromObject(XMLList::create(getSystemState(),attributes,attributelist,name));
	}
	else if(XML::isValidMultiname(getSystemState(),name,index))
	{
		// If the multiname is a valid array property, the XML
		// object is treated as a single-item XMLList.
		if(index==0)
		{
			return asAtom::fromObject(this);
		}
		else
			return asAtom::fromObject(getSystemState()->getUndefinedRef());
	}
	else
	{
		if (normalizedName == "*")
		{
			XMLVector ret;
			childrenImpl(ret, "*");
			multiname mname(NULL);
			mname.name_s_id=getSystemState()->getUniqueStringId("*");
			mname.name_type=multiname::NAME_STRING;
			mname.ns.emplace_back(getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
			XMLList* retObj=XMLList::create(getSystemState(),ret,this->getChildrenlist(),mname);
			return asAtom::fromObject(retObj);
		}
		else
		{
			const XMLVector& ret=getValuesByMultiname(childrenlist,name);
			if(ret.empty() && (opt & XML_STRICT)!=0)
				return asAtom::invalidAtom;
			
			asAtom ch =asAtom::fromObject(XMLList::create(getSystemState(),ret,this->getChildrenlist(),name));
			return ch;
		}
	}
}

void XML::setVariableByMultiname(const multiname& name, asAtom& o, CONST_ALLOWED_FLAG allowConst)
{
	unsigned int index=0;
	bool isAttr=name.isAttribute;
	//Normalize the name to the string form
	const tiny_string normalizedName=name.normalizedName(getSystemState());

	//Only the first namespace is used, is this right?
	uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
	uint32_t ns_prefix = BUILTIN_STRINGS::EMPTY;
	if(name.ns.size() > 0 && !name.hasEmptyNS)
	{
		if (name.ns[0].kind==NAMESPACE)
		{
			ns_uri=name.ns[0].nsNameId;
			ns_prefix=getNamespacePrefixByURI(ns_uri);
		}
	}

	// namespace set by "default xml namespace = ..."
	if (ns_uri == BUILTIN_STRINGS::EMPTY && ns_prefix == BUILTIN_STRINGS::EMPTY)
	{
		ns_uri = getVm(getSystemState())->getDefaultXMLNamespaceID();
	}

	const char *buf=normalizedName.raw_buf();
	if(!normalizedName.empty() && normalizedName.charAt(0)=='@')
	{
		isAttr=true;
		buf+=1;
	}
	if (childrenlist.isNull())
		childrenlist = _MR(Class<XMLList>::getInstanceSNoArgs(getSystemState()));
	
	if(isAttr)
	{
		tiny_string nodeval;
		if(o.getObject() && o.getObject()->is<XMLList>())
		{
			_NR<XMLList> x = _NR<XMLList>(o.getObject()->as<XMLList>());
			for (auto it2 = x->nodes.begin(); it2 != x->nodes.end(); it2++)
			{
				if (nodeval != "")
					nodeval += " ";
				nodeval += (*it2)->toString();
			}
		}
		else
		{
			nodeval = o->toString();
		}
		_NR<XML> a;
		XMLList::XMLListVector::iterator it = attributelist->nodes.begin();
		while (it != attributelist->nodes.end())
		{
			_R<XML> attr = *it;
			XMLList::XMLListVector::iterator ittmp = it;
			it++;
			if (attr->nodenamespace_uri == ns_uri && (attr->nodename == buf || (*buf=='*')|| (*buf==0)))
			{
				if (!a.isNull())
					it=attributelist->nodes.erase(ittmp);
				a = *ittmp;
				a->nodevalue = nodeval;
			}
		}
		if (a.isNull() && !((*buf=='*')|| (*buf==0)))
		{
			_NR<XML> tmp = _MR<XML>(Class<XML>::getInstanceSNoArgs(getSystemState()));
			tmp->parentNode = _IMR<XML>(this);
			tmp->nodetype = pugi::node_null;
			tmp->isAttribute = true;
			tmp->nodename = buf;
			tmp->nodenamespace_uri = ns_uri;
			tmp->nodenamespace_prefix = ns_prefix;
			tmp->nodevalue = nodeval;
			tmp->constructed = true;
			attributelist->nodes.push_back(tmp);
		}
	}
	else if(XML::isValidMultiname(getSystemState(),name,index))
	{
		childrenlist->setVariableByMultiname(name,o,allowConst);
	}
	else
	{
		bool found = false;
		XMLVector tmpnodes;
		while (!childrenlist->nodes.empty())
		{
			_R<XML> tmpnode = childrenlist->nodes.back();
			if (tmpnode->nodenamespace_uri == ns_uri && tmpnode->nodename == normalizedName)
			{
				if(o.getObject() && o.getObject()->is<XMLList>())
				{
					if (!found)
					{
						_NR<XMLList> x = _NR<XMLList>(Class<XMLList>::getInstanceS(getSystemState(),o.getObject()->as<XMLList>()->toXMLString_internal(false)));
						tmpnodes.insert(tmpnodes.end(), x->nodes.rbegin(),x->nodes.rend());
					}
				}
				else if(o.getObject() && o.getObject()->is<XML>())
				{
					if (o.getObject()->as<XML>()->getNodeKind() == pugi::node_pcdata)
					{
						_R<XML> tmp = _IMR<XML>(Class<XML>::getInstanceSNoArgs(getSystemState()));
						tmp->parentNode = tmpnode;
						tmp->nodetype = pugi::node_pcdata;
						tmp->nodename = "text";
						tmp->nodenamespace_uri = BUILTIN_STRINGS::EMPTY;
						tmp->nodenamespace_prefix = BUILTIN_STRINGS::EMPTY;
						tmp->nodevalue = o->toString();
						tmp->constructed = true;
						tmpnode->childrenlist->clear();
						tmpnode->childrenlist->append(tmp);
						if (!found)
							tmpnodes.push_back(tmpnode);
					}
					else
					{
						_NR<XML> tmp = _MR<XML>(o.getObject()->as<XML>());
						tmp->parentNode = _MR<XML>(this);
						if (!found)
							tmpnodes.push_back(tmp);
					}
					
				}
				else
				{
					if (tmpnode->childrenlist.isNull())
						tmpnode->childrenlist = _MR(Class<XMLList>::getInstanceSNoArgs(getSystemState()));
					
					if (tmpnode->childrenlist->nodes.size() == 1 && tmpnode->childrenlist->nodes[0]->nodetype == pugi::node_pcdata)
						tmpnode->childrenlist->nodes[0]->nodevalue = o->toString();
					else
					{
						XML* newnode = createFromString(this->getSystemState(),o->toString());
						tmpnode->childrenlist->clear();
						asAtom v = asAtom::fromObject(newnode);
						tmpnode->setVariableByMultiname(name,v,allowConst);
					}
					if (!found)
					{
						tmpnodes.push_back(tmpnode);
					}
				}
				found = true;
			}
			else
			{
				tmpnodes.push_back(tmpnode);
			}
			childrenlist->nodes.pop_back();
		}
		if (!found)
		{
			if(o.getObject() && o.getObject()->is<XML>())
			{
				_R<XML> tmp = _MR<XML>(o.getObject()->as<XML>());
				tmp->parentNode = _MR<XML>(this);
				tmpnodes.insert(tmpnodes.begin(),tmp);
			}
			else
			{
				tiny_string tmpstr = "<";
				if (this->nodenamespace_prefix!=BUILTIN_STRINGS::EMPTY)
				{
					tmpstr += ns_prefix;
					tmpstr += ":";
				}
				tmpstr += normalizedName;
				if (ns_prefix!=BUILTIN_STRINGS::EMPTY && ns_uri!= BUILTIN_STRINGS::EMPTY)
				{
					tmpstr += " xmlns:";
					tmpstr += getSystemState()->getStringFromUniqueId(ns_prefix);
					tmpstr += "=\"";
					tmpstr += getSystemState()->getStringFromUniqueId(ns_uri);
					tmpstr += "\"";
				}
				tmpstr +=">";
				tmpstr += encodeToXML(o->toString(),false);
				tmpstr +="</";
				if (ns_prefix != BUILTIN_STRINGS::EMPTY)
				{
					tmpstr += getSystemState()->getStringFromUniqueId(ns_prefix);
					tmpstr += ":";
				}
				tmpstr += normalizedName;
				tmpstr +=">";
				_NR<XML> tmp = _MR<XML>(createFromString(this->getSystemState(),tmpstr));
				tmp->parentNode = _IMR<XML>(this);
				tmpnodes.push_back(tmp);
			}
		}
		childrenlist->nodes.insert(childrenlist->nodes.begin(), tmpnodes.rbegin(),tmpnodes.rend());
	}
}

bool XML::hasPropertyByMultiname(const multiname& name, bool considerDynamic, bool considerPrototype)
{
	if(considerDynamic == false)
		return ASObject::hasPropertyByMultiname(name, considerDynamic, considerPrototype);
	if (!isConstructed())
		return false;

	//Only the first namespace is used, is this right?
	uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
	uint32_t ns_prefix = BUILTIN_STRINGS::EMPTY;
	if(name.ns.size() > 0 && !name.hasEmptyNS)
	{
		//assert_and_throw(name.ns[0].kind==NAMESPACE);
		ns_uri=name.ns[0].nsNameId;
		ns_prefix=getNamespacePrefixByURI(ns_uri);
	}

	// namespace set by "default xml namespace = ..."
	if (ns_uri==BUILTIN_STRINGS::EMPTY && ns_prefix==BUILTIN_STRINGS::EMPTY)
	{
		ns_uri = getVm(getSystemState())->getDefaultXMLNamespaceID();
	}

	bool isAttr=name.isAttribute;
	unsigned int index=0;
	const tiny_string normalizedName=name.normalizedName(getSystemState());
	const char *buf=normalizedName.raw_buf();
	if(!normalizedName.empty() && normalizedName.charAt(0)=='@')
	{
		isAttr=true;
		buf+=1;
	}
	if(isAttr)
	{
		//Lookup attribute
		if (!attributelist.isNull())
		{
			for (XMLList::XMLListVector::const_iterator it = attributelist->nodes.begin(); it != attributelist->nodes.end(); it++)
			{
				_R<XML> attr = *it;
				if (attr->nodenamespace_uri == ns_uri && attr->nodename == buf)
					return true;
			}
		}
	}
	else if(XML::isValidMultiname(getSystemState(),name,index))
	{
		// If the multiname is a valid array property, the XML
		// object is treated as a single-item XMLList.
		return(index==0);
	}
	else
	{
		//Lookup children
		for (uint32_t i = 0; i < childrenlist->nodes.size(); i++)
		{
			_R<XML> child= childrenlist->nodes[i];
			bool name_match=(child->nodename == buf);
			bool ns_match=ns_uri==BUILTIN_STRINGS::EMPTY || 
				(child->nodenamespace_uri == ns_uri);
			if(name_match && ns_match)
				return true;
		}
	}

	//Try the normal path as the last resource
	return ASObject::hasPropertyByMultiname(name, considerDynamic, considerPrototype);
}
bool XML::deleteVariableByMultiname(const multiname& name)
{
	unsigned int index=0;
	if(name.isAttribute)
	{
		//Only the first namespace is used, is this right?
		uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
		uint32_t ns_prefix = BUILTIN_STRINGS::EMPTY;
		if(name.ns.size() > 0 && !name.hasEmptyNS)
		{
			assert_and_throw(name.ns[0].kind==NAMESPACE);
			ns_uri=name.ns[0].nsNameId;
			ns_prefix=getNamespacePrefixByURI(ns_uri);
		}
		if (ns_uri == BUILTIN_STRINGS::EMPTY && ns_prefix == BUILTIN_STRINGS::EMPTY)
		{
			ns_uri = getVm(getSystemState())->getDefaultXMLNamespaceID();
		}
		if (!attributelist.isNull() && attributelist->nodes.size() > 0)
		{
			XMLList::XMLListVector::iterator it = attributelist->nodes.end();
			while (it != attributelist->nodes.begin())
			{
				it--;
				_R<XML> attr = *it;
				if ((ns_uri==BUILTIN_STRINGS::EMPTY && name.normalizedName(getSystemState()) == "") ||
						(ns_uri==BUILTIN_STRINGS::EMPTY && name.normalizedName(getSystemState()) == attr->nodename) ||
						(attr->nodenamespace_uri == ns_uri && name.normalizedName(getSystemState()) == "") ||
						(attr->nodenamespace_uri == ns_uri && attr->nodename == name.normalizedName(getSystemState())))
				{
					attributelist->nodes.erase(it);
				}
			}
		}
	}
	else if(XML::isValidMultiname(getSystemState(),name,index))
	{
		if (!childrenlist.isNull())
			childrenlist->nodes.erase(childrenlist->nodes.begin() + index);
	}
	else
	{
		//Only the first namespace is used, is this right?
		uint32_t ns_uri = BUILTIN_STRINGS::EMPTY;
		if(name.ns.size() > 0 && !name.hasEmptyNS)
		{
			assert_and_throw(name.ns[0].kind==NAMESPACE);
			ns_uri=name.ns[0].nsNameId;
		}
		if (!childrenlist.isNull() && childrenlist->nodes.size() > 0)
		{
			XMLList::XMLListVector::iterator it = childrenlist->nodes.end();
			while (it != childrenlist->nodes.begin())
			{
				it--;
				_R<XML> node = *it;
				if ((ns_uri==BUILTIN_STRINGS::EMPTY && name.normalizedName(getSystemState()) == "") ||
						(ns_uri==BUILTIN_STRINGS::EMPTY && name.normalizedName(getSystemState()) == node->nodename) ||
						(node->nodenamespace_uri == ns_uri && name.normalizedName(getSystemState()) == "") ||
						(node->nodenamespace_uri == ns_uri && node->nodename == name.normalizedName(getSystemState())))
				{
					childrenlist->nodes.erase(it);
				}
			}
		}
	}
	return true;
}
bool XML::isValidMultiname(SystemState* sys,const multiname& name, uint32_t& index)
{
	//First of all the multiname has to contain the null namespace
	//As the namespace vector is sorted, we check only the first one
	if(name.ns.size()!=0 && !name.hasEmptyNS)
		return false;

	if (name.isEmpty())
		return false;
	bool validIndex=name.toUInt(sys,index, true);
	// Don't throw for non-numeric NAME_STRING or NAME_OBJECT
	// because they can still be valid built-in property names.
	if(!validIndex && (name.name_type==multiname::NAME_INT || name.name_type == multiname::NAME_INT ||name.name_type==multiname::NAME_NUMBER))
		throwError<RangeError>(kOutOfRangeError, name.normalizedNameUnresolved(sys), "?");

	return validIndex;
}

uint32_t XML::getNamespacePrefixByURI(uint32_t uri, bool create)
{
	uint32_t prefix = BUILTIN_STRINGS::EMPTY;
	bool found=false;


	XML* tmp = this;
	while(tmp && tmp->is<XML>())
	{
		if(tmp->nodenamespace_uri==uri)
		{
			prefix=tmp->nodenamespace_prefix;
			found = true;
			break;
		}
		if (!tmp->parentNode)
			break;
		tmp = tmp->parentNode.getPtr();
	}

	if(!found && create)
	{
		nodenamespace_uri = uri;
	}

	return prefix;
}

ASFUNCTIONBODY(XML,_toString)
{
	XML* th=Class<XML>::cast(obj);
	return abstract_s(obj->getSystemState(),th->toString_priv());
}

ASFUNCTIONBODY(XML,_getIgnoreComments)
{
	return abstract_b(getSys(),ignoreComments);
}
ASFUNCTIONBODY(XML,_setIgnoreComments)
{
	assert(args && argslen==1);
	ignoreComments = Boolean_concrete(args[0]);
	return NULL;
}
ASFUNCTIONBODY(XML,_getIgnoreProcessingInstructions)
{
	return abstract_b(getSys(),ignoreProcessingInstructions);
}
ASFUNCTIONBODY(XML,_setIgnoreProcessingInstructions)
{
	assert(args && argslen==1);
	ignoreProcessingInstructions = Boolean_concrete(args[0]);
	return NULL;
}
ASFUNCTIONBODY(XML,_getIgnoreWhitespace)
{
	return abstract_b(getSys(),ignoreWhitespace);
}
ASFUNCTIONBODY(XML,_setIgnoreWhitespace)
{
	assert(args && argslen==1);
	ignoreWhitespace = Boolean_concrete(args[0]);
	return NULL;
}
ASFUNCTIONBODY(XML,_getPrettyIndent)
{
	return abstract_i(obj->getSystemState(),prettyIndent);
}
ASFUNCTIONBODY(XML,_setPrettyIndent)
{
	assert(args && argslen==1);
	prettyIndent = args[0]->toInt();
	return NULL;
}
ASFUNCTIONBODY(XML,_getPrettyPrinting)
{
	return abstract_b(getSys(),prettyPrinting);
}
ASFUNCTIONBODY(XML,_setPrettyPrinting)
{
	assert(args && argslen==1);
	prettyPrinting = Boolean_concrete(args[0]);
	return NULL;
}
ASFUNCTIONBODY(XML,_getSettings)
{
	ASObject* res = Class<ASObject>::getInstanceS(getSys());
	multiname mn(NULL);
	mn.name_type=multiname::NAME_STRING;
	mn.ns.emplace_back(res->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	mn.ns.emplace_back(res->getSystemState(),BUILTIN_STRINGS::STRING_AS3NS,NAMESPACE);
	mn.isAttribute = true;

	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreComments");
	asAtom v;
	v = asAtom(ignoreComments);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreProcessingInstructions");
	v = asAtom(ignoreProcessingInstructions);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreWhitespace");
	v = asAtom(ignoreWhitespace);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("prettyIndent");
	v = asAtom(prettyIndent);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("prettyPrinting");
	v = asAtom(prettyPrinting);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	return res;
}
ASFUNCTIONBODY(XML,_setSettings)
{
	if (argslen == 0)
	{
		setDefaultXMLSettings();
		return getSys()->getNullRef();
	}
	_NR<ASObject> arg0;
	ARG_UNPACK(arg0);
	if (arg0->is<Null>() || arg0->is<Undefined>())
	{
		setDefaultXMLSettings();
		return arg0->getSystemState()->getNullRef();
	}
	multiname mn(NULL);
	mn.name_type=multiname::NAME_STRING;
	mn.ns.emplace_back(arg0->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	mn.ns.emplace_back(arg0->getSystemState(),BUILTIN_STRINGS::STRING_AS3NS,NAMESPACE);
	mn.isAttribute = true;
	asAtom o;

	mn.name_s_id=getSys()->getUniqueStringId("ignoreComments");
	if (arg0->hasPropertyByMultiname(mn,true,true))
	{
		o=arg0->getVariableByMultiname(mn,SKIP_IMPL);
		ignoreComments = o->toInt();
	}

	mn.name_s_id=getSys()->getUniqueStringId("ignoreProcessingInstructions");
	if (arg0->hasPropertyByMultiname(mn,true,true))
	{
		o=arg0->getVariableByMultiname(mn,SKIP_IMPL);
		ignoreProcessingInstructions = o->toInt();
	}

	mn.name_s_id=getSys()->getUniqueStringId("ignoreWhitespace");
	if (arg0->hasPropertyByMultiname(mn,true,true))
	{
		o=arg0->getVariableByMultiname(mn,SKIP_IMPL);
		ignoreWhitespace = o->toInt();
	}

	mn.name_s_id=getSys()->getUniqueStringId("prettyIndent");
	if (arg0->hasPropertyByMultiname(mn,true,true))
	{
		o=arg0->getVariableByMultiname(mn,SKIP_IMPL);
		prettyIndent = o->toInt();
	}

	mn.name_s_id=getSys()->getUniqueStringId("prettyPrinting");
	if (arg0->hasPropertyByMultiname(mn,true,true))
	{
		o=arg0->getVariableByMultiname(mn,SKIP_IMPL);
		prettyPrinting = o->toInt();
	}
	return getSys()->getNullRef();
}
ASFUNCTIONBODY(XML,_getDefaultSettings)
{
	ASObject* res = Class<ASObject>::getInstanceS(getSys());
	multiname mn(NULL);
	mn.name_type=multiname::NAME_STRING;
	mn.ns.emplace_back(res->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
	mn.ns.emplace_back(res->getSystemState(),BUILTIN_STRINGS::STRING_AS3NS,NAMESPACE);
	mn.isAttribute = true;

	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreComments");
	res->setVariableByMultiname(mn,asAtom::trueAtomR,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreProcessingInstructions");
	res->setVariableByMultiname(mn,asAtom::trueAtomR,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("ignoreWhitespace");
	res->setVariableByMultiname(mn,asAtom::trueAtomR,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("prettyIndent");
	asAtom v = asAtom((int32_t)2);
	res->setVariableByMultiname(mn,v,CONST_NOT_ALLOWED);
	mn.name_s_id=res->getSystemState()->getUniqueStringId("prettyPrinting");
	res->setVariableByMultiname(mn,asAtom::trueAtomR,CONST_NOT_ALLOWED);
	return res;
}
ASFUNCTIONBODY(XML,_toJSON)
{
	return abstract_s(obj->getSystemState(),"XML");
}

void XML::CheckCyclicReference(XML* node)
{
	XML* tmp = node;
	if (tmp == this)
		throwError<TypeError>(kXMLIllegalCyclicalLoop);
	if (!childrenlist.isNull())
	{
		for (auto it = tmp->childrenlist->nodes.begin(); it != tmp->childrenlist->nodes.end(); it++)
		{
			if ((*it).getPtr() == this)
				throwError<TypeError>(kXMLIllegalCyclicalLoop);
			CheckCyclicReference((*it).getPtr());
		}
	}
}

XML *XML::createFromString(SystemState* sys, const tiny_string &s)
{
	XML* res = Class<XML>::getInstanceSNoArgs(sys);
	res->createTree(res->buildFromString(s, getParseMode()),false);
	return res;
}

XML *XML::createFromNode(const pugi::xml_node &_n, XML *parent, bool fromXMLList)
{
	XML* res = Class<XML>::getInstanceSNoArgs(parent ? parent->getSystemState() : getSys());
	if (parent)
	{
		res->parentNode = _IMNR<XML>(parent);
	}
	res->createTree(_n,fromXMLList);
	return res;
}

_NR<XMLList> XML::getChildrenlist() { return childrenlist ? childrenlist : NullRef; }

ASFUNCTIONBODY_ATOM(XML,insertChildAfter)
{
	XML* th=obj->as<XML>();
	_NR<ASObject> child1;
	_NR<ASObject> child2;
	ARG_UNPACK_ATOM(child1)(child2);
	if (th->nodetype != pugi::node_element)
		return asAtom::fromObject(sys->getUndefinedRef());
	
	if (child2->is<XML>())
		th->CheckCyclicReference(child2->as<XML>());
	else if (child2->is<XMLList>())
	{
		for (auto it = child2->as<XMLList>()->nodes.begin(); it < child2->as<XMLList>()->nodes.end(); it++)
		{
			th->CheckCyclicReference((*it).getPtr());
		}
	}
	else
		child2 = _NR<XML>(createFromString(sys,child2->toString()));
	if (th->childrenlist.isNull())
		th->childrenlist = _MR(Class<XMLList>::getInstanceSNoArgs(sys));
	if (child1->is<Null>())
	{
		if (child2->is<XML>())
		{
			child2->as<XML>()->parentNode = _IMNR<XML>(th);
			th->childrenlist->nodes.insert(th->childrenlist->nodes.begin(),_IMNR<XML>(child2->as<XML>()));
		}
		else if (child2->is<XMLList>())
		{
			for (auto it2 = child2->as<XMLList>()->nodes.begin(); it2 < child2->as<XMLList>()->nodes.end(); it2++)
			{
				(*it2)->incRef();
				(*it2)->parentNode = _IMNR<XML>(th);
			}
			th->childrenlist->nodes.insert(th->childrenlist->nodes.begin(),child2->as<XMLList>()->nodes.begin(), child2->as<XMLList>()->nodes.end());
		}
		return asAtom::fromObject(th);
	}
	if (child1->is<XMLList>())
	{
		if (child1->as<XMLList>()->nodes.size()==0)
			return asAtom::fromObject(sys->getUndefinedRef());
		child1 = child1->as<XMLList>()->nodes[0];
	}
	for (auto it = th->childrenlist->nodes.begin(); it != th->childrenlist->nodes.end(); it++)
	{
		if ((*it).getPtr() == child1.getPtr())
		{
			if (child2->is<XML>())
			{
				child2->as<XML>()->parentNode = _IMNR<XML>(th);
				th->childrenlist->nodes.insert(it+1,_IMNR<XML>(child2->as<XML>()));
			}
			else if (child2->is<XMLList>())
			{
				for (auto it2 = child2->as<XMLList>()->nodes.begin(); it2 < child2->as<XMLList>()->nodes.end(); it2++)
				{
					(*it2)->incRef();
					(*it2)->parentNode = _IMNR<XML>(th);
				}
				th->childrenlist->nodes.insert(it+1,child2->as<XMLList>()->nodes.begin(), child2->as<XMLList>()->nodes.end());
			}
			return asAtom::fromObject(th);
		}
	}
	return asAtom::fromObject(sys->getUndefinedRef());
}
ASFUNCTIONBODY_ATOM(XML,insertChildBefore)
{
	XML* th=obj->as<XML>();
	_NR<ASObject> child1;
	_NR<ASObject> child2;
	ARG_UNPACK_ATOM(child1)(child2);
	if (th->nodetype != pugi::node_element)
		return asAtom::fromObject(sys->getUndefinedRef());
	
	if (child2->is<XML>())
		th->CheckCyclicReference(child2->as<XML>());
	else if (child2->is<XMLList>())
	{
		for (auto it = child2->as<XMLList>()->nodes.begin(); it < child2->as<XMLList>()->nodes.end(); it++)
		{
			th->CheckCyclicReference((*it).getPtr());
		}
	}
	else
		child2 = _NR<XML>(createFromString(sys,child2->toString()));

	if (th->childrenlist.isNull())
		th->childrenlist = _MR(Class<XMLList>::getInstanceSNoArgs(sys));
	if (child1->is<Null>())
	{
		if (child2->is<XML>())
		{
			th->appendChild(_NR<XML>(child2->as<XML>()));
		}
		else if (child2->is<XMLList>())
		{
			for (auto it = child2->as<XMLList>()->nodes.begin(); it < child2->as<XMLList>()->nodes.end(); it++)
			{
				(*it)->incRef();
				(*it)->parentNode = _IMNR<XML>(th);
				th->childrenlist->nodes.push_back(_NR<XML>(*it));
			}
		}
		return asAtom::fromObject(th);
	}
	if (child1->is<XMLList>())
	{
		if (child1->as<XMLList>()->nodes.size()==0)
			return asAtom::fromObject(sys->getUndefinedRef());
		child1 = child1->as<XMLList>()->nodes[0];
	}
	for (auto it = th->childrenlist->nodes.begin(); it != th->childrenlist->nodes.end(); it++)
	{
		if ((*it).getPtr() == child1.getPtr())
		{
			if (child2->is<XML>())
			{
				child2->as<XML>()->parentNode = _IMNR<XML>(th);
				th->childrenlist->nodes.insert(it,_IMNR<XML>(child2->as<XML>()));
			}
			else if (child2->is<XMLList>())
			{
				for (auto it2 = child2->as<XMLList>()->nodes.begin(); it2 < child2->as<XMLList>()->nodes.end(); it2++)
				{
					(*it2)->incRef();
					(*it2)->parentNode = _IMNR<XML>(th);
				}
				th->childrenlist->nodes.insert(it,child2->as<XMLList>()->nodes.begin(), child2->as<XMLList>()->nodes.end());
			}
			return asAtom::fromObject(th);
		}
	}
	return asAtom::fromObject(sys->getUndefinedRef());
}

ASFUNCTIONBODY(XML,namespaceDeclarations)
{
	XML *th = obj->as<XML>();
	Array *namespaces = Class<Array>::getInstanceSNoArgs(obj->getSystemState());
	for (uint32_t i = 0; i < th->namespacedefs.size(); i++)
	{
		_R<Namespace> tmpns = th->namespacedefs[i];
		bool b;
		if (tmpns->getPrefix(b) != BUILTIN_STRINGS::EMPTY)
		{
			asAtom element = asAtom::fromObject(tmpns.getPtr());
			namespaces->push(element);
		}
	}
	return namespaces;
}

ASFUNCTIONBODY_ATOM(XML,removeNamespace)
{
	XML *th = obj->as<XML>();
	_NR<ASObject> arg1;
	ARG_UNPACK_ATOM(arg1);
	Namespace* ns;
	if (arg1->is<Namespace>())
		ns = arg1->as<Namespace>();
	else
		ns = Class<Namespace>::getInstanceSRaw(sys,arg1->toStringId(), BUILTIN_STRINGS::EMPTY);

	th->RemoveNamespace(ns);
	return asAtom::fromObject(th);
}
void XML::RemoveNamespace(Namespace *ns)
{
	if (this->nodenamespace_uri == ns->getURI())
	{
		this->nodenamespace_uri = BUILTIN_STRINGS::EMPTY;
		this->nodenamespace_prefix = BUILTIN_STRINGS::EMPTY;
	}
	for (auto it = namespacedefs.begin(); it !=  namespacedefs.end(); it++)
	{
		_R<Namespace> tmpns = *it;
		if (tmpns->getURI() == ns->getURI())
		{
			namespacedefs.erase(it);
			break;
		}
	}
	if (childrenlist)
	{
		for (auto it = childrenlist->nodes.begin(); it != childrenlist->nodes.end(); it++)
		{
			(*it)->RemoveNamespace(ns);
		}
	}
}

ASFUNCTIONBODY(XML,comments)
{
	XML* th=Class<XML>::cast(obj);
	tiny_string name;
	ARG_UNPACK(name,"*");
	XMLVector ret;
	th->getComments(ret);
	return XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),multiname(NULL));
}
void XML::getComments(XMLVector& ret)
{
	if (childrenlist)
	{
		for (auto it = childrenlist->nodes.begin(); it != childrenlist->nodes.end(); it++)
		{
			if ((*it)->getNodeKind() == pugi::node_comment)
			{
				ret.push_back(*it);
			}
		}
	}
}

ASFUNCTIONBODY(XML,processingInstructions)
{
	XML* th=Class<XML>::cast(obj);
	tiny_string name;
	ARG_UNPACK(name,"*");
	XMLVector ret;
	th->getprocessingInstructions(ret,name);
	return XMLList::create(obj->getSystemState(),ret,th->getChildrenlist(),multiname(NULL));
}
void XML::getprocessingInstructions(XMLVector& ret, tiny_string name)
{
	if (childrenlist)
	{
		for (auto it = childrenlist->nodes.begin(); it != childrenlist->nodes.end(); it++)
		{
			if ((*it)->getNodeKind() == pugi::node_pi && (name == "*" || name == (*it)->nodename))
			{
				ret.push_back(*it);
			}
		}
	}
}
ASFUNCTIONBODY(XML,_propertyIsEnumerable)
{
	return abstract_b(obj->getSystemState(),argslen == 1 && args[0]->toString() == "0" );
}
ASFUNCTIONBODY(XML,_hasOwnProperty)
{
	tiny_string prop;
	ARG_UNPACK(prop);

	bool ret = false;
	if (prop == "0")
		ret = true;
	else
	{
		multiname name(NULL);
		name.name_type=multiname::NAME_STRING;
		name.name_s_id=args[0]->toStringId();
		name.ns.emplace_back(obj->getSystemState(),BUILTIN_STRINGS::EMPTY,NAMESPACE);
		name.ns.emplace_back(obj->getSystemState(),BUILTIN_STRINGS::STRING_AS3NS,NAMESPACE);
		name.isAttribute=false;
		ret=obj->hasPropertyByMultiname(name, true, true);
	}
	return abstract_b(obj->getSystemState(),ret);
}

tiny_string XML::toString_priv()
{
	tiny_string ret;
	if (getNodeKind() == pugi::node_pcdata ||
		isAttribute ||
		getNodeKind() == pugi::node_cdata)
	{
		ret=nodevalue;
	}
	else if (getNodeKind() == pugi::node_comment ||
			 getNodeKind() == pugi::node_pi)
	{
		ret="";
	}
	else if (hasSimpleContent())
	{
		if (!childrenlist.isNull())
		{
			auto it = childrenlist->nodes.begin();
			while(it != childrenlist->nodes.end())
			{
				if ((*it)->getNodeKind() != pugi::node_comment &&
						(*it)->getNodeKind() != pugi::node_pi)
					ret += (*it)->toString_priv();
				it++;
			}
		}
	}
	else
	{
		ret=toXMLString_internal();
	}
	return ret;
}

bool XML::getPrettyPrinting()
{
	return prettyPrinting;
}

unsigned int XML::getParseMode()
{
	unsigned int parsemode = pugi::parse_cdata | pugi::parse_escapes|pugi::parse_fragment | pugi::parse_doctype |pugi::parse_pi|pugi::parse_declaration;
	if (!ignoreWhitespace) parsemode |= pugi::parse_ws_pcdata;
	//if (!ignoreProcessingInstructions) parsemode |= pugi::parse_pi|pugi::parse_declaration;
	if (!ignoreComments) parsemode |= pugi::parse_comments;
	return parsemode;
}

tiny_string XML::toString()
{
	return toString_priv();
}

int32_t XML::toInt()
{
	if (!hasSimpleContent())
		return 0;

	tiny_string str = toString_priv();
	return Integer::stringToASInteger(str.raw_buf(), 0);
}
int64_t XML::toInt64()
{
	if (!hasSimpleContent())
		return 0;

	tiny_string str = toString_priv();
	int64_t value;
	bool valid=Integer::fromStringFlashCompatible(str.raw_buf(), value, 0);
	if (!valid)
		return 0;
	return value;
}

number_t XML::toNumber()
{
	if (!hasSimpleContent())
		return 0;
	return parseNumber(toString_priv());
}

bool XML::nodesEqual(XML *a, XML *b) const
{
	assert(a && b);

	// type
	if(a->nodetype!=b->nodetype)
		return false;

	// name
	if(a->nodename!=b->nodename || 
	   (!a->nodename.empty() && 
	    a->nodenamespace_uri!=b->nodenamespace_uri))
		return false;

	// content
	if (a->nodevalue != b->nodevalue)
		return false;
	// attributes
	if (a->attributelist.isNull())
		return b->attributelist.isNull() || b->attributelist->nodes.size() == 0;
	if (b->attributelist.isNull())
		return a->attributelist.isNull() || a->attributelist->nodes.size() == 0;
	if (a->attributelist->nodes.size() != b->attributelist->nodes.size())
		return false;
	for (int i = 0; i < (int)a->attributelist->nodes.size(); i++)
	{
		_R<XML> oa= a->attributelist->nodes[i];
		bool bequal = false;
		for (int j = 0; j < (int)b->attributelist->nodes.size(); j++)
		{
			_R<XML> ob= b->attributelist->nodes[j];
			if (oa->isEqual(ob.getPtr()))
			{
				bequal = true;
				break;
			}
		}
		if (!bequal)
			return false;
	}
	if (!ignoreProcessingInstructions && (!a->procinstlist.isNull() || !b->procinstlist.isNull()))
	{
		if (a->procinstlist.isNull() || b->procinstlist.isNull())
			return false;
		if (a->procinstlist->nodes.size() != b->procinstlist->nodes.size())
			return false;
		for (int i = 0; i < (int)a->procinstlist->nodes.size(); i++)
		{
			_R<XML> oa= a->procinstlist->nodes[i];
			bool bequal = false;
			for (int j = 0; j < (int)b->procinstlist->nodes.size(); j++)
			{
				_R<XML> ob= b->procinstlist->nodes[j];
				if (oa->isEqual(ob.getPtr()))
				{
					bequal = true;
					break;
				}
			}
			if (!bequal)
				return false;
		}
	}
	
	// children
	if (a->childrenlist.isNull())
		return b->childrenlist.isNull() || b->childrenlist->nodes.size() == 0;
	if (b->childrenlist.isNull())
		return a->childrenlist.isNull() || a->childrenlist->nodes.size() == 0;
	
	return a->childrenlist->isEqual(b->childrenlist.getPtr());
}

uint32_t XML::nextNameIndex(uint32_t cur_index)
{
	if(cur_index < 1)
		return 1;
	else
		return 0;
}

asAtom XML::nextName(uint32_t index)
{
	if(index<=1)
		return asAtom(index-1);
	else
		throw RunTimeException("XML::nextName out of bounds");
}

asAtom XML::nextValue(uint32_t index)
{
	if(index<=1)
	{
		return asAtom::fromObject(this);
	}
	else
		throw RunTimeException("XML::nextValue out of bounds");
}

bool XML::isEqual(ASObject* r)
{
	if (!isConstructed())
		return !r->isConstructed() || r->getObjectType() == T_NULL || r->getObjectType() == T_UNDEFINED;
	if(r->is<XML>())
		return nodesEqual(this, r->as<XML>());

	if(r->is<XMLList>())
		return r->as<XMLList>()->isEqual(this);

	if(hasSimpleContent())
		return toString()==r->toString();

	return false;
}

void XML::serialize(ByteArray* out, std::map<tiny_string, uint32_t>& stringMap,
		    std::map<const ASObject*, uint32_t>& objMap,
		    std::map<const Class_base*, uint32_t>& traitsMap)
{
	if (out->getObjectEncoding() == ObjectEncoding::AMF0)
	{
		LOG(LOG_NOT_IMPLEMENTED,"serializing XML in AMF0 not implemented");
		return;
	}

	out->writeByte(xml_marker);
	out->writeXMLString(objMap, this, toString());
}

void XML::createTree(const pugi::xml_node& rootnode,bool fromXMLList)
{
	pugi::xml_node node = rootnode;
	bool done = false;
	this->childrenlist = _IMR(Class<XMLList>::getInstanceSNoArgs(getSystemState()));
	if (parentNode.isNull() && !fromXMLList)
	{
		while (true)
		{
			//LOG(LOG_INFO,"rootfill:"<<node.name()<<" "<<node.value()<<" "<<node.type()<<" "<<parentNode.isNull());
			switch (node.type())
			{
				case pugi::node_null: // Empty (null) node handle
					fillNode(this,node);
					done = true;
					break;
				case pugi::node_document:// A document tree's absolute root
					createTree(node.first_child(),fromXMLList);
					return;
				case pugi::node_pi:	// Processing instruction, i.e. '<?name?>'
				case pugi::node_declaration: // Document declaration, i.e. '<?xml version="1.0"?>'
				{
					_NR<XML> tmp = _MR<XML>(Class<XML>::getInstanceSNoArgs(getSystemState()));
					fillNode(tmp.getPtr(),node);
					if(this->procinstlist.isNull())
						this->procinstlist = _MR(Class<XMLList>::getInstanceSNoArgs(getSystemState()));
					this->procinstlist->incRef();
					this->procinstlist->append(_R<XML>(tmp));
					break;
				}
				case pugi::node_doctype:// Document type declaration, i.e. '<!DOCTYPE doc>'
					fillNode(this,node);
					break;
				case pugi::node_pcdata: // Plain character data, i.e. 'text'
				case pugi::node_cdata: // Character data, i.e. '<![CDATA[text]]>'
					fillNode(this,node);
					done = true;
					break;
				case pugi::node_comment: // Comment tag, i.e. '<!-- text -->'
					fillNode(this,node);
					break;
				case pugi::node_element: // Element tag, i.e. '<node/>'
				{
					fillNode(this,node);
					pugi::xml_node_iterator it=node.begin();
					while(it!=node.end())
					{
						//LOG(LOG_INFO,"rootchildnode1:"<<it->name()<<" "<<it->value()<<" "<<it->type()<<" "<<parentNode.isNull());
						_NR<XML> tmp = _MR<XML>(XML::createFromNode(*it,this));
						this->childrenlist->append(_R<XML>(tmp));
						it++;
					}
					done = true;
					break;
				}
				default:
					LOG(LOG_ERROR,"createTree:unhandled type:" <<node.type());
					done=true;
					break;
			}
			if (done)
				break;
			node = node.next_sibling();
		}
		//LOG(LOG_INFO,"constructed:"<<this->toXMLString_internal());
	}
	else
	{
		switch (node.type())
		{
			case pugi::node_pi:	// Processing instruction, i.e. '<?name?>'
			case pugi::node_declaration: // Document declaration, i.e. '<?xml version="1.0"?>'
			case pugi::node_doctype:// Document type declaration, i.e. '<!DOCTYPE doc>'
			case pugi::node_pcdata: // Plain character data, i.e. 'text'
			case pugi::node_cdata: // Character data, i.e. '<![CDATA[text]]>'
			case pugi::node_comment: // Comment tag, i.e. '<!-- text -->'
				fillNode(this,node);
				break;
			case pugi::node_element: // Element tag, i.e. '<node/>'
			{
				fillNode(this,node);
				pugi::xml_node_iterator it=node.begin();
				{
					while(it!=node.end())
					{
						_NR<XML> tmp = _MR<XML>(XML::createFromNode(*it,this));
						this->childrenlist->append(_R<XML>(tmp));
						it++;
					}
				}
				break;
			}
			default:
				LOG(LOG_ERROR,"createTree:subtree unhandled type:" <<node.type());
				break;
		}
	}
}

void XML::fillNode(XML* node, const pugi::xml_node &srcnode)
{
	if (node->childrenlist.isNull())
	{
		node->childrenlist = _IMR(Class<XMLList>::getInstanceSNoArgs(node->getSystemState()));
	}
	node->nodetype = srcnode.type();
	node->nodename = srcnode.name();
	node->nodevalue = srcnode.value();
	if (!node->parentNode.isNull() && node->parentNode->nodenamespace_prefix == BUILTIN_STRINGS::EMPTY)
		node->nodenamespace_uri = node->parentNode->nodenamespace_uri;
	else
		node->nodenamespace_uri = getVm(node->getSystemState())->getDefaultXMLNamespaceID();
	if (ignoreWhitespace && node->nodetype == pugi::node_pcdata)
		node->nodevalue = node->removeWhitespace(node->nodevalue);
	node->attributelist = _MR(Class<XMLList>::getInstanceSNoArgs(node->getSystemState()));
	pugi::xml_attribute_iterator itattr;
	for(itattr = srcnode.attributes_begin();itattr!=srcnode.attributes_end();++itattr)
	{
		tiny_string aname = tiny_string(itattr->name(),true);
		if(aname == "xmlns")
		{
			uint32_t uri = node->getSystemState()->getUniqueStringId(itattr->value());
			Ref<Namespace> ns = Class<Namespace>::getInstanceS(node->getSystemState(),uri,BUILTIN_STRINGS::EMPTY);
			node->namespacedefs.push_back(ns);
			node->nodenamespace_uri = uri;
		}
		else if (aname.numBytes() >= 6 && aname.substr_bytes(0,6) == "xmlns:")
		{
			uint32_t uri = node->getSystemState()->getUniqueStringId(itattr->value());
			tiny_string prefix = aname.substr(6,aname.end());
			Ref<Namespace> ns = Class<Namespace>::getInstanceS(node->getSystemState(),uri,node->getSystemState()->getUniqueStringId(prefix));
			node->namespacedefs.push_back(ns);
		}
	}
	uint32_t pos = node->nodename.find(":");
	if (pos != tiny_string::npos)
	{
		// nodename has namespace
		node->nodenamespace_prefix = node->getSystemState()->getUniqueStringId(node->nodename.substr(0,pos));
		node->nodename = node->nodename.substr(pos+1,node->nodename.end());
		if (node->nodenamespace_prefix == BUILTIN_STRINGS::STRING_XML)
			node->nodenamespace_uri = BUILTIN_STRINGS::STRING_NAMESPACENS;
		else
		{
			XML* tmpnode = node;
			bool found = false;
			while (tmpnode)
			{
				for (auto itns = tmpnode->namespacedefs.begin(); itns != tmpnode->namespacedefs.end();itns++)
				{
					bool undefined;
					if ((*itns)->getPrefix(undefined) == node->nodenamespace_prefix)
					{
						node->nodenamespace_uri = (*itns)->getURI();
						found = true;
						break;
					}
				}
				if (found)
					break;
				if (tmpnode->parentNode.isNull())
					break;
				tmpnode = tmpnode->parentNode.getPtr();
			}
		}
	}
	for(itattr = srcnode.attributes_begin();itattr!=srcnode.attributes_end();++itattr)
	{
		tiny_string aname = tiny_string(itattr->name(),true);
		if(aname == "xmlns" || (aname.numBytes() >= 6 && aname.substr_bytes(0,6) == "xmlns:"))
			continue;
		_NR<XML> tmp = _MR<XML>(Class<XML>::getInstanceSNoArgs(node->getSystemState()));
		tmp->parentNode = _IMR<XML>(node);
		tmp->nodetype = pugi::node_null;
		tmp->isAttribute = true;
		tmp->nodename = aname;
		tmp->nodenamespace_uri = getVm(node->getSystemState())->getDefaultXMLNamespaceID();
		pos = tmp->nodename.find(":");
		if (pos != tiny_string::npos)
		{
			tmp->nodenamespace_prefix = node->getSystemState()->getUniqueStringId(tmp->nodename.substr(0,pos));
			tmp->nodename = tmp->nodename.substr(pos+1,tmp->nodename.end());
			if (tmp->nodenamespace_prefix == BUILTIN_STRINGS::STRING_XML)
				tmp->nodenamespace_uri = BUILTIN_STRINGS::STRING_NAMESPACENS;
			else
			{
				XML* tmpnode = node;
				bool found = false;
				while (tmpnode)
				{
					for (auto itns = tmpnode->namespacedefs.begin(); itns != tmpnode->namespacedefs.end();itns++)
					{
						bool undefined;
						if ((*itns)->getPrefix(undefined) == tmp->nodenamespace_prefix)
						{
							tmp->nodenamespace_uri = (*itns)->getURI();
							found = true;
							break;
						}
					}
					if (found)
						break;
					if (tmpnode->parentNode.isNull())
						break;
					tmpnode = tmpnode->parentNode.getPtr();
				}
			}
		}
		tmp->nodevalue = itattr->value();
		tmp->constructed = true;
		node->attributelist->nodes.push_back(tmp);
	}
	node->constructed=true;
}

ASFUNCTIONBODY_ATOM(XML,_prependChild)
{
	XML* th=obj->as<XML>();
	assert_and_throw(argslen==1);
	_NR<XML> arg;
	if(args[0].getObject()->getClass()==Class<XML>::getClass(sys))
	{
		arg=_IMR(args[0]->as<XML>());
	}
	else if(args[0].getObject()->getClass()==Class<XMLList>::getClass(sys))
	{
		_NR<XMLList> list=_IMNR(args[0]->as<XMLList>());
		list->prependNodesTo(th);
		return asAtom::fromObject(th);
	}
	else
	{
		//The appendChild specs says that any other type is converted to string
		//NOTE: this is explicitly different from XML constructor, that will only convert to
		//string Numbers and Booleans
		arg=_MR(createFromString(sys,"dummy"));
		//avoid interpretation of the argument, just set it as text node
		arg->setTextContent(args[0]->toString());
	}

	th->prependChild(arg);
	return asAtom::fromObject(th);
}
void XML::prependChild(_R<XML> newChild)
{
	if (newChild->constructed)
	{
		if (this == newChild.getPtr())
			throwError<TypeError>(kXMLIllegalCyclicalLoop);
		_NR<XML> node = this->parentNode;
		while (!node.isNull())
		{
			if (node == newChild)
				throwError<TypeError>(kXMLIllegalCyclicalLoop);
			node = node->parentNode;
		}
		newChild->parentNode = _IMNR<XML>(this);
		childrenlist->prepend(newChild);
	}
}

ASFUNCTIONBODY_ATOM(XML,_replace)
{
	XML* th=obj->as<XML>();
	_NR<ASObject> propertyName;
	_NR<ASObject> value;
	ARG_UNPACK_ATOM(propertyName) (value);

	multiname name(NULL);
	name.name_type=multiname::NAME_STRING;
	if (propertyName->is<ASQName>())
	{
		name.name_s_id=propertyName->as<ASQName>()->getLocalName();
		name.ns.emplace_back(sys,propertyName->as<ASQName>()->getURI(),NAMESPACE);
	}
	else if (propertyName->toString() == "*")
	{
		if (value->is<XMLList>())
		{
			th->childrenlist = _IMNR<XMLList>(value->as<XMLList>());
		}
		else if (value->is<XML>())
		{
			th->childrenlist->clear();
			th->childrenlist->append(_IMR<XML>(value->as<XML>()));
		}
		else
		{
			XML* x = createFromString(sys,value->toString());
			th->childrenlist->clear();
			th->childrenlist->append(_IMR<XML>(x));
		}
		return asAtom::fromObject(th);
	}
	else
	{
		name.name_s_id=sys->getUniqueStringId(propertyName->toString());
		name.ns.emplace_back(sys,BUILTIN_STRINGS::EMPTY,NAMESPACE);
	}
	uint32_t index=0;
	asAtom v = asAtom::fromObject(value.getPtr());
	if(XML::isValidMultiname(sys,name,index))
	{
		th->childrenlist->setVariableByMultiname(name,v,CONST_NOT_ALLOWED);
	}	
	else if (th->hasPropertyByMultiname(name,true,false))
	{
		if (value->is<XMLList>())
		{
			th->deleteVariableByMultiname(name);
			th->setVariableByMultiname(name,v,CONST_NOT_ALLOWED);
		}
		else if (value->is<XML>())
		{
			th->deleteVariableByMultiname(name);
			th->setVariableByMultiname(name,v,CONST_NOT_ALLOWED);
		}
		else
		{
			XML* x = createFromString(sys,value->toString());
			th->deleteVariableByMultiname(name);
			v = asAtom::fromObject(x);
			th->setVariableByMultiname(name,v,CONST_NOT_ALLOWED);
		}
	}
	return asAtom::fromObject(th);
}
ASFUNCTIONBODY(XML,setNotification)
{
	LOG(LOG_NOT_IMPLEMENTED,"XML.setNotification does nothing");
	return NULL;
}
