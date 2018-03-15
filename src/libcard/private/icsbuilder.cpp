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

// Qt
#include <QtCore/QUrl>
#include <QtCore/QMimeType>

// StdC++
#include <fstream>
#include <iostream>

// Ring
#include <libcard/event.h>
#include <libcard/calendar.h>
#include <account.h>
#include <contactmethod.h>
#include <person.h>
#include <media/attachment.h>
#include <uri.h>

class ICSBuilderPrivate final
{

};


/**
 * Attach files to the event. For network transparent transfers, it should be
 * embedded as a MIME payload while for local or network assets encoded into
 * an URI.
 *
 * ==Variant 1 (embedded files)==
 *
 *    PROPERTY  Ring role (custom)                    MIME ATTACHMENT ID
 *       |             |                                      |
 *    /-----\/--------------------\/------------------------------------------------\
 *    ATTACH;X_RING_ROLE=recording:CID:jsmith.part3.960817T083000.xyzMail@example.com
 *
 * ==Variant 2 (URIs)==
 *
 *   PROPERTY            MIME type               Ring role (custom)        URI
 *       |                  |                           |                   |
 *    /----\/------------------------------\/--------------------\/------------------\
 *    ATTACH;FMTTYPE=application/postscript/;X_RING_ROLE=recording:file://ring.cx/f.ps
 *
 * @ref https://tools.ietf.org/html/rfc5545#section-3.8.1.1
 */
bool ICSBuilder::toStream(Media::Attachment* file, std::basic_iostream<char>* device)
{
    (*device) << "ATTACH;FMTTYPE=" << file->mimeType()->name().data();

    const char* path = file->path().toString().toLatin1().data();

    (*device) << ";X_RING_ROLE=\"" << file->role().data() << "\":" << path << '\n';
}

/**
 * Serialize all relevant secondary keys of the ContactMethod so the directory
 * can recreate it.
 */
bool ICSBuilder::toStream(ContactMethod* cm, const QString& name, std::basic_iostream<char>* device)
{
    (*device) << "ATTENDEE;CN=\"";

    (*device) << (
        name.isEmpty() ? cm->bestName() : name
    ).toLatin1().data() <<'\"';

    if (cm->contact() && !cm->contact()->uid().isEmpty())
        (*device) << ";UID=" << cm->contact()->uid().data();

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
    (*device) << "CATEGORIES:" << Event::categoryName(e->eventCategory()).data() << '\n';

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

    const auto attachedFiles = e->attachedFiles();

    for (const auto file : qAsConst(attachedFiles))
        toStream(file, device);

    (*device) << "END:VEVENT\n";

    return true;
}

bool ICSBuilder::toStream(const QTimeZone* tz, std::basic_iostream<char>* device)
{
    //FIXME implement the format properly, this is the bare minimum to be parsed
    // by third party products

    (*device) << "BEGIN:VTIMEZONE\n";
    (*device) << "TZID:" << tz->id().data() << '\n';
    (*device) << "END:VTIMEZONE\n";

    return false;
}

/**
 * Create a basic VCALENDAR wrapper for the VEVENTs (and other objects)
 */
bool ICSBuilder::toStream(Calendar* cal, std::basic_iostream<char>* device)
{
    (*device) << "BEGIN:VCALENDAR\n";
    (*device) << "VERSION:2.0\n";

    //TODO add the PRODID:

    // First, add all the timezones

    const auto timezones = cal->timezones();

    for (const auto tz : qAsConst(timezones))
        toStream(tz, device);



    (*device) << "END:VCALENDAR\n";
}

/**
 * Overwrite the whole calendar.
 *
 * This is sometime a good idea and sometime a bad one. The spec support
 * appending "diff" with different DTSTAMP to update existing events.
 *
 * After a long time, it might be a good idea to squash everything back into
 * single VEVENTs instead of keeping multiple updated versions.
 */
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

/**
 * Remove the footer of the previous calendar and overwrite with new content.
 *
 * This is more efficient than rewriting the file everytime. Do not use this
 * for synchronization, use DAV objects.
 */
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
