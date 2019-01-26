#include "indexer.hpp"

#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QThread>
#include <QTextCodec>
#include <QVector>
#include <algorithm>

Indexer::Indexer(QString path, QThread *baseThread):
    path(path), baseThread(baseThread)
{
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, &Indexer::changed);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &Indexer::changed);
}

Indexer::~Indexer() {
    disconnect(this, nullptr, nullptr, nullptr);
    disconnect(&watcher, nullptr, nullptr, nullptr);
}

void Indexer::changed(QString path) {
    emit sendChanged(path);
}

void Indexer::indexDirectory() {
    QDir dir(path);

    QDirIterator dirIt(path, QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (dirIt.hasNext()) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            this->moveToThread(baseThread);
            emit setStatus("Process was interrupted");
            emit initIndex();
            return;
        }
        dirIt.next();

        watcher.addPath(dirIt.filePath());
    }

    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    qint64 total = 0;

    while (it.hasNext()) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            this->moveToThread(baseThread);
            emit setStatus("Process was interrupted");
            emit initIndex();
            return;
        }

        it.next();

        QFileInfo file_info = it.fileInfo();

        index[file_info.filePath()] = std::set<uint32_t>();

        total += file_info.size();
    }

    qint64 processed = 0;

    for (auto &[path, trigrams]: index) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            this->moveToThread(baseThread);
            emit setStatus("Process was interrupted");
            emit initIndex();
            return;
        }

        QFile file(path);

        processed += file.size();
        emit setProgressBar(int(processed * 100 / total));

        if (!file.open(QIODevice::ReadWrite)) {
            emit sendIOError(path);
            continue;
        }

        bool fileIsText = true;

        QByteArray arr;
        while (!file.atEnd()) {
            arr = file.read(BLOCK_SIZE);

            if (!isTextData(arr)) {
                fileIsText = false;
                break;
            }

            std::set<uint32_t> newTrigrams = splitTrigrams(arr);
            trigrams.insert(newTrigrams.begin(), newTrigrams.end());

            if (arr.size() > 2)
                arr = arr.mid(arr.size() - 2, 2);
        }

        file.close();

        if (!fileIsText)
            trigrams.clear();
    }

    this->moveToThread(baseThread);
    emit setProgressBar(100);
    emit setStatus("Directory has indexed. Enter your search query.");
    emit initSearch();
};

std::set<uint32_t> Indexer::splitTrigrams(const QByteArray& data) {
    std::set<uint32_t> trigrams;
    uint32_t value = 256u * uint8_t(data.at(0)) + uint8_t(data.at(1));
    for (int i = 2; i < data.size(); i++) {
        value <<= 8;
        value += uint8_t(data.at(i));
        value &= (1u << 24) - 1;
        trigrams.insert(value);
    }
    return trigrams;
}

bool Indexer::isTextData(const QByteArray& data) {
    if (data.lastIndexOf('\0') >= 0)
        return false;
    QTextCodec::ConverterState state;
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    codec->toUnicode(data.constData(), data.size(), &state);
    return state.invalidChars <= 2;
}

void Indexer::setQuery(QString query) {
    this->query = query;
}

void Indexer::search() {
    emit setProgressBar(0);
    QByteArray byteQuery = query.toUtf8();

    std::set<uint32_t> trigrams = splitTrigrams(byteQuery);
    QVector<QString> goodFiles;
    goodFiles.clear();

    qint64 total = 0, processed = 0;

    for (auto const &[path, fileTrigrams]: index) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            this->moveToThread(baseThread);
            emit setStatus("Process was interrupted");
            emit initSearch();
            return;
        }

        if (std::includes(fileTrigrams.begin(), fileTrigrams.end(),
                          trigrams.begin(), trigrams.end())) {
            goodFiles.push_back(path);
            total += QFileInfo(path).size();
        }
    }

    for (auto const& path: goodFiles) {
        QFile file(path);

        processed += file.size();
        emit setProgressBar(int(processed * 100 / total));

        if (!file.open(QIODevice::ReadWrite)) {
            emit sendIOError(path);
            continue;
        }

        QByteArray arr = file.read(BLOCK_SIZE);
        bool found = false;

        qint32 i = 0;

        while (i < arr.size()) {
            if (i + byteQuery.size() >= arr.size() && !file.atEnd()) {
                arr += file.read(BLOCK_SIZE);
                arr = arr.mid(i, arr.size() - i);
                i = 0;
            }

            qint32 j = 0;

            for (; j < byteQuery.size(); j++) {
                if (i + j == arr.size() || arr.at(i + j) != byteQuery.at(j))
                    break;
            }
            if (j == byteQuery.size()) {
                found = true;
                break;
            }

            i++;
        }

        if (found)
            emit sendFile(path);

        file.close();
    }

    this->moveToThread(baseThread);
    emit setProgressBar(100);
    emit setStatus("There are your results:");
    emit initSearch();
}
