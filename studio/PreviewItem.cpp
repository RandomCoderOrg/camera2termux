#include "PreviewItem.h"

#include <QPainter>

PreviewItem::PreviewItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(false);
    setOpaquePainting(true);
}

void PreviewItem::setFrame(const QImage &frame) {
    m_frame = frame;
    update();
}

void PreviewItem::paint(QPainter *painter) {
    painter->fillRect(boundingRect(), QColor(8, 11, 13));
    if (m_frame.isNull()) return;
    QSizeF scaled = m_frame.size();
    scaled.scale(boundingRect().size(), Qt::KeepAspectRatio);
    QRectF target(QPointF((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(target, m_frame);
}
