#include <QApplication>
#include <QByteArray>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QtGlobal>

#ifdef __linux__
#include <unistd.h>

#include <string>
#include <thread>
#endif

#include "app/MainWindow.h"

namespace {
QtMessageHandler gPreviousHandler = nullptr;

void filteredQtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    if (type == QtWarningMsg && msg.contains("Using a variable-width font in the terminal")) {
        return;
    }

    if (gPreviousHandler != nullptr) {
        gPreviousHandler(type, context, msg);
        return;
    }

    const QByteArray local = msg.toLocal8Bit();
    fprintf(stderr, "%s\n", local.constData());
}

#ifdef __linux__
constexpr const char* kVariableWidthWarning = "Using a variable-width font in the terminal";

void writeAll(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        const ssize_t out = ::write(fd, data + written, len - written);
        if (out <= 0) {
            return;
        }
        written += static_cast<size_t>(out);
    }
}

void installStderrWarningFilter() {
    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) {
        return;
    }

    const int originalStderr = ::dup(STDERR_FILENO);
    if (originalStderr < 0) {
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return;
    }

    if (::dup2(pipeFds[1], STDERR_FILENO) < 0) {
        ::close(originalStderr);
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return;
    }

    ::close(pipeFds[1]);

    std::thread([readFd = pipeFds[0], originalStderr]() {
        std::string buffered;
        char chunk[512];

        while (true) {
            const ssize_t bytes = ::read(readFd, chunk, sizeof(chunk));
            if (bytes <= 0) {
                break;
            }

            buffered.append(chunk, static_cast<size_t>(bytes));

            size_t newlinePos = std::string::npos;
            while ((newlinePos = buffered.find('\n')) != std::string::npos) {
                std::string line = buffered.substr(0, newlinePos);
                buffered.erase(0, newlinePos + 1);

                if (line.find(kVariableWidthWarning) != std::string::npos) {
                    continue;
                }

                line.push_back('\n');
                writeAll(originalStderr, line.data(), line.size());
            }
        }

        if (!buffered.empty() && buffered.find(kVariableWidthWarning) == std::string::npos) {
            writeAll(originalStderr, buffered.data(), buffered.size());
        }

        ::close(readFd);
        ::close(originalStderr);
    }).detach();
}
#endif
}

int main(int argc, char* argv[]) {
#ifdef __linux__
    installStderrWarningFilter();
#endif

    gPreviousHandler = qInstallMessageHandler(filteredQtMessageHandler);

    QApplication app(argc, argv);
    app.setApplicationName("OpenXterm");
    app.setOrganizationName("OpenXterm");

    const QStringList preferredFamilies = {
        "Noto Sans Mono",
        "DejaVu Sans Mono",
        "Liberation Mono"
    };

    QFont terminalClassFont;
    for (const QString& family : preferredFamilies) {
        if (!QFontDatabase::families().contains(family) || !QFontDatabase::isFixedPitch(family)) {
            continue;
        }

        QFont candidate(family);
        candidate.setStyleHint(QFont::TypeWriter);
        candidate.setFixedPitch(true);
        candidate.setPointSize(11);
        if (QFontInfo(candidate).fixedPitch()) {
            terminalClassFont = candidate;
            break;
        }
    }

    if (terminalClassFont.family().isEmpty()) {
        terminalClassFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        terminalClassFont.setStyleHint(QFont::TypeWriter);
        terminalClassFont.setFixedPitch(true);
        terminalClassFont.setPointSize(11);
    }

    app.setFont(terminalClassFont, "QTermWidget");

    MainWindow window;
    window.resize(1440, 900);
    window.show();

    return app.exec();
}
