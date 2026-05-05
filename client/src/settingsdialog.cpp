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

    // 设置对话框背景色
    setStyleSheet("QDialog { background-color: #FAFAFA; }");

    // 输入框样式
    QString inputStyle = R"(
        QLineEdit {
            padding: 8px;
            border: 2px solid #B0BEC5;
            border-radius: 6px;
            background-color: white;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 2px solid #607D8B;
        }
    )";
    ui->editServerIp->setStyleSheet(inputStyle);
    ui->editPort->setStyleSheet(inputStyle);
    ui->editUsername->setStyleSheet(inputStyle);
    ui->editPassword->setStyleSheet(inputStyle);
    ui->editDownloadPath->setStyleSheet(inputStyle);

    // SpinBox 样式
    ui->spinRefreshInterval->setStyleSheet(R"(
        QSpinBox {
            padding: 6px;
            border: 2px solid #B0BEC5;
            border-radius: 6px;
            background-color: white;
        }
        QSpinBox:focus {
            border: 2px solid #607D8B;
        }
    )");
    ui->spinTimeout->setStyleSheet(ui->spinRefreshInterval->styleSheet());

    // 按钮样式
    ui->btnBrowse->setStyleSheet(R"(
        QPushButton {
            background-color: #ECEFF1;
            color: #455A64;
            border-radius: 6px;
            padding: 6px 12px;
            border: 1px solid #B0BEC5;
        }
        QPushButton:hover {
            background-color: #CFD8DC;
        }
        QPushButton:pressed {
            background-color: #B0BEC5;
        }
    )");

    // 复选框样式
    ui->checkRemember->setStyleSheet(R"(
        QCheckBox {
            padding: 5px;
            color: #455A64;
            font-weight: bold;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
        }
    )");

    // TabWidget 样式
    ui->tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #B0BEC5;
            border-radius: 8px;
            background-color: white;
            padding: 10px;
        }
        QTabBar::tab {
            background-color: #ECEFF1;
            padding: 10px 20px;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
            color: #455A64;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background-color: #607D8B;
            color: white;
        }
        QTabBar::tab:hover:!selected {
            background-color: #CFD8DC;
        }
        QGroupBox {
            border: 2px solid #B0BEC5;
            border-radius: 8px;
            margin-top: 10px;
            font-weight: bold;
            color: #455A64;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 2px 10px;
            background-color: #FAFAFA;
        }
    )");

    // 按钮盒样式
    ui->buttonBox->setStyleSheet(R"(
        QDialogButtonBox {
            button-layout: 5;
        }
        QDialogButtonBox QPushButton {
            padding: 8px 20px;
            border-radius: 6px;
            min-width: 80px;
        }
        QDialogButtonBox QPushButton[objectName="ok"] {
            background-color: #607D8B;
            color: white;
        }
        QDialogButtonBox QPushButton[objectName="ok"]:hover {
            background-color: #455A64;
        }
        QDialogButtonBox QPushButton[objectName="cancel"] {
            background-color: #ECEFF1;
            color: #455A64;
        }
        QDialogButtonBox QPushButton[objectName="cancel"]:hover {
            background-color: #CFD8DC;
        }
    )");

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
