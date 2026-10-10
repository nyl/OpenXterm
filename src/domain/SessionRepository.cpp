#include "SessionRepository.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace domain {

namespace {

QString sanitizePathSegment(const QString& value) {
    QString sanitized = value.trimmed();
    sanitized.replace('\\', '_');
    sanitized.replace('/', '_');
    sanitized.replace(':', '_');
    sanitized.replace('*', '_');
    sanitized.replace('?', '_');
    sanitized.replace('"', '_');
    sanitized.replace('<', '_');
    sanitized.replace('>', '_');
    sanitized.replace('|', '_');
    sanitized = sanitized.simplified();

    if (sanitized == "." || sanitized == "..") {
        return QString();
    }

    return sanitized;
}

QString sanitizeFileStem(const QString& value) {
    QString stem = sanitizePathSegment(value);
    stem.replace(' ', '_');
    if (stem.isEmpty()) {
        stem = QString("session-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    return stem;
}

QString normalizeFolderPath(const QString& folderPath) {
    QStringList parts = folderPath.split('/', Qt::SkipEmptyParts);
    QStringList normalizedParts;
    normalizedParts << "User sessions";

    for (const QString& rawPart : parts) {
        if (rawPart == "User sessions" && normalizedParts.size() == 1) {
            continue;
        }

        const QString part = sanitizePathSegment(rawPart);
        if (part.isEmpty()) {
            continue;
        }
        normalizedParts << part;
    }

    return normalizedParts.join('/');
}

bool pathWithinRoot(const QString& rootPath, const QString& absolutePath) {
    const QString relative = QDir(rootPath).relativeFilePath(absolutePath);
    if (relative == ".") {
        return true;
    }

    return !relative.startsWith("../") && relative != ".." && !QDir::isAbsolutePath(relative);
}

QString kindToString(const SessionKind kind) {
    return kind == SessionKind::Vnc ? "vnc" : "ssh";
}

SessionKind stringToKind(const QString& value) {
    return value.compare("vnc", Qt::CaseInsensitive) == 0 ? SessionKind::Vnc : SessionKind::Ssh;
}

QJsonObject profileToJson(const SessionProfile& profile) {
    QJsonObject auth;
    auth.insert("username", profile.auth.username);
    if (profile.auth.password.has_value()) {
        auth.insert("password", profile.auth.password.value());
    }
    if (profile.auth.privateKeyPath.has_value()) {
        auth.insert("privateKeyPath", profile.auth.privateKeyPath.value());
    }

    QJsonObject tunnel;
    tunnel.insert("enabled", profile.tunnel.enabled);
    tunnel.insert("jumpHost", profile.tunnel.jumpHost);
    tunnel.insert("jumpPort", profile.tunnel.jumpPort);
    tunnel.insert("jumpUsername", profile.tunnel.jumpUsername);

    QJsonObject vnc;
    vnc.insert("username", profile.vnc.username);
    vnc.insert("viewOnly", profile.vnc.viewOnly);
    vnc.insert("shareDesktop", profile.vnc.shareDesktop);
    vnc.insert("enableJpeg", profile.vnc.enableJpeg);
    vnc.insert("useRemoteCursor", profile.vnc.useRemoteCursor);
    vnc.insert("qualityPreset", profile.vnc.qualityPreset);
    vnc.insert("useQualityPreset", profile.vnc.useQualityPreset);
    vnc.insert("requestedDepth", profile.vnc.requestedDepth);
    vnc.insert("qualityLevel", profile.vnc.qualityLevel);
    vnc.insert("compressLevel", profile.vnc.compressLevel);
    vnc.insert("encodings", profile.vnc.encodings);
    vnc.insert("repeater", profile.vnc.repeater);
    vnc.insert("connectTimeoutSeconds", profile.vnc.connectTimeoutSeconds);
    vnc.insert("disableEncryption", profile.vnc.disableEncryption);
    vnc.insert("disableClipboard", profile.vnc.disableClipboard);
    vnc.insert("dynamicResolution", profile.vnc.dynamicResolution);
    vnc.insert("aspectRatioLock", profile.vnc.aspectRatioLock);

    QJsonObject json;
    json.insert("id", profile.id);
    json.insert("name", profile.name);
    json.insert("folderPath", normalizeFolderPath(profile.folderPath));
    json.insert("kind", kindToString(profile.kind));
    json.insert("host", profile.host);
    json.insert("port", profile.port);
    json.insert("favorite", profile.favorite);
    json.insert("auth", auth);
    json.insert("tunnel", tunnel);
    json.insert("vnc", vnc);
    return json;
}

SessionProfile profileFromJson(const QJsonObject& json) {
    SessionProfile profile;
    profile.id = json.value("id").toString();
    profile.name = json.value("name").toString();
    profile.folderPath = normalizeFolderPath(json.value("folderPath").toString("User sessions"));
    profile.kind = stringToKind(json.value("kind").toString("ssh"));
    profile.host = json.value("host").toString();
    profile.port = json.value("port").toInt(profile.kind == SessionKind::Vnc ? 5900 : 22);
    profile.favorite = json.value("favorite").toBool(false);

    const QJsonObject auth = json.value("auth").toObject();
    profile.auth.username = auth.value("username").toString();
    if (auth.contains("password")) {
        profile.auth.password = auth.value("password").toString();
    } else {
        profile.auth.password = std::nullopt;
    }
    if (auth.contains("privateKeyPath")) {
        profile.auth.privateKeyPath = auth.value("privateKeyPath").toString();
    } else {
        profile.auth.privateKeyPath = std::nullopt;
    }

    const QJsonObject tunnel = json.value("tunnel").toObject();
    profile.tunnel.enabled = tunnel.value("enabled").toBool(false);
    profile.tunnel.jumpHost = tunnel.value("jumpHost").toString();
    profile.tunnel.jumpPort = tunnel.value("jumpPort").toInt(22);
    profile.tunnel.jumpUsername = tunnel.value("jumpUsername").toString();

    const QJsonObject vnc = json.value("vnc").toObject();
    profile.vnc.username = vnc.value("username").toString();
    profile.vnc.viewOnly = vnc.value("viewOnly").toBool(false);
    profile.vnc.shareDesktop = vnc.value("shareDesktop").toBool(true);
    profile.vnc.enableJpeg = vnc.value("enableJpeg").toBool(true);
    profile.vnc.useRemoteCursor = vnc.value("useRemoteCursor").toBool(true);
    profile.vnc.qualityPreset = vnc.value("qualityPreset").toInt(2);
    profile.vnc.useQualityPreset = vnc.value("useQualityPreset").toBool(true);
    profile.vnc.requestedDepth = vnc.value("requestedDepth").toInt(24);
    profile.vnc.qualityLevel = vnc.value("qualityLevel").toInt(6);
    profile.vnc.compressLevel = vnc.value("compressLevel").toInt(2);
    profile.vnc.encodings = vnc.value("encodings").toString("tight zrle hextile copyrect raw");
    profile.vnc.repeater = vnc.value("repeater").toString();
    profile.vnc.connectTimeoutSeconds = vnc.value("connectTimeoutSeconds").toInt(15);
    profile.vnc.disableEncryption = vnc.value("disableEncryption").toBool(false);
    profile.vnc.disableClipboard = vnc.value("disableClipboard").toBool(false);
    profile.vnc.dynamicResolution = vnc.value("dynamicResolution").toBool(false);
    profile.vnc.aspectRatioLock = vnc.value("aspectRatioLock").toDouble(0.0);

    return profile;
}

QString relativeFolderPath(const QString& normalizedFolderPath) {
    if (normalizedFolderPath == "User sessions") {
        return QString();
    }

    constexpr char kPrefix[] = "User sessions/";
    if (normalizedFolderPath.startsWith(kPrefix)) {
        return normalizedFolderPath.mid(static_cast<int>(sizeof(kPrefix) - 1));
    }

    return QString();
}

}  // namespace

SessionRepository::SessionRepository()
    : sessionsRootPath_(QDir::cleanPath(QString("%1/.openxterm/userprofile/sessions").arg(QDir::homePath()))) {
    QDir().mkpath(sessionsRootPath_);
    reloadFromDisk();
}

const std::vector<SessionProfile>& SessionRepository::loadAll() const {
    return profiles_;
}

QStringList SessionRepository::loadFolderPaths() const {
    QSet<QString> folderPaths;
    folderPaths.insert("User sessions");

    QDirIterator it(sessionsRootPath_, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString absoluteDirPath = QDir::cleanPath(it.filePath());
        if (!pathWithinRoot(sessionsRootPath_, absoluteDirPath)) {
            continue;
        }

        const QString relative = QDir(sessionsRootPath_).relativeFilePath(absoluteDirPath);
        if (relative.isEmpty() || relative == ".") {
            continue;
        }

        folderPaths.insert(normalizeFolderPath(QString("User sessions/%1").arg(relative)));
    }

    QStringList result = folderPaths.values();
    std::sort(result.begin(), result.end());
    return result;
}

QString SessionRepository::sessionsRootPath() const {
    return sessionsRootPath_;
}

SessionProfile* SessionRepository::findById(const QString& id) {
    for (SessionProfile& profile : profiles_) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

void SessionRepository::upsert(const SessionProfile& profile) {
    SessionProfile persisted = profile;
    if (persisted.id.isEmpty()) {
        persisted.id = QString("session-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    persisted.folderPath = normalizeFolderPath(persisted.folderPath);

    const QString relativeFolder = relativeFolderPath(persisted.folderPath);
    const QString absoluteFolderPath = relativeFolder.isEmpty()
        ? sessionsRootPath_
        : QDir(sessionsRootPath_).filePath(relativeFolder);

    if (!pathWithinRoot(sessionsRootPath_, QDir::cleanPath(absoluteFolderPath))) {
        return;
    }
    QDir().mkpath(absoluteFolderPath);

    const QString fileName = QString("%1.oxt").arg(sanitizeFileStem(persisted.id));
    const QString targetFilePath = QDir(absoluteFolderPath).filePath(fileName);
    if (!pathWithinRoot(sessionsRootPath_, QDir::cleanPath(targetFilePath))) {
        return;
    }

    QSaveFile file(targetFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const QJsonDocument document(profileToJson(persisted));
    file.write(document.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return;
    }

    const QString previousFilePath = filePathById_.value(persisted.id);
    if (!previousFilePath.isEmpty() && previousFilePath != targetFilePath && QFile::exists(previousFilePath)) {
        QFile::remove(previousFilePath);
    }

    bool updated = false;
    for (SessionProfile& existing : profiles_) {
        if (existing.id == persisted.id) {
            existing = persisted;
            updated = true;
            break;
        }
    }

    if (!updated) {
        profiles_.push_back(persisted);
    }

    filePathById_.insert(persisted.id, QDir::cleanPath(targetFilePath));
}

bool SessionRepository::ensureFolder(const QString& folderPath) {
    const QString normalizedFolderPath = normalizeFolderPath(folderPath);
    const QString relativeFolder = relativeFolderPath(normalizedFolderPath);
    const QString absoluteFolderPath = relativeFolder.isEmpty()
        ? sessionsRootPath_
        : QDir(sessionsRootPath_).filePath(relativeFolder);

    const QString cleanedAbsolutePath = QDir::cleanPath(absoluteFolderPath);
    if (!pathWithinRoot(sessionsRootPath_, cleanedAbsolutePath)) {
        return false;
    }

    return QDir().mkpath(cleanedAbsolutePath);
}

bool SessionRepository::deleteSession(const QString& id) {
    const QString filePath = filePathById_.value(id);
    if (filePath.isEmpty()) {
        return false;
    }

    if (QFile::exists(filePath) && !QFile::remove(filePath)) {
        return false;
    }

    filePathById_.remove(id);
    profiles_.erase(std::remove_if(profiles_.begin(),
                                   profiles_.end(),
                                   [&id](const SessionProfile& profile) { return profile.id == id; }),
                    profiles_.end());
    return true;
}

bool SessionRepository::moveSessionToFolder(const QString& id, const QString& folderPath) {
    SessionProfile* profile = findById(id);
    if (profile == nullptr) {
        return false;
    }

    SessionProfile updated = *profile;
    updated.folderPath = normalizeFolderPath(folderPath);
    if (!ensureFolder(updated.folderPath)) {
        return false;
    }

    upsert(updated);
    return true;
}

bool SessionRepository::deleteFolder(const QString& folderPath) {
    const QString normalizedFolderPath = normalizeFolderPath(folderPath);
    if (normalizedFolderPath == "User sessions") {
        return false;
    }

    QStringList idsToDelete;
    for (const SessionProfile& profile : profiles_) {
        if (profile.folderPath == normalizedFolderPath ||
            profile.folderPath.startsWith(QString("%1/").arg(normalizedFolderPath))) {
            idsToDelete.append(profile.id);
        }
    }

    for (const QString& id : idsToDelete) {
        if (!deleteSession(id)) {
            return false;
        }
    }

    const QString relativeFolder = relativeFolderPath(normalizedFolderPath);
    if (relativeFolder.isEmpty()) {
        return false;
    }

    const QString absoluteFolderPath = QDir(sessionsRootPath_).filePath(relativeFolder);
    const QString cleanedAbsolutePath = QDir::cleanPath(absoluteFolderPath);
    if (!pathWithinRoot(sessionsRootPath_, cleanedAbsolutePath)) {
        return false;
    }

    QDir dir(cleanedAbsolutePath);
    if (dir.exists() && !dir.removeRecursively()) {
        return false;
    }

    return true;
}

bool SessionRepository::moveFolder(const QString& sourceFolderPath, const QString& destinationFolderPath) {
    const QString source = normalizeFolderPath(sourceFolderPath);
    const QString destination = normalizeFolderPath(destinationFolderPath);
    if (source == "User sessions") {
        return false;
    }

    if (destination == source || destination.startsWith(QString("%1/").arg(source))) {
        return false;
    }

    const int slashIndex = source.lastIndexOf('/');
    const QString sourceLeaf = slashIndex >= 0 ? source.mid(slashIndex + 1) : source;
    const QString target = destination == "User sessions"
        ? QString("User sessions/%1").arg(sourceLeaf)
        : QString("%1/%2").arg(destination, sourceLeaf);

    if (target == source) {
        return true;
    }

    if (!ensureFolder(target)) {
        return false;
    }

    std::vector<SessionProfile> updates;
    for (const SessionProfile& profile : profiles_) {
        if (profile.folderPath == source || profile.folderPath.startsWith(QString("%1/").arg(source))) {
            SessionProfile updated = profile;
            updated.folderPath = profile.folderPath;
            updated.folderPath.replace(0, source.size(), target);
            updates.push_back(updated);
        }
    }

    for (const SessionProfile& updated : updates) {
        upsert(updated);
    }

    const QString relativeSource = relativeFolderPath(source);
    if (!relativeSource.isEmpty()) {
        const QString absoluteSource = QDir(sessionsRootPath_).filePath(relativeSource);
        QDir sourceDir(QDir::cleanPath(absoluteSource));
        if (sourceDir.exists()) {
            sourceDir.removeRecursively();
        }
    }

    return true;
}

void SessionRepository::reloadFromDisk() {
    profiles_.clear();
    filePathById_.clear();

    QDir().mkpath(sessionsRootPath_);

    QDirIterator it(sessionsRootPath_, QStringList() << "*.oxt", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = QDir::cleanPath(it.next());
        if (!pathWithinRoot(sessionsRootPath_, filePath)) {
            continue;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }

        SessionProfile profile = profileFromJson(document.object());
        if (profile.id.isEmpty() || profile.name.trimmed().isEmpty()) {
            continue;
        }

        const QString relativeFileDir = QDir(sessionsRootPath_).relativeFilePath(QFileInfo(filePath).absolutePath());
        if (profile.folderPath.trimmed().isEmpty()) {
            if (relativeFileDir.isEmpty() || relativeFileDir == ".") {
                profile.folderPath = "User sessions";
            } else {
                profile.folderPath = normalizeFolderPath(QString("User sessions/%1").arg(relativeFileDir));
            }
        } else {
            profile.folderPath = normalizeFolderPath(profile.folderPath);
        }

        profiles_.push_back(profile);
        filePathById_.insert(profile.id, filePath);
    }
}

}  // namespace domain
