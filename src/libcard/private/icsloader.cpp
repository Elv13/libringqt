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
#include <fstream>
#include <unordered_map>

#include "../matrixutils.h"

using PropertyHandler = std::function<
    void(void* p, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params)
>;

using FallbackPropertyHandler = std::function<
    void(
        void* p,
        const std::basic_string<char>& name,
        const std::basic_string<char>& value,
        const AbstractVObjectAdaptor::Parameters& params
    )
>;

using FallbackObjectHandler = std::function<
    void(
        void* p,
        void* p2,
        const std::basic_string<char>& name
    )
>;

class AbstractVObjectAdaptorPrivate
{
public:
    std::function<void*(const std::basic_string<char>& object_type)> m_Factory;

    std::unordered_map< std::basic_string<char>, PropertyHandler > m_hPropertyMap;

    std::unordered_map<int ,FallbackObjectHandler> m_hAbtractObjectHandler;

    FallbackPropertyHandler m_AbstractPropertyHandler;

    int m_Type {0};
};

AbstractVObjectAdaptor::AbstractVObjectAdaptor(int typeId) : d_ptr(new AbstractVObjectAdaptorPrivate)
{
    d_ptr->m_Type = typeId;
}

void AbstractVObjectAdaptor::setAbstractFactory(std::function<void*(const std::basic_string<char>& object_type)> f)
{
    d_ptr->m_Factory = f;
}

void AbstractVObjectAdaptor::setAbstractPropertyHandler(char* name, PropertyHandler handler)
{
    d_ptr->m_hPropertyMap[name] = handler;
}

void AbstractVObjectAdaptor::setAbstractFallbackPropertyHandler(FallbackPropertyHandler handler)
{
    d_ptr->m_AbstractPropertyHandler = handler;
}

void AbstractVObjectAdaptor::setAbstractFallbackObjectHandler(int typeId, FallbackObjectHandler handler)
{
    d_ptr->m_hAbtractObjectHandler[typeId] = handler;
}

int AbstractVObjectAdaptor::getNewTypeId()
{
    static int typeSystem = 42;
    return typeSystem++;
}

namespace VParser {

struct VContext;

/**
 * The list of parameters associated with a property.
 *
 * It is a simple key=value pair delimited:
 *
 *     key_name="value";
 *    \_______/|\_____/|
 *        |    |  |    +-- End delimited, either ';' or ':'
 *        |    |  +------- The parameter value with or without double quote
 *        |    +---------- The key to value separator
 *        +--------------- The parameter name
 *
 * Backslash escaped or quoted ';'/':' shall be appended to the value and not
 * used as delimited.
 */
struct VParameters {
    using VParameter = std::pair<std::basic_string<char>, std::basic_string<char> >;

    enum class State {
        EMPTY      , /*!< Before anything happens   */
        NAME       , /*!< Parsing the name          */
        ESCAPED    , /*!< Ignore everything         */
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
        SPLIT , /*!<  = / ;           */
        SKIP  , /*!<                  */
        FINISH, /*!< :                */
        COUNT__
    };

    /**
     * More pretty than a tuple, otherwise just a pair.
     */
    VParameter m_CurrentParam;

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
    VParameters::Event charToEvent();

    // Actions
    void increment();
    void skip();
    void saveName();
    void saveValue();
    void error();
    void nothing();

    typedef void (VParameters::*function)();
    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> m_StateMap;
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::function> m_StateFunctor;
};

/**
 * Map of the next state
 */
#define VPE VParameters::Event::
#define VPF  &VParameters::
#define ST State::
static EnumClassReordering<VParameters::Event> paramrow =
{                             VPE READ      , VPE QUOTE     , VPE SPLIT, VPE SKIP, VPE FINISH     };
const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> VParameters::m_StateMap = {
{ST EMPTY      , {{paramrow, {ST NAME       , ST ERROR      , ST ERROR, ST NAME       , ST DONE  }}}},
{ST NAME       , {{paramrow, {ST NAME       , ST NAME       , ST VALUE, ST NAME       , ST ERROR }}}},
{ST ESCAPED    , {{paramrow, {ST VALUE      , ST VALUE      , ST NAME , ST VALUE      , ST DONE  }}}},
{ST VALUE      , {{paramrow, {ST VALUE      , ST VALUE_QUOTE, ST NAME , ST ESCAPED    , ST DONE  }}}},
{ST VALUE_QUOTE, {{paramrow, {ST VALUE_QUOTE, ST VALUE      , ST NAME , ST VALUE_QUOTE, ST VALUE }}}},
{ST DONE       , {{paramrow, {ST ERROR      , ST ERROR      , ST ERROR, ST ERROR      , ST DONE  }}}},
{ST ERROR      , {{paramrow, {ST ERROR      , ST ERROR      , ST ERROR, ST ERROR      , ST ERROR }}}}
};

const Matrix2D<VParameters::State, VParameters::Event, VParameters::function> VParameters::m_StateFunctor = {
{State::EMPTY      , {{paramrow, { VPF increment  , VPF increment  ,  VPF error    , VPF increment , VPF nothing  }}}},
{State::NAME       , {{paramrow, { VPF increment  , VPF increment  ,  VPF saveName , VPF increment , VPF nothing  }}}},
{State::ESCAPED    , {{paramrow, { VPF increment  , VPF increment  ,  VPF increment, VPF increment , VPF increment}}}},
{State::VALUE      , {{paramrow, { VPF increment  , VPF skip       ,  VPF saveValue, VPF skip      , VPF saveValue}}}},
{State::VALUE_QUOTE, {{paramrow, { VPF increment  , VPF skip       ,  VPF saveValue, VPF increment , VPF increment}}}},
{State::DONE       , {{paramrow, { VPF error      , VPF error      ,  VPF nothing  , VPF error     , VPF nothing  }}}},
{State::ERROR      , {{paramrow, { VPF nothing    , VPF nothing    ,  VPF error    , VPF nothing   , VPF nothing  }}}},
};
#undef ST
#undef VPF
#undef VPE

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
        FINISH      , /*!< */
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
    Event charToEvent();

    // Actions, return the number of bytes the cursor has to be incremented with
    void pushName();
    void increment();
    void parameter();
    void push();
    void pushLine();
    void error();
    void nothing();

    std::list<VParameters*> parameters;

    std::basic_string<char> m_Name;
    std::basic_string<char> m_Value;

    typedef void (VProperty::*function)();
    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
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
{                                    VPE READ,        VPE SPLIT_PARAM  ,   VPE SPLIT   ,     VPE LINE_BREAK,  VPE FINISH,  VPE ERROR};
const Matrix2D<VProperty::State, VProperty::Event, VProperty::State> VProperty::m_StateMap = {
{State::EMPTY       , {{proprow, {State::NAME      , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR, State::ERROR }}}},
{State::NAME        , {{proprow, {State::NAME      , State::PARAMETERS , State::VALUE  , State::ERROR        , State::ERROR, State::ERROR }}}},
{State::PARAMETERS  , {{proprow, {State::PARAMETERS, State::PARAMETERS , State::VALUE  , State::LINE_BREAK_P , State::ERROR, State::ERROR }}}},
{State::VALUE       , {{proprow, {State::VALUE     , State::VALUE      , State::VALUE  , State::LINE_BREAK_V , State::DONE , State::ERROR }}}},
{State::LINE_BREAK_P, {{proprow, {State::PARAMETERS, State::PARAMETERS , State::ERROR  , State::ERROR        , State::ERROR, State::ERROR }}}},
{State::LINE_BREAK_V, {{proprow, {State::VALUE     , State::VALUE      , State::ERROR  , State::ERROR        , State::ERROR, State::ERROR }}}},
{State::DONE        , {{proprow, {State::ERROR     , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR, State::ERROR }}}},
{State::ERROR       , {{proprow, {State::ERROR     , State::ERROR      , State::ERROR  , State::ERROR        , State::ERROR, State::ERROR }}}}
};

const Matrix2D<VProperty::State, VProperty::Event, VProperty::function> VProperty::m_StateFunctor = {
{State::EMPTY       , {{proprow, { VPF increment , VPF error     , VPF error     , VPF error    , VPF error , VPF error }}}},
{State::NAME        , {{proprow, { VPF increment , VPF pushName  , VPF pushName  , VPF error    , VPF error , VPF error }}}},
{State::PARAMETERS  , {{proprow, { VPF parameter , VPF nothing   , VPF nothing   , VPF pushLine , VPF error , VPF error }}}},
{State::VALUE       , {{proprow, { VPF increment , VPF increment , VPF increment , VPF pushLine , VPF push  , VPF error }}}},
{State::LINE_BREAK_P, {{proprow, { VPF increment , VPF nothing   , VPF error     , VPF error    , VPF error , VPF error }}}},
{State::LINE_BREAK_V, {{proprow, { VPF increment , VPF increment , VPF error     , VPF error    , VPF error , VPF error }}}},
{State::DONE        , {{proprow, { VPF error     , VPF error     , VPF error     , VPF error    , VPF error , VPF error }}}},
{State::ERROR       , {{proprow, { VPF error     , VPF error     , VPF error     , VPF error    , VPF error , VPF error }}}},
};
#undef VPF
#undef VPE

const Matrix1D<VParameters::State, VProperty::Event> VProperty::m_ParamToEvent = {
    { VParameters::State::EMPTY      , VProperty::Event::READ  },
    { VParameters::State::NAME       , VProperty::Event::READ  },
    { VParameters::State::ESCAPED    , VProperty::Event::READ  },
    { VParameters::State::VALUE      , VProperty::Event::READ  },
    { VParameters::State::VALUE_QUOTE, VProperty::Event::READ  },
    { VParameters::State::DONE       , VProperty::Event::SPLIT },
    { VParameters::State::ERROR      , VProperty::Event::ERROR },
};

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
     * The adapter will safely cast this void* using templated lambdas.
     */
    void* m_pReflected { nullptr };

    std::shared_ptr<AbstractVObjectAdaptor> m_pAdaptor {nullptr};

    /**
     *
     */
    enum class State {
        EMPTY     , /*!< Nothing parsed yet              */
        HEADER    , /*!< BEGIN:OBJECT_TYPE               */ //FIXME remove (handled as a property)
        PROPERTIES, /*!< Normal key / value properties   */
        QUOTE     , /*!< This is a quoted parameter      */ //FIXME implement
        FOOTER    , /*!< END:OBJECT_TYPE                 */ //FIXME remove (handled as a property)
        DONE      , /*!< Once the footer is fully parsed */
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
    std::list<VObject*> children; //TODO remove?
    std::basic_string<char> name;


    /**
     *
     */
    Event charToEvent();

    static const Matrix1D<VProperty::State, VObject::Event> m_PropertyToEvent;

    void error();
    void increment();
    void propertyEvent();
    void pushProperty();
    void finish();
    void nothing();

    typedef void (VObject::*function)();
    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
    static const Matrix2D<VObject::State, VObject::Event, VObject::State> m_StateMap;
    static const Matrix2D<VObject::State, VObject::Event, VObject::function> m_StateFunctor;
};

#define VPE VObject::Event::
#define VPF &VObject::
static EnumClassReordering<VObject::Event> objrow =
{                                 VPE READ,            VPE NEW_LINE,     VPE CHILD_FINISH,   VPE ERROR   };
const Matrix2D<VObject::State, VObject::Event, VObject::State> VObject::m_StateMap = {
{State::EMPTY      , {{objrow, {State::PROPERTIES , State::ERROR     , State::ERROR      , State::ERROR }}}},
{State::HEADER     , {{objrow, {State::HEADER     , State::PROPERTIES, State::ERROR      , State::ERROR }}}},//TODO remove
{State::PROPERTIES , {{objrow, {State::PROPERTIES , State::PROPERTIES, State::PROPERTIES , State::ERROR }}}},
{State::QUOTE      , {{objrow, {State::QUOTE      , State::PROPERTIES, State::PROPERTIES , State::ERROR }}}},
{State::FOOTER     , {{objrow, {State::FOOTER     , State::DONE      , State::ERROR      , State::ERROR }}}},//TODO remove
{State::DONE       , {{objrow, {State::ERROR      , State::ERROR     , State::ERROR      , State::ERROR }}}},
{State::ERROR      , {{objrow, {State::ERROR      , State::ERROR     , State::ERROR      , State::ERROR }}}},
};

const Matrix2D<VObject::State, VObject::Event, VObject::function> VObject::m_StateFunctor = {
{State::EMPTY      , {{objrow, { VPF increment    , VPF nothing       , VPF error        , VPF error}}}},
{State::HEADER     , {{objrow, { VPF increment    , VPF error         , VPF error        , VPF error}}}}, //TODO remove
{State::PROPERTIES , {{objrow, { VPF propertyEvent, VPF propertyEvent , VPF pushProperty , VPF error}}}},
{State::QUOTE      , {{objrow, { VPF propertyEvent, VPF propertyEvent , VPF propertyEvent, VPF error}}}},
{State::FOOTER     , {{objrow, { VPF increment    , VPF finish        , VPF error        , VPF error}}}}, //TODO remove
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

/**
 * This is a 4 byte circular buffer to hold the current state.
 *
 * There is little point in reading more than that beside maybe some underlying
 * OS I/O frame size.
 *
 * This makes zero dynamic allocation and can be extended later for micro
 * optimization without side effects. Eventually the whole line should be kept
 * until it is flushed, so a 78 byte buffer (previous + 75 + /r/n). But for now
 * the line beffer is copied to an std::vector to avoid useless complexity.
 *
 *                next
 *                  |
 *   current -----+ |
 *  previous ---+ | | +-- last
 *              | | | |
 *            B E G I N : V C A R D
 *              \-----/
 *                 |
 *                 + The sliding window
 *
 * Note that the position of the writing cursor is always ahead of the reading
 * cursor. Given the window size is fixed, this is a "simplified" circular
 * buffer as the offset between the reading cursor and the writing one is
 * static (always 3 bytes).
 */
class VBuffer
{
public:
    inline void insert(char c, char rawPos) {
        m_lContent[rawPos] = c;
    }

    inline void put(char c) {
        m_Room--;
        assert(m_Room+1);
        m_IsInactive = m_IsInactive || !c; //BUG lose the last 2 bytes, ignore for now
        assert(c || m_IsInactive == true);
        m_Pos = (m_Pos+1)%4;

        m_lContent[m_Pos] = c;
    }

    inline void pop(char count) {
        assert(count <= 3);
//         m_Pos = (m_Pos+count)%4;
        m_Room += count;
        assert(m_Room <= 4);
    }

    inline char get(char pos) const {
        assert(pos < 4 && pos >= 0);
        return m_lContent[(m_Pos+pos) % 4];
    }

    // Create validated access for each chars. When compiled in release mode
    // it will inline nicely.
    inline char current() const {
        return get(2);
    }

    inline char previous() const {
        return get(1);
    }

    inline char next() const {
        assert(m_Room <= 2);
        return get(3);
    }

    inline char last() const {
        assert(m_Room <= 1);
        return get(0);
    }

    inline bool isActive() const {
        return !m_IsInactive;
    }

    inline char room() const
    {
        return m_Room;
    }

private:
    char m_lContent[4] = {0x00,0x00,0x00,0x00};
    char m_Pos {3}; /*!< The previous byte is always necessary, so start at 1*/
    bool m_IsInactive {false};
    char m_Room {0};

//     m_Pos == 1 and p == 0 --> 1  | 1 % 4 | 1
//     m_Pos == 1 and p == 1 --> 2  | 2 % 4 | 2
//     m_Pos == 1 and p == 2 --> 3  | 3 % 4 | 3
//     m_Pos == 1 and p == 3 --> 0  | 4 % 4 | 0
//                                  |       |
//     m_Pos == 2 and p == 0 --> 2  | 2 % 4 | 2
//     m_Pos == 2 and p == 1 --> 3  | 3 % 4 | 3
//     m_Pos == 2 and p == 2 --> 0  | 4 % 4 | 0
//     m_Pos == 2 and p == 3 --> 1  | 5 % 4 | 1
//                                  |       |
//     m_Pos == 3 and p == 0 --> 3  | 3 % 4 | 3
//     m_Pos == 3 and p == 1 --> 0  | 4 % 4 | 0
//     m_Pos == 3 and p == 2 --> 1  | 5 % 4 | 1
//     m_Pos == 3 and p == 3 --> 2  | 6 % 4 | 2
};

struct VContext
{

    std::basic_string<char> m_Buffer;

    bool     m_IsActive       {  true   };
    VObject* m_pCurrentObject { nullptr };
    int      m_ToGet {0};

    std::unordered_map<std::basic_string<char>, std::shared_ptr<AbstractVObjectAdaptor> > m_Adaptors;
    std::shared_ptr<AbstractVObjectAdaptor> m_FallbackAdaptors;

    VBuffer m_Window;

    void push(char count) {
        assert(count < 4);
        for (int i =0; i < count; i++) {
            m_Buffer.push_back(m_Window.current()); //FIXME use a slice list, not a copy
            m_Window.pop(1);
        }
    }

    void skip(char count) {
        assert(count < 4);
        m_Window.pop(count);
    }

    bool isActive() const {
        return m_Window.isActive() && m_IsActive;
    }

    VObject* currentObject() {
        return m_pCurrentObject;
    }

    std::function<void*(const std::basic_string<char>& object_type)>* factoryForType(const std::basic_string<char>& name) {
        if (auto& a = m_Adaptors[name])
            return &a->d_ptr->m_Factory;

        if (auto& f = m_FallbackAdaptors)
            return &f->d_ptr->m_Factory;

        return nullptr;
    }

    FallbackObjectHandler handlerForAdaptor(std::shared_ptr<AbstractVObjectAdaptor> parent, std::shared_ptr<AbstractVObjectAdaptor> a) {
        if (!parent || !a)
            return nullptr;

        return parent->d_ptr->m_hAbtractObjectHandler[a->d_ptr->m_Type];
    }

    PropertyHandler* propertyHandler(const std::basic_string<char>& name) {
        if ((!currentObject()) || !currentObject()->m_pReflected || !currentObject()->m_pAdaptor)
            return nullptr;

        if (auto& h = currentObject()->m_pAdaptor->d_ptr->m_hPropertyMap[name])
            return &h;

        return nullptr;
    }

    bool handleProperty()
    {
        auto o = currentObject();

        if (!o)
            return false;

        if (auto handler = propertyHandler(o->m_pCurrentProperty->m_Name))
            (*handler)(
                o->m_pReflected,
                o->m_pCurrentProperty->m_Value,
                o->m_pCurrentProperty->m_Parameters.parameters
            );
        else if (o->m_pAdaptor && o->m_pAdaptor->d_ptr->m_AbstractPropertyHandler)
            o->m_pAdaptor->d_ptr->m_AbstractPropertyHandler(
                o->m_pReflected,
                o->m_pCurrentProperty->m_Name,
                o->m_pCurrentProperty->m_Value,
                o->m_pCurrentProperty->m_Parameters.parameters
            );
    }

    std::shared_ptr<AbstractVObjectAdaptor> adapter(const std::basic_string<char>& name) {
        return m_Adaptors[name];
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

        auto newO = new VObject(this);
        newO->m_State == VObject::State::PROPERTIES;
        newO->m_pParent = m_pCurrentObject;

        newO->name = m_pCurrentObject->m_pCurrentProperty->m_Value;

        if (auto factory = factoryForType(newO->name)) {
            newO->m_pReflected = (*factory)(newO->name);
            newO->m_pAdaptor   = adapter(newO->name);
        }

        m_pCurrentObject = newO;
    }

    void popObject() {
        auto oldO = m_pCurrentObject;

        m_pCurrentObject = m_pCurrentObject->m_pParent;

        if (!m_pCurrentObject)
            m_IsActive = false;
        else {
            m_pCurrentObject->children.push_back(oldO);

            if (auto h = handlerForAdaptor(m_pCurrentObject->m_pAdaptor, oldO->m_pAdaptor)) {
                h(m_pCurrentObject->m_pReflected, oldO->m_pReflected, oldO->name);
            }

            m_pCurrentObject->m_State == VObject::State::PROPERTIES;
        }

    }

    std::basic_string<char> flush() {
        std::basic_string<char> ret = m_Buffer;
        m_Buffer.clear();
        return ret;
    }

    ICSLoader::AbstractObject* _test_raw(const char* content);

    bool readFile(const char* path);
};

VParameters::Event VParameters::charToEvent()
{
    if (m_pContext->m_Window.previous() == '\\')
        return Event::READ;

    switch(m_pContext->m_Window.current()) {
        case ';':
        case '=':
            return Event::SPLIT;
        case ':':
            return Event::FINISH;
        case '"':
            return Event::QUOTE;
        case '\\':
            return Event::SKIP;
    };

    return Event::READ;
}

/**
 * It needs the next 3 bytes to properly detect line breaks on all OSes.
 */
VProperty::Event VProperty::charToEvent()
{
    // Check the simple case
    switch(m_pContext->m_Window.current()) {
        case ':':
            return Event::SPLIT;
        case ';':
            return Event::SPLIT_PARAM;
    };

    // Detect all kind of "continue on the next line"
    if ((m_pContext->m_Window.current() == '\n' || m_pContext->m_Window.current() == '\r') && ((m_pContext->m_Window.next() == '\n' && m_pContext->m_Window.last() == ' ') || m_pContext->m_Window.next() == ' '))
        return Event::LINE_BREAK;

    // If the above failed, then the property is completed
    if (m_pContext->m_Window.current() == '\n' || m_pContext->m_Window.current() == '\r')
        return Event::FINISH;

    return Event::READ;
}


VObject::Event VObject::charToEvent()
{
    if (m_pContext->m_Window.previous() == '\\')
        return Event::READ;

    // Detect multi-line props
    if (
      (
        m_pContext->m_Window.current() == '\n' || m_pContext->m_Window.current() == '\r'
      ) && (
        (m_pContext->m_Window.next() == '\n' && m_pContext->m_Window.last() == ' ')
        || m_pContext->m_Window.next() == ' '
      )
    ) {
        assert(false);
        return Event::READ;
    }

    switch(m_pContext->m_Window.current()) {
        case '\n':
        case '\r':
        case '=':
            return Event::NEW_LINE;
    };

    return Event::READ;
}

void VParameters::increment()
{
    m_pContext->push(1);
}

void VParameters::skip()
{
    m_pContext->skip(1);
}

void VParameters::saveName()
{
    m_CurrentParam.first = m_pContext->flush();
    m_pContext->skip(1);
}

void VParameters::saveValue()
{
    m_CurrentParam.second = m_pContext->flush();

    parameters.push_back(m_CurrentParam);
    assert(m_CurrentParam.second != "\"b");
    m_pContext->skip(1);
    m_CurrentParam.first.clear();
    m_CurrentParam.second.clear();
}

void VParameters::error()
{
    assert(false);
}

void VParameters::nothing()
{
}

void VProperty::pushName()
{
    m_Name = m_pContext->flush();
    m_pContext->skip(1);
}

void VProperty::increment()
{
    m_pContext->push(1);
}

void VProperty::parameter()
{
    m_Parameters.applyEvent();
}

void VProperty::push()
{
    m_Value = m_pContext->flush();
    m_pContext->skip(m_pContext->m_Window.current() == '\r' ? 2 : 1);
}

void VProperty::pushLine()
{
    m_pContext->skip(m_pContext->m_Window.current() == '\r' ? 3 : 2);
}

void VProperty::nothing()
{
}

void VProperty::error()
{
    assert(false);
}

void VObject::error()
{
    assert(false);
}

void VObject::increment()
{
    m_pContext->push(1);
}

void VObject::propertyEvent()
{
    if (!m_pCurrentProperty)
        m_pCurrentProperty = new VProperty(m_pContext);

    m_pCurrentProperty->applyEvent();
}

void VObject::finish()
{
    m_pContext->flush();
    m_pContext->popObject();
}

void VObject::nothing()
{
    m_pContext->push(1);
}

void VObject::pushProperty()
{
    //TODO move everything into the context
    assert(m_pCurrentProperty);

    /// Detect when the property is an object
    if (m_pCurrentProperty->m_Name.substr(0, 5) == "BEGIN") {

        m_pContext->stashObject();
    }
    else if (m_pCurrentProperty->m_Name.substr(0, 3) == "END") {
        m_pContext->popObject();
    }
    else {
        m_pContext->handleProperty();
    }

    m_pCurrentProperty = nullptr;
}

void VParameters::applyEvent()
{
    auto s = m_State;

    auto event = charToEvent();

    m_State = m_StateMap[s][event];

    assert(m_State != VParameters::State::ERROR);

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed\n";
        return;
    }

    (this->*(m_StateFunctor[s][event]))();
}

void VProperty::applyEvent()
{
    auto s = m_State;

    Event event = Event::COUNT__;

    if (s == State::PARAMETERS || s == State::LINE_BREAK_P) //FIXME can be incorporated
        event = m_ParamToEvent[m_Parameters.m_State];
    else
        event = charToEvent();

    m_State = m_StateMap[s][event];


    if (m_State == State::ERROR) {
        std::cout << "Parsing failed\n";
        return;
    }

    return (this->*(m_StateFunctor[s][event]))();
}

void VObject::applyEvent()
{
    auto s = m_State;

    Event event = Event::COUNT__;

    if (s == State::PROPERTIES) { //FIXME remove all this
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
        event = charToEvent();
        if (event == Event::ERROR)
            assert(false);
    }

    m_State = m_StateMap[s][event];

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed" << (int) s << (int) event << std::endl;
        return;
    }

    return (this->*(m_StateFunctor[s][event]))();
}


// For unit testing without reading from the disk
ICSLoader::AbstractObject* VContext::_test_raw(const char* content)
{
    int pos = 3;
    m_pCurrentObject = new VObject(this);

    m_Window.insert(content[0], 1);
    m_Window.insert(content[1], 2);
    m_Window.insert(content[2], 3);
    currentObject()->applyEvent();

    while (true) {
        while (m_Window.room())
            m_Window.put(content[pos++]);

        currentObject()->applyEvent();

        if (currentObject()->m_State == VObject::State::ERROR) {
            assert(false);
        }

        if ((!currentObject()) || (!isActive())) {
            std::cout << "COMPLETE!\n";
            return nullptr;
        }
    }
}

bool VContext::readFile(const char* path)
{
    int c;
//     FILE *file;
//     file = fopen(path, "r");
    std::ifstream file2(path, std::ios::binary);

    if (file2) {

        // Implement an inlined buffer along with __attribute__((always_inline))
        //
        // The first implementation had serious issues with using iostream
        // own buffer due to the overhead of the function call not being
        // inlined reliably (100x too slow, not ~5%). The proper inlining of
        // this type of state machine is absolutely necessary to get any
        // performance out of state machine based parsers.
        char buf[4096];
        file2.read(buf, sizeof(buf) / sizeof(*buf));
        int pos = 0;

        m_pCurrentObject = new VObject(this);

        // Init the sling window
        for (pos = 1; pos < 4; pos++) {
            if ((c = buf[pos]) != EOF)
                m_Window.insert(c, pos+1);
            else {
                file2.close();
                return false;
            }
        }

        while (isActive()) {

            while (m_Window.room() && (c = buf[pos]) != EOF) {
                m_Window.put(c);
                pos++;
                if (pos == sizeof(buf) / sizeof(*buf)) {
                    file2.read(buf, sizeof(buf) / sizeof(*buf));
                    pos = 0;
                }
            }

            if (c == EOF || !c)
                m_IsActive = false;

            if (!isActive())
                break;

            currentObject()->applyEvent();
        }

        file2.close();

        if ((!currentObject()) || (!isActive())) {
            std::cout << "COMPLETE!\n";
        }

        return true;
    }

    return false;
}

}

bool ICSLoader::loadFile(const char* path)
{
    d_ptr->readFile(path);
}

ICSLoader::ICSLoader() : d_ptr(new VParser::VContext)
{
}

ICSLoader::~ICSLoader()
{
    delete d_ptr;
}

void ICSLoader::registerVObjectAdaptor(char* name, std::shared_ptr<AbstractVObjectAdaptor> adaptor)
{
    d_ptr->m_Adaptors[name] = adaptor;
}

void ICSLoader::registerFallbackVObjectAdaptor(std::shared_ptr<AbstractVObjectAdaptor> adaptor)
{
    d_ptr->m_FallbackAdaptors = adaptor;
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

    d_ptr->_test_raw(data);
}
