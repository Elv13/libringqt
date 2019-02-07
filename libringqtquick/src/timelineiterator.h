/***************************************************************************
 *   Copyright (C) 2019 by Blue Systems                                    *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef TIMELINEITERATOR_H
#define TIMELINEITERATOR_H

#include <QtCore/QObject>
#include <QtCore/QModelIndex>

class Individual;
class TimelineIteratorPrivate;

class Q_DECL_EXPORT TimelineIterator : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(Individual* currentIndividual READ currentIndividual WRITE setCurrentIndividual NOTIFY changed)
    Q_PROPERTY(QModelIndex firstVisibleIndex READ firstVisibleIndex WRITE setFirstVisibleIndex NOTIFY changed)
    Q_PROPERTY(QModelIndex lastVisibleIndex READ lastVisibleIndex WRITE setLastVisibleIndex NOTIFY changed)
    Q_PROPERTY(QModelIndex currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY changed)
    Q_PROPERTY(QModelIndex newestIndex READ newestIndex NOTIFY contentAdded)
    Q_PROPERTY(QString     searchString READ searchString WRITE setSearchString NOTIFY changed)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY changed)
    Q_PROPERTY(int bookmarkCount READ bookmarkCount NOTIFY changed)
    Q_PROPERTY(int searchResultCount READ searchResultCount NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)

    Q_INVOKABLE explicit TimelineIterator(QObject* parent = nullptr);
    virtual ~TimelineIterator();

    Individual* currentIndividual() const;
    void setCurrentIndividual(Individual* i);

    QModelIndex firstVisibleIndex() const;
    void setFirstVisibleIndex(const QModelIndex& i);

    QModelIndex lastVisibleIndex() const;
    void setLastVisibleIndex(const QModelIndex& i);

    QModelIndex currentIndex() const;
    void setCurrentIndex(const QModelIndex& i);

    QModelIndex newestIndex() const;

    QString searchString() const;
    void setSearchString(const QString& s);

    int unreadCount() const;
    int bookmarkCount() const;
    int searchResultCount() const;

    bool hasMore() const;

public Q_SLOTS:
    /*
     * Those methods do not return the index directly because it may take some
     * time to find it. Returning it directly would potentially make ongoing
     * animation glitchy.
     */

    Q_INVOKABLE void proposePreviousUnread() const;
    Q_INVOKABLE void proposeNextUnread() const;

    Q_INVOKABLE void proposePreviousBookmark() const;
    Q_INVOKABLE void proposeNextBookmark() const;

    Q_INVOKABLE void proposePreviousResult() const;
    Q_INVOKABLE void proposeNextResult() const;

    Q_INVOKABLE void proposeNewest() const; // bottom
    Q_INVOKABLE void proposeOldest() const; // top

Q_SIGNALS:
    void changed();
    void proposeIndex(const QModelIndex& poposedIndex) const;
    void contentAdded();

private:
    TimelineIteratorPrivate* d_ptr;
    Q_DECLARE_PRIVATE(TimelineIterator)
};

#endif
