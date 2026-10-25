#include "Config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool AppConfig::load(const QString &path, AppConfig &out, QString &error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error = "Tidak bisa membuka " + path;
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!doc.isObject()) {
        error = "cameras.json tidak valid: " + pe.errorString();
        return false;
    }
    const QJsonObject o = doc.object();
    out.maxFps = o["max_fps"].toDouble(0);
    out.hwaccel = o["hwaccel"].toBool(true);
    out.cameras.clear();
    for (const QJsonValue &v : o["cameras"].toArray()) {
        const QJsonObject c = v.toObject();
        if (c["url"].toString().isEmpty()) continue;
        out.cameras.push_back({c["name"].toString(), c["url"].toString(), c["sub_url"].toString()});
    }
    if (out.cameras.empty()) {
        error = "Tidak ada kamera di cameras.json";
        return false;
    }
    return true;
}
