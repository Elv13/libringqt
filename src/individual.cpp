/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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
#include "individual.h"

// Ring
#include <contactmethod.h>
#include <numbercategory.h>
#include <categorizedhistorymodel.h>
#include <phonedirectorymodel.h>
#include <person.h>
#include <account.h>
#include <accountmodel.h>
#include <certificatemodel.h>
#include <peertimelinemodel.h>

class IndividualPrivate final : public QObject
{
    Q_OBJECT
public:

    // This is a private class, no need for d_ptr
    Person* m_pPerson;
    QMetaObject::Connection m_cBeginCB;
    QMetaObject::Connection m_cEndCB;
    TemporaryContactMethod* m_pTmpCM {nullptr};

    ContactMethod*           m_LastUsedCM {nullptr};
    QVector<ContactMethod*>  m_HiddenContactMethods;
    Person::ContactMethods   m_Numbers             ;

    QWeakPointer<PeerTimelineModel> m_TimelineModel;

    Individual* q_ptr;

public Q_SLOTS:
    void slotLastUsedTimeChanged(::time_t t       );
    void slotLastContactMethod  (ContactMethod* cm);
    void slotCallAdded          (Call *call       );
    void slotUnreadCountChanged (                 );
};

Individual::Individual(Person* parent) :
    QAbstractListModel(const_cast<Person*>(parent)), d_ptr(new IndividualPrivate)
{
    d_ptr->m_pPerson = parent;
    d_ptr->q_ptr = this;

    // Row inserted/deleted can be implemented later
    d_ptr->m_cBeginCB = connect(this, &Individual::phoneNumbersAboutToChange, this, [this](){beginResetModel();});
    d_ptr->m_cEndCB   = connect(this, &Individual::phoneNumbersChanged      , this, [this](){endResetModel  ();});
}

Individual::Individual(ContactMethod* parent)
{
    Q_ASSERT(false);
}

Individual::~Individual()
{
    disconnect( d_ptr->m_cEndCB   );
    disconnect( d_ptr->m_cBeginCB );

    setEditRow(false);
}

QVariant Individual::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    if (d_ptr->m_pTmpCM && index.row() >= phoneNumbers().size()) {
        Q_ASSERT(index.row() == phoneNumbers().size());

        switch(role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
                return d_ptr->m_pTmpCM->uri();
            case (int) ContactMethod::Role::CategoryName:
                return d_ptr->m_pTmpCM->category() ?
                    d_ptr->m_pTmpCM->category()->name() : QString();
            case static_cast<int>(Call::Role::ContactMethod):
            case static_cast<int>(Ring::Role::Object):
                return QVariant::fromValue(d_ptr->m_pTmpCM);
        }

        return {};
    }

    // As this model is always associated with the person, the relevant icon
    // is the phone number type (category)
    if (index.isValid() && role == Qt::DecorationRole) {
        return phoneNumbers()[index.row()]->category()->icon();
    }

    return index.isValid() ? phoneNumbers()[index.row()]->roleData(role) : QVariant();
}

bool Individual::setData( const QModelIndex& index, const QVariant &value, int role)
{
    if (index.row() == rowCount()) {
        beginInsertRows({}, rowCount(), rowCount());
        setEditRow(true);
        endInsertRows();
    }
    else if (!index.isValid())
        return false;

    switch(role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            d_ptr->m_pTmpCM->setUri(value.toString());
            emit dataChanged(index, index);
            return true;
    }

    return false;
}

int Individual::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : phoneNumbers().size() + (
        d_ptr->m_pTmpCM ? 1 : 0
    );
}

// Re-implement to allow adding new rows
QModelIndex Individual::index(int row, int col, const QModelIndex& parent) const
{
    if (col || parent.isValid())
        return {};

    if (row > phoneNumbers().size()+(d_ptr->m_pTmpCM ? -1 : 0))
        return {};

    return createIndex(row, 0, row);
}

bool Individual::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row >= rowCount())
        return false;

    if (row == rowCount()-1) {
        setEditRow(false);
        emit layoutChanged();
        return true;
    }

    auto pn = phoneNumbers();
    for (int i = row; i < row+count; i++)
        pn.remove(row);

    setPhoneNumbers(pn);

    emit layoutChanged();

    return true;
}

QHash<int,QByteArray> Individual::roleNames() const
{
    static QHash<int, QByteArray> roles = PhoneDirectoryModel::instance().roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;
    }

    return roles;
}

bool Individual::hasEditRow() const
{
    return d_ptr->m_pTmpCM;
}

void Individual::setEditRow(bool v)
{
    if (v && !d_ptr->m_pTmpCM) {
        d_ptr->m_pTmpCM = new TemporaryContactMethod();
    }
    else if ((!v) && d_ptr->m_pTmpCM)
        delete d_ptr->m_pTmpCM;

    emit hasEditRowChanged(v);
    emit layoutChanged();
}

void Individual::registerContactMethod(ContactMethod* m)
{
    d_ptr->m_HiddenContactMethods << m;

    emit relatedContactMethodsAdded(m);

    connect(m, &ContactMethod::lastUsedChanged, d_ptr, &IndividualPrivate::slotLastUsedTimeChanged);
    connect(m, &ContactMethod::callAdded, d_ptr, &IndividualPrivate::slotCallAdded);

    if ((!d_ptr->m_LastUsedCM) || m->lastUsed() > d_ptr->m_LastUsedCM->lastUsed())
        d_ptr->slotLastContactMethod(m);
}

///Get the phone number list
QVector<ContactMethod*> Individual::phoneNumbers() const
{
   return d_ptr->m_Numbers;
}

/**
 * Over time, more ContactMethods are associated with a Person. For example,
 * if the person move and get a new phone number (and lose the old one) or
 * change country. Other example could be losing the Ring account private
 * key and having to create a new one. There is other accidental instances
 * caused by various race conditions.
 */
QVector<ContactMethod*> Individual::relatedContactMethods() const
{
    return d_ptr->m_HiddenContactMethods;
}

///Set the phone number (type and number)
void Individual::setPhoneNumbers(const QVector<ContactMethod*>& numbers)
{
   //TODO make a diff instead of full reload
   const auto dedup = QSet<ContactMethod*>::fromList(numbers.toList());

   emit phoneNumbersAboutToChange();

   for (ContactMethod* n : qAsConst(d_ptr->m_Numbers)) {
      disconnect(n,SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
      disconnect(n,SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
      disconnect(n, &ContactMethod::lastUsedChanged, d_ptr, &IndividualPrivate::slotLastUsedTimeChanged);
      disconnect(n, &ContactMethod::unreadTextMessageCountChanged, d_ptr, &IndividualPrivate::slotUnreadCountChanged);
      disconnect(n, &ContactMethod::callAdded, d_ptr, &IndividualPrivate::slotCallAdded);
   }

   d_ptr->m_Numbers = QVector<ContactMethod*>::fromList(dedup.toList());

   for (ContactMethod* n : qAsConst(d_ptr->m_Numbers)) {
      connect(n,SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
      connect(n,SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
      connect(n, &ContactMethod::lastUsedChanged, d_ptr, &IndividualPrivate::slotLastUsedTimeChanged);
      connect(n, &ContactMethod::unreadTextMessageCountChanged, d_ptr, &IndividualPrivate::slotUnreadCountChanged);
      connect(n, &ContactMethod::callAdded, d_ptr, &IndividualPrivate::slotCallAdded);
   }

   emit phoneNumbersChanged();

   //Allow incoming calls from those numbers
   const QList<Account*> ringAccounts = AccountModel::instance().getAccountsByProtocol(Account::Protocol::RING);
   QStringList certIds;
   for (ContactMethod* n : qAsConst(d_ptr->m_Numbers)) {
      if (n->uri().protocolHint() == URI::ProtocolHint::RING)
         certIds << n->uri().userinfo(); // certid must only contain the hash, no scheme
   }

   for (const QString& hash : qAsConst(certIds)) {
      Certificate* cert = CertificateModel::instance().getCertificateFromId(hash);
      if (cert) {
         for (Account* a : ringAccounts) {
            if (a->allowIncomingFromContact())
               a->allowCertificate(cert);
         }
      }
   }
}

ContactMethod* Individual::addPhoneNumber(ContactMethod* cm)
{
    if ((!cm) || cm->type() == ContactMethod::Type::BLANK)
        return nullptr;

    if (Q_UNLIKELY(d_ptr->m_Numbers.indexOf(cm) != -1)) {
        qWarning() << this << "already has the phone number" << cm;
        return cm;
    }

    if (cm->type() == ContactMethod::Type::TEMPORARY)
        cm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(cm)
        );

    if (Q_UNLIKELY(cm->contact() && *cm->contact() == *d_ptr->m_pPerson)) {
        qWarning() << "Adding a phone number to" << this << "already owned by" << cm->contact();
    }

    d_ptr->m_Numbers << cm;

    emit phoneNumbersChanged();

    if (d_ptr->m_HiddenContactMethods.indexOf(cm) != -1) {
        d_ptr->m_HiddenContactMethods.removeAll(cm);
        emit relatedContactMethodsRemoved(cm);
    }

    return cm;
}

ContactMethod* Individual::removePhoneNumber(ContactMethod* cm)
{
    if (Q_UNLIKELY(!cm)) {
        return nullptr;
    }

    const int idx = d_ptr->m_Numbers.indexOf(cm);

    if (Q_UNLIKELY(idx == -1)) {
        qWarning() << this << "trying to replace a phone number that doesn't exist";
        return nullptr;
    }

    phoneNumbersAboutToChange();
    d_ptr->m_Numbers.remove(idx);
    phoneNumbersChanged();

    d_ptr->m_HiddenContactMethods << cm;

    emit relatedContactMethodsAdded(cm);

    return cm;
}

/// ContactMethods URI are immutable, they need to be replaced when edited
ContactMethod* Individual::replacePhoneNumber(ContactMethod* old, ContactMethod* newCm)
{
    if (Q_UNLIKELY(!newCm)) {
        qWarning() << this << "trying to replace a phone number with nothing";
        return nullptr;
    }

    const int idx = d_ptr->m_Numbers.indexOf(old);

    if (Q_UNLIKELY((!old) || idx == -1)) {
        qWarning() << this << "trying to replace a phone number that doesn't exist";
        return nullptr;
    }

    if (old == newCm) {
        qWarning() << "Trying to replace a phone number with itself";
        return old;
    }

    if (newCm->type() == ContactMethod::Type::TEMPORARY)
        newCm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(newCm)
        );

    phoneNumbersAboutToChange();

    d_ptr->m_Numbers.replace(idx, newCm);

    d_ptr->m_HiddenContactMethods << newCm;

    emit relatedContactMethodsAdded(old);

    phoneNumbersChanged();

    return newCm;
}

/// Get the last ContactMethod used with that person.
ContactMethod* Individual::lastUsedContactMethod() const
{
    return d_ptr->m_LastUsedCM;
}

bool Individual::hasHiddenContactMethods() const
{
    return !d_ptr->m_HiddenContactMethods.isEmpty();
}

/**
 * Returns a timeline model if and only if a person phone number was contacted.
 *
 * For "self manager" Person objects, it means it is *required* to set the phone
 * numbers right.
 */
QSharedPointer<QAbstractItemModel> Individual::timelineModel() const
{
//     // See if one of the contact methods already built one
//     auto begin(d_ptr->m_Numbers.constBegin()), end(d_ptr->m_Numbers.constEnd());
//
//     auto cmi = std::find_if(begin, end, [](ContactMethod* cm) {
//         return cm->d_ptr->m_TimelineModel;
//     });
//
//     if (cmi != end)
//         return (*cmi)->timelineModel();
//
//     // Too bad, build one
//     if (!d_ptr->m_Numbers.isEmpty())
//         return  d_ptr->m_Numbers.constFirst()->timelineModel();
//
//     // This person was never contacted
//     return QSharedPointer<QAbstractItemModel>();
    if (!d_ptr->m_TimelineModel) {
        auto p = QSharedPointer<PeerTimelineModel>(new PeerTimelineModel(d_ptr->m_pPerson));
        d_ptr->m_TimelineModel = p;
        return p;
    }

    return d_ptr->m_TimelineModel;
}

void IndividualPrivate::slotLastUsedTimeChanged(::time_t t)
{
   Q_UNUSED(t)

   auto cm = qobject_cast<ContactMethod*>(sender());

   // This function should always be called by a contactmethod signal. Otherwise
   // the timestamp comes from an unknown source and cannot be traced.
   Q_ASSERT(cm);

   slotLastContactMethod(cm);
}

void IndividualPrivate::slotLastContactMethod(ContactMethod* cm)
{
    if (cm && q_ptr->lastUsedTime() > cm->lastUsed())
        return;

    m_LastUsedCM = cm;

    emit q_ptr->lastUsedTimeChanged(cm ? cm->lastUsed() : 0);
}

void IndividualPrivate::slotCallAdded(Call *call)
{
    emit q_ptr->callAdded(call);
}

void IndividualPrivate::slotUnreadCountChanged()
{
    emit q_ptr->unreadCountChanged(0);
}

/// Like std::any_of
bool Individual::matchExpression(const std::function<bool(ContactMethod*)>& functor)
{
    for (const auto l : {phoneNumbers(), relatedContactMethods()}) {
        for (const auto cm : qAsConst(l))
            if (functor(cm))
                return true;
    }

    return false;
}

void Individual::forAllNumbers(const std::function<void(ContactMethod*)> functor, bool indludeHidden)
{
    const auto nbs = phoneNumbers();
    for (const auto cm : qAsConst(nbs))
        functor(cm);

    if (indludeHidden) {
        const auto nb2 = relatedContactMethods();
        for (const auto cm : qAsConst(nb2))
            functor(cm);
    }
}


/** Get the last time this person was contacted.
 * This method returns zero when the person was never contacted.
 *  @todo Implement some caching
 */
time_t Individual::lastUsedTime() const
{
    if (!lastUsedContactMethod())
        return 0;

    return lastUsedContactMethod()->lastUsed();
}

#include <individual.moc>
