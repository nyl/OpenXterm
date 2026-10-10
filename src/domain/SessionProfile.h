#pragma once

#include <QString>

#include <optional>

namespace domain {

enum class SessionKind {
    Ssh,
    Vnc,
};

struct SessionAuthSettings {
    QString username;
    std::optional<QString> password;
    std::optional<QString> privateKeyPath;
};

struct SessionTunnelSettings {
    bool enabled = false;
    QString jumpHost;
    int jumpPort = 22;
    QString jumpUsername;
};

struct VncSettings {
    QString username;
    bool viewOnly = false;
    bool shareDesktop = true;
    bool enableJpeg = true;
    bool useRemoteCursor = false;
    int qualityPreset = 2;
    bool useQualityPreset = true;
    int requestedDepth = 24;
    int qualityLevel = 6;
    int compressLevel = 2;
    QString encodings = "tight zrle hextile copyrect raw";
    QString repeater;
    int connectTimeoutSeconds = 15;
    bool disableEncryption = false;
    bool disableClipboard = false;
    bool dynamicResolution = false;
    double aspectRatioLock = 0.0;
};

struct SessionProfile {
    QString id;
    QString name;
    QString folderPath = "User sessions";
    SessionKind kind = SessionKind::Ssh;
    QString host;
    int port = 22;
    SessionAuthSettings auth;
    SessionTunnelSettings tunnel;
    VncSettings vnc;
    bool favorite = false;
};

}  // namespace domain
