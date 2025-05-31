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

#ifndef VOICE_CONTENT_H
#define VOICE_CONTENT_H

#include "../chatlinecontent.h"

#include <QTimeLine>

namespace module::im {

class VolumnAnimationItem;

class VoiceContent : public ChatLineContent {
public:
    VoiceContent(uint duration);

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
                       const QStyleOptionGraphicsItem* option,
                       QWidget* widget) override;
    void setWidth(qreal width) override;
    const void* getContent() override;
    void fontChanged(const QFont& font) final;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void onCopyEvent() override{
    }
    void setAlternated(bool alt) override;

private:
    void clicked();
    void play();
    void stop();
    void onAudioPlayStart();
    void onAudioStopped();

private:
    void calcSize();

private:
    bool pressed = false;
    bool audioPlaying = false;
    QFont font;
    VolumnAnimationItem* iconItem = nullptr;
    uint duration = 0;
    QString durationText;
    bool sizeDirty = true;
    QSizeF baseSize;
    qreal preferWidth = -1;
};

class VolumnAnimationItem : public QGraphicsItem
{
public:
    VolumnAnimationItem(VoiceContent* parent);
    void setIconSize(QSizeF size);
    void setBlenderColor(const QColor & color);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void start();
    void stop();

private:
    void onFrameChanged(int frame);
    void onStateChanged(QTimeLine::State newState);
    const QStringList & getVolumnFrames();

private:
    VoiceContent* contentItem;
    QSizeF iconSize{12, 12};
    int currFrame = 0;
    QImage cachedImage;
    bool regenerateImage = true;
    QColor renderColor;
    static QStringList volumnFrames;
    static QTimeLine* animationTimeLine;
};
}  // namespace module::im

#endif  // VOICE_CONTENT_H
