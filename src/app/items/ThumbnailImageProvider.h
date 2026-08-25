#pragma once

#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QThreadPool>

namespace yave::app {

/// image://yave-thumb/<assetId> でアセットのサムネイルを供給する (1.7.5)。
///
/// 生成は専用のワーカースレッドで行い、UI スレッドではデコードしない。
/// 生成済みのものは実行間で使い回せるようディスクにも残す。
class ThumbnailImageProvider : public QQuickAsyncImageProvider
{
public:
    ThumbnailImageProvider();
    ~ThumbnailImageProvider() override;

    QQuickImageResponse* requestImageResponse(const QString& id,
                                              const QSize& requestedSize) override;

    /// ディスクキャッシュの置き場 (無ければ作る)。
    static QString cacheDir();

private:
    QThreadPool pool_;
};

} // namespace yave::app
