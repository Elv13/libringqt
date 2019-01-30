/************************************************************************************
 *   Copyright (C) 2014-2016 by Savoir-faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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
#include "notificationstatecollection.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>

//Ring
#include <call.h>
#include <account.h>
#include <session.h>
#include <contactmethod.h>
#include <accountmodel.h>
#include <persondirectory.h>
#include <individualdirectory.h>
#include <picocms/collectioneditor.h>
#include <globalinstances.h>
#include <interfaces/pixmapmanipulatori.h>
#include <private/contactmethod_p.h>

namespace Serializable {
   class NotificationStateNode
   {
   public:
      mutable Account* account  ;
      ContactMethod*   cm       ;
      Person*          contact  ;

      void read (const QJsonObject &json);
      void write(QJsonObject       &json) const;
   };
}

class NotificationStateEditor final : public CollectionEditor<ContactMethod>
{
public:
   NotificationStateEditor(CollectionMediator<ContactMethod>* m)
     : CollectionEditor<ContactMethod>(m),m_Tracked(false) {}
   virtual bool save       ( const ContactMethod* item ) override;
   virtual bool remove     ( const ContactMethod* item ) override;
   virtual bool addNew     ( ContactMethod*       item ) override;
   virtual bool contains   (const ContactMethod*  item ) const override;
   virtual bool addExisting( const ContactMethod* item ) override;

   //Attributes
   QVector<ContactMethod*> m_lNumbers;
   QList<Serializable::NotificationStateNode> m_Nodes;
   bool m_Tracked;

private:
   virtual QVector<ContactMethod*> items() const override;
};

class NotificationStateCollectionPrivate
{
public:

   //Attributes
   constexpr static const char FILENAME[] = "notificationstate.json";
};

constexpr const char NotificationStateCollectionPrivate::FILENAME[];

NotificationStateCollection::NotificationStateCollection(CollectionMediator<ContactMethod>* mediator) :
   CollectionInterface(new NotificationStateEditor(mediator)), d_ptr(new NotificationStateCollectionPrivate())
{
    QTimer::singleShot(0, [this]() {load();});
}


NotificationStateCollection::~NotificationStateCollection()
{
   delete d_ptr;
}

bool NotificationStateCollection::load()
{
   QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
              + QLatin1Char('/')
              + NotificationStateCollectionPrivate::FILENAME);
   if ( file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
      NotificationStateEditor* e = static_cast<NotificationStateEditor*>(editor<ContactMethod>());
      const QByteArray content = file.readAll();
      QJsonDocument loadDoc = QJsonDocument::fromJson(content);
      QJsonArray a = loadDoc.array();

      for (int i = 0; i < a.size(); ++i) {
         QJsonObject o = a[i].toObject();
         Serializable::NotificationStateNode n;
         n.read(o);

         e->addExisting(n.cm);

         n.cm->d_ptr->m_NotificationPolicy = (ContactMethod::NotificationPolicy) 1;

         e->m_Nodes << n;
      }

      return true;
   }
   else
      qWarning() << "NotificationState doesn't exist or is not readable";
   return false;
}

bool NotificationStateEditor::save(const ContactMethod* number)
{
   Q_UNUSED(number)

   static QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + QLatin1Char('/')
        + NotificationStateCollectionPrivate::FILENAME;

   QFile file(path);

   if (file.open(QIODevice::WriteOnly | QIODevice::Text) ) {

      QJsonArray a;

      foreach (const Serializable::NotificationStateNode& g, m_Nodes) {
         QJsonObject o;
         g.write(o);
         a.append(o);
      }

      QJsonDocument doc(a);

      QTextStream streamFileOut(&file);
      streamFileOut << doc.toJson();
      streamFileOut.flush();
      file.close();

      return true;
   }
   else
      qWarning() << "Unable to save the notification state";

   return false;
}

bool NotificationStateEditor::remove(const ContactMethod* item)
{
   Q_UNUSED(item)

   int idx = -1;

   if ((idx = m_lNumbers.indexOf(const_cast<ContactMethod*>(item))) != -1) {
      m_lNumbers.removeAt(idx);
      mediator()->removeItem(item);

      for (int i =0;i<m_Nodes.size();i++) {
         if (m_Nodes[i].cm == item) {
            m_Nodes.removeAt(i);
            break;
         }
      }

      return save(nullptr);
   }
   return false;
}

bool NotificationStateEditor::contains(const ContactMethod* item) const
{
    return m_lNumbers.contains(const_cast<ContactMethod*>(item));
}

bool NotificationStateEditor::addNew( ContactMethod* number)
{
   if (!contains(number)) {
      number->setTracked(m_Tracked);
      Serializable::NotificationStateNode n;

      n.cm = number;
      n.account = number->account();
      n.contact = number->contact();
      m_Nodes << n;

      addExisting(number);

      if (!save(number))
         qWarning() << "Unable to save notification state";
   }

   return save(number);
}

bool NotificationStateEditor::addExisting(const ContactMethod* item)
{
   m_lNumbers << const_cast<ContactMethod*>(item);
   mediator()->addItem(item);
   return false;
}

QVector<ContactMethod*> NotificationStateEditor::items() const
{
   return m_lNumbers;
}

QString NotificationStateCollection::name () const
{
   return QObject::tr("Notification State");
}

QString NotificationStateCollection::category () const
{
   return QObject::tr("Notification");
}

QVariant NotificationStateCollection::icon() const
{
    return {};
}

bool NotificationStateCollection::isEnabled() const
{
   return true;
}

bool NotificationStateCollection::reload()
{
   return false;
}

FlagPack<CollectionInterface::SupportedFeatures> NotificationStateCollection::supportedFeatures() const
{
   return
      CollectionInterface::SupportedFeatures::NONE      |
      CollectionInterface::SupportedFeatures::LOAD      |
      CollectionInterface::SupportedFeatures::CLEAR     |
      CollectionInterface::SupportedFeatures::ADD       |
      CollectionInterface::SupportedFeatures::MANAGEABLE|
      CollectionInterface::SupportedFeatures::REMOVE    ;
}

bool NotificationStateCollection::clear()
{
   return QFile::remove(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                        + QLatin1Char('/')
                        + NotificationStateCollectionPrivate::FILENAME);
}

QByteArray NotificationStateCollection::id() const
{
   return "notificationstate";
}


void NotificationStateCollection::setPresenceTracked(bool tracked)
{
   static_cast<NotificationStateEditor*>(editor<ContactMethod>())->m_Tracked = tracked;
}

bool NotificationStateCollection::isPresenceTracked() const
{
   return static_cast<NotificationStateEditor*>(editor<ContactMethod>())->m_Tracked;
}

void Serializable::NotificationStateNode::read(const QJsonObject &json)
{
   const QString&    uri       = json[ QStringLiteral("uri")       ].toString()           ;
   const QByteArray& accountId = json[ QStringLiteral("accountId") ].toString().toLatin1();
   const QByteArray& contactId = json[ QStringLiteral("contactId") ].toString().toLatin1();

   account = accountId.isEmpty()?nullptr:Session::instance()->accountModel()->getById( accountId );
   contact = contactId.isEmpty()?nullptr:Session::instance()->personDirectory()->getPersonByUid( contactId );
   cm      = uri.isEmpty()?nullptr:Session::instance()->individualDirectory()->getNumber( uri, contact, account);
}

void Serializable::NotificationStateNode::write(QJsonObject& json) const
{
   if (!account)
      account = cm->account();

   json[ QStringLiteral("uri")       ] = cm->uri()                      ;
   json[ QStringLiteral("accountId") ] = account?account->id():QString();
   json[ QStringLiteral("contactId") ] = contact?contact->uid ():QString();
}
