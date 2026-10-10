#pragma once

#include <QImage>
#include <QElapsedTimer>
#include <QWidget>

#include "../domain/SessionProfile.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QProcess;
class QScrollArea;
class QThread;
class QToolButton;

class VncClientWorker;

class VncSessionWidget : public QWidget {
    Q_OBJECT

public:
    explicit VncSessionWidget(const domain::SessionProfile& profile, QWidget* parent = nullptr);
    ~VncSessionWidget() override;

private slots:
    void startConnection();
    void stopConnection();
    void openExternalViewer();
    void toggleFitToWindow(bool enabled);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void requestRefresh();
    void reconnect();
    void toggleFullscreen();
    void toggleViewOnly(bool enabled);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void log(const QString& line);
    void setConnectedUi(bool connected);
    void updateRenderedFrame();
    int buttonMaskFromMouseButtons(Qt::MouseButtons buttons) const;
    QPoint mapToRemote(const QPoint& pos) const;
    quint32 keySymFromQtKey(int key, const QString& text) const;
    void sendCtrlAltDel();

    domain::SessionProfile profile_;

    QLabel* endpointLabel_ = nullptr;
    QLabel* renderSurface_ = nullptr;
    QScrollArea* renderScrollArea_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* viewerButton_ = nullptr;
    QToolButton* fitButton_ = nullptr;
    QToolButton* zoomInButton_ = nullptr;
    QToolButton* zoomOutButton_ = nullptr;
    QToolButton* zoomResetButton_ = nullptr;
    QToolButton* refreshButton_ = nullptr;
    QToolButton* reconnectButton_ = nullptr;
    QToolButton* fullscreenButton_ = nullptr;
    QToolButton* cadButton_ = nullptr;
    QToolButton* viewOnlyButton_ = nullptr;
    QLabel* zoomLabel_ = nullptr;

    QProcess* tunnelProcess_ = nullptr;
    int localTunnelPort_ = -1;

    QThread* vncThread_ = nullptr;
    VncClientWorker* vncWorker_ = nullptr;
    QImage currentFrame_;
    double zoomFactor_ = 1.0;
    bool fitToWindow_ = true;
    bool isConnected_ = false;
    bool runtimeViewOnly_ = false;
    QSize lastRenderedSize_;
    QElapsedTimer frameRenderClock_;
    bool frameRenderClockStarted_ = false;
};
