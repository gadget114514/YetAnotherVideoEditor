#pragma once

#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <vector>

namespace yave::sdk {

enum class ParamType { Bool, Int, Double, Color, String, Enum, Point, Curve, FilePath };

struct ParamDef
{
    QString    key;                  ///< JSON に保存されるキー。変更禁止
    QString    displayNameKey;       ///< 翻訳キー or そのまま表示する文字列
    ParamType  type = ParamType::Double;
    QVariant   defaultValue;
    QVariant   minValue;             ///< Int / Double のみ
    QVariant   maxValue;
    QVariant   step;
    QStringList enumKeys;            ///< Enum のときの選択肢(翻訳キー)
    QString    unitSuffix;           ///< "px" / "%" / "s"
    QString    tooltipKey;
    bool       animatable = false;   ///< キーフレーム可能か(将来拡張)
};

using ParameterSchema = std::vector<ParamDef>;

/// 実際の値。key -> QVariant
class ParameterValues
{
public:
    bool contains(const QString& key) const { return values_.contains(key); }

    QVariant get(const QString& key) const { return values_.value(key); }

    double getDouble(const QString& key, double fallback = 0.0) const
    {
        return values_.contains(key) ? values_.value(key).toDouble() : fallback;
    }
    int getInt(const QString& key, int fallback = 0) const
    {
        return values_.contains(key) ? values_.value(key).toInt() : fallback;
    }
    bool getBool(const QString& key, bool fallback = false) const
    {
        return values_.contains(key) ? values_.value(key).toBool() : fallback;
    }
    QColor getColor(const QString& key, const QColor& fallback = Qt::white) const
    {
        if (!values_.contains(key)) return fallback;
        const QVariant v = values_.value(key);
        return v.canConvert<QColor>() ? v.value<QColor>() : QColor(v.toString());
    }
    QString getString(const QString& key, const QString& fallback = {}) const
    {
        return values_.contains(key) ? values_.value(key).toString() : fallback;
    }

    void set(const QString& key, const QVariant& v) { values_.insert(key, v); }
    void remove(const QString& key) { values_.remove(key); }
    void clear() { values_.clear(); }
    int  size() const { return values_.size(); }

    QJsonObject toJson() const
    {
        QJsonObject o;
        for (auto it = values_.constBegin(); it != values_.constEnd(); ++it) {
            const QVariant& v = it.value();
            switch (v.typeId()) {
            case QMetaType::Bool:   o[it.key()] = v.toBool(); break;
            case QMetaType::Int:
            case QMetaType::LongLong: o[it.key()] = v.toLongLong(); break;
            case QMetaType::Double:
            case QMetaType::Float:  o[it.key()] = v.toDouble(); break;
            case QMetaType::QStringList: {
                QJsonArray arr;
                for (const QString& s : v.toStringList()) arr.append(s);
                o[it.key()] = arr;
                break;
            }
            default:
                // QColor は name() 文字列で保存する
                if (v.canConvert<QColor>())
                    o[it.key()] = v.value<QColor>().name(QColor::HexArgb);
                else
                    o[it.key()] = v.toString();
                break;
            }
        }
        return o;
    }

    static ParameterValues fromJson(const QJsonObject& o)
    {
        ParameterValues pv;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            const QJsonValue v = it.value();
            if      (v.isBool())   pv.values_.insert(it.key(), v.toBool());
            else if (v.isDouble()) pv.values_.insert(it.key(), v.toDouble());
            else if (v.isArray()) {
                QStringList l;
                for (const QJsonValue& e : v.toArray()) l.append(e.toString());
                pv.values_.insert(it.key(), l);
            }
            else                   pv.values_.insert(it.key(), v.toString());
        }
        return pv;
    }

private:
    QHash<QString, QVariant> values_;
};

} // namespace yave::sdk
