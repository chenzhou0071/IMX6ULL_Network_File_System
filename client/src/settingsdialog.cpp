#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_port(8888)
    , m_rememberPassword(true)
{
    ui->setupUi(this);
    setWindowTitle("系统设置");

    loadSettings();

    // 信号连接
    connect(ui->btnBrowse, &QPushButton::clicked, this, &SettingsDialog::onBtnBrowseClicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onOkClicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setServerInfo(const QString& ip, quint16 port, const QString& username)
{
    m_serverIp = ip;
    m_port = port;
    m_username = username;

    ui->editServerIp->setText(ip);
    ui->editPort->setText(QString::number(port));
    ui->editUsername->setText(username);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("IMX6ULL", "Client", this);

    m_serverIp = settings.value("server/ip", "192.168.183.176").toString();
    m_port = settings.value("server/port", 8888).toUInt();
    m_username = settings.value("server/username", "admin").toString();
    m_password = settings.value("server/password", "").toString();
    m_rememberPassword = settings.value("server/remember", true).toBool();

    ui->editServerIp->setText(m_serverIp);
    ui->editPort->setText(QString::number(m_port));
    ui->editUsername->setText(m_username);
    ui->checkRemember->setChecked(m_rememberPassword);

    if (m_rememberPassword && !m_password.isEmpty()) {
        ui->editPassword->setText(m_password);
    }

    ui->spinRefreshInterval->setValue(settings.value("app/refreshInterval", 5).toInt());
    ui->editDownloadPath->setText(settings.value("app/downloadPath", "./downloads").toString());
    ui->spinTimeout->setValue(settings.value("app/timeout", 30).toInt());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("IMX6ULL", "Client", this);

    settings.setValue("server/ip", m_serverIp);
    settings.setValue("server/port", m_port);
    settings.setValue("server/username", m_username);
    settings.setValue("server/remember", m_rememberPassword);

    if (m_rememberPassword) {
        settings.setValue("server/password", m_password);
    } else {
        settings.remove("server/password");
    }

    settings.setValue("app/refreshInterval", ui->spinRefreshInterval->value());
    settings.setValue("app/downloadPath", ui->editDownloadPath->text());
    settings.setValue("app/timeout", ui->spinTimeout->value());
}

void SettingsDialog::onBtnBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择下载目录",
                                                   ui->editDownloadPath->text());
    if (!dir.isEmpty()) {
        ui->editDownloadPath->setText(dir);
    }
}

void SettingsDialog::onOkClicked()
{
    m_serverIp = ui->editServerIp->text();
    m_port = ui->editPort->text().toUInt();
    m_username = ui->editUsername->text();
    m_password = ui->editPassword->text();
    m_rememberPassword = ui->checkRemember->isChecked();

    saveSettings();
    accept();
}
