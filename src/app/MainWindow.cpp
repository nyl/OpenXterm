#include "MainWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCursor>
#include <QDir>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QPainter>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QFrame>
#include <QSplitter>
#include <QStatusBar>
#include <QStackedWidget>
#include <QSizePolicy>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "SessionEditorDialog.h"
#include "../domain/SessionProfile.h"
#include "../runtime/SshTerminalWidget.h"
#include "../runtime/VncSessionWidget.h"

#include <qtermwidget.h>

namespace {
QFont chooseMonospaceUiFont() {
    const QStringList preferredFamilies = {
        "DejaVu Sans Mono",
        "Noto Sans Mono",
        "Liberation Mono"
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

QIcon makePinIcon() {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(58, 96, 148));

    // Pushpin head.
    painter.drawRoundedRect(QRectF(3.5, 2.0, 9.0, 4.0), 1.5, 1.5);
    // Neck.
    painter.drawRect(QRectF(7.0, 6.0, 2.0, 3.5));

    QPen needlePen(QColor(58, 96, 148));
    needlePen.setWidth(2);
    needlePen.setCapStyle(Qt::RoundCap);
    painter.setPen(needlePen);
    painter.drawLine(QPointF(8.0, 9.0), QPointF(8.0, 14.0));

    return QIcon(pixmap);
}

}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenXterm");
    resize(1200, 760);
    setStyleSheet(
        "QMainWindow { background: #e9edf3; }"
        "QMenuBar {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f6f8fb, stop:1 #e7ecf4);"
        " border-bottom: 1px solid #b8c1cd;"
        " color: #1f2937;"
        "}"
        "QMenuBar::item { padding: 6px 10px; margin: 1px 1px; border-radius: 4px; }"
        "QMenuBar::item:selected { background: #dbe8ff; }"
        "QMenu { background: #f7f9fc; border: 1px solid #b8c1cd; }"
        "QMenu::item { padding: 6px 20px; }"
        "QMenu::item:selected { background: #dbe8ff; color: #0f274d; }"
        "QTreeWidget { background: #f6f8fb; border: 1px solid #c5ced8; selection-background-color: #dbe8ff; selection-color: #1f2937; }"
    );
    createMenus();
    createTopToolbar();
    createCentralLayout();
    createStatusAndKeyBar();

    sidebarHideTimer_ = new QTimer(this);
    sidebarHideTimer_->setSingleShot(true);
    sidebarHideTimer_->setInterval(650);
    connect(sidebarHideTimer_, &QTimer::timeout, this, &MainWindow::hideFloatingSidebar);

    sessionRepository_.ensureFolder("User sessions");
    folderPaths_.insert("User sessions");
    for (const QString& folderPath : sessionRepository_.loadFolderPaths()) {
        folderPaths_.insert(folderPath);
    }
}

void MainWindow::createMenus() {
    auto* terminalMenu = menuBar()->addMenu("Terminal");
    auto* newSshAction = terminalMenu->addAction("New SSH");
    newSshAction->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    newSshAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(newSshAction, &QAction::triggered, this, &MainWindow::onNewSshSession);
    auto* newVncAction = terminalMenu->addAction("New VNC", this, &MainWindow::onNewVncSession);
    newVncAction->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
    terminalMenu->addSeparator();
    auto* closeAction = terminalMenu->addAction("Close");
    closeAction->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    closeAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);

    auto* sessionsMenu = menuBar()->addMenu("Sessions");
    auto* newSshSessionAction = sessionsMenu->addAction("New SSH Session");
    newSshSessionAction->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    newSshSessionAction->setShortcut(QKeySequence("Ctrl+Alt+S"));
    connect(newSshSessionAction, &QAction::triggered, this, &MainWindow::onNewSshSession);

    auto* newVncSessionAction = sessionsMenu->addAction("New VNC Session");
    newVncSessionAction->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
    newVncSessionAction->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(newVncSessionAction, &QAction::triggered, this, &MainWindow::onNewVncSession);

    sessionsMenu->addSeparator();
    auto* editSessionAction = sessionsMenu->addAction("Edit Selected Session");
    editSessionAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    editSessionAction->setShortcut(QKeySequence("F2"));
    connect(editSessionAction, &QAction::triggered, this, &MainWindow::onEditSelectedSession);

    menuBar()->addMenu("View");
    menuBar()->addMenu("X server");
    menuBar()->addMenu("Tools");
    menuBar()->addMenu("Games");
    menuBar()->addMenu("Settings");
    menuBar()->addMenu("Macros");
    menuBar()->addMenu("Help");
}

void MainWindow::createTopToolbar() {
    // Intentionally empty: quick-connect is rendered in the top workspace strip.
}

void MainWindow::createCentralLayout() {
    centralRoot_ = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(centralRoot_);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    workspaceStrip_ = new QWidget(centralRoot_);
    workspaceStrip_->setFixedHeight(32);
    workspaceStrip_->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e8edf5, stop:1 #d4dce8);"
        "border: 1px solid #9aa6b6;"
        "border-bottom: 0;"
    );
    auto* workspaceStripLayout = new QHBoxLayout(workspaceStrip_);
    workspaceStripLayout->setContentsMargins(2, 0, 4, 0);
    workspaceStripLayout->setSpacing(4);

    workspaceStripTabs_ = new QTabBar(workspaceStrip_);
    workspaceStripTabs_->setDrawBase(false);
    workspaceStripTabs_->setMovable(false);
    workspaceStripTabs_->setExpanding(false);
    workspaceStripTabs_->setUsesScrollButtons(true);
    workspaceStripTabs_->setTabsClosable(true);
    workspaceStripTabs_->setIconSize(QSize(14, 14));
    workspaceStripTabs_->setStyleSheet(
        "QTabBar::tab {"
        " background: #d9e1ec;"
        " color: #1f2937;"
        " border: 1px solid #9aa6b6;"
        " border-bottom: 0;"
        " border-top-left-radius: 6px;"
        " border-top-right-radius: 6px;"
        " padding: 4px 10px 4px 10px;"
        " padding-right: 24px;"
        " margin-top: 4px;"
        " margin-right: 3px;"
        "}"
        "QTabBar::tab:hover { background: #e6edf8; }"
        "QTabBar::tab:selected {"
        " background: #1f2f46;"
        " color: #ffffff;"
        " border-color: #1f2f46;"
        "}"
    );

    quickConnectEdit_ = new QLineEdit(workspaceStrip_);
    quickConnectEdit_->setPlaceholderText("Quick connect...");
    quickConnectEdit_->setFixedHeight(26);
    quickConnectEdit_->setFixedWidth(236);
    quickConnectEdit_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    quickConnectEdit_->setClearButtonEnabled(true);
    quickConnectEdit_->setStyleSheet(
        "QLineEdit {"
        " background: #f9fbfe;"
        " border: 1px solid #a8b4c4;"
        " border-radius: 5px;"
        " padding: 3px 8px;"
        " color: #1f2937;"
        " selection-background-color: #cfe0fb;"
        "}"
        "QLineEdit:focus {"
        " border: 1px solid #5b84c4;"
        " background: #ffffff;"
        "}"
    );
    quickConnectEdit_->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::LeadingPosition);
    connect(quickConnectEdit_, &QLineEdit::returnPressed, this, &MainWindow::onQuickConnect);
    workspaceStripLayout->addWidget(quickConnectEdit_);
    workspaceStripLayout->addWidget(workspaceStripTabs_, 1);

    rootLayout->addWidget(workspaceStrip_);

    splitter_ = new QSplitter(Qt::Horizontal, centralRoot_);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet("QSplitter::handle { background: #b8bec7; }");

    leftShell_ = new QWidget(splitter_);
    leftShell_->setStyleSheet(
        "background: #eef3fa;"
        "border-right: 1px solid #b9c4d2;"
    );
    leftShellLayout_ = new QVBoxLayout(leftShell_);
    leftShellLayout_->setContentsMargins(0, 0, 0, 0);
    leftShellLayout_->setSpacing(0);

    leftHeaderRow_ = new QWidget(leftShell_);
    auto* leftHeaderLayout = new QHBoxLayout(leftHeaderRow_);
    leftHeaderLayout->setContentsMargins(0, 0, 0, 4);
    leftHeaderLayout->setSpacing(6);

    auto* leftHeader = new QLabel("", leftHeaderRow_);
    leftHeader->setStyleSheet("font-weight: 600; color: #313131;");
    leftHeaderLayout->addWidget(leftHeader);
    leftHeaderLayout->addStretch(1);

    sidebarTabs_ = new QTabWidget(leftShell_);
    sidebarTabs_->setTabPosition(QTabWidget::West);
    sidebarTabs_->setMovable(false);
    sidebarTabs_->setDocumentMode(true);
    applySidebarVisualState(true);

    auto* sessionsPage = new QWidget(sidebarTabs_);
    auto* sessionsPageLayout = new QVBoxLayout(sessionsPage);
    sessionsPageLayout->setContentsMargins(6, 6, 6, 6);
    sessionsPageLayout->setSpacing(6);

    sessionsTree_ = new QTreeWidget(sessionsPage);
    sessionsTree_->setHeaderLabel("Sessions");
    sessionsTree_->setMinimumWidth(220);
    sessionsTree_->header()->setStretchLastSection(true);
    sessionsTree_->setStyleSheet(
        "QTreeWidget {"
        " background: #f8fbff;"
        " border: 1px solid #c2ccd8;"
        " alternate-background-color: #f1f6fd;"
        " show-decoration-selected: 1;"
        "}"
        "QTreeWidget::item { padding: 2px 4px; }"
        "QTreeWidget::item:selected {"
        " background: #dbe8ff;"
        " color: #14253f;"
        " border: 1px solid #b8c9e6;"
        "}"
    );
    sessionsTree_->setDragEnabled(true);
    sessionsTree_->viewport()->setAcceptDrops(true);
    sessionsTree_->setDropIndicatorShown(true);
    sessionsTree_->setDragDropMode(QAbstractItemView::InternalMove);
    sessionsTree_->setDefaultDropAction(Qt::MoveAction);
    sessionsTree_->viewport()->installEventFilter(this);
    sessionsPageLayout->addWidget(sessionsTree_);

    auto* toolsPage = new QWidget(sidebarTabs_);
    auto* toolsLayout = new QVBoxLayout(toolsPage);
    toolsLayout->setContentsMargins(10, 10, 10, 10);
    auto* toolsLabel = new QLabel("Tools\n\n- Tunnels\n- Port scan\n- Bookmarks", toolsPage);
    toolsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    toolsLayout->addWidget(toolsLabel);

    auto* macrosPage = new QWidget(sidebarTabs_);
    auto* macrosLayout = new QVBoxLayout(macrosPage);
    macrosLayout->setContentsMargins(10, 10, 10, 10);
    auto* macrosLabel = new QLabel("Macros\n\nRecord and replay command sequences.", macrosPage);
    macrosLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    macrosLayout->addWidget(macrosLabel);

    sidebarTabs_->addTab(sessionsPage, style()->standardIcon(QStyle::SP_DirOpenIcon), "Sessions");
    sidebarTabs_->addTab(toolsPage, style()->standardIcon(QStyle::SP_FileDialogDetailedView), "Tools");
    sidebarTabs_->addTab(macrosPage, style()->standardIcon(QStyle::SP_ArrowRight), "Macros");
    sidebarTabs_->setCurrentIndex(0);
    leftShellLayout_->addWidget(sidebarTabs_);

    populateSessionsTree();
    setupSessionsContextMenu();

    workspaceTabs_ = new QTabWidget(splitter_);
    workspaceTabs_->setObjectName("workspaceTabs");
    workspaceTabs_->setDocumentMode(true);
    workspaceTabs_->setTabsClosable(false);
    workspaceTabs_->setStyleSheet(
        "QTabWidget#workspaceTabs::pane {"
        " border: 1px solid #8e949d;"
        " border-top: 1px solid #8e949d;"
        "}"
        "QTabWidget#workspaceTabs QTabBar::tab { max-height: 0px; min-height: 0px; padding: 0; margin: 0; border: 0; }"
    );
    workspaceTabs_->tabBar()->hide();

    auto* homePane = new QWidget(workspaceTabs_);
    homePane->setStyleSheet("background: #dcdcdc;");
    auto* homeLayout = new QVBoxLayout(homePane);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(0);

    auto* homeCenter = new QWidget(homePane);
    homeCenter->setFixedWidth(520);
    auto* centerLayout = new QVBoxLayout(homeCenter);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(12);
    auto* titleIcon = new QLabel(homeCenter);
    titleIcon->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(56, 56));
    auto* titleText = new QLabel("OpenXterm", homeCenter);
    titleText->setStyleSheet("font-size: 44px; color: #323232; font-weight: 400;");
    titleRow->addStretch();
    titleRow->addWidget(titleIcon);
    titleRow->addWidget(titleText);
    titleRow->addStretch();

    auto* actionsRow = new QHBoxLayout();
    actionsRow->setSpacing(14);
    auto* startLocalTerminalButton = new QPushButton("Start local terminal", homeCenter);
    auto* newSessionButton = new QPushButton("New session", homeCenter);
    startLocalTerminalButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    newSessionButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    startLocalTerminalButton->setFixedSize(160, 28);
    newSessionButton->setFixedSize(140, 28);
    startLocalTerminalButton->setStyleSheet(
        "QPushButton {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e8f2ff, stop:1 #d7e9ff);"
        " border: 1px solid #9ebbe6;"
        " border-radius: 5px;"
        " color: #1b2a3a;"
        "}"
        "QPushButton:hover { background: #dcecff; }"
    );
    newSessionButton->setStyleSheet(
        "QPushButton {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0f3f8, stop:1 #e1e7f0);"
        " border: 1px solid #b6c0ce;"
        " border-radius: 5px;"
        " color: #2f2f2f;"
        "}"
        "QPushButton:hover { background: #e8edf5; }"
    );
    actionsRow->addStretch();
    actionsRow->addWidget(startLocalTerminalButton);
    actionsRow->addWidget(newSessionButton);
    actionsRow->addStretch();

    auto* themeBox = new QFrame(homeCenter);
    themeBox->setStyleSheet("QFrame { background: #d8dde6; border: 1px solid #c3cad4; border-radius: 4px; }");
    auto* themeLayout = new QVBoxLayout(themeBox);
    themeLayout->setContentsMargins(14, 10, 14, 10);
    themeLayout->setSpacing(8);
    auto* themeTitle = new QLabel("Select your favorite theme", themeBox);
    themeTitle->setAlignment(Qt::AlignHCenter);
    themeTitle->setStyleSheet("color: #222;");

    auto* previewsRow = new QHBoxLayout();
    previewsRow->setSpacing(18);
    auto makeThemePreview = [](const QString& name, const QString& fill) {
        auto* box = new QWidget();
        auto* layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        auto* preview = new QFrame(box);
        preview->setFixedSize(170, 78);
        preview->setStyleSheet(QString("QFrame { background: %1; border: 1px solid #8f8f8f; }").arg(fill));
        auto* label = new QLabel(name, box);
        label->setAlignment(Qt::AlignHCenter);
        layout->addWidget(preview);
        layout->addWidget(label);
        return box;
    };
    previewsRow->addStretch();
    previewsRow->addWidget(makeThemePreview("Light", "#f7f7f7"));
    previewsRow->addWidget(makeThemePreview("Dark", "#2a2a2a"));
    previewsRow->addStretch();

    themeLayout->addWidget(themeTitle);
    themeLayout->addLayout(previewsRow);

    centerLayout->addLayout(titleRow);
    centerLayout->addLayout(actionsRow);
    centerLayout->addWidget(themeBox);

    homeLayout->addStretch();
    homeLayout->addWidget(homeCenter, 0, Qt::AlignHCenter);
    homeLayout->addStretch();

    const QIcon homeIcon = style()->standardIcon(QStyle::SP_DirHomeIcon);
    workspaceTabs_->addTab(homePane, homeIcon, "Home");

    connect(startLocalTerminalButton, &QPushButton::clicked, this, &MainWindow::onStartLocalTerminal);
    connect(newSessionButton, &QPushButton::clicked, this, &MainWindow::onNewSshSession);
    workspaceTabs_->setCurrentIndex(0);
    rebuildWorkspaceStripTabs();
    workspaceStripTabs_->setCurrentIndex(0);

    splitter_->addWidget(leftShell_);
    splitter_->addWidget(workspaceTabs_);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({240, 960});

    rootLayout->addWidget(splitter_);
    setCentralWidget(centralRoot_);

    sidebarToggleButton_ = new QToolButton(sidebarTabs_->tabBar());
    sidebarToggleButton_->setFixedSize(24, 28);
    sidebarToggleButton_->setAutoRaise(true);
    QFont toggleFont("DejaVu Sans Mono", 13, QFont::Bold);
    toggleFont.setStyleHint(QFont::TypeWriter);
    sidebarToggleButton_->setFont(toggleFont);
    sidebarToggleButton_->setStyleSheet(
        "QToolButton {"
        "  background: #edf3fc;"
        "  border: 1px solid #b7c3d3;"
        "  border-radius: 2px;"
        "  color: #2b5f9c;"
        "  padding: 0px;"
        "}"
        "QToolButton:hover {"
        "  background: #dfebfb;"
        "  border: 1px solid #8fb2e4;"
        "  color: #1d4f8b;"
        "}"
    );
    sidebarToggleButton_->installEventFilter(this);
    connect(sidebarToggleButton_, &QToolButton::clicked, this, &MainWindow::toggleSidebarAutoHide);

    sidebarTabs_->installEventFilter(this);
    sidebarToggleButton_->show();
    if (sidebarAutoHide_) {
        applySidebarVisualState(false);
        splitter_->setSizes({30, std::max(1, splitter_->width() - 30)});
    }

    updateSidebarToggleButton();
    updateFloatingSidebarGeometry();

    connect(workspaceStripTabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (workspaceTabs_ == nullptr || index < 0 || index >= workspaceTabs_->count()) {
            return;
        }
        workspaceTabs_->setCurrentIndex(index);
    });

    connect(workspaceStripTabs_, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (index == 0) {
            return;
        }
        workspaceTabs_->removeTab(index);
        rebuildWorkspaceStripTabs();
        const int targetIndex = std::min(index, workspaceTabs_->count() - 1);
        workspaceTabs_->setCurrentIndex(std::max(0, targetIndex));
    });

    connect(workspaceTabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (workspaceStripTabs_ != nullptr && index >= 0 && index < workspaceStripTabs_->count()) {
            workspaceStripTabs_->setCurrentIndex(index);
        }
    });

    connect(sessionsTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        const QString id = item->data(0, Qt::UserRole).toString();
        if (id.isEmpty()) {
            return;
        }

        if (domain::SessionProfile* profile = findProfileById(id); profile != nullptr) {
            openProfileTab(*profile);
        }
    });
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (sessionsTree_ != nullptr && watched == sessionsTree_->viewport() && event->type() == QEvent::Drop) {
        QTimer::singleShot(0, this, [this]() {
            persistTreeReorder();
        });
    }

    if (watched == sidebarToggleButton_) {
        if (sidebarAutoHide_ && event->type() == QEvent::Enter) {
            sidebarToggleButton_->setText(QString());
            sidebarToggleButton_->setIcon(makePinIcon());
            sidebarToggleButton_->setIconSize(QSize(16, 16));
            showFloatingSidebar();
        } else if (sidebarAutoHide_ && event->type() == QEvent::Leave) {
            updateSidebarToggleButton();
            scheduleFloatingSidebarHide();
        }
    }

    if (watched == sidebarTabs_) {
        if (sidebarAutoHide_ && event->type() == QEvent::Enter) {
            if (sidebarHideTimer_ != nullptr) {
                sidebarHideTimer_->stop();
            }
        } else if (sidebarAutoHide_ && event->type() == QEvent::Leave) {
            scheduleFloatingSidebarHide();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateFloatingSidebarGeometry();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    // Ensure tab bar geometry is finalized before placing the sidebar toggle.
    QTimer::singleShot(0, this, [this]() {
        updateSidebarToggleButton();
        updateFloatingSidebarGeometry();
    });
}

void MainWindow::toggleSidebarAutoHide() {
    const int collapsedWidth = 30;
    sidebarAutoHide_ = !sidebarAutoHide_;

    if (sidebarAutoHide_) {
        if (splitter_ != nullptr) {
            const QList<int> sizes = splitter_->sizes();
            if (!sizes.isEmpty() && sizes.first() > collapsedWidth + 10) {
                lastSidebarWidth_ = sizes.first();
            }
        }

        applySidebarVisualState(false);
        if (splitter_ != nullptr) {
            splitter_->setSizes({collapsedWidth, std::max(1, splitter_->width() - collapsedWidth)});
        }
    } else {
        if (sidebarHideTimer_ != nullptr) {
            sidebarHideTimer_->stop();
        }

        hideFloatingSidebar();
        applySidebarVisualState(true);
        if (splitter_ != nullptr) {
            splitter_->setSizes({lastSidebarWidth_, std::max(1, splitter_->width() - lastSidebarWidth_)});
        }
    }

    updateSidebarToggleButton();
    updateFloatingSidebarGeometry();
}

void MainWindow::showFloatingSidebar() {
    if (!sidebarAutoHide_ || sidebarTabs_ == nullptr) {
        return;
    }

    if (sidebarHideTimer_ != nullptr) {
        sidebarHideTimer_->stop();
    }

    applySidebarVisualState(true);
    if (splitter_ != nullptr) {
        splitter_->setSizes({lastSidebarWidth_, std::max(1, splitter_->width() - lastSidebarWidth_)});
    }

    updateFloatingSidebarGeometry();
}

void MainWindow::hideFloatingSidebar() {
    if (!sidebarAutoHide_ || sidebarTabs_ == nullptr) {
        return;
    }

    const QPoint globalPos = QCursor::pos();
    if (sidebarTabs_->rect().contains(sidebarTabs_->mapFromGlobal(globalPos))) {
        return;
    }

    if (sidebarToggleButton_ != nullptr &&
        sidebarToggleButton_->rect().contains(sidebarToggleButton_->mapFromGlobal(globalPos))) {
        return;
    }

    const int collapsedWidth = 30;
    applySidebarVisualState(false);
    if (splitter_ != nullptr) {
        splitter_->setSizes({collapsedWidth, std::max(1, splitter_->width() - collapsedWidth)});
    }
    updateFloatingSidebarGeometry();
}

void MainWindow::updateWorkspaceStripLeftPadding() {
    // Top strip is intentionally full-width (tabs + quick connect) and no longer offset.
}

void MainWindow::rebuildWorkspaceStripTabs() {
    if (workspaceStripTabs_ == nullptr || workspaceTabs_ == nullptr) {
        return;
    }

    workspaceStripTabs_->blockSignals(true);
    while (workspaceStripTabs_->count() > 0) {
        workspaceStripTabs_->removeTab(0);
    }
    for (int i = 0; i < workspaceTabs_->count(); ++i) {
        const int stripIndex = workspaceStripTabs_->addTab(workspaceTabs_->tabIcon(i), workspaceTabs_->tabText(i));
        if (i == 0) {
            workspaceStripTabs_->setTabButton(stripIndex, QTabBar::RightSide, nullptr);
            continue;
        }

        auto* closeHost = new QWidget(workspaceStripTabs_);
        closeHost->setFixedSize(22, 16);
        auto* closeHostLayout = new QHBoxLayout(closeHost);
        closeHostLayout->setContentsMargins(0, 1, 8, 1); // Extra 3px left shift + better vertical centering.
        closeHostLayout->setSpacing(0);

        auto* closeButton = new QToolButton(closeHost);
        closeButton->setText("x");
        closeButton->setCursor(Qt::PointingHandCursor);
        closeButton->setAutoRaise(true);
        closeButton->setFixedSize(14, 14);
        closeButton->setStyleSheet(
            "QToolButton {"
            " color: #24354d;"
            " border: 1px solid #9fb1c8;"
            " border-radius: 7px;"
            " background: #e8eef7;"
            " font-size: 11px;"
            " font-weight: 700;"
            " padding: 0px;"
            "}"
            "QToolButton:hover {"
            " color: #ffffff;"
            " background: #da5a5a;"
            " border: 1px solid #c84b4b;"
            "}"
            "QToolButton:pressed {"
            " color: #ffffff;"
            " background: #be3f3f;"
            " border: 1px solid #a63636;"
            "}"
        );
        closeHostLayout->addWidget(closeButton, 0, Qt::AlignCenter);
        closeButton->setProperty("tabIndex", stripIndex);
        connect(closeButton, &QToolButton::clicked, this, [this, closeButton]() {
            if (workspaceTabs_ == nullptr) {
                return;
            }

            bool ok = false;
            const int index = closeButton->property("tabIndex").toInt(&ok);
            if (!ok || index <= 0 || index >= workspaceTabs_->count()) {
                return;
            }

            workspaceTabs_->removeTab(index);
            rebuildWorkspaceStripTabs();
            const int targetIndex = std::min(index, workspaceTabs_->count() - 1);
            workspaceTabs_->setCurrentIndex(std::max(0, targetIndex));
        });
        workspaceStripTabs_->setTabButton(stripIndex, QTabBar::RightSide, closeHost);
    }
    workspaceStripTabs_->setCurrentIndex(workspaceTabs_->currentIndex());
    workspaceStripTabs_->blockSignals(false);
}

void MainWindow::scheduleFloatingSidebarHide() {
    if (!sidebarAutoHide_ || sidebarHideTimer_ == nullptr || sidebarTabs_ == nullptr) {
        return;
    }

    const QPoint globalPos = QCursor::pos();
    if (sidebarTabs_->rect().contains(sidebarTabs_->mapFromGlobal(globalPos))) {
        sidebarHideTimer_->stop();
        return;
    }

    if (sidebarToggleButton_ != nullptr &&
        sidebarToggleButton_->rect().contains(sidebarToggleButton_->mapFromGlobal(globalPos))) {
        sidebarHideTimer_->stop();
        return;
    }

    sidebarHideTimer_->start();
}

void MainWindow::updateFloatingSidebarGeometry() {
    if (sidebarToggleButton_ == nullptr || sidebarTabs_ == nullptr || sidebarTabs_->tabBar() == nullptr) {
        return;
    }

    QTabBar* bar = sidebarTabs_->tabBar();
    const int toggleX = std::max(0, (bar->width() - sidebarToggleButton_->width()) / 2);
    const int toggleY = 6;
    sidebarToggleButton_->move(toggleX, toggleY);
    sidebarToggleButton_->raise();
    sidebarToggleButton_->setParent(bar);
    sidebarToggleButton_->show();
}

void MainWindow::onStartLocalTerminal() {
    if (workspaceTabs_ == nullptr) {
        return;
    }

    auto* terminal = new QTermWidget(0, workspaceTabs_);
    terminal->setTerminalFont(chooseMonospaceUiFont());
    terminal->setColorScheme("Linux");
    terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    terminal->setAutoClose(false);

    const QString shellProgram = qEnvironmentVariable("SHELL", "/bin/bash");
    terminal->setShellProgram(shellProgram);
    terminal->setArgs(QStringList());
    terminal->startShellProgram();

    ++localTerminalCount_;
    const QString tabTitle = QString("%1. %2").arg(workspaceTabs_->count() + 1).arg(QDir::homePath());
    const QIcon terminalIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
    const int index = workspaceTabs_->addTab(terminal, terminalIcon, tabTitle);
    workspaceTabs_->setCurrentIndex(index);
    rebuildWorkspaceStripTabs();
}

void MainWindow::updateSidebarToggleButton() {
    if (sidebarToggleButton_ == nullptr) {
        return;
    }

    if (sidebarAutoHide_) {
        sidebarToggleButton_->setIcon(QIcon());
        sidebarToggleButton_->setText(">>");
        sidebarToggleButton_->setToolTip("Sidebar: Auto-hide (hover to open, click to pin)");
    } else {
        sidebarToggleButton_->setIcon(QIcon());
        sidebarToggleButton_->setText("<<");
        sidebarToggleButton_->setToolTip("Sidebar: Pinned (click to auto-hide)");
    }
}

void MainWindow::applySidebarVisualState(bool expanded) {
    if (leftHeaderRow_ != nullptr) {
        leftHeaderRow_->setVisible(false);
    }

    if (sidebarTabs_ == nullptr) {
        return;
    }

    if (QStackedWidget* stack = sidebarTabs_->findChild<QStackedWidget*>(); stack != nullptr) {
        stack->setVisible(expanded);
    }

    if (sessionsTree_ != nullptr) {
        sessionsTree_->setMinimumWidth(expanded ? 220 : 0);
    }

    sidebarTabs_->setMinimumWidth(expanded ? 220 : 30);
    sidebarTabs_->setMaximumWidth(expanded ? QWIDGETSIZE_MAX : 30);

    if (expanded) {
        sidebarTabs_->setStyleSheet(
            "QTabBar::tab:first { margin-top: 42px; }"
            "QTabBar::tab {"
            " min-width: 24px;"
            " min-height: 96px;"
            " padding: 6px 3px;"
            " color: #1f2a3a;"
            " background: #e5ebf4;"
            " border: 1px solid #c3cddd;"
            " border-right: 0;"
            "}"
            "QTabBar::tab:selected { background: #f8fbff; color: #0f274d; }"
            "QTabWidget::pane { border: 1px solid #bcc7d6; background: #f6f9fd; }"
        );
        return;
    }

    sidebarTabs_->setStyleSheet(
        "QTabBar::tab:first { margin-top: 42px; }"
        "QTabBar::tab {"
        " min-width: 24px;"
        " min-height: 96px;"
        " padding: 6px 3px;"
        " color: #1f2a3a;"
        " background: #e5ebf4;"
        " border: 1px solid #c3cddd;"
        "}"
        "QTabBar::tab:selected { background: #f8fbff; color: #0f274d; }"
        "QTabWidget::pane { border: 0; background: transparent; margin: 0; padding: 0; }"
    );
}

void MainWindow::populateSessionsTree() {
    sessionsTree_->clear();

    sessionsTree_->setHeaderHidden(true);

    folderPaths_.clear();
    for (const QString& folderPath : sessionRepository_.loadFolderPaths()) {
        folderPaths_.insert(folderPath);
    }
    folderPaths_.insert("User sessions");

    sessionsRootItem_ = new QTreeWidgetItem(sessionsTree_);
    sessionsRootItem_->setText(0, "User sessions");
    sessionsRootItem_->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
    sessionsRootItem_->setData(0, RoleIsFolder, true);
    sessionsRootItem_->setData(0, RoleFolderPath, "User sessions");
    sessionsRootItem_->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);

    QHash<QString, QTreeWidgetItem*> folderNodeByPath;
    folderNodeByPath.insert("User sessions", sessionsRootItem_);

    for (const QString& folderPath : folderPaths_) {
        if (folderPath.isEmpty() || folderPath == "User sessions") {
            continue;
        }

        const QStringList parts = folderPath.split('/', Qt::SkipEmptyParts);
        QString currentPath;
        QTreeWidgetItem* current = sessionsRootItem_;
        for (const QString& part : parts) {
            if (part == "User sessions") {
                currentPath = "User sessions";
                continue;
            }

            currentPath = currentPath.isEmpty() ? part : QString("%1/%2").arg(currentPath, part);
            if (currentPath.startsWith("User sessions/") == false) {
                currentPath = QString("User sessions/%1").arg(currentPath);
            }

            if (folderNodeByPath.contains(currentPath)) {
                current = folderNodeByPath.value(currentPath);
                continue;
            }

            auto* child = new QTreeWidgetItem(current);
            child->setText(0, part);
            child->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            child->setData(0, RoleIsFolder, true);
            child->setData(0, RoleFolderPath, currentPath);
            child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            folderNodeByPath.insert(currentPath, child);
            current = child;
        }
    }

    for (const domain::SessionProfile& profile : sessionRepository_.loadAll()) {
        const QString folderPath = profile.folderPath.isEmpty() ? "User sessions" : profile.folderPath;
        QTreeWidgetItem* parent = folderNodeByPath.value(folderPath, sessionsRootItem_);
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, profile.favorite ? QString("* %1").arg(profile.name) : profile.name);
        item->setIcon(0, profile.kind == domain::SessionKind::Ssh
                             ? style()->standardIcon(QStyle::SP_DriveNetIcon)
                             : style()->standardIcon(QStyle::SP_DesktopIcon));
        item->setData(0, RoleSessionId, profile.id);
        item->setData(0, RoleIsFolder, false);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    }

    sessionsTree_->expandAll();
}

bool MainWindow::isFolderItem(const QTreeWidgetItem* item) const {
    return item != nullptr && item->data(0, RoleIsFolder).toBool();
}

QString MainWindow::folderPathForItem(const QTreeWidgetItem* item) const {
    if (item == nullptr) {
        return "User sessions";
    }

    if (isFolderItem(item)) {
        const QString path = item->data(0, RoleFolderPath).toString();
        return path.isEmpty() ? "User sessions" : path;
    }

    const QTreeWidgetItem* parent = item->parent();
    if (parent == nullptr) {
        return "User sessions";
    }

    const QString path = parent->data(0, RoleFolderPath).toString();
    return path.isEmpty() ? "User sessions" : path;
}

QString MainWindow::folderPathFromTreeItem(QTreeWidgetItem* folderItem) const {
    if (folderItem == nullptr || !isFolderItem(folderItem)) {
        return "User sessions";
    }

    QStringList parts;
    QTreeWidgetItem* current = folderItem;
    while (current != nullptr) {
        const QString name = current->text(0).trimmed();
        if (!name.isEmpty()) {
            parts.prepend(name);
        }
        current = current->parent();
    }

    if (parts.isEmpty()) {
        return "User sessions";
    }
    if (parts.first() != "User sessions") {
        parts.prepend("User sessions");
    }
    return parts.join('/');
}

void MainWindow::refreshFolderRolesFromTree() {
    if (sessionsRootItem_ == nullptr) {
        return;
    }

    sessionsRootItem_->setData(0, RoleFolderPath, "User sessions");

    QList<QTreeWidgetItem*> queue;
    for (int i = 0; i < sessionsRootItem_->childCount(); ++i) {
        queue.append(sessionsRootItem_->child(i));
    }

    while (!queue.isEmpty()) {
        QTreeWidgetItem* item = queue.takeFirst();
        if (item == nullptr) {
            continue;
        }

        if (isFolderItem(item)) {
            item->setData(0, RoleFolderPath, folderPathFromTreeItem(item));
        }

        for (int i = 0; i < item->childCount(); ++i) {
            queue.append(item->child(i));
        }
    }
}

void MainWindow::persistTreeReorder() {
    if (sessionsTree_ == nullptr || sessionsRootItem_ == nullptr) {
        return;
    }

    struct FolderChange {
        QString oldPath;
        QString newPath;
    };

    QList<FolderChange> folderChanges;
    QList<QTreeWidgetItem*> scanQueue;
    for (int i = 0; i < sessionsRootItem_->childCount(); ++i) {
        scanQueue.append(sessionsRootItem_->child(i));
    }

    while (!scanQueue.isEmpty()) {
        QTreeWidgetItem* item = scanQueue.takeFirst();
        if (item == nullptr) {
            continue;
        }

        if (isFolderItem(item)) {
            const QString oldPath = item->data(0, RoleFolderPath).toString();
            const QString newPath = folderPathFromTreeItem(item);
            if (!oldPath.isEmpty() && oldPath != "User sessions" && oldPath != newPath) {
                folderChanges.push_back({oldPath, newPath});
            }
            item->setData(0, RoleFolderPath, newPath);
        }

        for (int i = 0; i < item->childCount(); ++i) {
            scanQueue.append(item->child(i));
        }
    }

    std::sort(folderChanges.begin(), folderChanges.end(), [](const FolderChange& lhs, const FolderChange& rhs) {
        return lhs.oldPath.count('/') < rhs.oldPath.count('/');
    });

    QStringList movedFolderRoots;
    for (const FolderChange& change : folderChanges) {
        bool skipAsChildMove = false;
        for (const QString& movedRoot : movedFolderRoots) {
            if (change.oldPath.startsWith(QString("%1/").arg(movedRoot))) {
                skipAsChildMove = true;
                break;
            }
        }
        if (skipAsChildMove) {
            continue;
        }

        const int slashIndex = change.newPath.lastIndexOf('/');
        const QString destinationParent = slashIndex > 0 ? change.newPath.left(slashIndex) : "User sessions";
        if (sessionRepository_.moveFolder(change.oldPath, destinationParent)) {
            movedFolderRoots.append(change.oldPath);
        }
    }

    QList<QTreeWidgetItem*> queue;
    for (int i = 0; i < sessionsRootItem_->childCount(); ++i) {
        queue.append(sessionsRootItem_->child(i));
    }

    folderPaths_.clear();
    folderPaths_.insert("User sessions");

    while (!queue.isEmpty()) {
        QTreeWidgetItem* item = queue.takeFirst();
        if (item == nullptr) {
            continue;
        }

        if (isFolderItem(item)) {
            const QString path = item->data(0, RoleFolderPath).toString();
            if (!path.isEmpty()) {
                sessionRepository_.ensureFolder(path);
                folderPaths_.insert(path);
            }
        } else {
            const QString id = item->data(0, RoleSessionId).toString();
            if (!id.isEmpty()) {
                const QString targetFolder = folderPathForItem(item);
                sessionRepository_.moveSessionToFolder(id, targetFolder);
            }
        }

        for (int i = 0; i < item->childCount(); ++i) {
            queue.append(item->child(i));
        }
    }

    populateSessionsTree();
}

void MainWindow::deleteTreeItem(QTreeWidgetItem* item) {
    if (item == nullptr || item == sessionsRootItem_) {
        return;
    }

    if (isFolderItem(item)) {
        const QString folderPath = item->data(0, RoleFolderPath).toString();
        const int result = QMessageBox::question(this,
                                                 "Delete folder",
                                                 QString("Delete folder '%1' and all contained sessions?").arg(item->text(0)));
        if (result != QMessageBox::Yes) {
            return;
        }

        if (!sessionRepository_.deleteFolder(folderPath)) {
            statusBar()->showMessage("Unable to delete folder", 2500);
            return;
        }
        populateSessionsTree();
        return;
    }

    const QString id = item->data(0, RoleSessionId).toString();
    if (id.isEmpty()) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             "Delete session",
                                             QString("Delete session '%1'?").arg(item->text(0)));
    if (result != QMessageBox::Yes) {
        return;
    }

    if (!sessionRepository_.deleteSession(id)) {
        statusBar()->showMessage("Unable to delete session", 2500);
        return;
    }
    populateSessionsTree();
}

void MainWindow::moveTreeItemToFolder(QTreeWidgetItem* item, const QString& targetFolderPath) {
    if (item == nullptr || targetFolderPath.trimmed().isEmpty()) {
        return;
    }

    if (isFolderItem(item)) {
        const QString sourcePath = item->data(0, RoleFolderPath).toString();
        if (!sessionRepository_.moveFolder(sourcePath, targetFolderPath)) {
            statusBar()->showMessage("Unable to move folder", 2500);
            return;
        }
        populateSessionsTree();
        return;
    }

    const QString id = item->data(0, RoleSessionId).toString();
    if (id.isEmpty()) {
        return;
    }

    if (!sessionRepository_.moveSessionToFolder(id, targetFolderPath)) {
        statusBar()->showMessage("Unable to move session", 2500);
        return;
    }
    populateSessionsTree();
}

void MainWindow::setupSessionsContextMenu() {
    if (sessionsTree_ == nullptr) {
        return;
    }

    sessionsTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sessionsTree_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = sessionsTree_->itemAt(pos);
        const QString targetFolderPath = folderPathForItem(item);

        QMenu menu(this);
        QAction* newFolder = menu.addAction("New folder");
        QAction* newSsh = menu.addAction("New SSH session");
        QAction* newVnc = menu.addAction("New VNC session");
        QAction* moveToFolder = menu.addAction("Move to folder...");
        QAction* deleteItem = menu.addAction("Delete");

        menu.addSeparator();
        QAction* editSelected = menu.addAction("Edit selected session");
        if (item == nullptr || isFolderItem(item)) {
            editSelected->setEnabled(false);
        }
        moveToFolder->setEnabled(item != nullptr && item != sessionsRootItem_);
        deleteItem->setEnabled(item != nullptr && item != sessionsRootItem_);

        QAction* chosen = menu.exec(sessionsTree_->viewport()->mapToGlobal(pos));
        if (chosen == nullptr) {
            return;
        }

        if (chosen == newFolder) {
            bool ok = false;
            const QString folderName = QInputDialog::getText(this,
                                                             "New folder",
                                                             "Folder name:",
                                                             QLineEdit::Normal,
                                                             QString(),
                                                             &ok)
                                           .trimmed();
            if (!ok || folderName.isEmpty()) {
                return;
            }

            const QString fullPath = QString("%1/%2").arg(targetFolderPath, folderName);
            if (!sessionRepository_.ensureFolder(fullPath)) {
                statusBar()->showMessage("Unable to create folder", 2500);
                return;
            }
            folderPaths_.insert(fullPath);
            populateSessionsTree();
            return;
        }

        if (chosen == newSsh) {
            openSessionEditor(domain::SessionKind::Ssh, QString(), targetFolderPath);
            return;
        }

        if (chosen == newVnc) {
            openSessionEditor(domain::SessionKind::Vnc, QString(), targetFolderPath);
            return;
        }

        if (chosen == editSelected) {
            onEditSelectedSession();
            return;
        }

        if (chosen == moveToFolder) {
            const QStringList folders = sessionRepository_.loadFolderPaths();
            bool ok = false;
            const QString selectedFolder = QInputDialog::getItem(this,
                                                                 "Move item",
                                                                 "Target folder:",
                                                                 folders,
                                                                 0,
                                                                 false,
                                                                 &ok);
            if (!ok || selectedFolder.trimmed().isEmpty()) {
                return;
            }

            moveTreeItemToFolder(item, selectedFolder);
            return;
        }

        if (chosen == deleteItem) {
            deleteTreeItem(item);
        }
    });
}

void MainWindow::createStatusAndKeyBar() {
    connectionStateLabel_ = new QLabel("Ready");
    statusBar()->addPermanentWidget(connectionStateLabel_);
    statusBar()->hide();
}

void MainWindow::onQuickConnect() {
    const QString endpoint = quickConnectEdit_->text().trimmed();
    if (endpoint.isEmpty()) {
        statusBar()->showMessage("Quick connect field is empty", 2500);
        return;
    }

    const domain::SessionProfile profile = makeProfileFromEndpoint(endpoint);
    openProfileTab(profile);
}

void MainWindow::onNewSshSession() {
    openSessionEditor(domain::SessionKind::Ssh, QString(), "User sessions");
}

void MainWindow::onNewVncSession() {
    openSessionEditor(domain::SessionKind::Vnc, QString(), "User sessions");
}

void MainWindow::onEditSelectedSession() {
    const QTreeWidgetItem* selected = sessionsTree_->currentItem();
    if (selected == nullptr) {
        statusBar()->showMessage("Select a session to edit", 2500);
        return;
    }

    const QString id = selected->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) {
        statusBar()->showMessage("Select a concrete session item", 2500);
        return;
    }

    if (domain::SessionProfile* profile = findProfileById(id); profile != nullptr) {
        openSessionEditor(profile->kind, id);
    }
}

domain::SessionProfile MainWindow::makeProfileFromEndpoint(const QString& endpoint) const {
    domain::SessionProfile profile;
    profile.id = createProfileId();
    profile.name = endpoint;
    profile.favorite = false;
    profile.auth.password = std::nullopt;
    profile.auth.privateKeyPath = std::nullopt;
    profile.tunnel.enabled = false;
    profile.tunnel.jumpPort = 22;

    if (endpoint.startsWith("vnc://")) {
        const QString value = endpoint.mid(6);
        const QStringList parts = value.split(':');
        profile.kind = domain::SessionKind::Vnc;
        profile.host = parts.value(0);
        profile.port = parts.size() > 1 ? parts.value(1).toInt() : 5900;
        return profile;
    }

    profile.kind = domain::SessionKind::Ssh;

    QString userHostPort = endpoint;
    if (endpoint.contains('@')) {
        const QStringList split = endpoint.split('@');
        profile.auth.username = split.value(0);
        userHostPort = split.value(1);
    } else {
        profile.auth.username = qEnvironmentVariable("USER");
    }

    const QStringList hostPort = userHostPort.split(':');
    profile.host = hostPort.value(0);
    profile.port = hostPort.size() > 1 ? hostPort.value(1).toInt() : 22;
    return profile;
}

QString MainWindow::makeTabTitle(const domain::SessionProfile& profile) const {
    return profile.kind == domain::SessionKind::Ssh
        ? QString("SSH:%1").arg(profile.name)
        : QString("VNC:%1").arg(profile.name);
}

void MainWindow::openProfileTab(const domain::SessionProfile& profile) {
    QWidget* content = nullptr;
    if (profile.kind == domain::SessionKind::Ssh) {
        content = new SshTerminalWidget(profile, workspaceTabs_);
    } else {
        content = new VncSessionWidget(profile, workspaceTabs_);
    }

    const int index = workspaceTabs_->addTab(content, makeTabTitle(profile));
    workspaceTabs_->setCurrentIndex(index);
    rebuildWorkspaceStripTabs();
    connectionStateLabel_->setText("Active");
}

void MainWindow::openSessionEditor(domain::SessionKind defaultKind,
                                   const QString& existingId,
                                   const QString& targetFolderPath) {
    SessionEditorDialog dialog(this);
    QString existingFolderPath = "User sessions";

    if (!existingId.isEmpty()) {
        if (domain::SessionProfile* profile = findProfileById(existingId); profile != nullptr) {
            existingFolderPath = profile->folderPath.isEmpty() ? "User sessions" : profile->folderPath;
            dialog.setProfile(*profile);
        }
    } else {
        domain::SessionProfile profile;
        profile.id = createProfileId();
        profile.kind = defaultKind;
        profile.port = defaultKind == domain::SessionKind::Ssh ? 22 : 5900;
        profile.auth.username = qEnvironmentVariable("USER");
        profile.auth.password = std::nullopt;
        profile.auth.privateKeyPath = std::nullopt;
        profile.tunnel.enabled = false;
        profile.tunnel.jumpPort = 22;
        dialog.setProfile(profile);
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    domain::SessionProfile profile = dialog.profile();
    if (profile.id.isEmpty()) {
        profile.id = createProfileId();
    }
    if (profile.name.isEmpty()) {
        profile.name = QString("%1-%2")
            .arg(profile.kind == domain::SessionKind::Ssh ? "ssh" : "vnc", profile.host);
    }

    const QString folderPath = targetFolderPath.isEmpty() ? existingFolderPath : targetFolderPath;
    profile.folderPath = folderPath.isEmpty() ? "User sessions" : folderPath;
    if (!sessionRepository_.ensureFolder(profile.folderPath)) {
        statusBar()->showMessage("Unable to prepare session folder", 2500);
        return;
    }

    sessionRepository_.upsert(profile);
    folderPaths_.insert(profile.folderPath);
    populateSessionsTree();
    openProfileTab(profile);
}

domain::SessionProfile* MainWindow::findProfileById(const QString& id) {
    return sessionRepository_.findById(id);
}

QString MainWindow::createProfileId() const {
    return QString("session-%1").arg(QDateTime::currentMSecsSinceEpoch());
}
