#include "VncClientWorker.h"

#include "../domain/SessionProfile.h"

#include <QByteArray>
#include <QSysInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kNoEncryptAuthTypes[] = {rfbNoAuth, rfbVncAuth, rfbMSLogon, 0};
thread_local VncClientWorker* tActiveWorker = nullptr;

char* duplicateEncoding(const char* value) {
    if (value == nullptr) {
        return nullptr;
    }
    return ::strdup(value);
}

void relayClientLog(const bool isError, const char* format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    const QString line = QString::fromLocal8Bit(buffer).trimmed();
    if (line.isEmpty()) {
        return;
    }

    if (VncClientWorker* worker = tActiveWorker; worker != nullptr) {
        worker->handleLibraryLog(line, isError);
    }
}

void clientLogCallback(const char* format, ...) {
    va_list args;
    va_start(args, format);
    relayClientLog(false, format, args);
    va_end(args);
}

void clientErrCallback(const char* format, ...) {
    va_list args;
    va_start(args, format);
    relayClientLog(true, format, args);
    va_end(args);
}

}  // namespace

VncClientWorker::VncClientWorker(QObject* parent) : QObject(parent) {}

VncClientWorker::~VncClientWorker() {
    stop();
}

void VncClientWorker::start(const QString& host,
                            int port,
                            const domain::VncSettings& settings,
                            const QString& username,
                            const std::optional<QString>& password) {
    stopRequested_ = false;
    refreshRequested_.store(false);
    username_ = username;
    password_ = password;
    lastLibraryError_.clear();

    tActiveWorker = this;
    const auto previousClientLog = rfbClientLog;
    const auto previousClientErr = rfbClientErr;
    rfbClientLog = &clientLogCallback;
    rfbClientErr = &clientErrCallback;

    emit status(QString("Connecting VNC to %1:%2").arg(host).arg(port));

    if (!initializeClientOrFallback(host, port, settings)) {
        const QString reason = lastLibraryError_.isEmpty() ? "Unknown error from libvncclient" : lastLibraryError_;
        emit failed(QString("Unable to initialize VNC client connection (%1)").arg(reason));
        if (client_ != nullptr) {
            rfbClientCleanup(client_);
            client_ = nullptr;
        }
        rfbClientLog = previousClientLog;
        rfbClientErr = previousClientErr;
        tActiveWorker = nullptr;
        emit finished();
        return;
    }

    emit connectedChanged(true);
    emit status("VNC connection established");

    if (width_ > 0 && height_ > 0) {
        SendFramebufferUpdateRequest(client_, 0, 0, width_, height_, FALSE);
        emit status("Requested full framebuffer refresh");
    }

    while (!stopRequested_) {
        processPendingClientEvents();

        if (client_ == nullptr || client_->sock <= 0) {
            emit status("VNC socket is no longer valid");
            break;
        }

        const int waitResult = WaitForMessage(client_, 100);
        if (waitResult < 0) {
            emit status("VNC connection closed by server");
            break;
        }

        if (waitResult == 0) {
            continue;
        }

        if (!HandleRFBServerMessage(client_)) {
            emit status("VNC message loop ended");
            break;
        }
    }

    emit connectedChanged(false);

    if (client_ != nullptr) {
        rfbClientCleanup(client_);
        client_ = nullptr;
    }

    rfbClientLog = previousClientLog;
    rfbClientErr = previousClientErr;
    tActiveWorker = nullptr;

    emit finished();
}

void VncClientWorker::stop() {
    stopRequested_ = true;
}

void VncClientWorker::requestRefresh() {
    refreshRequested_.store(true);
}

void VncClientWorker::sendPointerEvent(int x, int y, int buttonMask) {
    std::lock_guard<std::mutex> lock(pendingEventsMutex_);
    pendingPointerEvents_.push_back(PendingPointerEvent{x, y, buttonMask});
}

void VncClientWorker::sendKeyEvent(quint32 keySym, bool pressed) {
    std::lock_guard<std::mutex> lock(pendingEventsMutex_);
    pendingKeyEvents_.push_back(PendingKeyEvent{keySym, pressed});
}

void VncClientWorker::processPendingClientEvents() {
    if (client_ == nullptr) {
        return;
    }

    std::vector<PendingPointerEvent> pointerEvents;
    std::vector<PendingKeyEvent> keyEvents;
    {
        std::lock_guard<std::mutex> lock(pendingEventsMutex_);
        if (!pendingPointerEvents_.empty()) {
            pointerEvents.swap(pendingPointerEvents_);
        }
        if (!pendingKeyEvents_.empty()) {
            keyEvents.swap(pendingKeyEvents_);
        }
    }

    for (const PendingPointerEvent& event : pointerEvents) {
        SendPointerEvent(client_, event.x, event.y, event.buttonMask);
    }

    for (const PendingKeyEvent& event : keyEvents) {
        SendKeyEvent(client_, event.keySym, event.pressed ? TRUE : FALSE);
    }

    if (refreshRequested_.exchange(false) && width_ > 0 && height_ > 0) {
        SendFramebufferUpdateRequest(client_, 0, 0, width_, height_, FALSE);
    }
}

rfbBool VncClientWorker::onMallocFrameBuffer(rfbClient* client) {
    if (client == nullptr) {
        return FALSE;
    }

    auto* self = static_cast<VncClientWorker*>(
        rfbClientGetClientData(client, const_cast<char*>(kClientDataTag)));
    if (self == nullptr) {
        if (VncClientWorker* worker = tActiveWorker; worker != nullptr) {
            worker->handleLibraryLog("MallocFrameBuffer callback missing worker context", true);
        }
        return FALSE;
    }
    self->width_ = client->width;
    self->height_ = client->height;
    self->rawBytesPerPixel_ = std::max(1, client->format.bitsPerPixel / 8);

    if (self->width_ <= 0 || self->height_ <= 0) {
        return FALSE;
    }

    const size_t pixels = static_cast<size_t>(self->width_) * static_cast<size_t>(self->height_);
    const size_t rawBytes = pixels * static_cast<size_t>(self->rawBytesPerPixel_);
    const size_t rgbBytes = pixels * static_cast<size_t>(4);

    self->rawFrameBuffer_.assign(rawBytes, 0);
    self->displayFrameBuffer_.assign(rgbBytes, 0);
    if (self->rawFrameBuffer_.empty() || self->displayFrameBuffer_.empty()) {
        return FALSE;
    }

    client->frameBuffer = self->rawFrameBuffer_.data();

    self->emit status(QString("Framebuffer allocated: %1x%2").arg(self->width_).arg(self->height_));
    return TRUE;
}

void VncClientWorker::onFrameBufferUpdate(rfbClient* client, int x, int y, int w, int h) {
    if (client == nullptr) {
        return;
    }

    auto* self = static_cast<VncClientWorker*>(
        rfbClientGetClientData(client, const_cast<char*>(kClientDataTag)));
    if (self == nullptr) {
        return;
    }

    if (!self->convertRawRectToRgb32(x, y, w, h)) {
        self->handleLibraryLog("Failed to convert VNC framebuffer update", true);
        return;
    }

    self->emitCurrentFrame();
}

void VncClientWorker::onCursorShape(rfbClient* client,
                                    int xHot,
                                    int yHot,
                                    int width,
                                    int height,
                                    int bytesPerPixel) {
    if (client == nullptr) {
        return;
    }

    auto* self = static_cast<VncClientWorker*>(
        rfbClientGetClientData(client, const_cast<char*>(kClientDataTag)));
    if (self == nullptr) {
        return;
    }

    if (!self->useRemoteCursor_) {
        return;
    }

    if (width <= 0 || height <= 0 || bytesPerPixel <= 0 || client->rcSource == nullptr || client->rcMask == nullptr) {
        emit self->remoteCursorChanged(QImage(), 0, 0);
        return;
    }

    QImage cursorImage(width, height, QImage::Format_ARGB32);
    cursorImage.fill(Qt::transparent);

    const uint32_t redMax = static_cast<uint32_t>(client->format.redMax);
    const uint32_t greenMax = static_cast<uint32_t>(client->format.greenMax);
    const uint32_t blueMax = static_cast<uint32_t>(client->format.blueMax);
    const int redShift = static_cast<int>(client->format.redShift);
    const int greenShift = static_cast<int>(client->format.greenShift);
    const int blueShift = static_cast<int>(client->format.blueShift);

    const int sourceStride = width * bytesPerPixel;
    const int maskStride = (width + 7) / 8;
    const auto* source = reinterpret_cast<const uint8_t*>(client->rcSource);
    const auto* mask = reinterpret_cast<const uint8_t*>(client->rcMask);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int maskByteIndex = y * maskStride + (x / 8);
            const uint8_t maskBit = static_cast<uint8_t>(1U << (7 - (x % 8)));
            const bool visible = (mask[maskByteIndex] & maskBit) != 0;
            if (!visible) {
                cursorImage.setPixelColor(x, y, QColor(0, 0, 0, 0));
                continue;
            }

            const int sourceOffset = y * sourceStride + x * bytesPerPixel;
            uint32_t sourcePixel = 0;
            for (int b = 0; b < bytesPerPixel; ++b) {
                sourcePixel |= static_cast<uint32_t>(source[sourceOffset + b]) << (8 * b);
            }

            const uint8_t r = toEightBit((sourcePixel >> redShift) & redMax, redMax);
            const uint8_t g = toEightBit((sourcePixel >> greenShift) & greenMax, greenMax);
            const uint8_t b = toEightBit((sourcePixel >> blueShift) & blueMax, blueMax);
            cursorImage.setPixelColor(x, y, QColor(r, g, b, 255));
        }
    }

    emit self->remoteCursorChanged(cursorImage, xHot, yHot);
}

char* VncClientWorker::onGetPassword(rfbClient* client) {
    if (client == nullptr) {
        return nullptr;
    }

    auto* self = static_cast<VncClientWorker*>(
        rfbClientGetClientData(client, const_cast<char*>(kClientDataTag)));
    if (self == nullptr || !self->password_.has_value()) {
        return nullptr;
    }

    QByteArray password = self->password_->toLocal8Bit();
    return ::strdup(password.constData());
}

rfbCredential* VncClientWorker::onGetCredential(rfbClient* client, int credentialType) {
    if (client == nullptr) {
        return nullptr;
    }

    auto* self = static_cast<VncClientWorker*>(
        rfbClientGetClientData(client, const_cast<char*>(kClientDataTag)));
    if (self == nullptr) {
        return nullptr;
    }

    if (credentialType != rfbCredentialTypeUser) {
        return nullptr;
    }

    auto* credential = static_cast<rfbCredential*>(std::calloc(1, sizeof(rfbCredential)));
    if (credential == nullptr) {
        return nullptr;
    }

    const QByteArray usernameBytes = self->username_.toLocal8Bit();
    credential->userCredential.username = ::strdup(usernameBytes.constData());

    if (self->password_.has_value()) {
        const QByteArray passwordBytes = self->password_->toLocal8Bit();
        credential->userCredential.password = ::strdup(passwordBytes.constData());
    }

    return credential;
}

rfbClient* VncClientWorker::createClientForDepth(int requestedDepth) {
    switch (requestedDepth) {
    case 8:
        return rfbGetClient(2, 3, 1);
    case 15:
    case 16:
        return rfbGetClient(5, 3, 2);
    case 24:
        return rfbGetClient(6, 3, 3);
    case 32:
    default:
        return rfbGetClient(8, 3, 4);
    }
}

void VncClientWorker::applyQualityProfile(rfbClient* client, const domain::VncSettings& settings) {
    if (client == nullptr) {
        return;
    }

    client->appData.encodingsString = nullptr;

    if (settings.useQualityPreset) {
        switch (settings.qualityPreset) {
        case 9:
            client->appData.useBGR233 = 0;
            client->appData.compressLevel = 1;
            client->appData.qualityLevel = 9;
            client->appData.encodingsString = duplicateEncoding("copyrect zlib hextile raw");
            break;
        case 1:
            client->appData.useBGR233 = 0;
            client->appData.compressLevel = 3;
            client->appData.qualityLevel = 5;
            client->appData.encodingsString = duplicateEncoding("tight zrle ultra copyrect hextile zlib corre rre raw");
            break;
        case 2:
            client->appData.useBGR233 = 0;
            client->appData.compressLevel = 2;
            client->appData.qualityLevel = 7;
            client->appData.encodingsString = duplicateEncoding("tight zrle ultra copyrect hextile zlib corre rre raw");
            break;
        case 0:
        default:
            client->appData.useBGR233 = 1;
            client->appData.compressLevel = 3;
            client->appData.qualityLevel = 1;
            client->appData.encodingsString = duplicateEncoding("copyrect zrle ultra zlib hextile corre rre raw");
            break;
        }
    }

    if (!settings.useQualityPreset) {
        client->appData.compressLevel = settings.compressLevel;
        client->appData.qualityLevel = settings.qualityLevel;
    }

    const QByteArray encodings = settings.encodings.toLocal8Bit();
    if (!encodings.isEmpty()) {
        client->appData.encodingsString = ::strdup(encodings.constData());
    }
}

void VncClientWorker::applyDepthProfile(rfbClient* client, int requestedDepth) {
    if (client == nullptr) {
        return;
    }

    client->appData.requestedDepth = requestedDepth;
    client->format.depth = requestedDepth;
    client->format.trueColour = true;
    client->format.bigEndian = (QSysInfo::ByteOrder == QSysInfo::BigEndian) ? TRUE : FALSE;

    switch (requestedDepth) {
    case 8:
        client->format.depth = 8;
        client->format.bitsPerPixel = 8;
        client->format.redShift = 0;
        client->format.greenShift = 3;
        client->format.blueShift = 6;
        client->format.redMax = 7;
        client->format.greenMax = 7;
        client->format.blueMax = 3;
        break;
    case 16:
    case 15:
        client->format.depth = 15;
        client->format.bitsPerPixel = 16;
        client->format.redShift = 11;
        client->format.greenShift = 6;
        client->format.blueShift = 1;
        client->format.redMax = 31;
        client->format.greenMax = 31;
        client->format.blueMax = 31;
        break;
    case 32:
    case 24:
    default:
        client->format.depth = 24;
        client->format.bitsPerPixel = 32;
        client->format.blueShift = 0;
        client->format.greenShift = 8;
        client->format.redShift = 16;
        client->format.blueMax = 0xff;
        client->format.greenMax = 0xff;
        client->format.redMax = 0xff;
        break;
    }
}

void VncClientWorker::configureClient(rfbClient* client, const domain::VncSettings& settings) {
    if (client == nullptr) {
        return;
    }

    rfbClientSetClientData(client, const_cast<char*>(kClientDataTag), this);
    client->MallocFrameBuffer = &VncClientWorker::onMallocFrameBuffer;
    client->GotFrameBufferUpdate = &VncClientWorker::onFrameBufferUpdate;
    client->GotCursorShape = &VncClientWorker::onCursorShape;
    client->GetPassword = &VncClientWorker::onGetPassword;
    client->GetCredential = &VncClientWorker::onGetCredential;

    client->appData.shareDesktop = settings.shareDesktop ? TRUE : FALSE;
    client->appData.viewOnly = settings.viewOnly ? TRUE : FALSE;
    client->appData.enableJPEG = settings.enableJpeg ? TRUE : FALSE;
    client->appData.useRemoteCursor = settings.useRemoteCursor ? TRUE : FALSE;
    useRemoteCursor_ = settings.useRemoteCursor;

    applyQualityProfile(client, settings);
    const int effectiveDepth = settings.requestedDepth == 32 ? 24 : settings.requestedDepth;
    applyDepthProfile(client, effectiveDepth);
}

bool VncClientWorker::initializeClientOrFallback(const QString& host, int port, const domain::VncSettings& settings) {
    const QString repeaterTarget = settings.repeater.trimmed();
    QString connectHost = host;
    int connectPort = port;

    client_ = createClientForDepth(settings.requestedDepth);
    if (client_ == nullptr) {
        lastLibraryError_ = "Failed to allocate VNC client";
        return false;
    }

    configureClient(client_, settings);

    if (!repeaterTarget.isEmpty()) {
        const QStringList parts = repeaterTarget.split(':', Qt::SkipEmptyParts);
        bool repeaterPortOk = false;
        if (parts.size() == 2) {
            const int parsedPort = parts.at(1).toInt(&repeaterPortOk);
            if (repeaterPortOk && parsedPort > 0 && parsedPort <= 65535) {
                connectHost = parts.at(0).trimmed();
                connectPort = parsedPort;

                QByteArray destinationHost = host.toLocal8Bit();
                client_->destHost = ::strdup(destinationHost.constData());
                client_->destPort = port;
                emit status(QString("VNC repeater enabled via %1:%2").arg(connectHost).arg(connectPort));
            } else {
                emit status("Invalid repeater port, falling back to direct VNC target.");
            }
        } else {
            emit status("Invalid repeater format. Use host:port.");
        }
    }

    QByteArray hostBytes = connectHost.toLocal8Bit();
    client_->serverHost = ::strdup(hostBytes.constData());
    client_->serverPort = connectPort;

    if (settings.disableEncryption) {
        SetClientAuthSchemes(client_, kNoEncryptAuthTypes, -1);
    }

    const auto initSucceededOrRecoverable = [this](const QString& phase, const int initResult) {
        const int sock = (client_ != nullptr) ? client_->sock : -1;
        const bool socketOk = client_ != nullptr && client_->sock > 0;
        const bool geometryOk = width_ > 0 && height_ > 0;
        const bool frameBufferPtrOk = client_ != nullptr && client_->frameBuffer != nullptr;
        const bool frameBuffersOk = !rawFrameBuffer_.empty() && !displayFrameBuffer_.empty();

        emit status(QString("%1 init result=%2, sock=%3, width=%4, height=%5, framebuffer=%6")
                        .arg(phase)
                        .arg(initResult)
                        .arg(sock)
                        .arg(width_)
                        .arg(height_)
                        .arg(frameBufferPtrOk ? "yes" : "no"));

        if (initResult >= 0) {
            return true;
        }

        if (socketOk && geometryOk && frameBufferPtrOk && frameBuffersOk) {
            emit status(QString("%1 init returned %2 but session state is usable; continuing.")
                            .arg(phase)
                            .arg(initResult));
            return true;
        }

        return false;
    };

    emit status(QString("VNC connect timeout configured: %1s").arg(settings.connectTimeoutSeconds));
    emit status("Starting primary VNC init handshake...");

    const int primaryInitResult = rfbInitClient(client_, nullptr, nullptr);
    if (initSucceededOrRecoverable("Primary", primaryInitResult)) {
        SetFormatAndEncodings(client_);
        return true;
    }

    const QString firstFailureReason = lastLibraryError_.isEmpty() ? "Unknown error from libvncclient" : lastLibraryError_;
    emit status(QString("Primary init failed, retrying with compatibility profile (%1)").arg(firstFailureReason));

    rfbClientCleanup(client_);
    client_ = nullptr;

    lastLibraryError_.clear();
    client_ = createClientForDepth(24);
    if (client_ == nullptr) {
        lastLibraryError_ = "Fallback allocation failed";
        return false;
    }

    domain::VncSettings fallbackSettings = settings;
    fallbackSettings.useQualityPreset = true;
    fallbackSettings.qualityPreset = 9;
    fallbackSettings.requestedDepth = 24;
    fallbackSettings.encodings = "copyrect zlib hextile raw";

    configureClient(client_, fallbackSettings);

    QByteArray retryHostBytes = connectHost.toLocal8Bit();
    client_->serverHost = ::strdup(retryHostBytes.constData());
    client_->serverPort = connectPort;

    if (settings.disableEncryption) {
        SetClientAuthSchemes(client_, kNoEncryptAuthTypes, -1);
    }

    emit status("Starting fallback VNC init handshake...");
    const int fallbackInitResult = rfbInitClient(client_, nullptr, nullptr);
    if (!initSucceededOrRecoverable("Fallback", fallbackInitResult)) {
        return false;
    }

    SetFormatAndEncodings(client_);
    emit status("VNC initialized with compatibility fallback profile.");
    return true;
}

uint8_t VncClientWorker::toEightBit(const uint32_t value, const uint32_t maxValue) {
    if (maxValue == 0) {
        return 0;
    }
    if (maxValue >= 255) {
        return static_cast<uint8_t>(std::min<uint32_t>(value, 255));
    }
    return static_cast<uint8_t>((value * 255u) / maxValue);
}

bool VncClientWorker::convertRawRectToRgb32(int x, int y, int w, int h) {
    if (client_ == nullptr || width_ <= 0 || height_ <= 0 || rawBytesPerPixel_ <= 0) {
        return false;
    }
    if (rawFrameBuffer_.empty() || displayFrameBuffer_.empty()) {
        return false;
    }

    const int left = std::clamp(x, 0, width_ - 1);
    const int top = std::clamp(y, 0, height_ - 1);
    const int right = std::clamp(x + std::max(1, w), 0, width_);
    const int bottom = std::clamp(y + std::max(1, h), 0, height_);
    if (right <= left || bottom <= top) {
        return false;
    }

    const uint32_t redMax = static_cast<uint32_t>(client_->format.redMax);
    const uint32_t greenMax = static_cast<uint32_t>(client_->format.greenMax);
    const uint32_t blueMax = static_cast<uint32_t>(client_->format.blueMax);
    const int redShift = static_cast<int>(client_->format.redShift);
    const int greenShift = static_cast<int>(client_->format.greenShift);
    const int blueShift = static_cast<int>(client_->format.blueShift);

    auto* rgb32 = reinterpret_cast<uint32_t*>(displayFrameBuffer_.data());
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            const size_t pixelIndex = static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px);
            const size_t rawOffset = pixelIndex * static_cast<size_t>(rawBytesPerPixel_);
            if (rawOffset + static_cast<size_t>(rawBytesPerPixel_) > rawFrameBuffer_.size()) {
                return false;
            }

            uint32_t sourcePixel = 0;
            for (int b = 0; b < rawBytesPerPixel_; ++b) {
                sourcePixel |= static_cast<uint32_t>(rawFrameBuffer_[rawOffset + static_cast<size_t>(b)]) << (8 * b);
            }

            const uint8_t r = toEightBit((sourcePixel >> redShift) & redMax, redMax);
            const uint8_t g = toEightBit((sourcePixel >> greenShift) & greenMax, greenMax);
            const uint8_t b = toEightBit((sourcePixel >> blueShift) & blueMax, blueMax);
            rgb32[pixelIndex] = qRgb(r, g, b);
        }
    }

    return true;
}

void VncClientWorker::handleLibraryLog(const QString& line, bool isError) {
    if (isError) {
        lastLibraryError_ = line;
        emit status(QString("libvncclient: %1").arg(line));
    } else {
        emit status(QString("libvncclient: %1").arg(line));
    }
}

void VncClientWorker::emitCurrentFrame() {
    if (width_ <= 0 || height_ <= 0 || displayFrameBuffer_.empty()) {
        return;
    }

    const QImage frame(displayFrameBuffer_.data(), width_, height_, QImage::Format_RGB32);
    emit frameReady(frame.copy());
}
