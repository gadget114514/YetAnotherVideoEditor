#pragma once

#include "MediaFolder.h"
#include "Rational.h"
#include "TimeRange.h"

#include <QColor>
#include <QObject>
#include <QSize>
#include <QString>

#include <QUndoStack>
#include <memory>

namespace yave {

class AssetLibrary;
class Timeline;

/// プロジェクト全体の状態。
///
/// 所有関係:
///   Project (所有)
///     ├── Timeline (unique_ptr)
///     │     └── std::vector<std::unique_ptr<Track>>
///     ├── AssetLibrary (unique_ptr)
///     └── QUndoStack (unique_ptr)
///
/// 字幕スタイルプリセットテーブルは yave_subtitle 側が管理し、
/// io 層 (ProjectSerializer) が両者を繋ぐ。
class Project : public QObject
{
    Q_OBJECT
public:
    explicit Project(QObject* parent = nullptr);
    ~Project() override;

    // --- 属性 ---
    QString name() const { return name_; }
    void    setName(const QString& n) { name_ = n; }

    Rational timebase() const;
    void     setTimebase(const Rational& tb);

    QSize canvasSize() const;
    void  setCanvasSize(const QSize& s);

    int sampleRate() const { return sampleRate_; }
    void setSampleRate(int sr) { sampleRate_ = qBound(8000, sr, 384000); }

    int channels() const { return channels_; }
    void setChannels(int c) { channels_ = qBound(1, c, 32); }

    /// 色空間名 ("bt709" / "bt2020")
    QString colorSpaceName() const { return colorSpace_; }
    void    setColorSpaceName(const QString& cs) { colorSpace_ = cs; }

    // --- 再生状態 ---
    int64_t playhead() const { return playhead_; }
    void    setPlayhead(int64_t f) { playhead_ = f > 0 ? f : 0; }

    const TimeRange& workRange() const { return workRange_; }
    void             setWorkRange(const TimeRange& r) { workRange_ = r; }

    double masterGain() const { return masterGain_; }
    void   setMasterGain(double g) { masterGain_ = g < 0.0 ? 0.0 : g; }

    // --- 設定 ---
    bool isPdcEnabled() const { return pdcEnabled_; }
    void setPdcEnabled(bool on) { pdcEnabled_ = on; }

    bool isProxyEnabled() const { return proxyEnabled_; }
    void setProxyEnabled(bool on) { proxyEnabled_ = on; }

    bool isAutoCommitAi() const { return autoCommitAi_; }
    void setAutoCommitAi(bool on) { autoCommitAi_ = on; }

    bool isModified() const { return modified_; }
    void clearModified() { modified_ = false; }
    void markModified() { modified_ = true; }

    // --- メディアライブラリのフォルダ構成 (1.7.5 / 9.2.1) ---
    /// UI のフォルダ整理はプロジェクトの内容なのでここに持つ。
    /// エフェクトライブラリ側のフォルダは QSettings (アプリ側) に置く。
    const MediaFolderTree& mediaFolders() const { return mediaFolders_; }
    MediaFolderTree&       mutableMediaFolders() { return mediaFolders_; }
    void setMediaFolders(MediaFolderTree t) { mediaFolders_ = std::move(t); }

    // --- 子オブジェクト ---
    Timeline*     timeline() const { return timeline_.get(); }
    AssetLibrary* assets() const { return assetLibrary_.get(); }
    QUndoStack*   undoStack() const { return undoStack_.get(); }

signals:
    void modifiedChanged();

private:
    MediaFolderTree mediaFolders_;
    QString  name_;
    int      sampleRate_   = 48000;
    int      channels_     = 2;
    QString  colorSpace_   = QStringLiteral("bt709");

    int64_t  playhead_  = 0;
    TimeRange workRange_{0, 0};
    double   masterGain_ = 1.0;

    bool     pdcEnabled_   = true;
    bool     proxyEnabled_ = true;
    bool     autoCommitAi_ = false;
    bool     modified_     = false;

    std::unique_ptr<Timeline>     timeline_;
    std::unique_ptr<AssetLibrary> assetLibrary_;
    std::unique_ptr<QUndoStack>   undoStack_;
};

} // namespace yave
