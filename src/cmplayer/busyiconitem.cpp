#include "busyiconitem.hpp"

struct ColorRing {
    QColor color;
    QImage image;
    float tx0 = 0, ty0 = 0;
};

enum { Dark, Light };

struct BusyIconItem::Data {
    BusyIconItem *p = nullptr;
    qreal thickness = 20, radius = 1, angle = 0, last = -1;
    bool dirtyGeometry = true, running = true;
    bool upload = false, redraw = false;
    int quater = -1, filled = Light;
    ColorRing rings[2];
    QSize textureSize = {1, 1};
    QPointF textureScale = {1, 1};
    QVariantAnimation prog;

    void reset() {
        angle = 0;
        quater = last = -1;
        filled = true;
        p->update();
    }

    void updateAnimation() {
        if (running && p->isVisible())
            prog.start();
        else
            prog.stop();
    }
};

BusyIconItem::BusyIconItem(QQuickItem *parent)
: SimpleTextureItem(parent), d(new Data) {
    d->p = this;
    setFlag(ItemHasContents, true);
    d->rings[Dark].color = Qt::darkGray;
    d->rings[Light].color = Qt::lightGray;
    d->reset();
    attributes().resize(6*3);

    d->prog.setDuration(1000);
    d->prog.setLoopCount(-1);
    d->prog.setStartValue(0.0);
    d->prog.setEndValue(2.0*M_PI);

    connect(&d->prog, &QVariantAnimation::valueChanged, [this] (const QVariant &var) {
        d->angle = var.toDouble();
        polish();
        update();
    });
    d->updateAnimation();
}

BusyIconItem::~BusyIconItem() {
    delete d;
}

void BusyIconItem::itemChange(ItemChange change, const ItemChangeData &data) {
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged)
        d->updateAnimation();
}

bool BusyIconItem::isRunning() const {
    return d->running;
}

void BusyIconItem::setRunning(bool running) {
    if (_Change(d->running, running)) {
        d->reset();
        d->updateAnimation();
        polish();
        emit runningChanged();
    }
}

QColor BusyIconItem::darkColor() const {
    return d->rings[Dark].color;
}

QColor BusyIconItem::lightColor() const {
    return d->rings[Light].color;
}

qreal BusyIconItem::thickness() const {
    return d->thickness;
}

void BusyIconItem::setDarkColor(const QColor &color) {
    if (_Change(d->rings[Dark].color, color)) {
        d->reset();
        emit darkColorChanged();
        update();
    }
}

void BusyIconItem::setLightColor(const QColor &color) {
    if (_Change(d->rings[Light].color, color)) {
        d->reset();
        emit lightColorChanged();
        update();
    }
}

void BusyIconItem::setThickness(qreal thickness) {
    if (_Change(d->thickness, thickness)) {
        d->redraw = true;
        emit thicknessChanged();
        polish();
        update();
    }
}

void BusyIconItem::geometryChanged(const QRectF &new_, const QRectF &old) {
    QQuickItem::geometryChanged(new_, old);
    polish();
    update();
}

void BusyIconItem::initializeGL() {
    SimpleTextureItem::initializeGL();
    OpenGLTexture2D texture;
    texture.create();
    texture.setAttributes(0, 0, OpenGLCompat::textureTransferInfo(OGL::BGRA));
    setTexture(texture);
}

void BusyIconItem::finalizeGL() {
    SimpleTextureItem::finalizeGL();
    texture().destroy();
}

void BusyIconItem::updateTexture(OpenGLTexture2D &texture) {
    if (d->upload) {
        const int length = d->textureSize.height();
        OpenGLTextureBinder<OGL::Target2D> binder(&texture);
        if (texture.height() < length)
            texture.initialize(d->textureSize);
        texture.upload(0, 0, length*2, length, d->rings[0].image.bits());
        texture.upload(length*2+5, 0, length*2, length, d->rings[1].image.bits());
        d->upload = false;
    }
}

void BusyIconItem::updatePolish() {
    if (_Change(d->radius, qMin(width(), height())*0.5) || d->redraw) {
        const int length = d->radius + 1.5;
        d->textureSize.rwidth() = length * 4 + 5;
        d->textureSize.rheight() = length;
        d->textureScale.rx() = 1.0 / (length * 4 + 5);
        d->textureScale.ry() = 1.0 / length;
        d->rings[0].tx0 = d->radius * d->textureScale.x();
        d->rings[1].tx0 = (length*2 + 5 + d->radius) * d->textureScale.x();
        for (int i=0; i<2; ++i) {
            auto &image = d->rings[i].image;
            image = QImage(length*2, length, QImage::Format_ARGB32_Premultiplied);
            image.fill(0x0);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QBrush(d->rings[i].color));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(d->radius, 0), d->radius, d->radius);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
            painter.setBrush(QBrush(QColor(Qt::red)));
            const auto in = qMax(d->radius - d->thickness, 0.0);
            painter.drawEllipse(QPointF(d->radius, 0), in, in);
        }
        d->upload = true;
        d->redraw = false;
        reserve(UpdateMaterial);
    }
    QPointF o(d->radius, d->radius);
    if (width() < height())
        o.ry() += 0.5*(height() - width());
    else if (height() < width())
        o.rx() += 0.5*(width() - height());
    const float txrad = d->radius*d->textureScale.x();
    const float tymax = d->radius*d->textureScale.y();

    auto it = attributes().begin();
    auto fill = [&] (int index, double tan1, double tan2, int quater) {
        const float tx0 = d->rings[index].tx0;
        const float ty0 = d->rings[index].ty0;
        QMatrix4x4 mat;
        mat.translate(o.x(), o.y());
        mat.rotate(90.0*quater, 0, 0, 1);
        it->vertex = mat*QPointF{d->radius*tan1, -d->radius};
        it->texCoord = {tx0 + txrad*tan1, ty0 + tymax};
        ++it;
        it->vertex = mat*QPointF{d->radius*tan2, -d->radius};
        it->texCoord = {tx0 + txrad*tan2, ty0 + tymax};
        ++it;
        it->vertex = mat*QPointF{0, 0};
        it->texCoord = {tx0, ty0};
        ++it;
    };

    if (d->angle < d->last)
        d->filled = !d->filled;
    d->last = d->angle;

    if (d->angle <= M_PI/4.0) {
        const auto t = qTan(d->angle);
        fill( d->filled,  0, t, 0);
        fill(!d->filled,  t, 1, 0);
        if (_Change(d->quater, 0)) {
            fill(!d->filled, -1, 0, 0);
            fill(!d->filled, -1, 1, 1);
            fill(!d->filled, -1, 1, 2);
            fill(!d->filled, -1, 1, 3);
        }
    } else if (d->angle <= M_PI*3/4) {
        const auto t = qTan(d->angle - M_PI/2);
        fill( d->filled, -1, t, 1);
        fill(!d->filled,  t, 1, 1);
        if (_Change(d->quater, 1)) {
            fill(!d->filled, -1, 0, 0);
            fill( d->filled,  0, 1, 0);
            fill(!d->filled, -1, 1, 2);
            fill(!d->filled, -1, 1, 3);
        }
    } else if (d->angle <= M_PI*5/4) {
        const auto t = qTan(d->angle - M_PI);
        fill( d->filled, -1, t, 2);
        fill(!d->filled,  t, 1, 2);
        if (_Change(d->quater, 2)) {
            fill(!d->filled, -1, 0, 0);
            fill( d->filled,  0, 1, 0);
            fill( d->filled, -1, 1, 1);
            fill(!d->filled, -1, 1, 3);
        }
    } else if (d->angle <= M_PI*7/4) {
        const auto t = qTan(d->angle - M_PI*3/2);
        fill( d->filled, -1, t, 3);
        fill(!d->filled,  t, 1, 3);
        if (_Change(d->quater, 3)) {
            fill(!d->filled, -1, 0, 0);
            fill( d->filled,  0, 1, 0);
            fill( d->filled, -1, 1, 1);
            fill( d->filled, -1, 1, 2);
        }
    } else {
        const auto t = qTan(d->angle - 2*M_PI);
        fill( d->filled, -1, t, 0);
        fill(!d->filled,  t, 0, 0);
        if (_Change(d->quater, 4)) {
            fill( d->filled,  0, 1, 0);
            fill( d->filled, -1, 1, 1);
            fill( d->filled, -1, 1, 2);
            fill( d->filled, -1, 1, 3);
        }
    }
    reserve(UpdateGeometry);
}
