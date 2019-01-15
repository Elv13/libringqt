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
#include "callhistorymodel.h"
#include "contactmodel.h"
#include "bookmarkmodel.h"
#include "availableaccountmodel.h"
#include "namedirectory.h"
#include "peerstimelinemodel.h"
#include "numbercategorymodel.h"
#include "individualdirectory.h"
#include "profilemodel.h"
#include "presencestatusmodel.h"
#include "eventmodel.h"
#include "ringtonemodel.h"
#include "media/recordingmodel.h"
#include "video/previewmanager.h"
#include "persondirectory.h"
#include "infotemplatemanager.h"
#include "numbercompletionmodel.h"

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

CallHistoryModel* Session::historyModel() const
{
    static CallHistoryModel m;

    return &m;
}

ContactModel* Session::contactModel() const
{
    static ContactModel m;

    return &m;
}

BookmarkModel* Session::bookmarkModel() const
{
    static BookmarkModel m(const_cast<Session*>(this));

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

NumberCategoryModel* Session::numberCategoryModel() const
{
    static NumberCategoryModel m;

    return &m;
}

IndividualDirectory* Session::individualDirectory() const
{
    static IndividualDirectory m;

    return &m;
}

ProfileModel* Session::profileModel() const
{
    static ProfileModel m;

    return &m;
}

PresenceStatusModel* Session::presenceStatusModel() const
{
    static PresenceStatusModel m;

    return &m;
}

EventModel* Session::eventModel() const
{
    static EventModel m;

    return &m;
}

RingtoneModel* Session::ringtoneModel() const
{
    static RingtoneModel m;

    return &m;
}

Media::RecordingModel* Session::recordingModel() const
{
    static Media::RecordingModel m(const_cast<Session*>(this));

    return &m;
}

PersonDirectory* Session::personDirectory() const
{
    static PersonDirectory m;

    return &m;
}

Video::PreviewManager* Session::previewManager() const
{
    static Video::PreviewManager m;

    return &m;
}

InfoTemplateManager* Session::infoTemplateManager() const
{
    static InfoTemplateManager m;

    return &m;
}

NumberCompletionModel* Session::numberCompletionModel() const
{
    static NumberCompletionModel m;

    return &m;
}

QAbstractItemModel* Session::sortedContactModel() const
{
    return ContactModel::SortedProxy::instance().model();
}

QAbstractItemModel* Session::contactCategoryModel() const
{
    return ContactModel::SortedProxy::instance().categoryModel();
}

QItemSelectionModel* Session::contactCategorySelectionModel() const
{
    return ContactModel::SortedProxy::instance().categorySelectionModel();
}
