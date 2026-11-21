#include "settingsdialog.h"
#include "downloadmanager.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QLabel>
#include <QStandardPaths>

SettingsDialog::SettingsDialog(DownloadManager *downloadManager, QWidget *parent)
    : QDialog(parent)
    , m_downloadManager(downloadManager)
{
    setWindowTitle(tr("Preferences - WhatsApp Desktop"));
    setMinimumSize(500, 360);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabs = new QTabWidget(this);

    // Tab 1: General
    QWidget *generalTab = new QWidget(this);
    QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);

    QLabel *dirLabel = new QLabel(tr("Media & Attachments Download Folder:"), generalTab);
    generalLayout->addWidget(dirLabel);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_downloadDirEdit = new QLineEdit(generalTab);
    QPushButton *browseBtn = new QPushButton(tr("Browse..."), generalTab);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDownloadDir);
    dirLayout->addWidget(m_downloadDirEdit);
    dirLayout->addWidget(browseBtn);
    generalLayout->addLayout(dirLayout);

    m_autoDownloadCheck = new QCheckBox(tr("Automatically save incoming media to download folder without prompting"), generalTab);
    generalLayout->addWidget(m_autoDownloadCheck);

    generalLayout->addSpacing(10);
    m_startMinimizedCheck = new QCheckBox(tr("Start application minimized to system tray"), generalTab);
    generalLayout->addWidget(m_startMinimizedCheck);

    m_closeToTrayCheck = new QCheckBox(tr("Closing window minimizes to system tray instead of exiting"), generalTab);
    generalLayout->addWidget(m_closeToTrayCheck);

    generalLayout->addStretch();
    tabs->addTab(generalTab, tr("General"));

    // Tab 2: Appearance
    QWidget *appearanceTab = new QWidget(this);
    QFormLayout *appearanceLayout = new QFormLayout(appearanceTab);

    m_themeCombo = new QComboBox(appearanceTab);
    m_themeCombo->addItem(tr("Follow Desktop System Theme"), 0);
    m_themeCombo->addItem(tr("Dark Theme"), 1);
    m_themeCombo->addItem(tr("Light Theme"), 2);
    appearanceLayout->addRow(tr("Application Theme:"), m_themeCombo);

    m_zoomCombo = new QComboBox(appearanceTab);
    m_zoomCombo->addItem("80%", 0.8);
    m_zoomCombo->addItem("90%", 0.9);
    m_zoomCombo->addItem("100% (Default)", 1.0);
    m_zoomCombo->addItem("110%", 1.1);
    m_zoomCombo->addItem("125%", 1.25);
    m_zoomCombo->addItem("150%", 1.5);
    appearanceLayout->addRow(tr("Interface Zoom:"), m_zoomCombo);

    tabs->addTab(appearanceTab, tr("Appearance"));

    // Tab 3: Notifications
    QWidget *notificationsTab = new QWidget(this);
    QVBoxLayout *notifLayout = new QVBoxLayout(notificationsTab);

    m_notificationsCheck = new QCheckBox(tr("Enable native desktop notifications for messages and calls"), notificationsTab);
    notifLayout->addWidget(m_notificationsCheck);

    m_notificationPreviewCheck = new QCheckBox(tr("Show message preview and sender name in notifications"), notificationsTab);
    notifLayout->addWidget(m_notificationPreviewCheck);

    notifLayout->addStretch();
    tabs->addTab(notificationsTab, tr("Notifications"));

    mainLayout->addWidget(tabs);

    // Bottom action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    QPushButton *saveBtn = new QPushButton(tr("Save Preferences"), this);
    saveBtn->setDefault(true);
    saveBtn->setStyleSheet("QPushButton { background-color: #25D366; color: white; font-weight: bold; border-radius: 4px; padding: 6px 16px; }"
                           "QPushButton:hover { background-color: #20BA5C; }");

    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);

    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    mainLayout->addLayout(buttonLayout);

    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

QString SettingsDialog::getDownloadDirectory()
{
    QSettings settings;
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/WhatsApp";
    return settings.value("General/DownloadDirectory", defaultDir).toString();
}

bool SettingsDialog::getAutoDownload()
{
    QSettings settings;
    return settings.value("General/AutoDownload", true).toBool();
}

bool SettingsDialog::getStartMinimized()
{
    QSettings settings;
    return settings.value("General/StartMinimized", false).toBool();
}

bool SettingsDialog::getCloseToTray()
{
    QSettings settings;
    return settings.value("General/CloseToTray", true).toBool();
}

int SettingsDialog::getThemeMode()
{
    QSettings settings;
    return settings.value("Appearance/Theme", 0).toInt();
}

qreal SettingsDialog::getZoomFactor()
{
    QSettings settings;
    return settings.value("Appearance/ZoomFactor", 1.0).toReal();
}

bool SettingsDialog::getNotificationsEnabled()
{
    QSettings settings;
    return settings.value("Notifications/Enabled", true).toBool();
}

bool SettingsDialog::getShowNotificationPreview()
{
    QSettings settings;
    return settings.value("Notifications/ShowPreview", true).toBool();
}

void SettingsDialog::loadSettings()
{
    m_downloadDirEdit->setText(getDownloadDirectory());
    m_autoDownloadCheck->setChecked(getAutoDownload());
    m_startMinimizedCheck->setChecked(getStartMinimized());
    m_closeToTrayCheck->setChecked(getCloseToTray());

    int themeIndex = m_themeCombo->findData(getThemeMode());
    if (themeIndex >= 0) m_themeCombo->setCurrentIndex(themeIndex);

    qreal currentZoom = getZoomFactor();
    for (int i = 0; i < m_zoomCombo->count(); ++i) {
        if (qFuzzyCompare(m_zoomCombo->itemData(i).toReal(), currentZoom)) {
            m_zoomCombo->setCurrentIndex(i);
            break;
        }
    }

    m_notificationsCheck->setChecked(getNotificationsEnabled());
    m_notificationPreviewCheck->setChecked(getShowNotificationPreview());
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    QString newDir = m_downloadDirEdit->text().trimmed();
    if (!newDir.isEmpty()) {
        settings.setValue("General/DownloadDirectory", newDir);
        if (m_downloadManager) {
            m_downloadManager->setDownloadDirectory(newDir);
        }
    }

    bool autoDl = m_autoDownloadCheck->isChecked();
    settings.setValue("General/AutoDownload", autoDl);
    if (m_downloadManager) {
        m_downloadManager->setAutoDownloadEnabled(autoDl);
    }

    settings.setValue("General/StartMinimized", m_startMinimizedCheck->isChecked());
    settings.setValue("General/CloseToTray", m_closeToTrayCheck->isChecked());

    int themeMode = m_themeCombo->currentData().toInt();
    settings.setValue("Appearance/Theme", themeMode);

    qreal zoom = m_zoomCombo->currentData().toReal();
    settings.setValue("Appearance/ZoomFactor", zoom);

    settings.setValue("Notifications/Enabled", m_notificationsCheck->isChecked());
    settings.setValue("Notifications/ShowPreview", m_notificationPreviewCheck->isChecked());

    emit themeChanged(themeMode);
    emit zoomChanged(zoom);
    emit downloadSettingsChanged();
}

void SettingsDialog::onBrowseDownloadDir()
{
    QString selected = QFileDialog::getExistingDirectory(
        this,
        tr("Select Downloads Directory"),
        m_downloadDirEdit->text()
    );
    if (!selected.isEmpty()) {
        m_downloadDirEdit->setText(selected);
    }
}

void SettingsDialog::onSaveClicked()
{
    saveSettings();
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}
