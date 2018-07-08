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
    (*device) << "ATTACH;FMTTYPE=" << file->mimeType()->name().toStdString();

    const auto path = file->path().toString().toStdString();

    (*device) << ";X_RING_ROLE=\"" << file->role().toStdString() << "\":" << path << '\n';

    return true;
}

/**
 * Serialize all relevant secondary keys of the ContactMethod so the directory
 * can recreate it.
 */
bool ICSBuilder::toStream(ContactMethod* cm, const QString& name, std::basic_iostream<char>* device)
{
    (*device) << "ATTENDEE";

    // Don't waste space with redundant parameters
    if (name != cm->uri().userinfo())
        (*device) << ";CN=\"" <<(
            name.isEmpty() ? cm->bestName() : name
        ).toStdString() <<'\"';

    if (cm->contact() && !cm->contact()->uid().isEmpty())
        (*device) << ";UID=" << cm->contact()->uid().toStdString();

    (*device) << ':' << cm->uri().format (
        URI::Section::SCHEME | URI::Section::USER_INFO | URI::Section::HOSTNAME
    ).toStdString() << '\n';

    return true;
}

bool ICSBuilder::toStream(Account* a, std::basic_iostream<char>* device)
{
    (*device) << "ORGANIZER;CN=\"";

    (*device) << (
        a->registeredName().isEmpty() ?
            a->displayName().toStdString() : a->registeredName().toStdString()
    ) <<'\"';

    (*device) << ";X_RING_ACCOUNTID=" << a->id().toStdString() << ':';
    const auto uri = a->contactMethod()->uri().format (
        URI::Section::SCHEME | URI::Section::USER_INFO | URI::Section::HOSTNAME
    );

    (*device) << a->contactMethod()->uri().format (
        URI::Section::SCHEME | URI::Section::USER_INFO | URI::Section::HOSTNAME
    ).toStdString() << '\n';

    return true;
}

bool ICSBuilder::toStream(Event* e, std::basic_iostream<char>* device)
{
    // This is a current convention in libringqt to discard
    if (e->status() == Event::Status::CANCELLED) {
        return false;
    }

    (*device) << "BEGIN:" << Event::typeName(e->type()).toStdString() <<"\n";

    (*device) << "UID:" << e->uid().toStdString() << '\n';
    (*device) << "CATEGORIES:" << Event::categoryName(e->eventCategory()).toStdString() << '\n';

    const auto tzid = e->timezone()->id().toStdString();

    (*device) << "DTSTART;TZID=" << tzid << ':' << e->startTimeStamp() << '\n';
    (*device) << "DTEND;TZID=" << tzid << ':' << e->stopTimeStamp() << '\n';
    (*device) << "DTSTAMP;TZID=" << tzid << ':' << e->revTimeStamp() << '\n';

    // A custom property as defined in rfc5545#section-3.8.4.1
    // In theory it can be reverse engineered from the ORGANIZER/ATTENDEE
    // properties, but that breaks when an account is deleted
    (*device) << "X_RING_DIRECTION;VALUE=STRING:" << (
        e->direction() == Event::Direction::INCOMING ?
            "INCOMING" : "OUTGOING"
    ) << '\n';

    (*device) << "STATUS:" << Event::statusName(e->status()).toStdString() << '\n';

    toStream(e->account(), device);

    const auto attendees = e->attendees();

    for (const auto pair : qAsConst(attendees))
        toStream(pair.first, pair.second, device);

    const auto attachedFiles = e->attachedFiles();

    for (const auto file : qAsConst(attachedFiles))
        toStream(file, device);

    (*device) << "END:" << Event::typeName(e->type()).toStdString() <<"\n";

    return true;
}

bool ICSBuilder::toStream(const QTimeZone* tz, std::basic_iostream<char>* device)
{
    //FIXME implement the format properly, this is the bare minimum to be parsed
    // by third party products

    (*device) << "BEGIN:VTIMEZONE\n";
    (*device) << "TZID:" << tz->id().toStdString() << '\n';
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

    Q_ASSERT(device->good());

    (*device) << "END:VCALENDAR\n";

    return true;
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
bool ICSBuilder::create(Calendar* cal, std::function<void(bool)> cb)
{
    std::fstream fs(cal->path().toLatin1(),
        std::fstream::out
    );

    if (!fs.good()) {
        if (cb)
            cb(false);
        return false;
    }

    toStream(cal, &fs);

    if (cb)
        cb(true);

    fs.close();

    return true;
}

bool ICSBuilder::save(Calendar* cal, std::function<void(bool)> cb)
{
    // If it doesn't exist, build it
    {
        std::ifstream infile(cal->path().toLatin1());
        if (!infile.good())
            return rebuild(cal);
    }

    // If there is nothing to save, then don't
    if (!cal->unsavedCount()) {
        if (cb)
            cb(true);
        return true;
    }

    std::fstream fs(cal->path().toLatin1(),
        std::fstream::binary |
        std::fstream::in     |
        std::fstream::out
    );

    if (!fs.good()) {
        if (cb)
            cb(false);
        return false;
    }

    static int len = -((int) strlen("\nEND:VCALENDAR"));

    // Move before the END:VCALENDAR
    fs.seekp(len, std::ios_base::end); //TODO actually check if it's right

    const auto unsaved = cal->unsavedEvents();


    for (auto e : qAsConst(unsaved)) {
        toStream(e.data(), &fs);
    }

    fs << "END:VCALENDAR\n";

    fs.close();

    if (cb)
        cb(true);

    return true;
}

/**
 * Remove the footer of the previous calendar and overwrite with new content.
 *
 * This is more efficient than rewriting the file everytime. Do not use this
 * for synchronization, use DAV objects.
 */
void ICSBuilder::append(Calendar* cal, Event* event, std::function<void(bool)> cb)
{

    // If the file does not exist, do a full save
    {
        std::ifstream infile(cal->path().toLatin1());
        if (!infile.good()) {
            create(cal, [event, cb, cal](bool success) {
                if (!success)
                    cb(false);
                else if (!event->isSaved())
                    append(cal, event, cb);
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

    static int len = -((int) strlen("\nEND:VCALENDAR"));

    // Move before the END:VCALENDAR
    fs.seekp(len, std::ios_base::end); //TODO actually check if it's right

    toStream(event, &fs);

    fs << "END:VCALENDAR\n";

    fs.close();
}

/**
 * Replace the old file with a new file instead of using an append-only strategy.
 *
 * This is important for 2 reasons:
 *
 *  * Faster load time because the file database is more compact
 *  * Lower CPU because there is less elements
 *  * Lower CPU because elements are indexed and therefor use the "fast path"
 *    instead of sorting at runtime
 *  * Waste less of the limited mobile device local storage
 *  * Improve load time
 *
 * Note that this is I/O and CPU intensive and should be avoided if the file is
 * already mostly clean.
 */
bool ICSBuilder::rebuild(Calendar* cal, std::function<void(bool)> cb)
{
    if (!create(cal, nullptr))
        return false;

    std::fstream fs(cal->path().toLatin1(),
        std::fstream::binary |
        std::fstream::in     |
        std::fstream::out
    );

    if (!fs.good()) {
        if (cb)
            cb(false);
        return false;
    }

    static int len = -((int) strlen("\nEND:VCALENDAR"));

    // Move before the END:VCALENDAR
    fs.seekp(len, std::ios_base::end); //TODO actually check if it's right

    for (int i = 0; i < cal->size(); i++) {
        if (auto e = cal->eventAt(i)) {
            toStream(e.data(), &fs);
        }
    }

    fs << "END:VCALENDAR\n";

    fs.close();

    if (cb)
        cb(true);

    return true;
}
