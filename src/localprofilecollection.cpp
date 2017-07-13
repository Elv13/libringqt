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
#include "profile.h"
#include "private/vcardutils.h"
#include "account.h"
#include "accountmodel.h"
#include "personmodel.h"
#include "person.h"
#include "fallbackpersoncollection.h"

/// Avoid copy paste
class ProfileVCardCollection final : public FallbackPersonCollection
{
public:
    explicit ProfileVCardCollection(CollectionMediator<Person>* mediator, const QString& path = {});
    virtual ~ProfileVCardCollection() {}

    virtual QString    name     () const override;
    virtual QString    category () const override;
    virtual QByteArray id       () const override;
    virtual FlagPack<CollectionInterface::SupportedFeatures> supportedFeatures() const override;
};

ProfileVCardCollection::
ProfileVCardCollection(CollectionMediator<Person>* mediator, const QString& path) :
    FallbackPersonCollection(mediator, path, false)
{
}

QString ProfileVCardCollection::name () const
{
    return QObject::tr("Own profiles vCard collection");
}

QString ProfileVCardCollection::category () const
{
    return QObject::tr("Profile Collection");
}

QByteArray ProfileVCardCollection::id() const
{
   return "pvcc";
}

FlagPack<CollectionInterface::SupportedFeatures> ProfileVCardCollection::supportedFeatures() const
{
   return
      CollectionInterface::SupportedFeatures::NONE   |
      CollectionInterface::SupportedFeatures::LOAD   |
      CollectionInterface::SupportedFeatures::CLEAR  |
      CollectionInterface::SupportedFeatures::SAVE   |
      CollectionInterface::SupportedFeatures::EDIT   |
      CollectionInterface::SupportedFeatures::REMOVE |
      CollectionInterface::SupportedFeatures::ADD    ;
}

class LocalProfileCollectionPrivate
{
public:
    void setupDefaultProfile();

    LocalProfileCollection* q_ptr;
};

class LocalProfileEditor final : public CollectionEditor<Profile>
{
public:
   LocalProfileEditor(CollectionMediator<Profile>* m, LocalProfileCollection* parent);
   virtual bool save       ( const Profile* item ) override;
   virtual bool remove     ( const Profile* item ) override;
   virtual bool edit       ( Profile*       item ) override;
   virtual bool addNew     ( Profile*       item ) override;
   virtual bool addExisting( const Profile* item ) override;

    ProfileVCardCollection* m_pVCardCollection;

private:
    virtual QVector<Profile*> items() const override;

    //Helpers
    QString path(const Profile* p) const;

    //Attributes
    QVector<Profile*> m_lItems;
    LocalProfileCollection* m_pCollection;
};

LocalProfileEditor::LocalProfileEditor(CollectionMediator<Profile>* m, LocalProfileCollection* parent) :
CollectionEditor<Profile>(m),m_pCollection(parent)
{
    const QString profilesDir(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/"
    );

    m_pVCardCollection = PersonModel::instance().addCollection<ProfileVCardCollection, QString>(
        profilesDir, LoadOptions::FORCE_ENABLED
    );
}

LocalProfileCollection::LocalProfileCollection(CollectionMediator<Profile>* mediator) :
CollectionInterface(new LocalProfileEditor(mediator,this)), d_ptr(new LocalProfileCollectionPrivate)
{
    d_ptr->q_ptr = this;
}

LocalProfileCollection::~LocalProfileCollection()
{
    delete d_ptr;
}

bool LocalProfileEditor::save(const Profile* pro)
{
    if (!pro->isActive())
        return false;

    Q_ASSERT(pro->person()->collection() == m_pVCardCollection);

    // Update the profile list
    pro->person()->removeAllCustomFields("X-RINGACCOUNTID");

    for (auto a : qAsConst(pro->accounts())) {
        pro->person()->addCustomField("X-RINGACCOUNTID", a->id());
    }

    pro->person()->save();

    return true;
}

bool LocalProfileEditor::remove(const Profile* item)
{
    if (item->person()->remove()) {
        mediator()->removeItem(item);
        Q_ASSERT(!item->person()->isActive());
        return true;
    }

    return false;
}

bool LocalProfileEditor::edit( Profile* item)
{
    Q_UNUSED(item)
    return false;
}

bool LocalProfileEditor::addNew(Profile* pro)
{
    pro->person()->ensureUid();
    qDebug() << "Creating new profile" << pro->person()->uid();
    m_lItems << pro;
    pro->person()->setCollection(m_pVCardCollection);
    pro->setCollection(m_pCollection);
    mediator()->addItem(pro);
    save(pro);
    return true;
}

bool LocalProfileEditor::addExisting(const Profile* item)
{
    m_lItems << const_cast<Profile*>(item);
    item->person()->setCollection(m_pVCardCollection);
    mediator()->addItem(item);
    return true;
}

QVector<Profile*> LocalProfileEditor::items() const
{
    return m_lItems;
}

QString LocalProfileEditor::path(const Profile* p) const
{
    const QDir profilesDir = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/profiles/";
    profilesDir.mkpath(profilesDir.path());
    return QString("%1/%2.vcf")
        .arg(profilesDir.absolutePath())
        .arg(QString(p->person()->uid()));
}

QString LocalProfileCollection::name () const
{
    return QObject::tr("Local profiles");
}

QString LocalProfileCollection::category () const
{
    return QObject::tr("Profile Collection");
}

QVariant LocalProfileCollection::icon() const
{
   return QVariant();
}

bool LocalProfileCollection::isEnabled() const
{
   return true;
}

static QVector<Account*> getAccountList(const QList<QByteArray>& accs)
{
    QVector<Account*> ret;
    for (const auto& id : qAsConst(accs)) {
        if (auto a = AccountModel::instance().getById(id))
            ret << a;
    }

    return ret;
}

bool LocalProfileCollection::load()
{
    const QDir profilesDir = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/profiles/";

    qDebug() << "Loading vcf from:" << profilesDir;

    auto e = static_cast<LocalProfileEditor*>(editor<Profile>());

    const auto vCards = e->m_pVCardCollection->items<Person>();

    for (auto p : qAsConst(vCards)) {
        auto profile = new Profile(this, p);
        profile->setCollection(this);
        profile->setAccounts(getAccountList(p->getCustomFields("X-RINGACCOUNTID")));
        e->addExisting(profile);
    }

    // No profiles found, create one
    if (vCards.isEmpty())
        d_ptr->setupDefaultProfile();

    return true;
}

bool LocalProfileCollection::reload()
{
    return true;
}

FlagPack<CollectionInterface::SupportedFeatures> LocalProfileCollection::supportedFeatures() const
{
    return
        CollectionInterface::SupportedFeatures::NONE   |
        CollectionInterface::SupportedFeatures::LOAD   |
        CollectionInterface::SupportedFeatures::CLEAR  |
        CollectionInterface::SupportedFeatures::REMOVE |
        CollectionInterface::SupportedFeatures::ADD    ;
}

bool LocalProfileCollection::clear()
{
    QFile::remove((QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/profiles/");
    return true;
}

QByteArray LocalProfileCollection::id() const
{
    return "lpc";
}

void LocalProfileCollectionPrivate::setupDefaultProfile()
{
    auto profile = new Profile(q_ptr, new Person());
    profile->person()->setFormattedName(QObject::tr("Default"));

    for (int i = 0 ; i < AccountModel::instance().size() ; i++) {
        profile->addAccount(AccountModel::instance()[i]);
    }

    q_ptr->editor<Profile>()->addNew(profile);
}
