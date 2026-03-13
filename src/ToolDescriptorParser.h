#ifndef TOOLDESCRIPTORPARSER_H
#define TOOLDESCRIPTORPARSER_H

#include <QJsonObject>
#include <QString>

namespace ToolDescriptorParser {
bool parseDescriptorFile(const QString& filePath, QJsonObject& outMetaData, QString* errorMessage = nullptr);
}

#endif // TOOLDESCRIPTORPARSER_H
