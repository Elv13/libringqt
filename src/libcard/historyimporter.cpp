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
#include "../localhistorycollection.h"

// Qt
#include <QtCore/QTimer>

// Ring
#include <call.h>
#include <account.h>
#include <media/recordingmodel.h>
#include <media/recording.h>
#include <media/textrecording.h>
#include <private/textrecording_p.h>
#include <libcard/calendar.h>
#include <libcard/event.h>

#include <QtCore/QDebug>

namespace HistoryImporter
{

void importHistory(LocalHistoryCollection* histo, std::function<void (const QVector<Calendar*>&)> callback)
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

    // Import the text messages after the first event loop (to prevent being
    // created before the accounts are loaded.

    QTimer::singleShot(0, []() {
        const auto recordingCollections = Media::RecordingModel::instance().collections();
        for (CollectionInterface* backend : qAsConst(recordingCollections)) {
            if (backend->id() == "localtextrecording") {
                const auto items = backend->items<Media::Recording>();
                for (auto r : qAsConst(items)) {
                    if (r->type() == Media::Recording::Type::TEXT) {
                        const auto tR = static_cast<Media::TextRecording*>(r);
                        const auto nodes = tR->d_ptr->m_lNodes;

                        Serializable::Group* g = nullptr;

                        time_t oldest = 0;
                        time_t newest = 0;

                        for (auto n : qAsConst(nodes)) {
                            if (!g && !oldest && n->m_pMessage) {
                                oldest = n->m_pMessage->timestamp();
                                newest = oldest;
                            }

                            if (n->m_pGroup && n->m_pGroup != g) {
                                if (g) {
                                    g->event();
                                }
                                g = n->m_pGroup;
                            }
                            if (n->m_pMessage && n->m_pMessage->timestamp() > newest) {
                                //
                            }
                        }
                    }
                }
            }
        }
    });
}

}
