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

#pragma once

#include <QString>

class DesktopInfo
{
public:
    DesktopInfo();

    enum WM
    {
        GNOME,
        KDE,
        OTHER,
        SWAY
    };

    bool waylandDetected();
    WM windowManager();

private:
    QString XDG_CURRENT_DESKTOP;
    QString XDG_SESSION_TYPE;
    QString WAYLAND_DISPLAY;
    QString KDE_FULL_SESSION;
    QString GNOME_DESKTOP_SESSION_ID;
    QString GDMSESSION;
    QString DESKTOP_SESSION;
};
