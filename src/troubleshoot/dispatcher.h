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

#include <QtCore/QIdentityProxyModel>
#include <typedefs.h>

#include <troubleshoot/base.h>

class Call;


namespace Troubleshoot {

class DispatcherPrivate;

class LIB_EXPORT Dispatcher : public QIdentityProxyModel
{
    Q_OBJECT
public:
    Q_PROPERTY(Call*    call       READ call       WRITE  setCall      )
    Q_PROPERTY(bool     isActive   READ isActive   NOTIFY activeChanged)
    Q_PROPERTY(QString  headerText READ headerText NOTIFY textChanged  )
    Q_PROPERTY(int severity   READ severity   NOTIFY textChanged  )
    Q_PROPERTY(QString currentIssue READ currentIssue NOTIFY activeChanged)

    Q_INVOKABLE explicit Dispatcher(QObject* parent = nullptr);
    virtual ~Dispatcher();

    bool isActive() const;

    QString headerText() const;

    int severity() const;

    QString currentIssue() const;

    Call* call() const;
    void setCall(Call* call);

    Base* currentModule() const;

    Q_INVOKABLE bool setSelection(const QModelIndex& idx);
    Q_INVOKABLE bool setSelection(int idx);

Q_SIGNALS:
    void activeChanged();
    void textChanged();

public Q_SLOTS:
    Q_INVOKABLE void dismiss();

private:
    DispatcherPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Dispatcher)
};

}
