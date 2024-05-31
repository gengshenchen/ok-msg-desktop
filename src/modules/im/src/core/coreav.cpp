/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */


#include "coreav.h"
#include "core.h"
#include "src/model/friend.h"
#include "src/model/group.h"
#include "src/persistence/settings.h"
#include "base/compatiblerecursivemutex.h"
#include "src/video/corevideosource.h"
#include "src/video/videoframe.h"
#include "src/audio/audio.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <cassert>

/**
 * 音视频
 * @brief CoreAV::CoreAV
 */

CoreAV::CoreAV()
    : audioCtrl{nullptr},
      coreavThread{new QThread{this}},
      iterateTimer{new QTimer{this}} {
  assert(coreavThread);
  assert(iterateTimer);
    

  Settings& settings = Settings::getInstance();
  auto audioControl = std::unique_ptr<IAudioControl>(Audio::makeAudio(settings));
  assert(audioControl != nullptr);

  audioCtrl = audioControl.get();


  connect(this, &CoreAV::createCallToPeerId, this, &CoreAV::doCreateCallToPeerId);

  coreavThread->setObjectName("CoreAV");
  moveToThread(coreavThread.get());

  connectCallbacks(*this->toxav);

  iterateTimer->setSingleShot(true);

  connect(iterateTimer, &QTimer::timeout, this, &CoreAV::process);
  connect(coreavThread.get(), &QThread::finished, iterateTimer, &QTimer::stop);
  connect(coreavThread.get(), &QThread::started, this, &CoreAV::process);



//  selfVideoSource = std::unique_ptr<CoreVideoSource>(new CoreVideoSource);
}

void CoreAV::connectCallbacks(ToxAV &toxav) {

}

/**
 * @brief Factory method for CoreAV
 * @param core pointer to the Tox instance
 * @return CoreAV instance on success, {} on failure
 */
CoreAV::CoreAVPtr CoreAV::makeCoreAV() {
    return CoreAVPtr{new CoreAV};
}

CoreAV *CoreAV::getInstance()
{
    static CoreAV* instance = nullptr;
    if(!instance){
        instance = new CoreAV;
        instance->start();
    }
    return instance;
}

/**
 * @brief Set the audio backend
 * @param audio The audio backend to use
 * @note This must be called before starting CoreAV and audio must outlive
 * CoreAV
 */
void CoreAV::setAudio(IAudioControl &newAudio) { audioCtrl.exchange(&newAudio); }

/**
 * @brief Get the audio backend used
 * @return Pointer to the audio backend
 * @note This is needed only for the case CoreAV needs to restart and the
 * restarting class doesn't have access to the audio backend and wants to keep
 * it the same.
 */
IAudioControl *CoreAV::getAudio() { return audioCtrl; }

CoreAV::~CoreAV() {
  /* Gracefully leave calls and group calls to avoid deadlocks in destructor */
  for (const auto &call : calls) {
    cancelCall(call.first);
  }
  for (const auto &call : groupCalls) {
    leaveGroupCall(call.first);
  }

  assert(calls.empty());
  assert(groupCalls.empty());

  coreavThread->exit(0);
  coreavThread->wait();
}

/**
 * @brief Starts the CoreAV main loop that calls toxav's main loop
 */
void CoreAV::start() {
    qDebug() << __func__;
    coreavThread->start();
}

void CoreAV::process() {
  qDebug() << __func__;

  assert(QThread::currentThread() == coreavThread.get());

  toxav = std::make_unique<ToxAV>();
  toxav->addCallHandler(this);

  connect(toxav.get(), &ToxAV::receiveFriendVideoFrame, this,
          &CoreAV::onFriendVideoFrame);

  connect(toxav.get(), &ToxAV::receiveSelfVideoFrame, this,
          &CoreAV::onSelfVideoFrame);
}

bool CoreAV::isCallStarted(const ContactId *f) const {
  QReadLocker locker{&callsLock};
  return f && (calls.find(f->toString()) != calls.end());
}

bool CoreAV::isCallActive(const ContactId *f) const {
  QReadLocker locker{&callsLock};
  auto it = calls.find(f->toString());
  if (it == calls.end()) {
    return false;
  }
  return isCallStarted(f) && it->second->isActive();
}

bool CoreAV::isCallVideoEnabled(const ContactId *f) const {
  QReadLocker locker{&callsLock};
  auto it = calls.find(f->toString());
  return isCallStarted(f) && it->second->getVideoEnabled();
}

bool CoreAV::answerCall(QString friendNum, bool video) {
  QWriteLocker locker{&callsLock};


  qDebug() << QString("Answering call %1").arg(friendNum);
  auto it = calls.find(friendNum);
  assert(it != calls.end());

  QString callId = it->second->getCallId();
  auto answer = toxav->callAnswerToFriend(friendNum, callId, video);


  locker.unlock();

  if (answer) {
    it->second->setActive(true);
    return true;
  } else {
    qWarning() << "Failed to answer call with error";
    //    toxav_call_control(toxav.get(), friendNum, TOXAV_CALL_CONTROL_CANCEL,
    //                               nullptr);
    calls.erase(it);
    return false;
  }

  //  TOXAV_ERR_ANSWER err;
  //
  //  const uint32_t videoBitrate = video ? VIDEO_DEFAULT_BITRATE : 0;
  //  if (toxav_answer(toxav.get(), friendNum,
  //                   Settings::getInstance().getAudioBitrate(), videoBitrate,
  //                   &err)) {
  //    it->second->setActive(true);
  //    return true;
  //  } else {
  //    qWarning() << "Failed to answer call with error" << err;
  //    toxav_call_control(toxav.get(), friendNum, TOXAV_CALL_CONTROL_CANCEL,
  //                       nullptr);
  //    calls.erase(it);
  //    return false;
  ////  }
}
//
bool CoreAV::startCall(QString friendNum, bool video) {
  qDebug() << QString("Starting call with %1").arg(friendNum);

  QWriteLocker locker{&callsLock};


  auto it = calls.find(friendNum);
  if (it != calls.end()) {
    qWarning() << QString(
                      "Can't start call with %1, we're already in this call!")
                      .arg(friendNum);
    return false;
  }

  QString sId = QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch());
  if (!toxav->callToFriend(friendNum, sId, video)){
      qWarning() << "Failed call to friend" << friendNum;
      return false;
  }

  // Audio backend must be set before making a call
  assert(audioCtrl != nullptr);
  ToxFriendCallPtr call = ToxFriendCallPtr(new ToxFriendCall(friendNum, video, *this, *audioCtrl));
  assert(call != nullptr);
  call->setCallId(sId);
  // Call object must be owned by this thread or there will be locking problems
  // with Audio
  call->moveToThread(this->thread());
  calls.emplace(friendNum, std::move(call));
  return true;
}

bool CoreAV::cancelCall(QString friendNum) {
//  QWriteLocker locker{&callsLock};
//  QMutexLocker coreLocker{&coreLock};
  qDebug() << QString("Canceling call with %1").arg(friendNum);

  auto it = calls.find(friendNum);
  if (it == calls.end()) {
    qWarning() << QString(
                      "Can't cancel call with %1, we're already not in this call!")
                      .arg(friendNum);
    return true;
  }

  QString callId = it->second->getCallId();
  bool cancel = toxav->callCancelToFriend(friendNum, callId);
  qDebug() << QString("Canceling call=>%1").arg(cancel);
  calls.erase(friendNum);

//  locker.unlock();
//  coreLocker.unlock();

  emit avEnd(friendNum);
  return true;
}

void CoreAV::timeoutCall(QString friendNum) {
  // QWriteLocker locker{&callsLock};

  if (!cancelCall(friendNum)) {
    qWarning() << QString("Failed to timeout call with %1").arg(friendNum);
    return;
  }
  qDebug() << "Call with friend" << friendNum << "timed out";
}

/**
 * @brief Send audio frame to a friend
 * @param callId Id of friend in call list.
 * @param pcm An array of audio samples (Pulse-code modulation).
 * @param samples Number of samples in this frame.
 * @param chans Number of audio channels.
 * @param rate Audio sampling rate used in this frame.
 * @return False only on error, but not if there's nothing to send.
 */
bool CoreAV::sendCallAudio(QString callId, const int16_t *pcm, size_t samples,
                           uint8_t chans, uint32_t rate) const {
  QReadLocker locker{&callsLock};

  auto it = calls.find(callId);
  if (it == calls.end()) {
    return false;
  }

  ToxFriendCall const &call = *it->second;

  if (call.getMuteMic() || !call.isActive() ||
      !(call.getState() & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A)) {
    return true;
  }

  // TOXAV_ERR_SEND_FRAME_SYNC means toxav failed to lock, retry 5 times in this
  // case
  //  TOXAV_ERR_SEND_FRAME err;
  //  int retries = 0;
  //  do {
  //    if (!toxav_audio_send_frame(toxav.get(), callId, pcm, samples, chans,
  //    rate,
  //                                &err)) {
  //      if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
  //        ++retries;
  //        QThread::usleep(500);
  //      } else {
  //        qDebug() << "toxav_audio_send_frame error: " << err;
  //      }
  //    }
  //  } while (err == TOXAV_ERR_SEND_FRAME_SYNC && retries < 5);
  //  if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
  //    qDebug() << "toxav_audio_send_frame error: Lock busy, dropping frame";
  //  }

  return true;
}

void CoreAV::sendCallVideo(QString callId, std::shared_ptr<VideoFrame> vframe) {
  // QWriteLocker locker{&callsLock};

  // We might be running in the FFmpeg thread and holding the CameraSource lock
  // So be careful not to deadlock with anything while toxav locks in
  // toxav_video_send_frame
  auto it = calls.find(callId);
  if (it == calls.end()) {
    return;
  }

  ToxFriendCall &call = *it->second;

  if (!call.getVideoEnabled() || !call.isActive() ||
      !(call.getState() & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V)) {
    return;
  }

  if (call.getNullVideoBitrate()) {
    qDebug() << "Restarting video stream to friend" << callId;
    // QMutexLocker coreLocker{&coreLock};
    //    toxav_video_set_bit_rate(toxav.get(), callId, VIDEO_DEFAULT_BITRATE,
    //                             nullptr);
    call.setNullVideoBitrate(false);
  }

  ToxYUVFrame frame = vframe->toToxYUVFrame();

  if (!frame) {
    return;
  }

  // TOXAV_ERR_SEND_FRAME_SYNC means toxav failed to lock, retry 5 times in this
  // case We don't want to be dropping iframes because of some lock held by
  // toxav_iterate
  //  TOXAV_ERR_SEND_FRAME err;
  //  int retries = 0;
  //  do {
  //    if (!toxav_video_send_frame(toxav.get(), callId, frame.width,
  //    frame.height,
  //                                frame.y, frame.u, frame.v, &err)) {
  //      if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
  //        ++retries;
  //        QThread::usleep(500);
  //      } else {
  //        qDebug() << "toxav_video_send_frame error: " << err;
  //      }
  //    }
  //  } while (err == TOXAV_ERR_SEND_FRAME_SYNC && retries < 5);
  //  if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
  //    qDebug() << "toxav_video_send_frame error: Lock busy, dropping frame";
  //  }
}

/**
 * @brief Toggles the mute state of the call's input (microphone).
 * @param f The friend assigned to the call
 */
void CoreAV::toggleMuteCallInput(const ContactId *f) {
   QWriteLocker locker{&callsLock};

  auto it = calls.find(f->toString());
  if (f && (it != calls.end())) {
    ToxCall &call = *it->second;
    call.setMuteMic(!call.getMuteMic());
    toxav->setMute(call.getMuteMic());
  }
}

/**
 * @brief Toggles the mute state of the call's output (speaker).
 * @param f The friend assigned to the call
 */
void CoreAV::toggleMuteCallOutput(const ContactId *f) {
   QWriteLocker locker{&callsLock};

  auto it = calls.find(f->toString());
  if (f && (it != calls.end())) {
    ToxCall &call = *it->second;
    call.setMuteVol(!call.getMuteVol());

  }
  if (f && (it != calls.end())) {
    ToxCall &call = *it->second;
    call.setMuteMic(!call.getMuteMic());
    toxav->setRemoteMute(call.getMuteMic());
  }
}

/**
 * @brief Called from Tox API when group call receives audio data.
 *
 * @param[in] tox          the Tox object
 * @param[in] group        the group number
 * @param[in] peer         the peer number
 * @param[in] data         the audio data to playback
 * @param[in] samples      the audio samples
 * @param[in] channels     the audio channels
 * @param[in] sample_rate  the audio sample rate
 * @param[in] core         the qTox Core class
 */
void CoreAV::groupCallCallback(void *tox, QString group, QString peer,
                               const int16_t *data, unsigned samples,
                               uint8_t channels, uint32_t sample_rate,
                               void *core) {
  /*
   * Currently group call audio decoding is handled in the Tox thread by
   * c-toxcore, so we can be sure that this function is always called from the
   * Core thread. To change this, an API change in c-toxcore is needed and this
   * function probably must be changed. See
   * https://github.com/TokTok/c-toxcore/issues/1364 for details.
   */

  Q_UNUSED(tox);
  Core *c = static_cast<Core *>(core);

//  CoreAV *cav = c->getAv();
//  QReadLocker locker{&cav->callsLock};

  const ToxPk peerPk = c->getGroupPeerPk(group, peer);
  const Settings &s = Settings::getInstance();
  // don't play the audio if it comes from a muted peer
  if (s.getBlackList().contains(peerPk.toString())) {
    return;
  }

  emit c->groupPeerAudioPlaying(group, peerPk);

  //  auto it = cav->groupCalls.find(group);
  //  if (it == cav->groupCalls.end()) {
  //    return;
  //  }
  //
  //  ToxGroupCall &call = *it->second;
  //
  //  if (call.getMuteVol() || !call.isActive()) {
  //    return;
  //  }
  //
  //  call.playAudioBuffer(peerPk, data, samples, channels, sample_rate);
}

/**
 * @brief Called from core to make sure the source for that peer is invalidated
 * when they leave.
 * @param group Group Index
 * @param peer Peer Index
 */
void CoreAV::invalidateGroupCallPeerSource(QString group, ToxPk peerPk) {
  // QWriteLocker locker{&callsLock};

  auto it = groupCalls.find(group);
  if (it == groupCalls.end()) {
    return;
  }
  it->second->removePeer(peerPk);
}

/**
 * @brief Get a call's video source.
 * @param friendNum Id of friend in call list.
 * @return Video surface to show
 */
VideoSource *CoreAV::getVideoSourceFromCall(QString friendNum) const {
  QReadLocker locker{&callsLock};

  auto it = calls.find(friendNum);
  if (it == calls.end()) {
    qWarning() << "CoreAV::getVideoSourceFromCall: No such call, did it die "
                  "before we finished "
                  "answering?";
    return nullptr;
  }

  return it->second->getVideoSource();
}

/**
 * @brief Starts a call in an existing AV groupchat.
 * @note Call from the GUI thread.
 * @param groupId Id of group to join
 */
void CoreAV::joinGroupCall(const Group &group) {
  QWriteLocker locker{&callsLock};

  qDebug() << QString("Joining group call %1").arg(group.getId());

  // Audio backend must be set before starting a call
  assert(audioCtrl != nullptr);

  ToxGroupCallPtr groupcall =
      ToxGroupCallPtr(new ToxGroupCall{group, *this, *audioCtrl});
  // Call Objects must be owned by CoreAV or there will be locking problems with
  // Audio
  groupcall->moveToThread(this->thread());

  auto ret = groupCalls.emplace(group.getId(), std::move(groupcall));
  if (!ret.second) {
    qWarning() << "This group call already exists, not joining!";
    return;
  }
  ret.first->second->setActive(true);

  // TODO 发起群视频
  toxav->callToGroup(group.getId());
}

/**
 * @brief Will not leave the group, just stop the call.
 * @note Call from the GUI thread.
 * @param groupId Id of group to leave
 */
void CoreAV::leaveGroupCall(QString groupId) {
  QWriteLocker locker{&callsLock};

  qDebug() << QString("Leaving group call %1").arg(groupId);

  groupCalls.erase(groupId);
}

bool CoreAV::sendGroupCallAudio(QString groupId, const int16_t *pcm,
                                size_t samples, uint8_t chans,
                                uint32_t rate) const {
  QReadLocker locker{&callsLock};

  std::map<QString, ToxGroupCallPtr>::const_iterator it =
      groupCalls.find(groupId);
  if (it == groupCalls.end()) {
    return false;
  }

  if (!it->second->isActive() || it->second->getMuteMic()) {
    return true;
  }

  //  if (toxav_group_send_audio(toxav_get_tox(toxav.get()), groupId, pcm,
  //  samples,
  //                             chans, rate) != 0)
  //    qDebug() << "toxav_group_send_audio error";
  //  return true;
  return false;
}

/**
 * @brief Mutes or unmutes the group call's input (microphone).
 * @param g The group
 * @param mute True to mute, false to unmute
 */
void CoreAV::muteCallInput(const Group *g, bool mute) {
  QWriteLocker locker{&callsLock};

  auto it = groupCalls.find(g->getId());
  if (g && (it != groupCalls.end())) {
    it->second->setMuteMic(mute);
  }
}

/**
 * @brief Mutes or unmutes the group call's output (speaker).
 * @param g The group
 * @param mute True to mute, false to unmute
 */
void CoreAV::muteCallOutput(const Group *g, bool mute) {
  QWriteLocker locker{&callsLock};

  auto it = groupCalls.find(g->getId());
  if (g && (it != groupCalls.end())) {
    it->second->setMuteVol(mute);
  }
}

/**
 * @brief Returns the group calls input (microphone) state.
 * @param groupId The group id to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isGroupCallInputMuted(const Group *g) const {
  QReadLocker locker{&callsLock};

  if (!g) {
    return false;
  }

  const QString groupId = g->getId();
  auto it = groupCalls.find(groupId);
  return (it != groupCalls.end()) && it->second->getMuteMic();
}

/**
 * @brief Returns the group calls output (speaker) state.
 * @param groupId The group id to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isGroupCallOutputMuted(const Group *g) const {
  QReadLocker locker{&callsLock};

  if (!g) {
    return false;
  }

  const QString groupId = g->getId();
  auto it = groupCalls.find(groupId);
  return (it != groupCalls.end()) && it->second->getMuteVol();
}

/**
 * @brief Returns the calls input (microphone) mute state.
 * @param f The friend to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isCallInputMuted(const ContactId *f) const {
  QReadLocker locker{&callsLock};

  if (!f) {
    return false;
  }
  const QString friendId = f->toString();
  auto it = calls.find(friendId);
  return (it != calls.end()) && it->second->getMuteMic();
}

/**
 * @brief Returns the calls output (speaker) mute state.
 * @param friendId The friend to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isCallOutputMuted(const ContactId *f) const {
  QReadLocker locker{&callsLock};

  if (!f) {
    return false;
  }
  const QString friendId = f->getId();
  auto it = calls.find(friendId);
  return (it != calls.end()) && it->second->getMuteVol();
}

/**
 * @brief Signal to all peers that we're not sending video anymore.
 * @note The next frame sent cancels this.
 */
void CoreAV::sendNoVideo() {
   QWriteLocker locker{&callsLock};

  // We don't change the audio bitrate, but we signal that we're not sending
  // video anymore
  qDebug() << "CoreAV: Signaling end of video sending";
  for (auto &kv : calls) {
    ToxFriendCall &call = *kv.second;
    call.setNullVideoBitrate(true);
  }
}

void CoreAV::onCall(const QString &friendId, const QString &callId, bool audio, bool video) {
  callCallback( friendId, callId, audio, video );
}

void CoreAV::onCallRetract(const QString &friendNum, int state) {
  qDebug() << QString("Canceling call with %1").arg(friendNum);

  auto it = calls.find(friendNum);
  if (it == calls.end()) {
    qWarning() << QString(
                      "Can't cancel call with %1, we're already not in this call!")
                      .arg(friendNum);
    return ;
  }

  calls.erase(friendNum);
  emit avEnd(friendNum);
}

void CoreAV::onCallAcceptByOther(const QString& callId,
                                 const lib::messenger::IMPeerId & peerId) {

  qDebug() << ("onCallAcceptByOther")<<(peerId.toString())<<"callId"<<callId;
  QString friendNum;
  for (auto &item : calls) {
    if(item.second->getCallId() == callId){
      friendNum=item.first;
      break;
    }
  }
  if(friendNum.isEmpty())
  {
    qWarning()<<"Unable to find friend for call"<<callId;
    return;
  }

  qDebug()<<"avEnd"<<friendNum;
  calls.erase(friendNum);
  emit avEnd(friendNum);
}


void CoreAV::receiveCallStateAccepted(lib::messenger::IMPeerId peerId, QString callId, bool video) {
  qDebug() << __func__
           <<"peerId" << peerId.toString()
           << "callId:" << callId;

  stateCallback(peerId.toFriendId(), //
                video ? TOXAV_FRIEND_CALL_STATE::TOXAV_FRIEND_CALL_STATE_ACCEPTING_V
                      :  TOXAV_FRIEND_CALL_STATE::TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);

  if(QThread::currentThread() != coreavThread.get()){
//   bool invoked= QMetaObject::invokeMethod(toxav.get(),
//                                "callToPeerId",
//                                Q_ARG(lib::IM::IMPeerId, receiver),
//                                Q_ARG(QString, callId),
//                                Q_ARG(bool, video));
    emit createCallToPeerId(peerId, callId, video);
  }else{
    toxav->callToPeerId(peerId, callId, video);
  }

}

void CoreAV::doCreateCallToPeerId(lib::messenger::IMPeerId friendId, QString callId, bool video) { toxav->callToPeerId(friendId, callId, video);
}

void CoreAV::receiveCallStateRejected(lib::messenger::IMPeerId friendId, QString callId, bool video) {
  qDebug() << __func__ << "peerId:" << friendId.toString() <<"callId:" << callId;
  stateCallback(friendId.toFriendId(), TOXAV_FRIEND_CALL_STATE::TOXAV_FRIEND_CALL_STATE_FINISHED);
}

void CoreAV::onHangup(const QString &friendId, TOXAV_FRIEND_CALL_STATE state) {
  qDebug() << __func__ << "peerId:" << friendId;
  stateCallback(friendId, state);
}

void CoreAV::onFriendVideoFrame(const QString &friendId, uint16_t w, uint16_t h,
                                const uint8_t *y, const uint8_t *u,
                                const uint8_t *v, int32_t ystride,
                                int32_t ustride, int32_t vstride) {
  videoFrameCallback(toxav.get(), friendId, w, h, y, u, v, ystride, ustride,
                     vstride, this);
}

void CoreAV::onSelfVideoFrame(uint16_t w, uint16_t h, const uint8_t *y,
                              const uint8_t *u, const uint8_t *v,
                              int32_t ystride, int32_t ustride,
                              int32_t vstride) {
//  videoFramePush(selfVideoSource.get(), w, h, y, u, v, ystride, ustride,
//                 vstride);
}

void CoreAV::callCallback( QString friendNum, QString callId, bool audio_, bool video ) {


  qDebug()<<__func__<<"friendId:"<<friendNum <<"callId:"<< callId ;

//  CoreAV *self = static_cast<CoreAV *>(vSelf);

  // // QWriteLocker locker{&self->callsLock};

  // Audio backend must be set before receiving a call
  assert(audioCtrl != nullptr);

  ToxFriendCallPtr call = ToxFriendCallPtr(new ToxFriendCall{friendNum, video, *this, *audioCtrl});
  call->setCallId(callId);
  // Call object must be owned by CoreAV thread or there will be locking
  // problems with Audio
  call->moveToThread(thread());
  assert(call != nullptr);

  auto it = calls.emplace(friendNum, std::move(call));
  if (it.second == false) {
    qWarning()
        << QString("Rejecting call invite from %1, we're already in that call!")
               .arg(friendNum);
    //    toxav_call_control(toxav, friendNum, TOXAV_CALL_CONTROL_CANCEL,
    //    nullptr);
    return;
  }
  qDebug() << QString("Received call invite from %1").arg(friendNum);

  // We don't get a state callback when answering, so fill the state ourselves
  // in advance
  int state = 0;
  if (audioCtrl)
    state |=
        TOXAV_FRIEND_CALL_STATE_SENDING_A | TOXAV_FRIEND_CALL_STATE_ACCEPTING_A;
  if (video)
    state |=
        TOXAV_FRIEND_CALL_STATE_SENDING_V | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V;
  it.first->second->setState(static_cast<TOXAV_FRIEND_CALL_STATE>(state));

  // Must explicitly unlock, because a deadlock can happen via ChatForm/Audio
  //  locker.unlock();

  emit avInvite(friendNum, video);
}

void CoreAV::stateCallback(QString friendNum, uint32_t state ) {

  qDebug() << "stateCallback friend:" << friendNum;


  auto it =  calls.find(friendNum);
  if (it ==  calls.end()) {
    qWarning() << QString("stateCallback called, but call %1 is already dead")
                      .arg(friendNum);
    return;
  }

  // we must unlock this lock before emitting any signals
  //  // // QWriteLocker locker{&self->callsLock};

  ToxFriendCall &call = *it->second;
  if (state & TOXAV_FRIEND_CALL_STATE_ERROR) {
    qWarning() << "Call with friend" << friendNum
               << "died of unnatural causes!";
    calls.erase(friendNum);
    emit avEnd(friendNum, true);
  } else if (state & TOXAV_FRIEND_CALL_STATE_FINISHED) {
    qDebug() << "Call with friend" << friendNum << "finished quietly";
    calls.erase(friendNum);
    emit avEnd(friendNum);
  } else {
    // If our state was null, we started the call and were still ringing
    if (!call.getState() && state) {
      call.setActive(true);
      bool videoEnabled = call.getVideoEnabled();
      call.setState(static_cast<TOXAV_FRIEND_CALL_STATE>(state));
      emit avStart(friendNum, videoEnabled);
    } else if ((call.getState() & TOXAV_FRIEND_CALL_STATE_SENDING_V) &&
               !(state & TOXAV_FRIEND_CALL_STATE_SENDING_V)) {
      qDebug() << "IMFriend" << friendNum << "stopped sending video";
      if (call.getVideoSource()) {
        call.getVideoSource()->stopSource();
      }

      call.setState(static_cast<TOXAV_FRIEND_CALL_STATE>(state));
    } else if (!(call.getState() & TOXAV_FRIEND_CALL_STATE_SENDING_V) &&
               (state & TOXAV_FRIEND_CALL_STATE_SENDING_V)) {
      // Workaround toxav sometimes firing callbacks for "send last frame" ->
      // "stop sending video" out of orders (even though they were sent in order
      // by the other end). We simply stop the videoSource from emitting
      // anything while the other end says it's not sending
      if (call.getVideoSource()) {
        call.getVideoSource()->restartSource();
      }

      call.setState(static_cast<TOXAV_FRIEND_CALL_STATE>(state));
    }
  }

  //  locker.unlock();
}

// This is only a dummy implementation for now
void CoreAV::bitrateCallback(ToxAV *toxav, QString friendNum, uint32_t arate,
                             uint32_t vrate, void *vSelf) {
  CoreAV *self = static_cast<CoreAV *>(vSelf);
  Q_UNUSED(self);
  Q_UNUSED(toxav);

  qDebug() << "Recommended bitrate with" << friendNum << " is now " << arate
           << "/" << vrate << ", ignoring it";
}

// This is only a dummy implementation for now
void CoreAV::audioBitrateCallback(ToxAV *toxav, QString friendNum,
                                  uint32_t rate, void *vSelf) {
  CoreAV *self = static_cast<CoreAV *>(vSelf);
  Q_UNUSED(self);
  Q_UNUSED(toxav);

  qDebug() << "Recommended audio bitrate with" << friendNum << " is now "
           << rate << ", ignoring it";
}

// This is only a dummy implementation for now
void CoreAV::videoBitrateCallback(ToxAV *toxav, QString friendNum,
                                  uint32_t rate, void *vSelf) {
  CoreAV *self = static_cast<CoreAV *>(vSelf);
  Q_UNUSED(self);
  Q_UNUSED(toxav);

  qDebug() << "Recommended video bitrate with" << friendNum << " is now "
           << rate << ", ignoring it";
}

void CoreAV::audioFrameCallback(ToxAV *, QString friendNum, const int16_t *pcm,
                                size_t sampleCount, uint8_t channels,
                                uint32_t samplingRate, void *vSelf) {
  CoreAV *self = static_cast<CoreAV *>(vSelf);
  // This callback should come from the CoreAV thread
  assert(QThread::currentThread() == self->coreavThread.get());
  QReadLocker locker{&self->callsLock};

  auto it = self->calls.find(friendNum);
  if (it == self->calls.end()) {
    return;
  }

  ToxFriendCall &call = *it->second;

  if (call.getMuteVol()) {
    return;
  }

  call.playAudioBuffer(pcm, sampleCount, channels, samplingRate);
}

/**
 * 接收视频帧
 * @param friendNum
 * @param w
 * @param h
 * @param y
 * @param u
 * @param v
 * @param ystride
 * @param ustride
 * @param vstride
 * @param vSelf
 */
void CoreAV::videoFrameCallback(ToxAV *, QString friendNum, //
                                uint16_t w, uint16_t h,     //
                                const uint8_t *y,           //
                                const uint8_t *u,           //
                                const uint8_t *v,           //
                                int32_t ystride,            //
                                int32_t ustride,            //
                                int32_t vstride,            //
                                void *vSelf) {

  auto self = static_cast<CoreAV *>(vSelf);
  // This callback should come from the CoreAV thread
  assert(QThread::currentThread() == self->coreavThread.get());
  QReadLocker locker{&self->callsLock};

  auto it = self->calls.find(friendNum);
  if (it == self->calls.end()) {
    return;
  }

  CoreVideoSource *videoSource = it->second->getVideoSource();
  videoFramePush(videoSource, w, h, y, u, v, ystride, ustride, vstride);
}

void CoreAV::videoFramePush(CoreVideoSource *videoSource, //
                            uint16_t w,                   //
                            uint16_t h,                   //
                            const uint8_t *y,             //
                            const uint8_t *u,             //
                            const uint8_t *v,             //
                            int32_t ystride,              //
                            int32_t ustride,              //
                            int32_t vstride               //
) {

  if (!videoSource) {
    return;
  }

  vpx_image frame;
  frame.d_h = h;
  frame.d_w = w;
  frame.planes[0] = const_cast<uint8_t *>(y);
  frame.planes[1] = const_cast<uint8_t *>(u);
  frame.planes[2] = const_cast<uint8_t *>(v);
  frame.stride[0] = ystride;
  frame.stride[1] = ustride;
  frame.stride[2] = vstride;

  videoSource->pushFrame(&frame);
}
