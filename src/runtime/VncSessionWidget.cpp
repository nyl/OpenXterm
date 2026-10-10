#include "VncSessionWidget.h"

#include "VncClientWorker.h"

#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr quint32 kKeySymBackSpace = 0xff08;
constexpr quint32 kKeySymTab = 0xff09;
constexpr quint32 kKeySymReturn = 0xff0d;
constexpr quint32 kKeySymEscape = 0xff1b;
constexpr quint32 kKeySymHome = 0xff50;
constexpr quint32 kKeySymLeft = 0xff51;
constexpr quint32 kKeySymUp = 0xff52;
constexpr quint32 kKeySymRight = 0xff53;
constexpr quint32 kKeySymDown = 0xff54;
constexpr quint32 kKeySymPageUp = 0xff55;
constexpr quint32 kKeySymPageDown = 0xff56;
constexpr quint32 kKeySymEnd = 0xff57;
constexpr quint32 kKeySymInsert = 0xff63;
constexpr quint32 kKeySymDelete = 0xffff;
constexpr quint32 kKeySymShiftL = 0xffe1;
constexpr quint32 kKeySymControlL = 0xffe3;
constexpr quint32 kKeySymAltL = 0xffe9;
constexpr quint32 kKeySymF1 = 0xffbe;

QCursor buildDotCursor() {
    QImage image(7, 7, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int dx = x - 3;
            const int dy = y - 3;
            const int dist2 = dx * dx + dy * dy;

            if (dist2 <= 9) {
                image.setPixelColor(x, y, QColor(255, 255, 255, 255));
            }
            if (dist2 <= 4) {
                image.setPixelColor(x, y, QColor(0, 0, 0, 255));
            }
        }
    }

    return QCursor(QPixmap::fromImage(image), 3, 3);
}
}

VncSessionWidget::VncSessionWidget(const domain::SessionProfile& profile, QWidget* parent)
    : QWidget(parent), profile_(profile) {
    runtimeViewOnly_ = profile_.vnc.viewOnly;

    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();

    connectButton_ = new QPushButton("Connect", this);
    disconnectButton_ = new QPushButton("Disconnect", this);
    disconnectButton_->setEnabled(false);
    viewerButton_ = new QPushButton("Open External Viewer", this);
    viewerButton_->setEnabled(false);

    fitButton_ = new QToolButton(this);
    fitButton_->setText("Fit");
    fitButton_->setCheckable(true);
    fitButton_->setChecked(true);

    zoomOutButton_ = new QToolButton(this);
    zoomOutButton_->setText("-");

    zoomResetButton_ = new QToolButton(this);
    zoomResetButton_->setText("100%");

    zoomInButton_ = new QToolButton(this);
    zoomInButton_->setText("+");

    refreshButton_ = new QToolButton(this);
    refreshButton_->setText("Refresh");

    reconnectButton_ = new QToolButton(this);
    reconnectButton_->setText("Reconnect");

    fullscreenButton_ = new QToolButton(this);
    fullscreenButton_->setText("Fullscreen");

    cadButton_ = new QToolButton(this);
    cadButton_->setText("Ctrl+Alt+Del");

    viewOnlyButton_ = new QToolButton(this);
    viewOnlyButton_->setText("View Only");
    viewOnlyButton_->setCheckable(true);
    viewOnlyButton_->setChecked(runtimeViewOnly_);

    zoomLabel_ = new QLabel("Fit", this);

    controls->addWidget(connectButton_);
    controls->addWidget(disconnectButton_);
    controls->addWidget(viewerButton_);
    controls->addSpacing(8);
    controls->addWidget(fitButton_);
    controls->addWidget(zoomOutButton_);
    controls->addWidget(zoomResetButton_);
    controls->addWidget(zoomInButton_);
    controls->addWidget(zoomLabel_);
    controls->addSpacing(8);
    controls->addWidget(refreshButton_);
    controls->addWidget(reconnectButton_);
    controls->addWidget(cadButton_);
    controls->addWidget(viewOnlyButton_);
    controls->addWidget(fullscreenButton_);
    controls->addStretch();

    endpointLabel_ = new QLabel(this);
    endpointLabel_->setText(QString("Target: %1:%2").arg(profile_.host).arg(profile_.port));

    renderSurface_ = new QLabel(this);
    renderSurface_->setMinimumSize(640, 360);
    renderSurface_->setAlignment(Qt::AlignCenter);
    renderSurface_->setStyleSheet("background-color: #101318; color: #d8dee9; border: 1px solid #2c3440;");
    renderSurface_->setText("Embedded VNC renderer is idle");
    renderSurface_->setFocusPolicy(Qt::StrongFocus);
    renderSurface_->setMouseTracking(true);
    renderSurface_->installEventFilter(this);

    renderScrollArea_ = new QScrollArea(this);
    renderScrollArea_->setWidget(renderSurface_);
    renderScrollArea_->setWidgetResizable(false);
    renderScrollArea_->setAlignment(Qt::AlignCenter);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumHeight(140);

    root->addLayout(controls);
    root->addWidget(endpointLabel_);
    root->addWidget(renderScrollArea_, 1);
    root->addWidget(logView_);

    connect(connectButton_, &QPushButton::clicked, this, &VncSessionWidget::startConnection);
    connect(disconnectButton_, &QPushButton::clicked, this, &VncSessionWidget::stopConnection);
    connect(viewerButton_, &QPushButton::clicked, this, &VncSessionWidget::openExternalViewer);
    connect(fitButton_, &QToolButton::toggled, this, &VncSessionWidget::toggleFitToWindow);
    connect(zoomInButton_, &QToolButton::clicked, this, &VncSessionWidget::zoomIn);
    connect(zoomOutButton_, &QToolButton::clicked, this, &VncSessionWidget::zoomOut);
    connect(zoomResetButton_, &QToolButton::clicked, this, &VncSessionWidget::zoomReset);
    connect(refreshButton_, &QToolButton::clicked, this, &VncSessionWidget::requestRefresh);
    connect(reconnectButton_, &QToolButton::clicked, this, &VncSessionWidget::reconnect);
    connect(fullscreenButton_, &QToolButton::clicked, this, &VncSessionWidget::toggleFullscreen);
    connect(cadButton_, &QToolButton::clicked, this, &VncSessionWidget::sendCtrlAltDel);
    connect(viewOnlyButton_, &QToolButton::toggled, this, &VncSessionWidget::toggleViewOnly);

    setConnectedUi(false);
}

VncSessionWidget::~VncSessionWidget() {
    stopConnection();
}

void VncSessionWidget::log(const QString& line) {
    logView_->appendPlainText(line);
}

void VncSessionWidget::setConnectedUi(bool connected) {
    isConnected_ = connected;
    connectButton_->setEnabled(!connected);
    disconnectButton_->setEnabled(connected);
    viewerButton_->setEnabled(connected);
    refreshButton_->setEnabled(connected);
    reconnectButton_->setEnabled(connected);
    fullscreenButton_->setEnabled(connected);
    cadButton_->setEnabled(connected && !runtimeViewOnly_);
    viewOnlyButton_->setEnabled(true);
    zoomInButton_->setEnabled(!fitToWindow_);
    zoomOutButton_->setEnabled(!fitToWindow_);
    zoomResetButton_->setEnabled(!fitToWindow_);
}

void VncSessionWidget::startConnection() {
    if (vncThread_ != nullptr) {
        return;
    }

    QString targetHost = profile_.host;
    int targetPort = profile_.port;

    if (profile_.tunnel.enabled && !profile_.tunnel.jumpHost.isEmpty()) {
        localTunnelPort_ = static_cast<int>(QRandomGenerator::global()->bounded(59050, 59999));

        tunnelProcess_ = new QProcess(this);
        QStringList args;
        args << "-N"
             << "-L"
             << QString("%1:%2:%3").arg(localTunnelPort_).arg(profile_.host).arg(profile_.port)
             << "-p"
             << QString::number(profile_.tunnel.jumpPort)
             << QString("%1@%2").arg(profile_.tunnel.jumpUsername, profile_.tunnel.jumpHost);

        connect(tunnelProcess_, &QProcess::readyReadStandardError, this, [this]() {
            log(QString::fromLocal8Bit(tunnelProcess_->readAllStandardError()));
        });

        connect(tunnelProcess_, &QProcess::readyReadStandardOutput, this, [this]() {
            log(QString::fromLocal8Bit(tunnelProcess_->readAllStandardOutput()));
        });

        tunnelProcess_->start("ssh", args);
        if (!tunnelProcess_->waitForStarted(3000)) {
            log("Failed to start SSH tunnel for VNC.");
            tunnelProcess_->deleteLater();
            tunnelProcess_ = nullptr;
            localTunnelPort_ = -1;
            return;
        }

        endpointLabel_->setText(QString("Target via tunnel: 127.0.0.1:%1").arg(localTunnelPort_));
        log("SSH tunnel established for VNC session.");
        targetHost = "127.0.0.1";
        targetPort = localTunnelPort_;
    } else {
        log("Direct VNC mode selected.");
    }

    renderSurface_->setText("Connecting...");

    vncThread_ = new QThread(this);
    vncWorker_ = new VncClientWorker();
    vncWorker_->moveToThread(vncThread_);

    domain::VncSettings effectiveVncSettings = profile_.vnc;
    effectiveVncSettings.useRemoteCursor = false;

    connect(vncThread_, &QThread::started, vncWorker_, [this, targetHost, targetPort, effectiveVncSettings]() {
        const QString username = profile_.vnc.username.isEmpty() ? profile_.auth.username : profile_.vnc.username;
        vncWorker_->start(targetHost, targetPort, effectiveVncSettings, username, profile_.auth.password);
    });

    connect(vncWorker_, &VncClientWorker::status, this, [this](const QString& message) {
        log(message);
    });

    connect(vncWorker_, &VncClientWorker::failed, this, [this](const QString& message) {
        log(QString("VNC error: %1").arg(message));
    });

    connect(vncWorker_, &VncClientWorker::connectedChanged, this, [this](bool connected) {
        setConnectedUi(connected);
        if (!connected) {
            renderSurface_->setText("Disconnected");
        }
    });

    connect(vncWorker_, &VncClientWorker::frameReady, this, [this](const QImage& frame) {
        currentFrame_ = frame;
        if (!frameRenderClockStarted_) {
            frameRenderClock_.start();
            frameRenderClockStarted_ = true;
        }

        // Limit repaint churn to about 30 FPS to avoid excessive allocations on heavy VNC streams.
        if (frameRenderClock_.elapsed() < 33) {
            return;
        }
        frameRenderClock_.restart();
        updateRenderedFrame();
    });

    connect(vncWorker_, &VncClientWorker::remoteCursorChanged, this, [this](const QImage& cursor, int hotX, int hotY) {
        if (cursor.isNull()) {
            renderSurface_->unsetCursor();
            return;
        }

        renderSurface_->setCursor(QCursor(QPixmap::fromImage(cursor), hotX, hotY));
    });

    connect(vncWorker_, &VncClientWorker::finished, this, [this]() {
        if (vncThread_ != nullptr) {
            vncThread_->quit();
        }
    });

    connect(vncThread_, &QThread::finished, this, [this]() {
        if (vncWorker_ != nullptr) {
            vncWorker_->deleteLater();
            vncWorker_ = nullptr;
        }
        if (vncThread_ != nullptr) {
            vncThread_->deleteLater();
            vncThread_ = nullptr;
        }
        setConnectedUi(false);
    });

    vncThread_->start();
    setConnectedUi(true);
    renderSurface_->setCursor(buildDotCursor());
}

void VncSessionWidget::stopConnection() {
    if (vncWorker_ != nullptr) {
        vncWorker_->stop();
    }

    if (vncThread_ != nullptr) {
        vncThread_->quit();
        vncThread_->wait(1500);
    }

    if (tunnelProcess_ != nullptr) {
        tunnelProcess_->terminate();
        tunnelProcess_->waitForFinished(2000);
        tunnelProcess_->deleteLater();
        tunnelProcess_ = nullptr;
    }

    localTunnelPort_ = -1;
    endpointLabel_->setText(QString("Target: %1:%2").arg(profile_.host).arg(profile_.port));
    currentFrame_ = QImage();
    lastRenderedSize_ = QSize();
    frameRenderClockStarted_ = false;
    renderSurface_->setText("Embedded VNC renderer is idle");
    renderSurface_->setPixmap(QPixmap());
    renderSurface_->unsetCursor();
    setConnectedUi(false);
}

void VncSessionWidget::toggleFitToWindow(bool enabled) {
    fitToWindow_ = enabled;
    zoomInButton_->setEnabled(!fitToWindow_);
    zoomOutButton_->setEnabled(!fitToWindow_);
    zoomResetButton_->setEnabled(!fitToWindow_);
    updateRenderedFrame();
}

void VncSessionWidget::zoomIn() {
    if (fitToWindow_) {
        return;
    }
    zoomFactor_ = std::min(4.0, zoomFactor_ + 0.1);
    updateRenderedFrame();
}

void VncSessionWidget::zoomOut() {
    if (fitToWindow_) {
        return;
    }
    zoomFactor_ = std::max(0.2, zoomFactor_ - 0.1);
    updateRenderedFrame();
}

void VncSessionWidget::zoomReset() {
    fitToWindow_ = false;
    fitButton_->setChecked(false);
    zoomFactor_ = 1.0;
    updateRenderedFrame();
}

void VncSessionWidget::requestRefresh() {
    if (vncWorker_ == nullptr) {
        return;
    }
    vncWorker_->requestRefresh();
}

void VncSessionWidget::reconnect() {
    stopConnection();
    startConnection();
}

void VncSessionWidget::toggleFullscreen() {
    if (QWidget* top = window(); top != nullptr) {
        if (top->isFullScreen()) {
            top->showNormal();
        } else {
            top->showFullScreen();
        }
    }
}

void VncSessionWidget::toggleViewOnly(bool enabled) {
    runtimeViewOnly_ = enabled;
    cadButton_->setEnabled(isConnected_ && !runtimeViewOnly_);
}

bool VncSessionWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != renderSurface_ || !isConnected_ || vncWorker_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseMove) {
        if (runtimeViewOnly_) {
            return false;
        }

        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (event->type() == QEvent::MouseButtonPress) {
            renderSurface_->setFocus(Qt::MouseFocusReason);
        }
        const QPoint remote = mapToRemote(mouseEvent->position().toPoint());
        const int mask = buttonMaskFromMouseButtons(mouseEvent->buttons());
        vncWorker_->sendPointerEvent(remote.x(), remote.y(), mask);
        return false;
    }

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        if (runtimeViewOnly_) {
            return false;
        }

        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const quint32 keySym = keySymFromQtKey(keyEvent->key(), keyEvent->text());
        if (keySym != 0) {
            vncWorker_->sendKeyEvent(keySym, event->type() == QEvent::KeyPress);
        }
        return false;
    }

    return QWidget::eventFilter(watched, event);
}

void VncSessionWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (fitToWindow_) {
        updateRenderedFrame();
    }
}

void VncSessionWidget::updateRenderedFrame() {
    if (currentFrame_.isNull()) {
        return;
    }

    QSize targetSize;
    if (fitToWindow_) {
        const QSize viewport = renderScrollArea_->viewport()->size();
        targetSize = currentFrame_.size().scaled(viewport, Qt::KeepAspectRatio);
        zoomLabel_->setText("Fit");
    } else {
        targetSize = QSize(static_cast<int>(currentFrame_.width() * zoomFactor_),
                           static_cast<int>(currentFrame_.height() * zoomFactor_));
        zoomLabel_->setText(QString("%1%").arg(static_cast<int>(zoomFactor_ * 100.0)));
    }

    targetSize = targetSize.expandedTo(QSize(1, 1));
    lastRenderedSize_ = targetSize;

    const QPixmap pixmap = QPixmap::fromImage(
        currentFrame_.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    renderSurface_->setPixmap(pixmap);
    renderSurface_->resize(targetSize);
}

int VncSessionWidget::buttonMaskFromMouseButtons(Qt::MouseButtons buttons) const {
    int mask = 0;
    if (buttons.testFlag(Qt::LeftButton)) {
        mask |= 1;
    }
    if (buttons.testFlag(Qt::MiddleButton)) {
        mask |= 2;
    }
    if (buttons.testFlag(Qt::RightButton)) {
        mask |= 4;
    }
    return mask;
}

QPoint VncSessionWidget::mapToRemote(const QPoint& pos) const {
    if (currentFrame_.isNull() || lastRenderedSize_.isEmpty()) {
        return QPoint();
    }

    const int x = std::clamp(pos.x(), 0, lastRenderedSize_.width() - 1);
    const int y = std::clamp(pos.y(), 0, lastRenderedSize_.height() - 1);

    const int rx = (x * currentFrame_.width()) / std::max(1, lastRenderedSize_.width());
    const int ry = (y * currentFrame_.height()) / std::max(1, lastRenderedSize_.height());
    return QPoint(rx, ry);
}

quint32 VncSessionWidget::keySymFromQtKey(int key, const QString& text) const {
    switch (key) {
    case Qt::Key_Backspace:
        return kKeySymBackSpace;
    case Qt::Key_Tab:
        return kKeySymTab;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return kKeySymReturn;
    case Qt::Key_Escape:
        return kKeySymEscape;
    case Qt::Key_Home:
        return kKeySymHome;
    case Qt::Key_Left:
        return kKeySymLeft;
    case Qt::Key_Up:
        return kKeySymUp;
    case Qt::Key_Right:
        return kKeySymRight;
    case Qt::Key_Down:
        return kKeySymDown;
    case Qt::Key_PageUp:
        return kKeySymPageUp;
    case Qt::Key_PageDown:
        return kKeySymPageDown;
    case Qt::Key_End:
        return kKeySymEnd;
    case Qt::Key_Insert:
        return kKeySymInsert;
    case Qt::Key_Delete:
        return kKeySymDelete;
    case Qt::Key_Shift:
        return kKeySymShiftL;
    case Qt::Key_Control:
        return kKeySymControlL;
    case Qt::Key_Alt:
        return kKeySymAltL;
    default:
        break;
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return kKeySymF1 + static_cast<quint32>(key - Qt::Key_F1);
    }

    if (!text.isEmpty()) {
        return static_cast<quint32>(text.at(0).unicode());
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<quint32>('a' + (key - Qt::Key_A));
    }

    return 0;
}

void VncSessionWidget::sendCtrlAltDel() {
    if (vncWorker_ == nullptr || runtimeViewOnly_) {
        return;
    }

    vncWorker_->sendKeyEvent(kKeySymControlL, true);
    vncWorker_->sendKeyEvent(kKeySymAltL, true);
    vncWorker_->sendKeyEvent(kKeySymDelete, true);
    vncWorker_->sendKeyEvent(kKeySymDelete, false);
    vncWorker_->sendKeyEvent(kKeySymAltL, false);
    vncWorker_->sendKeyEvent(kKeySymControlL, false);
}

void VncSessionWidget::openExternalViewer() {
    const QString host = localTunnelPort_ > 0 ? "127.0.0.1" : profile_.host;
    const int port = localTunnelPort_ > 0 ? localTunnelPort_ : profile_.port;

    const QUrl url(QString("vnc://%1:%2").arg(host).arg(port));
    const bool ok = QDesktopServices::openUrl(url);
    if (!ok) {
        log(QString("No system handler for %1").arg(url.toString()));
    }
}
