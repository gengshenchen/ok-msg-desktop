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

#ifndef GROUP_MESSAGE_DISPATCHER_H
#define GROUP_MESSAGE_DISPATCHER_H

#include "src/core/icoregroupmessagesender.h"
#include "src/core/icoreidhandler.h"
#include "src/model/group.h"
#include "src/model/imessagedispatcher.h"
#include "src/model/message.h"

#include <QObject>
#include <QString>

#include <cstdint>

class IGroupSettings;

class GroupMessageDispatcher : public IMessageDispatcher
{
    Q_OBJECT
public:
    GroupMessageDispatcher(Group& group, MessageProcessor processor, ICoreIdHandler& idHandler,
                           ICoreGroupMessageSender& messageSender,
                           const IGroupSettings& groupSettings);

    std::pair<DispatchedMessageId, DispatchedMessageId> sendMessage(bool isAction,
                                                                    QString const& content,
                                                                    bool encrypt = false) override;

    void onMessageReceived(ToxPk const& sender,
                           bool isAction,
                           QString const& content,
                           QString const& nick,
                           QString const& from,
                           const QDateTime& time);

private:
    Group& group;
    MessageProcessor processor;
    ICoreIdHandler& idHandler;
    ICoreGroupMessageSender& messageSender;
    const IGroupSettings& groupSettings;
    DispatchedMessageId nextMessageId{0};
};


#endif /* IMESSAGE_DISPATCHER_H */
