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
#pragma once

#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <string>
#include <list>
#include <functional>

class AbstractVObjectAdaptorPrivate;

// pimpl
namespace VParser
{
    struct VContext;
}

/**
 * This is the type independant API and should not be used directly.
 *
 * Use `VObjectAdapter` for everything as it takes care of the casting.
 */
class AbstractVObjectAdaptor
{
    friend struct VParser::VContext;
public:
    using Parameters = std::list< std::pair<std::basic_string<char>, std::basic_string<char> > >;
    //

protected:
    explicit AbstractVObjectAdaptor(int typeId);

    void setAbstractFactory(std::function<void*(const std::basic_string<char>& object_type)> f);
    void setAbstractPropertyHandler(const char* name, std::function<void(void* p, const std::basic_string<char>& value, const Parameters& params)> handler);

    void setAbstractFallbackPropertyHandler(std::function<
        void(void* p, const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params)
    > handler);

    void setAbstractFallbackObjectHandler(int typeId, std::function<
        void(
            void* p,
            void* p2,
            const std::basic_string<char>& name
        )
    > handler);

    static int getNewTypeId();

private:
    AbstractVObjectAdaptorPrivate* d_ptr;
};

/**
 * Instead of storing temporary copies of everything, the ICSLoader will
 * delegate copying to the VObjectAdapter. This is both more efficient and
 * more flexible.
 */
template<typename T>
class VObjectAdapter : public AbstractVObjectAdaptor
{
public:
    VObjectAdapter() : AbstractVObjectAdaptor(getTypeId()){}

    /**
     * Set the function that will be called after BEGIN.
     *
     * About the design. Not having a context or a parent is intentional. Even
     * if you "real" object is more complex, the structure used to reflect
     * should be kept simple and stateless. The main object can act as a proxy
     * to that object.
     */
    void setObjectFactory(std::function<T*(const std::basic_string<char>& object_type)> factory);

    void addPropertyHandler(const char* name, std::function<void(T* self, const std::basic_string<char>& value, const Parameters& params)> handler);
    void setFallbackPropertyHandler(std::function<void(T* self, const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params)> handler);

    template<typename T2 = T>
    void addObjectHandler(const char* name, std::shared_ptr< VObjectAdapter<T2> > handler);

    template<typename T2 = T>
    void setFallbackObjectHandler(std::function<void(T* self, T2* object, const std::basic_string<char>& name)> handler);

    ///@warning Do **not** rely this in a symbol exported in a shared library
    static int getTypeId();
private:
};

/**
 * Raw C++14 iCalendar and vCard stream parser implemented as a finite state
 * machine. This subsystems aims at being able to support local assets loading
 * and eventually remote assets synchronization. It also aims at being easy to
 * unit test.
 *
 * The reason this class exposes a "stream" API instead of a one off job is to
 * accommodate rfc4791/rfc6352/rfc6764.
 *
 * Some pipedream future feature might be multi-process sandboxing using SHM or
 * other pointless bullet points to make the feature list impressive.
 *
 * It run in an std::tread with a callback when completed.
 *
 * As for the horrifying complexity and "why don't you use `string.split()` and
 * move along?". The reason is that the RFC is full of ways to shoot yourself in
 * the foot. That includes, among other thing, it's own way of encoding UTF8
 * characters across multiple lines and a bunch of alternate way to escape or
 * reference values. It could have been implemented as a pipeline and multiple
 * parsing passes to get it right or just ignore the corner case and go for the
 * "hope for the best, my data is simple" approach. However the finite state
 * machine implementation is more future proof that either of those solutions.
 *
 * Qt isn't necessary or beneficial for this beside a few char* to QString
 * conversion later on.
 */
class ICSLoader
{
public:
    using Parameters = std::vector<
        std::pair <std::basic_string<char>, std::basic_string<char>>
    >;

    /**
     * The property payload encoding.
     *
     * TEXT is always expected to be utf8 even if the RFC does not require it.
     *
     * @ref https://tools.ietf.org/html/rfc4648
     */
    enum class Encodings {
        TEXT       ,
        BASE64     ,
        BASE64_JPEG,
        BASE64_PNG ,
        BINARY     ,
    };

    /**
     * The list of properties handled in custom ways by the ::Event class.
     *
     * The parser must provide this field to make the conversion from
     * ::AbstractEvent to ::Event fast so it can be done safely in the main
     * thread.
     */
    enum class SupportedProperties {
        OTHER = 0, /*!< Anything that isn't explicitly used */
        UID      ,
        ATTACH   ,
        DTSTAMP  ,
        DTSTART  ,
        DTEND    ,
    };

    class Property {
        //SupportedProperties type; //TODO
        std::basic_string<char> name;
        Parameters parameters;
    };

    class AbstractObject {
        std::list<Property*> properties;
    };

    explicit ICSLoader();

    /**
     * @warning This blocks until the thread quits
     */
    virtual ~ICSLoader();

    /**
     * The parser runs in a thread and communicate using message passing and
     * callbacks.
     */
    enum class ParserStatus {
        READY            , /*!< The file is open and ready to be read         */
        FILE_ACCESS_ERROR, /*!< Opening the file failed                       */
        PARSING          , /*!< It is current reading and parsing data        */
        IDLE             , /*!< The parser is waiting for data (DAV only)     */
        FINISHED         , /*!< There is no more data and everything is parsed*/
        TERMINATED       , /*!< The data has been freed                       */
        ERROR            , /*!< */
    };

    /**
     * Message passed to the parsing thread.
     *
     * Note that getting the transfer queue automatically send the LOCK/UNLOCK
     * messages.
     */
    enum class ParserActions {
        START , /*!< Begin reading and parse the file                   */
        CANCEL, /*!< Stop the parsing, quit the thread                  */
        LOCK  , /*!< Stop adding new entries to the data transfer queue */
        UNLOCK, /*!< Resume adding new entries to the transfer queue    */
    };

    /**
     * For each object name (VCARD, VCALENDAR, VEVENT, etc), set an adaptor
     * to map external object to the serialized representation.
     */
    void registerVObjectAdaptor(const char* name, std::shared_ptr<AbstractVObjectAdaptor> adaptor);

    /**
     * When an object name isn't registered, this handler will be used.
     */
    void registerFallbackVObjectAdaptor(std::shared_ptr<AbstractVObjectAdaptor> adaptor);

    /**
     * Open and parse a local file.
     *
     * This function is blocking and should be called from a thread.
     *
     * @return If the file was readable and non-empty.
     */
    bool loadFile(const char* path);

    void _test_CharToObj(const char* data);

private:
    VParser::VContext* d_ptr;
};

#include "icsloader.hpp"
