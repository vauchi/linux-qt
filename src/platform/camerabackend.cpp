// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camerabackend.h"
#include "hardwarebackend.h"
#include "../i18n.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVideoWidget>
#include <QMediaDevices>
#include <QCameraDevice>

#include <zbar.h>

CameraBackend::CameraBackend(HardwareBackend *parent)
    : QObject(parent), m_backend(parent) {}

void CameraBackend::startScan(HardwareBackend *parent) {
    auto *backend = new CameraBackend(parent);

    // Find default camera
    auto cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        emit parent->qrScanned(QString()); // trigger paste fallback
        backend->deleteLater();
        return;
    }

    backend->m_camera = new QCamera(cameras.first(), backend);
    backend->m_sink = new QVideoSink(backend);
    backend->m_session.setCamera(backend->m_camera);
    backend->m_session.setVideoSink(backend->m_sink);

    // Create scanning dialog
    backend->m_dialog = new QDialog(qobject_cast<QWidget *>(parent->parent()));
    backend->m_dialog->setWindowTitle(tr_vauchi("platform.qr_scan_title", "Scan QR Code"));
    backend->m_dialog->resize(400, 350);

    auto *layout = new QVBoxLayout(backend->m_dialog);
    auto *videoWidget = new QVideoWidget;
    backend->m_session.setVideoOutput(videoWidget);
    layout->addWidget(videoWidget);

    auto *status = new QLabel(tr_vauchi("platform.qr_camera_prompt", "Point camera at QR code..."));
    status->setAlignment(Qt::AlignCenter);
    layout->addWidget(status);

    auto *cancelBtn = new QPushButton(tr_vauchi("platform.cancel", "Cancel"));
    layout->addWidget(cancelBtn);

    QObject::connect(cancelBtn, &QPushButton::clicked, backend->m_dialog, &QDialog::reject);
    QObject::connect(backend->m_dialog, &QDialog::rejected, backend, [backend, parent]() {
        backend->m_camera->stop();
        emit parent->qrScanned(QString()); // trigger paste fallback
        backend->deleteLater();
    });

    // Process frames for QR codes
    QObject::connect(backend->m_sink, &QVideoSink::videoFrameChanged,
                     backend, &CameraBackend::processFrame);

    backend->m_camera->start();
    backend->m_dialog->show();
}

void CameraBackend::processFrame(const QVideoFrame &frame) {
    if (m_found) return;

    QVideoFrame f = frame;
    if (!f.map(QVideoFrame::ReadOnly)) return;

    // Convert to grayscale for zbar
    QImage image = f.toImage().convertToFormat(QImage::Format_Grayscale8);
    f.unmap();

    if (image.isNull()) return;

    // Scan with zbar
    zbar::ImageScanner scanner;
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0);
    scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1);

    zbar::Image zbarImage(
        static_cast<unsigned>(image.width()),
        static_cast<unsigned>(image.height()),
        "Y800",
        image.constBits(),
        static_cast<unsigned long>(image.sizeInBytes()));

    int n = scanner.scan(zbarImage);
    if (n > 0) {
        for (auto it = zbarImage.symbol_begin(); it != zbarImage.symbol_end(); ++it) {
            if (it->get_type() == zbar::ZBAR_QRCODE) {
                m_found = true;
                QString data = QString::fromStdString(it->get_data());

                m_camera->stop();
                m_dialog->accept();

                // Emit scanned data — the hardware backend will forward
                // it as a QrScanned event to core via CABI.
                emit m_backend->qrScanned(data);

                deleteLater();
                return;
            }
        }
    }
}
