/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                            *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "icsloader.h"

#include <thread>
#include <iostream>

#include <matrixutils.h>

namespace VParser {

struct VContext;

/**
 * The list of parameters associated with a property.
 *
 * It is a simple key=value pair delimited
 *
 *     key_name="value";
 *    \_______/|\_____/|
 *        |    |  |  +-- End delimited, either ';' or ':'
 *        |    |  +----- The parameter value with or without double quote
 *        |    +-------- The key to value separator
 *        +------------- The parameter name
 */
struct VParameters {
    enum class State {
        EMPTY      , /*!< Before anything happens   */
        NAME       , /*!< Parsing the name          */
        VALUE      , /*!< Parsing the value         */
        VALUE_QUOTE, /*!< Parsing the value with "  */
        DONE       , /*!< Success!                  */
        ERROR      , /*!< The encoding is corrupted */
        COUNT__
    };

    /// The classes of characters
    enum class Event {
        READ  , /*!< Anything but ";: */
        QUOTE , /*!<     "            */
        SPLIT , /*!<  = / ; / :       */
        COUNT__
    };

    /**
     * More pretty than a tuple, otherwise just a pair.
     */
    struct VParameter {
        std::basic_string<char> name ;
        std::basic_string<char> value;
    };

    explicit VParameters(VContext* context) : m_pContext(context) {}

    // Attributes
    VContext* m_pContext;
    std::list<VParameter> parameters;

    /**
     * Note that UTF chars can be over multiple bytes and this may explode with
     * fire until fixed.
     *
     * The current char is at position **1** ([0] being the previous one to
     * help detect escaping)
     */
    VParameters::Event charToEvent(const char content[4]);

    // Actions
    char increment();
    char saveName();
    char saveValue();
    char resetBuffer();
    char error();
    char nothing();

    typedef char (VParameters::*function)();
    State m_State {State::EMPTY};
    char applyEvent(const char content[4]);
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> m_StateMap;
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::function> m_StateFunctor;
};

/**
 * Map of the next state
 */
#define VPE VParameters::Event::
#define VPF  &VParameters::
static EnumClassReordering<VParameters::Event> paramrow =
{                                   VPE READ,        VPE QUOTE,      VPE SPLIT     };
const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> VParameters::m_StateMap = {
{State::EMPTY      , {{paramrow, {State::NAME , State::ERROR, State::ERROR  }}}},
{State::NAME       , {{paramrow, {State::NAME , State::NAME , State::VALUE  }}}},
{State::VALUE      , {{paramrow, {State::VALUE, State::VALUE, State::DONE   }}}},
{State::VALUE_QUOTE, {{paramrow, {State::VALUE, State::VALUE, State::DONE   }}}},
{State::DONE       , {{paramrow, {State::ERROR, State::ERROR, State::ERROR  }}}},
{State::ERROR      , {{paramrow, {State::ERROR, State::ERROR, State::ERROR  }}}}
};

const Matrix2D<VParameters::State, VParameters::Event, VParameters::function> VParameters::m_StateFunctor = {
{State::EMPTY      , {{paramrow, { VPF increment  , VPF increment  ,  VPF error     }}}},
{State::NAME       , {{paramrow, { VPF increment  , VPF increment  ,  VPF saveName  }}}},
{State::VALUE      , {{paramrow, { VPF increment  , VPF increment  ,  VPF saveValue }}}},
{State::VALUE_QUOTE, {{paramrow, { VPF increment  , VPF increment  ,  VPF saveValue }}}},
{State::DONE       , {{paramrow, { VPF error      , VPF error      ,  VPF nothing   }}}},
{State::ERROR      , {{paramrow, { VPF nothing    , VPF nothing    ,  VPF error     }}}},
};
#undef VPF
#undef VPE

VParameters::Event VParameters::charToEvent(const char content[4])
{
    if (content[0] == '\\')
        return Event::READ;

    switch(content[1]) {
        case ';':
        case ':':
        case '=':
            return Event::SPLIT;
        case '"':
            return Event::QUOTE;
    };

    return Event::READ;
}

/**
 * An object property.
 *
 * It can optionally contain parameters or be over multiple lines.
 *
 *         +-------------------------------------- Name
 *         |                +--------------------- Parameters
 *         |                |            +-------- Key/Value splitter
 *         |                |            |  +----- Content
 *         |                |            |  |  +-- Line break and/or carriage
 *     /-------\/-----------------------\|/---\|
 *     PROP_NAME;PARAM1="val";PARAM2=val2:line1
 *      line2
 *      line3
 *     |
 *     +---- One empty space
 *
 * It is important to note that a single UTF8 character can be spread across
 * 2 lines to honor the 75 character limit and support "dumb" encoders.
 *
 * The parser depends both on the past and future, for example:
 *
 *     MYPROPERTY;PARAM=foo\\:\n PARAM2=bar:value
 *
 * Has parameters over 2 lines while it has values over 2 lines if parsed
 * trivially. See RFC5545 section 3.3.11.
 */
struct VProperty
{
    /**
     *
     */
    enum class State {
        EMPTY       , /*!< The property is not parsed yet               */
        NAME        , /*!< The property name is being parsed            */
        PARAMETERS  , /*!< The parameters are being parsed              */
        VALUE       , /*!< The value is being parsed                    */
        LINE_BREAK_P, /*!< When it decides if it is done or not (param) */
        LINE_BREAK_V, /*!< When it decides if it is done or not (value) */
        DONE        , /*!< The value is parsed                          */
        ERROR       , /*!< Something went wrong                         */
        COUNT__
    };

    /**
     * The events that affect the parsing.
     */
    enum class Event {
        READ       , /*!<  */
        SPLIT_PARAM, /*!<  */
        SPLIT      , /*!<  */
        LINE_BREAK , /*!<  */
        ERROR      , /*!<  */
        COUNT__
    };

    explicit VProperty(VContext* context) : m_pContext(context), m_Parameters(context){}

    // Attributes
    VContext*  m_pContext;
    VParameters m_Parameters;

    /**
     *
     */
    Event charToEvent(const char c[4]);

    // Actions, return the number of bytes the cursor has to be incremented with
    char pushName();
    char increment();
    char parameter();
    char pushParameter();
    char pushProperty();
    char finishParameters();
    char pushLine();
    char saveName();
    char saveValue();
    char error();

    std::list<VParameters*> parameters;

    std::basic_string<char> m_Name;
    std::basic_string<char> m_Value;

    typedef char (VProperty::*function)();
    State m_State {State::EMPTY};
    char applyEvent(const char content[4]);
    static const Matrix2D<VProperty::State , VProperty::Event, VProperty::State> m_StateMap;
    static const Matrix2D<VProperty::State , VProperty::Event, VProperty::function> m_StateFunctor;
    static const Matrix1D<VParameters::State, VProperty::Event> m_ParamToEvent;
};

/**
 * Map of the next state
 */
#define VPE VProperty::Event::
#define VPF &VProperty::
static EnumClassReordering<VProperty::Event> proprow =
{                                    VPE READ,        VPE SPLIT_PARAM  ,   VPE SPLIT   ,     VPE LINE_BREAK,    VPE ERROR};
const Matrix2D<VProperty::State, VProperty::Event, VProperty::State> VProperty::m_StateMap = {
{State::EMPTY       , {{proprow, {State::NAME      , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR }}}},
{State::NAME        , {{proprow, {State::NAME      , State::PARAMETERS , State::VALUE  , State::ERROR        , State::ERROR }}}},
{State::PARAMETERS  , {{proprow, {State::PARAMETERS, State::PARAMETERS , State::VALUE  , State::LINE_BREAK_P , State::ERROR }}}},
{State::VALUE       , {{proprow, {State::VALUE     , State::VALUE      , State::DONE   , State::LINE_BREAK_V , State::ERROR }}}},
{State::LINE_BREAK_P, {{proprow, {State::PARAMETERS, State::PARAMETERS , State::ERROR  , State::ERROR        , State::ERROR }}}},
{State::LINE_BREAK_V, {{proprow, {State::VALUE     , State::VALUE      , State::ERROR  , State::ERROR        , State::ERROR }}}},
{State::DONE        , {{proprow, {State::ERROR     , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR }}}},
{State::ERROR       , {{proprow, {State::ERROR     , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR }}}}
};

const Matrix2D<VProperty::State, VProperty::Event, VProperty::function> VProperty::m_StateFunctor = {
{State::EMPTY       , {{proprow, { VPF increment ,  VPF error        , VPF error            , VPF error    , VPF error }}}},
{State::NAME        , {{proprow, { VPF increment ,  VPF pushName     , VPF pushName         , VPF error    , VPF error }}}},
{State::PARAMETERS  , {{proprow, { VPF parameter ,  VPF pushParameter, VPF finishParameters , VPF pushLine , VPF error }}}},
{State::VALUE       , {{proprow, { VPF increment ,  VPF increment    , VPF pushProperty     , VPF pushLine , VPF error }}}},
{State::LINE_BREAK_P, {{proprow, { VPF increment ,  VPF pushParameter, VPF error            , VPF error    , VPF error }}}},
{State::LINE_BREAK_V, {{proprow, { VPF increment ,  VPF increment    , VPF error            , VPF error    , VPF error }}}},
{State::DONE        , {{proprow, { VPF error     ,  VPF error        , VPF error            , VPF error    , VPF error }}}},
{State::ERROR       , {{proprow, { VPF error     ,  VPF error        , VPF error            , VPF error    , VPF error }}}},
};
#undef VPF
#undef VPE

const Matrix1D<VParameters::State, VProperty::Event> VProperty::m_ParamToEvent = {
    { VParameters::State::EMPTY      , VProperty::Event::READ  },
    { VParameters::State::NAME       , VProperty::Event::READ  },
    { VParameters::State::VALUE      , VProperty::Event::READ  },
    { VParameters::State::VALUE_QUOTE, VProperty::Event::READ  },
    { VParameters::State::DONE       , VProperty::Event::SPLIT },
    { VParameters::State::ERROR      , VProperty::Event::ERROR },
};

/**
 * It needs the next 3 bytes to properly detect line breaks on all OSes.
 */
VProperty::Event VProperty::charToEvent(const char c[4])
{
    // Check the simple case
    switch(c[1]) {
        case ':':
            return Event::SPLIT;
        case ';':
            return Event::SPLIT_PARAM;
    };

    // Detect all kind of "continue on the next line"
    if ((c[1] == '\n' || c[1] == '\r') && ((c[2] == '\r' && c[2] == ' ') || c[2] == ' '))
        return Event::LINE_BREAK;

    // If the above failed, then the property is completed
    if (c[1] == '\n' || c[1] == '\r')
        return Event::SPLIT;

    return Event::READ;
}

/**
 * Everything between a BEGIN:OBJECT_TYPE and it's matching END:OBJECT_TYPE.
 *
 * Note that objects can contain children objects of the same type, so a depth
 * counter is needed.
 */
struct VObject
{
    ICSLoader::ObjectType type;

    // Keep track of the parent so the global context can push/pop elements.
    VObject* m_pParent {nullptr};

    /**
     *
     */
    enum class State {
        EMPTY     , /*!< Nothing parsed yet              */
        HEADER    , /*!< BEGIN:OBJECT_TYPE               */ //FIXME remove (handled as a property)
        PROPERTIES, /*!< Normal key / value properties   */
        SUB_OBJECT, /*!< Objects within objects          */ //FIXME remove (moved to context)
        FOOTER    , /*!< END:OBJECT_TYPE                 */ //FIXME remove (handled as a property)
        DONE      , /*!< Once the footer is fully parsed */ //FIXME remove (no longer needed)
        ERROR     , /*!< Parsing failed                  */
        COUNT__
    };

    /**
     *
     */
    enum class Event {
        READ    , /*!<  */
        NEW_LINE, /*!<  */
        CHILD_FINISH, /*!< */
        ERROR,
        COUNT__
    };

    explicit VObject(VContext* context) : m_pContext(context) {}

    // Attributes
    VContext* m_pContext;
    VProperty* m_pCurrentProperty {nullptr};
    std::list<VProperty*> properties;
    std::list<VObject*> children;


    /**
     *
     */
    Event charToEvent(const char c[4]);

    static const Matrix1D<VProperty::State, VObject::Event> m_PropertyToEvent;

    char error();
    char increment();
    char selectObject();
    char childEvent();
    char propertyEvent();
    char pushProperty();
    char finish();
    char nothing();

    typedef char (VObject::*function)();
    State m_State {State::EMPTY};
    char applyEvent(const char content[4]);
    static const Matrix2D<VObject::State, VObject::Event, VObject::State> m_StateMap;
    static const Matrix2D<VObject::State, VObject::Event, VObject::function> m_StateFunctor;
};

#define VPE VObject::Event::
#define VPF &VObject::
static EnumClassReordering<VObject::Event> objrow =
{                                 VPE READ,     VPE NEW_LINE, VPE CHILD_FINISH, VPE ERROR   };
const Matrix2D<VObject::State, VObject::Event, VObject::State> VObject::m_StateMap = {
{State::EMPTY      , {{objrow, {State::HEADER     , State::ERROR     , State::ERROR      , State::ERROR }}}},
{State::HEADER     , {{objrow, {State::HEADER     , State::PROPERTIES, State::ERROR      , State::ERROR }}}},
{State::PROPERTIES , {{objrow, {State::PROPERTIES , State::PROPERTIES, State::PROPERTIES , State::ERROR }}}},
{State::SUB_OBJECT , {{objrow, {State::SUB_OBJECT , State::PROPERTIES, State::PROPERTIES , State::ERROR }}}},
{State::FOOTER     , {{objrow, {State::FOOTER     , State::DONE      , State::ERROR      , State::ERROR }}}},
{State::DONE       , {{objrow, {State::ERROR      , State::ERROR     , State::ERROR      , State::ERROR }}}},
{State::ERROR      , {{objrow, {State::ERROR      , State::ERROR     , State::ERROR      , State::ERROR }}}},
};

const Matrix2D<VObject::State, VObject::Event, VObject::function> VObject::m_StateFunctor = {
{State::EMPTY      , {{objrow, { VPF increment    , VPF nothing       , VPF error        , VPF error}}}},
{State::HEADER     , {{objrow, { VPF increment    , VPF selectObject  , VPF error        , VPF error}}}},
{State::PROPERTIES , {{objrow, { VPF propertyEvent, VPF propertyEvent , VPF pushProperty , VPF error}}}},
{State::SUB_OBJECT , {{objrow, { VPF childEvent   , VPF childEvent    , VPF nothing      , VPF error}}}},
{State::FOOTER     , {{objrow, { VPF increment    , VPF finish        , VPF error        , VPF error}}}},
{State::DONE       , {{objrow, { VPF error        , VPF error         , VPF error        , VPF error}}}},
{State::ERROR      , {{objrow, { VPF error        , VPF error         , VPF error        , VPF error}}}},
};
#undef VPF
#undef VPE

const Matrix1D<VProperty::State, VObject::Event> VObject::m_PropertyToEvent = {
    { VProperty::State::EMPTY       , VObject::Event::READ         },
    { VProperty::State::NAME        , VObject::Event::READ         },
    { VProperty::State::PARAMETERS  , VObject::Event::READ         },
    { VProperty::State::VALUE       , VObject::Event::READ         },
    { VProperty::State::LINE_BREAK_P, VObject::Event::READ         },
    { VProperty::State::LINE_BREAK_V, VObject::Event::READ         },
    { VProperty::State::DONE        , VObject::Event::CHILD_FINISH },
    { VProperty::State::ERROR       , VObject::Event::ERROR        },
};

VObject::Event VObject::charToEvent(const char content[4])
{
    if (content[0] == '\\')
        return Event::READ;

    switch(content[1]) {
        case '\n':
        case '\r':
        case '=':
            return Event::NEW_LINE;
    };

    return Event::READ;
}

struct VContext
{

    std::basic_string<char> m_Buffer;

    char* m_pContent { nullptr };
    int   m_Position {    0    };
    bool  m_IsActive {  true   };
    VObject* m_pCurrentObject {nullptr};

    char* currentWindow() {
        if (!m_Position)
            return new char[4] { //FIXME leak
                0x00,
                m_pContent[0],
                m_pContent[1],
                m_pContent[2]
            };

        return m_pContent + m_Position - 1;
    }

    char currentChar() {
        return currentWindow()[1];
    }

    void push(char count) {
        for (int i =0; i < count; i++) {
            m_Buffer.push_back(m_pContent[m_Position++]); //FIXME use a slice list, not a copy
            if (!m_pContent[m_Position])
                m_IsActive = false;
        }
    }

    void skip(char count) {
        for (int i =0; i < count; i++) {
            if (!m_pContent[++m_Position])
                m_IsActive = false;
        }
    }

    bool isActive() {
        return m_IsActive;
    }

    VObject* currentObject() {
        return m_pCurrentObject;
    }

    /**
     * The context can get deeper and deeper into the object tree
     */
    void stashObject() {
        assert(m_pCurrentObject);
        assert(m_pCurrentObject->m_State == VObject::State::PROPERTIES);
        assert(m_pCurrentObject->m_pCurrentProperty);
        assert(m_pCurrentObject->m_pCurrentProperty->m_Name.size() > 0);
        m_pCurrentObject->m_pCurrentProperty;

        std::cout << "\n\n\nPUSH!!!!!" << m_pCurrentObject << std::endl;

        auto newO = new VObject(this);
        newO->m_State == VObject::State::PROPERTIES;
        newO->m_pParent = m_pCurrentObject;

        m_pCurrentObject = newO;
    }

    void popObject() {

        std::cout << "\n\n\nPOP!!!!!" << m_pCurrentObject << std::endl;

        auto oldO = m_pCurrentObject;

        m_pCurrentObject = m_pCurrentObject->m_pParent;

        if (!m_pCurrentObject)
            m_IsActive = false;
        else {
            m_pCurrentObject->children.push_back(oldO);
            m_pCurrentObject->m_State == VObject::State::PROPERTIES;
        }
    }

    std::basic_string<char> flush() {
        std::basic_string<char> ret = m_Buffer;
        m_Buffer.clear();
        return ret;
    }

    ICSLoader::AbstractObject* _test_raw(const char* content);
};

char VParameters::increment()
{
    m_pContext->push(1);
    return 1;
}

char VParameters::saveName()
{
    return 1;
}

char VParameters::saveValue()
{
    return 1;
}

char VParameters::resetBuffer()
{
    return 1;
}

char VParameters::error()
{
    return 1;
}

char VParameters::nothing()
{
//     std::cout << "not\n";
    return 1;
}

char VProperty::pushName()
{
    m_Name = m_pContext->flush();
    m_pContext->skip(1);
    std::cout << "name2: " << m_Name <<" === ";
    return 1;
}

char VProperty::increment()
{
//     std::cout << "inc\n";
    m_pContext->push(1);
    return 1;
}

char VProperty::parameter()
{
//     std::cout << "param\n";
    return 1;
}

char VProperty::pushParameter()
{
//     std::cout << "pparam\n";
    assert(false);
    return 1;
}

char VProperty::pushProperty()
{
    m_Value = m_pContext->flush();
//     std::cout << "\n\nHERE2!!! " << m_pContext->currentChar() << std::endl;
    m_pContext->skip(m_pContext->currentChar() == '\r' ? 2 : 1);
    std::cout << "pprop " << m_Value << "\n";
//     assert(false);
    return 1;
}

char VProperty::finishParameters()
{
// //     std::cout << "finish\n";

    m_pContext->push(1);
    return 1;
}

char VProperty::pushLine()
{
//     std::cout << "\n\nHERE!!! " << m_pContext->currentChar() << std::endl;
    m_pContext->push(m_pContext->currentChar() == '\r' ? 2 : 1);
    return 1;
}

char VProperty::saveName()
{
    return 1;
}

char VProperty::saveValue()
{
    return 1;
}

char VProperty::error()
{
    return 1;
}

char VObject::error()
{
    return 1;
}

char VObject::increment()
{
    m_pContext->push(1);
    return 1;
}

char VObject::childEvent()
{
//     std::cout << "child\n";
    return 1;
}

char VObject::propertyEvent()
{
    if (!m_pCurrentProperty)
        m_pCurrentProperty = new VProperty(m_pContext);

//     std::cout << "propEv\n";
    m_pCurrentProperty->applyEvent(m_pContext->currentWindow());
    return 1;
}

char VObject::selectObject()
{
    m_pCurrentProperty = new VProperty(m_pContext);

    std::cout << "SELECT OBJECT_TYPE" << m_pContext->flush() << "foo\n";
    m_pContext->skip(m_pContext->currentChar() == '\r' ? 2 : 1);

    return 1;
}

char VObject::finish()
{
    m_pContext->flush();
    m_pContext->popObject();
    return 1;
}

char VObject::nothing()
{
    std::cout << "obj not\n";
    m_pContext->push(1);
    return 1;
}

char VObject::pushProperty()
{
    assert(m_pCurrentProperty);

    /// Detect when the property is an object
    if (m_pCurrentProperty->m_Name.substr(0, 5) == "BEGIN")
        m_pContext->stashObject();
    else if (m_pCurrentProperty->m_Name.substr(0, 3) == "END")
        m_pContext->popObject();
    else
        properties.push_back(m_pCurrentProperty);

    std::cout << "PUSH PROP: " << m_pCurrentProperty->m_Name << std::endl;

    m_pCurrentProperty = nullptr;
}

char VParameters::applyEvent(const char content[4])
{
    auto s = m_State;

    auto event = charToEvent(content);

    m_State = m_StateMap[s][event];

    assert(m_State != VParameters::State::ERROR);

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed\n";
        return 0;
    }

    return (this->*(m_StateFunctor[s][event]))();
}

char VProperty::applyEvent(const char content[4])
{
    auto s = m_State;

    Event event = Event::COUNT__;

    if (s == State::PARAMETERS || s == State::LINE_BREAK_P) //FIXME can be incorporated
        event = m_ParamToEvent[m_Parameters.m_State];
    else
        event = charToEvent(content);

    m_State = m_StateMap[s][event];

    //TODO remove For debug
    assert(m_State != VProperty::State::EMPTY);
    assert(m_State != VProperty::State::ERROR);

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed\n";
        return 0;
    }
//     else
//         std::cout << this << "LINE: " << content[0] << content[1] << content[2] <<content[3] << '\t' << (int) s << (int) event << std::endl;

    return (this->*(m_StateFunctor[s][event]))();
}

char VObject::applyEvent(const char content[4])
{
    auto s = m_State;

    Event event = Event::COUNT__;

    if (s == State::PROPERTIES) {
        if (!m_pCurrentProperty) {
            m_pCurrentProperty = new VProperty(m_pContext);
            assert(m_pCurrentProperty->m_State != VProperty::State::ERROR);
        }
        assert(m_pCurrentProperty->m_State != VProperty::State::ERROR);

        event = m_PropertyToEvent[m_pCurrentProperty->m_State];


        if (event == Event::ERROR)
            assert(false);
    }
    else {
        event = charToEvent(content);
        if (event == Event::ERROR)
            assert(false);
    }

    m_State = m_StateMap[s][event];

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed" << (int) s << (int) event << std::endl;
        return 0;
    }

//     std::cout << "s" << (int)s << " e" << (int) event << std::endl;

    return (this->*(m_StateFunctor[s][event]))();
}

ICSLoader::AbstractObject* VContext::_test_raw(const char* content)
{
    m_pContent = (char*) content;

    m_pCurrentObject = new VObject(this);

    char first[] = {
        0x00,
        content[0],
        content[1],
        content[2]
    };
    currentObject()->applyEvent(first);

    while (true) {
//         std::cout << m_Position << "\tS=" << content[m_Position] << '\n';
        currentObject()->applyEvent(currentWindow());


        if (currentObject()->m_State == VObject::State::ERROR) {
            assert(false);
        }

        if ((!currentObject()) || (!isActive())) {
            std::cout << "COMPLETE!\n";
            return nullptr;
        }
    }
}

}

ICSLoader::ICSLoader(const char* path, bool recursive)
{
    //
}

ICSLoader::~ICSLoader()
{

}

std::atomic<unsigned> ICSLoader::transerQueueSize() const
{
    return {};
}

std::unique_ptr< std::list< ICSLoader::AbstractObject* > > ICSLoader::acquireTrasferQueue() const
{

}

ICSLoader::AbstractObject* ICSLoader::_test_CharToObj(const char* data)
{
    std::cout << "In test\n";

    VParser::VContext ctx;

    ctx._test_raw(data);
}
