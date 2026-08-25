#include "PluginWindow.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace yave::plugin {

PluginWindow::~PluginWindow() = default;

PluginWindow::PluginWindow(const PluginDescriptor& desc, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , desc_(desc)
{
    setWindowTitle(desc.name);
    setAttribute(Qt::WA_DeleteOnClose, false);   ///< ライフタイムは呼び出し側が管理

    // AviUtl は DPI 非対応のものが大半。ネイティブウィンドウを作る前に判定。
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

    connect(bypassButton_, &QPushButton::toggled, this,
            [this](bool on) { Q_UNUSED(on); emit parametersChangedByPlugin(); });
    connect(resetButton_, &QPushButton::clicked, this,
            [this]() { emit parametersChangedByPlugin(); });

    retranslateUi();
}

void PluginWindow::createNativeContainer()
{
    container_ = new QWidget(this);

    // WA_NativeWindow: この QWidget に実際の HWND / NSView を作らせる。
    //   これを立てないと Qt は「アルファウィジェット」として扱い、
    //   winId() を呼んだ時点で初めて生成される (タイミングが不定になる)。
    // WA_DontCreateNativeAncestors: 親ウィジェット群までネイティブ化されるのを防ぐ。
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
        // ウィンドウ生成瞬間の awareness context が採用されるため、
        // winId() の前後で切り替える必要がある。
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
    // macOS で NSWindow* を返すと VST3 側は kPlatformTypeNSView を期待しているため
    // GUI が表示されない。Qt の winId() は NSView* を返すのでそのまま渡してよい。
    return reinterpret_cast<void*>(container_->winId());
}

void PluginWindow::resizeClientArea(int width, int height)
{
    // container_ を目標サイズにしたときのウィンドウ全体サイズを求める
    const int extraH = height + (this->height() - container_->height());
    const int extraW = width + (this->width() - container_->width());
    resize(extraW, extraH);
}

void PluginWindow::onPluginRequestedResize(int width, int height)
{
    container_->setMinimumSize(width, height);
    container_->setMaximumSize(resizable_
                                   ? QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
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
    // 順序を誤ると、破棄済み NSView / HWND へプラグインが描画してクラッシュする。
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
