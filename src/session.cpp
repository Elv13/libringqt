/************************************************************************************
 *   Copyright (C) 2019 by BlueSystems GmbH                                         *
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
#include "session.h"

#include "callmodel.h"
#include "accountmodel.h"
#include "categorizedhistorymodel.h"
#include "categorizedcontactmodel.h"
#include "categorizedbookmarkmodel.h"
#include "availableaccountmodel.h"
#include "namedirectory.h"
#include "peerstimelinemodel.h"

Session::Session(QObject* parent) : QObject(parent)
{}

Session::~Session()
{}

Session* Session::instance()
{
    static Session s(QCoreApplication::instance());

    return &s;
}

CallModel* Session::callModel() const
{
    static CallModel m;
    m.init();

    return &m;
}

AccountModel* Session::accountModel() const
{
    static AccountModel m;
    m.init();

    return &m;
}

CategorizedHistoryModel* Session::historyModel() const
{
    static CategorizedHistoryModel m;

    return &m;
}

CategorizedContactModel* Session::contactModel() const
{
    static CategorizedContactModel m;

    return &m;
}

CategorizedBookmarkModel* Session::bookmarkModel() const
{
    static CategorizedBookmarkModel m(const_cast<Session*>(this));

    return &m;
}

AvailableAccountModel* Session::availableAccountModel() const
{
    static AvailableAccountModel m;

    return &m;
}

NameDirectory* Session::nameDirectory() const
{
    static NameDirectory m;

    return &m;
}

PeersTimelineModel* Session::peersTimelineModel() const
{
    static PeersTimelineModel m;

    return &m;
}
