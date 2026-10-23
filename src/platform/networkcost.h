// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Reports what the current network costs, so Core can hold "Wi-Fi only"
/// content updates back on a metered one (vauchi/private#733 D6). Core
/// decides; the shell only observes.

#pragma once

#include <QJsonObject>
#include <QNetworkInformation>
#include <QObject>
#include <QString>

namespace vauchi {

/// Unknown unless the backend can tell metered from not and there is a
/// connection to judge.
inline QString networkCostName(bool backendKnowsMetering, bool connected, bool metered) {
    if (!backendKnowsMetering || !connected) return QStringLiteral("unknown");
    return metered ? QStringLiteral("metered") : QStringLiteral("unmetered");
}

inline QJsonObject networkCostEvent(const QString &cost) {
    return QJsonObject{{QStringLiteral("NetworkCostChanged"),
                        QJsonObject{{QStringLiteral("cost"), cost}}}};
}

/// Calls `report` with the cost now and again whenever it changes, for as
/// long as `context` lives. Qt's NetworkManager backend is the one on Linux
/// that knows metering; without it the cost stays unknown.
template <typename Report>
void watchNetworkCost(QObject *context, Report report) {
    if (!QNetworkInformation::loadBackendByFeatures(
            QNetworkInformation::Feature::Metered)) {
        report(networkCostName(false, false, false));
        return;
    }
    auto *information = QNetworkInformation::instance();
    auto reportCurrent = [information, report]() {
        const auto reachability = information->reachability();
        const bool connected =
            reachability != QNetworkInformation::Reachability::Disconnected &&
            reachability != QNetworkInformation::Reachability::Unknown;
        report(networkCostName(true, connected, information->isMetered()));
    };
    QObject::connect(information, &QNetworkInformation::isMeteredChanged,
                     context, reportCurrent);
    QObject::connect(information, &QNetworkInformation::reachabilityChanged,
                     context, reportCurrent);
    reportCurrent();
}

} // namespace vauchi
