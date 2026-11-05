// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include <QApplication>
#include <QFrame>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QStyleHints>
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#include <QAccessibilityHints>
#endif

namespace {
bool reducedMotion() {
    if (qEnvironmentVariableIsSet("QT_REDUCE_MOTION")
        || !QApplication::isEffectEnabled(Qt::UI_General)) return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if (const auto *hints = QGuiApplication::styleHints()->accessibility())
        return hints->motionPreference() == Qt::MotionPreference::ReducedMotion;
#endif
    return false;
}
}

void QtDisplayHost::celebrate(const QJsonObject &feedback) {
    const QString haptic = feedback.value("haptic").toString();
    const QString sound = feedback.value("sound").toString();
    const QString animation = feedback.value("animation").toString();
    if (!QStringList{"success", "light", "none"}.contains(haptic)
        || !QStringList{"exchange_chime", "none"}.contains(sound)
        || animation != "cards_meet" || reducedMotion()) return;
    delete m_celebration;
    auto *effect = new QWidget(this);
    m_celebration = effect;
    effect->setObjectName("qt-cards-meet");
    effect->setAttribute(Qt::WA_TransparentForMouseEvents);
    effect->setFocusPolicy(Qt::NoFocus);
    effect->setFixedSize(220, 100);
    effect->move(centralWidget()->geometry().center() - QPoint(110, 50));
    auto *group = new QParallelAnimationGroup(effect);
    for (int index = 0; index < 2; ++index) {
        auto *card = new QFrame(effect);
        card->setFixedSize(96, 64);
        card->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid %2; border-radius: 8px;")
            .arg(palette().color(QPalette::Base).name(), palette().color(QPalette::Highlight).name()));
        const QPoint start(index == 0 ? 4 : 120, 18);
        card->move(start);
        auto *movement = new QPropertyAnimation(card, "pos", group);
        movement->setStartValue(start);
        movement->setEndValue(QPoint(index == 0 ? 48 : 76, 18));
        movement->setDuration(600);
        movement->setEasingCurve(QEasingCurve::InOutCubic);
        group->addAnimation(movement);
    }
    connect(group, &QParallelAnimationGroup::finished, effect, &QObject::deleteLater);
    effect->show();
    effect->raise();
    group->start();
}
