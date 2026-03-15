// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QDialog>

class HardwareBackend;

/// Camera-based QR code scanner using Qt Multimedia + ZBar.
class CameraBackend : public QObject {
    Q_OBJECT

public:
    /// Start QR scanning in a modal dialog. Emits parent->qrScanned() on success.
    static void startScan(HardwareBackend *parent);

private:
    explicit CameraBackend(HardwareBackend *parent);
    void processFrame(const QVideoFrame &frame);

    QCamera *m_camera = nullptr;
    QMediaCaptureSession m_session;
    QVideoSink *m_sink = nullptr;
    QDialog *m_dialog = nullptr;
    HardwareBackend *m_backend;
    bool m_found = false;
};
