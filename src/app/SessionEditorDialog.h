#pragma once

#include <QDialog>

#include "../domain/SessionProfile.h"

class QCheckBox;
class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class QWidget;

class SessionEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit SessionEditorDialog(QWidget* parent = nullptr);

    void setProfile(const domain::SessionProfile& profile);
    [[nodiscard]] domain::SessionProfile profile() const;

private slots:
    void onProtocolChanged(int index);
    void onTunnelToggled(bool enabled);

private:
    void buildUi();

    QButtonGroup* protocolButtons_ = nullptr;
    QToolButton* sshButton_ = nullptr;
    QToolButton* vncButton_ = nullptr;
    QStackedWidget* protocolStack_ = nullptr;

    QWidget* sshPage_ = nullptr;
    QLineEdit* sshNameEdit_ = nullptr;
    QLineEdit* sshHostEdit_ = nullptr;
    QSpinBox* sshPortSpin_ = nullptr;
    QLineEdit* sshUsernameEdit_ = nullptr;
    QLineEdit* sshPasswordEdit_ = nullptr;
    QCheckBox* sshFavoriteCheck_ = nullptr;

    QWidget* vncPage_ = nullptr;
    QLineEdit* vncNameEdit_ = nullptr;
    QLineEdit* vncHostEdit_ = nullptr;
    QSpinBox* vncPortSpin_ = nullptr;
    QLineEdit* vncUsernameEdit_ = nullptr;
    QLineEdit* vncPasswordEdit_ = nullptr;
    QLineEdit* vncRepeaterEdit_ = nullptr;
    QCheckBox* vncFavoriteCheck_ = nullptr;

    QCheckBox* tunnelEnabledCheck_ = nullptr;
    QLineEdit* jumpHostEdit_ = nullptr;
    QSpinBox* jumpPortSpin_ = nullptr;
    QLineEdit* jumpUserEdit_ = nullptr;

    QGroupBox* vncOptionsBox_ = nullptr;
    QCheckBox* vncViewOnlyCheck_ = nullptr;
    QCheckBox* vncShareDesktopCheck_ = nullptr;
    QCheckBox* vncEnableJpegCheck_ = nullptr;
    QCheckBox* vncRemoteCursorCheck_ = nullptr;
    QCheckBox* vncUsePresetCheck_ = nullptr;
    QComboBox* vncPresetCombo_ = nullptr;
    QCheckBox* vncDisableEncryptionCheck_ = nullptr;
    QCheckBox* vncDisableClipboardCheck_ = nullptr;
    QCheckBox* vncDynamicResolutionCheck_ = nullptr;
    QComboBox* vncDepthCombo_ = nullptr;
    QSpinBox* vncQualitySpin_ = nullptr;
    QSpinBox* vncCompressSpin_ = nullptr;
    QSpinBox* vncTimeoutSpin_ = nullptr;
    QDoubleSpinBox* vncAspectRatioSpin_ = nullptr;
    QLineEdit* vncEncodingsEdit_ = nullptr;

    QString profileId_;
};
