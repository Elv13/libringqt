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
#pragma once

#include <QtCore/QAbstractListModel>

// Ring
#include <contactmethod.h>
#include <media/media.h>
#include <typedefs.h>
class Person;
class Call;
class IndividualEditor;
class IndividualPrivate;
class EventAggregate;
namespace Media {
    class TextRecording;
};

/**
 * There is often more than one way to reach someone.
 *
 * This model help to track those ways from both a Person and ContactMethod
 * point of view.
 */
class LIB_EXPORT Individual final : public QAbstractListModel
{
    Q_OBJECT

public:
    Q_PROPERTY(bool editRow READ hasEditRow WRITE setEditRow NOTIFY hasEditRowChanged)
    Q_PROPERTY(QSharedPointer<QAbstractItemModel> timelineModel READ timelineModel)
    Q_PROPERTY(QSharedPointer<EventAggregate> eventAggregate READ eventAggregate CONSTANT)
    Q_PROPERTY(QString bestName READ bestName NOTIFY changed)
    Q_PROPERTY(QString lastUsedUri READ lastUsedUri NOTIFY lastUsedTimeChanged)
    Q_PROPERTY(ContactMethod* lastUsedContactMethod READ lastUsedContactMethod NOTIFY lastUsedTimeChanged)
    Q_PROPERTY(ContactMethod* firstBookmark READ firstBookmark NOTIFY bookmarkedChanged)
    Q_PROPERTY(bool hasBookmarks READ hasBookmarks NOTIFY bookmarkedChanged)
    Q_PROPERTY(bool hasPhoneNumbers READ hasPhoneNumbers NOTIFY phoneNumbersChanged)
    Q_PROPERTY(Person* person READ person CONSTANT)
    Q_PROPERTY(Call* firstActiveCall READ firstActiveCall NOTIFY hasActiveCallChanged)
    Q_PROPERTY(ContactMethod* mainContactMethod READ mainContactMethod NOTIFY relatedContactMethodsAdded)
    Q_PROPERTY(bool requireUserSelection READ requireUserSelection NOTIFY relatedContactMethodsAdded)
    Q_PROPERTY(QModelIndex defaultIndex READ defaultIndex NOTIFY relatedContactMethodsAdded)
    Q_PROPERTY(QString formattedLastUsedTime READ formattedLastUsedTime NOTIFY lastUsedTimeChanged)
    Q_PROPERTY(int callCount READ callCount NOTIFY callAdded)
    Q_PROPERTY(int totalSpentTime READ totalSpentTime NOTIFY callAdded)
    Q_PROPERTY(int textMessageCount READ textMessageCount NOTIFY eventAdded)
    Q_PROPERTY(int lastUsedTime READ lastUsedTime NOTIFY lastUsedTimeChanged)
    Q_PROPERTY(bool isSelf READ isSelf NOTIFY isSelfChanged)

    //
    Q_PROPERTY(bool canSendTexts READ canSendTexts NOTIFY mediaAvailabilityChanged )
    Q_PROPERTY(bool canCall      READ canCall      NOTIFY mediaAvailabilityChanged )
    Q_PROPERTY(bool canVideoCall READ canVideoCall NOTIFY mediaAvailabilityChanged )
    Q_PROPERTY(bool isAvailable  READ isAvailable  NOTIFY mediaAvailabilityChanged )
    Q_PROPERTY(bool isOffline    READ isOffline    NOTIFY changed                  )


    virtual ~Individual();

    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    virtual bool setData( const QModelIndex& index, const QVariant &value, int role) override;
    virtual int rowCount( const QModelIndex& parent = {} ) const override;
    virtual QModelIndex index(int row, int col, const QModelIndex& parent = {}) const override;
    virtual QHash<int,QByteArray> roleNames() const override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent) override;

    bool hasEditRow() const;
    void setEditRow(bool v);

    Person* person() const;

    QString bestName() const;

    QString lastUsedUri() const;

    QVariant roleData(int role) const;

    int unreadTextMessageCount() const;

    time_t lastUsedTime() const;

    QString formattedLastUsedTime() const;

    int callCount() const;
    int textMessageCount() const;
    int totalSpentTime() const;

    bool hasPhoneNumbers() const;

    bool hasBookmarks() const;
    ContactMethod* firstBookmark() const;

    bool hasHiddenContactMethods() const;

    bool requireUserSelection() const;

    Call* firstActiveCall() const;

    bool isSelf() const;
    bool isDuplicate() const;
    Individual* masterObject() const;
    bool merge(Individual* other);

    bool hasPhoneNumber(ContactMethod* cm) const;

    /**
     * The most recently used ContactMethos that supports outgoing media `m`.
     *
     * It will match `mainContactMethod` when possible then fallback if it's
     * not available.
     *
     * @see mainContactMethod
     */
    ContactMethod* preferredContactMethod(Media::Media::Type m) const;
    Q_INVOKABLE ContactMethod* preferredContactMethod(int m) const; //QTBUG-58454

    /**
     * A more specialized version of the presence that returns true when the
     * individual is known to be offline rather than being a mix of isTracked,
     * Ring accounts and isPresent.
     */
    bool isOffline() const;

    /**
     * There is no value in a QItemSelectionModel here, all there is to know is
     * the default selection.
     */
    QModelIndex defaultIndex() const;

    QSharedPointer<QAbstractItemModel> timelineModel() const;
    ContactMethod* lastUsedContactMethod() const;

    /**
     * 95% of the time there is a single "official" ContactMethod per Individual.
     *
     * For those case, skip the selection dialog and have a "fast path"
     */
    ContactMethod* mainContactMethod() const;

    QSharedPointer<IndividualEditor> createEditor() const;

    QSharedPointer<EventAggregate> eventAggregate() const;

    Q_INVOKABLE ContactMethod* addPhoneNumber(ContactMethod* cm);
    Q_INVOKABLE ContactMethod* removePhoneNumber(ContactMethod* cm);
    Q_INVOKABLE ContactMethod* replacePhoneNumber(ContactMethod* old, ContactMethod* newCm);

    Q_INVOKABLE Person* buildPerson() const;

    Q_INVOKABLE bool markAsRead();

    QVector<ContactMethod*> phoneNumbers() const;
    QVector<ContactMethod*> relatedContactMethods() const;
    QVector<Media::TextRecording*> textRecordings() const;

    bool canSendTexts() const;
    bool canCall() const;
    bool canVideoCall() const;
    bool isAvailable() const;

    /**
     * Group all events related to this individual.
     */
    //QSharedPointer<EventAggregate> events(FlagPack<Event::EventCategory> categories = Event::EventCategory::ALL);

    /// Reverse map the boolean cardinality from N to 1
    template<bool (ContactMethod :: *prop)() const>
    bool hasProperty() const {
        for (const auto l : {phoneNumbers(), relatedContactMethods()}) {
            for (const auto cm : qAsConst(l))
                if ((cm->*prop)())
                    return true;
        }

        return false;
    }

    /// Sum the total of all ContactMethod
    template<int (ContactMethod :: *prop)() const>
    int propertySum() const {
        int ret = 0;
        for (const auto l : {phoneNumbers(), relatedContactMethods()}) {
            for (const auto cm : qAsConst(l))
                ret += (cm->*prop)();
        }

        return ret;
    }

    /// Helpers
    bool matchExpression(const std::function<bool(ContactMethod*)>& functor) const;
    void forAllNumbers(const std::function<void(ContactMethod*)> functor, bool indludeHidden = true) const;

    //TODO make private
    void setPhoneNumbers(const QVector<ContactMethod*>& cms);

    //Helper
    void registerContactMethod(ContactMethod* m);

    // Factory
    static Individual* getIndividual(ContactMethod* cm);
    static Individual* getIndividual(Person* cm);
    static Individual* getIndividual(const QList<ContactMethod*>& cms);

    // Check deduplication
    void* d() const;

Q_SIGNALS:
    void hasEditRowChanged(bool v);
    ///The number of and/or the contact methods themselves have changed
    void phoneNumbersChanged       (                );
    ///The number of and/or the contact methods themselvesd are about to change
    void phoneNumbersAboutToChange (                );
    void relatedContactMethodsAdded( ContactMethod* cm );
    void relatedContactMethodsRemoved( ContactMethod* cm );
    void unreadCountChanged();
    void callAdded(Call* call);
    void lastUsedTimeChanged(time_t time);
    void isSelfChanged();

    /// When any ContactMethod bookmark status changes
    void bookmarkedChanged(ContactMethod* cm, bool isBookmarked);

    void textRecordingAdded(Media::TextRecording* r);

    // Forward some relevant contactMethods signals
    void childrenContactChanged(ContactMethod* cm, Person* newContact, Person* oldContact);
    void childrenRebased(ContactMethod* cm,  ContactMethod* other  );

    /// When an event is attached to a ContactMethod
    void eventAdded(QSharedPointer<Event>& e);
    /// When an event is detached from a ContactMethod
    void eventDetached(QSharedPointer<Event>& e);

    /// When any ContactMethod changes
    void changed();

    void mediaAvailabilityChanged();

    /// When one or more call is in progress.
    void hasActiveCallChanged();

private:
    explicit Individual();
    Individual(Person* parent);
    Q_DECLARE_PRIVATE(Individual)

    IndividualPrivate* d_ptr;
};

Q_DECLARE_METATYPE(Individual*)
