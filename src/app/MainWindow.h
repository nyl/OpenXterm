#pragma once

#include <QMainWindow>

#include <QHash>
#include <QSet>

#include "../domain/SessionRepository.h"

class QLabel;
class QLineEdit;
class QSplitter;
class QTabBar;
class QTabWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;
class QResizeEvent;
class QShowEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onQuickConnect();
    void onNewSshSession();
    void onNewVncSession();
    void onEditSelectedSession();
    void onStartLocalTerminal();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    [[nodiscard]] domain::SessionProfile makeProfileFromEndpoint(const QString& endpoint) const;
    [[nodiscard]] QString makeTabTitle(const domain::SessionProfile& profile) const;
    void openProfileTab(const domain::SessionProfile& profile);
    void openSessionEditor(domain::SessionKind defaultKind,
                           const QString& existingId = QString(),
                           const QString& targetFolderPath = QString());
    [[nodiscard]] domain::SessionProfile* findProfileById(const QString& id);
    [[nodiscard]] QString createProfileId() const;
    [[nodiscard]] bool isFolderItem(const QTreeWidgetItem* item) const;
    [[nodiscard]] QString folderPathForItem(const QTreeWidgetItem* item) const;
    void setupSessionsContextMenu();
    [[nodiscard]] QString folderPathFromTreeItem(QTreeWidgetItem* folderItem) const;
    void refreshFolderRolesFromTree();
    void persistTreeReorder();
    void deleteTreeItem(QTreeWidgetItem* item);
    void moveTreeItemToFolder(QTreeWidgetItem* item, const QString& targetFolderPath);

    void toggleSidebarAutoHide();
    void showFloatingSidebar();
    void hideFloatingSidebar();
    void scheduleFloatingSidebarHide();
    void updateFloatingSidebarGeometry();
    void updateSidebarToggleButton();
    void applySidebarVisualState(bool expanded);
    void updateWorkspaceStripLeftPadding();
    void rebuildWorkspaceStripTabs();

    void createMenus();
    void createTopToolbar();
    void createCentralLayout();
    void createStatusAndKeyBar();
    void populateSessionsTree();

    enum ItemRoles {
        RoleSessionId = Qt::UserRole,
        RoleIsFolder = Qt::UserRole + 1,
        RoleFolderPath = Qt::UserRole + 2,
    };

    QWidget* centralRoot_ = nullptr;
    QWidget* workspaceStrip_ = nullptr;
    QWidget* workspaceStripLeftPad_ = nullptr;
    QTabBar* workspaceStripTabs_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QWidget* leftShell_ = nullptr;
    QVBoxLayout* leftShellLayout_ = nullptr;
    QWidget* leftHeaderRow_ = nullptr;
    QTabWidget* sidebarTabs_ = nullptr;
    QLineEdit* quickConnectEdit_ = nullptr;
    QTreeWidget* sessionsTree_ = nullptr;
    QTreeWidgetItem* sessionsRootItem_ = nullptr;
    QTabWidget* workspaceTabs_ = nullptr;
    QLabel* connectionStateLabel_ = nullptr;
    QToolButton* sidebarToggleButton_ = nullptr;
    QTimer* sidebarHideTimer_ = nullptr;

    bool sidebarAutoHide_ = false;
    int lastSidebarWidth_ = 240;
    int localTerminalCount_ = 0;
    QSet<QString> folderPaths_;

    domain::SessionRepository sessionRepository_;
};
