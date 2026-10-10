#include "SessionEditorDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSshProtocolId = 0;
constexpr int kVncProtocolId = 1;

QToolButton* createProtocolButton(QWidget* parent, const QString& text, const QIcon& icon) {
    auto* button = new QToolButton(parent);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIcon(icon);
    button->setIconSize(QSize(28, 28));
    button->setText(text);
    button->setMinimumSize(90, 64);
    return button;
}

QWidget* addScrollableTab(QTabWidget* tabs, const QString& title) {
    auto* container = new QWidget(tabs);
    auto* scroll = new QScrollArea(tabs);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(container);
    tabs->addTab(scroll, title);
    return container;
}

}  // namespace

SessionEditorDialog::SessionEditorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Session settings");
    resize(620, 540);
    setStyleSheet(
        "QDialog { background: #efefef; }"
        "QGroupBox {"
        " border: 1px solid #b9b9b9;"
        " margin-top: 10px;"
        " padding-top: 8px;"
        " background: #efefef;"
        "}"
        "QGroupBox::title {"
        " subcontrol-origin: margin;"
        " left: 8px;"
        " padding: 0 4px;"
        " color: #202020;"
        "}"
    );
    buildUi();
}

void SessionEditorDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* selectorRow = new QWidget(this);
    auto* selectorLayout = new QHBoxLayout(selectorRow);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->setSpacing(10);

    protocolButtons_ = new QButtonGroup(this);
    protocolButtons_->setExclusive(true);

    sshButton_ = createProtocolButton(selectorRow, "SSH", style()->standardIcon(QStyle::SP_DriveNetIcon));
    vncButton_ = createProtocolButton(selectorRow, "VNC", style()->standardIcon(QStyle::SP_DesktopIcon));

    protocolButtons_->addButton(sshButton_, kSshProtocolId);
    protocolButtons_->addButton(vncButton_, kVncProtocolId);

    selectorLayout->addWidget(sshButton_);
    selectorLayout->addWidget(vncButton_);
    selectorLayout->addStretch(1);

    selectorRow->setStyleSheet(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "}"
        "QToolButton:checked {"
        "  border: 1px solid palette(highlight);"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
    );

    protocolStack_ = new QStackedWidget(this);

    sshPage_ = new QWidget(protocolStack_);
    auto* sshLayout = new QVBoxLayout(sshPage_);
    sshLayout->setContentsMargins(0, 0, 0, 0);
    sshLayout->setSpacing(10);

    auto* sshBasicBox = new QGroupBox("Basic Ssh settings", sshPage_);
    auto* sshBasicLayout = new QFormLayout(sshBasicBox);

    sshNameEdit_ = new QLineEdit(sshBasicBox);
    sshHostEdit_ = new QLineEdit(sshBasicBox);
    sshPortSpin_ = new QSpinBox(sshBasicBox);
    sshPortSpin_->setRange(1, 65535);
    sshPortSpin_->setValue(22);
    sshUsernameEdit_ = new QLineEdit(sshBasicBox);
    sshFavoriteCheck_ = new QCheckBox("Favorite", sshBasicBox);

    sshBasicLayout->addRow("Name", sshNameEdit_);
    sshBasicLayout->addRow("Remote hostname or IP address *", sshHostEdit_);
    sshBasicLayout->addRow("Port", sshPortSpin_);
    sshBasicLayout->addRow("Username", sshUsernameEdit_);
    sshBasicLayout->addRow("", sshFavoriteCheck_);

    auto* sshAdvancedContent = new QWidget(sshPage_);
    auto* sshAdvancedLayout = new QFormLayout(sshAdvancedContent);
    sshPasswordEdit_ = new QLineEdit(sshAdvancedContent);
    sshPasswordEdit_->setEchoMode(QLineEdit::Password);
    sshAdvancedLayout->addRow("Password", sshPasswordEdit_);
    auto* sshAdvancedNote = new QLabel("Additional SSH options will be added here.", sshAdvancedContent);
    sshAdvancedNote->setWordWrap(true);
    sshAdvancedLayout->addRow("", sshAdvancedNote);

    auto* sshTabs = new QTabWidget(sshPage_);
    sshTabs->setDocumentMode(true);
    sshTabs->addTab(sshAdvancedContent, "Advanced Ssh settings");

    auto* sshNetworkTab = new QWidget(sshTabs);
    auto* sshNetworkLayout = new QVBoxLayout(sshNetworkTab);
    sshNetworkLayout->addWidget(new QLabel("Ssh network options will be added here.", sshNetworkTab));
    sshNetworkLayout->addStretch(1);
    sshTabs->addTab(sshNetworkTab, "Network settings");

    auto* sshBookmarkTab = new QWidget(sshTabs);
    auto* sshBookmarkLayout = new QVBoxLayout(sshBookmarkTab);
    sshBookmarkLayout->addWidget(new QLabel("Ssh bookmark options will be added here.", sshBookmarkTab));
    sshBookmarkLayout->addStretch(1);
    sshTabs->addTab(sshBookmarkTab, "Bookmark settings");

    sshLayout->addWidget(sshBasicBox);
    sshLayout->addWidget(sshTabs);
    sshLayout->addStretch(1);

    vncPage_ = new QWidget(protocolStack_);
    auto* vncPageLayout = new QVBoxLayout(vncPage_);
    vncPageLayout->setContentsMargins(0, 0, 0, 0);
    vncPageLayout->setSpacing(10);

    auto* vncTabs = new QTabWidget(vncPage_);
    vncTabs->setDocumentMode(true);

    auto* vncBasicContent = addScrollableTab(vncTabs, "Basic");
    auto* vncBasicLayout = new QFormLayout(vncBasicContent);

    vncNameEdit_ = new QLineEdit(vncBasicContent);
    vncHostEdit_ = new QLineEdit(vncBasicContent);
    vncPortSpin_ = new QSpinBox(vncBasicContent);
    vncPortSpin_->setRange(1, 65535);
    vncPortSpin_->setValue(5900);
    vncPasswordEdit_ = new QLineEdit(vncBasicContent);
    vncPasswordEdit_->setEchoMode(QLineEdit::Password);
    vncUsernameEdit_ = new QLineEdit(vncBasicContent);
    vncRepeaterEdit_ = new QLineEdit(vncBasicContent);
    vncRepeaterEdit_->setPlaceholderText("10.10.10.12:5901");
    vncRepeaterEdit_->setToolTip("Connect through a VNC repeater endpoint (IP:port).");
    vncFavoriteCheck_ = new QCheckBox("Favorite", vncBasicContent);

    auto* keyboardMappingCombo = new QComboBox(vncBasicContent);
    keyboardMappingCombo->addItem("Auto");
    keyboardMappingCombo->addItem("us");
    keyboardMappingCombo->addItem("uk");
    keyboardMappingCombo->addItem("de");

    vncPresetCombo_ = new QComboBox(vncBasicContent);
    vncPresetCombo_->addItem("Poor (fastest)", 0);
    vncPresetCombo_->addItem("Medium", 1);
    vncPresetCombo_->addItem("Good", 2);
    vncPresetCombo_->addItem("Best (slowest)", 9);
    vncPresetCombo_->setCurrentIndex(2);

    vncDepthCombo_ = new QComboBox(vncBasicContent);
    vncDepthCombo_->addItem("True colour (32 bpp)", 32);
    vncDepthCombo_->addItem("High colour (16 bpp)", 16);
    vncDepthCombo_->addItem("256 colours (8 bpp)", 8);
    vncDepthCombo_->addItem("True colour (24 bpp)", 24);
    vncDepthCombo_->setCurrentIndex(0);

    vncBasicLayout->setContentsMargins(8, 8, 8, 8);
    vncBasicLayout->addRow("Name", vncNameEdit_);
    vncBasicLayout->addRow("Server *", vncHostEdit_);
    vncBasicLayout->addRow("Port", vncPortSpin_);
    vncBasicLayout->addRow("Repeater", vncRepeaterEdit_);
    vncBasicLayout->addRow("Username", vncUsernameEdit_);
    vncBasicLayout->addRow("User password", vncPasswordEdit_);
    vncBasicLayout->addRow("Color depth", vncDepthCombo_);
    vncBasicLayout->addRow("Quality", vncPresetCombo_);
    vncBasicLayout->addRow("Keyboard mapping", keyboardMappingCombo);
    vncBasicLayout->addRow("", vncFavoriteCheck_);

    auto* vncAdvancedContent = addScrollableTab(vncTabs, "Advanced");
    auto* vncAdvancedContentLayout = new QVBoxLayout(vncAdvancedContent);
    vncAdvancedContentLayout->setContentsMargins(8, 8, 8, 8);
    vncAdvancedContentLayout->setSpacing(10);

    vncOptionsBox_ = new QGroupBox("", vncAdvancedContent);
    auto* vncOptionsLayout = new QFormLayout(vncOptionsBox_);

    vncViewOnlyCheck_ = new QCheckBox("View only", vncOptionsBox_);
    vncShareDesktopCheck_ = new QCheckBox("Prevent local interaction on the server", vncOptionsBox_);
    vncEnableJpegCheck_ = new QCheckBox("Force tight encoding", vncOptionsBox_);
    vncRemoteCursorCheck_ = new QCheckBox("Show remote cursor", vncOptionsBox_);
    vncUsePresetCheck_ = new QCheckBox("Use quality pre-set", vncOptionsBox_);

    vncQualitySpin_ = new QSpinBox(vncOptionsBox_);
    vncQualitySpin_->setRange(0, 9);
    vncQualitySpin_->setValue(6);

    vncCompressSpin_ = new QSpinBox(vncOptionsBox_);
    vncCompressSpin_->setRange(0, 9);
    vncCompressSpin_->setValue(2);

    vncTimeoutSpin_ = new QSpinBox(vncOptionsBox_);
    vncTimeoutSpin_->setRange(0, 600);
    vncTimeoutSpin_->setValue(0);

    vncDisableEncryptionCheck_ = new QCheckBox("Turn off encryption", vncOptionsBox_);
    vncDisableClipboardCheck_ = new QCheckBox("Turn off clipboard sync", vncOptionsBox_);
    vncDynamicResolutionCheck_ = new QCheckBox("Disable smooth scrolling", vncOptionsBox_);

    vncAspectRatioSpin_ = new QDoubleSpinBox(vncOptionsBox_);
    vncAspectRatioSpin_->setRange(0.0, 4.0);
    vncAspectRatioSpin_->setSingleStep(0.01);
    vncAspectRatioSpin_->setDecimals(3);
    vncAspectRatioSpin_->setValue(0.0);
    vncAspectRatioSpin_->setToolTip("0 disables aspect lock. 1.777 is roughly 16:9.");

    vncEncodingsEdit_ = new QLineEdit(vncOptionsBox_);

    vncOptionsLayout->addRow("Override pre-set VNC encodings", vncEncodingsEdit_);
    vncOptionsLayout->addRow("Dynamic resolution enforced aspect ratio", vncAspectRatioSpin_);
    vncOptionsLayout->addRow("TCP_USER_TIMEOUT length (seconds)", vncTimeoutSpin_);
    vncOptionsLayout->addRow("", vncEnableJpegCheck_);
    vncOptionsLayout->addRow("", vncDynamicResolutionCheck_);
    vncOptionsLayout->addRow("", vncShareDesktopCheck_);
    vncOptionsLayout->addRow("", vncRemoteCursorCheck_);
    vncOptionsLayout->addRow("", vncDisableClipboardCheck_);
    vncOptionsLayout->addRow("", vncDisableEncryptionCheck_);
    vncOptionsLayout->addRow("", vncViewOnlyCheck_);

    vncShareDesktopCheck_->setChecked(true);
    vncEnableJpegCheck_->setChecked(true);
    vncRemoteCursorCheck_->setChecked(true);
    vncUsePresetCheck_->setChecked(true);
    vncQualitySpin_->setEnabled(false);
    vncCompressSpin_->setEnabled(false);

    connect(vncUsePresetCheck_, &QCheckBox::toggled, this, [this](const bool enabled) {
        vncPresetCombo_->setEnabled(enabled);
        vncQualitySpin_->setEnabled(!enabled);
        vncCompressSpin_->setEnabled(!enabled);
    });

    vncAdvancedContentLayout->addWidget(vncOptionsBox_);
    vncAdvancedContentLayout->addStretch(1);

    auto* vncBehaviorContent = addScrollableTab(vncTabs, "Behavior");
    auto* vncBehaviorLayout = new QFormLayout(vncBehaviorContent);
    auto* beforeCommandEdit = new QLineEdit(vncBehaviorContent);
    auto* afterCommandEdit = new QLineEdit(vncBehaviorContent);
    afterCommandEdit->setPlaceholderText("/path/to/command -opt1 arg ...");
    auto* autoStartCheck = new QCheckBox("Auto-start this profile", vncBehaviorContent);
    auto* requirePasswordCheck = new QCheckBox("Require password to connect or edit the profile", vncBehaviorContent);
    auto* keepWindowCheck = new QCheckBox("Keep window from closing if not disconnected by Remmina", vncBehaviorContent);

    vncBehaviorLayout->addRow("Before connecting", beforeCommandEdit);
    vncBehaviorLayout->addRow("After connecting", afterCommandEdit);
    vncBehaviorLayout->addRow("", autoStartCheck);
    vncBehaviorLayout->addRow("", requirePasswordCheck);
    vncBehaviorLayout->addRow("", keepWindowCheck);

    auto* vncSshTunnelContent = addScrollableTab(vncTabs, "SSH Tunnel");
    auto* vncSshTunnelLayout = new QFormLayout(vncSshTunnelContent);
    tunnelEnabledCheck_ = new QCheckBox("Enable SSH tunnel", vncSshTunnelContent);
    jumpHostEdit_ = new QLineEdit(vncSshTunnelContent);
    jumpPortSpin_ = new QSpinBox(vncSshTunnelContent);
    jumpPortSpin_->setRange(1, 65535);
    jumpPortSpin_->setValue(22);
    jumpUserEdit_ = new QLineEdit(vncSshTunnelContent);
    vncSshTunnelLayout->addRow("", tunnelEnabledCheck_);
    vncSshTunnelLayout->addRow("Jump host", jumpHostEdit_);
    vncSshTunnelLayout->addRow("Jump port", jumpPortSpin_);
    vncSshTunnelLayout->addRow("Jump username", jumpUserEdit_);

    auto* vncNotesContent = addScrollableTab(vncTabs, "Notes");
    auto* vncNotesLayout = new QVBoxLayout(vncNotesContent);
    auto* notesEdit = new QPlainTextEdit(vncNotesContent);
    notesEdit->setPlaceholderText("Session notes");
    notesEdit->setMinimumHeight(220);
    vncNotesLayout->addWidget(notesEdit);

    vncPageLayout->addWidget(vncTabs);
    vncPageLayout->addStretch(1);

    protocolStack_->addWidget(sshPage_);
    protocolStack_->addWidget(vncPage_);

    root->addWidget(selectorRow);
    root->addWidget(protocolStack_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(protocolButtons_, &QButtonGroup::idClicked, this, &SessionEditorDialog::onProtocolChanged);
    connect(tunnelEnabledCheck_, &QCheckBox::toggled, this, &SessionEditorDialog::onTunnelToggled);

    sshButton_->setChecked(true);
    onProtocolChanged(kSshProtocolId);
    onTunnelToggled(tunnelEnabledCheck_->isChecked());
}

void SessionEditorDialog::setProfile(const domain::SessionProfile& profile) {
    profileId_ = profile.id;

    sshNameEdit_->setText(profile.name);
    vncNameEdit_->setText(profile.name);
    sshHostEdit_->setText(profile.host);
    vncHostEdit_->setText(profile.host);
    sshPortSpin_->setValue(profile.kind == domain::SessionKind::Ssh ? profile.port : 22);
    vncPortSpin_->setValue(profile.kind == domain::SessionKind::Vnc ? profile.port : 5900);
    sshUsernameEdit_->setText(profile.auth.username);
    sshPasswordEdit_->setText(profile.auth.password.value_or(""));
    vncUsernameEdit_->setText(profile.vnc.username.isEmpty() ? profile.auth.username : profile.vnc.username);
    vncPasswordEdit_->setText(profile.auth.password.value_or(""));
    vncRepeaterEdit_->setText(profile.vnc.repeater);
    sshFavoriteCheck_->setChecked(profile.favorite);
    vncFavoriteCheck_->setChecked(profile.favorite);

    tunnelEnabledCheck_->setChecked(profile.tunnel.enabled);
    jumpHostEdit_->setText(profile.tunnel.jumpHost);
    jumpPortSpin_->setValue(profile.tunnel.jumpPort);
    jumpUserEdit_->setText(profile.tunnel.jumpUsername);

    vncViewOnlyCheck_->setChecked(profile.vnc.viewOnly);
    vncShareDesktopCheck_->setChecked(profile.vnc.shareDesktop);
    vncEnableJpegCheck_->setChecked(profile.vnc.enableJpeg);
    vncRemoteCursorCheck_->setChecked(profile.vnc.useRemoteCursor);
    vncUsePresetCheck_->setChecked(profile.vnc.useQualityPreset);

    const int presetIndex = vncPresetCombo_->findData(profile.vnc.qualityPreset);
    if (presetIndex >= 0) {
        vncPresetCombo_->setCurrentIndex(presetIndex);
    }

    const int depthIndex = vncDepthCombo_->findData(profile.vnc.requestedDepth);
    if (depthIndex >= 0) {
        vncDepthCombo_->setCurrentIndex(depthIndex);
    }
    vncQualitySpin_->setValue(profile.vnc.qualityLevel);
    vncCompressSpin_->setValue(profile.vnc.compressLevel);
    vncTimeoutSpin_->setValue(profile.vnc.connectTimeoutSeconds);
    vncDisableEncryptionCheck_->setChecked(profile.vnc.disableEncryption);
    vncDisableClipboardCheck_->setChecked(profile.vnc.disableClipboard);
    vncDynamicResolutionCheck_->setChecked(profile.vnc.dynamicResolution);
    vncAspectRatioSpin_->setValue(profile.vnc.aspectRatioLock);
    vncEncodingsEdit_->setText(profile.vnc.encodings);

    const int protocolId = profile.kind == domain::SessionKind::Ssh ? kSshProtocolId : kVncProtocolId;
    protocolButtons_->button(protocolId)->setChecked(true);
    onProtocolChanged(protocolId);
    onTunnelToggled(tunnelEnabledCheck_->isChecked());
}

domain::SessionProfile SessionEditorDialog::profile() const {
    const bool isSsh = protocolButtons_->checkedId() != kVncProtocolId;
    const auto selectedKind = isSsh ? domain::SessionKind::Ssh : domain::SessionKind::Vnc;

    domain::SessionProfile profile;
    profile.id = profileId_;
    profile.name = isSsh ? sshNameEdit_->text().trimmed() : vncNameEdit_->text().trimmed();
    profile.kind = selectedKind;
    profile.host = isSsh ? sshHostEdit_->text().trimmed() : vncHostEdit_->text().trimmed();
    profile.port = isSsh ? sshPortSpin_->value() : vncPortSpin_->value();
    profile.auth.username = isSsh ? sshUsernameEdit_->text().trimmed() : vncUsernameEdit_->text().trimmed();

    const QString passwordValue = isSsh ? sshPasswordEdit_->text() : vncPasswordEdit_->text();
    if (passwordValue.isEmpty()) {
        profile.auth.password = std::nullopt;
    } else {
        profile.auth.password = passwordValue;
    }
    profile.auth.privateKeyPath = std::nullopt;
    profile.tunnel.enabled = tunnelEnabledCheck_->isChecked();
    profile.tunnel.jumpHost = jumpHostEdit_->text().trimmed();
    profile.tunnel.jumpPort = jumpPortSpin_->value();
    profile.tunnel.jumpUsername = jumpUserEdit_->text().trimmed();

    profile.vnc.viewOnly = vncViewOnlyCheck_->isChecked();
    profile.vnc.shareDesktop = vncShareDesktopCheck_->isChecked();
    profile.vnc.enableJpeg = vncEnableJpegCheck_->isChecked();
    profile.vnc.useRemoteCursor = vncRemoteCursorCheck_->isChecked();
    profile.vnc.username = vncUsernameEdit_->text().trimmed();
    profile.vnc.repeater = vncRepeaterEdit_->text().trimmed();
    profile.vnc.qualityPreset = vncPresetCombo_->currentData().toInt();
    profile.vnc.useQualityPreset = vncUsePresetCheck_->isChecked();
    profile.vnc.requestedDepth = vncDepthCombo_->currentData().toInt();
    profile.vnc.qualityLevel = vncQualitySpin_->value();
    profile.vnc.compressLevel = vncCompressSpin_->value();
    profile.vnc.connectTimeoutSeconds = vncTimeoutSpin_->value();
    profile.vnc.disableEncryption = vncDisableEncryptionCheck_->isChecked();
    profile.vnc.disableClipboard = vncDisableClipboardCheck_->isChecked();
    profile.vnc.dynamicResolution = vncDynamicResolutionCheck_->isChecked();
    profile.vnc.aspectRatioLock = vncAspectRatioSpin_->value();
    profile.vnc.encodings = vncEncodingsEdit_->text().trimmed();

    profile.favorite = isSsh ? sshFavoriteCheck_->isChecked() : vncFavoriteCheck_->isChecked();
    return profile;
}

void SessionEditorDialog::onProtocolChanged(int index) {
    const bool isSsh = index == kSshProtocolId;

    if (isSsh) {
        if (sshNameEdit_->text().trimmed().isEmpty()) {
            sshNameEdit_->setText(vncNameEdit_->text());
        }
        if (sshHostEdit_->text().trimmed().isEmpty()) {
            sshHostEdit_->setText(vncHostEdit_->text());
        }
        if (sshPortSpin_->value() == 5900) {
            sshPortSpin_->setValue(22);
        }
        if (sshPasswordEdit_->text().isEmpty() && !vncPasswordEdit_->text().isEmpty()) {
            sshPasswordEdit_->setText(vncPasswordEdit_->text());
        }
        if (vncFavoriteCheck_->isChecked()) {
            sshFavoriteCheck_->setChecked(true);
        }
    } else {
        if (vncNameEdit_->text().trimmed().isEmpty()) {
            vncNameEdit_->setText(sshNameEdit_->text());
        }
        if (vncHostEdit_->text().trimmed().isEmpty()) {
            vncHostEdit_->setText(sshHostEdit_->text());
        }
        if (vncPortSpin_->value() == 22) {
            vncPortSpin_->setValue(5900);
        }
        if (vncPasswordEdit_->text().isEmpty() && !sshPasswordEdit_->text().isEmpty()) {
            vncPasswordEdit_->setText(sshPasswordEdit_->text());
        }
        if (sshFavoriteCheck_->isChecked()) {
            vncFavoriteCheck_->setChecked(true);
        }
    }

    protocolStack_->setCurrentIndex(isSsh ? 0 : 1);
    tunnelEnabledCheck_->setEnabled(!isSsh);
    vncOptionsBox_->setEnabled(!isSsh);
    if (isSsh) {
        tunnelEnabledCheck_->setChecked(false);
    }
}

void SessionEditorDialog::onTunnelToggled(bool enabled) {
    jumpHostEdit_->setEnabled(enabled);
    jumpPortSpin_->setEnabled(enabled);
    jumpUserEdit_->setEnabled(enabled);
}
