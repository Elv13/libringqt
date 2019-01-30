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
#include "historyimporter.h"
#include <collections/localhistorycollection.h>

// Qt
#include <QtCore/QTimer>

// Ring
#include <call.h>
#include <account.h>
#include <accountmodel.h>
#include <session.h>
#include <media/recordingmodel.h>
#include <media/recording.h>
#include <media/textrecording.h>
#include <collections/localtextrecordingcollection.h>
#include <private/textrecording_p.h>
#include <libcard/calendar.h>
#include <libcard/event.h>

#include <QtCore/QDebug>

namespace HistoryImporter
{

void importHistory(LocalHistoryCollection* histo)
{
    // Import the history
    histo->addCompletionCallback([](LocalHistoryCollection* c) {
        if (c->size() == 0)
            return;

        // Create the events for all calls
        const auto calls = c->items<Call>();

        for (auto c : qAsConst(calls)) {
            // The account may have been deleted
            if (!c->account())
                continue;

            auto cal = c->account()->calendar();

            Q_ASSERT(cal);

            cal->addEvent(c);

            //histo->clear();
        }
    });

    static QHash<Serializable::Group*, QSharedPointer<Event> > fuck;
    // Import the text messages after the first event loop (to prevent being
    // created before the accounts are loaded.
    QTimer::singleShot(0, []() {
        // Make *sure* it's loaded if for unknown reason it is not, there will
        // be some data corruption
        LocalTextRecordingCollection::instance();

        auto callback = [] {
            const auto recordingCollections = Session::instance()->recordingModel()->collections();

            // Load all text recording, read all files, scan everything
            for (CollectionInterface* backend : qAsConst(recordingCollections)) {
                if (backend->id() == "localtextrecording") {
                    const auto items = backend->items<Media::Recording>();

                    // All text recordings (files)
                    for (auto r : qAsConst(items)) {
                        if (r->type() == Media::Recording::Type::TEXT) {
                            const auto tR = static_cast<Media::TextRecording*>(r);
                            const auto groups = tR->d_ptr->allGroups();

                            // All messages
                            for (auto g : qAsConst(groups)) {
                                // The event has already been loaded
                                if (g->hasEvent()) {
                                    Q_ASSERT(!g->eventUid.isEmpty());
                                    continue;
                                }

                                // Get the event only if it exists
                                auto e = g->event(false);

                                // If that happens, either the database is corrupted or
                                // deleting something failed to remove the messages themselves
                                // but succeeded in deleting the event.
                                if (e && e->syncState() == Event::SyncState::PLACEHOLDER)
                                    qWarning() << "An event was referenced by a text message but was not found (1)" << e->uid();
                                else if ((!e) && !g->eventUid.isEmpty())
                                    qWarning() << "An event was referenced by a text message but was not found (2)" << g->eventUid;

                                // Create an event
                                e = g->event(true);

                                fuck[g] = e;

                                // There is no event
                                Q_ASSERT(e);

                                Q_ASSERT(g->hasEvent());
                            }
                        }
                    }
                }
            }

            // If there is new groups while they are imported, it's game over
            Serializable::Group::warnOfRaceCondition = true;
            LocalTextRecordingCollection::instance().saveEverything();
            Serializable::Group::warnOfRaceCondition = false;
        };


        int counter = 0;
        const auto accountCount = Session::instance()->accountModel()->size();

        // Wait until all calendars are loaded to limit the number of placeholder events
        for (int i = 0; i < accountCount; i++) {
            const auto a = (*Session::instance()->accountModel())[i];

            // While they currently load synchronously in the main thread, in the
            // future this should move either to threads or use coroutines.
            // To avoid forgetting about this, handle it anyway
            if (!a->calendar()->isLoaded()) {
                counter++;
                QMetaObject::Connection conn;
                conn = QObject::connect(a->calendar(), &Calendar::loadingFinished, a, [&conn, &counter, &callback]() {
                    QObject::disconnect(conn);
                    if (!--counter)
                        callback();
                });
            }
        }

        if (!counter)
            callback();

    });
}

}
