#include "SshTerminalWidget.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QVBoxLayout>

#include <qtermwidget.h>

namespace {
QFont chooseMonospaceFont() {
    const QStringList preferredFamilies = {
        "DejaVu Sans Mono",
        "Noto Sans Mono",
        "Liberation Mono",
        "Courier New"
    };

    const QStringList installed = QFontDatabase::families();
    for (const QString& family : preferredFamilies) {
        if (!installed.contains(family) || !QFontDatabase::isFixedPitch(family)) {
            continue;
        }

        QFont font(family);
        font.setStyleHint(QFont::TypeWriter);
        font.setFixedPitch(true);
        font.setPointSize(11);
        if (QFontInfo(font).fixedPitch()) {
            return font;
        }
    }

    for (const QString& family : installed) {
        if (!QFontDatabase::isFixedPitch(family)) {
            continue;
        }

        QFont font(family);
        font.setStyleHint(QFont::TypeWriter);
        font.setFixedPitch(true);
        font.setPointSize(11);
        if (QFontInfo(font).fixedPitch()) {
            return font;
        }
    }

    QFont fallback = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fallback.setStyleHint(QFont::TypeWriter);
    fallback.setFixedPitch(true);
    fallback.setFamilies({"Noto Sans Mono", "DejaVu Sans Mono", fallback.family()});
    fallback.setPointSize(11);
    return fallback;
}
}

SshTerminalWidget::SshTerminalWidget(const domain::SessionProfile& profile, QWidget* parent)
    : QWidget(parent), profile_(profile) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    terminal_ = new QTermWidget(0, this);
    terminal_->setTerminalFont(chooseMonospaceFont());
    terminal_->setColorScheme("Linux");
    terminal_->setScrollBarPosition(QTermWidget::ScrollBarRight);
    terminal_->setAutoClose(false);
    terminal_->setArgs(buildSshArgs());
    terminal_->setShellProgram("ssh");

    root->addWidget(terminal_, 1);

    startSession();
}

QStringList SshTerminalWidget::buildSshArgs() const {
    QStringList args;
    args << "-tt";
    args << "-p" << QString::number(profile_.port);

    if (profile_.tunnel.enabled && !profile_.tunnel.jumpHost.isEmpty()) {
        const QString jumpEndpoint = QString("%1@%2:%3")
            .arg(profile_.tunnel.jumpUsername, profile_.tunnel.jumpHost)
            .arg(profile_.tunnel.jumpPort);
        args << "-J" << jumpEndpoint;
    }

    args << QString("%1@%2").arg(profile_.auth.username, profile_.host);
    return args;
}

void SshTerminalWidget::startSession() {
    if (terminal_ == nullptr) {
        return;
    }

    terminal_->startShellProgram();
}
