/****************************************************************************
 *   Copyright (C) 2016 by Savoir-faire Linux                               *
 *   Copyright (C) 2017 by Bluesystems                                      *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>      *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                    *
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
#include "localprofilecollection.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <QtCore/QDateTime>

//Ring
#include "private/vcardutils.h"
#include "account.h"
#include "session.h"
#include "accountmodel.h"
#include "persondirectory.h"
#include "person.h"

class LocalProfileCollectionPrivate
{
public:
    void setupDefaultProfile();
    static QString profileDir();
    LocalProfileCollection* q_ptr;
};

QString LocalProfileCollectionPrivate::profileDir()
{
    static QString dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral("/profiles/");
    return dir;
}

LocalProfileCollection::LocalProfileCollection(CollectionMediator<Person>* mediator) :
FallbackPersonCollection(mediator, LocalProfileCollectionPrivate::profileDir(), false), d_ptr(new LocalProfileCollectionPrivate)
{
    d_ptr->q_ptr = this;
}

LocalProfileCollection::~LocalProfileCollection()
{
    delete d_ptr;
}

QString LocalProfileCollection::name () const
{
    return QObject::tr("Local profiles");
}

QString LocalProfileCollection::category () const
{
    return QObject::tr("Profile Collection");
}

bool LocalProfileCollection::isEnabled() const
{
   return true;
}

static QVector<Account*> getAccountList(const QList<QByteArray>& accs)
{
    QVector<Account*> ret;
    for (const auto& id : qAsConst(accs)) {
        if (auto a = Session::instance()->accountModel()->getById(id))
            ret << a;
    }

    return ret;
}

bool LocalProfileCollection::load()
{
    FallbackPersonCollection::load();

    // No profiles found, create one
    if (size() == 0)
        d_ptr->setupDefaultProfile();

    return true;
}

bool LocalProfileCollection::reload()
{
    return true;
}

QByteArray LocalProfileCollection::id() const
{
    return "lpc";
}

void LocalProfileCollectionPrivate::setupDefaultProfile()
{
    // Select a better name
    auto name = QObject::tr("Default");

    const int accountCount = Session::instance()->accountModel()->size();

    for (int i = 0; i < accountCount; i++) {
        const auto a = (*Session::instance()->accountModel())[i];
        if (!a->registeredName().isEmpty()) {
            name = a->registeredName();
            break;
        }
        else if (!a->alias().isEmpty()) {
            name = a->alias();
            break;
        }
    }

    auto profile = new Person();
    profile->setFormattedName(name);
    profile->addCustomField(VCardUtils::Property::X_RINGDEFAULTACCOUNT, "1");

    for (int i = 0 ; i < Session::instance()->accountModel()->size() ; i++)
        (*Session::instance()->accountModel())[i]->setProfile(profile);

    profile->setCollection(q_ptr);
    q_ptr->editor<Person>()->addNew(profile);
}
