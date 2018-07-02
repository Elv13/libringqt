/****************************************************************************
 *   Copyright (C) 2009-2017 Savoir-faire Linux                             *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
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
#pragma once

//Qt
#include <QtCore/QMetaType>
#include <QtCore/QMap>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QDebug>

//Typedefs (required to avoid '<' and '>' in the DBus XML)
typedef QMap<QString, QString>                              MapStringString               ;
typedef QMap<QString, int>                                  MapStringInt                  ;
typedef QVector<int>                                        VectorInt                     ;
typedef QVector<uint>                                       VectorUInt                    ;
typedef QVector<qulonglong>                                 VectorULongLong               ;
typedef QVector< QMap<QString, QString> >                   VectorMapStringString         ;
typedef QVector< QString >                                  VectorString                  ;
typedef QMap< QString, QMap< QString, QVector<QString> > >  MapStringMapStringVectorString;
typedef QMap< QString, QVector<QString> >                   MapStringVectorString         ;
typedef QMap< QString, QMap< QString, QStringList > >       MapStringMapStringStringList  ;
typedef QMap< QString, QStringList >                        MapStringStringList           ;
typedef QVector< QByteArray >                               VectorVectorByte              ;

struct Message {
    QString from;
    MapStringString payloads;
    quint64 received;
};

typedef QVector<Message> messages;


// Adapted from libring DRing::DataTransferInfo
// Adapted from libring DRing::DataTransferInfo
struct DataTransferInfo
{
    QString accountId;
    quint32 lastEvent;
    quint32 flags;
    qlonglong totalSize;
    qlonglong bytesProgress;
    QString peer;
    QString displayName;
    QString path;
    QString mimetype;
};
Q_DECLARE_METATYPE(DataTransferInfo)

#define LIB_EXPORT Q_DECL_EXPORT

//Doesn't work
#if ((__GNUC_MINOR__ > 8) || (__GNUC_MINOR__ == 8))
   #define STRINGIFY(x) #x
   #define IGNORE_NULL(content)\
   _Pragma(STRINGIFY(GCC diagnostic ignored "-Wzero-as-null-pointer-constant")) \
      content
#else
   #define IGNORE_NULL(content) content
#endif //ENABLE_IGNORE_NULL

#if QT_VERSION < 0x050700
// Keep the code compatible with Qt < 5.7. Also, once C++17 becomes mandatory,
// drop this and use `std::as_const`.
template<typename T>
const T& qAsConst(const T& v)
{
    return const_cast<const T&>(v);
}
#endif

