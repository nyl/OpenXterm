#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <optional>

#include <rfb/rfbclient.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace domain {
struct VncSettings;
}

class VncClientWorker : public QObject {
    Q_OBJECT

public:
    explicit VncClientWorker(QObject* parent = nullptr);
    ~VncClientWorker() override;

public slots:
    void start(const QString& host,
               int port,
               const domain::VncSettings& settings,
               const QString& username,
               const std::optional<QString>& password);
    void stop();
    void requestRefresh();
    void sendPointerEvent(int x, int y, int buttonMask);
    void sendKeyEvent(quint32 keySym, bool pressed);
    void handleLibraryLog(const QString& line, bool isError);

signals:
    void frameReady(const QImage& frame);
    void remoteCursorChanged(const QImage& cursor, int hotX, int hotY);
    void status(const QString& message);
    void connectedChanged(bool connected);
    void failed(const QString& message);
    void finished();

private:
    struct PendingPointerEvent {
        int x;
        int y;
        int buttonMask;
    };

    struct PendingKeyEvent {
        quint32 keySym;
        bool pressed;
    };

    static constexpr const char* kClientDataTag = "OpenXtermQt.VncClientWorker";

    static rfbBool onMallocFrameBuffer(rfbClient* client);
    static void onFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h);
    static void onCursorShape(rfbClient* client, int xHot, int yHot, int width, int height, int bytesPerPixel);
    static char* onGetPassword(rfbClient* client);
    static rfbCredential* onGetCredential(rfbClient* client, int credentialType);

    static rfbClient* createClientForDepth(int requestedDepth);
    static void applyQualityProfile(rfbClient* client, const domain::VncSettings& settings);
    static void applyDepthProfile(rfbClient* client, int requestedDepth);
    void configureClient(rfbClient* client, const domain::VncSettings& settings);
    bool initializeClientOrFallback(const QString& host, int port, const domain::VncSettings& settings);
    bool convertRawRectToRgb32(int x, int y, int w, int h);
    static uint8_t toEightBit(uint32_t value, uint32_t maxValue);
    void processPendingClientEvents();

    void emitCurrentFrame();

    rfbClient* client_ = nullptr;
    std::vector<uint8_t> rawFrameBuffer_;
    std::vector<uint8_t> displayFrameBuffer_;
    int width_ = 0;
    int height_ = 0;
    int rawBytesPerPixel_ = 4;
    bool useRemoteCursor_ = false;
    QString username_;
    QString lastLibraryError_;
    std::optional<QString> password_;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<bool> refreshRequested_ = false;
    std::mutex pendingEventsMutex_;
    std::vector<PendingPointerEvent> pendingPointerEvents_;
    std::vector<PendingKeyEvent> pendingKeyEvents_;
};
