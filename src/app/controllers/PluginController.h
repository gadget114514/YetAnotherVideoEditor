#pragma once

#include <QObject>
#include <QString>

namespace yave {

/// プラグイン一覧の UI ブリッジ。
/// PluginManager のシグナルを QML へ中継し、検索パス設定を提供する。
class PluginController : public QObject
{
    Q_OBJECT
public:
    explicit PluginController(QObject* parent = nullptr);

    Q_INVOKABLE void rescan();
    Q_INVOKABLE bool isAviUtlSupported() const;

signals:
    void scanStarted();
    void scanFinished();

private:
};

} // namespace yave
