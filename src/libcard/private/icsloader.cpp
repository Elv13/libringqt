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

void AbstractVObjectAdaptor::setAbstractPropertyHandler(const char* name, PropertyHandler handler)
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
struct VParameters final {
    using VParameter = std::pair<std::basic_string<char>, std::basic_string<char> >;

    enum class State : unsigned char {
        EMPTY  , /*!< Before anything happens   */
        NAME   , /*!< Parsing the name          */
        ESCAPED, /*!< Ignore everything         */
        VALUE  , /*!< Parsing the value         */
        QUOTED , /*!< Parsing the value with "  */
        DONE   , /*!< Success!                  */
        ERROR  , /*!< The encoding is corrupted */
        COUNT__
    };

    /// The classes of characters
    enum class Event : unsigned char {
        READ  , /*!< Anything but ";: */
        QUOTE , /*!<     "            */
        SPLIT , /*!<  = / ;           */
        SKIP  , /*!<                  */
        FINISH, /*!< :                */
        COUNT__
    };

    /**
     * Categories of mutations performed from the file to the buffer
     */
    enum class Action : unsigned char {
        INCREMENT, /*!< Add a byte to the buffer                       */
        SKIP     , /*!< skip a byte instead of adding it to the buffer */
        SAVENAME , /*!< "save" the current buffer into the name field  */
        SAVEVALUE, /*!< "save" the current buffer into the value field */
        ERROR    , /*!< The document is in an invalid state            */
        NOTHING  , /*!< No operation                                   */
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
    VParameters::Event charToEvent() __attribute__((always_inline));

    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
    void applyActions(VParameters::Action a) __attribute__((always_inline));
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> m_StateMap;
    static const Matrix2D<VParameters::State, VParameters::Event, VParameters::Action> m_StateActions;
};

/**
 * Map of the next state
 */
#define VPE VParameters::Event::
#define VPF  VParameters::Action::
#define ST State::
static EnumClassReordering<VParameters::Event> paramrow =
{                          VPE READ, VPE QUOTE, VPE SPLIT, VPE SKIP, VPE FINISH   };
const Matrix2D<VParameters::State, VParameters::Event, VParameters::State> VParameters::m_StateMap = {
{ST EMPTY  , {{paramrow, {ST NAME  , ST ERROR , ST ERROR, ST NAME   , ST DONE  }}}},
{ST NAME   , {{paramrow, {ST NAME  , ST NAME  , ST VALUE, ST NAME   , ST ERROR }}}},
{ST ESCAPED, {{paramrow, {ST VALUE , ST VALUE , ST NAME , ST VALUE  , ST DONE  }}}},
{ST VALUE  , {{paramrow, {ST VALUE , ST QUOTED, ST NAME , ST ESCAPED, ST DONE  }}}},
{ST QUOTED , {{paramrow, {ST QUOTED, ST VALUE , ST NAME , ST QUOTED , ST VALUE }}}},
{ST DONE   , {{paramrow, {ST ERROR , ST ERROR , ST ERROR, ST ERROR  , ST DONE  }}}},
{ST ERROR  , {{paramrow, {ST ERROR , ST ERROR , ST ERROR, ST ERROR  , ST ERROR }}}}
};

const Matrix2D<VParameters::State, VParameters::Event, VParameters::Action> VParameters::m_StateActions = {
{State::EMPTY  , {{paramrow, { VPF INCREMENT, VPF INCREMENT, VPF ERROR    , VPF INCREMENT, VPF NOTHING  }}}},
{State::NAME   , {{paramrow, { VPF INCREMENT, VPF INCREMENT, VPF SAVENAME , VPF INCREMENT, VPF NOTHING  }}}},
{State::ESCAPED, {{paramrow, { VPF INCREMENT, VPF INCREMENT, VPF INCREMENT, VPF INCREMENT, VPF INCREMENT}}}},
{State::VALUE  , {{paramrow, { VPF INCREMENT, VPF SKIP     , VPF SAVEVALUE, VPF SKIP     , VPF SAVEVALUE}}}},
{State::QUOTED , {{paramrow, { VPF INCREMENT, VPF SKIP     , VPF SAVEVALUE, VPF INCREMENT, VPF INCREMENT}}}},
{State::DONE   , {{paramrow, { VPF ERROR    , VPF ERROR    , VPF NOTHING  , VPF ERROR    , VPF NOTHING  }}}},
{State::ERROR  , {{paramrow, { VPF NOTHING  , VPF NOTHING  , VPF ERROR    , VPF NOTHING  , VPF NOTHING  }}}},
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
struct VProperty final
{
    /**
     *
     */
    enum class State : unsigned char {
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
    enum class Event : unsigned char {
        READ       , /*!<  */
        SPLIT_PARAM, /*!<  */
        SPLIT      , /*!<  */
        LINE_BREAK , /*!<  */
        FINISH     , /*!<  */
        ERROR      , /*!<  */
        COUNT__
    };

    /**
     * Actual things to perform on the object.
     *
     * The first version of this system directly used methods for this but
     * they could not be inlined by the compiler and the overhead of millions
     * of function calls ended up being the largest bottleneck.
     */
    enum class Action : unsigned char {
        PUSHNAME ,
        INCREMENT,
        PARAMETER,
        PUSH     ,
        PUSHLINE ,
        ERROR    ,
        NOTHING  ,
        RESET    ,
    };

    explicit VProperty(VContext* context) : m_pContext(context), m_Parameters(context){}

    // Attributes
    VContext*  m_pContext;
    VParameters m_Parameters;
    std::basic_string<char> m_Name;
    std::basic_string<char> m_Value;

    /**
     *
     */
    Event charToEvent() __attribute__((always_inline));

    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
    void applyEvent(VProperty::Event e) __attribute__((always_inline));
    void applyActions(VProperty::Action a) __attribute__((always_inline));
    static const Matrix2D<VProperty::State , VProperty::Event, VProperty::State> m_StateMap;
    static const Matrix2D<VProperty::State , VProperty::Event, VProperty::Action> m_StateActions;
    static const Matrix1D<VParameters::State, VProperty::Event> m_ParamToEvent;
};

/**
 * Map of the next state
 */
#define VPE VProperty::Event::
#define VPF VProperty::Action::
#define VPS VProperty::State::
static EnumClassReordering<VProperty::Event> proprow =
{                                    VPE READ,    VPE SPLIT_PARAM, VPE SPLIT, VPE LINE_BREAK,  VPE FINISH, VPE ERROR   };
const Matrix2D<VProperty::State, VProperty::Event, VProperty::State> VProperty::m_StateMap = {
{State::EMPTY       , {{proprow, {VPS NAME      , VPS ERROR     , VPS ERROR, VPS ERROR       , VPS ERROR, VPS ERROR }}}},
{State::NAME        , {{proprow, {VPS NAME      , VPS PARAMETERS, VPS VALUE, VPS ERROR       , VPS ERROR, VPS ERROR }}}},
{State::PARAMETERS  , {{proprow, {VPS PARAMETERS, VPS PARAMETERS, VPS VALUE, VPS LINE_BREAK_P, VPS ERROR, VPS ERROR }}}},
{State::VALUE       , {{proprow, {VPS VALUE     , VPS VALUE     , VPS VALUE, VPS LINE_BREAK_V, VPS DONE , VPS ERROR }}}},
{State::LINE_BREAK_P, {{proprow, {VPS PARAMETERS, VPS PARAMETERS, VPS ERROR, VPS ERROR       , VPS ERROR, VPS ERROR }}}},
{State::LINE_BREAK_V, {{proprow, {VPS VALUE     , VPS VALUE     , VPS ERROR, VPS ERROR       , VPS ERROR, VPS ERROR }}}},
{State::DONE        , {{proprow, {VPS ERROR     , VPS ERROR     , VPS ERROR, VPS ERROR       , VPS EMPTY, VPS ERROR }}}},
{State::ERROR       , {{proprow, {VPS ERROR     , VPS ERROR     , VPS ERROR, VPS ERROR       , VPS ERROR, VPS ERROR }}}}
};

const Matrix2D<VProperty::State, VProperty::Event, VProperty::Action> VProperty::m_StateActions = {
{State::EMPTY       , {{proprow, { VPF INCREMENT , VPF ERROR     , VPF ERROR     , VPF ERROR    , VPF ERROR , VPF ERROR }}}},
{State::NAME        , {{proprow, { VPF INCREMENT , VPF PUSHNAME  , VPF PUSHNAME  , VPF ERROR    , VPF ERROR , VPF ERROR }}}},
{State::PARAMETERS  , {{proprow, { VPF PARAMETER , VPF NOTHING   , VPF NOTHING   , VPF PUSHLINE , VPF ERROR , VPF ERROR }}}},
{State::VALUE       , {{proprow, { VPF INCREMENT , VPF INCREMENT , VPF INCREMENT , VPF PUSHLINE , VPF PUSH  , VPF ERROR }}}},
{State::LINE_BREAK_P, {{proprow, { VPF INCREMENT , VPF NOTHING   , VPF ERROR     , VPF ERROR    , VPF ERROR , VPF ERROR }}}},
{State::LINE_BREAK_V, {{proprow, { VPF INCREMENT , VPF INCREMENT , VPF ERROR     , VPF ERROR    , VPF ERROR , VPF ERROR }}}},
{State::DONE        , {{proprow, { VPF ERROR     , VPF ERROR     , VPF ERROR     , VPF ERROR    , VPF RESET , VPF ERROR }}}},
{State::ERROR       , {{proprow, { VPF ERROR     , VPF ERROR     , VPF ERROR     , VPF ERROR    , VPF ERROR , VPF ERROR }}}},
};
#undef VPS
#undef VPF
#undef VPE

const Matrix1D<VParameters::State, VProperty::Event> VProperty::m_ParamToEvent = {
    { VParameters::State::EMPTY      , VProperty::Event::READ  },
    { VParameters::State::NAME       , VProperty::Event::READ  },
    { VParameters::State::ESCAPED    , VProperty::Event::READ  },
    { VParameters::State::VALUE      , VProperty::Event::READ  },
    { VParameters::State::QUOTED, VProperty::Event::READ  },
    { VParameters::State::DONE       , VProperty::Event::SPLIT },
    { VParameters::State::ERROR      , VProperty::Event::ERROR },
};

/**
 * Everything between a BEGIN:OBJECT_TYPE and it's matching END:OBJECT_TYPE.
 */
struct VObject final
{
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
    enum class State : unsigned char {
        EMPTY     , /*!< Nothing parsed yet              */
        PROPERTIES, /*!< Normal key / value properties   */
        QUOTE     , /*!< This is a quoted parameter      */ //FIXME implement
        DONE      , /*!< Once the footer is fully parsed */
        ERROR     , /*!< Parsing failed                  */
        COUNT__
    };

    /**
     *
     */
    enum class Event : unsigned char {
        READ        , /*!<  */
        NEW_LINE    , /*!<  */
        CHILD_FINISH, /*!< TODO normalize with children state */
        ERROR       ,
        COUNT__
    };

    enum class Action : unsigned char {
        ERROR    ,
        INCREMENT,
        DELEGATE ,
        PUSH     ,
        FINISH   ,
        NOTHING  ,
    };

    explicit VObject(VContext* context) : m_pContext(context) {}

    // Attributes
    VContext* m_pContext;
    std::basic_string<char> name;

    /**
     *
     */
    Event charToEvent();


    State m_State {State::EMPTY};
    void applyEvent() __attribute__((always_inline));
    void applyActions(VObject::Action a) __attribute__((always_inline));
    static const Matrix1D<VProperty::State, VObject::Event> m_PropertyToEvent;
    static const Matrix2D<VObject::State, VObject::Event, VObject::State> m_StateMap;
    static const Matrix2D<VObject::State, VObject::Event, VObject::Action> m_StateActions;
};

#define VPE VObject::Event::
#define VPF VObject::Action::
#define VPS VObject::State::
static EnumClassReordering<VObject::Event> objrow =
{                                 VPE READ,            VPE NEW_LINE,     VPE CHILD_FINISH,   VPE ERROR   };
const Matrix2D<VObject::State, VObject::Event, VObject::State> VObject::m_StateMap = {
{State::EMPTY      , {{objrow, {VPS PROPERTIES, VPS ERROR     , VPS ERROR     , VPS ERROR }}}},
{State::PROPERTIES , {{objrow, {VPS PROPERTIES, VPS PROPERTIES, VPS PROPERTIES, VPS ERROR }}}},
{State::QUOTE      , {{objrow, {VPS QUOTE     , VPS PROPERTIES, VPS PROPERTIES, VPS ERROR }}}},
{State::DONE       , {{objrow, {VPS ERROR     , VPS ERROR     , VPS ERROR     , VPS ERROR }}}},
{State::ERROR      , {{objrow, {VPS ERROR     , VPS ERROR     , VPS ERROR     , VPS ERROR }}}},
};

const Matrix2D<VObject::State, VObject::Event, VObject::Action> VObject::m_StateActions = {
{State::EMPTY      , {{objrow, { VPF INCREMENT, VPF NOTHING , VPF ERROR   , VPF ERROR}}}},
{State::PROPERTIES , {{objrow, { VPF DELEGATE , VPF DELEGATE, VPF PUSH    , VPF ERROR}}}},
{State::QUOTE      , {{objrow, { VPF DELEGATE , VPF DELEGATE, VPF DELEGATE, VPF ERROR}}}},
{State::DONE       , {{objrow, { VPF ERROR    , VPF ERROR   , VPF ERROR   , VPF ERROR}}}},
{State::ERROR      , {{objrow, { VPF ERROR    , VPF ERROR   , VPF ERROR   , VPF ERROR}}}},
};
#undef VPS
#undef VPF
#undef VPE

//FIXME remove
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
    inline void insert(char c, unsigned char rawPos) {
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

        m_Room += count;
        assert(m_Room <= 4);
    }

    // Uses a template to allow static_assert and inlining to work properly
    template<int pos, char room = 99>
    inline constexpr char get() const {
        static_assert(pos < 4 && pos >= 0, "Position must be between 0 and 3");
#ifdef ENABLE_TEST_ASSERTS
        assert(m_Room <= room);
#endif
        return m_lContent[(m_Pos+pos) % 4];
    }

    // Create validated access for each chars. When compiled in release mode
    // it will inline nicely.
    char current () {return get<2   >();}
    char previous() {return get<1   >();}
    char next    () {return get<3, 2>();}
    char last    () {return get<0, 1>();}

    inline bool isActive() const {
        return !m_IsInactive;
    }

    inline char room() const {
        return m_Room;
    }

private:
    //TODO it could be applied to the main buffer instead of being a copy
    char m_lContent[4] = {0x00,0x00,0x00,0x00};

    unsigned char m_Pos {3}; /*!< The previous byte is always necessary, so start at 1*/
    bool m_IsInactive {false};

    char m_Room {0};
};

struct VContext
{
    /**
     * Until it becomes a major bottleneck, copy everything in a buffer.
     *
     * This removes a lot of complexity.
     */
    std::basic_string<char> m_Buffer;

    bool m_IsActive {true};

    /**
     * Objects can be children of other objects, keep a linked list down to the
     * root object. Further management isn't required since it acts as a memento
     * and not much else.
     */
    VObject* m_pCurrentObject { nullptr };

    /**
     * Keep the smallest possible number of VObject instances.
     *
     * That's usually 2 but the spec doesn't limit them.
     */
    std::list<VObject*> m_lObjectCache;
    VObject* acquireObject();
    void releaseObject(VObject* o);

    VProperty m_CurrentProperty {this};

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

    void handleProperty()
    {
        auto o = currentObject();

        if (!o)
            return;

        if (auto handler = propertyHandler(m_CurrentProperty.m_Name))
            (*handler)(
                o->m_pReflected,
                m_CurrentProperty.m_Value,
                m_CurrentProperty.m_Parameters.parameters
            );
        else if (o->m_pAdaptor && o->m_pAdaptor->d_ptr->m_AbstractPropertyHandler)
            o->m_pAdaptor->d_ptr->m_AbstractPropertyHandler(
                o->m_pReflected,
                m_CurrentProperty.m_Name,
                m_CurrentProperty.m_Value,
                m_CurrentProperty.m_Parameters.parameters
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
        assert(m_CurrentProperty.m_Name.size() > 0);

        auto newO = acquireObject();

        newO->m_pParent = m_pCurrentObject;
        newO->name = m_CurrentProperty.m_Value;

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
        else if (auto h = handlerForAdaptor(m_pCurrentObject->m_pAdaptor, oldO->m_pAdaptor))
            h(m_pCurrentObject->m_pReflected, oldO->m_pReflected, oldO->name);

        releaseObject(oldO);

    }

    std::basic_string<char> flush() {
        std::basic_string<char> ret = m_Buffer;
        m_Buffer.clear();
        return ret;
    }

    bool _test_raw(const char* content);

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
    if (m_pContext->m_Window.current() == '\n' || m_pContext->m_Window.current() == '\r') {
        return Event::FINISH;
    }

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

void VParameters::applyActions(VParameters::Action a)
{
    switch(a) {
        case Action::INCREMENT:
            m_pContext->push(1);
            break;

        case Action::SKIP:
            m_pContext->skip(1);
            break;
        case Action::SAVENAME:
            m_CurrentParam.first = m_pContext->flush();
            m_pContext->skip(1);
            break;
        case Action::SAVEVALUE:
            m_CurrentParam.second = m_pContext->flush();

            parameters.push_back(m_CurrentParam);
            assert(m_CurrentParam.second != "\"b");
            m_pContext->skip(1);
            m_CurrentParam.first.clear();
            m_CurrentParam.second.clear();
            break;
        case Action::ERROR:
            assert(false);
            break;
        case Action::NOTHING:
            break;
    }
}

void VProperty::applyActions(VProperty::Action a)
{
    switch(a) {
        case Action::PUSHNAME:
            m_Name = m_pContext->flush();
            m_pContext->skip(1);
            break;
        case Action::INCREMENT:
            m_pContext->push(1);
            break;
        case Action::PARAMETER:
            m_Parameters.applyEvent();
            break;
        case Action::PUSH:
            m_Value = m_pContext->flush();
            m_pContext->skip(m_pContext->m_Window.current() == '\r' ? 2 : 1);
            break;
        case Action::PUSHLINE:
            m_pContext->skip(m_pContext->m_Window.current() == '\r' ? 3 : 2);
            break;
        case Action::NOTHING:
            break;
        case Action::RESET:
            m_Value.clear();
            m_Name.clear();

            //TODO add an explicit action to VParameters and use applyEvent
            m_Parameters.parameters.clear();
            m_Parameters.m_CurrentParam = {};
            m_Parameters.m_State = VParameters::State::EMPTY;

            m_State = VProperty::State::EMPTY;
            break;
        case Action::ERROR:
            assert(false);
            break;
    }
}

void VObject::applyActions(VObject::Action a)
{
    switch(a) {
        case Action::ERROR:
            assert(false);
            break;
        case Action::INCREMENT:
            m_pContext->push(1);
            break;
        case Action::DELEGATE:
            m_pContext->m_CurrentProperty.applyEvent();
            break;
        case Action::FINISH:
            m_pContext->flush();
            m_pContext->popObject();
            break;
        case Action::NOTHING:
            m_pContext->push(1);
            break;
        case Action::PUSH:
            // Helps debugging a lot
            //std::cout << "PUSH " << m_pContext->m_CurrentProperty.m_Name << " ===> "
            //  << m_pContext->m_CurrentProperty.m_Value << std::endl;


            /// Detect when the property is an object
            if (m_pContext->m_CurrentProperty.m_Name.substr(0, 5) == "BEGIN") {
                m_pContext->stashObject();
            }
            else if (m_pContext->m_CurrentProperty.m_Name.substr(0, 3) == "END") {
                m_pContext->popObject();
                // Helps debugging a lot
                //std::cout << "END OBJ" << m_pContext->m_CurrentProperty.m_Name << " ===> "
                //    << m_pContext->m_CurrentProperty.m_Value << std::endl;
            }
            else {
                m_pContext->handleProperty();
            }

            assert(m_pContext->m_CurrentProperty.m_State == VProperty::State::DONE);
            m_pContext->m_CurrentProperty.applyEvent(VProperty::Event::FINISH);
            assert(m_pContext->m_CurrentProperty.m_State == VProperty::State::EMPTY);
            break;
    }
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

    applyActions(m_StateActions[s][event]);
}


void VProperty::applyEvent(VProperty::Event e) //TODO merge both versions
{
    auto s = m_State;

    m_State = m_StateMap[s][e];

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed\n";
        return;
    }

    applyActions(m_StateActions[s][e]);
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
        std::cout << "Parsing failed " << (int)s << " " << (int)event << " : " << m_pContext->m_Buffer << std::endl;
        return;
    }

    applyActions(m_StateActions[s][event]);
}

void VObject::applyEvent()
{
    auto s = m_State;

    Event event = Event::COUNT__;

    if (s == State::PROPERTIES) { //FIXME remove all this
        assert(m_pContext->m_CurrentProperty.m_State != VProperty::State::ERROR);
        event = m_PropertyToEvent[m_pContext->m_CurrentProperty.m_State];
    }
    else
        event = charToEvent();

    assert(event != Event::ERROR);
    m_State = m_StateMap[s][event];

    if (m_State == State::ERROR) {
        std::cout << "Parsing failed" << (int) s << (int) event << std::endl;
        return;
    }

    applyActions(m_StateActions[s][event]);
}

VObject* VContext::acquireObject()
{
    if (m_lObjectCache.size()) {
        auto o = m_lObjectCache.back();
        m_lObjectCache.pop_back();
        return o;
    }
    return new VObject(this);
}

void VContext::releaseObject(VObject* o)
{
    // reset the object
    o->name.clear();
    o->m_pParent = nullptr;
    o->m_State = VObject::State::EMPTY;

    m_lObjectCache.push_back(o);
}

// For unit testing without reading from the disk
bool VContext::_test_raw(const char* content)
{
    int pos = 3;
    m_pCurrentObject = acquireObject();

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
            return true;
        }
    }

    return false;
}

bool VContext::readFile(const char* path)
{
    char c;

    std::ifstream file2(path, std::ios::binary);

    if (!file2)
        return false;

    // Implement an inlined buffer along with __attribute__((always_inline))
    //
    // The first implementation had serious issues with using iostream
    // own buffer due to the overhead of the function call not being
    // inlined reliably (100x too slow, not ~5%). The proper inlining of
    // this type of state machine is absolutely necessary to get any
    // performance out of state machine based parsers.
    char buf[4096];
    int pos(0), eofAt(sizeof(buf) / sizeof(*buf) + 1);

    if (!file2.read(buf, sizeof(buf) / sizeof(*buf))) {
        eofAt = file2.gcount();
        buf[file2.gcount()] = EOF;
    }

    m_pCurrentObject = acquireObject();

    // Init the sliding window
    for (pos = 0; pos < 3; pos++) {
        if ((c = buf[pos]) != EOF)
            m_Window.insert(c, pos+1);
        else {
            file2.close();
            return false;
        }
    }

    assert(!m_Window.previous());

    while (isActive()) {
        while (m_Window.room() && (c = buf[pos]) != EOF) {
            m_Window.put(c);
            if (++pos == sizeof(buf) / sizeof(*buf)) {
                if (!file2.read(buf, sizeof(buf) / sizeof(*buf))) {
                    eofAt = file2.gcount();
                    buf[file2.gcount()] = EOF;
                }
                pos = 0;
            }
        }

        if (c == EOF || !c || pos >= eofAt)
            m_IsActive = false;

        if (!isActive())
            break;

        //std::cout << "char " << buf[pos] << " " << pos << " " << eofAt << std::endl;

        currentObject()->applyEvent();
    }

    file2.close();

    return true;
}

}

bool ICSLoader::loadFile(const char* path)
{
    return d_ptr->readFile(path);
}

ICSLoader::ICSLoader() : d_ptr(new VParser::VContext)
{
}

ICSLoader::~ICSLoader()
{
    delete d_ptr;
}

void ICSLoader::registerVObjectAdaptor(const char* name, std::shared_ptr<AbstractVObjectAdaptor> adaptor)
{
    d_ptr->m_Adaptors[name] = adaptor;
}

void ICSLoader::registerFallbackVObjectAdaptor(std::shared_ptr<AbstractVObjectAdaptor> adaptor)
{
    d_ptr->m_FallbackAdaptors = adaptor;
}

void ICSLoader::_test_CharToObj(const char* data)
{
    std::cout << "In test\n";

    d_ptr->_test_raw(data);
}
