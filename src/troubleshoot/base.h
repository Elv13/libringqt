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
#pragma once

#include <QtCore/QStringListModel>
#include <typedefs.h>

class Call;

namespace Troubleshoot {

class Dispatcher;

class BasePrivate;

/**
 * When things go wrong *during* a live media session, letting it fail is an
 * option, but very often it is possible to involve the user directly and offer
 * option and information about why it failed and how to fix it.
 *
 * This is the base class of a chain of responsibility
 */
class LIB_EXPORT Base : public QStringListModel
{
    Q_OBJECT
public:
    Q_PROPERTY(QString        headerText READ headerText NOTIFY textChanged)
    Q_PROPERTY(Base::Severity severity   READ severity   NOTIFY textChanged)

    explicit Base(Dispatcher* parent = nullptr);
    virtual ~Base();

    /**
     * Whether this message is an error, a negligible warning or just a tip
     */
    enum class Severity {
        NONE,
        TIP,
        WARNING,
        ERROR
    };
    Q_ENUM(Severity)

    Dispatcher* dispatcher() const;

    virtual QString headerText() const = 0;

    virtual void reset();
    virtual void activate();
    virtual void deactivate();

    virtual QVariant icon() const;
    virtual int autoDismissDelay() const;

    virtual Base::Severity severity() const = 0;

    virtual bool setSelection(const QModelIndex& idx, Call* c);

Q_SIGNALS:
    void textChanged();

private:
    BasePrivate* d_ptr;
    Q_DECLARE_PRIVATE(Base)

};

}
