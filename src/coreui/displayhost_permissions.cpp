// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayhost.h"
#include "../platform/permissionprompt.h"

void QtDisplayHost::requestPermission(const QJsonObject &payload) {
    const auto value = payload.value("transport");
    if (!value.isString() || value.toString().isEmpty() || value.toString().toUtf8().size() > 64) return;
    if (!m_permissionPrompt) {
        m_permissionPrompt = new PermissionPrompt(this);
        connect(m_permissionPrompt, &PermissionPrompt::completed, this,
            [this, generation = m_generation](const QString &event, const QString &transport) {
                if (generation == m_generation)
                    emitEvent({{event, QJsonObject{{"transport", transport}}}});
            }, Qt::QueuedConnection);
    }
    m_permissionPrompt->request(value.toString());
}

void QtDisplayHost::resetPermissionPrompt() {
    delete m_permissionPrompt;
    m_permissionPrompt = nullptr;
}
