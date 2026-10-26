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
    out.hd = o["quality"].toString("sd").toLower() == "hd";
    {
        const QJsonObject ov = o["overlay"].toObject();
        OverlayStyle &st = out.overlay;
        st.atBottom = ov["position"].toString("bottom").toLower() != "top";
        st.font = ov["font"].toString(st.font);
        st.size = qBound(6, ov["size"].toInt(st.size), 48);
        st.bold = ov["bold"].toBool(st.bold);
        st.nameColor = ov["name_color"].toString(st.nameColor);
        st.liveColor = ov["live_color"].toString(st.liveColor);
        st.warnColor = ov["warn_color"].toString(st.warnColor);
        st.barOpacity = qBound(0, ov["bar_opacity"].toInt(st.barOpacity), 255);
        st.showStatus = ov["show_status"].toBool(st.showStatus);
        st.showName = ov["show_name"].toBool(st.showName);
    }
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
