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
#include "localnameservicecache.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>

//Ring
#include <call.h>
#include <account.h>
#include <contactmethod.h>
#include <accountmodel.h>
#include <personmodel.h>
#include <phonedirectorymodel.h>
#include <collectioneditor.h>
#include <globalinstances.h>
#include <interfaces/pixmapmanipulatori.h>

class LocalNameServiceEditor final : public QObject, public CollectionEditor<ContactMethod>
{
    Q_OBJECT
public:
    LocalNameServiceEditor(CollectionMediator<ContactMethod>* m)
        : CollectionEditor<ContactMethod>(m), QObject(QCoreApplication::instance()) {}
    virtual bool save       ( const ContactMethod* item ) override;
    virtual bool remove     ( const ContactMethod* item ) override;
    virtual bool addNew     ( ContactMethod*       item ) override;
    virtual bool contains   (const ContactMethod*  item ) const override;
    virtual bool addExisting( const ContactMethod* item ) override;

    //Attributes
    QHash<QByteArray, QString> m_hCache;
    QMutex m_AsyncMutex;

private:
    virtual QVector<ContactMethod*> items() const override;
};

class LocalNameServiceCachePrivate
{
public:

    //Attributes
    constexpr static const char FILENAME[] = "nameservice.csv";
};

constexpr const char LocalNameServiceCachePrivate::FILENAME[];

LocalNameServiceCache::LocalNameServiceCache(CollectionMediator<ContactMethod>* mediator) :
   CollectionInterface(new LocalNameServiceEditor(mediator)), d_ptr(new LocalNameServiceCachePrivate())
{
    QTimer::singleShot(0, [this]() {load();});
}


LocalNameServiceCache::~LocalNameServiceCache()
{
    delete d_ptr;
}

bool LocalNameServiceCache::load()
{
    QTimer::singleShot(0, [this]() {
        QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                    + QLatin1Char('/')
                    + LocalNameServiceCachePrivate::FILENAME);
        if ( file.open(QIODevice::ReadOnly | QIODevice::Text) ) {

            const QByteArray content = file.readAll();

            const auto lines = content.split('\n');

            for (const auto& line : qAsConst(lines)) {
                const auto fields = line.split('\t');
                if (fields.count() != 2) {
                    qWarning() << "The registered name cache is corrupted";
                    return;
                }
                PhoneDirectoryModel::instance().setRegisteredNameForRingId(fields[0], fields[1]);
            }

            return;
        }
        else
            qWarning() << "Name cache doesn't exist or is not readable";
    });

    return true;
}

bool LocalNameServiceEditor::save(const ContactMethod* number)
{
    Q_UNUSED(number)

    if (!m_AsyncMutex.tryLock())
        return true;

    // Do not trash the I/O for nothing, it's just a cache
    //TODO support append only operations
    QTimer::singleShot(500, [this]() {
        m_AsyncMutex.unlock();

        static QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation)
            + QLatin1Char('/')
            + LocalNameServiceCachePrivate::FILENAME;

        QFile file(path);

        if (file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
            QTextStream stream(&file);

            for (auto i = m_hCache.constBegin(); i != m_hCache.constEnd(); i++) {
                stream
                    << i.key()
                    << '\t'
                    << i.value()
                    << '\n';
            }

            file.close();

            return false;
        }
        else
            qWarning() << "Unable to save the registered names";
    });

    return true;
}

bool LocalNameServiceEditor::remove(const ContactMethod* item)
{
    Q_UNUSED(item)

    if (m_hCache.remove(item->uri().format(URI::Section::USER_INFO).toLatin1()))
        return save(nullptr);

    return false;
}

bool LocalNameServiceEditor::contains(const ContactMethod* item) const
{
    return m_hCache.contains(item->uri().format(URI::Section::USER_INFO).toLatin1());
}

bool LocalNameServiceEditor::addNew( ContactMethod* number)
{
    if (contains(number))
        return true;

    addExisting(number);

//       number->setBookmarked(true);

    if (!save(number))
        qWarning() << "Unable to save the registered names";

    return save(number);
}

bool LocalNameServiceEditor::addExisting(const ContactMethod* item)
{
    if (item->registeredName().isEmpty())
        return false;

    if (contains(item))
        return true;

    m_hCache[item->uri().format(URI::Section::USER_INFO).toLatin1()] = item->registeredName();
    mediator()->addItem(item);
    return save(item);
}

QVector<ContactMethod*> LocalNameServiceEditor::items() const
{
    return {}; //unsupported
}

QString LocalNameServiceCache::name () const
{
    return QObject::tr("Local registered names cache");
}

QString LocalNameServiceCache::category () const
{
    return QObject::tr("Name service");
}

QVariant LocalNameServiceCache::icon() const
{
   return {};
}

bool LocalNameServiceCache::isEnabled() const
{
    return true;
}

bool LocalNameServiceCache::reload()
{
    return false;
}

FlagPack<CollectionInterface::SupportedFeatures> LocalNameServiceCache::supportedFeatures() const
{
    return
        CollectionInterface::SupportedFeatures::NONE      |
        CollectionInterface::SupportedFeatures::LOAD      |
        CollectionInterface::SupportedFeatures::CLEAR     |
        CollectionInterface::SupportedFeatures::ADD       |
        CollectionInterface::SupportedFeatures::REMOVE    ;
}

bool LocalNameServiceCache::clear()
{
    return QFile::remove(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                        + QLatin1Char('/')
                        + LocalNameServiceCachePrivate::FILENAME);
}

QByteArray LocalNameServiceCache::id() const
{
    return "localnameservicecache";
}

#include <localnameservicecache.moc>
