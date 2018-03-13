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

class ICSLoaderPrivate;

/**
 * Raw C++14 iCalendar and vCard file parser implemented as a finite state
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
        SupportedProperties type;
        std::basic_string<char> name;
        Parameters parameters;
    };

    /**
     * The object type this parser cares about.
     *
     * For now, other objects such as timeline, todo or alarms are ignored.
     */
    enum class ObjectType {
        V_CALENDAR, /*!< A calendar with multiple event/todo/journal */
        V_CARD    , /*!< A contact card                              */
        V_EVENT   , /*!< An active event (**WITH** busy time)        */
        V_JOURNAL , /*!< An passive event (**WITHOUT** busy time)    */
    };

    class AbstractObject {
        std::list<Property*> properties;
    };

    explicit ICSLoader(const char* path, bool recursive = false);

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
     * Get the size without blocking the parser.
     */
    std::atomic<unsigned> transerQueueSize() const;

    /**
     * Get the transfer queue.
     *
     * Note that holding the point prevent the parser from adding elements
     * to the queue, but it has it's own buffer, so accessing this in a
     * `while (transerQueueSize()) { auto q = p->acquireTrasferQueue();}` is a
     * good idea.
     */
    std::unique_ptr< std::list< AbstractObject* > > acquireTrasferQueue() const;

    AbstractObject* _test_CharToObj(const char* data);

private:
    ICSLoaderPrivate* d_ptr;
};
