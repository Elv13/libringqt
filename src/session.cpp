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
#include "recentfilemodel.h"
#include "video/devicemodel.h"

class SessionPrivate {
public:
    CallModel*             m_pCallModel             {nullptr};
    AccountModel*          m_pAccountModel          {nullptr};
    CallHistoryModel*      m_pHistoryModel          {nullptr};
    ContactModel*          m_pContactModel          {nullptr};
    BookmarkModel*         m_pBookmarkModel         {nullptr};
    AvailableAccountModel* m_pAvailableAccountModel {nullptr};
    NameDirectory*         m_pNameDirectory         {nullptr};
    PeersTimelineModel*    m_pPeersTimelineModel    {nullptr};
    NumberCategoryModel*   m_pNumberCategoryModel   {nullptr};
    IndividualDirectory*   m_pIndividualDirectory   {nullptr};
    ProfileModel*          m_pProfileModel          {nullptr};
    PresenceStatusModel*   m_pPresenceStatusModel   {nullptr};
    EventModel*            m_pEventModel            {nullptr};
    RingtoneModel*         m_pRingtoneModel         {nullptr};
    Media::RecordingModel* m_pRecordingModel        {nullptr};
    PersonDirectory*       m_pPersonDirectory       {nullptr};
    Video::PreviewManager* m_pPreviewManager        {nullptr};
    InfoTemplateManager*   m_pInfoTemplateManager   {nullptr};
    NumberCompletionModel* m_pNumberCompletionModel {nullptr};
    RecentFileModel*       m_pRecentFileModel       {nullptr};
    Video::DeviceModel*    m_pDeviceModel           {nullptr};
};

// The code was too repetitive, changing one detail meant copy/pasting the
// same change too many time.
#define ACCESS(type, prop, name, parent) \
    type* Session::prop() const {\
    if (d_ptr->name) \
        return d_ptr->name;\
    \
    d_ptr->name = new type();\
    d_ptr->name->setParent((QObject*) Session:: parent ());\
    return d_ptr->name;}

Session::Session(QObject* parent) : QObject(parent), d_ptr(new SessionPrivate())
{}

Session::~Session()
{
    delete d_ptr;
}

Session* Session::instance()
{
    static Session* s = nullptr;
    s = s ? s : new Session(QCoreApplication::instance());
    return s;
}

/*          CLASS                  SINGLETON                ATTRIBUTE               PARENT       */
ACCESS(CallModel            , callModel            , m_pCallModel            , accountModel       )
ACCESS(AccountModel         , accountModel         , m_pAccountModel         , instance           )
ACCESS(CallHistoryModel     , historyModel         , m_pHistoryModel         , individualDirectory)
ACCESS(ContactModel         , contactModel         , m_pContactModel         , individualDirectory)
ACCESS(BookmarkModel        , bookmarkModel        , m_pBookmarkModel        , individualDirectory)
ACCESS(AvailableAccountModel, availableAccountModel, m_pAvailableAccountModel, accountModel       )
ACCESS(NameDirectory        , nameDirectory        , m_pNameDirectory        , instance           )
ACCESS(PeersTimelineModel   , peersTimelineModel   , m_pPeersTimelineModel   , individualDirectory)
ACCESS(NumberCategoryModel  , numberCategoryModel  , m_pNumberCategoryModel  , instance           )
ACCESS(IndividualDirectory  , individualDirectory  , m_pIndividualDirectory  , callModel          )
ACCESS(ProfileModel         , profileModel         , m_pProfileModel         , accountModel       )
ACCESS(PresenceStatusModel  , presenceStatusModel  , m_pPresenceStatusModel  , accountModel       )
ACCESS(EventModel           , eventModel           , m_pEventModel           , instance           )
ACCESS(RingtoneModel        , ringtoneModel        , m_pRingtoneModel        , instance           )
ACCESS(Media::RecordingModel, recordingModel       , m_pRecordingModel       , instance           )
ACCESS(PersonDirectory      , personDirectory      , m_pPersonDirectory      , individualDirectory)
ACCESS(Video::PreviewManager, previewManager       , m_pPreviewManager       , instance           )
ACCESS(InfoTemplateManager  , infoTemplateManager  , m_pInfoTemplateManager  , instance           )
ACCESS(NumberCompletionModel, numberCompletionModel, m_pNumberCompletionModel, individualDirectory)
ACCESS(RecentFileModel      , recentFileModel      , m_pRecentFileModel      , instance           )
ACCESS(Video::DeviceModel   , deviceModel          , m_pDeviceModel          , instance           )

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

/* The old manual deletion order:
   delete Session::instance()->peersTimelineModel();
   delete Session::instance()->recordingModel();
   delete Session::instance()->personDirectory();
   delete Session::instance()->callModel();
   delete Session::instance()->profileModel();
   delete Session::instance()->accountModel();
   delete Session::instance()->individualDirectory();
   delete Session::instance()->numberCategoryModel();*/

#undef ACCESS
