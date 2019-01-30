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
#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QStringList>

// Qt
class QItemSelectionModel;

// Ring
#include "picocms/collectionmanagerinterface.h"
#include "picocms/collectioninterface.h"
#include "typedefs.h"
#include "contactmethod.h"

class ContactMethod;

namespace Media {
   class Recording;
   class TextRecording;
   class AVRecording;
   class RecordingModelPrivate;
   class MimeMessage;

/**
 * This model host the Ring recordings. Recording sessions span one or
 * more media, themselves possibly spanning multiple communications. They
 * can be paused indefinitely and resumed. Those events cause the recording
 * to be tagged at a specific point.
 *
 * The purpose of this model is mostly to track the recordings and handle
 * housekeeping task. It could also be used to manage recordings, move them
 * and so on.
 */
class LIB_EXPORT RecordingModel :  public QAbstractItemModel, public CollectionManagerInterface<Recording>
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
   friend class Session; // factory
public:
    // Properties
    Q_PROPERTY(QItemSelectionModel* selectionModel  READ selectionModel  CONSTANT                           )
    Q_PROPERTY(bool alwaysRecording                 READ isAlwaysRecording WRITE setAlwaysRecording         )
    Q_PROPERTY(QString recordPath                   READ recordPath        WRITE setRecordPath              )
    Q_PROPERTY(int unreadCount                      READ unreadCount       NOTIFY unreadMessagesCountChanged)
    Q_PROPERTY(::Media::Recording* currentRecording READ currentRecording  WRITE setCurrentRecording NOTIFY currentRecordingChanged   )

    // Constructor
    virtual ~RecordingModel();
    explicit RecordingModel(QObject* parent);

    virtual bool clearAllCollections() const override;

    // Model implementation
    virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role   )       override;
    virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole        ) const override;
    virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                   ) const override;
    virtual Qt::ItemFlags flags       ( const QModelIndex& index                                    ) const override;
    virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                   ) const override;
    virtual QModelIndex   parent      ( const QModelIndex& index                                    ) const override;
    virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent=QModelIndex()) const override;
    virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    //Getter
    bool                 isAlwaysRecording  () const;
    QString              recordPath         () const;
    int                  unreadCount        () const;
    QItemSelectionModel* selectionModel     () const;
    Recording*           currentRecording   () const;
    QAbstractItemModel*  audioRecordingModel() const;
    QAbstractItemModel*  textRecordingModel () const;

    bool hasTextRecordings(const ContactMethod* cm) const;

    //Setter
    void setAlwaysRecording ( bool            record );
    void setRecordPath      ( const QString&  path   );
    void clear              (                        );
    void setCurrentRecording( Recording* recording   );

    //Mutator
    TextRecording* createTextRecording(const ContactMethod* cm);

Q_SIGNALS:
    void newTextMessage(::Media::TextRecording* t, ContactMethod* cm);
    void mimeMessageInserted(MimeMessage* message, ::Media::TextRecording* t, ContactMethod* cm);
    void unreadMessagesCountChanged(int unreadCount);
    void currentRecordingChanged(Recording* r);

private:
    RecordingModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(RecordingModel)

    //Collection interface
    virtual void collectionAddedCallback(CollectionInterface* backend) override;
    virtual bool addItemCallback        (const Recording* item       ) override;
    virtual bool removeItemCallback     (const Recording* item       ) override;
};

}
