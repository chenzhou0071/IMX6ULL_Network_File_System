#include "fileviewerdialog.h"
#include <QFont>

FileViewerDialog::FileViewerDialog(const QString& fileName, const QString& content, QWidget *parent)
    : QDialog(parent)
    , m_fileName(fileName)
    , m_content(content)
{
    setWindowTitle(QString("查看文件 - %1").arg(fileName));
    resize(700, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 文件名标签
    QLabel* lblFileName = new QLabel(QString("文件: %1").arg(fileName), this);
    lblFileName->setStyleSheet("font-weight: bold; padding: 5px;");
    mainLayout->addWidget(lblFileName);

    // 文本编辑器（只读）
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setPlainText(content);
    m_textEdit->setFont(QFont("Consolas", 10));
    m_textEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #1E1E1E;
            color: #D4D4D4;
            border: 1px solid #3C3C3C;
            border-radius: 5px;
            padding: 10px;
        }
    )");
    mainLayout->addWidget(m_textEdit);

    // 按钮栏
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton* btnClose = new QPushButton("关闭", this);
    btnClose->setMinimumSize(100, 35);
    btnClose->setStyleSheet(R"(
        QPushButton {
            background-color: #607D8B;
            color: white;
            border-radius: 5px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #455A64;
        }
    )");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnClose);

    mainLayout->addLayout(btnLayout);
}

FileViewerDialog::~FileViewerDialog()
{
}
