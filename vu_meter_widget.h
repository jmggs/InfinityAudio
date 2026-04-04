#pragma once

#include <QWidget>

class VuMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit VuMeterWidget(QWidget* parent = nullptr);

    // Thread-safe: called from main thread only.
    void setLevelsDb(float leftDb, float rightDb);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float  clampDb(float value) const;
    QColor colorForDb(float db) const;
    int    levelToY(float db, const QRect& meterRect) const;

    float  m_leftDb{-60.0f};
    float  m_rightDb{-60.0f};
};
