#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& error);
    void onLoginResult(bool success, int perm, const QString& message);

    void onBtnFileManagerClicked();
    void onBtnSystemMonitorClicked();
    void onBtnLogViewerClicked();
    void onBtnSettingsClicked();

    void onActionConnect();
    void onActionDisconnect();
    void onActionExit();

private:
    Ui::MainWindow *ui;
    NetworkManager* m_network;
    QString m_serverIp;
    quint16 m_serverPort;
    QString m_username;
    QLabel* m_labelStatus;
    QLabel* m_labelServer;

    void updateStatus();
    void showSettingsDialog();
    void showFileManager();
    void showSystemMonitor();
    void showLogViewer();
};

#endif // MAINWINDOW_H