// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// The vauchi.app/x landing page's "Open in Vauchi" button and every shared
/// `vauchi://` link must reach the installed app (vauchi/private#650): the
/// desktop entry registers the scheme, only vauchi:// arguments go to Core,
/// and a second launch hands its link to the running instance instead of
/// opening the same data twice.

#include "../src/platform/deeplink.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <cassert>
#include <cstdio>

static QString desktopEntryValue(const QString &key) {
    QFile file(QStringLiteral(VAUCHI_DESKTOP_ENTRY));
    const bool opened = file.open(QIODevice::ReadOnly | QIODevice::Text);
    assert(opened);
    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    for (const QString &line : lines) {
        if (line.startsWith(key + '=')) return line.mid(key.size() + 1);
    }
    return QString();
}

static void test_desktop_entry_registers_the_scheme() {
    assert(desktopEntryValue(QStringLiteral("MimeType"))
           == QStringLiteral("x-scheme-handler/vauchi;"));
    assert(desktopEntryValue(QStringLiteral("Exec"))
           == QStringLiteral("qvauchi %u"));
    printf("  PASS: desktop_entry_registers_the_scheme\n");
}

static void test_only_vauchi_links_are_passed_to_core() {
    const QStringList args{
        QStringLiteral("qvauchi"),
        QStringLiteral("vauchi://exchange?pk=AAAA&n=BBBB"),
        QStringLiteral("--render-fixture"),
        QStringLiteral("/home/someone/contacts.vcf"),
        QStringLiteral("https://vauchi.app/x#pk=AAAA&n=BBBB"),
        QStringLiteral("VAUCHI://exchange?pk=CCCC&n=DDDD"),
        QStringLiteral("vauchi-evil://exchange"),
    };
    assert(vauchi::deepLinkUris(args)
           == (QStringList{QStringLiteral("vauchi://exchange?pk=AAAA&n=BBBB"),
                           QStringLiteral("VAUCHI://exchange?pk=CCCC&n=DDDD")}));
    printf("  PASS: only_vauchi_links_are_passed_to_core\n");
}

static void test_a_second_launch_hands_its_links_to_the_running_one() {
    const QString name = QStringLiteral("vauchi-deep-link-test-%1")
                             .arg(QCoreApplication::applicationPid());
    vauchi::DeepLinkRelay running(name);
    assert(running.listen());

    QStringList received;
    QEventLoop loop;
    QObject::connect(&running, &vauchi::DeepLinkRelay::linkReceived,
                     [&](const QString &uri) {
                         received.append(uri);
                         if (received.size() == 2) loop.quit();
                     });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);

    vauchi::DeepLinkRelay second(name);
    const QStringList links{QStringLiteral("vauchi://exchange?pk=AAAA&n=BBBB"),
                            QStringLiteral("vauchi://contact/xyz")};
    assert(second.handOff(links));
    loop.exec();

    assert(received == links);
    printf("  PASS: a_second_launch_hands_its_links_to_the_running_one\n");
}

static void test_with_no_running_instance_the_launch_keeps_its_links() {
    vauchi::DeepLinkRelay alone(QStringLiteral("vauchi-deep-link-test-none-%1")
                                    .arg(QCoreApplication::applicationPid()));
    assert(!alone.handOff({QStringLiteral("vauchi://exchange?pk=AAAA&n=BBBB")}));
    printf("  PASS: with_no_running_instance_the_launch_keeps_its_links\n");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    printf("deep_link_test\n");
    test_desktop_entry_registers_the_scheme();
    test_only_vauchi_links_are_passed_to_core();
    test_a_second_launch_hands_its_links_to_the_running_one();
    test_with_no_running_instance_the_launch_keeps_its_links();
    printf("All deep_link tests passed\n");
    return 0;
}
