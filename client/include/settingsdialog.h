#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setServerInfo(const QString& ip, quint16 port, const QString& username);

    QString serverIp() const { return m_serverIp; }
    quint16 port() const { return m_port; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }
    bool rememberPassword() const { return m_rememberPassword; }

private slots:
    void onBtnBrowseClicked();
    void onOkClicked();

private:
    Ui::SettingsDialog *ui;
    QString m_serverIp;
    quint16 m_port;
    QString m_username;
    QString m_password;
    bool m_rememberPassword;

    void loadSettings();
    void saveSettings();
};

#endif // SETTINGSDIALOG_H
