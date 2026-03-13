#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QDebug>
#include <QSet>
#include <QTextStream>
#include <iostream>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void logMessage(const QString& msg) {
    qDebug() << msg;
    std::cout << msg.toStdString() << std::endl;
}

// Recursively collect all file paths relative to baseDir
QStringList getAllLocalFiles(const QString& baseDir) {
    QStringList result;
    QDir dir(baseDir);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            QStringList sub = getAllLocalFiles(info.filePath());
            result.append(sub);
        } else {
            QString rel = QDir(baseDir).relativeFilePath(info.filePath());
            result.append(rel.replace('\\', '/'));
        }
    }
    return result;
}

// Extract the tool directory name from a path like "tools/SomeTool/..."
// Returns empty string if path is not under tools/
QString extractToolName(const QString& relativePath) {
    if (!relativePath.startsWith("tools/")) {
        return QString();
    }
    int secondSlash = relativePath.indexOf('/', 6); // after "tools/"
    if (secondSlash < 0) {
        // File directly in tools/ (not in a subdirectory) - treat as non-tool file
        return QString();
    }
    return relativePath.mid(0, secondSlash); // e.g. "tools/SomeTool"
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() < 3) {
        logMessage("Usage: updater.exe <target_dir> <temp_dir>");
        return 1;
    }

    QString targetDir = args[1];
    QString tempDir = args[2];

    logMessage("Updater started.");
    logMessage("Target Dir: " + targetDir);
    logMessage("Temp Dir: " + tempDir);

    // Wait for main application to exit
    logMessage("Waiting for main application to exit...");
    QThread::sleep(2); // Give it 2 seconds to close

#ifdef Q_OS_WIN
    // Force kill just in case
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "APEHOI4ToolStudio.exe");
#endif

    QDir tDir(tempDir);
    if (!tDir.exists()) {
        logMessage("Temp directory does not exist. Nothing to update.");
        return 1;
    }

    // Prepare target dir
    QDir target(targetDir);
    if (!target.exists()) {
        target.mkpath(".");
    }

    // --- Cleanup obsolete files (third-party tools protection) ---
    QString manifestListPath = QDir(tempDir).filePath("manifest_files.txt");
    if (QFile::exists(manifestListPath)) {
        logMessage("Reading manifest file list for cleanup...");

        // Read manifest file list
        QSet<QString> manifestFiles;
        QSet<QString> officialToolDirs; // e.g. "tools/LogManagerTool"

        QFile manifestFile(manifestListPath);
        if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&manifestFile);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                    manifestFiles.insert(line);
                    // Track official tool directories
                    QString toolName = extractToolName(line);
                    if (!toolName.isEmpty()) {
                        officialToolDirs.insert(toolName);
                    }
                }
            }
            manifestFile.close();
        }

        logMessage(QString("Manifest contains %1 files, %2 official tool directories.")
            .arg(manifestFiles.size())
            .arg(officialToolDirs.size()));

        // Files/dirs that should never be deleted
        QSet<QString> protectedFiles;
        protectedFiles.insert("Updater.exe");
        protectedFiles.insert("config.json");

        // Scan local directory and remove obsolete files
        QStringList localFiles = getAllLocalFiles(targetDir);
        int removedCount = 0;

        for (const QString& localFile : localFiles) {
            // Skip protected files
            if (protectedFiles.contains(localFile)) {
                continue;
            }

            // Skip if file is in the manifest (it's a current official file)
            if (manifestFiles.contains(localFile)) {
                continue;
            }

            // Check if this file is under tools/ directory
            QString toolName = extractToolName(localFile);
            if (!toolName.isEmpty()) {
                // This file is inside a tool directory
                if (!officialToolDirs.contains(toolName)) {
                    // This tool directory is NOT in the manifest at all
                    // -> it's a third-party tool, skip it
                    continue;
                }
                // This tool IS an official tool but this specific file
                // is no longer in the manifest -> delete it
            }

            // Delete the obsolete file
            QString fullPath = QDir(targetDir).filePath(localFile);
            if (QFile::remove(fullPath)) {
                logMessage("Removed obsolete file: " + localFile);
                removedCount++;
            } else {
                logMessage("Failed to remove obsolete file: " + localFile);
            }
        }

        // Clean up empty directories (but not tool directories that belong to third-party tools)
        // We do a second pass to remove empty dirs
        std::function<void(const QString&, const QString&)> removeEmptyDirs = [&](const QString& dirPath, const QString& basePath) {
            QDir dir(dirPath);
            QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& entry : entries) {
                removeEmptyDirs(entry.filePath(), basePath);
            }

            // Don't remove the root target directory itself
            if (dirPath == basePath) return;

            QString relDir = QDir(basePath).relativeFilePath(dirPath).replace('\\', '/');

            // Protect third-party tool directories
            if (relDir.startsWith("tools/")) {
                QString toolName = relDir;
                int slash = relDir.indexOf('/', 6);
                if (slash > 0) {
                    toolName = relDir.mid(0, slash);
                }
                if (!officialToolDirs.contains(toolName)) {
                    // Third-party tool directory, don't remove even if empty
                    return;
                }
            }

            // Remove if empty
            if (dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                if (QDir().rmdir(dirPath)) {
                    logMessage("Removed empty directory: " + relDir);
                }
            }
        };

        removeEmptyDirs(targetDir, targetDir);

        logMessage(QString("Cleanup complete. Removed %1 obsolete files.").arg(removedCount));
    } else {
        logMessage("No manifest_files.txt found, skipping cleanup (legacy update mode).");
    }

    logMessage("Starting file copy...");

    // Recursive copy function
    std::function<bool(const QString&, const QString&)> copyRecursively = [&](const QString& srcPath, const QString& dstPath) -> bool {
        QDir srcDir(srcPath);
        if (!srcDir.exists()) return false;

        QDir dstDir(dstPath);
        if (!dstDir.exists()) {
            dstDir.mkpath(".");
        }

        bool success = true;
        QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fileInfo : fileInfoList) {
            QString srcFilePath = fileInfo.filePath();
            QString dstFilePath = dstDir.filePath(fileInfo.fileName());

            if (fileInfo.isDir()) {
                success = copyRecursively(srcFilePath, dstFilePath) && success;
            } else {
                // Skip manifest_files.txt and Updater.exe
                if (fileInfo.fileName() == "manifest_files.txt" || fileInfo.fileName() == "Updater.exe") {
                    continue;
                }
                if (QFile::exists(dstFilePath)) {
                    if (!QFile::remove(dstFilePath)) {
                        logMessage("Failed to remove existing file: " + dstFilePath);
                        success = false;
                        continue;
                    }
                }
                if (!QFile::copy(srcFilePath, dstFilePath)) {
                    logMessage("Failed to copy file: " + srcFilePath + " to " + dstFilePath);
                    success = false;
                } else {
                    logMessage("Copied: " + fileInfo.fileName());
                }
            }
        }
        return success;
    };

    if (copyRecursively(tempDir, targetDir)) {
        logMessage("File copy completed successfully.");

        // Restart main application
        QString mainAppPath = QDir(targetDir).filePath("APEHOI4ToolStudio.exe");
        if (QFile::exists(mainAppPath)) {
            logMessage("Restarting main application...");
            QProcess::startDetached(mainAppPath, QStringList());
        } else {
            logMessage("Main application not found at: " + mainAppPath);
        }
    } else {
        logMessage("Update failed during file copy.");
        return 1;
    }

    return 0;
}
