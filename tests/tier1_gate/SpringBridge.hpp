// ============================================================================
// SpringBridge.hpp
//
// Qt6 QObject wrapper around SpringStatePortable. Drives the spring via a
// QTimer at 60 Hz and exposes spring.value as a Q_PROPERTY so QML Items can
// bind to it directly for scale/x/y animations — no QML SpringAnimation used,
// meaning we get the EXACT same physics curve as txui::SpringState.
// ============================================================================
#pragma once
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include "SpringStatePortable.hpp"

class SpringBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(double value    READ value    NOTIFY valueChanged)
    Q_PROPERTY(double target   READ target   WRITE setTarget   NOTIFY targetChanged)
    Q_PROPERTY(bool   settled  READ isSettled NOTIFY settledChanged)

public:
    explicit SpringBridge(QObject* parent = nullptr) : QObject(parent) {
        m_timer.setInterval(16);  // ~60 Hz
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SpringBridge::tick);
    }

    double value()    const { return m_spring.value; }
    double target()   const { return m_spring.target; }
    bool   isSettled() const { return m_settled; }

    Q_INVOKABLE void setTarget(double t) {
        if (m_spring.target != t) {
            m_spring.set_target(t);
            m_settled = false;
            m_timer.start();
            emit targetChanged();
        }
    }

    Q_INVOKABLE void reset(double from, double to) {
        m_spring.reset(from, to);
        m_settled = false;
        m_timer.start();
        emit valueChanged();
        emit targetChanged();
    }

signals:
    void valueChanged();
    void targetChanged();
    void settledChanged();

private slots:
    void tick() {
        constexpr double dt = 1.0 / 60.0;
        const bool now_settled = m_spring.step(dt);
        emit valueChanged();

        if (now_settled && !m_settled) {
            m_settled = true;
            m_timer.stop();
            emit settledChanged();
        }
    }

private:
    tinexus::migration::SpringStatePortable m_spring;
    QTimer m_timer;
    bool   m_settled {true};
};
