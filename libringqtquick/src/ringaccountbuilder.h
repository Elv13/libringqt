/***************************************************************************
 *   Copyright (C) 2015 by Savoir-Faire Linux                              *
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
#ifndef RINGACCOUNTBUILDER_H
#define RINGACCOUNTBUILDER_H

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QItemSelectionModel>

#include <protocolmodel.h>
#include <account.h>
#include <namedirectory.h>

class RingAccountBuilderPrivate;

/**
 * This class exposes a way for QML code to create or import an account or
 * a profile without tons of boilerplate. It also ensure everything is saved
 * properly or the transaction gets reverted if necessary.
 */
class Q_DECL_EXPORT RingAccountBuilder : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(bool canCreate READ canCreate NOTIFY changed)
    Q_PROPERTY(bool creating READ isCreating NOTIFY changed)
    Q_PROPERTY(bool passwordMatch READ passwordMatch NOTIFY changed)
    Q_PROPERTY(bool nameLookup READ nameLookup NOTIFY changed)
    Q_PROPERTY(bool nameLookupError READ hasNameLookupError NOTIFY changed)
    Q_PROPERTY(NameDirectory::LookupStatus nameStatus READ nameStatus NOTIFY changed)
    Q_PROPERTY(Account::Protocol protocol READ protocol WRITE setProtocol NOTIFY changed)
    Q_PROPERTY(bool registerRingName READ registerRingName WRITE setRegisterRingName NOTIFY changed)
    Q_PROPERTY(bool usePassword READ usePassword WRITE setUsePassword NOTIFY changed)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY changed)
    Q_PROPERTY(QString repeatPassword READ repeatPassword WRITE setRepeatPassword NOTIFY changed)
    Q_PROPERTY(QString registeredRingName READ registeredRingName WRITE setRegisteredRingName NOTIFY changed)
    Q_PROPERTY(QString nameStatusMessage READ nameStatusMessage NOTIFY changed)
    Q_PROPERTY(QString creationStatusMessage READ creationStatusMessage NOTIFY changed)
    Q_PROPERTY(Account* account READ account NOTIFY changed)

    Q_INVOKABLE explicit RingAccountBuilder(QObject* parent = nullptr);
    virtual ~RingAccountBuilder();

    enum class ExtendedRole {
        PROFILE = 0,
        IMPORT  = 1,
        COUNT__,
    };
    Q_ENUM(ExtendedRole)

    bool isCreating() const;

    bool nameLookup() const;

    bool hasNameLookupError() const;

    bool canCreate() const;
    bool passwordMatch() const;
    NameDirectory::LookupStatus nameStatus() const;
    QString nameStatusMessage() const;
    QString creationStatusMessage() const;

    Account::Protocol protocol() const;
    void setProtocol(Account::Protocol);

    bool registerRingName() const;
    void setRegisterRingName(bool);

    bool usePassword() const;
    void setUsePassword(bool);

    QString password() const;
    void setPassword(const QString&);

    QString repeatPassword() const;
    void setRepeatPassword(const QString&);

    QString registeredRingName() const;
    void setRegisteredRingName(const QString&);

    Account* account() const;

public Q_SLOTS:
    void create();
    void commit();
    void cancel();

Q_SIGNALS:
    void finished();
    void changed();

private:
    RingAccountBuilderPrivate* d_ptr;
    Q_DECLARE_PRIVATE(RingAccountBuilder)
};

#endif

// kate: space-indent on; indent-width 4; replace-tabs on;
