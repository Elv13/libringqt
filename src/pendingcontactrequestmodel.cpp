/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
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
#include "pendingcontactrequestmodel.h"

//Qt
#include <QtCore/QDateTime>

//Ring
#include <contactrequest.h>
#include <accountmodel.h>
#include <certificate.h>
#include <account.h>
#include <session.h>
#include "private/pendingcontactrequestmodel_p.h"
#include "person.h"
#include "personmodel.h"
#include "individual.h"
#include "contactmethod.h"
#include "private/contactmethod_p.h"
#include "phonedirectorymodel.h"
#include "private/vcardutils.h"
#include "dbus/configurationmanager.h"
#include "uri.h"


class IncomingContactRequestManager : public QObject
{
    Q_OBJECT
public:
    explicit IncomingContactRequestManager();
    virtual ~IncomingContactRequestManager();

    static IncomingContactRequestManager& instance();

public Q_SLOTS:
    void slotIncomingContactRequest(const QString& accountId, const QString& hash, const QByteArray& payload, time_t time);
    void slotContactAdded(const QString& accountId, const QString& hash, bool confirm);
};

PendingContactRequestModelPrivate::PendingContactRequestModelPrivate(PendingContactRequestModel* p) : QObject(p), q_ptr(p)
{}

PendingContactRequestModel::PendingContactRequestModel(QObject* parent) : QAbstractTableModel(parent),
d_ptr(new PendingContactRequestModelPrivate(this))
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();

   IncomingContactRequestManager& m = IncomingContactRequestManager::instance();

    connect(&configurationManager, &ConfigurationManagerInterface::incomingTrustRequest, &m,
        &IncomingContactRequestManager::slotIncomingContactRequest, Qt::QueuedConnection);

    connect(&configurationManager, &ConfigurationManagerInterface::contactAdded, &m,
        &IncomingContactRequestManager::slotContactAdded, Qt::QueuedConnection);
}

PendingContactRequestModel::~PendingContactRequestModel()
{
}

IncomingContactRequestManager::IncomingContactRequestManager() : QObject(QCoreApplication::instance())
{
    //
}

IncomingContactRequestManager::~IncomingContactRequestManager()
{
    //
}

IncomingContactRequestManager& IncomingContactRequestManager::instance()
{
    auto ins = new IncomingContactRequestManager();

    return *ins;
}

QVariant PendingContactRequestModel::data( const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return {};

   const auto cr = d_ptr->m_lRequests[index.row()];

   switch (role) {
        case (int) Ring::Role::Object:
            return QVariant::fromValue(cr);
        case (int) Ring::Role::ObjectType:
            return QVariant::fromValue(Ring::ObjectType::ContactRequest);
        case (int) Role::Person:
            return QVariant::fromValue(cr->peer());
        case (int) Role::Account:
            return QVariant::fromValue(cr->account());
        case (int) Role::DateTime:
            return cr->date();

   }

   switch(index.column()) {
     case Columns::PEER_ID:
          switch(role) {
             case Qt::DisplayRole:
             return d_ptr->m_lRequests[index.row()]->peer()->individual()->phoneNumbers().first()->bestId();
          }
          break;
      case Columns::TIME:
         switch(role) {
            case Qt::DisplayRole:
               return d_ptr->m_lRequests[index.row()]->date();
         }
         break;
      case Columns::FORMATTED_NAME:
         switch(role) {
            case Qt::DisplayRole:
               return d_ptr->m_lRequests[index.row()]->peer()->formattedName();
         }
         break;
      case Columns::COUNT__:
         switch(role) {
             case Qt::DisplayRole:
                return static_cast<int>(PendingContactRequestModel::Columns::COUNT__);
         }
         break;
   }

   if (cr->peer())
       return cr->peer()->roleData(role);

   return {};
}

int PendingContactRequestModel::rowCount( const QModelIndex& parent ) const
{
   return parent.isValid()? 0 : d_ptr->m_lRequests.size();
}

int PendingContactRequestModel::columnCount( const QModelIndex& parent ) const
{
   return parent.isValid()? 0 : static_cast<int>(PendingContactRequestModel::Columns::COUNT__);
}

Qt::ItemFlags PendingContactRequestModel::flags( const QModelIndex& index ) const
{
   Q_UNUSED(index);
   return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool PendingContactRequestModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

QHash<int,QByteArray> PendingContactRequestModel::roleNames() const
{
    static QHash<int, QByteArray> roles = PersonModel::instance().roleNames();

    static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
    if (!init_flag.test_and_set()) {
        roles[ (int)Role::Person   ] = QByteArray("person"  );
        roles[ (int)Role::Account  ] = QByteArray("account" );
        roles[ (int)Role::DateTime ] = QByteArray("dateTime");
    }

    return roles;
}

void PendingContactRequestModelPrivate::addRequest(ContactRequest* r)
{
   // do not add the same contact request several time
   if(std::any_of(m_lRequests.begin(), m_lRequests.end(),
      [&](ContactRequest* r_){ return *r_ == *r ;})) {
      return;
   }

   // update (remove old add new) contact request if the remoteIds match.
   auto iter = std::find_if(m_lRequests.begin(), m_lRequests.end(), [&](ContactRequest* r_) {
      return (r_->certificate()->remoteId() == r->certificate()->remoteId());
   });

    if(iter)
        removeRequest(*iter);

   q_ptr->beginInsertRows({}, m_lRequests.size(),m_lRequests.size());
   m_lRequests << r;
   q_ptr->endInsertRows();

    QObject::connect(r, &ContactRequest::requestAccepted, this, [this,r]() {
        // the request was handled so it can be removed, from the pending list
        removeRequest(r);

        // it's important to emit after the request was removed.
        emit q_ptr->requestAccepted(r);
    });

    QObject::connect(r, &ContactRequest::requestDiscarded, this, [this,r]() {
        // the request was handled so it can be removed, from the pending list
        removeRequest(r);

        // it's important to emit after the request was removed.
        emit q_ptr->requestDiscarded(r);
    });

    QObject::connect(r, &ContactRequest::requestBlocked, this, [this,r]() {
        // the request was handled so it can be removed, from the pending list
        removeRequest(r);
    });

    emit q_ptr->requestAdded(r);
    emit q_ptr->countChanged();
}

void PendingContactRequestModelPrivate::removeRequest(ContactRequest* r)
{
   const int index = m_lRequests.indexOf(r);

   if (index == -1)
      return;

   q_ptr->beginRemoveRows(QModelIndex(), index, index);
   m_lRequests.removeAt(index);
   q_ptr->endRemoveRows();

   emit q_ptr->countChanged();
}

int PendingContactRequestModel::size() const
{
    return rowCount();
}

ContactRequest*
PendingContactRequestModel::findContactRequestFrom(const ContactMethod* cm) const
{
    auto iter = std::find_if(d_ptr->m_lRequests.constBegin(), d_ptr->m_lRequests.constEnd(),
                             [&](ContactRequest* r){ return r->certificate()->remoteId() == cm->uri(); });

    return (iter != d_ptr->m_lRequests.constEnd()) ? *iter : nullptr;
}

///When a Ring-DHT trust request arrive
void IncomingContactRequestManager::slotIncomingContactRequest(const QString& accountId, const QString& ringID, const QByteArray& payload, time_t time)
{
   Q_UNUSED(payload);

   auto a = Session::instance()->accountModel()->getById(accountId.toLatin1());

   if (!a) {
      qWarning() << "Incoming trust request for unknown account" << accountId;
      return;
   }

   /* do not pass a person before the contact request was added to his model */
   auto r = new ContactRequest(a, ringID, time);
   a->pendingContactRequestModel()->d_ptr->addRequest(r);

   /* Also keep a global list of incoming requests */
   qobject_cast<PendingContactRequestModel*>(
       Session::instance()->accountModel()->incomingContactRequestModel()
   )->d_ptr->addRequest(r);

   auto contactMethod = PhoneDirectoryModel::instance().getNumber(ringID, a);
   r->setPeer(VCardUtils::mapToPersonFromReceivedProfile(contactMethod, payload));
}

void IncomingContactRequestManager::slotContactAdded(const QString& accountId, const QString& hash, bool confirm)
{
    auto a = Session::instance()->accountModel()->getById(accountId.toLatin1());

    if (!a) {
        qWarning() << "Incoming trust request for unknown account" << accountId;
        return;
    }

    auto contactMethod = PhoneDirectoryModel::instance().getNumber(hash, a);

    contactMethod->d_ptr->m_ConfirmationStatus = confirm ?
        ContactMethod::ConfirmationStatus::CONFIRMED :
        ContactMethod::ConfirmationStatus::PENDING;

    emit contactMethod->confirmationChanged(contactMethod->confirmationStatus());
}

#include <pendingcontactrequestmodel.moc>
