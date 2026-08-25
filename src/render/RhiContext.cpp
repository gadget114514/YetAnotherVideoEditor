#include "RhiContext.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#if __has_include(<rhi/qrhi.h>)
#  define YAVE_HAVE_QRHI 1
#  include <rhi/qrhi.h>
#endif

namespace yave::render {

void RhiContext::selectBackend()
{
    // ユーザーが明示指定している場合は尊重する
    if (!qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        return;

#ifdef Q_OS_WIN
    qputenv("QSG_RHI_BACKEND", "d3d11");
#elif defined(Q_OS_MACOS)
    qputenv("QSG_RHI_BACKEND", "metal");
#else
    qputenv("QSG_RHI_BACKEND", "opengl");
#endif
}

QString RhiContext::defaultBackendName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("d3d11");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("metal");
#else
    return QStringLiteral("opengl");
#endif
}

void* RhiContext::createStandaloneRhi()
{
#if defined(YAVE_HAVE_QRHI)
    QRhi::Flags flags = QRhi::EnableDebugMarkers;
#if defined(Q_OS_WIN)
    const QRhi::Implementation impl = QRhi::D3D11;
#elif defined(Q_OS_MACOS)
    const QRhi::Implementation impl = QRhi::Metal;
#else
    const QRhi::Implementation impl = QRhi::OpenGLES2;
#endif
    QRhi* rhi = QRhi::create(impl, nullptr, flags, nullptr);
    return rhi;
#else
    return nullptr;
#endif
}

} // namespace yave::render
