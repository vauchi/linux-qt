// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>
#endif

using namespace display_test;
namespace {
QJsonObject request(const QJsonValue &transport) {
    return {{"RequestPermission", QJsonObject{{"transport", transport}}}};
}
QJsonArray answer(const QString &name, const QString &transport = "camera") {
    return {event(name, {{"transport", transport}})};
}
QList<Step> unavailable() {
    return {{{request("BLE")}, [] {
        assert(events == answer("HardwareUnavailable", "BLE")); events = {};
    }, [] { return !events.isEmpty(); }}, {{request("camera")}, [] {
        assert(events == answer("HardwareUnavailable"));
    }, [] { return !events.isEmpty(); }}};
}
QList<Step> malformed() {
    return {{{request(3), request(""), request(QString(65, 'x')),
        QJsonObject{{"RequestPermission", QJsonObject{}}}}, [] { assert(events.isEmpty()); }}};
}
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
const QString service = "org.freedesktop.portal.Desktop";
const QString base = "/org/freedesktop/portal/desktop";
const QString requestInterface = "org.freedesktop.portal.Request";
QDBusConnection server() { return QDBusConnection("permission_test_portal"); }
class FakeRequest : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
public:
    int closes = 0;
public slots:
    void Close() { ++closes; emit closed(); }
signals:
    void closed();
};
class FakePortal : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Camera")
public:
    QStringList paths;
    FakeRequest endpoint;
    bool automatic = true;
    bool wrongPath = false;
    bool spoof = false;
    bool alternate = false;
    bool early = false;
    bool foreign = false;
    bool flood = false;
    uint response = 0;
    QDBusMessage deferred;
    QString deferredPath;
    void send(const QString &path, uint code, const QDBusConnection &bus = server()) {
        auto message = QDBusMessage::createSignal(path, requestInterface, "Response");
        message << code << QVariantMap{};
        assert(bus.send(message));
    }
public slots:
    void assertFloodRejected() {
        QMetaObject::invokeMethod(this, [] {
            assert(events == answer("HardwareUnavailable"));
        }, Qt::QueuedConnection);
    }
    QDBusObjectPath AccessCamera(const QVariantMap &options) {
        assert(options.size() == 1);
        assert(message().service() != server().baseService());
        const QString token = options.value("handle_token").toString();
        assert(!token.isEmpty());
        QString sender = message().service(); sender.remove(0, 1); sender.replace('.', '_');
        const QString path = base + "/request/" + (foreign ? "other_client" : sender) + "/" + token + (alternate ? "_actual" : "");
        paths.append(path);
        assert(server().registerObject(path, &endpoint, QDBusConnection::ExportAllSlots));
        if (flood) {
            setDelayedReply(true);
            deferred = message(); deferredPath = path;
            for (int index = 0; index < 17; ++index) send(path + "_other" + QString::number(index), 0);
            assert(server().send(QDBusMessage::createSignal(base + "/test",
                "app.vauchi.PermissionTest", "FloodSent")));
        }
        if (automatic && early) send(path, response);
        else if (automatic) QMetaObject::invokeMethod(this, [this, path] {
            if (wrongPath) send(path + "_other", 0);
            if (spoof) {
                auto impostor = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "impostor");
                send(path, 0, impostor);
            }
            send(path, response);
        }, Qt::QueuedConnection);
        return QDBusObjectPath(path);
    }
};
FakePortal *portal = nullptr;
void serve() {
    portal = new FakePortal;
    QObject::connect(&portal->endpoint, &FakeRequest::closed, portal, [] {
        if (portal->deferred.type() == QDBusMessage::MethodCallMessage) {
            const auto reply = portal->deferred.createReply(
                QVariantList{QVariant::fromValue(QDBusObjectPath(portal->deferredPath))});
            assert(server().send(reply));
            portal->deferred = {};
        }
    });
    auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "permission_test_portal");
    assert(bus.registerService(service));
    assert(bus.registerObject(base, portal, QDBusConnection::ExportAllSlots));
    assert(QDBusConnection::sessionBus().connect(service, base + "/test",
        "app.vauchi.PermissionTest", "FloodSent", portal, SLOT(assertFloodRejected())));
}
QList<Step> result(uint code, bool duplicate = false) {
    serve(); portal->response = code;
    const QJsonArray commands = duplicate ? QJsonArray{request("camera"), request("camera")}
                                         : QJsonArray{request("camera")};
    const QString outcome = code == 0 ? "PermissionGranted" : code == 1 ? "PermissionDenied" : "HardwareUnavailable";
    return {{commands, [outcome] {
        assert(events == answer(outcome));
        assert(portal->paths.size() == 1);
        assert(portal->endpoint.closes == (portal->flood ? 1 : 0));
    }, [] { return !events.isEmpty() && (!portal->flood || portal->endpoint.closes > 0); }}};
}
QList<Step> reset() {
    serve(); portal->automatic = false;
    return {{{request("camera")}, [] {}, [] { return portal->paths.size() == 1; }},
        {{"ResetApplication"}, [] { portal->send(portal->paths.first(), 0); }},
        {{request("camera")}, [] { portal->send(portal->paths.last(), 0); },
            [] { return portal->paths.size() == 2; }},
        {{}, [] {
            assert(events == answer("PermissionGranted"));
            assert(portal->paths.first() != portal->paths.last());
            assert(portal->endpoint.closes == 1);
        }, [] { return !events.isEmpty() && portal->endpoint.closes == 1; }}};
}
QList<Step> vanished() {
    serve(); portal->automatic = false;
    return {{{request("camera")}, [] {
        assert(server().unregisterService(service));
    }, [] { return portal->paths.size() == 1; }}, {{}, [] {
        assert(events == answer("HardwareUnavailable"));
    }, [] { return !events.isEmpty(); }}};
}
#endif
QString name;
void setup() {
    if (name == "unavailable") steps = unavailable();
    else if (name == "malformed") steps = malformed();
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
    else if (name == "granted") steps = result(0);
    else if (name == "denied") steps = result(1);
    else if (name == "failed") steps = result(2);
    else if (name == "duplicate") steps = result(0, true);
    else if (name == "alternate") { steps = result(0); portal->alternate = true; }
    else if (name == "foreign") { steps = result(2); portal->foreign = true; portal->response = 0; }
    else if (name == "flood") { steps = result(2); portal->flood = true; portal->response = 1; }
    else if (name == "alternate_reset") { steps = reset(); portal->alternate = true; }
    else if (name == "early") { steps = result(0); portal->early = true; }
    else if (name == "wrong_path") { steps = result(1); portal->wrongPath = true; }
    else if (name == "spoof") { steps = result(1); portal->spoof = true; }
    else if (name == "reset") steps = reset();
    else if (name == "vanished") steps = vanished();
#endif
    else std::abort();
    schedule();
}
}
Q_COREAPP_STARTUP_FUNCTION(setup)
int main(int argc, char **argv) {
    qunsetenv("QT_DISPLAY_SNAPSHOT");
    name = argc > 1 ? QString::fromUtf8(argv[1]) : "unavailable";
    const int result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
#ifdef VAUCHI_HAS_PERMISSION_PORTAL
#include "display_permission_test.moc"
#endif
