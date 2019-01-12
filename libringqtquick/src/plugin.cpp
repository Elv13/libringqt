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
#include "plugin.h"

// Qt
#include <QtCore/QObject>
#include <QQmlEngine>
#include <QQmlContext>

// LibRingQtQuick
#include "accountfields.h"
#include "accountbuilder.h"

// Globals
#include <session.h>

// Data objects
#include <account.h>
#include <call.h>
#include <person.h>
#include <individual.h>
#include <contactmethod.h>
#include <event.h>
#include <media/recording.h>
#include <media/textrecording.h>
#include <media/avrecording.h>
#include <media/availabilitytracker.h>
#include <video/sourcemodel.h>
#include <video/renderer.h>
#include <troubleshoot/dispatcher.h>
#include <troubleshoot/base.h>

// Models
#include <useractionmodel.h>
#include <individualtimelinemodel.h>
#include <pendingcontactrequestmodel.h>
#include <securityevaluationmodel.h>
#include <ringdevicemodel.h>
#include <codecmodel.h>
#include <credentialmodel.h>
#include <protocolmodel.h>
#include <callmodel.h>
#include <accountmodel.h>
#include <numbercompletionmodel.h>

#define QML_TYPE(name) qmlRegisterUncreatableType<name>(uri, 1,0, #name, #name "cannot be instantiated");
#define QML_TYPE_MED(name) qmlRegisterUncreatableType<name>(meduri, 1,0, #name, #name "cannot be instantiated");
#define QML_TYPE_MOD(name) qmlRegisterUncreatableType<name>(moduri, 1,0, #name, #name "cannot be instantiated");
#define QML_TYPE_VID(name) qmlRegisterUncreatableType<name>(viduri, 1,0, #name, #name "cannot be instantiated");
#define QML_TYPE_VIM(name) qmlRegisterUncreatableType<name>(vimuri, 1,0, #name, #name "cannot be instantiated");
#define QML_TYPE_TRO(name) qmlRegisterUncreatableType<name>(trouri, 1,0, #name, #name "cannot be instantiated");
#define QML_NS(name) qmlRegisterUncreatableMetaObject( name :: staticMetaObject, #name, 1, 0, #name, "Namespaces cannot be instantiated" );
#define QML_CRTYPE(name) qmlRegisterType<name>(uri, 1,0, #name);

void RingQtQuick::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("net.lvindustries.ringqtquick"));
    static QByteArray moduri = QByteArray(uri)+".models";
    static QByteArray meduri = QByteArray(uri)+".media";
    static QByteArray viduri = QByteArray(uri)+".video";
    static QByteArray vimuri = QByteArray(uri)+".video.models";
    static QByteArray trouri = QByteArray(uri)+".troubleshoot";

    // Uncreatable types
    QML_TYPE( Account       )
    QML_TYPE( const Account )
    QML_TYPE( Call          )
    QML_TYPE( Person        )
    QML_TYPE( Individual    )
    QML_TYPE( ContactMethod )
    QML_TYPE( Event         )

    // Uncreatable models
    QML_TYPE_MOD( UserActionModel            )
    QML_TYPE_MOD( IndividualTimelineModel    )
    QML_TYPE_MOD( PendingContactRequestModel )
    QML_TYPE_MOD( SecurityEvaluationModel    )
    QML_TYPE_MOD( RingDeviceModel            )
    QML_TYPE_MOD( CodecModel                 )
    QML_TYPE_MOD( CredentialModel            )
    QML_TYPE_MOD( ProtocolModel              )
    QML_TYPE_MOD( CallModel                  )
    QML_TYPE_MOD( AccountModel               )

    qmlRegisterType<NumberCompletionModel>(moduri, 1,0, "NumberCompletionModel");

    // Media subsystem
    { using namespace Media;
        QML_TYPE_MED( Recording     );
        QML_TYPE_MED( TextRecording );
        QML_TYPE_MED( AVRecording   );

        qmlRegisterType<AvailabilityTracker>(meduri, 1,0, "AvailabilityTracker");
    }

    // Video subsystem
    { using namespace Video;
        QML_TYPE_VIM( SourceModel );
        QML_TYPE_VID( Renderer    );
    }

    // Live troubleshooting subsystem
    { using namespace Troubleshoot;
        QML_TYPE_TRO( Base )
        qmlRegisterType<Dispatcher>(trouri, 1,0, "Dispatcher");
    }

    qmlRegisterType<AccountFields> (uri, 1, 0, "AccountFields" );
    qmlRegisterType<AccountBuilder>(uri, 1, 0, "AccountBuilder");

    // Alias
    qmlRegisterUncreatableType<AttachedAccountFieldStatus>(
        uri, 1, 0, "FieldStatus",
        "Cannot create objects of type FieldStatus, use it as an attached poperty"
    );
}

void RingQtQuick::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(engine)
    Q_UNUSED(uri)
    engine->rootContext()->setContextProperty("RingSession", Session::instance());
}
