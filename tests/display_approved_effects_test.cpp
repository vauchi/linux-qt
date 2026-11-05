// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include <QEventLoop>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>

using namespace display_test;

namespace {
QImage preparedQr;
bool invocationContinued = false;
bool disableEffects = false;

QJsonObject celebrate(const QString &animation = "cards_meet") {
    return {{"Celebrate", QJsonObject{{"haptic", "success"},
        {"sound", "exchange_chime"}, {"animation", animation}}}};
}

QWidget *ceremony() {
    return window()->findChild<QWidget *>("qt-cards-meet");
}

QImage qrImage() {
    for (auto *label : window()->findChildren<QLabel *>())
        if (label->accessibleName() == "Prepared QR" && !label->pixmap().isNull())
            return label->pixmap().toImage();
    assert(false);
    return {};
}

QJsonObject qrSurface(int revision) {
    return surface("first", revision, "Prepared surface", QJsonArray{QJsonObject{{"Qr", QJsonObject{
        {"id", "prepared-code"}, {"purpose", "display"},
        {"payloads", QJsonArray{"opaque-prepared-payload"}},
        {"accessibility", QJsonObject{{"label", "Prepared QR"}}}}}}});
}

// Feature: generic_presentation_protocol.feature — Core's prepared QR node is authoritative.
QList<Step> qr() {
    const QJsonObject legacy{{"QrDisplay", QJsonObject{{"data", "different-legacy-payload"}}}};
    return {{{qrSurface(1), profile("first")}, [] {
        preparedQr = qrImage();
        assert(!preparedQr.isNull());
    }}, {{legacy, qrSurface(2), legacy}, [] {
        assert(qrImage() == preparedQr);
        assert(events.isEmpty());
    }}};
}

QList<Step> invocation() {
    return {{{surface("first", 1, "Before"), profile("first"),
        QJsonObject{{"EmitDocument", QJsonObject{{"document", QJsonObject{
            {"media_type", "application/octet-stream"}, {"schema", "opaque-v1"},
            {"data", QJsonArray{83,72,79,85,76,68,45,78,79,84,45,80,82,73,78,84}}}}}}},
        QJsonObject{{"FinishInvocation", QJsonObject{{"outcome", "failed"}}}}}, [] {
        assert(window()->isVisible());
        assert(events.isEmpty());
    }}, {{surface("first", 2, "After")}, [] {
        bool found = false;
        for (auto *label : window()->findChildren<QLabel *>())
            found = found || (label->isVisible() && label->text() == "After");
        assert(found);
        invocationContinued = true;
        assert(events.isEmpty());
    }}};
}

class MeetingObserver : public QObject {
public:
    QWidget *left;
    QWidget *right;
    bool met = false;
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Move)
            met = met || left->geometry().intersects(right->geometry());
        return false;
    }
};

// Feature: contact_exchange.feature — render the supplied closed animation intent.
QList<Step> animated() {
    const QJsonArray input{QJsonObject{{"Input", QJsonObject{{"binding_id", "name"},
        {"label", "Prepared input"}, {"value", "Unchanged"}, {"input_kind", "text"},
        {"enabled", true}}}}};
    return {{{surface("first", 1, "Prepared", input), profile("first")}, [] {
        auto *field = window()->findChild<QLineEdit *>("name");
        assert(field);
        field->setFocus();
        assert(field->hasFocus());
    }}, {{celebrate()}, [] {
        auto *effect = ceremony();
        assert(effect && effect->isVisible());
        assert(effect->testAttribute(Qt::WA_TransparentForMouseEvents));
        assert(effect->focusPolicy() == Qt::NoFocus);
        auto cards = effect->findChildren<QFrame *>(QString(), Qt::FindDirectChildrenOnly);
        assert(cards.size() == 2);
        assert(!cards.at(0)->geometry().intersects(cards.at(1)->geometry()));
        MeetingObserver observer;
        observer.left = cards.at(0);
        observer.right = cards.at(1);
        for (auto *card : cards) card->installEventFilter(&observer);
        QEventLoop completed;
        QObject::connect(effect, &QObject::destroyed, &completed, &QEventLoop::quit);
        completed.exec();
        assert(observer.met);
        assert(ceremony() == nullptr);
        auto *field = window()->findChild<QLineEdit *>("name");
        assert(field && field->hasFocus());
        assert(field->text() == "Unchanged");
        assert(events.isEmpty());
    }}};
}

QList<Step> suppressed() {
    return {{{celebrate(), celebrate("none"), celebrate("invalid")}, [] {
        assert(ceremony() == nullptr);
        assert(events.isEmpty());
    }}};
}

QList<Step> none() {
    return {{{celebrate("none"), celebrate("invalid"),
        QJsonObject{{"Celebrate", QJsonObject{{"animation", "cards_meet"}}}}}, [] {
        assert(ceremony() == nullptr);
        assert(events.isEmpty());
    }}};
}

QList<Step> reset() {
    return {{{celebrate(), celebrate()}, [] {
        const auto effects = window()->findChildren<QWidget *>("qt-cards-meet");
        assert(effects.size() == 1);
        assert(effects.first()->isVisible());
    }}, {{"ResetApplication"}, [] {
        assert(ceremony() == nullptr);
        assert(events.isEmpty());
    }}};
}

void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    qunsetenv("QT_REDUCE_MOTION");
    const QString name = argc > 1 ? QString::fromUtf8(argv[1]) : "animation";
    if (name == "qr") steps = qr();
    else if (name == "invocation") steps = invocation();
    else if (name == "animation") steps = animated();
    else if (name == "reduced") { qputenv("QT_REDUCE_MOTION", "1"); steps = suppressed(); }
    else if (name == "disabled") { disableEffects = true; steps = suppressed(); }
    else if (name == "none") steps = none();
    else if (name == "reset") steps = reset();
    else return 2;
    steps.prepend({{}, [] { QApplication::setEffectEnabled(Qt::UI_General, !disableEffects); }});
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    if (name == "invocation") assert(invocationContinued);
    return result;
}
