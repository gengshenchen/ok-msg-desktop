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

#include "IMFile.h"
#include <bytestream.h>
#include <inbandbytestream.h>
#include <QEventLoop>
#include <utility>
#include "IM.h"
#include "IMFile.h"
#include "IMFileTask.h"
#include "base/logs.h"
#include "lib/session/AuthSession.h"

namespace lib {
namespace messenger {

QString File::toString() const {
    return QString("{id:%1, sId:%2, name:%3, path:%4, size:%5, status:%6, direction:%7}")
            .arg(id)
            .arg(sId)
            .arg(name)
            .arg(path)
            .arg(size)
            .arg((int)status)
            .arg((int)direction);
}

QDebug& operator<<(QDebug& debug, const File& f) {
    QDebugStateSaver saver(debug);
    debug.nospace() << f.toString();
    return debug;
}

IMFileSession::IMFileSession(const QString& sId,
                             Jingle::Session* session_,
                             const IMPeerId& peerId,
                             IMFile* sender_,
                             File* file)
        : sId(sId), target(peerId), sender(sender_), file(file), session(session_) {
    qDebug() << __func__ << "Create file task:" << sId;

    task = std::make_unique<IMFileTask>(target.toString(), file, sender);
    connect(task.get(), &IMFileTask::fileSending,
            [&](const QString& m_friendId, const File& m_file, int m_seq, int m_sentBytes,
                bool end) {
                for (auto h : sender->getHandlers())
                    h->onFileSendInfo(m_friendId, m_file, m_seq, m_sentBytes, end);
            });

    connect(task.get(), &IMFileTask::fileAbort,
            [&](const QString& m_friendId, const File& m_file, int m_sentBytes) {
                for (auto h : sender->getHandlers()) {
                    h->onFileSendAbort(m_friendId, m_file, m_sentBytes);
                }
            });

    connect(task.get(), &IMFileTask::fileError,
            [&](const QString& m_friendId, const File& m_file, int m_sentBytes) {
                for (auto h : sender->getHandlers()) {
                    h->onFileSendError(m_friendId, m_file, m_sentBytes);
                }
            });
}

void IMFileSession::stop() {
    qDebug() << __func__ << "Send file task will be clear." << sId;
    if (task->isRunning()) {
        task->forceQuit();
    }
}

void IMFileSession::start() {
    if (task && !task->isRunning()) {
        task->start();
    }
}

IMFile::IMFile(IM* im, QObject* parent) : IMJingle(im, parent) {
    qRegisterMetaType<File>("File");
    auto client = im->getClient();
    client->registerIqHandler(this, ExtIBB);
    //  jingle = IMJingle::getInstance();
    //  jingle->setFileHandlers(&fileHandlers);

    /*file handler*/
    //  connect(jingle, &IMFile::receiveFileChunk, this, [&](const IMContactId &friendId, const
    //  QString &sId, int seq, const std::string &chunk) -> void {
    //    for (auto handler : fileHandlers) {
    //      handler->onFileRecvChunk(friendId.toString(), sId, seq, chunk);
    //    }
    //  });
    //
    //  connect(jingle, &IMFile::receiveFileFinished, this, [&](const IMContactId &friendId, const
    //  QString &sId) -> void {
    //    for (auto handler : fileHandlers) {
    //      handler->onFileRecvFinished(friendId.toString(), sId);
    //    }
    //  });

    //  connect(jingle, &IMFile::receiveFileRequest, this, [&](const QString &friendId, const File
    //  &file) {
    //    for (auto h : fileHandlers) {
    //      h->onFileRequest(friendId, file);
    //    }
    //  });
}

IMFile::~IMFile() {
    //    disconnect(jingle, &IMFile::receiveFileChunk, this);
}

void IMFile::addFileHandler(FileHandler* handler) { fileHandlers.push_back(handler); }

void IMFile::fileRejectRequest(QString friendId, const File& file) {
    auto sId = file.sId;
    qDebug() << __func__ << sId;
    rejectFileRequest(friendId, sId);
}

void IMFile::fileAcceptRequest(QString friendId, const File& file) {
    auto sId = file.sId;
    qDebug() << __func__ << sId;
    acceptFileRequest(friendId, file);
}

void IMFile::fileCancel(QString fileId) { qDebug() << __func__ << "file" << fileId; }

void IMFile::fileFinishRequest(QString friendId, const QString& sId) {
    qDebug() << __func__ << sId;
    finishFileRequest(friendId, sId);
}

void IMFile::fileFinishTransfer(QString friendId, const QString& sId) {
    qDebug() << __func__ << sId;
    finishFileTransfer(friendId, sId);
}

bool IMFile::fileSendToFriend(const QString& f, const File& file) {
    qDebug() << __func__ << "friend:" << f << "file:" << file.id;

    auto bare = stdstring(f);

    auto resources = _im->getOnlineResources(bare);
    if (resources.empty()) {
        qWarning() << "Can not find online friends:" << f;
        return false;
    }

    JID jid(bare);
    for (auto& r : resources) {
        jid.setResource(r);
        sendFileToResource(jid, file);
    }

    return true;
}

void IMFile::addFile(const File& f) { m_waitSendFiles.append(f); }

void IMFile::doStartFileSendTask(const Session* session, const File& file) {
    //    qDebug() << __func__ << file.sId;
    //    m_fileSenderMap.insert(file.id, task);
    //    qDebug() << __func__ << ("Send file task has been stared.") << ((file.id));
}

void IMFile::doStopFileSendTask(const Session* session, const File& file) {
    Q_UNUSED(session)
    qDebug() << __func__ << file.sId;
    //    auto* task = m_fileSenderMap.value(file.sId);
    //    if (!task) {
    //        return;
    //    }
    //
    //    qDebug() << __func__ << "Send file task will be clear." << file.id;
    //    if (task->isRunning()) {
    //        task->forceQuit();
    //    }
    //    disconnect(task);
    //    delete task;

    // 返回截断后续处理
    //    m_fileSenderMap.remove(file.sId);
    //    qDebug() << "Send file task has been clean." << file.id;
}

/**
 * 文件传输
 * @param friendId
 * @param file
 */
void IMFile::rejectFileRequest(const QString& friendId, const QString& sId) {
    //  cancelCall(IMContactId{friendId}, sId);

    //  void IMJingle::cancelCall(const IMContactId &friendId, const QString &sId) {
    qDebug() << __func__ << friendId << sId;

    IMJingleSession* s = findSession(sId);
    if (s) {
        s->doTerminate();
        s->setCallStage(CallStage::StageNone);
        clearSessionInfo(s->getSession());
    }
    retractJingleMessage(friendId, sId);
    //  else {
    // jingle-message
    //    if (s->direction() == CallDirection:: CallOut) {
    //    } else if (s->direction() == CallDirection:: CallIn) {
    //      rejectJingleMessage(friendId.toString(), sId);
    //    }
    //  }
    //  }
}

void IMFile::acceptFileRequest(const QString& friendId, const File& file) {
    qDebug() << __func__ << file.name;
    qDebug() << __func__ << "sId:" << file.sId;
    qDebug() << __func__ << "fileId:" << file.id;

    auto session = m_fileSessionMap.value(file.sId);
    if (!session) {
        qWarning() << "Unable to find session sId:" << file.sId;
        return;
    }

    // 协议：https://xmpp.org/extensions/xep-0234.html#requesting
    PluginList pluginList;

    Jingle::FileTransfer::FileList files;
    files.push_back(
            Jingle::FileTransfer::File{.name = stdstring(file.name), .size = (long)file.size});

    auto ftf = new Jingle::FileTransfer(FileTransfer::Request, files);

    auto ibb = new Jingle::IBB(stdstring(file.txIbb.sid), file.txIbb.blockSize);

    pluginList.emplace_back(ftf);
    pluginList.emplace_back(ibb);

    auto c = new Jingle::Content("file", pluginList);
    auto js = session->getJingleSession();
    if (js) js->sessionAccept(c);
}

void IMFile::finishFileRequest(const QString& friendId, const QString& sId) {
    qDebug() << __func__ << "sId:" << (sId);
    auto* s = findSession(sId);
    if (!s) {
        qWarning() << "Can not find file session" << sId;
        return;
    }
    s->getSession()->sessionTerminate(new Session::Reason(Session::Reason::Success));
}

void IMFile::finishFileTransfer(const QString& friendId, const QString& sId) {
    qDebug() << __func__ << "sId:" << (sId);
    finishFileRequest(friendId, sId);
}

bool IMFile::sendFile(const QString& friendId, const File& file) {
    qDebug() << __func__ << friendId << (file.name);
    if (file.id.isEmpty()) {
        qWarning() << "file id is no existing";
        return false;
    }

    auto bare = stdstring(friendId);
    auto resources = _im->getOnlineResources(bare);
    if (resources.empty()) {
        qWarning() << "目标用户不在线！";
        return false;
    }

    JID jid(bare);
    for (auto& r : resources) {
        jid.setResource(r);
        sendFileToResource(jid, file);
    }

    return true;
}

bool IMFile::sendFileToResource(const JID& jid, const File& file) {
    qDebug() << __func__ << qstring(jid.full()) << "sId:" << file.sId;
    auto session = _sessionManager->createSession(jid, this, stdstring(file.sId));
    if (!session) {
        qDebug() << "Can not create session!";
        return false;
    }

    auto ws = cacheSessionInfo(session, ortc::JingleCallType::file);

    PluginList pl;

    // offer-file
    FileTransfer::FileList files;
    FileTransfer::File f;
    f.name = stdstring(file.name);
    f.size = file.size;
    files.emplace_back(f);
    auto ft = new FileTransfer(FileTransfer::Offer, files);
    pl.emplace_back(ft);

    // ibb
    auto ibb = new Jingle::IBB(stdstring(file.id), 4096);
    pl.emplace_back(ibb);

    // content
    auto jc = new Jingle::Content("offer-a-file", pl);
    session->sessionInitiate(jc);

    // 缓存文件
    auto& nf = const_cast<File&>(file);
    nf.sId = qstring(session->sid());
    addFile(nf);

    return true;
}

// 对方接收文件
void IMFile::sessionOnAccept(const QString& sId, Jingle::Session* session, const IMPeerId& peerId) {
    IMFileSession* sess = m_fileSessionMap.value(sId);
    if (sess) {
        qWarning() << "File session is existing, the sId is" << sId;
        return;
    }

    // 创建session
    for (auto& file : m_waitSendFiles) {
        auto s = new IMFileSession(sId, session, peerId, this, &file);
        s->start();
        m_fileSessionMap.insert(sId, s);
    }
}

void IMFile::sessionOnTerminate(const QString& sId, const IMPeerId& peerId) {
    IMFileSession* sess = m_fileSessionMap.value(sId);
    if (!sess) {
        qWarning() << "File session is no existing, the sId is" << sId;
        return;
    }
    m_fileSessionMap.remove(sId);
    delete sess;
}

void IMFile::sessionOnInitiate(const QString& sId,
                               Jingle::Session* session,
                               const Jingle::Session::Jingle* jingle,
                               const IMPeerId& peerId) {
    qDebug() << __func__ << "receive file:" << sId;

    for (auto p : jingle->plugins()) {
        auto pt = p->pluginType();
        switch (pt) {
            case JinglePluginType::PluginContent: {
                auto file = p->findPlugin<FileTransfer>(PluginFileTransfer);
                auto ibb = p->findPlugin<IBB>(PluginIBB);
                if (file && ibb) {
                    for (auto f : file->files()) {
                        auto id = qstring(ibb->sid());
                        //                        auto sId = qstring(session->sid());

                        qDebug() << "receive file:" << id << sId;
                        File* file0 = new File{.id = id,
                                               .sId = sId,
                                               .name = qstring(f.name),
                                               .path = {},
                                               .size = (quint64)f.size,
                                               .status = FileStatus::INITIALIZING,
                                               .direction = FileDirection::RECEIVING};
                        qDebug() << "receive file:" << file0->sId;
                        for (auto h : fileHandlers) {
                            h->onFileRequest(peerId.toFriendId(), *file0);
                        }

                        // 创建session
                        auto s = new IMFileSession(sId, session, peerId, this, file0);
                        m_fileSessionMap.insert(sId, s);
                    }
                }
                break;
            }
            default: {
            }
        }
    }
}

bool IMFile::handleIq(const IQ& iq) {
    const auto* ibb = iq.findExtension<InBandBytestream::IBB>(ExtIBB);
    if (!ibb) {
        return false;
    }

    IMContactId friendId(qstring(iq.from().bare()));
    qDebug() << __func__ << QString("IBB stream id:%1").arg(qstring(ibb->sid()));

    switch (ibb->type()) {
        case InBandBytestream::IBBOpen: {
            qDebug() << __func__ << QString("Open");
            break;
        }
        case InBandBytestream::IBBData: {
            auto sId = qstring(ibb->sid());

            qDebug() << __func__ << QString("Data seq:%1").arg(ibb->seq());
            for (auto h : fileHandlers) {
                h->onFileRecvChunk(friendId.toString(), sId, ibb->seq(), ibb->data());
            }

            break;
        }
        case InBandBytestream::IBBClose: {
            qDebug() << __func__ << QString("Close");
            auto sId = qstring(ibb->sid());
            //            auto ss = m_fileSessionMap.value(sId);
            for (auto k : m_fileSessionMap.keys()) {
                auto ss = m_fileSessionMap.value(k);
                ss->getJingleSession()->sessionTerminate(
                        new Session::Reason(Session::Reason::Success));
            }
            for (auto h : fileHandlers) {
                h->onFileRecvFinished(friendId.toString(), sId);
            }
            break;
        }
        default: {
        }
    }

    IQ riq(IQ::IqType::Result, iq.from(), iq.id());
    _im->getClient()->send(riq);
    return true;
}

}  // namespace messenger
}  // namespace lib
