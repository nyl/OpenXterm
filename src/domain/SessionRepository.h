#pragma once

#include "SessionProfile.h"

#include <QHash>
#include <QStringList>

#include <vector>

namespace domain {

class SessionRepository {
public:
    SessionRepository();

    [[nodiscard]] const std::vector<SessionProfile>& loadAll() const;
    [[nodiscard]] QStringList loadFolderPaths() const;
    [[nodiscard]] QString sessionsRootPath() const;
    [[nodiscard]] SessionProfile* findById(const QString& id);
    void upsert(const SessionProfile& profile);
    bool ensureFolder(const QString& folderPath);
    bool deleteSession(const QString& id);
    bool moveSessionToFolder(const QString& id, const QString& folderPath);
    bool deleteFolder(const QString& folderPath);
    bool moveFolder(const QString& sourceFolderPath, const QString& destinationFolderPath);

private:
    void reloadFromDisk();

    QString sessionsRootPath_;
    std::vector<SessionProfile> profiles_;
    QHash<QString, QString> filePathById_;
};

}  // namespace domain
