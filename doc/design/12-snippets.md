# 12. 重要実装コード断片

[← 目次に戻る](../design.md)

---

## 12.1 CMakeLists での AviUtl コード分離

### 12.1.1 src/plugin/CMakeLists.txt

**AviUtl 関連のソースファイルを、非 Windows ではコンパイル対象に入れない。**
`#ifdef` でファイル全体を囲む方式は採らない (include 忘れやリンクエラーの温床になる)。

```cmake
# src/plugin/CMakeLists.txt

# ---------------------------------------------------------------------------
# 共通ソース (全プラットフォーム)
# ---------------------------------------------------------------------------
set(YAVE_PLUGIN_COMMON_SOURCES
    PluginManager.cpp
    PluginManager.h
    PluginDescriptor.h
    PluginWindow.cpp
    PluginWindow.h

    vst3/Vst3Host.cpp
    vst3/Vst3Host.h
    vst3/Vst3Registry.cpp
    vst3/Vst3Registry.h
    vst3/Vst3ComponentHandler.cpp
    vst3/Vst3ComponentHandler.h
    vst3/Vst3PlugFrame.cpp
    vst3/Vst3PlugFrame.h
    vst3/Vst3ProcessorNode.cpp
    vst3/Vst3ProcessorNode.h
    vst3/Vst3ParamQueue.cpp
    vst3/Vst3ParamQueue.h

    subtitle/SubtitleEffectLoader.cpp
    subtitle/SubtitleEffectLoader.h
    subtitle/SubtitleEffectRegistry.cpp
    subtitle/SubtitleEffectRegistry.h
    subtitle/BuiltinEffectFactory.cpp
    subtitle/BuiltinEffectFactory.h
)

add_library(yave_plugin STATIC ${YAVE_PLUGIN_COMMON_SOURCES})

target_include_directories(yave_plugin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(yave_plugin
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        yave_core
        yave_audio
        yave_subtitle
    PRIVATE
        sdk_hosting          # VST3 SDK
        base
        pluginterfaces
)

# ---------------------------------------------------------------------------
# AviUtl プラグインホスト  ★ Windows 専用
#
#   - macOS / Linux ではソースファイル自体をターゲットに追加しない。
#   - YAVE_ENABLE_AVIUTL は OFF にできる (Windows でも無効化可能)。
#   - 有効時のみ YAVE_ENABLE_AVIUTL コンパイル定義が入り、
#     PluginManager.cpp 側の #if defined(YAVE_ENABLE_AVIUTL) が有効になる。
# ---------------------------------------------------------------------------
if(WIN32 AND YAVE_ENABLE_AVIUTL)

    message(STATUS "AviUtl plugin host: ENABLED (Windows x64)")

    target_sources(yave_plugin PRIVATE
        aviutl/AviUtlHost.cpp
        aviutl/AviUtlHost.h
        aviutl/AviUtlRegistry.cpp
        aviutl/AviUtlRegistry.h
        aviutl/AviUtlFrameBridge.cpp
        aviutl/AviUtlFrameBridge.h
        aviutl/AviUtlExFunctions.cpp
        aviutl/AviUtlExFunctions.h
        aviutl/AviUtlSubtitleEffectAdapter.cpp
        aviutl/AviUtlSubtitleEffectAdapter.h
        aviutl/AviUtlWindowProc.cpp
        aviutl/AviUtlWindowProc.h
    )

    target_include_directories(yave_plugin PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/aviutl_sdk
    )

    target_compile_definitions(yave_plugin PUBLIC
        YAVE_ENABLE_AVIUTL=1
        NOMINMAX                    # windows.h の min/max マクロを抑止
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    target_link_libraries(yave_plugin PRIVATE
        user32
        gdi32
        shlwapi
    )

    # AviUtl SDK のヘッダは古い書き方を含むため、
    # このターゲットのみ一部警告を抑制する。
    if(MSVC)
        set_source_files_properties(
            aviutl/AviUtlHost.cpp
            aviutl/AviUtlExFunctions.cpp
            PROPERTIES COMPILE_OPTIONS "/wd4996;/wd4267"
        )
    endif()

    # YC48 変換の SIMD 実装。AVX2 が使える環境向けに別 TU としてビルドする。
    # (実行時に CPUID で分岐するため、AVX2 を全体に有効化はしない)
    add_library(yave_yc48_avx2 OBJECT aviutl/AviUtlFrameBridgeAvx2.cpp)
    target_include_directories(yave_yc48_avx2 PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/aviutl_sdk
        ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(yave_yc48_avx2 PRIVATE /arch:AVX2)
    else()
        target_compile_options(yave_yc48_avx2 PRIVATE -mavx2 -mfma)
    endif()
    target_link_libraries(yave_plugin PRIVATE yave_yc48_avx2)

else()

    if(YAVE_ENABLE_AVIUTL AND NOT WIN32)
        # ルート CMakeLists で FATAL_ERROR にしているのでここには来ないが、
        # 単独でこのディレクトリを構成した場合の保険。
        message(FATAL_ERROR "YAVE_ENABLE_AVIUTL requires Windows.")
    endif()

    message(STATUS "AviUtl plugin host: DISABLED")

endif()

# ---------------------------------------------------------------------------
# macOS 固有
# ---------------------------------------------------------------------------
if(APPLE)
    target_sources(yave_plugin PRIVATE
        ../platform/mac/MacNativeView.mm
    )
    set_source_files_properties(../platform/mac/MacNativeView.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(yave_plugin PRIVATE
        "-framework Cocoa"
        "-framework CoreFoundation"
    )
endif()
```

### 12.1.2 呼び出し側での分岐

ヘッダには `#ifdef` を書かない。前方宣言だけで済ませ、
`.cpp` 側でのみ分岐する。

```cpp
// src/plugin/PluginManager.h   ← ここに #ifdef は書かない
namespace yave::plugin {

class Vst3Registry;
class SubtitleEffectRegistry;
class AviUtlRegistry;               // Windows 以外では定義されないが宣言は問題ない

class PluginManager : public QObject
{
    // ...
private:
    Vst3Registry*           vst3_       = nullptr;
    SubtitleEffectRegistry* subtitleFx_ = nullptr;
    AviUtlRegistry*         aviutl_     = nullptr;   // 非 Windows では常に nullptr
};

} // namespace yave::plugin
```

```cpp
// src/plugin/PluginManager.cpp
#include "PluginManager.h"
#include "vst3/Vst3Registry.h"
#include "subtitle/SubtitleEffectRegistry.h"

#if defined(YAVE_ENABLE_AVIUTL)
#  include "aviutl/AviUtlRegistry.h"
#endif

namespace yave::plugin {

bool PluginManager::isAviUtlSupported()
{
#if defined(YAVE_ENABLE_AVIUTL)
    return true;
#else
    return false;
#endif
}

PluginManager::PluginManager()
    : vst3_(new Vst3Registry(this))
    , subtitleFx_(new SubtitleEffectRegistry(this))
{
#if defined(YAVE_ENABLE_AVIUTL)
    aviutl_ = new AviUtlRegistry(this);
#endif
}

PluginManager::~PluginManager()
{
    // 生ポインタは QObject の親子関係で破棄されるが、
    // 明示的に破棄する場合はここも #if で囲む必要がある。
    // ここでは親を this にしているので何もしない。
}

void PluginManager::scanAsync()
{
    emit scanStarted();
    vst3_->scanAsync();
    subtitleFx_->scanAsync();
#if defined(YAVE_ENABLE_AVIUTL)
    aviutl_->scanAsync();
#endif
}

std::vector<PluginDescriptor> PluginManager::plugins(PluginKind kind) const
{
    QMutexLocker lock(&mutex_);
    switch (kind) {
    case PluginKind::Vst3:           return vst3_->descriptors();
    case PluginKind::SubtitleEffect: return subtitleFx_->descriptors();
    case PluginKind::AviUtlFilter:
    case PluginKind::AviUtlInput:
#if defined(YAVE_ENABLE_AVIUTL)
        return aviutl_->descriptors(kind);
#else
        return {};      // macOS では常に空。UI 側は特別な分岐を書かなくてよい
#endif
    }
    return {};
}

} // namespace yave::plugin
```

> **UI 側に `#ifdef` を書かないための設計**: macOS では
> `plugins(PluginKind::AviUtlFilter)` が空リストを返すだけなので、
> QML 側は「AviUtl プラグインが 0 件」という状態を普通に描画すればよい。
> `isAviUtlSupported()` は、設定画面で AviUtl の検索パス設定 UI 自体を
> 非表示にするためだけに使う。

---

## 12.2 PluginWindow からネイティブハンドルを取得して VST3 の IPlugView に接続

### 12.2.1 PluginWindow 側 — ネイティブハンドルの生成と取得

```cpp
// src/plugin/PluginWindow.cpp
#include "PluginWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include <QScreen>
#include <QWindow>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace yave::plugin {

PluginWindow::PluginWindow(const PluginDescriptor& desc, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , desc_(desc)
{
    setWindowTitle(desc.name);
    setAttribute(Qt::WA_DeleteOnClose, false);   // ライフタイムは呼び出し側が管理する

    // AviUtl は DPI 非対応のものが大半。
    // ネイティブウィンドウを作る前に判定しておく必要がある。
    dpiUnaware_ = (desc.kind == PluginKind::AviUtlFilter ||
                   desc.kind == PluginKind::AviUtlInput);

    createNativeContainer();

    // プラグイン GUI の下に、ホスト側の付随 UI を置く
    auto* toolbar = new QHBoxLayout;
    bypassButton_ = new QPushButton(this);
    bypassButton_->setCheckable(true);
    presetLabel_  = new QLabel(this);
    resetButton_  = new QPushButton(this);
    toolbar->addWidget(bypassButton_);
    toolbar->addWidget(presetLabel_);
    toolbar->addStretch();
    toolbar->addWidget(resetButton_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(container_, 1);
    root->addLayout(toolbar);

    retranslateUi();
}

void PluginWindow::createNativeContainer()
{
    container_ = new QWidget(this);

    // WA_NativeWindow: この QWidget に実際の HWND / NSView を作らせる。
    //   これを立てないと Qt は「アルファウィジェット」として扱い、
    //   winId() を呼んだ時点で初めて生成される (タイミングが不定になる)。
    // WA_DontCreateNativeAncestors: 親ウィジェット群までネイティブ化されるのを防ぐ。
    //   これが無いと描画パスが遅くなり、QML 側との合成にも影響が出る。
    // WA_NoSystemBackground / WA_OpaquePaintEvent:
    //   Qt が背景を塗るとプラグインの描画とちらつきを起こすため抑止する。
    container_->setAttribute(Qt::WA_NativeWindow, true);
    container_->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    container_->setAttribute(Qt::WA_NoSystemBackground, true);
    container_->setAttribute(Qt::WA_OpaquePaintEvent, true);
    container_->setFocusPolicy(Qt::StrongFocus);

#if defined(Q_OS_WIN)
    if (dpiUnaware_) {
        // DPI 非対応プラグイン用に、このウィンドウだけ DPI 仮想化を有効にする。
        // ウィンドウ生成の瞬間の awareness context が採用されるため、
        // winId() を呼ぶ前後で切り替える必要がある。
        DPI_AWARENESS_CONTEXT prev =
            SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
        (void)container_->winId();
        SetThreadDpiAwarenessContext(prev);
        return;
    }
#endif

    // winId() を呼ぶことでネイティブハンドルの生成を確定させる
    (void)container_->winId();
}

void* PluginWindow::nativeViewHandle() const
{
    // WId は Windows では HWND、macOS では NSView* を表す。
    //   -- ここが本設計の要点 --
    //   macOS で NSWindow* を返してしまうと VST3 側は
    //   kPlatformTypeNSView を期待しているため GUI が表示されない。
    //   Qt の winId() は NSView* を返すので、そのまま渡してよい。
    return reinterpret_cast<void*>(container_->winId());
}

void PluginWindow::resizeClientArea(int width, int height)
{
    // container_ をその大きさにしたときのウィンドウ全体サイズを求める
    const int extraH = height + (this->height() - container_->height());
    const int extraW = width  + (this->width()  - container_->width());
    resize(extraW, extraH);
}

void PluginWindow::onPluginRequestedResize(int width, int height)
{
    container_->setMinimumSize(width, height);
    container_->setMaximumSize(resizable_ ? QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
                                          : QSize(width, height));
    resizeClientArea(width, height);
}

void PluginWindow::setResizableByUser(bool on)
{
    resizable_ = on;
    if (!on) {
        container_->setFixedSize(container_->size());
        setFixedSize(size());
    } else {
        container_->setMinimumSize(0, 0);
        container_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
}

void PluginWindow::closeEvent(QCloseEvent* e)
{
    // プラグインを detach してからウィンドウを閉じる。
    // 順序を誤ると、破棄済みの NSView / HWND にプラグインが描画してクラッシュする。
    if (detachHandler_)
        detachHandler_();
    emit closed();
    QWidget::closeEvent(e);
}

void PluginWindow::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void PluginWindow::retranslateUi()
{
    bypassButton_->setText(tr("Bypass"));
    presetLabel_->setText(tr("Preset:"));
    resetButton_->setText(tr("Reset"));
    resetButton_->setToolTip(tr("Reset all parameters to their default values"));
}

void PluginWindow::raiseAndActivate()
{
    show();
    raise();
    activateWindow();
}

} // namespace yave::plugin
```

### 12.2.2 Vst3Host 側 — IPlugView への attach (OS 分岐)

```cpp
// src/plugin/vst3/Vst3Host.cpp
#include "Vst3Host.h"
#include "Vst3PlugFrame.h"
#include "../PluginWindow.h"

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/base/funknown.h>

#include <QGuiApplication>
#include <QScreen>

using namespace Steinberg;

namespace yave::plugin {

namespace {

/// このプラットフォームで VST3 に渡すべき platform type 文字列を返す。
///
///   Windows : kPlatformTypeHWND               -> HWND
///   macOS   : kPlatformTypeNSView             -> NSView*
///   Linux   : kPlatformTypeX11EmbedWindowID   -> X11 Window ID (参考)
///
/// VST3 SDK では以下のように定義されている (pluginterfaces/gui/iplugview.h):
///   const FIDString kPlatformTypeHWND = "HWND";
///   const FIDString kPlatformTypeNSView = "NSView";
constexpr FIDString currentPlatformType()
{
#if defined(Q_OS_WIN)
    return kPlatformTypeHWND;
#elif defined(Q_OS_MACOS)
    return kPlatformTypeNSView;
#else
    return kPlatformTypeX11EmbedWindowID;
#endif
}

} // anonymous namespace


PluginWindow* Vst3Host::openEditor(QWidget* parent)
{
    if (window_) {                       // 既に開いている
        window_->raiseAndActivate();
        return window_;
    }
    if (!controller_)
        return nullptr;

    auto* win = new PluginWindow(descriptor_, parent);
    if (!attachEditorTo(win)) {
        delete win;
        return nullptr;
    }
    window_ = win;
    window_->show();
    return window_;
}


bool Vst3Host::attachEditorTo(PluginWindow* w)
{
    Q_ASSERT(controller_);
    Q_ASSERT(!plugView_);

    // ---- (1) IPlugView を作る --------------------------------------------
    plugView_ = controller_->createView(Vst::ViewType::kEditor);
    if (!plugView_) {
        qCInfo(lcPlugin) << descriptor_.name << "has no editor view";
        return false;
    }

    // ---- (2) このプラットフォームに対応しているか確認 ----------------------
    const FIDString platformType = currentPlatformType();
    if (plugView_->isPlatformTypeSupported(platformType) != kResultTrue) {
        qCWarning(lcPlugin) << descriptor_.name
                            << "does not support platform type" << platformType;
        plugView_->release();
        plugView_ = nullptr;
        return false;
    }

    // ---- (3) High DPI のスケールを先に通知する ---------------------------
    //     attached() の後に通知すると、プラグインによっては
    //     初期サイズが誤ったまま固定されてしまう。
    if (auto scaleSupport = FUnknownPtr<IPlugViewContentScaleSupport>(plugView_)) {
        const qreal dpr = w->windowHandle()
                            ? w->windowHandle()->devicePixelRatio()
                            : qGuiApp->primaryScreen()->devicePixelRatio();
        scaleSupport->setContentScaleFactor(static_cast<IPlugViewContentScaleSupport::ScaleFactor>(dpr));
    }

    // ---- (4) IPlugFrame を設定する (attached より前に必須) ----------------
    //     プラグインは attached() の中で resizeView() を呼ぶことがある。
    //     frame が未設定だとその要求が失われ、GUI が正しいサイズにならない。
    plugFrame_ = std::make_unique<Vst3PlugFrame>(w);
    if (plugView_->setFrame(plugFrame_.get()) != kResultOk) {
        qCWarning(lcPlugin) << "setFrame failed for" << descriptor_.name;
        // 続行する。setFrame を実装していないプラグインもある。
    }

    // ---- (5) 推奨サイズを取得してウィンドウを合わせる ---------------------
    ViewRect rect{};
    if (plugView_->getSize(&rect) == kResultOk && rect.getWidth() > 0)
        w->resizeClientArea(rect.getWidth(), rect.getHeight());

    // ---- (6) ★ ネイティブハンドルを取得して attach ★ ---------------------
    //     Windows : container_ の HWND
    //     macOS   : container_ の NSView*
    //     どちらも QWidget::winId() の戻り値をそのまま使える。
    void* nativeHandle = w->nativeViewHandle();
    if (!nativeHandle) {
        qCWarning(lcPlugin) << "native view handle is null";
        plugView_->setFrame(nullptr);
        plugView_->release();
        plugView_ = nullptr;
        plugFrame_.reset();
        return false;
    }

    const tresult res = plugView_->attached(nativeHandle, platformType);
    if (res != kResultOk) {
        qCWarning(lcPlugin) << "IPlugView::attached failed for" << descriptor_.name
                            << "result =" << res;
        plugView_->setFrame(nullptr);
        plugView_->release();
        plugView_ = nullptr;
        plugFrame_.reset();
        return false;
    }

    // ---- (7) attach 後の状態を反映 ----------------------------------------
    w->setResizableByUser(plugView_->canResize() == kResultTrue);

    // ウィンドウを閉じるときに必ず removed() を呼ばせる。
    // これを忘れると、破棄済みビューへプラグインが描画してクラッシュする。
    w->setDetachHandler([this] { closeEditor(); });

    // attached() の後にもう一度サイズを取り直す。
    // プラグインによっては attach 時に実サイズが確定する。
    ViewRect after{};
    if (plugView_->getSize(&after) == kResultOk && after.getWidth() > 0)
        w->resizeClientArea(after.getWidth(), after.getHeight());

    qCInfo(lcPlugin) << "Editor attached:" << descriptor_.name
                     << "platform =" << platformType
                     << "size =" << after.getWidth() << "x" << after.getHeight();
    return true;
}


void Vst3Host::closeEditor()
{
    if (plugView_) {
        plugView_->removed();          // detach。attached と対になる
        plugView_->setFrame(nullptr);
        plugView_->release();
        plugView_ = nullptr;
    }
    plugFrame_.reset();

    if (window_) {
        window_->setDetachHandler({});  // 再入を防ぐ
        window_->deleteLater();
        window_ = nullptr;
    }
}

} // namespace yave::plugin
```

### 12.2.3 Vst3PlugFrame — リサイズ要求の受け口

```cpp
// src/plugin/vst3/Vst3PlugFrame.h
#pragma once

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/base/funknown.h>

namespace yave::plugin {

class PluginWindow;

/// プラグインからホストへのリサイズ要求を受け取る。
class Vst3PlugFrame : public Steinberg::IPlugFrame
{
public:
    explicit Vst3PlugFrame(PluginWindow* window) : window_(window) {}
    virtual ~Vst3PlugFrame() = default;

    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect* newSize) override
    {
        if (!window_ || !newSize)
            return Steinberg::kResultFalse;

        // 1. ホスト側のウィンドウを先にリサイズする
        window_->onPluginRequestedResize(newSize->getWidth(), newSize->getHeight());

        // 2. リサイズが完了したことをプラグインへ通知する。
        //    この呼び出しを忘れると、プラグインは自身の描画バッファを
        //    更新せず、表示が崩れたままになる。
        if (view)
            view->onSize(newSize);

        return Steinberg::kResultTrue;
    }

    // ---- FUnknown ----
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                                 void** obj) override
    {
        QUERY_INTERFACE(iid, obj, Steinberg::FUnknown::iid, Steinberg::IPlugFrame)
        QUERY_INTERFACE(iid, obj, Steinberg::IPlugFrame::iid, Steinberg::IPlugFrame)
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef()  override { return 1; }   // ホストが所有
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

private:
    PluginWindow* window_ = nullptr;
};

} // namespace yave::plugin
```

> **`addRef` / `release` が 1 を返す理由**: `Vst3PlugFrame` の寿命は
> `Vst3Host` が `unique_ptr` で管理している。VST3 の参照カウントに
> 破棄を任せると、`Vst3Host` の破棄と二重解放になる。
> ホストが所有するコールバックオブジェクトでは、この「参照カウントを
> 使わない」実装が一般的。

### 12.2.4 macOS 固有の注意点

```objc
// src/platform/mac/MacNativeView.mm
#import <Cocoa/Cocoa.h>
#include <QWidget>

namespace yave::plugin {

/// QWidget::winId() が返す値が本当に NSView* かを検証する (デバッグ用)。
bool verifyIsNSView(void* handle)
{
    if (!handle) return false;
    id obj = (__bridge id)handle;
    return [obj isKindOfClass:[NSView class]];
}

/// プラグインビューを attach した後に、親 NSView のレイヤーバッキングを
/// 有効にする。これをしないと一部のプラグインで描画が乱れる。
void enableLayerBacking(void* handle)
{
    if (!handle) return;
    NSView* view = (__bridge NSView*)handle;
    [view setWantsLayer:YES];
}

} // namespace yave::plugin
```

---

## 12.3 ProjectSerializer — 無限レイヤーと AI パラメータの入れ子シリアライズ

**本節が JSON 永続化の中核。**
`tracks` の動的配列 → 各トラックの `clips` 配列 → 字幕クリップの `effectStack` 配列、
および `aiTasks` の `params` オブジェクトを入れ子で構築する。

### 12.3.1 シリアライズ

```cpp
// src/io/ProjectSerializer.cpp
#include "ProjectSerializer.h"
#include "JsonKeys.h"
#include "EnumMapping.h"
#include "PathResolver.h"

#include "../core/Project.h"
#include "../core/Timeline.h"
#include "../core/Track.h"
#include "../core/VideoClip.h"
#include "../core/AudioClip.h"
#include "../core/AiPlaceholderClip.h"
#include "../subtitle/SubtitleClip.h"
#include "../ai/AiGenerationOrchestrator.h"
#include "../plugin/vst3/Vst3Host.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QDateTime>

namespace yave::io {

using namespace keys;

// ===========================================================================
//  ヘルパ
// ===========================================================================
namespace {

QJsonObject rationalToJson(const Rational& r)
{
    QJsonObject o;
    o[QStringLiteral("num")] = double(r.num);
    o[QStringLiteral("den")] = double(r.den);
    return o;
}

Rational rationalFromJson(const QJsonObject& o, const Rational& fallback)
{
    if (!o.contains(QStringLiteral("num"))) return fallback;
    return Rational(int64_t(o[QStringLiteral("num")].toDouble()),
                    int64_t(o[QStringLiteral("den")].toDouble(1)));
}

QJsonObject timeRangeToJson(const TimeRange& r)
{
    QJsonObject o;
    o[kRangeStart]    = double(r.start);
    o[kRangeDuration] = double(r.duration);
    return o;
}

TimeRange timeRangeFromJson(const QJsonObject& o)
{
    TimeRange r;
    r.start    = int64_t(o[kRangeStart].toDouble());
    r.duration = int64_t(o[kRangeDuration].toDouble());
    return r;
}

QString uuidToJson(const QUuid& u)
{
    return u.isNull() ? QString() : u.toString(QUuid::WithoutBraces);
}

QUuid uuidFromJson(const QJsonValue& v)
{
    const QString s = v.toString();
    return s.isEmpty() ? QUuid() : QUuid(s);
}

/// int64 を JSON に安全に入れる。
/// QJsonValue は double 保持なので 2^53 を超える値は精度が落ちる。
/// フレーム番号は 2^53 に達しない (59.94fps で 476 万年) ため double で問題ない。
/// ただし seed は 64bit を取りうるので文字列で保存する。
QString int64ToJsonString(int64_t v) { return QString::number(v); }
int64_t int64FromJsonString(const QJsonValue& v, int64_t fallback)
{
    if (v.isString()) { bool ok = false; const auto r = v.toString().toLongLong(&ok);
                        return ok ? r : fallback; }
    if (v.isDouble()) return int64_t(v.toDouble());
    return fallback;
}

} // anonymous namespace


// ===========================================================================
//  トップレベル
// ===========================================================================
QJsonObject ProjectSerializer::serializeProject(const Project& p, const SaveOptions& o)
{
    // ルートにも未知フィールド保持を効かせる (9.11.2)。
    // schemaVersion 2 で storyBible を追加したため、これが無いと
    // 古いビルドで開いて保存した瞬間に作品設定が消える。
    QJsonObject root = p.unknownRootFields();

    root[kSchemaVersion] = kCurrentSchemaVersion;   // 2 (13章で AIトラックを追加)

    QJsonObject app;
    app[QStringLiteral("name")]    = QStringLiteral("YAVE");
    app[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    root[QStringLiteral("application")] = app;
    root[QStringLiteral("savedAt")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // ---- プロジェクト設定 ----
    QJsonObject proj;
    proj[QStringLiteral("name")]       = p.name();
    proj[QStringLiteral("timebase")]   = rationalToJson(p.timebase());
    QJsonObject canvas;
    canvas[QStringLiteral("width")]    = p.canvasSize().width();
    canvas[QStringLiteral("height")]   = p.canvasSize().height();
    proj[QStringLiteral("canvasSize")] = canvas;
    proj[QStringLiteral("sampleRate")] = p.sampleRate();
    proj[QStringLiteral("channels")]   = p.channels();
    proj[QStringLiteral("duration")]   = double(p.timeline()->duration());
    proj[QStringLiteral("playhead")]   = double(p.playhead());
    proj[QStringLiteral("workRange")]  = timeRangeToJson(p.workRange());
    proj[QStringLiteral("colorSpace")] = p.colorSpaceName();
    root[kProject] = proj;

    // ---- アセット / スタイルプリセット ----
    root[kAssets] = serializeAssets(p, o);
    root[QStringLiteral("subtitleStylePresets")] = serializeSubtitleStylePresets(p);

    // ---- 作品設定 (AIトラックのプロンプトへカスケードする。13.3.4) ----
    root[kStoryBible] = p.storyBible().toJson();

    // ---- ★ 無限レイヤー: Track の動的リスト ----
    root[kTracks] = serializeTracks(*p.timeline(), o);

    // ---- AI タスク ----
    // Cached / Committed かつ生きた OutputBinding から参照されているものだけを書く。
    // 全部書くと再生成のたびにレコードが積み上がり、サイズ予算を破る (9.5.2)。
    root[kAiTasks] = serializeAiTasks(p);

    // ---- マスター / 設定 ----
    QJsonObject master;
    master[QStringLiteral("gain")] = p.masterGain();
    master[kEffectChain] = serializeVst3Chain(p.masterEffectChain());
    root[QStringLiteral("masterAudio")] = master;

    QJsonObject settings;
    settings[QStringLiteral("pdcEnabled")]   = p.isPdcEnabled();
    settings[QStringLiteral("proxyEnabled")] = p.isProxyEnabled();
    settings[QStringLiteral("autoCommitAi")] = p.isAutoCommitAi();
    root[QStringLiteral("settings")] = settings;

    return root;
}


// ===========================================================================
//  ★ 無限レイヤー (Track の動的リスト) -> QJsonArray
// ===========================================================================
QJsonArray ProjectSerializer::serializeTracks(const Timeline& tl, const SaveOptions& o)
{
    QJsonArray arr;

    // 配列の順序がそのまま Z オーダーになる。
    // index 0 = 最背面 ... index N-1 = 最前面。
    // zOrder フィールドは意図的に持たない (二重管理を避けるため)。
    const int n = tl.trackCount();
    for (int i = 0; i < n; ++i) {
        const Track* t = tl.trackAt(i);
        if (!t) continue;
        arr.append(serializeTrack(*t, o));
    }
    return arr;
}


QJsonObject ProjectSerializer::serializeTrack(const Track& t, const SaveOptions& o)
{
    // Clip と同様、認識できなかったフィールドを先に入れて既知フィールドで上書きする。
    // TrackType::Unknown のトラックは、これによって原文がそのまま保存される (9.11.2)。
    QJsonObject obj = t.unknownFields();

    obj[kTrackId]      = uuidToJson(t.id());
    obj[kTrackName]    = t.name();
    obj[kTrackType]    = enumToString(t.type());          // "video" / "audio" / "storyboard" / ...
    obj[kTrackVisible] = t.isVisible();
    obj[kTrackLocked]  = t.isLocked();

    if (!o.omitDefaultValues || t.isMuted()) obj[QStringLiteral("muted")] = t.isMuted();
    if (!o.omitDefaultValues || t.isSolo())  obj[QStringLiteral("solo")]  = t.isSolo();

    obj[QStringLiteral("height")] = t.uiHeight();
    obj[QStringLiteral("color")]  = t.color().name(QColor::HexRgb);

    switch (t.type()) {
    case TrackType::Video:
    case TrackType::AiGenerated:
        obj[QStringLiteral("opacity")]   = t.opacity();
        obj[QStringLiteral("blendMode")] = enumToString(t.blendMode());
        break;
    case TrackType::Audio:
        obj[QStringLiteral("gain")] = t.gain();
        obj[QStringLiteral("pan")]  = t.pan();
        break;
    case TrackType::Subtitle:
        obj[QStringLiteral("defaultStylePresetId")] = t.defaultStylePresetId();
        break;
    case TrackType::Storyboard:
    case TrackType::Unknown:
        // 合成にもオーディオグラフにも参加しないため、固有フィールドは持たない
        break;
    }

    // ---- AIトラック関連 (13章)。全トラック型で任意 ----
    if (!o.omitDefaultValues || !t.aiRole().isEmpty())
        obj[kAiRole] = t.aiRole();
    obj[kStoryboardTrackId] = t.storyboardTrackId().isNull()
                            ? QJsonValue(QJsonValue::Null)
                            : QJsonValue(uuidToJson(t.storyboardTrackId()));
    if (!t.roleDefaultsPatch().isEmpty())
        obj[kRoleDefaults] = t.roleDefaultsPatch();

    // ---- クリップ配列 (時刻順) ----
    QJsonArray clips;
    for (const std::shared_ptr<Clip>& c : t.clips()) {
        if (!c) continue;
        clips.append(serializeClip(*c, o));
    }
    obj[kClips] = clips;

    // ---- トラックのエフェクトチェーン (VST3) ----
    if (!t.effectChain().empty())
        obj[kEffectChain] = serializeVst3Chain(t.effectChain());

    return obj;
}


// ===========================================================================
//  クリップ (基底 + 型ごとの分岐)
// ===========================================================================
QJsonObject ProjectSerializer::serializeClip(const Clip& c, const SaveOptions& o)
{
    // 認識できなかったフィールドを先に入れておき、既知フィールドで上書きする。
    // これにより、新しいバージョンで追加されたデータを古いバージョンで
    // 開いて保存しても失われない (前方互換)。
    QJsonObject obj = c.unknownFields();

    obj[kClipId]   = uuidToJson(c.id());
    obj[kClipType] = enumToString(c.type());
    obj[kRange]    = timeRangeToJson(c.range());

    if (!c.name().isEmpty())          obj[QStringLiteral("name")]    = c.name();
    if (!o.omitDefaultValues || !c.isEnabled()) obj[kEnabled]        = c.isEnabled();
    if (!o.omitDefaultValues || c.isLocked())   obj[QStringLiteral("locked")] = c.isLocked();
    if (!o.omitDefaultValues || c.opacity() != 1.0)
        obj[QStringLiteral("opacity")] = c.opacity();
    if (!o.omitDefaultValues || c.blendMode() != BlendMode::Normal)
        obj[QStringLiteral("blendMode")] = enumToString(c.blendMode());
    if (c.fadeInFrames()  > 0) obj[QStringLiteral("fadeIn")]  = double(c.fadeInFrames());
    if (c.fadeOutFrames() > 0) obj[QStringLiteral("fadeOut")] = double(c.fadeOutFrames());

    if (!c.generatedByTaskId().isNull())
        obj[QStringLiteral("generatedByTaskId")] = uuidToJson(c.generatedByTaskId());

    // ---- 型ごとの追加フィールド ----
    switch (c.type()) {

    case ClipType::Video:
    case ClipType::Image: {
        const auto& vc = static_cast<const VideoClip&>(c);
        obj[kAssetId]      = uuidToJson(vc.assetId());
        obj[kSourceOffset] = double(vc.sourceOffset());
        obj[QStringLiteral("speed")]    = vc.speed();
        obj[QStringLiteral("reversed")] = vc.isReversed();
        obj[QStringLiteral("transform")] = transformToJson(vc.transform());
        obj[QStringLiteral("crop")]      = rectToJson(vc.cropRect());
        if (!vc.effectChain().empty())
            obj[kEffectChain] = serializeClipEffectChain(vc.effectChain());
        break;
    }

    case ClipType::Audio: {
        const auto& ac = static_cast<const AudioClip&>(c);
        obj[kAssetId]      = uuidToJson(ac.assetId());
        obj[kSourceOffset] = double(ac.sourceOffset());
        obj[QStringLiteral("gain")] = ac.gain();
        obj[QStringLiteral("pan")]  = ac.pan();
        obj[QStringLiteral("fadeInCurve")]  = enumToString(ac.fadeInCurve());
        obj[QStringLiteral("fadeOutCurve")] = enumToString(ac.fadeOutCurve());
        break;
    }

    case ClipType::Subtitle: {
        // ★ 字幕は専用の入れ子構造を持つ
        const auto& sc = static_cast<const subtitle::SubtitleClip&>(c);
        mergeInto(obj, serializeSubtitleClip(sc, o));
        break;
    }

    case ClipType::AiPlaceholder: {
        const auto& pc = static_cast<const AiPlaceholderClip&>(c);
        obj[QStringLiteral("taskId")] = uuidToJson(pc.taskId());
        obj[QStringLiteral("state")]  = enumToString(pc.state());
        break;
    }

    case ClipType::Color: {
        const auto& cc = static_cast<const ColorClip&>(c);
        obj[QStringLiteral("color")] = cc.color().name(QColor::HexArgb);
        break;
    }

    case ClipType::Cut: {
        // 演出指示 (13章)。フィールド数が多いので CutClip 自身に任せる。
        const auto& cut = static_cast<const CutClip&>(c);
        mergeInto(obj, cut.toJson());
        break;
    }
    }

    return obj;
}


// ===========================================================================
//  ★ 字幕クリップ: テキスト / スタイル差分 / エフェクトスタック の入れ子
// ===========================================================================
QJsonObject ProjectSerializer::serializeSubtitleClip(const subtitle::SubtitleClip& c,
                                                     const SaveOptions& o)
{
    Q_UNUSED(o);
    QJsonObject obj;

    obj[kStylePresetId] = c.stylePresetId();

    // ---- テキスト (プレーン + リッチスパン) ----
    QJsonObject text;
    text[kTextPlain] = c.text().plain();

    QJsonArray spans;
    for (const subtitle::TextSpan& s : c.text().spans()) {
        QJsonObject so;
        so[QStringLiteral("start")]  = s.start;
        so[QStringLiteral("length")] = s.length;
        if (s.bold)        so[QStringLiteral("bold")]       = *s.bold;
        if (s.italic)      so[QStringLiteral("italic")]     = *s.italic;
        if (s.underline)   so[QStringLiteral("underline")]  = *s.underline;
        if (s.color)       so[QStringLiteral("color")]      = s.color->name(QColor::HexArgb);
        if (s.fontFamily)  so[QStringLiteral("fontFamily")] = *s.fontFamily;
        if (s.sizeScale)   so[QStringLiteral("sizeScale")]  = *s.sizeScale;
        if (!s.ruby.isEmpty()) so[QStringLiteral("ruby")]   = s.ruby;
        spans.append(so);
    }
    if (!spans.isEmpty())
        text[kTextSpans] = spans;
    obj[kText] = text;

    // ---- スタイル差分 (プリセットからのオーバーライド分のみ) ----
    const QJsonObject styleDiff = styleDiffToJson(c.styleOverride());
    if (!styleDiff.isEmpty())
        obj[kStyleOverride] = styleDiff;

    // ---- ★ エフェクトスタック ----
    if (!c.effectStack().empty())
        obj[kEffectStack] = serializeSubtitleEffectStack(c.effectStack());

    // ---- STT 由来の単語タイミング ----
    if (c.hasWordTimings()) {
        QJsonArray wt;
        for (const auto& w : c.wordTimings()) {
            QJsonObject wo;
            wo[QStringLiteral("charStart")]  = w.charStart;
            wo[QStringLiteral("charLength")] = w.charLength;
            wo[QStringLiteral("startSec")]   = w.startSec;
            wo[QStringLiteral("endSec")]     = w.endSec;
            wt.append(wo);
        }
        obj[kWordTimings] = wt;
    }

    return obj;
}


QJsonArray ProjectSerializer::serializeSubtitleEffectStack(
        const std::vector<subtitle::SubtitleEffectInstance>& stack)
{
    QJsonArray arr;

    // 配列の順序 = 適用順。index 0 が最初に適用される。
    for (const subtitle::SubtitleEffectInstance& inst : stack) {
        QJsonObject o;
        o[kInstanceId] = uuidToJson(inst.instanceId);
        o[kEffectId]   = inst.effectId;
        if (!inst.pluginId.isEmpty())
            o[kPluginId] = inst.pluginId;
        o[kEnabled]    = inst.enabled;

        // ★ パラメータは QVariant マップ -> QJsonObject へ。
        //    未インストールプラグイン (inst.missing == true) の場合も
        //    inst.params には読み込み時の値がそのまま入っているため、
        //    何も失わずに書き戻せる。
        o[kParams]     = inst.params.toJson();

        arr.append(o);
    }
    return arr;
}


// ===========================================================================
//  ★ AI 生成パラメータ (入れ子オブジェクト)
// ===========================================================================
QJsonObject ProjectSerializer::serializeAiParams(const ai::AiGenerationParams& p)
{
    QJsonObject o;

    // ---- 共通 ----
    o[kKind]          = enumToString(p.kind);            // "video" / "audio" / "subtitle" / ...
    o[kTargetTrackId] = uuidToJson(p.targetTrackId);
    o[kRange]         = timeRangeToJson(p.range);
    o[kModelId]       = p.modelId;
    if (!p.providerId.isEmpty()) o[kProviderId] = p.providerId;
    o[kPrompt]        = p.prompt;
    if (!p.negativePrompt.isEmpty()) o[kNegativePrompt] = p.negativePrompt;

    // seed は 64bit を取りうるので文字列で保存する
    // (QJsonValue は double 保持のため 2^53 を超えると精度が落ちる)
    o[kSeed]          = int64ToJsonString(p.seed);
    o[QStringLiteral("steps")]         = p.steps;
    o[QStringLiteral("guidanceScale")] = p.guidanceScale;

    // ---- 種別ごとの入れ子 ----
    switch (p.kind) {

    case ai::GenerationKind::Video:
    case ai::GenerationKind::Image: {
        o[kVideoMode] = enumToString(p.videoMode);   // "textToVideo"/"imageToVideo"/"videoToVideo"

        if (p.videoMode == ai::VideoGenMode::ImageToVideo) {
            // ★ I2V の 3 パターン
            //    "startFrameOnly" / "endFrameOnly" / "bothEnds"
            o[kI2vRefMode] = enumToString(p.i2vRefMode);

            // 参照画像はそれぞれネストしたオブジェクトとして保存する
            o[kStartReference] = p.startReference
                                   ? imageRefToJson(*p.startReference)
                                   : QJsonValue(QJsonValue::Null);
            o[kEndReference]   = p.endReference
                                   ? imageRefToJson(*p.endReference)
                                   : QJsonValue(QJsonValue::Null);
        }
        else if (p.videoMode == ai::VideoGenMode::VideoToVideo) {
            o[kVideoReference] = p.videoReference
                                   ? videoRefToJson(*p.videoReference)
                                   : QJsonValue(QJsonValue::Null);
        }

        QJsonObject res;
        res[QStringLiteral("width")]  = p.outputResolution.width();
        res[QStringLiteral("height")] = p.outputResolution.height();
        o[QStringLiteral("outputResolution")] = res;
        o[QStringLiteral("outputFrameRate")]  = rationalToJson(p.outputFrameRate);
        o[QStringLiteral("loopSeamless")]     = p.loopSeamless;
        break;
    }

    case ai::GenerationKind::Audio: {
        o[QStringLiteral("audioMode")]    = enumToString(p.audioMode);
        o[QStringLiteral("voiceId")]      = p.voiceId;
        o[QStringLiteral("speakingRate")] = p.speakingRate;
        o[QStringLiteral("pitch")]        = p.pitch;
        if (!p.referenceAudioPath.isEmpty())
            o[QStringLiteral("referenceAudioPath")] = p.referenceAudioPath;
        o[QStringLiteral("audioSampleRate")] = p.audioSampleRate;
        o[QStringLiteral("audioChannels")]   = p.audioChannels;
        o[QStringLiteral("targetLufs")]      = p.targetLufs;
        break;
    }

    case ai::GenerationKind::Subtitle: {
        o[QStringLiteral("subtitleMode")]        = enumToString(p.subtitleMode);
        o[QStringLiteral("sourceAudioTrackId")]  = uuidToJson(p.sourceAudioTrackId);
        o[QStringLiteral("language")]            = p.language;
        if (!p.targetLanguage.isEmpty())
            o[QStringLiteral("targetLanguage")]  = p.targetLanguage;
        o[QStringLiteral("wordLevelTimestamps")] = p.wordLevelTimestamps;
        o[QStringLiteral("subtitleStylePresetId")] = p.subtitleStylePresetId;
        if (p.maxCharsPerCue > 0)
            o[QStringLiteral("maxCharsPerCue")]  = p.maxCharsPerCue;
        if (p.minCueDurationFrames > 0)
            o[QStringLiteral("minCueDurationFrames")] = double(p.minCueDurationFrames);
        break;
    }

    case ai::GenerationKind::Mask: {
        o[QStringLiteral("maskSourceTrackId")]     = uuidToJson(p.maskSourceTrackId);
        o[QStringLiteral("maskTargetDescription")] = p.maskTargetDescription;
        QJsonArray hints;
        for (const QPointF& pt : p.maskHintPoints) {
            QJsonObject h;
            h[QStringLiteral("x")] = pt.x();
            h[QStringLiteral("y")] = pt.y();
            hints.append(h);
        }
        if (!hints.isEmpty()) o[QStringLiteral("maskHintPoints")] = hints;
        o[QStringLiteral("maskTrackAcrossFrames")] = p.maskTrackAcrossFrames;
        o[QStringLiteral("maskFeather")]           = p.maskFeather;
        break;
    }

    case ai::GenerationKind::EffectMetadata:
        break;
    }

    // ---- 出力の扱い ----
    o[QStringLiteral("replaceExistingClips")] = p.replaceExistingClips;
    o[QStringLiteral("createNewTrack")]       = p.createNewTrack;

    // ---- モデル固有の追加パラメータ (そのまま入れ子で保存) ----
    if (!p.extraParams.isEmpty())
        o[kExtraParams] = p.extraParams;

    return o;
}


QJsonObject ProjectSerializer::serializeAiTask(const ai::AiGenerationTask& t)
{
    QJsonObject o;
    o[QStringLiteral("id")]           = uuidToJson(t.id());
    o[QStringLiteral("state")]        = enumToString(t.state());
    o[QStringLiteral("createdAt")]    = t.createdAt().toString(Qt::ISODate);
    if (t.completedAt().isValid())
        o[QStringLiteral("completedAt")] = t.completedAt().toString(Qt::ISODate);
    o[QStringLiteral("retryCount")]   = t.retryCount();
    if (!t.errorMessage().isEmpty())
        o[QStringLiteral("errorMessage")] = t.errorMessage();

    // ★ 生成パラメータを完全に永続化する。
    //    これによりキャッシュが消えても同じ設定で再生成できる。
    o[kParams] = serializeAiParams(t.params());

    // ---- 生成済みアセット ----
    QJsonArray assets;
    for (const ai::GeneratedAsset& a : t.assets()) {
        QJsonObject ao;
        ao[QStringLiteral("type")]      = enumToString(a.type);
        ao[QStringLiteral("path")]      = a.path;         // プロジェクト相対
        ao[QStringLiteral("collected")] = a.collected;
        QJsonObject res;
        res[QStringLiteral("width")]  = a.resolution.width();
        res[QStringLiteral("height")] = a.resolution.height();
        ao[QStringLiteral("resolution")]     = res;
        ao[QStringLiteral("durationFrames")] = double(a.durationFrames);
        ao[QStringLiteral("frameRate")]      = rationalToJson(a.frameRate);
        if (!a.metadata.isEmpty())
            ao[QStringLiteral("metadata")]   = a.metadata;
        assets.append(ao);
    }
    o[QStringLiteral("assets")] = assets;

    return o;
}


QJsonArray ProjectSerializer::serializeAiTasks(const Project& p)
{
    QJsonArray arr;
    if (!p.aiOrchestrator()) return arr;
    for (const ai::AiGenerationTask* t : p.aiOrchestrator()->tasks()) {
        if (!t) continue;
        // Cancelled のタスクは保存しない (ノイズになるため)
        if (t->state() == ai::TaskState::Cancelled) continue;
        arr.append(serializeAiTask(*t));
    }
    return arr;
}

} // namespace yave::io
```

### 12.3.2 デシリアライズ

```cpp
// src/io/ProjectSerializer.cpp (続き)
namespace yave::io {

// ===========================================================================
//  ★ QJsonArray -> 無限レイヤー (Track の動的リスト)
// ===========================================================================
void ProjectSerializer::deserializeTracks(Timeline* tl, const QJsonArray& arr,
                                          Project* p, LoadResult* r)
{
    // 配列の順序をそのまま Z オーダーとして復元する。
    // 途中のトラックが壊れていても、後続を読み込めるようにする
    // (1 トラックの破損で全体が開けなくなるのを避ける)。
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue v = arr.at(i);
        if (!v.isObject()) {
            r->warnings.append(QObject::tr("Track #%1 is not an object; skipped.").arg(i));
            continue;
        }
        auto track = deserializeTrack(v.toObject(), p, r);
        if (!track) {
            r->warnings.append(QObject::tr("Track #%1 could not be loaded; skipped.").arg(i));
            continue;
        }
        // 末尾へ追加していけば、配列順 = Z オーダーが自然に再現される
        tl->reinsertTrack(tl->trackCount(), std::move(track));
    }
}


std::unique_ptr<Track> ProjectSerializer::deserializeTrack(const QJsonObject& o,
                                                           Project* p, LoadResult* r)
{
    // TrackType だけは既定フォールバックを使わない。
    // 未知の型を Video に落とすと、新種のトラックが別の意味で合成に混ざる (9.9.2)。
    const TrackType type = parseTrackType(o[kTrackType].toString());   // 未知なら Unknown
    auto track = std::make_unique<Track>(type);
    if (type == TrackType::Unknown) {
        r->warnings.append(QObject::tr("Unknown track type '%1' was preserved read-only.")
                               .arg(o[kTrackType].toString()));
    }

    const QUuid id = uuidFromJson(o[kTrackId]);
    track->setId(id.isNull() ? QUuid::createUuid() : id);
    track->setName(o[kTrackName].toString());
    track->setVisible(o[kTrackVisible].toBool(true));
    track->setLocked(o[kTrackLocked].toBool(false));
    track->setMuted(o[QStringLiteral("muted")].toBool(false));
    track->setSolo(o[QStringLiteral("solo")].toBool(false));
    track->setUiHeight(o[QStringLiteral("height")].toInt(64));
    if (o.contains(QStringLiteral("color")))
        track->setColor(QColor(o[QStringLiteral("color")].toString()));

    switch (type) {
    case TrackType::Video:
    case TrackType::AiGenerated:
        track->setOpacity(o[QStringLiteral("opacity")].toDouble(1.0));
        track->setBlendMode(enumFromString<BlendMode>(
            o[QStringLiteral("blendMode")].toString(), BlendMode::Normal));
        break;
    case TrackType::Audio:
        track->setGain(o[QStringLiteral("gain")].toDouble(1.0));
        track->setPan(o[QStringLiteral("pan")].toDouble(0.0));
        break;
    case TrackType::Subtitle:
        track->setDefaultStylePresetId(
            o[QStringLiteral("defaultStylePresetId")].toString(QStringLiteral("default")));
        break;
    case TrackType::Storyboard:
    case TrackType::Unknown:
        break;
    }

    // ---- AIトラック関連 (13章) ----
    track->setAiRole(o[kAiRole].toString());
    track->setStoryboardTrackId(uuidFromJson(o[kStoryboardTrackId]));
    track->setRoleDefaultsPatch(o[kRoleDefaults].toObject());

    // ---- クリップ配列 ----
    const QJsonArray clips = o[kClips].toArray();
    for (int i = 0; i < clips.size(); ++i) {
        auto clip = deserializeClip(clips.at(i).toObject(), p, r);
        if (!clip) continue;
        if (!track->insertClip(clip)) {
            // 重なりがあった。無限レイヤーなので、警告して詰める。
            r->warnings.append(
                QObject::tr("Clip '%1' on track '%2' overlaps another clip and was shifted.")
                    .arg(clip->name(), track->name()));
            // 直近の空き位置へずらして挿入
            const int64_t freeStart = track->contentDuration();
            clip->setRange({freeStart, clip->range().duration});
            track->insertClip(clip);
        }
    }

    // ---- エフェクトチェーン ----
    if (o.contains(kEffectChain))
        deserializeVst3Chain(track.get(), o[kEffectChain].toArray(), p, r);

    track->assertInvariants();
    return track;
}


std::shared_ptr<Clip> ProjectSerializer::deserializeClip(const QJsonObject& o,
                                                          Project* p, LoadResult* r)
{
    const ClipType type = enumFromString<ClipType>(o[kClipType].toString(), ClipType::Video);

    std::shared_ptr<Clip> clip;
    switch (type) {
    case ClipType::Video:
    case ClipType::Image:         clip = std::make_shared<VideoClip>();         break;
    case ClipType::Audio:         clip = std::make_shared<AudioClip>();         break;
    case ClipType::Subtitle:      return deserializeSubtitleClip(o, p, r);
    case ClipType::AiPlaceholder: clip = std::make_shared<AiPlaceholderClip>(); break;
    case ClipType::Color:         clip = std::make_shared<ColorClip>();         break;
    case ClipType::Cut:           return CutClip::fromJson(o);                  // 13章
    }
    if (!clip) return nullptr;

    // ---- 未知フィールドを保持する (前方互換) ----
    clip->setUnknownFields(extractUnknownFields(o, knownClipKeys()));

    const QUuid id = uuidFromJson(o[kClipId]);
    clip->setId(id.isNull() ? QUuid::createUuid() : id);
    clip->setRange(timeRangeFromJson(o[kRange].toObject()));
    clip->setName(o[QStringLiteral("name")].toString());
    clip->setEnabled(o[kEnabled].toBool(true));
    clip->setLocked(o[QStringLiteral("locked")].toBool(false));
    clip->setOpacity(o[QStringLiteral("opacity")].toDouble(1.0));
    clip->setBlendMode(enumFromString<BlendMode>(
        o[QStringLiteral("blendMode")].toString(), BlendMode::Normal));
    clip->setFadeInFrames(int64_t(o[QStringLiteral("fadeIn")].toDouble(0)));
    clip->setFadeOutFrames(int64_t(o[QStringLiteral("fadeOut")].toDouble(0)));
    clip->setGeneratedByTaskId(uuidFromJson(o[QStringLiteral("generatedByTaskId")]));

    // ---- 型ごとの追加フィールド ----
    if (type == ClipType::Video || type == ClipType::Image) {
        auto* vc = static_cast<VideoClip*>(clip.get());
        vc->setAssetId(uuidFromJson(o[kAssetId]));
        vc->setSourceOffset(int64_t(o[kSourceOffset].toDouble(0)));
        vc->setSpeed(o[QStringLiteral("speed")].toDouble(1.0));
        vc->setReversed(o[QStringLiteral("reversed")].toBool(false));
        vc->setTransform(transformFromJson(o[QStringLiteral("transform")].toObject()));
        vc->setCropRect(rectFromJson(o[QStringLiteral("crop")].toObject()));
    }
    else if (type == ClipType::Audio) {
        auto* ac = static_cast<AudioClip*>(clip.get());
        ac->setAssetId(uuidFromJson(o[kAssetId]));
        ac->setSourceOffset(int64_t(o[kSourceOffset].toDouble(0)));
        ac->setGain(o[QStringLiteral("gain")].toDouble(1.0));
        ac->setPan(o[QStringLiteral("pan")].toDouble(0.0));
    }
    else if (type == ClipType::AiPlaceholder) {
        auto* pc = static_cast<AiPlaceholderClip*>(clip.get());
        pc->setTaskId(uuidFromJson(o[QStringLiteral("taskId")]));
        pc->setState(enumFromString<ai::TaskState>(
            o[QStringLiteral("state")].toString(), ai::TaskState::Queued));
    }

    return clip;
}


// ===========================================================================
//  ★ 字幕クリップ (テキスト / スタイル差分 / エフェクトスタック)
// ===========================================================================
std::shared_ptr<subtitle::SubtitleClip>
ProjectSerializer::deserializeSubtitleClip(const QJsonObject& o, Project* p, LoadResult* r)
{
    auto clip = std::make_shared<subtitle::SubtitleClip>();
    clip->setUnknownFields(extractUnknownFields(o, knownSubtitleClipKeys()));

    const QUuid id = uuidFromJson(o[kClipId]);
    clip->setId(id.isNull() ? QUuid::createUuid() : id);
    clip->setRange(timeRangeFromJson(o[kRange].toObject()));
    clip->setEnabled(o[kEnabled].toBool(true));
    clip->setStylePresetId(o[kStylePresetId].toString(QStringLiteral("default")));
    clip->setGeneratedByTaskId(uuidFromJson(o[QStringLiteral("generatedByTaskId")]));

    // ---- テキスト ----
    const QJsonObject textObj = o[kText].toObject();
    subtitle::SubtitleText text;
    text.setPlain(textObj[kTextPlain].toString());
    for (const QJsonValue& sv : textObj[kTextSpans].toArray()) {
        const QJsonObject so = sv.toObject();
        subtitle::TextSpan span;
        span.start  = so[QStringLiteral("start")].toInt();
        span.length = so[QStringLiteral("length")].toInt();
        if (so.contains(QStringLiteral("bold")))
            span.bold = so[QStringLiteral("bold")].toBool();
        if (so.contains(QStringLiteral("italic")))
            span.italic = so[QStringLiteral("italic")].toBool();
        if (so.contains(QStringLiteral("underline")))
            span.underline = so[QStringLiteral("underline")].toBool();
        if (so.contains(QStringLiteral("color")))
            span.color = QColor(so[QStringLiteral("color")].toString());
        if (so.contains(QStringLiteral("fontFamily")))
            span.fontFamily = so[QStringLiteral("fontFamily")].toString();
        if (so.contains(QStringLiteral("sizeScale")))
            span.sizeScale = so[QStringLiteral("sizeScale")].toDouble();
        span.ruby = so[QStringLiteral("ruby")].toString();
        text.addSpan(span);
    }
    clip->setText(text);

    // ---- スタイル差分 ----
    if (o.contains(kStyleOverride))
        clip->setStyleOverride(styleDiffFromJson(o[kStyleOverride].toObject()));

    // ---- ★ エフェクトスタック ----
    if (o.contains(kEffectStack)) {
        auto stack = deserializeSubtitleEffectStack(o[kEffectStack].toArray(), r);
        clip->mutableEffectStack() = std::move(stack);
    }

    // ---- 単語タイミング ----
    if (o.contains(kWordTimings)) {
        std::vector<subtitle::SubtitleClip::WordTiming> wt;
        for (const QJsonValue& v : o[kWordTimings].toArray()) {
            const QJsonObject wo = v.toObject();
            wt.push_back({ wo[QStringLiteral("charStart")].toInt(),
                           wo[QStringLiteral("charLength")].toInt(),
                           wo[QStringLiteral("startSec")].toDouble(),
                           wo[QStringLiteral("endSec")].toDouble() });
        }
        clip->setWordTimings(std::move(wt));
    }

    return clip;
}


std::vector<subtitle::SubtitleEffectInstance>
ProjectSerializer::deserializeSubtitleEffectStack(const QJsonArray& arr, LoadResult* r)
{
    std::vector<subtitle::SubtitleEffectInstance> stack;
    stack.reserve(size_t(arr.size()));

    auto& pm = plugin::PluginManager::instance();

    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();

        subtitle::SubtitleEffectInstance inst;
        const QUuid iid = uuidFromJson(o[kInstanceId]);
        inst.instanceId = iid.isNull() ? QUuid::createUuid() : iid;
        inst.effectId   = o[kEffectId].toString();
        inst.pluginId   = o[kPluginId].toString();
        inst.enabled    = o[kEnabled].toBool(true);
        inst.params     = yave::sdk::ParameterValues::fromJson(o[kParams].toObject());

        // ★ プラグインが見つからなくても params を破棄しない。
        //    missing フラグを立ててスタックに残し、保存時にそのまま書き戻す。
        //    後でプラグインをインストールすれば、そのまま復活する。
        //
        //    エフェクト実装はクリップごとに専用インスタンスを生成する
        //    (prepare() の前計算結果をインスタンスが保持するため共有できない)。
        inst.effect  = pm.createSubtitleEffect(inst.effectId);
        inst.missing = (inst.effect == nullptr);

        if (inst.missing) {
            r->missingPluginIds.append(inst.effectId);
            r->warnings.append(
                QObject::tr("Subtitle effect '%1' is not installed. "
                            "Its settings are preserved but it will not be applied.")
                    .arg(inst.effectId));
        }

        stack.push_back(std::move(inst));
    }
    return stack;
}


// ===========================================================================
//  ★ AI 生成パラメータの復元
// ===========================================================================
ai::AiGenerationParams ProjectSerializer::deserializeAiParams(const QJsonObject& o)
{
    ai::AiGenerationParams p;

    p.kind          = enumFromString<ai::GenerationKind>(o[kKind].toString(),
                                                         ai::GenerationKind::Video);
    p.targetTrackId = uuidFromJson(o[kTargetTrackId]);
    p.range         = timeRangeFromJson(o[kRange].toObject());
    p.modelId       = o[kModelId].toString();
    p.providerId    = o[kProviderId].toString();
    p.prompt        = o[kPrompt].toString();
    p.negativePrompt= o[kNegativePrompt].toString();
    p.seed          = int64FromJsonString(o[kSeed], -1);
    p.steps         = o[QStringLiteral("steps")].toInt(30);
    p.guidanceScale = o[QStringLiteral("guidanceScale")].toDouble(7.5);

    switch (p.kind) {

    case ai::GenerationKind::Video:
    case ai::GenerationKind::Image: {
        p.videoMode = enumFromString<ai::VideoGenMode>(
            o[kVideoMode].toString(), ai::VideoGenMode::TextToVideo);

        if (p.videoMode == ai::VideoGenMode::ImageToVideo) {
            // ★ I2V の 3 パターンを復元
            p.i2vRefMode = enumFromString<ai::I2VReferenceMode>(
                o[kI2vRefMode].toString(), ai::I2VReferenceMode::StartFrameOnly);

            if (o[kStartReference].isObject())
                p.startReference = ai::ImageReference::fromJson(
                    o[kStartReference].toObject());
            if (o[kEndReference].isObject())
                p.endReference = ai::ImageReference::fromJson(
                    o[kEndReference].toObject());
        }
        else if (p.videoMode == ai::VideoGenMode::VideoToVideo) {
            if (o[kVideoReference].isObject())
                p.videoReference = ai::VideoReference::fromJson(
                    o[kVideoReference].toObject());
        }

        const QJsonObject res = o[QStringLiteral("outputResolution")].toObject();
        p.outputResolution = QSize(res[QStringLiteral("width")].toInt(1280),
                                   res[QStringLiteral("height")].toInt(720));
        p.outputFrameRate  = rationalFromJson(
            o[QStringLiteral("outputFrameRate")].toObject(), timebase::Fps30);
        p.loopSeamless     = o[QStringLiteral("loopSeamless")].toBool(false);
        break;
    }

    case ai::GenerationKind::Audio: {
        p.audioMode    = enumFromString<ai::AudioGenMode>(
            o[QStringLiteral("audioMode")].toString(), ai::AudioGenMode::Narration);
        p.voiceId      = o[QStringLiteral("voiceId")].toString();
        p.speakingRate = o[QStringLiteral("speakingRate")].toDouble(1.0);
        p.pitch        = o[QStringLiteral("pitch")].toDouble(0.0);
        p.referenceAudioPath = o[QStringLiteral("referenceAudioPath")].toString();
        p.audioSampleRate    = o[QStringLiteral("audioSampleRate")].toInt(48000);
        p.audioChannels      = o[QStringLiteral("audioChannels")].toInt(2);
        p.targetLufs         = o[QStringLiteral("targetLufs")].toDouble(-16.0);
        break;
    }

    case ai::GenerationKind::Subtitle: {
        p.subtitleMode = enumFromString<ai::SubtitleGenMode>(
            o[QStringLiteral("subtitleMode")].toString(), ai::SubtitleGenMode::SpeechToText);
        p.sourceAudioTrackId  = uuidFromJson(o[QStringLiteral("sourceAudioTrackId")]);
        p.language            = o[QStringLiteral("language")].toString();
        p.targetLanguage      = o[QStringLiteral("targetLanguage")].toString();
        p.wordLevelTimestamps = o[QStringLiteral("wordLevelTimestamps")].toBool(true);
        p.subtitleStylePresetId =
            o[QStringLiteral("subtitleStylePresetId")].toString(QStringLiteral("default"));
        p.maxCharsPerCue      = o[QStringLiteral("maxCharsPerCue")].toInt(0);
        p.minCueDurationFrames =
            int64_t(o[QStringLiteral("minCueDurationFrames")].toDouble(0));
        break;
    }

    case ai::GenerationKind::Mask: {
        p.maskSourceTrackId     = uuidFromJson(o[QStringLiteral("maskSourceTrackId")]);
        p.maskTargetDescription = o[QStringLiteral("maskTargetDescription")].toString();
        for (const QJsonValue& v : o[QStringLiteral("maskHintPoints")].toArray()) {
            const QJsonObject h = v.toObject();
            p.maskHintPoints.emplace_back(h[QStringLiteral("x")].toDouble(),
                                          h[QStringLiteral("y")].toDouble());
        }
        p.maskTrackAcrossFrames = o[QStringLiteral("maskTrackAcrossFrames")].toBool(true);
        p.maskFeather           = o[QStringLiteral("maskFeather")].toBool(true);
        break;
    }

    case ai::GenerationKind::EffectMetadata:
        break;
    }

    p.replaceExistingClips = o[QStringLiteral("replaceExistingClips")].toBool(false);
    p.createNewTrack       = o[QStringLiteral("createNewTrack")].toBool(false);
    p.extraParams          = o[kExtraParams].toObject();

    return p;
}


// ===========================================================================
//  ImageReference / VideoReference
// ===========================================================================
QJsonObject imageRefToJson(const ai::ImageReference& r)
{
    QJsonObject o;
    o[QStringLiteral("source")] =
        (r.source == ai::ImageReference::Source::FilePath) ? QStringLiteral("filePath")
                                                           : QStringLiteral("timelineFrame");
    if (r.source == ai::ImageReference::Source::FilePath) {
        o[QStringLiteral("filePath")] = r.filePath;         // プロジェクト相対
    } else {
        o[QStringLiteral("sourceTrackId")] = uuidToJson(r.sourceTrackId);
        o[QStringLiteral("sourceFrame")]   = double(r.sourceFrame);
    }
    o[QStringLiteral("strength")] = r.strength;
    return o;
}

ai::ImageReference ai::ImageReference::fromJson(const QJsonObject& o)
{
    ImageReference r;
    r.source = (o[QStringLiteral("source")].toString() == QLatin1String("timelineFrame"))
                 ? Source::TimelineFrame : Source::FilePath;
    r.filePath      = o[QStringLiteral("filePath")].toString();
    r.sourceTrackId = QUuid(o[QStringLiteral("sourceTrackId")].toString());
    r.sourceFrame   = int64_t(o[QStringLiteral("sourceFrame")].toDouble(0));
    r.strength      = o[QStringLiteral("strength")].toDouble(1.0);
    return r;
}

} // namespace yave::io
```

### 12.3.3 保存 (原子的書き込み)

```cpp
bool ProjectSerializer::save(const Project& project, const QString& path,
                             const SaveOptions& opts, QString* errorOut)
{
    // (1) アセット収集 (オプション)
    if (opts.collectGeneratedAssets || opts.collectSourceAssets) {
        if (!collectAssets(project, path, opts, errorOut))
            return false;
    }

    // (2) JSON 構築
    const QJsonObject root = serializeProject(project, opts);
    const QByteArray data = QJsonDocument(root).toJson(
        opts.indented ? QJsonDocument::Indented : QJsonDocument::Compact);

    // (3) 原子的書き込み。
    //     QSaveFile は「一時ファイルへ書く -> fsync -> rename」を行う。
    //     rename は同一ボリューム内では原子的なので、
    //     途中でクラッシュしても既存ファイルは無傷のまま残る。
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        if (errorOut) *errorOut = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorOut) *errorOut = file.errorString();
        return false;
    }

    // (4) 世代バックアップ
    rotateBackups(path, 5);
    return true;
}
```

---

## 12.4 SRT パース → 字幕区間生成 → Undo 可能な一括挿入

```cpp
// src/core/commands/ImportSubtitleCommand.h
#pragma once

#include <QUndoCommand>
#include <memory>
#include <vector>

namespace yave {

class Timeline;
class Track;
namespace subtitle { class SubtitleClip; }

/// SRT の重なりをどう解決するか
enum class OverlapPolicy
{
    SplitToNewTracks,   ///< 重なるキューを新しい字幕トラックへ振り分ける (既定)
    TrimPrevious,       ///< 前のキューの Out を次のキューの In に合わせる
    SkipOverlapping     ///< 重なるキューを取り込まない
};

/// SRT 取り込みを 1 個の Undo コマンドとして扱う。
/// 500 キューを 1 回の Undo で取り消せることが重要
/// (クリップごとにコマンドを積むと Undo を 500 回押す羽目になる)。
class ImportSubtitleCommand : public QUndoCommand
{
public:
    ImportSubtitleCommand(Timeline* timeline,
                          std::vector<std::shared_ptr<subtitle::SubtitleClip>> clips,
                          OverlapPolicy policy,
                          int targetTrackIndex,           // -1 = 新規トラックを作る
                          const QString& sourceFileName);

    void redo() override;
    void undo() override;

private:
    Track* ensureTrack(int index, int overflowLevel);

    Timeline*                                                   timeline_;
    std::vector<std::shared_ptr<subtitle::SubtitleClip>>        clips_;
    OverlapPolicy                                               policy_;
    int                                                         targetTrackIndex_;

    // Undo 用の記録
    std::vector<int>                                            createdTrackIndices_;
    std::vector<std::pair<QUuid /*trackId*/, QUuid /*clipId*/>> insertedClips_;
    bool                                                        firstRedoDone_ = false;
};

} // namespace yave
```

```cpp
// src/core/commands/ImportSubtitleCommand.cpp
#include "ImportSubtitleCommand.h"
#include "../Timeline.h"
#include "../Track.h"
#include "../../subtitle/SubtitleClip.h"

namespace yave {

ImportSubtitleCommand::ImportSubtitleCommand(
        Timeline* timeline,
        std::vector<std::shared_ptr<subtitle::SubtitleClip>> clips,
        OverlapPolicy policy,
        int targetTrackIndex,
        const QString& sourceFileName)
    : QUndoCommand()
    , timeline_(timeline)
    , clips_(std::move(clips))
    , policy_(policy)
    , targetTrackIndex_(targetTrackIndex)
{
    setText(QObject::tr("Import %n subtitle cue(s) from %1", "", int(clips_.size()))
                .arg(sourceFileName));
}

Track* ImportSubtitleCommand::ensureTrack(int baseIndex, int overflowLevel)
{
    // overflowLevel 0 = ベーストラック、1 以上 = 重なり用の追加トラック
    const int wanted = baseIndex + overflowLevel;
    while (timeline_->trackCount() <= wanted) {
        Track* t = timeline_->appendTrack(
            TrackType::Subtitle,
            QObject::tr("Subtitles %1").arg(timeline_->trackCount() + 1));
        createdTrackIndices_.push_back(timeline_->indexOfTrack(t));
    }
    return timeline_->trackAt(wanted);
}

void ImportSubtitleCommand::redo()
{
    createdTrackIndices_.clear();
    insertedClips_.clear();

    int baseIndex = targetTrackIndex_;
    if (baseIndex < 0) {
        Track* t = timeline_->appendTrack(TrackType::Subtitle,
                                          QObject::tr("Subtitles"));
        baseIndex = timeline_->indexOfTrack(t);
        createdTrackIndices_.push_back(baseIndex);
    }

    int64_t prevEnd = std::numeric_limits<int64_t>::min();

    for (const auto& clip : clips_) {
        // Undo 後の Redo でも同じ id を使うため、ここでは id を振り直さない

        const bool overlaps = (clip->range().start < prevEnd);

        Track* target = nullptr;
        switch (policy_) {

        case OverlapPolicy::SplitToNewTracks:
            // ★ 無限レイヤーの利点をそのまま使う。
            //    重なったキューは 1 段上のトラックへ置く。
            //    さらに重なれば 2 段上、と必要なだけトラックを増やす。
            for (int level = 0; ; ++level) {
                Track* t = ensureTrack(baseIndex, level);
                if (t->clipsIn(clip->range()).empty()) { target = t; break; }
            }
            break;

        case OverlapPolicy::TrimPrevious:
            target = timeline_->trackAt(baseIndex);
            if (overlaps && !target->clips().empty()) {
                auto& prev = target->clips().back();
                TimeRange pr = prev->range();
                pr.duration = clip->range().start - pr.start;
                if (pr.duration > 0) prev->setRange(pr);
            }
            break;

        case OverlapPolicy::SkipOverlapping:
            if (overlaps) continue;
            target = timeline_->trackAt(baseIndex);
            break;
        }

        if (!target) continue;
        if (!target->insertClip(clip)) continue;

        insertedClips_.emplace_back(target->id(), clip->id());
        prevEnd = std::max(prevEnd, clip->range().end());
    }

    firstRedoDone_ = true;
}

void ImportSubtitleCommand::undo()
{
    // (1) 挿入したクリップを除去する
    for (const auto& [trackId, clipId] : insertedClips_) {
        if (Track* t = timeline_->trackById(trackId))
            t->removeClip(clipId);
    }
    insertedClips_.clear();

    // (2) 作成したトラックを削除する。
    //     index が大きいものから消さないと、削除のたびに後続の index がずれる。
    std::sort(createdTrackIndices_.begin(), createdTrackIndices_.end(),
              std::greater<int>());
    for (int idx : createdTrackIndices_)
        (void)timeline_->takeTrack(idx);
    createdTrackIndices_.clear();
}

} // namespace yave
```

---

## 12.5 ISubtitleEffect の実装例 (Typewriter) と適用ループ

### 12.5.1 TypewriterEffect

```cpp
// src/subtitle/effects/TypewriterEffect.cpp
#include <yave/sdk/ISubtitleEffect.h>

#include <QObject>
#include <cmath>

namespace yave::subtitle {

using namespace yave::sdk;

/// 1 文字ずつ出現させるエフェクト。
/// グリフ単位の visible / opacity / transform のみを書き換えるため、
/// テキストの再ラスタライズは一切発生しない。
class TypewriterEffect final : public ISubtitleEffect
{
public:
    QString id()          const override { return QStringLiteral("yave.typewriter"); }
    QString displayName() const override { return QStringLiteral("effect.typewriter.name"); }
    QString category()    const override { return QStringLiteral("Transition"); }

    ParameterSchema parameterSchema() const override
    {
        ParameterSchema s;

        ParamDef cps;
        cps.key              = QStringLiteral("charsPerSecond");
        cps.displayNameKey   = QStringLiteral("effect.typewriter.charsPerSecond");
        cps.type             = ParamType::Double;
        cps.defaultValue     = 18.0;
        cps.minValue         = 1.0;
        cps.maxValue         = 120.0;
        cps.step             = 1.0;
        cps.unitSuffix       = QStringLiteral("cps");
        cps.tooltipKey       = QStringLiteral("effect.typewriter.charsPerSecond.tooltip");
        s.push_back(cps);

        ParamDef delay;
        delay.key            = QStringLiteral("startDelay");
        delay.displayNameKey = QStringLiteral("effect.typewriter.startDelay");
        delay.type           = ParamType::Double;
        delay.defaultValue   = 0.0;
        delay.minValue       = 0.0;
        delay.maxValue       = 10.0;
        delay.step           = 0.05;
        delay.unitSuffix     = QStringLiteral("s");
        s.push_back(delay);

        ParamDef fade;
        fade.key             = QStringLiteral("perCharFade");
        fade.displayNameKey  = QStringLiteral("effect.typewriter.perCharFade");
        fade.type            = ParamType::Double;
        fade.defaultValue    = 0.05;
        fade.minValue        = 0.0;
        fade.maxValue        = 1.0;
        fade.step            = 0.01;
        fade.unitSuffix      = QStringLiteral("s");
        s.push_back(fade);

        ParamDef mode;
        mode.key             = QStringLiteral("unit");
        mode.displayNameKey  = QStringLiteral("effect.typewriter.unit");
        mode.type            = ParamType::Enum;
        mode.defaultValue    = 0;
        mode.enumKeys        = { QStringLiteral("effect.typewriter.unit.char"),
                                 QStringLiteral("effect.typewriter.unit.word"),
                                 QStringLiteral("effect.typewriter.unit.line") };
        s.push_back(mode);

        ParamDef useStt;
        useStt.key            = QStringLiteral("followWordTimings");
        useStt.displayNameKey = QStringLiteral("effect.typewriter.followWordTimings");
        useStt.type           = ParamType::Bool;
        useStt.defaultValue   = false;
        useStt.tooltipKey     = QStringLiteral("effect.typewriter.followWordTimings.tooltip");
        s.push_back(useStt);

        return s;
    }

    void prepare(const SubtitleGlyphRun& run,
                 const ParameterValues& params,
                 const QSize& canvasSize) override
    {
        Q_UNUSED(canvasSize);

        // apply() で確保しないよう、ここで作り切る。
        // 各グリフが「何番目に出現するか」を単位 (文字/単語/行) に応じて決める。
        const int unit = params.getInt(QStringLiteral("unit"), 0);

        order_.assign(run.glyphs.size(), 0);
        int maxOrder = 0;

        for (size_t i = 0; i < run.glyphs.size(); ++i) {
            const GlyphInfo& g = run.glyphs[i];
            int idx = 0;
            switch (unit) {
            case 0: idx = g.charIndex; break;      // 文字単位
            case 1: idx = g.wordIndex; break;      // 単語単位
            case 2: idx = g.lineIndex; break;      // 行単位
            default: idx = g.charIndex; break;
            }
            order_[i] = idx;
            maxOrder  = std::max(maxOrder, idx);
        }
        maxOrder_ = maxOrder;
    }

    void apply(SubtitleEffectFrame& frame,
               const SubtitleTimeInfo& time,
               const ParameterValues& params) override
    {
        if (!frame.run || !frame.glyphs) return;

        const double cps       = std::max(0.001, params.getDouble(
                                     QStringLiteral("charsPerSecond"), 18.0));
        const double delay     = params.getDouble(QStringLiteral("startDelay"), 0.0);
        const double perFade   = std::max(0.0, params.getDouble(
                                     QStringLiteral("perCharFade"), 0.05));
        const bool   followStt = params.getBool(
                                     QStringLiteral("followWordTimings"), false);

        const double t = time.secondsFromIn - delay;

        auto& glyphs = *frame.glyphs;
        const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

        for (size_t i = 0; i < n; ++i) {
            const GlyphInfo& g = frame.run->glyphs[i];

            // このグリフが出現し始める時刻
            double appearAt = 0.0;

            if (followStt && frame.wordTimings && !frame.wordTimings->empty()) {
                // ★ STT 由来の単語タイミングに追従する。
                //    書き起こし字幕で、実際の発話に同期したタイプライターになる。
                appearAt = wordStartFor(g.charIndex, *frame.wordTimings);
            } else {
                appearAt = double(order_[i]) / cps;
            }

            const double dt = t - appearAt;

            if (dt < 0.0) {
                // まだ出現していない
                glyphs[i].visible = false;
                glyphs[i].opacity = 0.0f;
                continue;
            }

            glyphs[i].visible = true;
            glyphs[i].opacity = (perFade <= 0.0)
                                  ? 1.0f
                                  : float(std::min(1.0, dt / perFade));
        }
    }

private:
    static double wordStartFor(int charIndex,
                               const std::vector<SubtitleEffectFrame::WordTiming>& wt)
    {
        for (const auto& w : wt) {
            if (charIndex >= w.charStart && charIndex < w.charStart + w.charLength)
                return w.startSec;
        }
        return 0.0;
    }

    // prepare() で確保する。apply() ではメモリ確保しない。
    std::vector<int> order_;
    int              maxOrder_ = 0;
};

// 組み込みエフェクトの登録
ISubtitleEffect* createTypewriterEffect() { return new TypewriterEffect(); }

} // namespace yave::subtitle
```

### 12.5.2 エフェクトスタックを毎フレーム適用するループ

```cpp
// src/subtitle/SubtitleRenderer.cpp
SubtitleDrawData SubtitleRenderer::buildFrame(const SubtitleClip& clip,
                                              int64_t currentFrame,
                                              const Rational& timebase,
                                              const QSize& canvasSize,
                                              const SubtitleStylePresetTable& presets)
{
    SubtitleDrawData out;

    const TimeRange r = clip.range();
    if (!r.contains(currentFrame) || r.duration <= 0)
        return out;                                   // 表示区間外

    const SubtitleStyle style = clip.resolvedStyle(presets);

    // ---- (1) レイアウト (キャッシュされる。テキスト/スタイル変更時のみ再計算) ----
    const SubtitleGlyphRun& run =
        layoutCache_.get(clip.id(), clip.contentRevision(), clip.text(), style, canvasSize);

    // ---- (2) グリフアトラス (同上。出力解像度でラスタライズ) ----
    const GlyphAtlas::Entry* atlas =
        atlas_.acquire(clip.text(), style, run, canvasSize);
    if (!atlas || !atlas->texture)
        return out;

    // ---- (3) グリフ変換配列を初期状態にリセット ----
    //         scratch_ は使い回すので、ここでの確保は基本的に起きない。
    scratch_.glyphs.resize(run.glyphs.size());
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        auto& gt = scratch_.glyphs[i];
        gt.transform.setToIdentity();
        gt.color      = run.glyphs[i].baseColor;
        gt.opacity    = 1.0f;
        gt.visible    = true;
        gt.blurRadius = 0.0f;
    }
    QMatrix4x4 blockTransform;                        // 単位行列
    float      blockOpacity = float(style.opacity);

    // ---- (4) 時間情報を作る ----
    yave::sdk::SubtitleTimeInfo time;
    time.progress        = double(currentFrame - r.start) / double(r.duration);
    time.secondsFromIn   = framesToSeconds(currentFrame - r.start, timebase);
    time.secondsToOut    = framesToSeconds(r.end() - currentFrame, timebase);
    time.clipDurationSec = framesToSeconds(r.duration, timebase);

    yave::sdk::SubtitleEffectFrame ef;
    ef.run            = &run;
    ef.canvasSize     = canvasSize;
    ef.clipStartFrame = r.start;
    ef.clipDuration   = r.duration;
    ef.currentFrame   = currentFrame;
    ef.fps            = 1.0 / timebase.toDouble();
    ef.wordTimings    = clip.hasWordTimings()
                          ? reinterpret_cast<const std::vector<
                                yave::sdk::SubtitleEffectFrame::WordTiming>*>(
                                &clip.wordTimings())
                          : nullptr;
    ef.glyphs         = &scratch_.glyphs;
    ef.blockTransform = &blockTransform;
    ef.blockOpacity   = &blockOpacity;

    // ---- (5) ★ エフェクトスタックを下から順に適用 ----
    for (const SubtitleEffectInstance& inst : clip.effectStack()) {
        if (!inst.enabled)  continue;
        if (inst.missing)   continue;      // 未インストール。設定は保持されている
        if (!inst.effect)   continue;

        // ブロックレベルエフェクト (AviUtl アダプタ) はグリフ系と併用できない。
        // UI で防いでいるが、プロジェクトを手編集された場合に備えて防御する。
        if (inst.effect->isBlockLevel()) {
            out.blockLevelEffects.push_back(inst.effect);
            continue;
        }

        // レイアウトが変わっていたら prepare() をやり直す。
        // (テキスト編集でグリフ数が変わると、prepare() の前計算結果が
        //  古い要素数のままになり、apply() で範囲外アクセスを起こす)
        auto& mutableInst = const_cast<SubtitleEffectInstance&>(inst);
        if (!mutableInst.prepared ||
            mutableInst.preparedForRevision != clip.contentRevision()) {
            mutableInst.effect->prepare(run, mutableInst.params, canvasSize);
            mutableInst.prepared            = true;
            mutableInst.preparedForRevision = clip.contentRevision();
        }
        inst.effect->apply(ef, time, inst.params);
    }

    // ---- (6) 描画用インスタンスデータを組み立てる ----
    out.atlasTexture   = atlas->texture;
    out.blockTransform = anchorTransform(style, run, canvasSize) * blockTransform;
    out.blockOpacity   = blockOpacity;
    out.instances.clear();
    out.instances.reserve(run.glyphs.size());

    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        const GlyphInfo& g  = run.glyphs[i];
        const auto&      gt = scratch_.glyphs[i];

        if (g.isWhitespace || !gt.visible || gt.opacity <= 0.001f)
            continue;

        GlyphInstance inst;
        inst.rect      = g.layoutRect;
        inst.uv        = g.atlasUv;
        inst.color     = gt.color;
        inst.opacity   = gt.opacity;
        inst.transform = gt.transform;
        out.instances.push_back(inst);
    }

    return out;
}
```

---

## 12.6 LanguageManager::setLanguage() と CMake の翻訳設定

### 12.6.1 CMake

```cmake
# src/app/CMakeLists.txt

qt_add_executable(yave_app
    main.cpp
    controllers/ProjectController.cpp
    controllers/EditController.cpp
    # ...
)

qt_add_qml_module(yave_app
    URI Yave
    VERSION 1.0
    QML_FILES
        qml/MainWindow.qml
        qml/timeline/TimelineView.qml
        qml/timeline/SubtitleClipItem.qml
        qml/inspector/SubtitleInspector.qml
        qml/inspector/AutoParameterForm.qml
        qml/ai/AiGenerateDialog.qml
    SOURCES
        ../i18n/LanguageManager.cpp      # QML_ELEMENT + QML_SINGLETON
        items/PreviewItem.cpp
        models/TimelineModel.cpp
)

# ---------------------------------------------------------------------------
# 翻訳: 日本語 / 英語
#
#   RESOURCE_PREFIX "/i18n" により、.qm が :/i18n/yave_ja.qm として
#   実行ファイルに埋め込まれる。外部ファイルの配布が不要になる。
#
#   LUPDATE_OPTIONS:
#     -no-obsolete   : 使われなくなった文字列を .ts から削除する
#     -locations none: 行番号情報を出力しない。
#                      これが無いと、コードを 1 行足すだけで .ts に
#                      巨大な diff が出て翻訳レビューが不可能になる。
# ---------------------------------------------------------------------------
qt_add_translations(yave_app
    TS_FILES
        ${CMAKE_SOURCE_DIR}/i18n/yave_ja.ts
        ${CMAKE_SOURCE_DIR}/i18n/yave_en.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none
)

target_link_libraries(yave_app PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Qml
    yave_core yave_media yave_render yave_audio yave_subtitle yave_ai yave_plugin
)
```

### 12.6.2 setLanguage() の実装

```cpp
// src/i18n/LanguageManager.cpp
#include "LanguageManager.h"

#include <QCoreApplication>
#include <QApplication>
#include <QQmlEngine>
#include <QLibraryInfo>
#include <QSettings>
#include <QLocale>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcI18n, "yave.i18n")

namespace yave {

LanguageManager& LanguageManager::instance()
{
    static LanguageManager s;
    return s;
}

QString LanguageManager::displayName(const QString& code) const
{
    if (code == QLatin1String("ja")) return QString::fromUtf8("日本語");
    if (code == QLatin1String("en")) return QStringLiteral("English");
    return code;
}

void LanguageManager::setLanguage(const QString& code)
{
    if (code == current_)                       return;
    if (!availableLanguages().contains(code))   return;

    const QString previous = current_;

    // ---- (1) 既存のトランスレータをすべて外す ----
    //     removeTranslator は installTranslator と逆順に呼ぶ必要はないが、
    //     すべて外してから入れ直すのが最も確実。
    removeAllTranslators();

    // ---- (2) 新しい言語をロード ----
    if (!loadTranslatorsFor(code)) {
        qCWarning(lcI18n) << "Failed to switch to" << code << "; reverting to" << previous;
        loadTranslatorsFor(previous);
        return;
    }
    current_ = code;

    // ---- (3) 設定を保存 ----
    QSettings().setValue(QStringLiteral("ui/language"), code);

    // ---- (4) UI フォントを言語に合わせる ----
    //     日本語 UI でシステムフォントに日本語グリフが無いと豆腐になる。
    //     なお、これは UI フォントのみ。字幕レンダリング用フォントは
    //     SubtitleStyle が持つため影響を受けない。
    applyUiFont(code);

    // ---- (5) ★ QML の qsTr() バインディングを再評価させる ----
    //     これ 1 回で、qsTr() を使っているすべてのバインディングが
    //     自動的に再計算される。QML 側に追加のコードは要らない。
    if (qmlEngine_)
        qmlEngine_->retranslate();

    // ---- (6) ★ Widgets 側へ LanguageChange イベントを送る ----
    //     QWidget::changeEvent(QEvent::LanguageChange) を実装している
    //     ウィジェット (PluginWindow 等) が retranslateUi() を呼ぶ。
    for (QWidget* w : QApplication::topLevelWidgets())
        QCoreApplication::sendEvent(w, new QEvent(QEvent::LanguageChange));

    // ---- (7) 自前で文字列を保持しているもの (動的生成した QAction、
    //          モデルのヘッダ、エフェクト一覧など) へ通知する ----
    emit languageChanged();

    qCInfo(lcI18n) << "UI language switched:" << previous << "->" << code;
}

bool LanguageManager::loadTranslatorsFor(const QString& code)
{
    // ---- アプリ本体 ----
    //   英語はソース文字列がそのまま使われるため、.qm が無くても正常。
    auto appTr = std::make_unique<QTranslator>();
    const QString appQm = QStringLiteral(":/i18n/yave_%1.qm").arg(code);
    if (appTr->load(appQm)) {
        QCoreApplication::installTranslator(appTr.get());
        appTranslator_ = std::move(appTr);
    } else if (code != QLatin1String("en")) {
        qCWarning(lcI18n) << "Translation not found:" << appQm;
        return false;
    }

    // ---- Qt 標準ダイアログ / ウィジェット ----
    //   これを忘れると、ファイルダイアログの「開く」「キャンセル」だけが
    //   前の言語のまま残る。
    auto qtTr = std::make_unique<QTranslator>();
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTr->load(QStringLiteral("qtbase_%1").arg(code), qtDir)) {
        QCoreApplication::installTranslator(qtTr.get());
        qtBaseTranslator_ = std::move(qtTr);
    }

    // ---- プラグイン同梱翻訳 ----
    //   後からインストールされるほど優先されるため、
    //   プラグイン翻訳はアプリ本体より後に入れる。
    for (const PluginTrSource& src : pluginSources_) {
        auto tr = std::make_unique<QTranslator>();
        if (tr->load(QStringLiteral("%1_%2").arg(src.prefix, code), src.dir)) {
            QCoreApplication::installTranslator(tr.get());
            pluginTranslators_.push_back(std::move(tr));
        }
    }
    return true;
}

void LanguageManager::removeAllTranslators()
{
    for (auto& t : pluginTranslators_)
        QCoreApplication::removeTranslator(t.get());
    pluginTranslators_.clear();

    if (qtBaseTranslator_) {
        QCoreApplication::removeTranslator(qtBaseTranslator_.get());
        qtBaseTranslator_.reset();
    }
    if (appTranslator_) {
        QCoreApplication::removeTranslator(appTranslator_.get());
        appTranslator_.reset();
    }
}

void LanguageManager::registerPluginTranslations(const QString& dir, const QString& prefix)
{
    // 既に登録済みなら何もしない
    for (const auto& s : pluginSources_)
        if (s.dir == dir && s.prefix == prefix) return;

    pluginSources_.push_back({dir, prefix});

    // 現在の言語で即座にロードする
    auto tr = std::make_unique<QTranslator>();
    if (tr->load(QStringLiteral("%1_%2").arg(prefix, current_), dir)) {
        QCoreApplication::installTranslator(tr.get());
        pluginTranslators_.push_back(std::move(tr));
        emit languageChanged();      // エフェクト一覧の表示名を更新させる
    }
}

QString LanguageManager::translateKey(const QString& key, const QString& context)
{
    // 翻訳が見つからなければ QCoreApplication::translate() はキー自身を返す。
    // したがって、翻訳を提供しないプラグインが displayName() で
    // "Glitch" のような生の文字列を返しても、そのまま表示される。
    // フォールバックのための特別な分岐が要らない。
    return QCoreApplication::translate(context.toUtf8().constData(),
                                       key.toUtf8().constData());
}

} // namespace yave
```

### 12.6.3 main.cpp での初期化順序

```cpp
// src/app/main.cpp
int main(int argc, char** argv)
{
    // QWidget 系 (PluginWindow) を使うので QApplication を使う。
    // QGuiApplication では QWidget が動かない。
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("YAVE"));
    QCoreApplication::setApplicationName(QStringLiteral("YetAnotherVideoEditor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // ★ QML エンジンを作る前に翻訳をロードする。
    //    後からだと、初回の qsTr() 評価が未翻訳のまま固定される要素が出る。
    auto& lm = yave::LanguageManager::instance();
    lm.initialize();

    yave::render::RhiBackendSelector::select();
    yave::plugin::PluginManager::instance().scanAsync();
    yave::audio::AudioRenderEngine::instance().openDevice();

    QQmlApplicationEngine engine;

    // ★ retranslate() を呼ぶためにエンジンを登録する
    lm.setQmlEngine(&engine);

    engine.loadFromModule("Yave", "MainWindow");
    if (engine.rootObjects().isEmpty())
        return -1;

    const int rc = app.exec();

    // シャットダウン順序は 1.6 を厳守する
    yave::audio::AudioRenderEngine::instance().stop();
    yave::plugin::PluginManager::instance().unloadAll();
    return rc;
}
```

---

## 12.7 FFmpeg HW デコーダの初期化

```cpp
// src/media/VideoDecoder.cpp
bool VideoDecoder::open(const OpenParams& params, QString* errorOut)
{
    // ---- (1) コンテナを開く ----
    AVFormatContext* fmt = nullptr;
    int ret = avformat_open_input(&fmt, params.filePath.toUtf8().constData(),
                                  nullptr, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = avErrorString(ret);
        return false;
    }
    fmt_.reset(fmt);

    if ((ret = avformat_find_stream_info(fmt_.get(), nullptr)) < 0) {
        if (errorOut) *errorOut = avErrorString(ret);
        return false;
    }

    // ---- (2) ビデオストリームとデコーダを選ぶ ----
    const AVCodec* decoder = nullptr;
    streamIndex_ = av_find_best_stream(fmt_.get(), AVMEDIA_TYPE_VIDEO,
                                       params.streamIndex, -1, &decoder, 0);
    if (streamIndex_ < 0 || !decoder) {
        if (errorOut) *errorOut = QObject::tr("No video stream found.");
        return false;
    }

    AVStream* stream = fmt_->streams[streamIndex_];
    srcTimebase_     = Rational(stream->time_base.num, stream->time_base.den);
    projectTimebase_ = params.projectTimebase;

    // ---- (3) コーデックコンテキストを作る ----
    codec_.reset(avcodec_alloc_context3(decoder));
    if (!codec_) {
        if (errorOut) *errorOut = QObject::tr("Failed to allocate codec context.");
        return false;
    }
    if ((ret = avcodec_parameters_to_context(codec_.get(), stream->codecpar)) < 0) {
        if (errorOut) *errorOut = avErrorString(ret);
        return false;
    }

    // ---- (4) ★ HW デバイスを設定する ----
    if (params.allowHwAccel) {
        hwDevice_ = HwDeviceContext::createBest(decoder->id);

        if (hwDevice_ && hwDevice_->accel() != HwAccel::None) {
            // get_format コールバックで HW ピクセルフォーマットを選ばせる。
            // これを設定しないと、FFmpeg は SW フォーマットに落としてしまう。
            codec_->opaque     = this;
            codec_->get_format = &VideoDecoder::getFormatCallback;

            // デバイスコンテキストへの参照を渡す。
            // av_buffer_ref で参照カウントを増やしておく
            // (codec_ の破棄時に FFmpeg 側が unref するため)。
            codec_->hw_device_ctx = av_buffer_ref(hwDevice_->ref());

            qCInfo(lcMedia) << "HW acceleration:" << int(hwDevice_->accel())
                            << "for" << params.filePath;
        } else {
            fallbackUsed_ = true;
            qCInfo(lcMedia) << "HW acceleration unavailable; using software decode";
        }
    } else {
        fallbackUsed_ = true;
    }

    // ---- (5) スレッド設定 ----
    codec_->thread_count = params.threadCount > 0
                             ? params.threadCount
                             : std::max(1, QThread::idealThreadCount() / 2);
    codec_->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;

    // ---- (6) オープン ----
    if ((ret = avcodec_open2(codec_.get(), decoder, nullptr)) < 0) {
        if (errorOut) *errorOut = avErrorString(ret);
        return false;
    }

    // ---- (7) 尺の算出 ----
    if (stream->nb_frames > 0) {
        durationFrames_ = rescaleFrames(stream->nb_frames,
                                        Rational(stream->avg_frame_rate.den,
                                                 stream->avg_frame_rate.num),
                                        projectTimebase_, RoundMode::Nearest);
    } else if (stream->duration != AV_NOPTS_VALUE) {
        const double sec = double(stream->duration) * srcTimebase_.toDouble();
        durationFrames_  = secondsToFrames(sec, projectTimebase_, RoundMode::Floor);
    }

    return true;
}
```

---

## 12.8 オーディオコールバック内のクロック更新

```cpp
// src/audio/AudioRenderEngine.cpp

void AudioRenderEngine::rtCallback(float* const* out, int numChannels,
                                   int numFrames, void* userData) noexcept
{
    // ===================================================================
    //  このスコープ内で禁止されていること:
    //    - malloc / free / new / delete
    //    - mutex の取得
    //    - Qt シグナルの発火
    //    - ファイル I/O
    //    - 例外の送出
    //  違反すると、優先度の低いスレッドがロックを持ったまま
    //  スケジュールアウトされた瞬間に音が途切れる (priority inversion)。
    // ===================================================================

    auto* self = static_cast<AudioRenderEngine*>(userData);

    // グラフは RCU で差し替えられる。acquire で読めば、
    // UI スレッドが publishGraph() で書いた内容が確実に見える。
    AudioRenderGraph* g = self->activeGraph_.load(std::memory_order_acquire);

    // 出力をゼロクリア
    for (int c = 0; c < numChannels; ++c)
        std::memset(out[c], 0, sizeof(float) * size_t(numFrames));

    if (!g || !self->playing_.load(std::memory_order_relaxed)) {
        // 停止中でも世代は進める。UI 側の遅延解放判定に使われる。
        self->rtGeneration_.fetch_add(1, std::memory_order_release);
        return;
    }

    // ★ このブロックが担当するタイムライン上のサンプル位置
    const int64_t blockStart = self->clock_.rawPlayedPosition();

    // ---- ループ再生の折り返し判定 ----
    int64_t framesToRender = numFrames;
    if (g->loopEnabled && blockStart + numFrames > g->loopEndSample) {
        framesToRender = g->loopEndSample - blockStart;
        // 残りはループ先頭から埋める (2 回に分けてレンダリングする)
    }

    // ---- トラックごとの処理 ----
    for (TrackNode& t : g->tracks) {
        if (t.muted || (g->anySolo && !t.solo))
            continue;

        float* const* buf = self->scratch_.trackBuffer(t.scratchIndex);
        for (int c = 0; c < numChannels; ++c)
            std::memset(buf[c], 0, sizeof(float) * size_t(numFrames));

        // (a) クリップの PCM をミックス
        mixClipsInto(t, blockStart, int(framesToRender), buf, numChannels);

        // (b) VST3 エフェクトチェーン (in-place)
        for (Vst3ProcessorNode* fx : t.effectChain)
            fx->processRt(buf, numChannels, numFrames);

        // (c) PDC の遅延を適用
        if (t.compensationDelay > 0 && t.delayLine)
            t.delayLine->process(buf, numChannels, numFrames);

        // (d) ゲイン / パンを掛けてマスターへ加算
        accumulateWithGainPan(buf, out, numChannels, numFrames, t.gain, t.pan);
    }

    // ---- マスターチェーン ----
    for (Vst3ProcessorNode* fx : g->masterChain)
        fx->processRt(out, numChannels, numFrames);

    applyGain(out, numChannels, numFrames, g->masterGain);

    // ---- メーター値を UI へ (ロックフリーリングバッファ) ----
    self->meterBridge_.pushPeaks(out, numChannels, numFrames);

    // ===================================================================
    //  ★ クロック更新
    //     これが映像同期のマスタークロックになる。
    //     release 順序で書くことで、他スレッドが acquire で読んだときに
    //     ここまでの処理結果が見えることを保証する。
    // ===================================================================
    if (g->loopEnabled && blockStart + numFrames >= g->loopEndSample) {
        // ループ先頭へ巻き戻す
        self->clock_.reset(g->loopStartSample + (numFrames - framesToRender));
    } else {
        self->clock_.advance(numFrames);
    }

    self->rtGeneration_.fetch_add(1, std::memory_order_release);
}


int64_t AudioRenderEngine::currentFrame() const
{
    // ★ 映像側はこれを毎 vsync で呼び、返ってきたフレームを表示する。
    //
    //    audibleSamplePosition() は
    //      「デバイスへ書き込んだサンプル数」-「出力レイテンシ」
    //    を返す。これを引かないと、映像がバッファ長 (数十 ms) 分だけ
    //    音より先行して見える。
    //
    //    さらに PDC が有効な場合、プラグインの合計レイテンシ分も
    //    引く必要がある (setOutputLatencySamples に加算済み)。

    const int64_t sample = clock_.audibleSamplePosition();
    const double  sec    = double(sample) / double(clock_.sampleRate());
    return secondsToFrames(sec, timebase_, RoundMode::Nearest);
}


void AudioRenderEngine::publishGraph(std::unique_ptr<AudioRenderGraph> g)
{
    // UI スレッドから呼ぶ。RT スレッドを止めない。

    // (1) 新グラフに必要な scratch を先に確保しておく。
    //     RT スレッドが新グラフを見た瞬間に scratch が足りないと落ちる。
    ensureScratchCapacity(*g);

    // (2) PDC を計算して反映
    const int64_t pluginLatency = pdcEnabled_
                                    ? DelayCompensator::compute(*g)
                                    : 0;
    pluginLatencySamples_ = pluginLatency;
    clock_.setOutputLatencySamples(device_->outputLatencySamples() + pluginLatency);

    // (3) アトミックに差し替える
    AudioRenderGraph* raw = g.get();
    liveGraphs_.push_back(std::move(g));
    AudioRenderGraph* old = activeGraph_.exchange(raw, std::memory_order_acq_rel);

    // (4) 古いグラフを「引退リスト」へ入れる。すぐには破棄しない。
    //     exchange した瞬間に RT スレッドが古いグラフを処理中の可能性がある。
    if (old) {
        auto it = std::find_if(liveGraphs_.begin(), liveGraphs_.end(),
                               [old](const auto& p) { return p.get() == old; });
        if (it != liveGraphs_.end()) {
            retired_.push_back({std::move(*it),
                                rtGeneration_.load(std::memory_order_acquire)});
            liveGraphs_.erase(it);
        }
    }

    // (5) RT が 2 世代以上進んだグラフだけを実際に破棄する
    collectRetiredGraphs();

    emit latencyChanged(totalLatencySamples());
}

void AudioRenderEngine::collectRetiredGraphs()
{
    const uint64_t now = rtGeneration_.load(std::memory_order_acquire);
    retired_.erase(
        std::remove_if(retired_.begin(), retired_.end(),
                       [now](const RetiredGraph& r) {
                           return now >= r.retiredAtGeneration + 2;
                       }),
        retired_.end());
}
```

---

## 12.9 RHI 合成ループ (1 フレーム分)

```cpp
// src/render/RhiCompositor.cpp
void RhiCompositor::renderFrame(const RenderSnapshot& snap, QRhiCommandBuffer* cb)
{
    if (snap.layers.empty()) {
        clearTo(cb, currentTarget(), Qt::transparent);
        return;
    }

    // ---- (1) PREPARE : 各レイヤーのソースを解決する ----
    //     ここで同期待ちを発生させてはならない。
    //     デコードが間に合っていなければ「直近の使えるフレーム」で代用する。
    preparedLayers_.clear();
    preparedLayers_.reserve(snap.layers.size());

    for (const LayerItem& layer : snap.layers) {
        PreparedLayer pl;
        pl.item = &layer;

        std::visit([&](auto&& src) {
            using T = std::decay_t<decltype(src)>;

            if constexpr (std::is_same_v<T, VideoSourceRef>) {
                auto frame = frameCache_->get({src.assetId, src.sourceFrame});
                if (!frame) {
                    // 先読みが間に合わなかった。要求は出しつつ、
                    // 直近の使えるフレームで代用する (フレームホールド)。
                    decodePool_->submit({src.assetId, src.sourceFrame, 0, generation_});
                    frame = frameCache_->nearest({src.assetId, src.sourceFrame});
                    ++droppedFrames_;
                }
                if (frame)
                    pl.textures = uploadOrWrap(*frame);
                pl.keepAlive = frame;     // GPU が使い終わるまで AVFrame を生かす
            }
            else if constexpr (std::is_same_v<T, SubtitleRenderRef>) {
                pl.subtitle = subtitleRenderer_->buildFrame(
                    *src.clip, snap.frameIndex, snap.timebase,
                    snap.canvasSize, *stylePresets_);
            }
            else if constexpr (std::is_same_v<T, GeneratedSourceRef>) {
                // 生成物も通常の動画と同じ扱い
                auto frame = frameCache_->get({src.assetId, src.sourceFrame});
                if (frame) pl.textures = uploadOrWrap(*frame);
                pl.keepAlive = frame;
            }
        }, layer.source);

        preparedLayers_.push_back(std::move(pl));
    }

    // ---- (2) リソース更新をまとめて発行 ----
    QRhiResourceUpdateBatch* rub = rhi_->nextResourceUpdateBatch();
    for (PreparedLayer& pl : preparedLayers_)
        queueUploads(rub, pl);

    // ---- (3) COMPOSITE : オフスクリーン RT へ背面から順に描く ----
    //     ping-pong RT を使う理由:
    //       ブレンドモードによっては「ここまでの合成結果」を
    //       シェーダでサンプルする必要があり、描画先を同時に読めないため。
    int pingPong = 0;
    QRhiTextureRenderTarget* dst = compositeRt_[pingPong];

    cb->beginPass(dst, Qt::transparent, {1.0f, 0}, rub);
    cb->endPass();

    for (const PreparedLayer& pl : preparedLayers_) {
        const bool needsDstRead = requiresDestinationRead(pl.item->blendMode);

        if (needsDstRead) {
            // ping-pong: 読み元 = 現在の dst、書き先 = もう一方
            const int src = pingPong;
            pingPong = 1 - pingPong;
            drawLayerWithDstRead(cb, pl, compositeTex_[src], compositeRt_[pingPong]);
        } else {
            // Normal ブレンドは固定機能ブレンドで済む。
            // ping-pong 不要なので、実運用の大半のケースで高速。
            drawLayerFixedBlend(cb, pl, compositeRt_[pingPong]);
        }
    }

    // ---- (4) PRESENT : プレビューウィンドウへスケール描画 ----
    QRhiSwapChain* sc = swapChain_;
    cb->beginPass(sc->currentFrameRenderTarget(), Qt::black, {1.0f, 0});
    drawFullscreenScaled(cb, compositeTex_[pingPong], sc->currentPixelSize());
    cb->endPass();

    // ---- (5) In-flight フレームの保持 ----
    //     GPU がまだ使っているテクスチャ / AVFrame を解放しないよう、
    //     FramesInFlight (通常 2) 分だけ遅らせて破棄する。
    const int slot = frameCounter_ % rhi_->resourceLimit(QRhi::FramesInFlight);
    inFlight_[slot] = std::move(preparedLayers_);
    ++frameCounter_;

    // ---- (6) 統計 ----
    perfMonitor_.recordFrame(cb->lastCompletedGpuTime(), droppedFrames_);
    texturePool_->trim();
}
```
