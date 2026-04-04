#include "vu_meter_widget.h"

#include <QPainter>

#include <algorithm>

namespace {
constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 0.0f;
}

VuMeterWidget::VuMeterWidget(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(70);
    setMinimumHeight(220);
}

void VuMeterWidget::setLevelsDb(float leftDb, float rightDb) {
    m_leftDb  = clampDb(leftDb);
    m_rightDb = clampDb(rightDb);

    update();
}

float VuMeterWidget::clampDb(float value) const {
    if (value < kMinDb) return kMinDb;
    if (value > kMaxDb) return kMaxDb;
    return value;
}

QColor VuMeterWidget::colorForDb(float db) const {
    if (db >= -8.0f) return QColor("#ff0000");
    if (db >= -12.0f) return QColor("#ff6600");
    if (db >= -20.0f) return QColor("#ffff00");
    if (db >= -40.0f) return QColor("#00ff00");
    return QColor("#66ff00");
}

int VuMeterWidget::levelToY(float db, const QRect& meterRect) const {
    const float t = (clampDb(db) - kMinDb) / (kMaxDb - kMinDb);
    return meterRect.bottom() - static_cast<int>(t * meterRect.height());
}

void VuMeterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(rect(), QColor("#222222"));

    const int margin = 3;
    const int spacing = 3;
    const int colWidth = (width() - margin * 2 - spacing) / 2;
    const int meterTop = 14;
    const int meterBottomPad = 16;
    const int meterHeight = height() - meterTop - meterBottomPad;

    const QRect leftRect(margin, meterTop, colWidth, meterHeight);
    const QRect rightRect(margin + colWidth + spacing, meterTop, colWidth, meterHeight);

    const int segH = 4;
    const int segGap = 1;
    const int segCount = std::max(12, meterHeight / (segH + segGap));

    auto drawMeter = [&](const QRect& r, float levelDb, const QString& ch) {
        const float clamped = clampDb(levelDb);
        const int topY = levelToY(clamped, r);

        for (int i = 0; i < segCount; ++i) {
            const int y = r.bottom() - (i + 1) * segH - i * segGap;
            const QRect seg(r.left(), y, r.width(), segH);
            if (y >= topY) {
                const float dbAtSeg = kMinDb + (kMaxDb - kMinDb)
                    * (static_cast<float>(i) / std::max(1, segCount - 1));
                p.fillRect(seg, colorForDb(dbAtSeg));
            } else {
                p.fillRect(seg, QColor("#2f2f2f"));
            }
        }

        QFont f = p.font();
        f.setPointSize(7);
        p.setFont(f);
        p.setPen(QColor("#eeeeee"));
        p.drawText(QRect(r.left(), 1, r.width(), 12), Qt::AlignCenter, ch);

        f.setPointSize(5);
        p.setFont(f);
        p.drawText(QRect(r.left() - 1, r.bottom() + 2, r.width() + 2, 12), Qt::AlignCenter,
               QString::asprintf("%.1fdB", clamped));
    };

    drawMeter(leftRect, m_leftDb, "L");
    drawMeter(rightRect, m_rightDb, "R");
}
