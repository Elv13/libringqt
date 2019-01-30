/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
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
#include "localinfotemplatecollection.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QCryptographicHash>
#include <QtCore/QStandardPaths>

//Ring
#include "infotemplate.h"
#include "private/vcardutils.h"
#include <picocms/collectioneditor.h>
#include "individual.h"
#include "session.h"
#include "infotemplatemanager.h"
#include "private/threadworker.h"

class LocalInfoTemplateCollectionEditor final : public CollectionEditor<InfoTemplate>
{
public:
    LocalInfoTemplateCollectionEditor(CollectionMediator<InfoTemplate>* m) : CollectionEditor<InfoTemplate>(m) {}
    virtual bool save       ( const InfoTemplate* item ) override;
    virtual bool remove     ( const InfoTemplate* item ) override;
    virtual bool edit       ( InfoTemplate*       item ) override;
    virtual bool addNew     ( InfoTemplate*       item ) override;
    virtual bool addExisting( const InfoTemplate* item ) override;

    QVector<InfoTemplate*>             m_lItems;
    QHash<const InfoTemplate*,QString> m_hPaths;

    static const QString m_Path;

private:
    virtual QVector<InfoTemplate*> items() const override;
};

const QString LocalInfoTemplateCollectionEditor::m_Path = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/templates/";

class LocalInfoTemplateCollectionPrivate final : public QObject
{
    Q_OBJECT
public:
    LocalInfoTemplateCollectionPrivate(LocalInfoTemplateCollection* parent, CollectionMediator<InfoTemplate>* mediator);
    CollectionMediator<InfoTemplate>*  m_pMediator;

    // Helpers
    QByteArray defaultTemplate() const;

    LocalInfoTemplateCollection* q_ptr;
};

LocalInfoTemplateCollectionPrivate::LocalInfoTemplateCollectionPrivate(LocalInfoTemplateCollection* parent, CollectionMediator<InfoTemplate>* mediator) : q_ptr(parent), m_pMediator(mediator)
{
}

LocalInfoTemplateCollection::LocalInfoTemplateCollection(CollectionMediator<InfoTemplate>* mediator) :
CollectionInterface(new LocalInfoTemplateCollectionEditor(mediator)),d_ptr(new LocalInfoTemplateCollectionPrivate(this,mediator))
{
    if (!QDir().mkpath(LocalInfoTemplateCollectionEditor::m_Path))
        qWarning() << "cannot create path for fallbackcollection: "
            << LocalInfoTemplateCollectionEditor::m_Path;
    load();
}

LocalInfoTemplateCollection::~LocalInfoTemplateCollection()
{
    delete d_ptr;
}

bool LocalInfoTemplateCollectionEditor::save(const InfoTemplate* item)
{
    if (!item)
        return false;


    const QString path = m_Path+'/'+item->uid()+".vcf";

    QFile file(path);

    if (Q_UNLIKELY(!file.open(QIODevice::WriteOnly))) {
        qWarning() << "Can't write to" << path;
        return false;
    }

    file.write(item->toVCard());
    file.close();
    return true;
}

bool LocalInfoTemplateCollectionEditor::remove(const InfoTemplate* item)
{
    if (!item)
        return false;

    QString path = m_hPaths[item];

    if (path.isEmpty())
        path = m_Path+'/'+item->uid()+".vcf";

    bool ret = QFile::remove(path);

    if (ret) {
        ret &= mediator()->removeItem(item);
    }
    else
        qWarning() << "Failed to delete" << path;

    return ret;
}

bool LocalInfoTemplateCollectionEditor::edit( InfoTemplate* item)
{
    Q_UNUSED(item)
    return true;
}

bool LocalInfoTemplateCollectionEditor::addNew( InfoTemplate* item)
{
    bool ret = save(item);

    if (ret) {
        addExisting(item);
    }

    return ret;
}

bool LocalInfoTemplateCollectionEditor::addExisting(const InfoTemplate* item)
{
    m_lItems << const_cast<InfoTemplate*>(item);
    mediator()->addItem(item);
    return true;
}

QVector<InfoTemplate*> LocalInfoTemplateCollectionEditor::items() const
{
    return m_lItems;
}

QString LocalInfoTemplateCollection::name () const
{
    return QStringLiteral("Contact templates");
}

QString LocalInfoTemplateCollection::category () const
{
    return QObject::tr("User interface");
}

bool LocalInfoTemplateCollection::isEnabled() const
{
    return true;
}

bool LocalInfoTemplateCollection::load()
{
    new ThreadWorker([this]() {
        QDir dir(LocalInfoTemplateCollectionEditor::m_Path);

        if (!dir.exists())
            return;

        for (const QString& path : dir.entryList({"*.vcf"}, QDir::Files)) {
            QFile file(path);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qDebug() << "Error opening vcard: " << path;
                continue;
            }

            auto it = new InfoTemplate(file.readAll(), Session::instance()->infoTemplateManager());

            it->setCollection(this);
            editor<InfoTemplate>()->addExisting(it);
        }

        // Add the default template
        if (size() == 0) {
            auto it = new InfoTemplate(d_ptr->defaultTemplate(), Session::instance()->infoTemplateManager());

            it->setCollection(this);
            editor<InfoTemplate>()->addExisting(it);
        }
    });

    return true;
}

bool LocalInfoTemplateCollection::reload()
{
    return false;
}

FlagPack<CollectionInterface::SupportedFeatures> LocalInfoTemplateCollection::supportedFeatures() const
{
    return
        CollectionInterface::SupportedFeatures::NONE       |
        CollectionInterface::SupportedFeatures::LOAD       |
        CollectionInterface::SupportedFeatures::CLEAR      |
        CollectionInterface::SupportedFeatures::EDIT       |
        CollectionInterface::SupportedFeatures::SAVE       |
        CollectionInterface::SupportedFeatures::ADD        ;
}

bool LocalInfoTemplateCollection::clear()
{
    QDir dir(LocalInfoTemplateCollectionEditor::m_Path);
    for (const QString& file : dir.entryList({"*.vcf"},QDir::Files))
        dir.remove(file);
    return true;
}

QByteArray LocalInfoTemplateCollection::id() const
{
    return "litc";
}

QByteArray LocalInfoTemplateCollectionPrivate::defaultTemplate() const
{
    return "NF: "+tr("Formatted name\n").toLatin1()
        + "N:"+"First name"+';'+"Last name"+";;;\n"
        "TEL\n"
        "VERSION:2.1\n"
        "EMAIL;PREF=1:Preferred email\n"
        "END:VCARD";
}

#include "localinfotemplatecollection.moc"
