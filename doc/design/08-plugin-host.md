# 8. プラグインホスト (VST3 / AviUtl / 字幕エフェクト)

[← 目次に戻る](../design.md)

---

## 8.1 全体構成

3 系統のプラグインを `PluginManager` が一元管理する。

```
                        PluginManager (singleton)
                              |
        +---------------------+---------------------+
        |                     |                     |
  Vst3Registry        SubtitleEffectRegistry   AviUtlRegistry
   (Win / Mac)            (Win / Mac)           (Windows のみ)
        |                     |                     |
   Vst3Host              ISubtitleEffect       AviUtlHost
        |                  実装インスタンス          |
        +---------------------+---------------------+
                              |
                        PluginWindow (QWidget)
                   ネイティブハンドル (HWND / NSView) を
                        プラグインへ引き渡す
```

## 8.2 PluginManager

```cpp
// src/plugin/PluginManager.h
namespace yave::plugin {

enum class PluginKind { Vst3, SubtitleEffect, AviUtlFilter, AviUtlInput };

struct PluginDescriptor
{
    QUuid       uid;                // アプリ内部で振る一意 ID
    PluginKind  kind = PluginKind::Vst3;
    QString     nativeId;           // VST3: FUID 文字列 / 字幕: effectId / AviUtl: DLL 名
    QString     name;
    QString     vendor;
    QString     version;
    QString     filePath;
    QString     category;
    bool        hasEditor = false;
    int64_t     latencySamples = 0;  // VST3 のみ
    bool        isValid = true;
    QString     invalidReason;
};

class PluginManager : public QObject
{
    Q_OBJECT
public:
    static PluginManager& instance();

    /// 起動時に呼ぶ。バックグラウンドで走査し、完了したら scanFinished を出す。
    void scanAsync();
    bool isScanning() const;

    std::vector<PluginDescriptor> plugins(PluginKind kind) const;
    const PluginDescriptor* find(const QUuid& uid) const;
    const PluginDescriptor* findByNativeId(PluginKind kind, const QString& nativeId) const;

    /// 検索パス
    QStringList searchPaths(PluginKind kind) const;
    void setSearchPaths(PluginKind kind, const QStringList& paths);

    /// ブラックリスト (クラッシュしたプラグイン)
    void blacklist(const QString& filePath, const QString& reason);
    void clearBlacklist();
    bool isBlacklisted(const QString& filePath) const;

    void unloadAll();

signals:
    void scanProgress(int done, int total);
    void scanFinished();
    void pluginCrashed(const QString& filePath, const QString& reason);

private:
    Vst3Registry*            vst3_ = nullptr;
    SubtitleEffectRegistry*  subtitleFx_ = nullptr;
#if defined(Q_OS_WIN)
    AviUtlRegistry*          aviutl_ = nullptr;
#endif
};

} // namespace yave::plugin
```

### 8.2.1 既定の検索パス

| 種別 | Windows | macOS |
|---|---|---|
| VST3 | `C:/Program Files/Common Files/VST3`<br>`%LOCALAPPDATA%/Programs/Common/VST3` | `/Library/Audio/Plug-Ins/VST3`<br>`~/Library/Audio/Plug-Ins/VST3` |
| 字幕エフェクト | `<app>/plugins/subtitle`<br>`%APPDATA%/YAVE/plugins/subtitle` | `<app>.app/Contents/PlugIns/subtitle`<br>`~/Library/Application Support/YAVE/plugins/subtitle` |
| AviUtl | ユーザー指定 (AviUtl の `plugins` フォルダを指す) | — |

### 8.2.2 走査キャッシュ

走査結果は `%APPDATA%/YAVE/plugin_cache.json` に保存する。
ファイルの更新日時とサイズが一致すれば再走査しない。

```json
{
  "schemaVersion": 1,
  "entries": [
    {
      "filePath": "C:/Program Files/Common Files/VST3/FabFilter Pro-Q 3.vst3",
      "mtime": 1724500000,
      "size": 12345678,
      "plugins": [
        { "kind": "Vst3", "nativeId": "…FUID…", "name": "FabFilter Pro-Q 3",
          "vendor": "FabFilter", "hasEditor": true, "latencySamples": 0 }
      ]
    }
  ],
  "blacklist": []
}
```

## 8.3 PluginWindow

**すべてのプラグイン GUI は独立したポップアップウィンドウ**として表示する。
QML のメインウィンドウには埋め込まない。

> **理由**: VST3 も AviUtl もネイティブの子ウィンドウ (HWND / NSView) を要求する。
> QML シーングラフの中にネイティブウィンドウを混ぜると、
> Z オーダー、スケーリング (High DPI)、リサイズ、入力イベントのすべてで問題が起きる。
> 別ウィンドウにすれば OS のウィンドウマネージャに任せられる。
> DAW でもプラグイン GUI は別ウィンドウが標準的な UX である。

```cpp
// src/plugin/PluginWindow.h
namespace yave::plugin {

/// プラグイン GUI を載せるネイティブコンテナウィンドウ。
class PluginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PluginWindow(const PluginDescriptor& desc, QWidget* parent = nullptr);
    ~PluginWindow() override;

    /// ネイティブビューのハンドルを返す。
    ///   Windows -> HWND を void* にキャストしたもの
    ///   macOS   -> NSView* を void* にキャストしたもの
    /// プラグインへ渡すのはこの値。
    void* nativeViewHandle() const;

    /// プラグインが要求するサイズにクライアント領域を合わせる。
    /// (ウィンドウ枠の分を加味する必要がある)
    void resizeClientArea(int width, int height);

    /// プラグインからのリサイズ要求 (IPlugFrame::resizeView)
    void onPluginRequestedResize(int width, int height);

    /// リサイズ可能かどうか。IPlugView::canResize() の結果を反映する。
    void setResizableByUser(bool on);

    /// 表示中のプラグインインスタンスへの弱参照。閉じるときに detach を呼ぶため。
    void setDetachHandler(std::function<void()> fn) { detachHandler_ = std::move(fn); }

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* e) override;
    void showEvent(QShowEvent* e) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    PluginDescriptor       desc_;
    QWidget*               container_ = nullptr;   // WA_NativeWindow を持つ子
    std::function<void()>  detachHandler_;
    bool                   resizable_ = false;
};

} // namespace yave::plugin
```

### 8.3.1 ネイティブハンドルの取得

```cpp
PluginWindow::PluginWindow(const PluginDescriptor& desc, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , desc_(desc)
{
    setWindowTitle(desc.name);
    setAttribute(Qt::WA_DeleteOnClose, false);   // 明示的に管理する

    // プラグインを載せるための、必ずネイティブハンドルを持つ子ウィジェット。
    // QWidget は既定では「アルファウィジェット」で HWND を持たないため、
    // WA_NativeWindow を立てて実体を作らせる。
    container_ = new QWidget(this);
    container_->setAttribute(Qt::WA_NativeWindow, true);
    container_->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    container_->setAttribute(Qt::WA_NoSystemBackground, true);
    container_->setAttribute(Qt::WA_OpaquePaintEvent, true);
    container_->setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(container_);

    // winId() を呼ぶことでネイティブハンドルの生成を強制する
    (void)container_->winId();
}

void* PluginWindow::nativeViewHandle() const
{
    return reinterpret_cast<void*>(container_->winId());
}
```

> **`winId()` の戻り値について**:
> - Windows: `HWND` がそのまま `WId` として返る。
> - macOS: `NSView*` が返る (`NSWindow*` ではない)。VST3 が要求するのも `NSView*` なので
>   そのまま渡せる。ここを取り違えるとプラグインが表示されない。
>
> `WA_DontCreateNativeAncestors` を立てないと、親ウィジェット群も
> すべてネイティブ化され、描画性能が落ちる。

### 8.3.2 High DPI の扱い

VST3 の `IPlugViewContentScaleSupport` にスケールファクタを通知する。

```cpp
void PluginWindow::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (auto* scaleSupport = plugView_ ? FUnknownPtr<IPlugViewContentScaleSupport>(plugView_)
                                       : nullptr) {
        scaleSupport->setContentScaleFactor(float(devicePixelRatioF()));
    }
}
```

**AviUtl は DPI 非対応**のプラグインが大半のため、
`PluginWindow` を DPI 非依存モードで作る必要がある。

```cpp
#if defined(Q_OS_WIN)
// AviUtl 用ウィンドウは per-monitor DPI awareness を無効にする
if (desc_.kind == PluginKind::AviUtlFilter) {
    DPI_AWARENESS_CONTEXT prev =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    (void)container_->winId();      // この状態でウィンドウを作る
    SetThreadDpiAwarenessContext(prev);
}
#endif
```

## 8.4 VST3 ホスト

### 8.4.1 全体の流れ

```
1. モジュールのロード
     Win: LoadLibraryW("Plugin.vst3/Contents/x86_64-win/Plugin.vst3")
     Mac: CFBundle でロード (bundleEntry / bundleExit を呼ぶ)
2. GetPluginFactory() を resolve して IPluginFactory を得る
3. countClasses() でクラスを列挙、kVstAudioEffectClass のものを探す
4. createInstance(cid, IComponent::iid, &component)
5. component->initialize(hostContext)
6. component->queryInterface(IEditController::iid, &controller)
      失敗したら getControllerClassId() で別途生成
7. controller->initialize(hostContext)
8. component->queryInterface(IAudioProcessor::iid, &processor)
9. connect(component, controller) : IConnectionPoint 同士を接続
10. processor->setupProcessing(ProcessSetup)
11. component->setActive(true)
12. processor->setProcessing(true)
     -- ここから RT スレッドで process() が呼べる --
13. GUI を出す場合: controller->createView("editor") -> IPlugView
14. plugView->setFrame(pluginFrame)
15. plugView->attached(nativeHandle, platformType)
```

### 8.4.2 Vst3Host

```cpp
// src/plugin/vst3/Vst3Host.h
namespace yave::plugin {

class Vst3Host : public QObject
{
    Q_OBJECT
public:
    explicit Vst3Host(QObject* parent = nullptr);
    ~Vst3Host() override;

    bool load(const PluginDescriptor& desc, double sampleRate, int maxBlockSize,
              QString* errorOut = nullptr);
    void unload();

    bool isLoaded() const { return component_ != nullptr; }

    // --- オーディオ処理 ---
    /// RT スレッドから呼ばれる。ここでロックを取ってはならない。
    void processRt(float* const* buffers, int numChannels, int numFrames) noexcept;

    int64_t latencySamples() const;
    void    setProcessing(bool on);
    void    setProcessMode(bool offline);   // kRealtime / kOffline

    // --- パラメータ ---
    int      parameterCount() const;
    QString  parameterTitle(int index) const;
    double   parameterNormalized(Steinberg::Vst::ParamID id) const;
    void     setParameterNormalized(Steinberg::Vst::ParamID id, double v);

    // --- 状態保存 ---
    QByteArray saveState() const;            // IComponent::getState + IEditController::getState
    bool       loadState(const QByteArray& data);

    // --- GUI ---
    bool hasEditor() const;
    /// PluginWindow を作って GUI を attach する。既に開いていれば前面に出す。
    PluginWindow* openEditor(QWidget* parent = nullptr);
    void          closeEditor();

signals:
    void latencyChanged();
    void parametersChanged();

private:
    bool attachEditorTo(PluginWindow* w);

    Steinberg::IPluginFactory*        factory_    = nullptr;
    Steinberg::Vst::IComponent*       component_  = nullptr;
    Steinberg::Vst::IEditController*  controller_ = nullptr;
    Steinberg::Vst::IAudioProcessor*  processor_  = nullptr;
    Steinberg::IPlugView*             plugView_   = nullptr;

    std::unique_ptr<Vst3ComponentHandler> handler_;
    std::unique_ptr<Vst3PlugFrame>        plugFrame_;
    PluginWindow*                         window_ = nullptr;

    // process() に渡すバッファ群。事前確保して RT で確保しない。
    Steinberg::Vst::ProcessData        processData_{};
    Steinberg::Vst::AudioBusBuffers    inputBus_{};
    Steinberg::Vst::AudioBusBuffers    outputBus_{};
    Steinberg::Vst::ProcessContext     processContext_{};
    std::unique_ptr<Vst3ParamQueue>    inputParamChanges_;   // ロックフリー

#if defined(Q_OS_WIN)
    HMODULE      module_ = nullptr;
#elif defined(Q_OS_MACOS)
    CFBundleRef  bundle_ = nullptr;
#endif
};

} // namespace yave::plugin
```

### 8.4.3 IPlugView への接続 (OS 分岐)

**このコードが本要件の中核部分**。詳細な実装は [12章 スニペット 2](12-snippets.md) を参照。

```cpp
bool Vst3Host::attachEditorTo(PluginWindow* w)
{
    if (!controller_) return false;

    plugView_ = controller_->createView(Steinberg::Vst::ViewType::kEditor);
    if (!plugView_) return false;

    // プラットフォーム種別文字列を選ぶ
#if defined(Q_OS_WIN)
    const Steinberg::FIDString platformType = Steinberg::kPlatformTypeHWND;
#elif defined(Q_OS_MACOS)
    const Steinberg::FIDString platformType = Steinberg::kPlatformTypeNSView;
#else
    const Steinberg::FIDString platformType = Steinberg::kPlatformTypeX11EmbedWindowID;
#endif

    if (plugView_->isPlatformTypeSupported(platformType) != Steinberg::kResultTrue) {
        plugView_->release();
        plugView_ = nullptr;
        return false;
    }

    // IPlugFrame を先に設定する (resizeView の通知先)
    plugFrame_ = std::make_unique<Vst3PlugFrame>(w);
    plugView_->setFrame(plugFrame_.get());

    // 推奨サイズを取得してウィンドウを合わせてから attach する
    Steinberg::ViewRect rect{};
    if (plugView_->getSize(&rect) == Steinberg::kResultOk)
        w->resizeClientArea(rect.getWidth(), rect.getHeight());

    void* handle = w->nativeViewHandle();
    if (plugView_->attached(handle, platformType) != Steinberg::kResultOk) {
        plugView_->setFrame(nullptr);
        plugView_->release();
        plugView_ = nullptr;
        return false;
    }

    w->setResizableByUser(plugView_->canResize() == Steinberg::kResultTrue);
    w->setDetachHandler([this] { closeEditor(); });
    return true;
}
```

### 8.4.4 IPlugFrame

プラグインからのリサイズ要求を受け取る。

```cpp
// src/plugin/vst3/Vst3PlugFrame.h
class Vst3PlugFrame : public Steinberg::IPlugFrame
{
public:
    explicit Vst3PlugFrame(PluginWindow* w) : window_(w) {}

    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect* newSize) override
    {
        if (!window_ || !newSize) return Steinberg::kResultFalse;
        window_->onPluginRequestedResize(newSize->getWidth(), newSize->getHeight());
        // ウィンドウをリサイズした後、プラグインに新しいサイズを通知し返す
        view->onSize(newSize);
        return Steinberg::kResultOk;
    }

    // FUnknown の実装 (queryInterface / addRef / release)
    DECLARE_FUNKNOWN_METHODS

private:
    PluginWindow* window_ = nullptr;
};
```

### 8.4.5 RT 処理

```cpp
void Vst3Host::processRt(float* const* buffers, int numChannels, int numFrames) noexcept
{
    if (!processor_ || !processingActive_.load(std::memory_order_relaxed))
        return;

    // 事前確保済みの構造体を書き換えるだけ。確保はしない。
    inputBus_.numChannels        = numChannels;
    inputBus_.channelBuffers32   = const_cast<float**>(buffers);
    outputBus_.numChannels       = numChannels;
    outputBus_.channelBuffers32  = const_cast<float**>(buffers);   // in-place 処理

    processData_.numSamples      = numFrames;
    processData_.numInputs       = 1;
    processData_.numOutputs      = 1;
    processData_.inputs          = &inputBus_;
    processData_.outputs         = &outputBus_;
    processData_.processContext  = &processContext_;

    // UI から積まれたパラメータ変更をロックフリーキューから取り出す
    inputParamChanges_->drainInto(processData_.inputParameterChanges);

    processor_->process(processData_);

    inputParamChanges_->clearQueues();
}
```

> **in-place 処理にする理由**: 別バッファを用意すると RT スレッドで
> バッファ切り替えのロジックが増え、チェーンが長いとコピーが積み重なる。
> VST3 は in-place 処理を許容している (`ProcessSetup` で `symbolicSampleSize` を
> 設定していれば、多くのプラグインが対応)。
> 対応していないプラグインは `IAudioProcessor::canProcessSampleSize` や
> 実測で検出し、そのプラグインのみ別バッファ経路に切り替える。

### 8.4.6 状態保存

```cpp
QByteArray Vst3Host::saveState() const
{
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << quint32(0x59565333);          // "YVS3" マジック
    ds << quint32(1);                    // フォーマットバージョン

    // IComponent の状態
    Steinberg::MemoryStream compStream;
    component_->getState(&compStream);
    ds << QByteArray(compStream.getData(), int(compStream.getSize()));

    // IEditController の状態 (別に持つプラグインがある)
    Steinberg::MemoryStream ctrlStream;
    if (controller_ && controller_->getState(&ctrlStream) == Steinberg::kResultOk)
        ds << QByteArray(ctrlStream.getData(), int(ctrlStream.getSize()));
    else
        ds << QByteArray();

    return out;
}
```

プロジェクト JSON には Base64 で埋め込む ([9章](09-project-io.md))。

## 8.5 AviUtl ホスト (Windows のみ)

### 8.5.1 前提

- **x64 のみを対象とする。** 32bit プラグインはロードしない
  (同一プロセスにロードするという要件を満たせないため)。
  32bit DLL を検出した場合は `PluginDescriptor::isValid = false` とし、
  理由を「32bit プラグインはサポートされません」として一覧に表示する。
- **macOS では完全に無効化する。** ソースファイル自体をコンパイル対象から外す
  ([12章 スニペット 1](12-snippets.md))。
- AviUtl 本体のフィルタ API (`filter.h` の `FILTER` / `FILTER_DLL` 構造体、
  `GetFilterTable` / `GetFilterTableList` エクスポート) に準拠する。

### 8.5.2 Win32 依存の切り離し方

```
src/plugin/aviutl/          ← このディレクトリ配下のみ Win32 に依存する
  AviUtlHost.h/.cpp             windows.h を include する唯一の場所の一つ
  AviUtlRegistry.h/.cpp
  AviUtlFrameBridge.h/.cpp      RGBA <-> YC48 変換 (SIMD)
  AviUtlSubtitleEffectAdapter.h/.cpp
  AviUtlWindowProc.h/.cpp

src/plugin/PluginManager.cpp  ← ここでは #if defined(Q_OS_WIN) で
                                 AviUtlRegistry の生成のみを分岐する
```

`PluginManager.h` は AviUtl の型を一切知らない。前方宣言のみを置く。

```cpp
// src/plugin/PluginManager.h
namespace yave::plugin {
class Vst3Registry;
class SubtitleEffectRegistry;
class AviUtlRegistry;          // Windows 以外では定義されないが、宣言だけなら問題ない

class PluginManager : public QObject
{
    // ...
private:
    Vst3Registry*           vst3_       = nullptr;
    SubtitleEffectRegistry* subtitleFx_ = nullptr;
    AviUtlRegistry*         aviutl_     = nullptr;   // 非 Windows では常に nullptr
};
}
```

```cpp
// src/plugin/PluginManager.cpp
#if defined(Q_OS_WIN) && defined(YAVE_ENABLE_AVIUTL)
#  include "aviutl/AviUtlRegistry.h"
#endif

void PluginManager::scanAsync()
{
    vst3_->scanAsync();
    subtitleFx_->scanAsync();
#if defined(Q_OS_WIN) && defined(YAVE_ENABLE_AVIUTL)
    if (!aviutl_) aviutl_ = new AviUtlRegistry(this);
    aviutl_->scanAsync();
#endif
}
```

> ポインタメンバは不完全型でも宣言できる。デストラクタで `delete` する箇所だけ
> `#if` で囲む必要があるので、`unique_ptr` ではなく生ポインタ + 明示的な破棄にしている。

### 8.5.3 AviUtl SDK の構造体

`third_party/aviutl_sdk/filter.h` に同梱する (AviUtl の公開 SDK 由来)。

```c
typedef struct {
    int    flag;
    int    x, y;
    TCHAR* name;
    int    track_n;
    TCHAR**track_name;
    int*   track_default;
    int*   track_s;
    int*   track_e;
    int    check_n;
    TCHAR**check_name;
    int*   check_default;
    BOOL  (*func_proc)(void* fp, void* fpip);
    BOOL  (*func_init)(void* fp);
    BOOL  (*func_exit)(void* fp);
    BOOL  (*func_update)(void* fp, int status);
    BOOL  (*func_WndProc)(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                          void* editp, void* fp);
    int   *track;
    int   *check;
    void  *ex_data_ptr;
    TCHAR *information;
    /* ... 以下略 ... */
    HINSTANCE dll_hinst;
    void  *ex_data_def;
    /* ... */
} FILTER_DLL;
```

`FILTER_PROC_INFO` (`fpip`) が入出力ピクセルバッファを持つ。

```c
typedef struct {
    int    flag;
    PIXEL_YC *ycp_edit;      // 編集用バッファ (YC48)
    PIXEL_YC *ycp_temp;      // 一時バッファ
    int    w, h;
    int    max_w, max_h;
    int    frame;
    int    frame_n;
    int    org_w, org_h;
    short *audiop;
    int    audio_n;
    int    audio_ch;
    /* ... */
    int    line_size;        // 1 ラインのバイト数
    /* ... */
} FILTER_PROC_INFO;
```

### 8.5.4 AviUtlHost

```cpp
// src/plugin/aviutl/AviUtlHost.h      ★ Windows のみ
#pragma once
#include <windows.h>
#include "third_party/aviutl_sdk/filter.h"
#include "../PluginDescriptor.h"

namespace yave::plugin {

class AviUtlHost : public QObject
{
    Q_OBJECT
public:
    explicit AviUtlHost(QObject* parent = nullptr);
    ~AviUtlHost() override;

    /// x64 検証 -> LoadLibraryW -> GetFilterTable(List) -> func_init
    bool load(const QString& dllPath, QString* errorOut = nullptr);
    void unload();

    int          filterCount() const;
    FILTER_DLL*  filter(int index) const;

    /// RGBA8 のフレームにフィルタを適用する。
    /// 内部で YC48 へ変換 -> func_proc -> RGBA へ戻す。
    bool applyFilter(int filterIndex,
                     QImage& rgbaInOut,
                     int frameIndex,
                     int totalFrames,
                     const yave::sdk::ParameterValues& params);

    /// フィルタの track / check 定義から ParameterSchema を生成する。
    yave::sdk::ParameterSchema buildSchema(int filterIndex) const;

    /// 設定ウィンドウを開く。フィルタが func_WndProc を持つ場合のみ。
    PluginWindow* openEditor(int filterIndex, QWidget* parent = nullptr);
    void          closeEditor(int filterIndex);

private:
    static bool isX64Dll(const QString& path, QString* errorOut);
    void        installHostFunctionTable();   // AviUtl 本体が提供する API の代替実装

    HMODULE                   module_ = nullptr;
    std::vector<FILTER_DLL*>  filters_;
    std::vector<std::unique_ptr<AviUtlFilterState>> states_;
    QString                   dllPath_;
};

} // namespace yave::plugin
```

### 8.5.5 x64 判定

```cpp
bool AviUtlHost::isX64Dll(const QString& path, QString* errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QObject::tr("Cannot open file.");
        return false;
    }
    // DOS ヘッダ -> e_lfanew -> PE ヘッダ -> Machine
    IMAGE_DOS_HEADER dos{};
    if (f.read(reinterpret_cast<char*>(&dos), sizeof(dos)) != sizeof(dos)
        || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        if (errorOut) *errorOut = QObject::tr("Not a valid DLL.");
        return false;
    }
    f.seek(dos.e_lfanew);
    DWORD sig = 0;
    IMAGE_FILE_HEADER fh{};
    f.read(reinterpret_cast<char*>(&sig), sizeof(sig));
    f.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    if (sig != IMAGE_NT_SIGNATURE) {
        if (errorOut) *errorOut = QObject::tr("Not a valid PE image.");
        return false;
    }
    if (fh.Machine != IMAGE_FILE_MACHINE_AMD64) {
        if (errorOut)
            *errorOut = QObject::tr("32-bit plugins are not supported. "
                                    "Only 64-bit (x64) AviUtl plugins can be loaded.");
        return false;
    }
    return true;
}
```

> **ロードする前にファイルを読んで判定する**ことが重要。
> `LoadLibraryW` に 32bit DLL を渡すと `ERROR_BAD_EXE_FORMAT` になるだけだが、
> エラーメッセージが不親切になるため、事前に明確な理由を出す。

### 8.5.6 フィルタテーブルの取得

```cpp
bool AviUtlHost::load(const QString& dllPath, QString* errorOut)
{
    if (!isX64Dll(dllPath, errorOut))
        return false;

    module_ = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!module_) {
        if (errorOut) *errorOut = QObject::tr("LoadLibrary failed (error %1).")
                                      .arg(GetLastError());
        return false;
    }

    using GetFilterTableListFn = FILTER_DLL** (__stdcall*)();
    using GetFilterTableFn     = FILTER_DLL*  (__stdcall*)();

    // 複数フィルタを持つ DLL が優先
    if (auto listFn = reinterpret_cast<GetFilterTableListFn>(
            GetProcAddress(module_, "GetFilterTableList"))) {
        FILTER_DLL** table = listFn();
        for (int i = 0; table && table[i]; ++i)
            filters_.push_back(table[i]);
    }
    else if (auto oneFn = reinterpret_cast<GetFilterTableFn>(
            GetProcAddress(module_, "GetFilterTable"))) {
        if (FILTER_DLL* f = oneFn())
            filters_.push_back(f);
    }

    if (filters_.empty()) {
        if (errorOut) *errorOut = QObject::tr("No AviUtl filter table found in this DLL.");
        FreeLibrary(module_);
        module_ = nullptr;
        return false;
    }

    // 各フィルタを初期化
    for (FILTER_DLL* f : filters_) {
        f->dll_hinst = module_;
        f->hwnd      = nullptr;
        f->exfunc    = &hostExFunctions();     // AviUtl 本体 API の代替 (8.5.8)
        if (f->func_init && !f->func_init(f)) {
            qCWarning(lcPlugin) << "func_init failed for" << QString::fromWCharArray(f->name);
        }
    }
    dllPath_ = dllPath;
    return true;
}
```

### 8.5.7 フレームブリッジ (RGBA ⇄ YC48)

AviUtl の内部形式は **YC48** (Y: 0..4096, Cb/Cr: -2048..2048、各 16bit signed)。

```cpp
// src/plugin/aviutl/AviUtlFrameBridge.h
#pragma pack(push, 1)
struct PIXEL_YC { short y; short cb; short cr; };
#pragma pack(pop)

class AviUtlFrameBridge
{
public:
    /// RGBA8 (premultiplied ではない) -> YC48
    /// line_size は 1 ラインのバイト数。AviUtl は 4 バイト境界に揃えることを期待する。
    static void rgbaToYc48(const QImage& src, PIXEL_YC* dst, int lineSize);

    /// YC48 -> RGBA8
    static void yc48ToRgba(const PIXEL_YC* src, int lineSize, QImage& dst);

    /// 必要なバッファサイズ (max_w * max_h を考慮)
    static size_t requiredBufferBytes(int maxW, int maxH);
};
```

変換式 (BT.601、AviUtl の実装に合わせる):

```
Y  = ( 4918*R + 9655*G + 1875*B) >> 14           // 0..4096 相当
Cb = ((-2775*R - 5448*G + 8223*B) >> 14) + 0     // -2048..2048
Cr = (( 8223*R - 6887*G - 1336*B) >> 14) + 0
```

**SSE2 / AVX2 による SIMD 実装を用意する。**
4K フレーム (830 万画素) のスカラー変換は 1 フレームあたり 20ms 超になり、
リアルタイムプレビューが破綻する。

```cpp
#if defined(__AVX2__)
void rgbaToYc48_avx2(const uint8_t* src, PIXEL_YC* dst, int width, int height, int srcStride);
#endif
```

> **精度上の注意**: RGBA → YC48 → RGBA のラウンドトリップは可逆ではない。
> AviUtl フィルタを通すと必ず微小な色ずれが生じる。
> これは AviUtl 側の仕様であり回避できないため、
> UI で「AviUtl フィルタを適用すると色空間変換が発生します」と注記する。

### 8.5.8 AviUtl 本体 API の代替実装

AviUtl フィルタは `FILTER::exfunc` 経由で本体の機能を呼ぶ。
本アプリは AviUtl ではないため、必要最小限の関数を自前で提供する。

```cpp
// src/plugin/aviutl/AviUtlExFunctions.cpp
namespace {

int  ex_get_frame(void* editp)                     { return currentContext().frame; }
int  ex_get_frame_n(void* editp)                   { return currentContext().frameCount; }
BOOL ex_get_frame_size(void* editp, int* w, int* h){ *w = currentContext().width;
                                                     *h = currentContext().height; return TRUE; }
void* ex_get_ycp_filtering(void* fp, void* editp, int frame, void* reserve) { /* ... */ }
BOOL ex_is_editing(void* editp)                    { return TRUE; }
BOOL ex_is_saving(void* editp)                     { return currentContext().exporting; }
// ... 以下、実装する関数を列挙

} // anonymous namespace

EXFUNC& hostExFunctions()
{
    static EXFUNC fns = [] {
        EXFUNC f{};
        f.get_frame        = &ex_get_frame;
        f.get_frame_n      = &ex_get_frame_n;
        f.get_frame_size   = &ex_get_frame_size;
        f.get_ycp_filtering= &ex_get_ycp_filtering;
        f.is_editing       = &ex_is_editing;
        f.is_saving        = &ex_is_saving;
        // 未実装の関数は nullptr のまま。
        // フィルタが nullptr チェックせずに呼ぶとクラッシュするため、
        // 未実装関数には「何もしないスタブ」を必ず入れる。
        return f;
    }();
    return fns;
}
```

**サポートする API の範囲を文書化する。**
すべての AviUtl フィルタが動くわけではないことを明示し、
互換性リストを Wiki 等で管理する運用にする。

> 完全な互換を目指すのは非現実的。
> 「映像フィルタとして `func_proc` だけで完結するもの」を主対象とし、
> 本体のタイムライン API に深く依存するもの (拡張編集連携など) は非対応とする。

### 8.5.9 設定ウィンドウ (func_WndProc)

```cpp
PluginWindow* AviUtlHost::openEditor(int filterIndex, QWidget* parent)
{
    FILTER_DLL* f = filters_[filterIndex];
    if (!f->func_WndProc)
        return nullptr;     // GUI 無し -> ParameterSchema から自動生成フォームを使う

    auto* win = new PluginWindow(descriptorFor(filterIndex), parent);
    HWND hwnd = reinterpret_cast<HWND>(win->nativeViewHandle());

    f->hwnd = hwnd;

    // AviUtl は WM_FILTER_INIT で初期化することを期待している
    f->func_WndProc(hwnd, WM_FILTER_INIT, 0, 0, nullptr, f);

    win->setDetachHandler([this, f, hwnd] {
        f->func_WndProc(hwnd, WM_FILTER_EXIT, 0, 0, nullptr, f);
        f->hwnd = nullptr;
    });

    win->show();
    return win;
}
```

`PluginWindow::nativeEvent` で受け取った Win32 メッセージをフィルタへ中継する。

```cpp
bool PluginWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#if defined(Q_OS_WIN)
    if (eventType == "windows_generic_MSG" && aviutlFilter_) {
        MSG* msg = static_cast<MSG*>(message);
        if (aviutlFilter_->func_WndProc(msg->hwnd, msg->message, msg->wParam, msg->lParam,
                                        nullptr, aviutlFilter_)) {
            // TRUE が返ったら「フィルタの設定が変更された」ことを意味する
            emit aviutlParametersChanged();
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
```

## 8.6 字幕エフェクトプラグイン

[6.6](06-subtitle-engine.md) で定義した `ISubtitleEffect` のロードを担う。

```cpp
// src/plugin/subtitle/SubtitleEffectRegistry.h
class SubtitleEffectRegistry : public QObject
{
    Q_OBJECT
public:
    void scanAsync();

    /// effectId からプロトタイプを取得。一覧表示と parameterSchema() の取得に使う。
    /// 所有権は Registry 側。クリップへ積む用途には使わない。
    const yave::sdk::ISubtitleEffect* prototype(const QString& effectId) const;

    /// クリップへ積むための新しいインスタンスを生成する。
    /// prepare() の前計算結果をインスタンスが保持するため、共有できない。
    /// 組み込みも外部プラグインも同じ経路で生成される。
    SubtitleEffectPtr createInstance(const QString& effectId) const;

    /// 一覧 (UI のエフェクト追加メニュー用)
    std::vector<PluginDescriptor> effects() const;

    /// 未インストールのプラグインを参照している場合の情報
    bool isMissing(const QString& effectId) const;

signals:
    void scanFinished();

private:
    void registerBuiltins();     // src/subtitle/effects/ の実装を登録

    struct Entry
    {
        SubtitleEffectPtr prototype;                     // 一覧 / schema 用
        std::function<SubtitleEffectPtr()> factory;      // インスタンス生成
        QString pluginId;                                // 組み込みなら空
    };
    std::unordered_map<QString, Entry>                 byId_;
    std::vector<std::unique_ptr<LoadedSubtitlePlugin>> loaded_;
};
```

**組み込みエフェクトも外部プラグインも同じ `byId_` に入る。**
`prototype()` / `createInstance()` の呼び出し側は両者を区別しない。
組み込みの `factory` は `[] { return SubtitleEffectPtr(new TypewriterEffect(),
[](auto* p){ delete p; }); }`、外部プラグインの `factory` は
`SubtitleEffectFactoryV1::createEffect` / `destroyEffect` を包んだものになる。

## 8.7 プラグイン設定の永続化

| 種別 | 保存内容 |
|---|---|
| VST3 | `nativeId` (FUID) + `saveState()` の Base64 + バイパス状態 + トラック内の位置 |
| 字幕エフェクト | `effectId` + `pluginId` + `ParameterValues` の JSON + `enabled` |
| AviUtl | `dllPath` の相対化 + `filterIndex` + `track[]` / `check[]` の値 + `ex_data` の Base64 |

### 8.7.1 未インストールプラグインの扱い

**設定を破棄しない。** これは強く守るべき原則。

```cpp
// ロード時
auto* effect = registry->resolve(effectId);
SubtitleEffectInstance inst;
inst.effectId = effectId;
inst.pluginId = pluginId;
inst.params   = ParameterValues::fromJson(o["params"].toObject());
inst.effect   = effect;
inst.missing  = (effect == nullptr);     // 見つからなくても params は保持
```

- 未インストールのエフェクトは UI で「⚠ 見つかりません: com.example.glitch」と表示し、
  無効状態でスタックに残す。
- **保存時にはそのまま書き戻す。**
- 後でプラグインをインストールすれば、そのまま復活する。

> これをやらないと、「プラグインを入れていない環境で開いて保存したら
> 設定が全部消えた」という事故が起きる。チームで作業する場合に致命的。

## 8.8 クラッシュ耐性

### 8.8.1 走査時の保護

```cpp
// Windows: SEH で保護
#if defined(Q_OS_WIN)
bool safeScan(const QString& path, PluginDescriptor* out)
{
    __try {
        return doScan(path, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        PluginManager::instance().blacklist(path, "SEH exception during scan");
        return false;
    }
}
#endif
```

> `__try`/`__except` は C++ オブジェクトのデストラクタを持つ関数では使えないため、
> `doScan` を別関数に切り出し、`safeScan` 自体には自動変数を置かない。

### 8.8.2 ブラックリスト

クラッシュしたプラグインは `plugin_cache.json` のブラックリストに入り、
次回起動時はロードしない。設定画面から手動で解除できる。

### 8.8.3 プロセス分離 (将来拡張)

完全な隔離には別プロセスでのホスティングが必要だが、
以下の理由から初期バージョンでは同一プロセスとする。

- AviUtl は要件で「同一プロセス内でロード」と指定されている
- VST3 の別プロセス化は GUI の埋め込み (子プロセスの HWND を親に attach) と
  RT オーディオの IPC が必要で、実装コストとレイテンシの両面で重い

設計上は `Vst3Host` をインタフェース化しておき、
将来 `Vst3RemoteHost` を差し込めるようにしておく。
