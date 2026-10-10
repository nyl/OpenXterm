#pragma once

#include <QWidget>

#include "../domain/SessionProfile.h"

class QLabel;
class QTermWidget;

class SshTerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit SshTerminalWidget(const domain::SessionProfile& profile, QWidget* parent = nullptr);

private:
    void startSession();
    [[nodiscard]] QStringList buildSshArgs() const;

    domain::SessionProfile profile_;
    QTermWidget* terminal_ = nullptr;
};
