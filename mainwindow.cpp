#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include <QAction>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QTextStream>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentIndex(0);
    ui->progressBar->setValue(0);
    ui->fileList->header()->resizeSection(0, 400);

    ui->dirPath->installEventFilter(this);

    connect(this, &MainWindow::lineEditClicked, this, &MainWindow::selectDirectory);
    connect(ui->dirSelect, &QAbstractButton::clicked, this, &MainWindow::selectDirectory);
    connect(ui->indexStart, &QAbstractButton::clicked, this, &MainWindow::indexDirectory);
    connect(ui->searchStart, &QAbstractButton::clicked, this, &MainWindow::search);

    connect(ui->indexCancel, &QAbstractButton::clicked, this, &MainWindow::cancelJob);
    connect(ui->searchCancel, &QAbstractButton::clicked, this, &MainWindow::cancelJob);

    connect(ui->fileList, &QTreeWidget::itemDoubleClicked, this, &MainWindow::openFile);
}

MainWindow::~MainWindow()
{
    if (job)
        job->requestInterruption();
    if (indexer)
        indexer->deleteLater();
    delete ui;
}

void MainWindow::selectDirectory() {
    path = QFileDialog::getExistingDirectory(
        this,
        "Select directory to index"
    );

    ui->dirPath->setText(path);

    ui->indexStart->setEnabled(path.size() > 0);
}

void MainWindow::indexDirectory() {
    ui->indexStart->setEnabled(false);
    ui->indexCancel->setEnabled(true);
    ui->taskLabel->setText("Scanning the directory...");
    ui->progressBar->setValue(0);

    if (indexer) {
        disconnect(this, nullptr, indexer, nullptr);
        indexer->deleteLater();
    }
    indexer = new Indexer(path, QThread::currentThread());
    job = new QThread;

    connect(this, &MainWindow::sendQuery, indexer, &Indexer::setQuery);

    connect(job, &QThread::started, indexer, &Indexer::indexDirectory);

    connect(indexer, &Indexer::sendChanged, this, &MainWindow::fileChanged);
    connect(indexer, &Indexer::initIndex, this, &MainWindow::initIndex);
    connect(indexer, &Indexer::initSearch, this, &MainWindow::initSearch);
    connect(indexer, &Indexer::setProgressBar, this, &MainWindow::updateProgressBar);
    connect(indexer, &Indexer::setStatus, this, &MainWindow::updateStatus);

    connect(indexer, &Indexer::sendFile, this, &MainWindow::getFile);

    connect(job, &QThread::finished, job, &QThread::deleteLater);

    indexer->moveToThread(job);
    job->start();
}

void MainWindow::search() {
    if (ui->searchText->text().length() < 3) {
        QMessageBox::critical(
            this,
            "Error",
            "Please provide string with 3 characters or more"
        );
        return;
    }

    ui->fileList->clear();
    ui->searchStart->setEnabled(false);
    ui->searchCancel->setText("Cancel");
    ui->taskLabel->setText("Searching your string...");
    ui->progressBar->setValue(0);

    job = new QThread;
    emit sendQuery(ui->searchText->text());

    connect(job, &QThread::started, indexer, &Indexer::search);
    connect(job, &QThread::finished, job, &QThread::deleteLater);

    indexer->moveToThread(job);
    job->start();
}

void MainWindow::updateProgressBar(int progress) {
    ui->progressBar->setValue(progress);
}

void MainWindow::updateStatus(QString message) {
    ui->taskLabel->setText(message);
}

void MainWindow::initIndex() {
    ui->stackedWidget->setCurrentIndex(0);
    ui->indexStart->setEnabled(path.size() > 0);
    ui->indexCancel->setEnabled(false);
}

void MainWindow::initSearch() {
    ui->stackedWidget->setCurrentIndex(1);
    ui->searchStart->setEnabled(true);

    ui->searchCancel->setText("Change directory");
}

void MainWindow::openFile(QTreeWidgetItem *item, int column) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(0, Qt::DisplayRole).toString()));
}

void MainWindow::getFile(QString path, quint32 occurences) {
    new QTreeWidgetItem(
        ui->fileList,
        QStringList({path, QString::number(occurences)})
    );
}

void MainWindow::getIOError(QString path) {
    auto button = QMessageBox::warning(
        this,
        "Can't access file",
        "Can't open file '" + path + "'.\n\nPress OK to continue, Cancel to stop operation.",
        QMessageBox::Ok | QMessageBox::Cancel);
    if (button == QMessageBox::Cancel) {
        cancelJob();
    }
}

void MainWindow::cancelJob() {
    if (job)
        job->requestInterruption();

    if (ui->stackedWidget->currentIndex() == 1 && ui->searchCancel->text() == "Change directory") {
        ui->fileList->clear();
        initIndex();
    }
}

bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    if (object == ui->dirPath) {
        if (event->type() == QEvent::MouseButtonRelease) {
            emit lineEditClicked();
            return true;
        } else {
            return false;
        }
    } else {
        return QMainWindow::eventFilter(object, event);
    }
}

void MainWindow::fileChanged(QString path) {
    QMessageBox::warning(
        this,
        "File changed",
        "File at '" + path + "' changed.\n\nMaybe you need to scan directory again.");
}
