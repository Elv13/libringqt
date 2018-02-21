/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>      *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include <address.h>

class AddressPrivate final
{
public:
   ~AddressPrivate() {}

   QString addressLine;
   QString city;
   QString zipCode;
   QString state;
   QString country;
   QString type;
};

Address::Address() : d_ptr(new AddressPrivate())
{

}

Address::~Address()
{
}

QString Address::addressLine() const
{
   return d_ptr->addressLine;
}

QString Address::city() const
{
   return d_ptr->city;
}

QString Address::zipCode() const
{
   return d_ptr->zipCode;
}

QString Address::state() const
{
   return d_ptr->state;
}

QString Address::country() const
{
   return d_ptr->country;
}

QString Address::type() const
{
   return d_ptr->type;
}

void Address::setAddressLine(const QString& value)
{
   d_ptr->addressLine = value;
}

void Address::setCity(const QString& value)
{
   d_ptr->city = value;
}

void Address::setZipCode(const QString& value)
{
   d_ptr->zipCode = value;
}

void Address::setState(const QString& value)
{
   d_ptr->state = value;
}

void Address::setCountry(const QString& value)
{
   d_ptr->country = value;
}

void Address::setType(const QString& value)
{
   d_ptr->type = value;
}
