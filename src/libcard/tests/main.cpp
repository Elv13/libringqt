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

#include <icsloader.h>

/**
 * This is a basic example
 */
static constexpr const char fromRfc[] = \
"BEGIN:VCALENDAR\r\n"
"PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
"VERSION:2.0\r\n"
"BEGIN:VEVENT\r\n"
"DTSTAMP:19960704T120000Z\r\n"
"UID:uid1@example.com\r\n"
"ORGANIZER:mailto:jsmith@example.com\r\n"
"DTSTART:19960918T143000Z\r\n"
"DTEND:19960920T220000Z\r\n"
"STATUS:CONFIRMED\r\n"
"CATEGORIES:CONFERENCE\r\n"
"SUMMARY:Networld+Interop Conference\r\n"
"DESCRIPTION:Networld+Interop Conference\r\n"
"  and Exhibit\\nAtlanta World Congress Center\\n\r\n"
" Atlanta\\, Georgia\r\n"
"END:VEVENT\r\n"
"END:VCALENDAR\r\n";


/**
 * This test the ',' ';' and ':' escaping
 */
static constexpr const char fromRfc2[] = \
"BEGIN:VCALENDAR\r\n"
"PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
"VERSION:2.0\r\n"
"BEGIN:VEVENT\r\n"
"DESCRIPTION;foo=bar;foo2=\"bar\";foo3=b\\;\\:ar:Networld+Interop \r\n"
"  and Exhibit\\nAtlanta World Congress Center\\n\r\n"
" Atlanta\\, Georgia\r\n"
"END:VEVENT\r\n"
"END:VCALENDAR\r\n";

/**
 * This test the windows and macOS9 line ending
 */
static constexpr const char fromRfc3[] = \
"BEGIN:VCALENDAR\r\n"
"PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
"VERSION:2.0\r\n"
"BEGIN:VEVENT\r\n"
"DESCRIPTION;foo=bar;foo2=\"bar\";foo3=b\\;\\:ar:Networld+Interop \r\n"
"  and Exhibit\\nAtlanta World Congress Center\\n\r\n"
" Atlanta\\, Georgia\r\n"
"END:VEVENT\r\n"
"END:VCALENDAR\r\n";

/**
 * Early batch of tests for the ICS loader and builder.
 *
 * They are not yet real unit tests, that will come later.
 */
int main()
{
    ICSLoader loader("");
    loader._test_CharToObj(fromRfc);
    return 0;
}
