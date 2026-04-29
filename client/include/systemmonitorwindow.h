#ifndef SYSTEMMONITORWINDOW_H
#define SYSTEMMONITORWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "networkmanager.h"

namespace Ui {
class SystemMonitorWindow;
}

class SystemMonitorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SystemMonitorWindow(NetworkManager* network, QWidget *parent = nullptr);
    ~SystemMonitorWindow();

private slots:
    void onSysInfoResult(const SysInfo& info);
    void onProcListResult(const QList<ProcessInfo>& processes);

    void onBtnStartClicked();
    void onBtnStopClicked();
    void onBtnRefreshClicked();
    void onIntervalChanged(int value);

private:
    Ui::SystemMonitorWindow *ui;
    NetworkManager* m_network;
    QTimer* m_updateTimer;
    bool m_isMonitoring;

    void updateSysInfo(const SysInfo& info);
    void updateProcList(const QList<ProcessInfo>& processes);
    void refresh();
    QString formatSize(quint32 size);
    QString formatUptime(quint32 seconds);
};

#endif // SYSTEMMONITORWINDOW_H