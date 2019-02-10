/****************************************************************************
 *   Copyright (C) 2009-2017 Savoir-faire Linux                          *
 *   Authors : Alexandre Lision alexandre.lision@savoirfairelinux.com       *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>      *
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

#include "instancemanager_wrap.h"
#include "callmanager.h"
#include "presencemanager.h"
#include "configurationmanager.h"
#include <dbus/metatypes.h>
#ifdef ENABLE_VIDEO
 #include "videomanager.h"
#endif //ENABLE_VIDEO

#ifdef Q_OS_ANDROID
 #include "interfaces/android/androidaudioformat.h"
#endif

static int ringFlags = 0;

InstanceManagerInterface::InstanceManagerInterface()
{
   using namespace std::placeholders;

   using std::bind;
   using DRing::exportable_callback;
   using DRing::CallSignal;
   using DRing::ConfigurationSignal;
   using DRing::PresenceSignal;
   using DRing::DataTransferSignal;
   using DRing::DebugSignal;

#ifdef ENABLE_VIDEO
   using DRing::VideoSignal;
#endif

#ifndef MUTE_DRING
   ringFlags |= DRing::DRING_FLAG_DEBUG;
   ringFlags |= DRing::DRING_FLAG_CONSOLE_LOG;
#endif

#ifdef Q_OS_ANDROID
   GlobalInstances::setInterface<Interfaces::AndroidAudioFormat>();
#endif

   DRing::init(static_cast<DRing::InitFlag>(ringFlags));

   registerSignalHandlers(CallManager::instance().callHandlers);
   registerSignalHandlers(ConfigurationManager::instance().confHandlers);
   registerSignalHandlers(PresenceManager::instance().presHandlers);
   registerSignalHandlers(ConfigurationManager::instance().dataXferHandlers);
#ifdef ENABLE_VIDEO
   registerSignalHandlers(VideoManager::instance().videoHandlers);
#endif

   if (!DRing::start())
      printf("Error initializing daemon\n");
   else
      printf("Daemon is running\n");
}

InstanceManagerInterface::~InstanceManagerInterface()
{

}

bool InstanceManagerInterface::isConnected()
{
   return true;
}
