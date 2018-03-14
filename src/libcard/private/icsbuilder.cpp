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
#include "icsbuilder.h"

// StdC++
#include <fstream>
#include <iostream>

// Ring
#include <libcard/event.h>
#include <libcard/calendar.h>
#include <account.h>
#include <contactmethod.h>
#include <person.h>
#include <uri.h>

class ICSBuilderPrivate final
{

};

bool ICSBuilder::toStream(ContactMethod* cm, const QString& name, std::basic_iostream<char>* device)
{
    (*device) << "ATTENDEE;CN=\"";

    (*device) << (
        name.isEmpty() ? cm->bestName() : name
    ).toLatin1().data() <<'\"';

    if (cm->contact()) {
        (*device) << ";UID=" << cm->contact()->uid().data();
    }

    (*device) << ':' << cm->uri().format (
        URI::Section::SCHEME | URI::Section::USER_INFO | URI::Section::HOSTNAME
    ).toLatin1().data() << '\n';

    return true;
}

bool ICSBuilder::toStream(Account* a, std::basic_iostream<char>* device)
{
    (*device) << "ORGANIZER;CN=\"";

    (*device) << (
        a->registeredName().isEmpty() ?
            a->displayName().toLatin1().data() : a->registeredName().toLatin1().data()
    ) <<'\"';

    (*device) << ";X_RING_ACCOUNTID=" << a->id().data() << ':';

    (*device) << a->contactMethod()->uri().format (
        URI::Section::SCHEME | URI::Section::USER_INFO | URI::Section::HOSTNAME
    ).toLatin1().data() << '\n';

    return true;
}

bool ICSBuilder::toStream(Event* e, std::basic_iostream<char>* device)
{

    (*device) << "BEGIN:VEVENT\n";

    (*device) << "UID:" << e->uid().data() << '\n';

    (*device) << "DTSTART;TZID=" << e->timezone()->id().data() << ':' << e->startTimeStamp() << '\n';
    (*device) << "DTEND;TZID=" << e->timezone()->id().data() << ':' << e->stopTimeStamp() << '\n';
    (*device) << "DTSTAMP;TZID=" << e->timezone()->id().data() << ':' << e->revTimeStamp() << '\n';

    // A custom property as defined in rfc5545#section-3.8.4.1
    // In theory it can be reverse engineered from the ORGANIZER/ATTENDEE
    // properties, but that breaks when an account is deleted
    (*device) << "X_RING_DIRECTION;VALUE=STRING:" << (
        e->direction() == Event::Direction::INCOMING ?
            "INCOMING" : "OUTGOING"
    ) << '\n';

    toStream(e->account(), device);

    const auto attendees = e->attendees();

    for (const auto pair : qAsConst(attendees))
        toStream(pair.first, pair.second, device);

    (*device) << "END:VEVENT\n";

    return true;
}

bool ICSBuilder::toStream(const QTimeZone* tz, std::basic_iostream<char>* device)
{
    //FIXME implement the format properly

    (*device) << "BEGIN:VTIMEZONE\n";
    (*device) << "TZID:" << tz->id().data() << '\n';
    (*device) << "END:VTIMEZONE\n";

    return false;
}

bool ICSBuilder::toStream(Calendar* cal, std::basic_iostream<char>* device)
{
    (*device) << "BEGIN:VCALENDAR\n";

    // First, add all the timezones

    const auto timezones = cal->timezones();

    for (const auto tz : qAsConst(timezones))
        toStream(tz, device);



    (*device) << "END:VCALENDAR\n";
}

bool ICSBuilder::save(Calendar* cal, std::function<void(bool)> cb)
{
    std::fstream fs(cal->path().toLatin1(),
        std::fstream::out
    );

    if (!fs.good())
        return false;

    toStream(cal, &fs);

    fs.close();
}

void ICSBuilder::appendToCalendar(Calendar* cal, Event* event, std::function<void(bool)> cb)
{

    // If the file does not exist, do a full save
    {
        std::ifstream infile(cal->path().toLatin1());
        if (!infile.good()) {
            save(cal, [event, cb, cal](bool success) {
                if (!success)
                    cb(false);
                else if (!event->isSaved())
                    appendToCalendar(cal, event, cb);
                else
                    cb(true);
            });
            return;
        }
    }

    std::fstream fs(cal->path().toLatin1(),
        std::fstream::binary |
        std::fstream::in     |
        std::fstream::out
    );

    if (!fs.good()) {
        cb(false);
        return;
    }

    static int len = -strlen("\nEND:VCALENDAR");

    // Move before the END:VCALENDAR
    fs.seekp(len, std::ios_base::end); //TODO actually check if it's right

    toStream(event, &fs);

    fs << "END:VCALENDAR\n";

    fs.close();
}
