#include "ToolDescriptorParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace {
bool parseLine(const QString& line, QString& key, QString& value) {
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith("#") || trimmed.startsWith("//")) {
        return false;
    }

    static const QRegularExpression pattern("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*\"(.*)\"\\s*$");
    const QRegularExpressionMatch match = pattern.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    key = match.captured(1).trimmed();
    value = match.captured(2);
    return !key.isEmpty();
}
}

namespace ToolDescriptorParser {

bool parseDescriptorFile(const QString& filePath, QJsonObject& outMetaData, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open descriptor file: %1").arg(filePath);
        }
        return false;
    }

    QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    file.close();

    QString name;
    QString version;
    QString supportedVersion;
    QString author;

    for (QString line : lines) {
        line.replace("\r", "");
        QString key;
        QString value;
        if (!parseLine(line, key, value)) {
            continue;
        }

        if (key == "name") {
            name = value;
        } else if (key == "version") {
            version = value;
        } else if (key == "supported_version") {
            supportedVersion = value;
        } else if (key == "author") {
            author = value;
        }
    }

    if (name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Descriptor missing name: %1").arg(filePath);
        }
        return false;
    }

    outMetaData = QJsonObject{
        {"id", name},
        {"version", version},
        {"compatibleVersion", supportedVersion},
        {"author", author}
    };

    return true;
}

}
