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
#pragma once

// Qt
#include <QtCore/QObject>
class QAbstractItemModel;
class QItemSelectionModel;

#include <typedefs.h>

// LibRingQt
class CallModel;
class AccountModel;
class CallHistoryModel;
class ContactModel;
class BookmarkModel;
class AvailableAccountModel;
class NameDirectory;
class PeersTimelineModel;
class NumberCategoryModel;
class IndividualDirectory;
class ProfileModel;
class PresenceStatusModel;
class EventModel;
class RingtoneModel;
class PersonDirectory;
class InfoTemplateManager;
class NumberCompletionModel;
class RecentFileModel;

namespace Media {
class RecordingModel;
}

namespace Video {
class PreviewManager;
class DeviceModel;
}

class SessionPrivate;

/**
 * All uncreatable single instance objects of the base namespace.
 */
class LIB_EXPORT Session final : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(CallModel* callModel READ callModel CONSTANT)
    Q_PROPERTY(AccountModel* accountModel READ accountModel CONSTANT)
    Q_PROPERTY(CallHistoryModel* historyModel READ historyModel CONSTANT)
    Q_PROPERTY(ContactModel* contactModel READ contactModel CONSTANT)
    Q_PROPERTY(BookmarkModel* bookmarkModel READ bookmarkModel CONSTANT)
    Q_PROPERTY(AvailableAccountModel* availableAccountModel READ availableAccountModel CONSTANT)
    Q_PROPERTY(NameDirectory* nameDirectory READ nameDirectory CONSTANT)
    Q_PROPERTY(PeersTimelineModel* peersTimelineModel READ peersTimelineModel CONSTANT)
    Q_PROPERTY(NumberCategoryModel* numberCategoryModel READ numberCategoryModel CONSTANT)
    Q_PROPERTY(ProfileModel* profileModel READ profileModel CONSTANT)
    Q_PROPERTY(PresenceStatusModel* presenceStatusModel READ presenceStatusModel CONSTANT)
    Q_PROPERTY(EventModel* eventModel READ eventModel CONSTANT)
    Q_PROPERTY(IndividualDirectory* individualDirectory READ individualDirectory CONSTANT)
    Q_PROPERTY(RingtoneModel* ringtoneModel READ ringtoneModel CONSTANT)
    Q_PROPERTY(Media::RecordingModel* recordingModel READ recordingModel CONSTANT)
    Q_PROPERTY(Video::PreviewManager* previewManager READ previewManager CONSTANT)
    Q_PROPERTY(InfoTemplateManager* infoTemplateManager READ infoTemplateManager CONSTANT)
    Q_PROPERTY(NumberCompletionModel* numberCompletionModel READ numberCompletionModel CONSTANT)
    Q_PROPERTY(RecentFileModel* recentFileModel READ recentFileModel CONSTANT)
    Q_PROPERTY(Video::DeviceModel* deviceModel READ deviceModel CONSTANT)

    Q_PROPERTY(QAbstractItemModel* contactCategoryModel READ contactCategoryModel CONSTANT)
    Q_PROPERTY(QItemSelectionModel* contactCategorySelectionModel READ contactCategorySelectionModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* sortedContactModel READ sortedContactModel CONSTANT)

    CallModel*    callModel   () const;
    AccountModel* accountModel() const;
    CallHistoryModel* historyModel() const;
    ContactModel* contactModel() const;
    BookmarkModel* bookmarkModel() const;
    AvailableAccountModel* availableAccountModel() const;
    NameDirectory* nameDirectory() const;
    PeersTimelineModel* peersTimelineModel() const;
    NumberCategoryModel* numberCategoryModel() const;
    IndividualDirectory* individualDirectory() const;
    ProfileModel* profileModel() const;
    PresenceStatusModel* presenceStatusModel() const;
    EventModel* eventModel() const;
    RingtoneModel* ringtoneModel() const;
    Media::RecordingModel* recordingModel() const;
    PersonDirectory* personDirectory() const;
    Video::PreviewManager* previewManager() const;
    InfoTemplateManager* infoTemplateManager() const;
    NumberCompletionModel* numberCompletionModel() const;
    RecentFileModel* recentFileModel() const;
    Video::DeviceModel* deviceModel() const;

    QAbstractItemModel* contactCategoryModel() const;
    QItemSelectionModel* contactCategorySelectionModel() const;
    QAbstractItemModel* sortedContactModel() const;

    static Session* instance();

private:
    explicit Session(QObject* parent = nullptr);
    virtual ~Session();

    SessionPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Session)
};
