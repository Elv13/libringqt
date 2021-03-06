/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                           *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com> *
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

#include "vcardutils.h"

//Qt
#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QMimeData>
#include <QtCore/QMutex>

//Ring
#include "individualdirectory.h"
#include "contactmethod.h"
#include "address.h"
#include "accountmodel.h"
#include "globalinstances.h"
#include "individual.h"
#include "interfaces/pixmapmanipulatori.h"
#include "persondirectory.h"
#include "session.h"

/* https://www.ietf.org/rfc/rfc2045.txt
 * https://www.ietf.org/rfc/rfc2047.txt
 * https://www.ietf.org/rfc/rfc2426.txt
 */

//BEGIN:VCARD
//VERSION:3.0
//N:Gump;Forrest
//FN:Forrest Gump
//ORG:Bubba Gump Shrimp Co.
//TITLE:Shrimp Man
//PHOTO;VALUE=URL;TYPE=GIF:http://www.example.com/dir_photos/my_photo.gif
//TEL;TYPE=WORK,VOICE:(111) 555-1212
//TEL;TYPE=HOME,VOICE:(404) 555-1212
//ADR;TYPE=WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America
//LABEL;TYPE=WORK:100 Waters Edge\nBaytown, LA 30314\nUnited States of America
//ADR;TYPE=HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America
//LABEL;TYPE=HOME:42 Plantation St.\nBaytown, LA 30314\nUnited States of America
//EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com
//REV:20080424T195243Z
//END:VCARD

//BUG VERSION is added multiple time when saving
//BUG BEGIN is added multiple time when saving

//TODO PARSE using a state machine for the PROPERTY:attr:attr = foo
//TODO using a state machine for the BEGIN/END sections
//TODO use raw C++17 Qt is not very useful here (beside qhash)
//TODO export a template version to create objects with custom fields
//TODO do not use Person directly, but keep separated

struct VCardMapper;

typedef void (VCardMapper:: *mapToProperty)(Person*, const QString&, const QByteArray&);

struct VCardMapper final {

   QHash<QByteArray, mapToProperty> m_hHash;

   // Calling getNumber before the Contact is finalized will create duplicates
   struct GetNumberFuture {
      QByteArray uri;
      Person*    c;
      QString    category;
   };

   QHash<Person*, QList<GetNumberFuture> > m_hDelayedCMInserts;
   static QMutex* m_pMutex;

   VCardMapper() {
      m_hHash[VCardUtils::Property::UID] = &VCardMapper::setUid;
      m_hHash[VCardUtils::Property::NAME] = &VCardMapper::setNames;
      m_hHash[VCardUtils::Property::FORMATTED_NAME] = &VCardMapper::setFormattedName;
      m_hHash[VCardUtils::Property::EMAIL] = &VCardMapper::setEmail;
      m_hHash[VCardUtils::Property::ORGANIZATION] = &VCardMapper::setOrganization;
      m_hHash[VCardUtils::Property::TELEPHONE] = &VCardMapper::addContactMethod;
      m_hHash[VCardUtils::Property::ADDRESS] = &VCardMapper::addAddress;
      m_hHash[VCardUtils::Property::PHOTO] = &VCardMapper::setPhoto;
   }

   void apply() {
      // Finalize the transaction, set the ContactsMethods
      // it is done at the end to make sure UID has been set and all CMs
      // are there at once not to mess IndividualDirectory detection

      QMutexLocker locker(m_pMutex);

      for (QHash<Person*, QList<GetNumberFuture>>::iterator i = m_hDelayedCMInserts.begin(); i != m_hDelayedCMInserts.end(); ++i) {

         foreach(const GetNumberFuture& v, i.value()) {
            ContactMethod* cm = Session::instance()->individualDirectory()->getNumber(v.uri,v.c,nullptr,v.category);

            i.key()->individual()->addPhoneNumber(cm);
         }
      }

      m_hDelayedCMInserts.clear();
   }

   void setFormattedName(Person* c,  const QString&, const QByteArray& fn) {
      c->setFormattedName(QString::fromUtf8(fn));
   }

   void setNames(Person* c,  const QString&, const QByteArray& fn) {
      QList<QByteArray> splitted = fn.split(';');
      if (splitted.length() > 0)
         c->setFamilyName(splitted.at(0).trimmed());
      if (splitted.length() > 1)
         c->setFirstName(splitted.at(1).trimmed());
   }

   void setUid(Person* c,  const QString&, const QByteArray& fn) {
      c->setUid(fn);
   }

   void setEmail(Person* c,  const QString&, const QByteArray& fn) {
      c->setPreferredEmail(fn);
   }

   void setOrganization(Person* c, const QString&,  const QByteArray& fn) {
      c->setOrganization(QString::fromUtf8(fn));
   }

   void setPhoto(Person* c, const QString& key, const QByteArray& fn) {
      QByteArray type = "PNG";

      QRegExp rx(QStringLiteral("TYPE=([A-Za-z]*)"));

      while ((rx.indexIn(key, 0)) != -1) {
         type = rx.cap(1).toLatin1();
         break;
      }

      QVariant photo = GlobalInstances::pixmapManipulator().personPhoto(fn,type);
      c->setPhoto(photo);
   }

   void addContactMethod(Person* c, const QString& key, const QByteArray& fn) {
      QByteArray type;

      QRegExp rx(QStringLiteral("TYPE=([A-Za-z,]*)"));

      //VCard spec: it is RECOMMENDED that property and parameter names
      // be upper-case on output.
      rx.setCaseSensitivity(Qt::CaseInsensitive);

      while ((rx.indexIn(key, 0)) != -1) {
         type = rx.cap(1).toLatin1();
         break;
      }

      // TODO: Currently we only support one type (the first on the line) TYPE=WORK,VOICE: <number>
      const QStringList categories = QString(type).split(',');

      {
         QMutexLocker l(m_pMutex);

         m_hDelayedCMInserts[c] << GetNumberFuture {
            fn,
            c,
            categories.size()?categories[0]:QString()
         };
      }
   }

   void addAddress(Person* c, const QString& key, const QByteArray& fn) {
      QList<QByteArray> fields = fn.split(VCardUtils::Delimiter::SEPARATOR_TOKEN[0]);
      QStringList keyFields = key.split(VCardUtils::Delimiter::SEPARATOR_TOKEN);

      if(keyFields.size() < 2 || fields.size() < 7) {
          qDebug() << "Malformatted Address";
          return;
      }

      auto addr = new Address;

      addr->setType        (keyFields[1]                   );
      addr->setAddressLine (QString::fromUtf8(fields[2])   );
      addr->setCity        (QString::fromUtf8(fields[3])   );
      addr->setState       (QString::fromUtf8(fields[4])   );
      addr->setZipCode     (QString::fromUtf8(fields[5])   );
      addr->setCountry     (QString::fromUtf8(fields[6])   );

      c->addAddress(addr);
   }

   bool metacall(Person* c, const QByteArray& key, const QByteArray& value) {
      const QStringList settings = QString(key).split(';');
      if (settings.length() < 1)
         return false;

      if (!m_hHash[settings[0].toLatin1()]) {
         if(key.contains(VCardUtils::Property::PHOTO)) {
            //key must contain additional attributes, we don't need them right now (ENCODING, TYPE...)
            setPhoto(c, key, value);
            return true;
         }

         if(key.contains(VCardUtils::Property::ADDRESS)) {
            addAddress(c, key, value);
            return true;
         }

         if(key.contains(VCardUtils::Property::TELEPHONE)) {
            addContactMethod(c, key, value);
            return true;
         }

         if (key != VCardUtils::Property::VERSION && key != "BEGIN" && key != "END")
            c->addCustomField(key, value);

         return true;
      }
      (this->*(m_hHash[settings[0].toLatin1()]))(c,key,value);
      return true;
   }
};

QMutex* VCardMapper::m_pMutex = new QMutex();
static VCardMapper* vc_mapper = new VCardMapper;

VCardUtils::VCardUtils()
{

}

void VCardUtils::startVCard(const QString& version)
{
   m_vCard << Delimiter::BEGIN_TOKEN;
   addProperty(Property::VERSION, version);
}

void VCardUtils::addProperty(const char* prop, const QString& value)
{
   if (value.isEmpty() || value == QString(';'))
      return;

   m_vCard << (QString::fromUtf8(prop) + ':' + value);
}

void VCardUtils::addProperty(const QByteArray& prop, const QString& value)
{
   if (value.isEmpty() || value == QString(';'))
      return;
   m_vCard << (prop + ':' + value);
}

void VCardUtils::addEmail(const QString& type, const QString& email)
{
   addProperty(QStringLiteral("%1%2%3%4").arg(Property::EMAIL).arg(Delimiter::SEPARATOR_TOKEN).arg(QStringLiteral("TYPE=")).arg(type).toLatin1(), email);
}

void VCardUtils::addAddress(Address* addr)
{
   const QByteArray prop = QStringLiteral("%1%2%3").arg(Property::ADDRESS)
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->type()).toLatin1();

   //First two fiels are left empty for now, they are for Postal box and Extended Address
   QString value = QStringLiteral("%1%2%3%4%5%6%7%8%9%10%11")
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->addressLine())
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->city())
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->state())
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->zipCode())
         .arg(Delimiter::SEPARATOR_TOKEN)
         .arg(addr->country());

   addProperty(prop, value);
}

void VCardUtils::addContactMethod(const QString& type, const QString& num)
{
   const auto prop = QString(Property::TELEPHONE) + QString(Delimiter::SEPARATOR_TOKEN) + type;
   addProperty(prop.toLatin1(), num);
}

void VCardUtils::addPhoto(const QByteArray img)
{
    const auto base64 = img.toBase64().trimmed();
    static auto header = QString::fromUtf8(Property::PHOTO) +
                QString::fromUtf8(Delimiter::SEPARATOR_TOKEN) +
                "ENCODING=BASE64" +
                QString::fromUtf8(Delimiter::SEPARATOR_TOKEN) +
                "TYPE=PNG:";

    const int offset = 76 - header.size();

    // Images are big, something less than 3 lines is impossible
    if (base64.size() < 3*80)
        return;

    m_vCard << (header + base64.left(offset));

    // QByteArray has built-in anti overflow, no need to replicate the logic
    for (int i = offset; i < base64.size(); i += std::min(base64.size() - i, 75))
        m_vCard << (' ' + base64.mid(i, 75));
}

const QByteArray VCardUtils::endVCard()
{
   m_vCard << Delimiter::END_TOKEN;
   const QString result = m_vCard.join(QString::fromUtf8(Delimiter::END_LINE_TOKEN));
   return result.toUtf8();
}

QList< Person* > VCardUtils::loadDir(const QUrl& path, bool& ok, QHash<const Person*,QString>& paths)
{
   QList< Person* > ret;

   QDir dir(path.toString());
   if (!dir.exists())
      ok = false;
   else {
      ok = true;
      for (const QString& file : dir.entryList({"*.vcf"}, QDir::Files)) {
         Person* p = new Person();
         mapToPerson(p,QUrl(dir.absoluteFilePath(file)));
         ret << p;
         paths[p] = dir.absoluteFilePath(file);
      }
   }

   return ret;
}

QList<QPair< QByteArray, QByteArray> > VCardUtils::
parseFields(const QByteArray& all)
{
    QList<QPair< QByteArray, QByteArray> > l;

    QByteArray previousKey,previousValue;
    const QList<QByteArray> lines = all.split('\n');

    // Each property can be spread over multiple lines, but it is not known
    // until those lines are processed, hence the weirdness.
    for (const QByteArray& property : qAsConst(lines)) {
        //Ignore empty lines
        if (property.size()) {
            //Some properties are over multiple lines
            if (property[0] == ' ' && previousKey.size() > 1) {
                // To avoid a deep copy as QByteArray::right() produces, use a the
                // data directly.
                previousValue.append(
                    QByteArray::fromRawData(property.data()+1, property.size()-1)
                );
            }
            else {
                if (previousKey.size())
                    l << QPair< QByteArray, QByteArray> {previousKey, previousValue.trimmed()};

                //Do not use split, URIs can have : in them
                const int dblptPos = property.indexOf(':');
                const QByteArray k(property.left(dblptPos)),v(property.right(property.size()-dblptPos-1));


                previousKey   = k;
                previousValue = v;
            }
        }
    }

    if (previousKey.size())
        l << QPair< QByteArray, QByteArray> {previousKey, previousValue.trimmed()};

    return l;
}

//TODO use QStringRef
bool VCardUtils::mapToPerson(Person* p, const QByteArray& all, QList<Account*>* accounts)
{
    const auto fields = parseFields(all);

    for (const auto& pair : qAsConst(fields)) {
        if (pair.first.size())
            vc_mapper->metacall(p, pair.first, pair.second);

        //Link with accounts
        if(pair.first == VCardUtils::Property::X_RINGACCOUNT) {
            if (accounts) {
                Account* a = Session::instance()->accountModel()->getById(pair.second,true);
                if(!a) {
                    qDebug() << "Could not find account: " << pair.second;
                    continue;
                }

                (*accounts) << a;
            }
        }
    }

    vc_mapper->apply();

    return true;
}

bool VCardUtils::mapToPerson(Person* p, const QUrl& path, QList<Account*>* accounts)
{

   QFile file(path.toString());
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qDebug() << "Error opening vcard: " << path;
      return false;
   }

   const QByteArray all = file.readAll();

   return mapToPerson(p,all,accounts);
}

Person* VCardUtils::mapToPerson(const QHash<QByteArray, QByteArray>& vCard, QList<Account*>* accounts)
{
    auto existingPerson = Session::instance()->personDirectory()->getPersonByUid(vCard[Property::UID]);
    auto personMapped = existingPerson == nullptr ? new Person() : existingPerson;

    QHashIterator<QByteArray, QByteArray> it(vCard);
    while (it.hasNext()) {
        it.next();
        if (it.key() == VCardUtils::Property::X_RINGACCOUNT) {
            if (accounts) {
                auto acc = Session::instance()->accountModel()->getById(it.value().trimmed(),true);
                if (!acc) {
                    qDebug() << "Could not find account: " << it.value().trimmed();
                    continue;
                }
                (*accounts) << acc;
           }
        }
        vc_mapper->metacall(personMapped, it.key(), it.value().trimmed());
    }

    vc_mapper->apply();
    return personMapped;
}

/**
 * Create a new Person from the vCard payload.
 */
Person* VCardUtils::mapToPerson(const QByteArray& payload, bool purgeUntrusted)
{
    auto person = new Person();

    const auto vCard = toHashMap(payload);

    QHashIterator<QByteArray, QByteArray> it(vCard);
    while (it.hasNext()) {
        it.next();

        // Only keep phone numbers
        if (it.key() == VCardUtils::Property::TELEPHONE) {
            URI uri(it.value());

            if (!(uri.charSets() & URI::CharSet::PHONE))
                continue;
        }

        // Do not trust UID from incoming vcard, we can't be sure that its unique, we will generate our own
        if (purgeUntrusted && it.key() == VCardUtils::Property::UID) continue;

        // This shouldn't be there anyways, but ignore it if it is
        if (purgeUntrusted && it.key() == VCardUtils::Property::X_RINGACCOUNT) continue;

        vc_mapper->metacall(person, it.key(), it.value().trimmed());
    }

    vc_mapper->apply();

    return person;
}

/**
 * There are many instances when we can receive a payload which contains a vCard. The vCard is
 * received from a ContactMethod so we want to check if this CM already has a Person we want to
 * update from the new vCard, or else create a new Person. We also can't trust the contents of the
 * vCard, so we ignore any idenitication info and only basically use the name and the photo.
 *
 * @param contactMethod, the contactMethod from which we received the profile
 * @param payload, the profile we received
 */
Person* VCardUtils::mapToPersonFromReceivedProfile(ContactMethod *contactMethod, const QByteArray& payload)
{
    /* TODO: here we only check if this ContatMethod has a Person already; however it possible that
     *       another CM with the same RingID (but associated with a different Account) has a profile
     *       already; in this case we probably want to update that Person as well (since its coming)
     *       from the same RingID, or maybe even use that Person and add this CM to its list of
     *       numbers
     */
    Person* person = nullptr;//contactMethod->contact();
    if (!person) {
        person = contactMethod->contact() ? contactMethod->contact() : new Person();

        person->individual()->addPhoneNumber(contactMethod);
        contactMethod->setPerson(person);
    }
    const auto vCard = toHashMap(payload);

    QHashIterator<QByteArray, QByteArray> it(vCard);
    while (it.hasNext()) {
        it.next();

        // Only keep phone numbers
        if (it.key() == VCardUtils::Property::TELEPHONE) {
            URI uri(it.value());

            if (!(uri.charSets() & URI::CharSet::PHONE))
                continue;
        }

        // Do not trust UID from incoming vcard, we can't be sure that its unique, we will generate our own
        if (it.key() == VCardUtils::Property::UID) continue;

        // This shouldn't be there anyways, but ignore it if it is
        if (it.key() == VCardUtils::Property::X_RINGACCOUNT) continue;

        vc_mapper->metacall(person, it.key(), it.value().trimmed());
    }

    vc_mapper->apply();

    return person;
}

QHash<QByteArray, QByteArray> VCardUtils::toHashMap(const QByteArray& content)
{
    QHash<QByteArray, QByteArray> vCard;
    QByteArray previousKey,previousValue;
    const QList<QByteArray> lines = content.split('\n');

    foreach (const QByteArray& property, lines) {
        //Ignore empty lines
        if (property.size()) {
            //Some properties are over multiple lines
            if (property[0] == ' ' && previousKey.size()) {
                previousValue += property.right(property.size()-1);
            }

            //Do not use split, URIs can have : in them
            const int dblptPos = property.indexOf(':');
            const QByteArray k(property.left(dblptPos)),v(property.right(property.size()-dblptPos-1));
            vCard[k] = v;
        }
    }
    return vCard;
}

//TODO get the daemon implementation, port it to Qt
QByteArray VCardUtils::wrapInMime(const QString& mimeType, const QByteArray& payload)
{
   QByteArray a;
   a += "MIME-Version: 1.0\n";
   a += "Content-Type: multipart/mixed; boundary=content\n";
   a += "\n";
   a += "--content\n";
   a += "Content-Type: "+mimeType+"\n";
   a += "\n";
   a += payload+"\n";
   a += "--content--\0";

   return a;
}

QMap<QString, QString> VCardUtils::parseMimeAttributes(const QString& mimeType)
{
    QMap<QString, QString> ret;

    const auto& list = mimeType.split(';');
    if (list.size() < 2)
        return {};

    const auto& pairs = list[1].split(',');
    for(const auto& p: pairs) {
        const auto& kv = p.split('=');
        ret[kv[0].trimmed()] = kv[1];
    }

    return ret;
}
