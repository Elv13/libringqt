/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "callmodel.h"

//Std
#include <atomic>

//Qt
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QMimeData>
#include <QtCore/QItemSelectionModel>

//Ring library
#include "call.h"
#include "uri.h"
#include "phonedirectorymodel.h"
#include "contactmethod.h"
#include "accountmodel.h"
#include "availableaccountmodel.h"
#include "dbus/metatypes.h"
#include "dbus/callmanager.h"
#include "dbus/configurationmanager.h"
#include "dbus/instancemanager.h"
#include "private/videorenderermanager.h"
#include "private/imconversationmanagerprivate.h"
#include "mime.h"
#include "typedefs.h"
#include "libcard/calendar.h"
#include "individual.h"
#include "collectioninterface.h"
#include "dbus/videomanager.h"
#include "categorizedhistorymodel.h"
#include "globalinstances.h"
#include "interfaces/contactmethodselectori.h"
#include "personmodel.h"
#include "useractionmodel.h"
#include "video/renderer.h"
#include "media/audio.h"
#include "media/video.h"
#include "private/media_p.h"
#include "call_const.h"

//System
#include <unistd.h>
#include <errno.h>

//Private
#include "private/call_p.h"

//Define
///InternalStruct: internal representation of a call
struct InternalStruct {
    Call*                  call_real  { nullptr };
    QModelIndex            index      {         };
    QList<InternalStruct*> m_lChildren{         };
    bool                   conference { false   };
    InternalStruct*        m_pParent  { nullptr };
};

class CallModelPrivate final : public QObject
{
    Q_OBJECT
public:
    CallModelPrivate(CallModel* parent);
    void init();
    Call* addCall2         ( Call* call           , Call* parent = nullptr );
    void  removeCall       ( Call* call           , bool noEmit = false    );
    Call* addExistingCall  ( const QString& callId, const QString& state   );
    Call* addConference    ( const QString& confID                         );
    void  removeConference ( const QString& confId                         );
    Call* addIncomingCall  ( const QString& callId                         );

    //Attributes
    QList<InternalStruct*>                 m_lInternalModel    {       };
    QHash< Call*       , InternalStruct* > m_shInternalMapping {       };
    QHash< QString     , InternalStruct* > m_shDringId         {       };
    QItemSelectionModel*                   m_pSelectionModel   {nullptr};
    UserActionModel*                       m_pUserActionModel  {nullptr};
    int                                    m_AutoCleanDelay    {   0   };


    //Helpers
    bool isPartOf        ( const QModelIndex& confIdx, Call* call );
    void removeConference( Call* conf                             );
    void removeInternal  ( InternalStruct* internal               );

    static QStringList getCallList();

private:
    CallModel* q_ptr;

public Q_SLOTS:
    void slotSelectionChanged(const QModelIndex& idx);

private Q_SLOTS:
    void slotCallStateChanged   ( const QString& callID    , const QString &state, int code);
    void slotIncomingCall       ( const QString& accountID , const QString & callID );
    void slotIncomingConference ( const QString& confID                             );
    void slotChangingConference ( const QString& confID    , const QString &state   );
    void slotConferenceRemoved  ( const QString& confId                             );
    void slotAddPrivateCall     ( Call* call                                        );
    void slotNewRecordingAvail  ( const QString& callId    , const QString& filePath);
    void slotCallChanged        (                                                   );
    void slotStateChanged       ( Call::State newState, Call::State previousState   );
    void slotDialNumberChanged  ( const QString& entry                              );
    void slotDTMFPlayed         ( const QString& str                                );
    void slotRecordStateChanged ( const QString& callId    , bool state             );
    void slotAudioMuted         ( const QString& callId    , bool state             );
    void slotVideoMutex         ( const QString& callId    , bool state             );
    void slotPeerHold           ( const QString& callId    , bool state             );
    void slotRtcpReportReceived ( const QString& callId    , const MapStringInt& m  );
};


/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Singleton
CallModel& CallModel::instance()
{
    static auto instance = new CallModel();

    // Fix loop-dependency issue between constructors
    static std::atomic_flag init_flag {ATOMIC_FLAG_INIT};
    if (!init_flag.test_and_set())
        instance->d_ptr->init();

    return *instance;
}

CallModelPrivate::CallModelPrivate(CallModel* parent) : QObject(parent),q_ptr(parent)
{
}

///Retrieve current and older calls from the daemon, fill history, model and enable drag n' drop
CallModel::CallModel() : QAbstractItemModel(QCoreApplication::instance()),d_ptr(new CallModelPrivate(this))
{
   //Register with the daemon
   InstanceManager::instance();
   setObjectName(QStringLiteral("CallModel"));
   #ifdef ENABLE_VIDEO
   VideoRendererManager::instance();
   #endif

   //Necessary to receive text message
   IMConversationManagerPrivate::instance();
} //CallModel

///Constructor (there fix an initializationn loop)
void CallModelPrivate::init()
{
    CallManagerInterface& callManager = CallManager::instance();
#ifdef ENABLE_VIDEO
    VideoManager::instance();
#endif

    //SLOTS
    /*             SENDER                          SIGNAL                     RECEIVER                    SLOT                   */
    /**/connect(&callManager, SIGNAL(callStateChanged(QString,QString,int))     , this , SLOT(slotCallStateChanged(QString,QString,int)),    Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(incomingCall(QString,QString,QString))     , this , SLOT(slotIncomingCall(QString,QString)),            Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(conferenceCreated(QString))                , this , SLOT(slotIncomingConference(QString)),              Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(conferenceChanged(QString,QString))        , this , SLOT(slotChangingConference(QString,QString)),      Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(conferenceRemoved(QString))                , this , SLOT(slotConferenceRemoved(QString)),               Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(recordPlaybackFilepath(QString,QString))   , this , SLOT(slotNewRecordingAvail(QString,QString)),       Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(recordingStateChanged(QString,bool))       , this , SLOT(slotRecordStateChanged(QString,bool)),         Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(audioMuted(QString,bool))                  , this , SLOT(slotAudioMuted(QString,bool)),                 Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(videoMuted(QString,bool))                  , this , SLOT(slotVideoMutex(QString,bool)),                 Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(peerHold(QString,bool))                    , this , SLOT(slotPeerHold(QString,bool)),                   Qt::QueuedConnection);
    /**/connect(&callManager, SIGNAL(onRtcpReportReceived(QString,MapStringInt)), this , SLOT(slotRtcpReportReceived(QString,MapStringInt)), Qt::QueuedConnection);
    /*                                                                                                                           */

    connect(&CategorizedHistoryModel::instance(),SIGNAL(newHistoryCall(Call*)),this,SLOT(slotAddPrivateCall(Call*)));

    registerCommTypes();

    const QStringList callList = getCallList();
    for (const QString& callId : qAsConst(callList)) {
        Call* tmpCall = CallPrivate::buildExistingCall(callId);
        addCall2(tmpCall);
    }

    const QStringList confList = callManager.getConferenceList();

    for (const QString& confId : qAsConst(confList)) {
        Call* conf = addConference(confId);
        emit q_ptr->conferenceCreated(conf);
    }
}

///Destructor
CallModel::~CallModel()
{
    const QList<Call*> keys = d_ptr->m_shInternalMapping.keys();
    const QList<InternalStruct*> values = d_ptr->m_shInternalMapping.values();

    for (Call* call : qAsConst(keys))
        delete call;

    qDeleteAll(values);

    d_ptr->m_shInternalMapping.clear  ();
    d_ptr->m_shDringId.clear();

    //Unregister from the daemon
    InstanceManagerInterface& instance = InstanceManager::instance();
    Q_NOREPLY instance.Unregister(getpid());

#ifndef ENABLE_LIBWRAP
    instance.connection().disconnectFromBus(instance.connection().baseService());
#endif //ENABLE_LIBWRAP

}

QHash<int,QByteArray> CallModel::roleNames() const
{
   static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

   static std::atomic_flag initRoles {ATOMIC_FLAG_INIT};
   if (!initRoles.test_and_set()) {
      roles.insert(static_cast<int>(Call::Role::Name            ) ,QByteArray("name")            );
      roles.insert(static_cast<int>(Call::Role::Number          ) ,QByteArray("number")          );
      roles.insert(static_cast<int>(Call::Role::Direction       ) ,QByteArray("direction")       );
      roles.insert(static_cast<int>(Call::Role::Date            ) ,QByteArray("date")            );
      roles.insert(static_cast<int>(Call::Role::Length          ) ,QByteArray("length")          );
      roles.insert(static_cast<int>(Call::Role::FormattedDate   ) ,QByteArray("formattedDate")   );
      roles.insert(static_cast<int>(Call::Role::HasAVRecording  ) ,QByteArray("hasAVRecording")  );
      roles.insert(static_cast<int>(Call::Role::Historystate    ) ,QByteArray("historyState")    );
      roles.insert(static_cast<int>(Call::Role::Filter          ) ,QByteArray("filter")          );
      roles.insert(static_cast<int>(Call::Role::FuzzyDate       ) ,QByteArray("fuzzyDate")       );
      roles.insert(static_cast<int>(Call::Role::IsBookmark      ) ,QByteArray("isBookmark")      );
      roles.insert(static_cast<int>(Call::Role::Security        ) ,QByteArray("security")        );
      roles.insert(static_cast<int>(Call::Role::Department      ) ,QByteArray("department")      );
      roles.insert(static_cast<int>(Call::Role::Email           ) ,QByteArray("email")           );
      roles.insert(static_cast<int>(Call::Role::Organisation    ) ,QByteArray("organisation")    );
      roles.insert(static_cast<int>(Call::Role::Object          ) ,QByteArray("object")          );
      roles.insert(static_cast<int>(Call::Role::Photo           ) ,QByteArray("photo")           );
      roles.insert(static_cast<int>(Call::Role::State           ) ,QByteArray("state")           );
      roles.insert(static_cast<int>(Call::Role::StartTime       ) ,QByteArray("startTime")       );
      roles.insert(static_cast<int>(Call::Role::StopTime        ) ,QByteArray("stopTime")        );
      roles.insert(static_cast<int>(Call::Role::DropState       ) ,QByteArray("dropState")       );
      roles.insert(static_cast<int>(Call::Role::DTMFAnimState   ) ,QByteArray("dTMFAnimState")   );
      roles.insert(static_cast<int>(Call::Role::LastDTMFidx     ) ,QByteArray("lastDTMFidx")     );
      roles.insert(static_cast<int>(Call::Role::IsAVRecording   ) ,QByteArray("isAVRecording")   );
      roles.insert(static_cast<int>(Call::Role::LifeCycleState  ) ,QByteArray("lifeCycleState")  );
      roles.insert(static_cast<int>(Call::Role::DateOnly        ) ,QByteArray("dateOnly")        );
      roles.insert(static_cast<int>(Call::Role::DateTime        ) ,QByteArray("dateTime")        );
      roles.insert(static_cast<int>(Call::Role::AudioRecording  ) ,QByteArray("audioRecording")  );
      roles.insert(static_cast<int>(Call::Role::IsConference    ) ,QByteArray("isConference")    );
   }

   return roles;
}

QItemSelectionModel* CallModel::selectionModel() const
{
    if (!d_ptr->m_pSelectionModel) {
        d_ptr->m_pSelectionModel = new QItemSelectionModel(const_cast<CallModel*>(this));
        connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged,
            d_ptr.data(), &CallModelPrivate::slotSelectionChanged);
    }

    return d_ptr->m_pSelectionModel;
}

/**
 * Use the selection model to extract the current Call
 *
 * @return The selection call or nullptr
 */
Call* CallModel::selectedCall() const
{
    return getCall(selectionModel()->currentIndex());
}

/**
 * Select a call or remove the selection if the call is invalid
 *
 * @param call the call to select
 */
void CallModel::selectCall(Call* call) const
{
    selectionModel()->setCurrentIndex(getIndex(call), QItemSelectionModel::ClearAndSelect);
}

/**
 * Select and return the dialing call. If there is none, one will be added
 *
 * @see CallModel::dialingCall
 * @return the dialing call
 */
Call* CallModel::selectDialingCall(const QString& peerName, Account* account)
{
    Call* c = dialingCall(peerName, account);

    selectCall(c);

    return c;
}

/*****************************************************************************
 *                                                                           *
 *                         Access related functions                          *
 *                                                                           *
 ****************************************************************************/

///Return the active call count
int CallModel::size()
{
    return d_ptr->m_lInternalModel.size();
}

///Return the action call list
CallList CallModel::getActiveCalls() const
{
    CallList callList;
    for (InternalStruct* internalS : qAsConst(d_ptr->m_lInternalModel)) {
        callList.push_back(internalS->call_real);

        for (InternalStruct* childInt : qAsConst(internalS->m_lChildren))
            callList.push_back(childInt->call_real);

    }

    return callList;
} //getCallList

///Return all conferences
CallList CallModel::getActiveConferences()
{
    CallList confList;

    //That way it can not be invalid
    const QStringList confListS = CallManager::instance().getConferenceList();

    for (const QString& confId : qAsConst(confListS)) {
        InternalStruct* internalS = d_ptr->m_shDringId[confId];
        if (!internalS) {
            qDebug() << "Warning: Conference not found, creating it, this should not happen";
            Call* conf = d_ptr->addConference(confId);
            confList << conf;
            emit conferenceCreated(conf);
        }
        else
            confList << internalS->call_real;
    }

    return confList;
} //getConferenceList

/**
 * Returns true if it is possible to create a conference using the calls currently
 * tracked by CallModel.
 */
bool CallModel::conferencePossible() const
{
    if (rowCount() < 2)
        return false;

    int compatible = 0;

    for (const InternalStruct* s : qAsConst(d_ptr->m_lInternalModel)) {
        if (s->m_lChildren.size())
            return true;

        compatible += s->call_real->lifeCycleState() == Call::LifeCycleState::PROGRESS ?
            1 : 0;
    }

    return compatible >= 2;
}

bool CallModel::hasConference() const
{
   for (const InternalStruct* s : qAsConst(d_ptr->m_lInternalModel)) {
      if (s->m_lChildren.size())
         return true;
   }
   return false;
}

QList<Call*> CallModel::getConferenceParticipants(Call* conf) const
{
    QList<Call*> participantCallList;

    const auto internalConf = d_ptr->m_shInternalMapping[conf];

    for (const auto s : qAsConst(internalConf->m_lChildren))
        participantCallList << s->call_real;

    return participantCallList;
}

bool CallModel::isConnected() const
{
#ifdef ENABLE_LIBWRAP
   return InstanceManager::instance().isConnected();
#else
   return InstanceManager::instance().connection().isConnected();
#endif //ENABLE_LIBWRAP
}

bool CallModel::isValid()
{
   return CallManager::instance().isValid();
}

/// The number of milliseconds before removing finished calls from the model.
int CallModel::autoCleanDelay() const
{
    return d_ptr->m_AutoCleanDelay;
}

void CallModel::setAudoCleanDelay(int delay)
{
    d_ptr->m_AutoCleanDelay = delay;
}

UserActionModel* CallModel::userActionModel() const
{
   if (!d_ptr->m_pUserActionModel)
      d_ptr->m_pUserActionModel = new UserActionModel(const_cast<CallModel*>(this));

   return d_ptr->m_pUserActionModel;
}


/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Get the call associated with this index
Call* CallModel::getCall( const QModelIndex& idx ) const
{
    if (idx.isValid() && idx.data(static_cast<int>(Call::Role::Object)).canConvert<Call*>())
        return qvariant_cast<Call*>(idx.data(static_cast<int>(Call::Role::Object)));

    return nullptr;
}

///Get the call associated with this ID
Call* CallModel::getCall( const QString& callId ) const
{
    if (d_ptr->m_shDringId.value(callId))
        return d_ptr->m_shDringId[callId]->call_real;

    return nullptr;
}

Call* CallModel::firstActiveCall(const QSharedPointer<Individual>& ind) const
{
    if (!ind)
        return nullptr;

    foreach (Call* call, getActiveCalls()) {
        if (call->lifeCycleState() != Call::LifeCycleState::FINISHED
          && call->state() != Call::State::DIALING
          && call->peer() == ind)
            return call;
    }

    return nullptr;
}

/// Because QML is horrible at handling shared pointers
Call* CallModel::firstActiveCall(Individual* ind) const
{
    return firstActiveCall(Individual::getIndividual(ind));
}

Call* CallModel::firstActiveCall(const ContactMethod* cm) const
{
    if (!cm)
        return nullptr;

    foreach (Call* call, getActiveCalls()) {
        if (call->lifeCycleState() != Call::LifeCycleState::FINISHED
          && call->state() != Call::State::DIALING
          && call->peerContactMethod()->d() == cm->d())
            return call;
    }

    return nullptr;
}


///Make sure all signals can be mapped back to Call objects
void CallModel::registerCall(Call* call)
{
   d_ptr->m_shDringId[ call->dringId() ] = d_ptr->m_shInternalMapping[call];
}

///Add a call in the model structure, the call must exist before being added to the model
Call* CallModelPrivate::addCall2(Call* call, Call* parentCall)
{
   //The current History implementation doesn't support conference
   //if something try to add an history conference, something went wrong
   if (!call
    || ((parentCall && parentCall->lifeCycleState() == Call::LifeCycleState::FINISHED)
    && (call->lifeCycleState() == Call::LifeCycleState::FINISHED))) {

      qWarning() << "Trying to add an invalid call to the tree" << call;
      Q_ASSERT(false);

      //WARNING this will trigger an assert later on, but isn't critical enough in release mode.
      //HACK This return an invalid object that should be equivalent to NULL but won't require
      //nullptr check everywhere in the code. It is safer to use an invalid object rather than
      //causing a NULL dereference
      return new Call(QString(),QString());
   }

   if (m_shInternalMapping.contains(call)) {
      qWarning() << "Trying to add a call that already have been added" << call;
      Q_ASSERT(false);
   }

   //Even history call currently need to be tracked in CallModel, this may change
   InternalStruct* aNewStruct = new InternalStruct;
   aNewStruct->call_real  = call;
   aNewStruct->conference = false;

   m_shInternalMapping  [ call       ] = aNewStruct;
   if (call->lifeCycleState() != Call::LifeCycleState::FINISHED) {
      q_ptr->beginInsertRows({},m_lInternalModel.size(),m_lInternalModel.size());
      m_lInternalModel << aNewStruct;
      q_ptr->endInsertRows();
   }

   //Dialing calls don't have remote yet, it will be added later
   if (call->hasRemote())
      m_shDringId[ call->dringId() ] = aNewStruct;

   //If the call is already finished, there is no point to track it here
   if (call->lifeCycleState() != Call::LifeCycleState::FINISHED) {
      emit q_ptr->callAdded(call,parentCall);
      const QModelIndex idx = q_ptr->index(m_lInternalModel.size()-1,0,{});
      emit q_ptr->dataChanged(idx, idx);
      emit q_ptr->callStateChanged(call, call->state());
      connect(call, &Call::changed, this, &CallModelPrivate::slotCallChanged);
      connect(call,&Call::stateChanged,this,&CallModelPrivate::slotStateChanged);
      connect(call,&Call::dialNumberChanged,this,&CallModelPrivate::slotDialNumberChanged);
      connect(call,SIGNAL(dtmfPlayed(QString)),this,SLOT(slotDTMFPlayed(QString)));
      connect(call,&Call::videoStarted, this, [this,call](Video::Renderer* r) {
         emit q_ptr->rendererAdded(call, r);
      });
      connect(call,&Call::videoStopped, this, [this,call](Video::Renderer* r) {
         emit q_ptr->rendererRemoved(call, r);
      });
      connect(call,&Call::mediaAdded, this, [this,call](Media::Media* media) {
         emit q_ptr->mediaAdded(call,media);
      });
      connect(call,&Call::mediaStateChanged, this, [this,call](Media::Media* media, const Media::Media::State s, const Media::Media::State m) {
         emit q_ptr->mediaStateChanged(call,media,s,m);
      });

      foreach(auto m, call->allMedia())
         emit q_ptr->mediaAdded(call, m);

      emit q_ptr->layoutChanged();
   }
   return call;
} //addCall

bool CallModel::supportsDTMF() const
{
    const auto call = selectedCall();

    if (!call)
        return false;

    switch(call->state()) {
        case Call::State::NEW:
        case Call::State::DIALING:
        case Call::State::CURRENT:
            return true;
        case Call::State::TRANSFERRED:
        case Call::State::ERROR:
        case Call::State::ABORTED:
        case Call::State::OVER:
        case Call::State::INCOMING:
        case Call::State::RINGING:
        case Call::State::INITIALIZATION:
        case Call::State::CONNECTED:
        case Call::State::HOLD:
        case Call::State::FAILURE:
        case Call::State::BUSY:
        case Call::State::TRANSF_HOLD:
        case Call::State::CONFERENCE:
        case Call::State::CONFERENCE_HOLD:
        case Call::State::COUNT__:
            return false;
    };

    return false;
}

bool CallModel::hasDialingCall() const
{
    // Only top level calls can be dialing, no need to be recursive
    for (const auto i : qAsConst(d_ptr->m_lInternalModel)) {
        if (i->call_real->lifeCycleState() == Call::LifeCycleState::CREATION)
            return true;
    }

    return false;
}

///Return the current or create a new dialing call from peer ContactMethod
Call* CallModel::dialingCall(ContactMethod* cm, Call* parent)
{
   auto call = dialingCall(QString(), nullptr, parent);
   call->setPeerContactMethod(cm);
   return call;
}

///Return the current or create a new dialing call from peer name and the account
Call* CallModel::dialingCall(const QString& peerName, Account* account, Call* parent)
{
   //Having multiple dialing calls could be supported, but for now we decided not to
   //handle this corner case as it will create issues of its own
   foreach (Call* call, getActiveCalls()) {
      if (call->lifeCycleState() == Call::LifeCycleState::CREATION)
         return call;
   }

   return d_ptr->addCall2(CallPrivate::buildDialingCall(peerName, account, parent));
}  //dialingCall

///Create a new incoming call when the daemon is being called
Call* CallModelPrivate::addIncomingCall(const QString& callId)
{
   qDebug() << "New incoming call:" << callId;

   // Since november 2015, calls are alowed to be declared with a state change
   // if it has been done, then they should be ignored
   // contains can be true and contain nullptr if it was accessed without
   // contains() first
   Call* call = nullptr;
   if (not m_shDringId.value(callId)) {

      call = CallPrivate::buildIncomingCall(callId);

      //The call can already have been invalidated by the daemon, then do nothing
      if (!call)
         return nullptr;

      call = addCall2(call);

      //The call can already have been invalidated by the daemon, then do nothing
      if (!call)
         return nullptr;

   } else {
       qDebug() << "The call" << callId << "already exist, avoiding re-creation";
       call = m_shDringId[callId]->call_real;
   }

   //Call without account is not possible
   if (call->account()) {
       //WARNING: This code will need to be moved if we decide to drop
       //incomingCall signal
      if (call->account()->isAutoAnswer()) {
         call->performAction(Call::Action::ACCEPT);
      }
   }
   else {
      qDebug() << "Incoming call from an invalid account";
      throw tr("Invalid account");
   }

   return call;
}

Call* CallModelPrivate::addExistingCall(const QString& callId, const QString& state)
{
    Q_UNUSED(state)

    qDebug() << "New foreign call:" << callId;
    auto call = CallPrivate::buildExistingCall(callId);

    //The call can already have been invalidated by the daemon, then do nothing
    if (!call)
        return {};

    call = addCall2(call);

    //The call can already have been invalidated by the daemon, then do nothing
    if (!call)
        return {};

    //Call without account is not possible
    if (!call->account()) {
        qDebug() << "Foreign call from an invalid account";
        throw tr("Invalid account");
    }

    return call;
}

///Properly remove an internal from the Qt model
void CallModelPrivate::removeInternal(InternalStruct* internal)
{
   if (!internal)
      return;

   const int idx = m_lInternalModel.indexOf(internal);
   //Exit if the call is not found
   if (idx == -1) {
      qDebug() << "Cannot remove " << internal->call_real << ": call not found in tree";
      return;
   }

   q_ptr->beginRemoveRows({} ,idx,idx);
   m_lInternalModel.removeAt(idx);
   q_ptr->endRemoveRows();
}

/**
 * LibRingClient doesn't [need to] handle INACTIVE calls
 * This method make sure they never get into the system.
 *
 * It's not very efficient, but do the job. This method isn't
 * called very often anyway.
 */
QStringList CallModelPrivate::getCallList()
{
   CallManagerInterface& callManager = CallManager::instance();
   const QStringList callList = callManager.getCallList();
   QStringList ret;

   for (const QString& callId : callList) {
      QMap<QString, QString> details = callManager.getCallDetails(callId);
      if (details[DRing::Call::Details::CALL_STATE] != DRing::Call::StateEvent::INACTIVE)
         ret << callId;
   }

   return ret;
}

///Remove a call and update the internal structure
void CallModelPrivate::removeCall(Call* call, bool noEmit)
{
   Q_UNUSED(noEmit)
   InternalStruct* internal = m_shInternalMapping[call];

   if (!internal || !call) {
      qDebug() << "Cannot remove " << (internal?internal->call_real:nullptr) << ": call not found";
      return;
   }

   if (internal != nullptr) {
      removeInternal(internal);
      //NOTE Do not free the memory, it can still be used elsewhere or in modelindexes
   }

   //Restore calls to the main list if they are not really over
   if (internal->m_lChildren.size()) {
      for (auto child : qAsConst(internal->m_lChildren)) {
         if (child->call_real->state() != Call::State::OVER && child->call_real->state() != Call::State::ERROR) {
            q_ptr->beginInsertRows({},m_lInternalModel.size(),m_lInternalModel.size());
            m_lInternalModel << child;
            q_ptr->endInsertRows();
         }
      }
   }

   //Be sure to reset these properties (just in case)
   call->setProperty("DTMFAnimState",0);
   call->setProperty("dropState",0);

   //The daemon often fail to emit the right signal, cleanup manually
   for (auto topLevel : qAsConst(m_lInternalModel)) {
      if (topLevel->call_real->type() == Call::Type::CONFERENCE &&
         (!topLevel->m_lChildren.size()
            //HACK Make a simple validation to prevent ERROR->ERROR->ERROR state loop for conferences
            || topLevel->m_lChildren.constFirst()->call_real->state() == Call::State::ERROR
            || topLevel->m_lChildren.constLast() ->call_real->state() == Call::State::ERROR))
            removeConference(topLevel->call_real);
   }

   emit q_ptr->layoutChanged();
} //removeCall


QModelIndex CallModel::getIndex(Call* call) const
{
    if (!call)
        return {};

    const auto internal = d_ptr->m_shInternalMapping[call];

    int idx = d_ptr->m_lInternalModel.indexOf(internal);

    if (idx != -1) {
        return index(idx,0);
    }

    for (InternalStruct* str : qAsConst(d_ptr->m_lInternalModel)) {
        idx = str->m_lChildren.indexOf(internal);
        if (idx != -1)
            return index(idx,0,index(d_ptr->m_lInternalModel.indexOf(str),0));
    }

    return {};
}

///Transfer "toTransfer" to "target" and wait to see it it succeeded
void CallModel::attendedTransfer(Call* toTransfer, Call* target)
{
   if ((!toTransfer) || (!target))
      return;

   Q_NOREPLY CallManager::instance().attendedTransfer(toTransfer->dringId(),target->dringId());

   //TODO [Daemon] Implement this correctly
   toTransfer->d_ptr->changeCurrentState(Call::State::OVER);
   target->d_ptr->changeCurrentState(Call::State::OVER);
} //attendedTransfer

///Transfer this call to  "target" number
void CallModel::transfer(Call* toTransfer, const ContactMethod* target)
{
   qDebug() << "Transferring call " << toTransfer << "to" << target->uri();
   toTransfer->setTransferNumber        ( target->uri()            );
   toTransfer->performAction            ( Call::Action::TRANSFER   );
   toTransfer->d_ptr->changeCurrentState( Call::State::TRANSFERRED );
   toTransfer->performAction            ( Call::Action::ACCEPT     );
   toTransfer->d_ptr->changeCurrentState( Call::State::OVER        );
   emit toTransfer->isOver();
} //transfer

/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
Call* CallModelPrivate::addConference(const QString& confID)
{
    qDebug() << "Notified of a new conference " << confID;
    CallManagerInterface& callManager = CallManager::instance();
    const QStringList callList = callManager.getParticipantList(confID);
    qDebug() << "Paticiapants are:" << callList;

    if (!callList.size()) {
        qDebug() << "This conference (" + confID + ") contain no call";
        return nullptr;
    }

    if (!m_shDringId.value(callList[0])) {
        qDebug() << "Invalid call";
        return nullptr;
    }

    Call* newConf = nullptr;

    if (m_shDringId[callList[0]]->call_real->account())
        newConf = new Call(confID, m_shDringId[callList[0]]->call_real->account()->id());

    if (!newConf)
        return nullptr;

    InternalStruct* aNewStruct = new InternalStruct;
    aNewStruct->call_real      = newConf;
    aNewStruct->conference     = true;

    m_shInternalMapping[newConf]  = aNewStruct;
    m_shDringId[confID] = aNewStruct;
    q_ptr->beginInsertRows({},m_lInternalModel.size(),m_lInternalModel.size());
    m_lInternalModel << aNewStruct;
    q_ptr->endInsertRows();

    for (const QString& callId : qAsConst(callList)) {
        if (!m_shDringId.contains(callId)) {
            qDebug() << "References to unknown call";
            continue;
        }

        InternalStruct* callInt = m_shDringId[callId];

        if (callInt->m_pParent && callInt->m_pParent != aNewStruct)
            callInt->m_pParent->m_lChildren.removeAll(callInt);

        removeInternal(callInt);
        callInt->m_pParent = aNewStruct;
        callInt->call_real->setProperty("dropState",0);

        if (aNewStruct->m_lChildren.indexOf(callInt) == -1) {
            auto parent = q_ptr->index(m_lInternalModel.indexOf(aNewStruct), 0, {});
            q_ptr->beginInsertRows(parent, aNewStruct->m_lChildren.size(), aNewStruct->m_lChildren.size());
            aNewStruct->m_lChildren << callInt;
            q_ptr->endInsertRows();
        }
    }

    const QModelIndex idx = q_ptr->index(m_lInternalModel.size()-1,0,{});
    emit q_ptr->dataChanged(idx, idx);
    emit q_ptr->layoutChanged();
    connect(newConf, &Call::changed, this, &CallModelPrivate::slotCallChanged);

    connect(newConf,&Call::videoStarted, this, [this,newConf](Video::Renderer* r) {
        emit q_ptr->rendererAdded(newConf, r);
    });

    connect(newConf,&Call::videoStopped, this, [this,newConf](Video::Renderer* r) {
        emit q_ptr->rendererRemoved(newConf, r);
    });

    return newConf;
} //addConference

bool CallModel::createJoinOrMergeConferenceFromCall(Call* call1, Call* call2)
{
   if (!call1 || !call2) {
      qWarning() << "Trying to join a call with nothing";
      return false;
   }

   if (call1->lifeCycleState() == Call::LifeCycleState::CREATION || call2->lifeCycleState() == Call::LifeCycleState::CREATION) {
       qWarning() << "Trying to add a dialing call to the conference, it wont work";
       return false;
   }

   qDebug() << "Joining call: " << call1 << " and " << call2;

   if (call1->type() == Call::Type::CONFERENCE)
      return addParticipant(call2, call1);
   else if (call2->type() == Call::Type::CONFERENCE)
      return addParticipant(call1, call2);
   else if (call1->type() == Call::Type::CONFERENCE && call2->type() == Call::Type::CONFERENCE)
      return mergeConferences(call1, call2);
   else
      Q_NOREPLY CallManager::instance().joinParticipant(call1->dringId(),call2->dringId());

   return true;
} //createConferenceFromCall

///Add a new participant to a conference
bool CallModel::addParticipant(Call* call2, Call* conference)
{
    if ((!conference) || (call2)) {
        qWarning() << "Trying to join a call with nothing";
        return false;
    }

    if (call2->lifeCycleState() == Call::LifeCycleState::CREATION) {
        qWarning() << "Trying to add a dialing call to the conference, it wont work";
        return false;
    }

    if (conference->type() == Call::Type::CONFERENCE) {
        Q_NOREPLY CallManager::instance().addParticipant(call2->dringId(), conference->dringId());
        return true;
    }

    qDebug() << "This is not a conference";

    return false;
} //addParticipant

///Remove a participant from a conference
bool CallModel::detachParticipant(Call* call)
{
   Q_NOREPLY CallManager::instance().detachParticipant(call->dringId());
   return true;
}

///Merge two conferences
bool CallModel::mergeConferences(Call* conf1, Call* conf2)
{
   Q_NOREPLY CallManager::instance().joinConference(conf1->dringId(),conf2->dringId());
   return true;
}

/// Take all Call::LifeCycleState::PROGRESS calls and merge them as a conference.
bool CallModel::mergeAllCalls()
{
    if (rowCount() < 2)
        return false;

    QList<Call*> compatible;

    for (const InternalStruct* s : qAsConst(d_ptr->m_lInternalModel)) {
        if (s->call_real->type() == Call::Type::CALL && s->call_real->lifeCycleState() == Call::LifeCycleState::PROGRESS)
            compatible << s->call_real;
    }

    if (compatible.size() < 2)
        return false;

    Call* first  = compatible.takeFirst();
    Call* second = compatible.takeFirst();

    createJoinOrMergeConferenceFromCall(first, second);

    return true;
}

/// Remove all participants from all conferences.
bool CallModel::detachAllCalls()
{

    QList<Call*> calls;

    // Do not iterate directly on the conference while mutating it
    for (const InternalStruct* conf : qAsConst(d_ptr->m_lInternalModel)) {
        if (conf->call_real->type() == Call::Type::CONFERENCE) {
            for (const InternalStruct* call : qAsConst(conf->m_lChildren))
                calls << call->call_real;

        }
    }

    for (auto c : qAsConst(calls))
        detachParticipant(c);

    return calls.size();
}

///Remove a conference from the model and the TreeView
void CallModelPrivate::removeConference(const QString &confId)
{
   if (m_shDringId.value(confId))
      qDebug() << "Ending conversation containing " << m_shDringId[confId]->m_lChildren.size() << " participants";

   removeConference(q_ptr->getCall(confId));
}

///Remove a conference using it's call object
void CallModelPrivate::removeConference(Call* call)
{
    if (!m_shInternalMapping.contains(call)) {
        qDebug() << "Cannot remove conference: call not found";
        return;
    }

    removeCall(call, true);

    // currently the daemon does not emit a  Call/Conference changed signal to indicate that the
    // conference is over so we change the conf state here (since this is called when we get the
    // "removeConference" signal from the daemon)
    call->d_ptr->changeCurrentState(Call::State::OVER);
}


/*****************************************************************************
 *                                                                           *
 *                                  Model                                    *
 *                                                                           *
 ****************************************************************************/

///This model doesn't support direct write, only the dragState hack
bool CallModel::setData( const QModelIndex& idx, const QVariant &value, int role)
{
    if (!idx.isValid())
        return false;

    if (role == static_cast<int>(Call::Role::DropState)) {
        if (auto call = getCall(idx))
            call->setProperty("dropState",value.toInt());
        emit dataChanged(idx,idx);
    }
    else if (role == Qt::EditRole) {
        const QString number = value.toString();
        Call* call = getCall(idx);
        if (call && number != call->dialNumber()) {
            call->setDialNumber(number);
            emit dataChanged(idx,idx);
            return true;
        }
    }
    else if (role == static_cast<int>(Call::Role::DTMFAnimState)) {
        if (auto call = getCall(idx)) {
            call->setProperty("DTMFAnimState",value.toInt());
            emit dataChanged(idx,idx);
            return true;
        }
    }
    else if (role == static_cast<int>(Call::Role::DropPosition)) {
        if (auto call = getCall(idx)) {
            call->setProperty("dropPosition",value.toInt());
            emit dataChanged(idx,idx);
            return true;
        }
    }

    return false;
}

///Get information relative to the index
QVariant CallModel::data( const QModelIndex& idx, int role) const
{
   if (!idx.isValid())
      return {};

   Call* call = nullptr;

   if (!idx.parent().isValid() && d_ptr->m_lInternalModel.size() > idx.row() && d_ptr->m_lInternalModel[idx.row()])
      call = d_ptr->m_lInternalModel[idx.row()]->call_real;
   else if (idx.parent().isValid() && d_ptr->m_lInternalModel.size() > idx.parent().row()) {
      InternalStruct* intList = d_ptr->m_lInternalModel[idx.parent().row()];
      if (intList->conference == true && intList->m_lChildren.size() > idx.row() && intList->m_lChildren[idx.row()])
         call = intList->m_lChildren[idx.row()]->call_real;
   }

   return call?call->roleData((Call::Role)role):QVariant();
}

///Header data
QVariant CallModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(section)

   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant(tr("Calls"));

   return {};
}

///The number of conference and stand alone calls
int CallModel::rowCount( const QModelIndex& parentIdx ) const
{
    if (!parentIdx.isValid() || !parentIdx.internalPointer())
        return d_ptr->m_lInternalModel.size();

    const InternalStruct* modelItem = static_cast<InternalStruct*>(parentIdx.internalPointer());

    Q_ASSERT(modelItem);

    if (modelItem->call_real->type() == Call::Type::CONFERENCE && modelItem->m_lChildren.size() > 0)
        return modelItem->m_lChildren.size();

    if (modelItem->call_real->type() == Call::Type::CONFERENCE)
        qWarning() << modelItem->call_real << "have"
            << modelItem->m_lChildren.size() << "and"
            << ((modelItem->call_real->type() == Call::Type::CONFERENCE)?"is":"is not") << "a conference";

   return 0;
}

///Make everything selectable and drag-able
Qt::ItemFlags CallModel::flags( const QModelIndex& idx ) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;

    const InternalStruct* modelItem = static_cast<InternalStruct*>(idx.internalPointer());

    Q_ASSERT(modelItem);

    const Call* c = modelItem->call_real;

    return Qt::ItemIsEnabled|Qt::ItemIsSelectable
        | Qt::ItemIsDragEnabled
        | ((c->type() != Call::Type::CONFERENCE)?(Qt::ItemIsDropEnabled):Qt::ItemIsEnabled)
        | ((c->lifeCycleState() == Call::LifeCycleState::CREATION)?Qt::ItemIsEditable:Qt::NoItemFlags);
}

///Return valid payload types
int CallModel::acceptedPayloadTypes()
{
    return DropPayloadType::CALL |
        DropPayloadType::HISTORY |
        DropPayloadType::CONTACT |
        DropPayloadType::NUMBER  |
        DropPayloadType::TEXT;
}

///There is always 1 column
int CallModel::columnCount ( const QModelIndex& parentIdx) const
{
    if (!parentIdx.isValid())
        return 1;

    const InternalStruct* modelItem = static_cast<InternalStruct*>(parentIdx.internalPointer());

    return modelItem->m_lChildren.isEmpty() ? 0 : 1;
}

///Return the conference if 'index' is part of one
QModelIndex CallModel::parent( const QModelIndex& idx) const
{
    if (!idx.isValid())
        return {};

    const InternalStruct* modelItem = (InternalStruct*)idx.internalPointer();

    Q_ASSERT(modelItem);

    if (modelItem->m_pParent) {
        const int rowidx = d_ptr->m_lInternalModel.indexOf(modelItem->m_pParent);
        if (rowidx != -1) {
            return CallModel::index(rowidx,0,{});
        }
    }

    return {};
}

///Get the call index at row,column (active call only)
QModelIndex CallModel::index( int row, int column, const QModelIndex& parentIdx) const
{
   if (row >= 0 && !parentIdx.isValid() && d_ptr->m_lInternalModel.size() > row)
      return createIndex(row,column,d_ptr->m_lInternalModel[row]);

   if (!parentIdx.isValid())
      return {};

   if (row < 0 || parentIdx.row() >= d_ptr->m_lInternalModel.size())
      return {};

   if (d_ptr->m_lInternalModel[parentIdx.row()]->m_lChildren.size() > row)
      return createIndex(row,column,d_ptr->m_lInternalModel[parentIdx.row()]->m_lChildren[row]);

   return {};
}

QStringList CallModel::mimeTypes() const
{
    static QStringList mimes {
        RingMimes::PLAIN_TEXT,
        RingMimes::PHONENUMBER,
        RingMimes::CALLID,
        QStringLiteral("text/html")
    };

    return mimes;
}

QMimeData* CallModel::mimeData(const QModelIndexList& indexes) const
{

    if (indexes.size() != 1)
        return new QMimeData();

    const QModelIndex idx = indexes.constFirst();

    if (!idx.isValid())
        return new QMimeData();

    if (auto call = getCall(idx))
        return call->mimePayload();

   //TODO handle/hardcode something for multiple selections / composite MIME

   return new QMimeData();
}

bool CallModelPrivate::isPartOf(const QModelIndex& confIdx, Call* call) //FIXME broken code
{
   if (!confIdx.isValid() || !call) return false;

   for (int i=0;i<confIdx.model()->rowCount(confIdx);i++) { //TODO use model one directly
      Call* c = q_ptr->getCall(confIdx);
      if (c && c->dringId() == call->dringId()) {
         return true;
      }
   }
   return false;
}

Call* CallModel::fromMime( const QByteArray& fromMime) const
{
   return getCall(QString(fromMime));
}

bool CallModel::dropMimeData(const QMimeData* mimedata, Qt::DropAction action, int row, int column, const QModelIndex& parentIdx )
{
   Q_UNUSED(action)
   const QModelIndex targetIdx = index( row,column,parentIdx );

   if (mimedata->hasFormat(RingMimes::CALLID)) {
      const QByteArray encodedCallId = mimedata->data( RingMimes::CALLID    );
      Call* call                     = fromMime ( encodedCallId        );
      Call* target                   = getCall  ( targetIdx            );
      Call* targetParent             = getCall  ( targetIdx.parent()   );

      //Call or conference dropped on itself -> cannot transfer or merge, so exit now
      if (target == call) {
         qDebug() << "Call/Conf dropped on itself (doing nothing)";
         return false;
      }

      if (!call) {
         qDebug() << "Call not found";
         return false;
      }

      auto dropAction = (RingMimes::Actions) mimedata->property("dropAction").toInt();

      if (dropAction == RingMimes::Actions::INVALID && mimedata->hasFormat(RingMimes::XRINGACTION))
          dropAction = RingMimes::fromActionName(mimedata->data(RingMimes::XRINGACTION));

      switch (dropAction) {
         case RingMimes::Actions::JOIN:

            //Call or conference dropped on part of itself -> cannot merge conference with itself
            if (d_ptr->isPartOf(targetIdx,call) || d_ptr->isPartOf(targetIdx.parent(),call) || (call && targetParent && targetParent == call)) {
               qDebug() << "Call/Conf dropped on its own conference (doing nothing)";
               return false;
            }

            //Conference dropped on a conference -> merge both conferences
            if (call && target && call->type() == Call::Type::CONFERENCE && target->type() == Call::Type::CONFERENCE) {
               qDebug() << "Merge conferences" << call << "and" << target;
               mergeConferences(call,target);
               return true;
            }

            //Conference dropped on a call part of a conference -> merge both conferences
            if (call && call->type() == Call::Type::CONFERENCE && targetParent) {
               qDebug() << "Merge conferences" << call << "and" << targetParent;
               mergeConferences(call,getCall(targetIdx.parent()));
               return true;
            }

            //Drop a call on a conference -> add it to the conference
            if (target && (targetIdx.parent().isValid() || target->type() == Call::Type::CONFERENCE)) {
               Call* conf = target->type() == Call::Type::CONFERENCE?target:qvariant_cast<Call*>(targetIdx.parent().data(static_cast<int>(Call::Role::Object)));
               if (conf) {
                  qDebug() << "Adding call " << call << "to conference" << conf;
                  addParticipant(call,conf);
                  return true;
               }
            }
            //Conference dropped on a call
            else if (target && call && rowCount(getIndex(call))) {
               qDebug() << "Conference dropped on a call: adding call to conference";
               addParticipant(target,call);
               return true;
            }
            //Call dropped on a call
            else if (call && target && !targetIdx.parent().isValid()) {
               qDebug() << "Call dropped on a call: creating a conference";
               createJoinOrMergeConferenceFromCall(call,target);
               return true;
            }

            break;
         case RingMimes::Actions::TRANSFER:
            qDebug() << "Performing an attended transfer";
            attendedTransfer(call,target);
            break;
         case RingMimes::Actions::INVALID:
             break;
         default:
            Q_ASSERT(false); // A cast went seriously wrong, abort
            break;
      }
   }
   else if (mimedata->hasFormat(RingMimes::PHONENUMBER)) {
      const QByteArray encodedContactMethod = mimedata->data( RingMimes::PHONENUMBER );
      Call* target = getCall(targetIdx);
      qDebug() << "Phone number" << encodedContactMethod << "on call" << target;
      Call* newCall = dialingCall(QString(),target->account());
      ContactMethod* nb = PhoneDirectoryModel::instance().fromHash(encodedContactMethod);
      newCall->setDialNumber(nb);
      newCall->performAction(Call::Action::ACCEPT);
      createJoinOrMergeConferenceFromCall(newCall,target);
   }
   else if (mimedata->hasFormat(RingMimes::CONTACT)) {
      const QByteArray encodedPerson = mimedata->data(RingMimes::CONTACT);
      Call* target = getCall(targetIdx);
      qDebug() << "Contact" << encodedPerson << "on call" << target;
      try {
         const ContactMethod* number = GlobalInstances::contactMethodSelector().number(
         PersonModel::instance().getPersonByUid(encodedPerson));
         if (!number->uri().isEmpty()) {
            Call* newCall = dialingCall();
            newCall->setDialNumber(number);
            newCall->performAction(Call::Action::ACCEPT);
            createJoinOrMergeConferenceFromCall(newCall,target);
         }
         else {
            qDebug() << "Person not found";
         }
      }
      catch (...) {
         qDebug() << "There is nothing to handle contact";
      }
   }
   return false;
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///When a call state change
void CallModelPrivate::slotCallStateChanged(const QString& callID, const QString& stateName, int code)
{

    //This code is part of the CallModel interface too
    qDebug() << "Call State Changed for call  " << callID << " . New state : " << stateName;
    InternalStruct* internal = m_shDringId[callID];
    Call* call = nullptr;

    if (!internal && stateName == DRing::Call::StateEvent::CONNECTING)
        return;

    if(!internal) {
        qDebug() << "Call not found" << callID << "new state" << stateName;
        addExistingCall(callID, stateName);
        return;
    }

    call = internal->call_real;

    QString sn = stateName;

    //Ring account handle "busy" differently from other types
    if (call->account()
      && call->account()->protocol() == Account::Protocol::RING
      && sn == DRing::Call::StateEvent::HUNGUP
      && code == ECONNREFUSED)
        sn = DRing::Call::StateEvent::BUSY;

    qDebug() << "Call found" << call << call->state();

    if (code && code != call->d_ptr->m_LastErrorCode) {
        call->d_ptr->m_LastErrorCode = code;
        emit call->errorChanged();
    }

    const Call::LifeCycleState oldLifeCycleState = call->lifeCycleState();
    const Call::State          oldState          = call->state();

    call->d_ptr->stateChanged(sn);

    //Remove call when they end normally, keep errors and failure one
    if ((sn == DRing::Call::StateEvent::HUNGUP)
      || ((oldState == Call::State::OVER) && (call->state() == Call::State::OVER))
      || (oldLifeCycleState != Call::LifeCycleState::FINISHED && call->state() == Call::State::OVER))
        QTimer::singleShot(0, [this,call]() {
            if (call->state() == Call::State::ABORTED) {
                removeCall(call);
                return;
            }
            else if (!call->isMissed())
                QTimer::singleShot(m_AutoCleanDelay, [this, call]() {
                    removeCall(call);
                });
        });

    //Add to history
    if (call->lifeCycleState() == Call::LifeCycleState::FINISHED) {
        if (!call->collection()) {
            /*TODO remove all traces of the old history
            foreach (CollectionInterface* backend, CategorizedHistoryModel::instance().collections(CollectionInterface::SupportedFeatures::ADD)) {
                if (backend->editor<Call>()->addNew(call))
                call->setCollection(backend);
            }
            */
            call->account()->calendar()->addEvent(call);
        }
    }

} //slotCallStateChanged

///When a new call is incoming
void CallModelPrivate::slotIncomingCall(const QString& accountID, const QString& callID)
{
    Q_UNUSED(accountID)
    qDebug() << "Signal : Incoming Call ! ID = " << callID;

    if (auto c = addIncomingCall(callID)) {
        emit q_ptr->incomingCall(c);
        emit q_ptr->callAttentionRequest(c);
    }
}

///When a new conference is incoming
void CallModelPrivate::slotIncomingConference(const QString& confID)
{
    if (q_ptr->getCall(confID))
       return;

    Call* conf = addConference(confID);

    qDebug() << "Adding conference" << conf << confID;

    emit q_ptr->conferenceCreated(conf);
    emit q_ptr->callStateChanged(conf, conf->state());
}

///When a conference change
void CallModelPrivate::slotChangingConference(const QString &confID, const QString& state)
{
    if (!m_shDringId.contains(confID)) {
        qWarning() << "Error: conference not found";
        return;
    }

    InternalStruct* confInt = m_shDringId[confID];

    Call* conf = confInt->call_real;

    qDebug() << "Changing conference state" << conf << confID;

    if (!conf) { //Prevent a race condition between call and conference
        qDebug() << "Trying to affect a conference that does not exist (anymore)";
        return;
    }

    if (!q_ptr->getIndex(conf).isValid()) {
        qWarning() << "The conference item does not exist";
        return;
    }

    conf->d_ptr->stateChanged(state);
    CallManagerInterface& callManager = CallManager::instance();
    const QStringList participants = callManager.getParticipantList(confID);

    qDebug() << "The conf has" << confInt->m_lChildren.size() << "calls, daemon has" <<participants.size();

    //First remove old participants, add them back to the top level list
    for (InternalStruct* child : qAsConst(confInt->m_lChildren)) {
        if (participants.indexOf(child->call_real->dringId()) == -1 && child->call_real->lifeCycleState() != Call::LifeCycleState::FINISHED) {
            qDebug() << "Remove" << child->call_real << "from" << conf;
            child->m_pParent = nullptr;
            q_ptr->beginInsertRows({},m_lInternalModel.size(),m_lInternalModel.size());
            m_lInternalModel << child;
            q_ptr->endInsertRows();
        }
    }

    auto confIdx = q_ptr->index(m_lInternalModel.indexOf(confInt),0,{});
    q_ptr->beginRemoveRows(confIdx,0,confInt->m_lChildren.size());
    confInt->m_lChildren.clear();
    q_ptr->endRemoveRows();

    for (const QString& callId : qAsConst(participants)) {
        if (!m_shDringId.contains(callId)) {
            qDebug() << "Participants not found";
            continue;
        }

        auto callInt = m_shDringId[callId];

        if (callInt->m_pParent && callInt->m_pParent != confInt)
            callInt->m_pParent->m_lChildren.removeAll(callInt);

        removeInternal(callInt);
        callInt->m_pParent = confInt;
        q_ptr->beginInsertRows(confIdx, confInt->m_lChildren.size(), confInt->m_lChildren.size());
        confInt->m_lChildren << callInt;
        q_ptr->endInsertRows();
    }

    //The daemon often fail to emit the right signal, cleanup manually
    for (InternalStruct* topLevel : qAsConst(m_lInternalModel)) {
        if (topLevel->call_real->type() == Call::Type::CONFERENCE && !topLevel->m_lChildren.size())
            removeConference(topLevel->call_real);
    }

    //Test if there is no inconsistencies between the daemon and the client
    const QStringList deamonCallList = getCallList();
    for (const QString& callId : qAsConst(deamonCallList)) {
        const QMap<QString,QString> callDetails = callManager.getCallDetails(callId);

        if (!m_shDringId.contains(callId)) {
            qWarning() << "Conference: Call from call list not found in internal list";
            continue;
        }

        InternalStruct* callInt = m_shDringId[callId];

        const QString confId = callDetails[DRing::Call::Details::CONF_ID];
        if (callInt->m_pParent) {
            if (!confId.isEmpty()  && callInt->m_pParent->call_real->dringId() != confId) {
                qWarning() << "Conference parent mismatch";
            }
            else if (confId.isEmpty() ){
                qWarning() << "Call:" << callId << "should not be part of a conference";
                callInt->m_pParent = nullptr;
            }
        }
        else if (!confId.isEmpty()) {
            qWarning() << "Found an orphan call";
            InternalStruct* confInt2 = m_shDringId[confId];
            if (confInt2 && confInt2->call_real->type() == Call::Type::CONFERENCE
              && (callInt->call_real->type() != Call::Type::CONFERENCE)) {
                removeInternal(callInt);
                if (confInt2->m_lChildren.indexOf(callInt) == -1) {
                    auto confIdx2 = q_ptr->index(m_lInternalModel.indexOf(confInt2), 0, {});
                    q_ptr->beginInsertRows(confIdx2, confInt2->m_lChildren.size(), confInt2->m_lChildren.size());
                    confInt2->m_lChildren << callInt;
                    q_ptr->endInsertRows();
                }
            }
        }
        callInt->call_real->setProperty("dropState",0);
    }

    //TODO force reload all conferences too

    emit q_ptr->layoutChanged();
    emit q_ptr->dataChanged(confIdx, confIdx);
    emit q_ptr->conferenceChanged(conf);
} //slotChangingConference

///When a conference is removed
void CallModelPrivate::slotConferenceRemoved(const QString &confId)
{
   Call* conf = q_ptr->getCall(confId);
   removeConference(confId);
   emit q_ptr->layoutChanged();
   emit q_ptr->conferenceRemoved(conf);
   emit q_ptr->callStateChanged(conf, conf->state());
}

///Make the call aware it has a recording
void CallModelPrivate::slotNewRecordingAvail( const QString& callId, const QString& filePath)
{
    if (auto c = q_ptr->getCall(callId))
        c->d_ptr->setRecordingPath(filePath);
}

void CallModelPrivate::slotStateChanged(Call::State newState, Call::State previousState)
{
   Q_UNUSED(newState)

   if (auto call = qobject_cast<Call*>(sender())) {
        emit q_ptr->callStateChanged(call, previousState);
        emit q_ptr->selectionSupportsDTMFChanged(q_ptr->supportsDTMF());

        if (call->lifeCycleState() == Call::LifeCycleState::INITIALIZATION
          && (previousState == Call::State::NEW || previousState == Call::State::DIALING)) {
            emit q_ptr->callAttentionRequest(call);
        }
   }
}

/// Forward the Call::dialNumberChanged signal
void CallModelPrivate::slotDialNumberChanged(const QString& entry)
{
    if (Call* call = qobject_cast<Call*>(sender()))
        emit q_ptr->dialNumberChanged(call, entry);
}

///Update model if the data change
void CallModelPrivate::slotCallChanged()
{
    auto call = qobject_cast<Call*>(sender());

    if (!call)
        return;

    switch(call->state()) {
        //Transfer is "local" state, it doesn't require the daemon, so it need to be
        //handled "manually" instead of relying on the backend signals
        case Call::State::TRANSFERRED:
            emit q_ptr->callStateChanged(call, Call::State::TRANSFERRED);
            break;
        //Same goes for some errors
        case Call::State::COUNT__:
        case Call::State::ERROR:
            removeCall(call);
            break;
        //Over can be caused by local events
        case Call::State::ABORTED:
        case Call::State::OVER:
            // Do it later to give the change to other handler to do something first
            QTimer::singleShot(0, [this, call]() {
                if (call->state() == Call::State::ABORTED) {
                    removeCall(call);
                    return;
                }
                else if (!call->isMissed()) {
                    QTimer::singleShot(m_AutoCleanDelay, [this, call]() {
                        removeCall(call);
                    });
                }
            });
            break;
        //Let the daemon handle the others
        case Call::State::INCOMING:
        case Call::State::RINGING:
        case Call::State::INITIALIZATION:
        case Call::State::CONNECTED:
        case Call::State::CURRENT:
        case Call::State::DIALING:
        case Call::State::NEW:
        case Call::State::HOLD:
        case Call::State::FAILURE:
        case Call::State::BUSY:
        case Call::State::TRANSF_HOLD:
        case Call::State::CONFERENCE:
        case Call::State::CONFERENCE_HOLD:
            break;
    };

    const QModelIndex idx = q_ptr->getIndex(call);
    emit q_ptr->dataChanged(idx,idx);
}

///Add call slot
void CallModelPrivate::slotAddPrivateCall(Call* call)
{
    if (m_shInternalMapping.contains(call))
        return;

    addCall2(call,nullptr);
}

///Notice views that a dtmf have been played
void CallModelPrivate::slotDTMFPlayed( const QString& str )
{
   Call* call = qobject_cast<Call*>(QObject::sender());
   if (str.size()==1) {
      int idx = 0;
      char s = str.toLower().toLatin1()[0];
      if (s >= '1' && s <= '9'     ) idx = s - '1'     ;
      else if (s >= 'a' && s <= 'v') idx = (s - 'a')/3 ;
      else if (s >= 'w' && s <= 'z') idx = 8           ;
      else if (s == '0'            ) idx = 10          ;
      else if (s == '*'            ) idx = 9           ;
      else if (s == '#'            ) idx = 11          ;
      else                           idx = -1          ;
      call->setProperty("latestDtmfIdx",idx);

      QChar code;

      switch(idx) {
          case 9:
              code = '*';
              break;
          case 10:
              code = '0';
              break;
          case 11:
              code = '#';
              break;
          default:
              code = QString::number(idx+1)[0];
      }

      emit q_ptr->dtmfPlayed(call, code);
   }
}

///Called when a recording state change
void CallModelPrivate::slotRecordStateChanged (const QString& callId, bool state)
{
    if (auto call = q_ptr->getCall(callId)) {
        call->d_ptr->m_mIsRecording[ Media::Media::Type::AUDIO ].setAt( Media::Media::Direction::IN  , state);
        call->d_ptr->m_mIsRecording[ Media::Media::Type::AUDIO ].setAt( Media::Media::Direction::OUT , state);
        call->d_ptr->m_mIsRecording[ Media::Media::Type::VIDEO ].setAt( Media::Media::Direction::IN  , state);
        call->d_ptr->m_mIsRecording[ Media::Media::Type::VIDEO ].setAt( Media::Media::Direction::OUT , state);

        emit call->changed();
    }
}

void CallModelPrivate::slotAudioMuted( const QString& callId, bool state)
{
    if (auto call = q_ptr->getCall(callId)) {
        auto a = call->firstMedia<Media::Audio>(Media::Media::Direction::OUT);
        if (state)
            a->Media::d_ptr->muteConfirmed();
        else
            a->Media::d_ptr->unmuteConfirmed();
    }
}

void CallModelPrivate::slotVideoMutex( const QString& callId, bool state)
{
   if (auto call = q_ptr->getCall(callId)) {
        auto v = call->firstMedia<Media::Video>(Media::Media::Direction::OUT);
        if (state)
            v->Media::d_ptr->muteConfirmed();
        else
            v->Media::d_ptr->unmuteConfirmed();
   }
}

void CallModelPrivate::slotPeerHold( const QString& callId, bool state)
{
    if (auto call = q_ptr->getCall(callId))
        call->d_ptr->peerHoldChanged(state);
}

void CallModelPrivate::slotRtcpReportReceived(const QString& callId, const MapStringInt& m)
{
}

void CallModelPrivate::slotSelectionChanged(const QModelIndex& idx)
{
    emit q_ptr->selectionChanged(q_ptr->getCall(idx));
    emit q_ptr->selectionSupportsDTMFChanged(q_ptr->supportsDTMF());
}

#include <callmodel.moc>
