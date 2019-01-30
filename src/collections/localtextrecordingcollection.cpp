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
#include "localtextrecordingcollection.h"

//Qt
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

//Ring
#include <globalinstances.h>
#include <session.h>
#include <interfaces/pixmapmanipulatori.h>
#include <media/recordingmodel.h>
#include <media/recording.h>
#include <media/textrecording.h>
#include <private/textrecording_p.h>
#include <private/contactmethod_p.h>
#include <media/media.h>

/*
 * This collection store and load the instant messaging conversations. Lets call
 * them "imc" for this section. An imc is a graph of one or more groups. Groups
 * are concatenated into a json file with the ContactMethod sha1 as filename.
 * Each group has a next group. If the next group is null, then the next one in
 * file will be used. If it isn't then another json file can be linked. Once the
 * second file has no next group, the next group from the first file is used.
 *
 * If more than 1 peer is part of the conversation, then their hash are
 * concatenated then hashed in sha1 again.
 */

class LocalTextRecordingEditor final : public CollectionEditor<Media::Recording>
{
public:
    LocalTextRecordingEditor(CollectionMediator<Media::Recording>* m) : CollectionEditor<Media::Recording>(m) {}
    virtual ~LocalTextRecordingEditor();

    virtual bool save       ( const Media::Recording* item ) override;
    virtual bool remove     ( const Media::Recording* item ) override;
    virtual bool edit       ( Media::Recording*       item ) override;
    virtual bool addNew     ( Media::Recording*       item ) override;
    virtual bool addExisting( const Media::Recording* item ) override;
    QString fetch(const QByteArray& sha1);
    static QString path(const QByteArray& sha1);

    void clearAll();
    void loadStat();

    virtual QVector<Media::Recording*> items() const override;
private:
    //Attributes
    QVector<Media::Recording*> m_lNumbers;
};

LocalTextRecordingCollection::LocalTextRecordingCollection(CollectionMediator<Media::Recording>* mediator) :
   CollectionInterface(new LocalTextRecordingEditor(mediator))
{
    // TODO use a thread
    //QTimer::singleShot(0, [this]() {
    load();
    //});
}

LocalTextRecordingEditor::~LocalTextRecordingEditor()
{
    // Save some metadata to speedup the startup process.
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    const QString path = QStringLiteral("%1/textrecordings.json").arg(dir.path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        qWarning() << "Could note save the text recording summary to" << path;
        return;
    }

    QJsonArray a;

    const auto itms = items();

    for (const auto recording : qAsConst(itms)) {
        const auto r = static_cast<const Media::TextRecording*>(recording);
        QJsonObject o;
        QJsonArray cms;
        const auto paths = r->paths();
        for (const auto& path : qAsConst(paths)) {
            //
            cms.append(path);
        }

        o[ "paths"    ] = cms;
        o[ "count"    ] = r->count();
        o[ "unread"   ] = r->unreadCount();
        o[ "lastUsed" ] = (int)r->lastUsed();

        a.append(o);
    }

    // Wrap everything into an object for futureproofness
    QJsonObject o;
    o["entries"] = a;

    QJsonDocument doc(a);

    QTextStream streamFileOut(&file);
    streamFileOut.setCodec("UTF-8");
    streamFileOut << doc.toJson();
    streamFileOut.flush();
    file.close();
}

LocalTextRecordingCollection::~LocalTextRecordingCollection()
{
}

LocalTextRecordingCollection& LocalTextRecordingCollection::instance()
{
    static auto instance = Session::instance()->recordingModel()->addCollection<LocalTextRecordingCollection>();
    return *instance;
}

QString LocalTextRecordingCollection::directoryPath()
{
    static QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
    if (!init_flag.test_and_set())
        dir.mkdir(QStringLiteral("text/"));

    static auto p = dir.path()+"/text/";

    return p;
}

bool LocalTextRecordingEditor::save(const Media::Recording* recording)
{
    Q_UNUSED(recording)

    QHash<QByteArray,QByteArray> ret = static_cast<const Media::TextRecording*>(recording)->d_ptr->toJsons();

    static QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    //Make sure the directory exist
    dir.mkdir(QStringLiteral("text/"));

    //Save each file
    for (QHash<QByteArray,QByteArray>::const_iterator i = ret.begin(); i != ret.end(); ++i) {
        QFile file(QStringLiteral("%1/text/%2.json").arg(dir.path()).arg(QString(i.key())));

        if ( file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
            QTextStream streamFileOut(&file);
            streamFileOut.setCodec("UTF-8");
            streamFileOut << i.value();
            streamFileOut.flush();
            file.close();
        }
    }

    if (ret.isEmpty()) {
        //TODO delete the file if it exists (requires to keep track of them)
        return false;
    }

    return true;
}

void LocalTextRecordingEditor::clearAll()
{
    for (Media::Recording *recording : items()) {
        auto textRecording = qobject_cast<Media::TextRecording*>(recording);
        textRecording->d_ptr->clear();
        save(recording);
    }
}

/** Reading *ALL* recordings during the initial event loop cause would freeze
 * the application for many seconds on slower drives. However knowing the number
 * of unread messages from previous sessions is necessary to display the UI.
 *
 * This collection therefor keep track of that in a small cache. It allows the
 * full recordings to be lazy-loaded.
 */
void LocalTextRecordingEditor::loadStat()
{
    //
}

bool LocalTextRecordingEditor::remove(const Media::Recording* item)
{
    Q_UNUSED(item)
    //TODO
    return false;
}

bool LocalTextRecordingEditor::edit( Media::Recording* item)
{
    Q_UNUSED(item)
    return false;
}

bool LocalTextRecordingEditor::addNew( Media::Recording* item)
{
    Q_UNUSED(item)
    addExisting(item);
    return save(item);
}

bool LocalTextRecordingEditor::addExisting(const Media::Recording* item)
{
    m_lNumbers << const_cast<Media::Recording*>(item);
    mediator()->addItem(item);
    return false;
}

QString LocalTextRecordingCollection::pathForCm(ContactMethod* cm)
{
    return LocalTextRecordingEditor::path(cm->sha1());
}

QString LocalTextRecordingEditor::path(const QByteArray& sha1)
{
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/text/" + sha1 + ".json";
}

QString LocalTextRecordingEditor::fetch(const QByteArray& sha1)
{
    QFile file(path(sha1));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

QVector<Media::Recording*> LocalTextRecordingEditor::items() const
{
    return m_lNumbers;
}

QString LocalTextRecordingCollection::name () const
{
    return QObject::tr("Local text recordings");
}

QString LocalTextRecordingCollection::category () const
{
    return QObject::tr("Recording");
}

QVariant LocalTextRecordingCollection::icon() const
{
    return GlobalInstances::pixmapManipulator().collectionIcon(this,Interfaces::PixmapManipulatorI::CollectionIconHint::RECORDING);
}

bool LocalTextRecordingCollection::isEnabled() const
{
    return true;
}

bool LocalTextRecordingCollection::load()
{

    // load all text recordings so we can recover CMs that are not in the call history
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/text/");

    // always return true, even if noting was loaded, since the collection can still be used to
    // save files
    if (!dir.exists())
        return true;

    const auto list = dir.entryInfoList(
        {QStringLiteral("*.json")},
        QDir::Files | QDir::NoSymLinks | QDir::Readable, QDir::Time
    );

    for (const auto& fileInfo : qAsConst(list)) {
        if (auto r = Media::TextRecording::fromPath(fileInfo.absoluteFilePath(), {}, this)) {

            // get CMs from recording
            const auto peers = r->peers();

            for (auto cm : qAsConst(peers)) {
                // since we load the recordings in order from newest to oldest, if there is
                // more than one found associated with a CM, we take the newest one
                if (!cm->d_ptr->m_pTextRecording) {
                    cm->d_ptr->setTextRecording(r);
                }
                else{
                    // Useful, but too noisy, it will happen as contacts are added
                    //qWarning() << "CM already has text recording" << cm;
                    cm->d_ptr->addAlternativeTextRecording(r);
                }
            }

            editor<Media::Recording>()->addExisting(r);
        }
    }

    return true;
}

bool LocalTextRecordingCollection::reload()
{
    return false;
}

FlagPack<CollectionInterface::SupportedFeatures> LocalTextRecordingCollection::supportedFeatures() const
{
    return
        CollectionInterface::SupportedFeatures::NONE      |
        CollectionInterface::SupportedFeatures::LOAD      |
        CollectionInterface::SupportedFeatures::ADD       |
        CollectionInterface::SupportedFeatures::SAVE      |
        CollectionInterface::SupportedFeatures::MANAGEABLE|
        CollectionInterface::SupportedFeatures::SAVE_ALL  |
        CollectionInterface::SupportedFeatures::LISTABLE  |
        CollectionInterface::SupportedFeatures::REMOVE    |
        CollectionInterface::SupportedFeatures::CLEAR     ;
}

bool LocalTextRecordingCollection::clear()
{
    static_cast<LocalTextRecordingEditor *>(editor<Media::Recording>())->clearAll();

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/text");

    // TODO: the file deletion should be done on each individual file to be able to catch errors
    // and to prevent us deleting files which are not the recordings
    return dir.removeRecursively();
}

QByteArray LocalTextRecordingCollection::id() const
{
    return "localtextrecording";
}

bool LocalTextRecordingCollection::listId(std::function<void(const QList<Element>)> callback) const
{
    QList<Element> list;

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/text/");

    if (!dir.exists())
        return false;

    for (const QString& str : dir.entryList({"*.json"}) ) {
        list << str.toLatin1();
    }

    callback(list);
    return true;
}

QList<CollectionInterface::Element> LocalTextRecordingCollection::listId() const
{
    return {};
}

bool LocalTextRecordingCollection::fetch(const Element& e)
{
    Q_UNUSED(e);
    return false;
}

bool LocalTextRecordingCollection::fetch( const QList<CollectionInterface::Element>& elements)
{
    Q_UNUSED(elements)
    return false;
}

Media::TextRecording* LocalTextRecordingCollection::fetchFor(const ContactMethod* cm)
{
    auto e = static_cast<LocalTextRecordingEditor*>(editor<Media::Recording>());

    const QByteArray& sha1 = cm->sha1();
    const QString content = e->fetch(sha1);

    if (content.isEmpty())
        return nullptr;

    QJsonParseError err;
    QJsonDocument loadDoc = QJsonDocument::fromJson(content.toUtf8(), &err);

    if (err.error != QJsonParseError::ParseError::NoError) {
        qWarning() << "Error Decoding Text Message History Json" << err.errorString();
        return nullptr;
    }

    Media::TextRecording* r = Media::TextRecording::fromJson(
        {loadDoc.object()}, e->path(sha1), const_cast<ContactMethod*>(cm), this
    );

    editor<Media::Recording>()->addExisting(r);

    return r;
}

Media::TextRecording* LocalTextRecordingCollection::createFor(const ContactMethod* cm)
{
    Media::TextRecording* r = fetchFor(cm);

    if (!r) {
        r = new Media::TextRecording(Media::Recording::Status::NEW);
        r->setCollection(this);
        cm->d_ptr->setTextRecording(r);
    }

    return r;
}

bool LocalTextRecordingCollection::exists(const ContactMethod* cm) const
{
    const auto e = static_cast<LocalTextRecordingEditor*>(editor<Media::Recording>());
    return QFile::exists(e->path(cm->sha1()));
}

void LocalTextRecordingCollection::saveEverything() const
{
    const auto itms = items<Media::Recording>();

    for (Media::Recording *recording : qAsConst(itms)) {
        recording->save();
    }
}
