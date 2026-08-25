#pragma once

#include "PluginManager.h"

#include <QMatrix4x4>
#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <functional>

namespace yave::plugin {

/// プラグイン GUI を載せるポップアップウィンドウ。
/// VST3 / AviUtl / 字幕エフェクト のすべてで共用する。
///
/// nativeViewHandle() が返す値:
///   Windows -> HWND   (void* にキャスト済み)
///   macOS   -> NSView* (void* にキャスト済み。NSWindow* ではない)
class PluginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PluginWindow(const PluginDescriptor& desc, QWidget* parent = nullptr);
    ~PluginWindow() override;

    const PluginDescriptor& descriptor() const { return desc_; }

    /// プラグインへ渡すネイティブハンドル。
    /// コンストラクタで winId() を呼んで生成済みなので、常に有効な値を返す。
    void* nativeViewHandle() const;

    /// クライアント領域 (プラグインが描画する範囲) を指定サイズにする。
    /// ウィンドウ枠の分は自動で加算される。
    void resizeClientArea(int width, int height);

    /// IPlugFrame::resizeView から呼ばれる。
    void onPluginRequestedResize(int width, int height);

    /// IPlugView::canResize() の結果を反映する。
    void setResizableByUser(bool on);
    bool isResizableByUser() const { return resizable_; }

    /// ウィンドウを閉じるときに呼ぶ処理 (IPlugView::removed など) を登録する。
    void setDetachHandler(std::function<void()> fn) { detachHandler_ = std::move(fn); }

    /// DPI 非対応プラグイン (AviUtl 等) 用にスケーリングを無効化する。
    bool isDpiUnaware() const { return dpiUnaware_; }

    /// 既に開いているウィンドウを前面に出す。
    void raiseAndActivate();

#if defined(Q_OS_WIN)
    /// AviUtl フィルタのメッセージ中継先を設定する。
    void setAviUtlMessageTarget(void* filterDll) { aviutlFilter_ = filterDll; }
#endif

signals:
    void closed();
    void parametersChangedByPlugin();

protected:
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;        ///< LanguageChange

private:
    void retranslateUi();
    void createNativeContainer();

    PluginDescriptor      desc_;
    QWidget*              container_ = nullptr;   ///< WA_NativeWindow を持つ子
    std::function<void()> detachHandler_;
    bool                  resizable_  = false;
    bool                  dpiUnaware_ = false;

    // 付随 UI (プラグイン GUI の外側に出すもの)
    QPushButton* bypassButton_ = nullptr;
    QLabel*      presetLabel_  = nullptr;
    QPushButton* resetButton_  = nullptr;

#if defined(Q_OS_WIN)
    void* aviutlFilter_ = nullptr;   ///< FILTER_DLL*
#endif
};

} // namespace yave::plugin
