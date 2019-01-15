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

#include <QtCore/QByteArray>

// Qt
class QTextStream;
class QTimeZone;
#include <QtCore/QSharedPointer>

// Ring
class Event;
class Calendar;
class Account;
class ContactMethod;

namespace Media {
    class Attachment;
}

// StdC++
#include <functional>
#include <iosfwd>

/**
 * Convert an event into a serialized VCALENDAR:VEVENT or a full VCALENDAR
 */
class ICSBuilder
{
public:

    /**
     * Some operations, such as MIME attachments are differed, therefor keeping
     * a context is necessary.
     *
     * Some modes, such as synchronization versus local storage also affect
     * the output.
     */
    class Context {

    };

    static bool toStream(Event* e, std::basic_iostream<char>* device);
    static bool toStream(const QTimeZone* tz, std::basic_iostream<char>* device);
    static bool toStream(Calendar* cal, std::basic_iostream<char>* device);
    static bool toStream(ContactMethod* cm, const QString& name, std::basic_iostream<char>* device);
    static bool toStream(Account* a, std::basic_iostream<char>* device);
    static bool toStream(Media::Attachment* file, std::basic_iostream<char>* device);

    static bool save(Calendar* cal, std::function<void(bool)> cb = nullptr);
    static bool create(Calendar* cal, std::function<void(bool)> cb = nullptr);
    static bool rebuild(Calendar* cal, std::function<void(bool)> cb = nullptr);
    static void append(Calendar* cal, Event* event, std::function<void(bool)> cb);

};
