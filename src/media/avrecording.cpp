/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "avrecording.h"

//Qt
#include <QtCore/QMimeDatabase>

//DRing
#include "dbus/callmanager.h"

//Ring
#include <callmodel.h>
#include <media/recordingmodel.h>

namespace Media {

class AVRecordingPrivate final {
public:
   AVRecordingPrivate(AVRecording* r);

   //Attributes
   QUrl                  m_Path      {     };
   AVRecording::Position m_Position  { 0.0 };
   int                   m_Duration  {  0  };
   bool                  m_IsPaused  {false};
   bool                  m_IsPlaying {false};
   int                   m_Elapsed   {  0  };
   int                   m_Left      {  0  };

   //Constants
   constexpr static const char minutes[] = "%1:%2";
   constexpr static const char hours  [] = "%1:%2:%3";

   //Helper
   void notifySeek(int position, int size);

private:
   AVRecording* q_ptr;
};


constexpr const char AVRecordingPrivate::minutes[];
constexpr const char AVRecordingPrivate::hours  [];

}


/**
 * Keep track of currently active recordings
 */
class RecordingPlaybackManager final : public QObject
{
   Q_OBJECT
public:
   explicit RecordingPlaybackManager();
   virtual ~RecordingPlaybackManager() {}

   //Attributes
   QList<Media::AVRecording*>         m_lActiveRecordings;
   QHash<QString,Media::AVRecording*> m_hActiveRecordings;

   //Mutator
   void activateRecording   (Media::AVRecording* r);
   void desactivateRecording(Media::AVRecording* r);

   //Singleton
   static RecordingPlaybackManager& instance();

public Q_SLOTS:
   void slotRecordPlaybackFilepath(const QString& callID, const QString& filepath );
   void slotRecordPlaybackStopped (const QString& filepath                        );
   void slotUpdatePlaybackScale   (const QString& filepath, int position, int size);
};

RecordingPlaybackManager::RecordingPlaybackManager() : QObject(&CallModel::instance())
{
   CallManagerInterface& callManager = CallManager::instance();
   connect(&callManager,&CallManagerInterface::recordPlaybackStopped , this, &RecordingPlaybackManager::slotRecordPlaybackStopped );
   connect(&callManager,&CallManagerInterface::updatePlaybackScale   , this, &RecordingPlaybackManager::slotUpdatePlaybackScale   );
   connect(&callManager,&CallManagerInterface::recordPlaybackFilepath, this, &RecordingPlaybackManager::slotRecordPlaybackFilepath);
}

///Singleton
RecordingPlaybackManager& RecordingPlaybackManager::instance()
{
    static auto instance = new RecordingPlaybackManager;
    return *instance;
}


Media::AVRecordingPrivate::AVRecordingPrivate(AVRecording* r) : q_ptr(r)
{

}

Media::AVRecording::AVRecording(const Recording::Status status) :
    Recording(Recording::Type::AUDIO_VIDEO, status), d_ptr(new AVRecordingPrivate(this))
{
}

Media::AVRecording::~AVRecording()
{
   delete d_ptr;
}

QVariant Media::AVRecording::roleData(int role) const
{
    if (call())
        return call()->roleData(role);

    switch(role) {
        case Qt::DisplayRole:
            return tr("N/A");
    }

   return {};
}

///Return this recording path, if any
QUrl Media::AVRecording::path() const
{
   return d_ptr->m_Path;
}

QMimeType* Media::AVRecording::mimeType() const
{
    // They are always .wav
    static QMimeType* t = nullptr;
    if (!t) {
        QMimeDatabase db;
        t = new QMimeType(db.mimeTypeForFile("foo.wav"));
    }

    return t;
}

Media::Attachment::BuiltInTypes Media::AVRecording::type() const
{
    return Attachment::BuiltInTypes::AUDIO_RECORDING;
}

///Get the current playback position (0.0 if not playing)
double Media::AVRecording::position() const
{
   return d_ptr->m_Position;
}

/**
 * Recording duration (in seconds). This is only available for audio/video
 * recordings and only after playback started.
 */
int Media::AVRecording::duration() const
{
   return d_ptr->m_Duration;
}

#define FORMATTED_TIME_MACRO(var) if (d_ptr->m_Duration > 3599) {\
      return QString(AVRecordingPrivate::hours)\
         .arg(var/3600                     )\
         .arg((var%3600)/60,2,10,QChar('0'))\
         .arg(var%60,2,10,QChar('0')       );\
   }\
   else {\
      return QString(AVRecordingPrivate::minutes)\
         .arg(var/60,2,10,QChar('0'))\
         .arg(var%60,2,10,QChar('0'));\
   }

///Get the a string in format hhh:mm:ss for the time elapsed since the playback started
QString Media::AVRecording::formattedTimeElapsed() const
{
   FORMATTED_TIME_MACRO(d_ptr->m_Elapsed);
}

///Get the a string in format hhh:mm:ss for the total time of this recording
QString Media::AVRecording::formattedDuration() const
{
   FORMATTED_TIME_MACRO(d_ptr->m_Duration);
}

///Get the a string in format hhh:mm:ss for the time elapsed before the playback end
QString Media::AVRecording::formattedTimeLeft() const
{
   FORMATTED_TIME_MACRO(d_ptr->m_Left);
}
#undef FORMATTED_TIME_MACRO

void Media::AVRecording::setPath(const QUrl& path)
{
   d_ptr->m_Path = path;
}

///Play (or resume) the playback
void Media::AVRecording::play()
{
   RecordingModel::instance().setCurrentRecording(this);

   RecordingPlaybackManager::instance().activateRecording(this);

   CallManagerInterface& callManager = CallManager::instance();
   const bool retval = callManager.startRecordedFilePlayback(path().path());
   if (retval) {
      d_ptr->m_IsPlaying = true;
      emit playingStatusChanged(true);
      emit started();
   }

   if (d_ptr->m_IsPaused) {
      seek(d_ptr->m_Position);
      d_ptr->m_IsPaused = false;
   }
}

///Stop the playback, cancel any pause point
void Media::AVRecording::stop()
{
   if (!d_ptr->m_IsPlaying)
      return;

   CallManagerInterface& callManager = CallManager::instance();
   Q_NOREPLY callManager.stopRecordedFilePlayback();
   d_ptr->m_IsPlaying = false;
   emit playingStatusChanged(false);
   emit stopped();

   RecordingPlaybackManager::instance().desactivateRecording(this);
   d_ptr->m_IsPaused = false;
}

///Pause (or resume)
void Media::AVRecording::pause()
{
   if (d_ptr->m_IsPaused) {
      play();
   }
   else {
      stop();
      d_ptr->m_IsPaused = true;
   }
}

/**
 * Change position in this recording
 * @note only available during playback
 * @args pos The position, in percent
 */
void Media::AVRecording::seek(double pos)
{
   CallManagerInterface& callManager = CallManager::instance();
   Q_NOREPLY callManager.recordPlaybackSeek(pos);
}

///Move the playback position to the origin
void Media::AVRecording::reset()
{
   seek(0.0);
}

///Update all internal playback metadatas
void Media::AVRecordingPrivate::notifySeek(int position, int size)
{
   const int oldElapsed  = m_Elapsed ;
   const int oldDuration = m_Duration;

   //Update the metadata
   m_Duration = size/1000;
   m_Position = ((double)position)/((double)size);
   m_Elapsed  = m_Duration * static_cast<int>(m_Position);
   m_Left     = m_Duration - m_Elapsed ;

   if (oldDuration != m_Duration)
      emit q_ptr->formattedDurationChanged(q_ptr->formattedDuration());

   if (oldElapsed != m_Elapsed) {
      emit q_ptr->formattedTimeElapsedChanged(q_ptr->formattedTimeElapsed());
      emit q_ptr->formattedTimeLeftChanged   (q_ptr->formattedTimeLeft   ());
   }

   emit q_ptr->playbackPositionChanged(m_Position);
}

///Callback when a recording start
void RecordingPlaybackManager::slotRecordPlaybackFilepath(const QString& callID, const QString& filepath)
{
   //TODO is this method dead code?
   qDebug() << "Playback started" << callID << filepath;
}

///Callback when a recording stop
void RecordingPlaybackManager::slotRecordPlaybackStopped(const QString& filepath)
{
   Media::AVRecording* r = m_hActiveRecordings[filepath];
   if (r) {
      desactivateRecording(r);
   }
}

///Callback when a recording position changed
void RecordingPlaybackManager::slotUpdatePlaybackScale(const QString& filepath, int position, int size)
{
   Media::AVRecording* r = m_hActiveRecordings[filepath];

   if (r) {
      r->d_ptr->notifySeek(position, size);
   }
   else
      qDebug() << "Unregistered recording position changed" << filepath;
}

///To avoid having to keep a list of all recording, manage a small active recording list
void RecordingPlaybackManager::activateRecording(Media::AVRecording* r)
{
   m_lActiveRecordings << r;
   m_hActiveRecordings[r->path().path()] = r;
}

///To avoid having to keep a list of all recording, manage a small active recording list
void RecordingPlaybackManager::desactivateRecording(Media::AVRecording* r)
{
   m_lActiveRecordings.removeAll(r);
   m_hActiveRecordings.remove(m_hActiveRecordings.key(r));
}

bool Media::AVRecording::isCurrent() const
{
    return RecordingModel::instance().currentRecording() == this;
}

bool Media::AVRecording::isPlaying() const
{
    return RecordingModel::instance().currentRecording() == this && d_ptr->m_IsPlaying;
}

#include <avrecording.moc>
