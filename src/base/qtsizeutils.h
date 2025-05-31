/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PubL v2 for more details.
 */

#ifndef OKMSG_QT_SIZE_UTILS_H
#define OKMSG_QT_SIZE_UTILS_H

#include <QSizeF>
#include <QMargins>

namespace base {
QSize operator +(const QSize& s, const QMargins& m)
{
    return QSize(s.width() + m.left() + m.right(), s.height() + m.top() + m.bottom());
}
QSizeF operator +(const QSizeF& s, const QMargins& m) {
    return QSizeF(s.width() + m.left() + m.right(), s.height() + m.top() + m.bottom());
}
}

#endif