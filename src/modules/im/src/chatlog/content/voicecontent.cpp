#include "voicecontent.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimeLine>
#include <cmath>
#include "base/qtsizeutils.h"
#include "chatlinecontentutil.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"

namespace module::im {

QTimeLine* VolumnAnimationItem::animationTimeLine = nullptr;
QStringList VolumnAnimationItem::volumnFrames;

constexpr qreal icon_duration_space = 6.0;
constexpr qreal conent_margin = 4.0;

VoiceContent::VoiceContent(uint duration)
        : ChatLineContent(ContentType::CHAT_VOICE), duration(duration) {
    iconItem = new VolumnAnimationItem(this);
    uint seconds = std::ceil(duration / 1000.0);
    uint minutes = seconds / 60;
    if (minutes > 0) {
        seconds = minutes % 60;
        durationText = QString("%1'%2\"").arg(minutes).arg(seconds);
    } else {
        durationText = QString("%1\"").arg(seconds);
    }
    fontChanged(Nexus::getProfile()->getSettings()->getChatMessageFont());
}

QRectF VoiceContent::boundingRect() const {
    if (preferWidth < 0) {
        return QRectF(0, 0, baseSize.width(), baseSize.height());
    }
    return QRectF(0, 0, std::min(baseSize.width(), preferWidth), baseSize.height());
}

void VoiceContent::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->translate(0.5, 0.5);
    constexpr qreal boundRadius = ChatLineContentUtils::borderRadius();
    QPainterPath path;
    path.addRoundedRect(boundingRect(), boundRadius, boundRadius);
    if (alternated())
        painter->fillPath(path, ChatLineContentUtils::alternatedBackground());
    else
        painter->fillPath(path, ChatLineContentUtils::background());
    int x = iconItem->boundingRect().width() + icon_duration_space;
    QRectF r = boundingRect().adjusted(x, 0, 0, 0);
    if (alternated())
        painter->setPen(QPen(ChatLineContentUtils::alternatedForegound()));
    else
        painter->setPen(QPen(ChatLineContentUtils::foreground()));
    painter->drawText(r, Qt::AlignVCenter, durationText);
    painter->restore();
}

void VoiceContent::setWidth(qreal width) {
    preferWidth = width;
    calcSize();

    // 布局，垂直居中图标
    QRectF br = boundingRect();
    QRectF icon_br = iconItem->boundingRect();

    qreal icon_y = br.center().y() - icon_br.height() / 2;
    qreal icon_x = br.left() + conent_margin;
    iconItem->setPos(QPointF(icon_x, icon_y) - icon_br.topLeft());
}

const void* VoiceContent::getContent() {
    return nullptr;
}
void VoiceContent::fontChanged(const QFont& font) {
    this->font = font;
}

void VoiceContent::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem::mousePressEvent(event);
    if (event->button() == Qt::LeftButton && event->buttons() == Qt::LeftButton) {
        pressed = true;
    }
}
void VoiceContent::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem::mouseReleaseEvent(event);
    if (pressed && event->button() == Qt::LeftButton) {
        pressed = false;
        if (contains(event->pos())) {
            clicked();
        }
    }
}

void VoiceContent::setAlternated(bool alt) {
    ChatLineContent::setAlternated(alt);
    if (alt) {
        iconItem->setBlenderColor(ChatLineContentUtils::alternatedForegound());
    } else {
        iconItem->setBlenderColor(ChatLineContentUtils::foreground());
    }
};

void VoiceContent::clicked() {
    if (audioPlaying) {
        stop();
    } else {
        play();
    }
}

void VoiceContent::play() {
    onAudioPlayStart();
}
void VoiceContent::stop() {
    onAudioStopped();
}

void VoiceContent::onAudioPlayStart() {
    audioPlaying = true;
    iconItem->start();
}
void VoiceContent::onAudioStopped() {
    audioPlaying = false;
    iconItem->stop();
}
void VoiceContent::calcSize() {
    QFontMetricsF fm(font);
    baseSize = fm.boundingRect("0\"").size();
    baseSize.rwidth() += icon_duration_space;
    baseSize.rwidth() += iconItem->boundingRect().width();  // volumn icon
    iconItem->setIconSize(QSizeF(fm.height(), fm.height()));

    // 根据时长计算一个附加长度值 
    baseSize.rwidth() += duration / 1000.0 * 2;

    using namespace base;
    QMargins margin(conent_margin, conent_margin, conent_margin, conent_margin);
    baseSize = baseSize + margin;
}

VolumnAnimationItem::VolumnAnimationItem(VoiceContent* parent)
        : QGraphicsItem(parent), contentItem(parent), cachedImage() {}

void VolumnAnimationItem::setIconSize(QSizeF size) {
    prepareGeometryChange();
    iconSize = size;
    cachedImage = QImage();
    regenerateImage = true;
}

void VolumnAnimationItem::setBlenderColor(const QColor& color) {
    renderColor = color;
    regenerateImage = true;
}

QRectF VolumnAnimationItem::boundingRect() const {
    return QRectF(QPointF(0, 0), iconSize);
}

void VolumnAnimationItem::paint(QPainter* painter,
                                const QStyleOptionGraphicsItem* option,
                                QWidget* widget) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    qreal radio = painter->device()->devicePixelRatioF();
    if (cachedImage.isNull() || radio != cachedImage.devicePixelRatioF()) {
        cachedImage = QImage((iconSize * radio).toSize(), QImage::Format_RGBA8888_Premultiplied);
        regenerateImage = true;
    }
    cachedImage.setDevicePixelRatio(1.0);
    if (regenerateImage) {
        QString path = getVolumnFrames().value(currFrame);
        cachedImage.fill(QColor(0, 0, 0, 0));
        QPainter imagePainter(&cachedImage);
        QIcon icon(path);
        icon.paint(&imagePainter, 0, 0, cachedImage.width(), cachedImage.height(), Qt::AlignCenter);
        imagePainter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        imagePainter.fillRect(cachedImage.rect(), renderColor);
        regenerateImage = false;
    }
    cachedImage.setDevicePixelRatio(radio);
    painter->drawImage(QPointF(0, 0), cachedImage);
    painter->restore();
}

void VolumnAnimationItem::start() {
    const QStringList& frams = getVolumnFrames();
    if (!animationTimeLine) {
        // 4帧，每帧持续固定时间
        int frame_count = frams.count();
        animationTimeLine = new QTimeLine(300 * frame_count, qApp);
        animationTimeLine->setLoopCount(0);
        animationTimeLine->setEasingCurve(QEasingCurve(QEasingCurve::Linear));
        animationTimeLine->setUpdateInterval(100);
        animationTimeLine->setFrameRange(0, frame_count - 1);
        animationTimeLine->setStartFrame(0);
    }
    // 仅允许一个播放实例，复用因此每次启动都重新绑定
    animationTimeLine->stop();
    animationTimeLine->disconnect();

    QObject::connect(animationTimeLine, &QTimeLine::frameChanged, contentItem, [this](int frame) {
        this->onFrameChanged(frame);
    });
    QObject::connect(animationTimeLine,
                     &QTimeLine::stateChanged,
                     contentItem,
                     [this](QTimeLine::State frame) { this->onStateChanged(frame); });
    animationTimeLine->start();
}
void VolumnAnimationItem::stop() {
    animationTimeLine->stop();
}
void VolumnAnimationItem::onFrameChanged(int frame) {
    currFrame = frame;
    regenerateImage = true;
    update();
}
void VolumnAnimationItem::onStateChanged(QTimeLine::State newState) {
    // 停止, 帧复位到0
    if (newState == QTimeLine::NotRunning) {
        if (currFrame != 0) {
            onFrameChanged(0);
        }
    }
}
const QStringList& VolumnAnimationItem::getVolumnFrames() {
    if (volumnFrames.isEmpty()) {
        volumnFrames = QStringList(
                {":/icons/chat/play/audio_play_3.svg", ":/icons/chat/play/audio_play_1.svg",
                 ":/icons/chat/play/audio_play_2.svg", ":/icons/chat/play/audio_play_3.svg"});
    }
    return volumnFrames;
}
}  // namespace module::im