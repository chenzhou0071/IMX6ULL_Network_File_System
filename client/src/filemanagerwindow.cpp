#include "filemanagerwindow.h"
#include "ui_filemanagerwindow.h"
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QInputDialog>
#include <QDirIterator>
#include <QFileInfo>

FileManagerWindow::FileManagerWindow(NetworkManager* network, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::FileManagerWindow)
    , m_network(network)
{
    ui->setupUi(this);
    setWindowTitle("文件管理器 - /");

    // 设置表格列宽
    ui->tableFiles->setColumnWidth(0, 300);
    ui->tableFiles->setColumnWidth(1, 100);
    ui->tableFiles->setColumnWidth(2, 150);

    // 连接信号
    connect(m_network, &NetworkManager::fileListResult, this, &FileManagerWindow::onFileListResult);
    connect(m_network, &NetworkManager::fileReadResult, this, &FileManagerWindow::onFileReadResult);
    connect(m_network, &NetworkManager::fileOperationResult, this, &FileManagerWindow::onFileOperationResult);
    connect(m_network, &NetworkManager::uploadProgress, this, &FileManagerWindow::onUploadProgress);
    connect(m_network, &NetworkManager::downloadProgress, this, &FileManagerWindow::onDownloadProgress);
    connect(m_network, &NetworkManager::transferComplete, this, &FileManagerWindow::onTransferComplete);

    connect(ui->btnBack, &QPushButton::clicked, this, &FileManagerWindow::onBtnBackClicked);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &FileManagerWindow::onBtnRefreshClicked);
    connect(ui->btnUpload, &QPushButton::clicked, this, &FileManagerWindow::onBtnUploadClicked);
    connect(ui->btnDownload, &QPushButton::clicked, this, &FileManagerWindow::onBtnDownloadClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &FileManagerWindow::onBtnDeleteClicked);
    connect(ui->btnView, &QPushButton::clicked, this, &FileManagerWindow::onBtnViewClicked);
    connect(ui->btnRename, &QPushButton::clicked, this, &FileManagerWindow::onBtnRenameClicked);

    connect(ui->tableFiles, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem* item) {
                onTableItemDoubleClicked(item->row(), item->column());
            });

    connect(ui->tableFiles, &QTableWidget::itemSelectionChanged,
            this, &FileManagerWindow::onTableSelectionChanged);

    // 加载文件列表
    loadFileList("/");
}

FileManagerWindow::~FileManagerWindow()
{
    delete ui;
}

void FileManagerWindow::loadFileList(const QString& path)
{
    m_currentPath = path;
    ui->editPath->setText(path);
    setWindowTitle(QString("文件管理器 - %1").arg(path));

    // 发送请求
    m_network->sendFileList(path);
}

void FileManagerWindow::onFileListResult(const QList<FileInfo>& files)
{
    m_files = files;

    ui->tableFiles->setRowCount(0);

    for (const FileInfo& info : files) {
        int row = ui->tableFiles->rowCount();
        ui->tableFiles->insertRow(row);

        // 名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(
            info.isDir ? "📁 " + info.name : "📄 " + info.name
        );
        nameItem->setData(Qt::UserRole, info.isDir);
        ui->tableFiles->setItem(row, 0, nameItem);

        // 大小
        QTableWidgetItem* sizeItem = new QTableWidgetItem(
            info.isDir ? "" : formatSize(info.size)
        );
        ui->tableFiles->setItem(row, 1, sizeItem);

        // 时间
        QTableWidgetItem* timeItem = new QTableWidgetItem(formatTime(info.mtime));
        ui->tableFiles->setItem(row, 2, timeItem);
    }

    updateStatusBar();
}

void FileManagerWindow::onFileReadResult(bool success, const QString& content, const QString& message)
{
    if (success) {
        // 显示文件内容对话框
        QMessageBox::information(this, "文件内容", content.left(1000));
    } else {
        QMessageBox::warning(this, "读取失败", message);
    }
}

void FileManagerWindow::onFileOperationResult(bool success, const QString& message)
{
    if (success) {
        QMessageBox::information(this, "操作成功", message);
        loadFileList(m_currentPath);  // 刷新
    } else {
        QMessageBox::warning(this, "操作失败", message);
    }
}

void FileManagerWindow::onBtnBackClicked()
{
    if (m_currentPath == "/") return;

    // 返回上级目录
    QString parentPath = m_currentPath;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        parentPath = parentPath.left(lastSlash);
    } else {
        parentPath = "/";
    }

    loadFileList(parentPath);
}

void FileManagerWindow::onBtnRefreshClicked()
{
    loadFileList(m_currentPath);
}

void FileManagerWindow::onBtnUploadClicked()
{
    // 弹出选择对话框，可以选择文件或文件夹
    QStringList files;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("选择上传");
    msgBox.setText("请选择上传方式:");
    QPushButton* btnFile = msgBox.addButton("选择文件", QMessageBox::ActionRole);
    QPushButton* btnFolder = msgBox.addButton("选择文件夹", QMessageBox::ActionRole);
    QPushButton* btnCancel = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.setDefaultButton(btnFile);

    msgBox.exec();

    if (msgBox.clickedButton() == btnCancel) {
        return;
    }

    if (msgBox.clickedButton() == btnFolder) {
        QString dir = QFileDialog::getExistingDirectory(this, "选择要上传的文件夹", QDir::homePath());
        if (dir.isEmpty()) return;

        // 遍历文件夹中的所有文件
        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files.append(it.next());
        }

        if (files.isEmpty()) {
            QMessageBox::warning(this, "提示", "文件夹中没有文件");
            return;
        }
    } else {  // 选择文件（支持多选）
        files = QFileDialog::getOpenFileNames(
            this,
            "选择要上传的文件",
            QDir::homePath(),
            "所有文件 (*.*)"
        );
        if (files.isEmpty()) return;
    }

    // 使用分片上传
    uploadFiles(files);
}

void FileManagerWindow::uploadFiles(const QStringList& files)
{
    for (const QString& localFile : files) {
        QFileInfo fileInfo(localFile);
        QString fileName = fileInfo.fileName();
        QString remotePath = m_currentPath == "/" ? "/" + fileName : m_currentPath + "/" + fileName;

        ui->labelStatus->setText(QString("正在上传: %1").arg(fileName));

        // 调用分片上传
        m_network->uploadFileChunked(localFile, remotePath);
    }

    ui->labelStatus->setText("上传完成");
    loadFileList(m_currentPath);  // 刷新
}

void FileManagerWindow::onBtnDownloadClicked()
{
    int row = ui->tableFiles->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请选择要下载的文件");
        return;
    }

    FileInfo info = m_files[row];
    if (info.isDir) {
        QMessageBox::warning(this, "提示", "不能下载目录");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "保存文件", info.name);
    if (savePath.isEmpty()) return;

    QString remotePath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;

    // 使用分片下载
    downloadFile(remotePath, savePath);
}

void FileManagerWindow::downloadFile(const QString& remotePath, const QString& localPath)
{
    // 使用分片下载
    m_network->startDownload(remotePath, localPath);

    ui->labelStatus->setText(QString("正在下载: %1").arg(remotePath));
}

void FileManagerWindow::onUploadProgress(const QString& fileName, quint32 transferred, quint32 total)
{
    int percent = total > 0 ? (transferred * 100 / total) : 0;
    ui->labelStatus->setText(QString("上传中: %1 - %2% (%3/%4)")
        .arg(fileName).arg(percent).arg(formatSize(transferred)).arg(formatSize(total)));
}

void FileManagerWindow::onDownloadProgress(const QString& fileName, quint32 transferred, quint32 total)
{
    int percent = total > 0 ? (transferred * 100 / total) : 0;
    ui->labelStatus->setText(QString("下载中: %1 - %2% (%3/%4)")
        .arg(fileName).arg(percent).arg(formatSize(transferred)).arg(formatSize(total)));
}

void FileManagerWindow::onTransferComplete(const QString& fileName, bool success)
{
    if (success) {
        ui->labelStatus->setText(QString("传输完成: %1").arg(fileName));
    } else {
        ui->labelStatus->setText(QString("传输失败: %1").arg(fileName));
        QMessageBox::warning(this, "传输失败", QString("文件 %1 传输失败").arg(fileName));
    }
}

void FileManagerWindow::onBtnDeleteClicked()
{
    int row = ui->tableFiles->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请选择要删除的文件");
        return;
    }

    FileInfo info = m_files[row];
    QString fullPath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", QString("确定要删除 %1 吗？").arg(info.name),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_network->sendFileDelete(fullPath);
    }
}

void FileManagerWindow::onBtnViewClicked()
{
    int row = ui->tableFiles->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请选择要查看的文件");
        return;
    }

    FileInfo info = m_files[row];
    if (info.isDir) {
        loadFileList(m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name);
    } else {
        QString fullPath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;
        m_network->sendFileRead(fullPath);
    }
}

void FileManagerWindow::onBtnRenameClicked()
{
    int row = ui->tableFiles->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请选择要重命名的文件");
        return;
    }

    FileInfo info = m_files[row];
    QString fullPath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;

    // 弹出输入对话框获取新名称
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "请输入新名称:", QLineEdit::Normal, info.name, &ok);
    if (!ok || newName.isEmpty() || newName == info.name) {
        return;
    }

    // 构建新路径
    QString newPath = m_currentPath == "/" ? "/" + newName : m_currentPath + "/" + newName;

    m_network->sendFileRename(fullPath, newPath);
}

void FileManagerWindow::onTableItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    FileInfo info = m_files[row];

    if (info.isDir) {
        QString newPath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;
        loadFileList(newPath);
    } else {
        QString fullPath = m_currentPath == "/" ? "/" + info.name : m_currentPath + "/" + info.name;
        m_network->sendFileRead(fullPath);
    }
}

void FileManagerWindow::onTableSelectionChanged()
{
    int selected = ui->tableFiles->selectedItems().count() / 3;  // 3列
    ui->labelStatus->setText(QString("已选择: %1 个项目 | 当前目录: %2 | %3 个文件")
                              .arg(selected).arg(m_currentPath).arg(m_files.size()));
}

QString FileManagerWindow::formatSize(quint32 size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024);
    return QString("%1 MB").arg(size / (1024 * 1024));
}

QString FileManagerWindow::formatTime(quint32 time)
{
    QDateTime dt = QDateTime::fromSecsSinceEpoch(time);
    return dt.toString("yyyy-MM-dd hh:mm");
}

void FileManagerWindow::updateStatusBar()
{
    onTableSelectionChanged();
}
