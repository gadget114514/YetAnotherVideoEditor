#include "ColorSpace.h"

#include <cmath>

namespace yave::render {

YuvToRgbMatrix YuvToRgbMatrix::bt709Limited()
{
    YuvToRgbMatrix m;
    m.offsetY = 16.0f;
    // BT.709 limited range (16-235)
    m.m[0][0] = 1.16438356f;  m.m[0][1] =  0.0f;        m.m[0][2] =  1.79274107f;
    m.m[1][0] = 1.16438356f;  m.m[1][1] = -0.21324861f; m.m[1][2] = -0.53290933f;
    m.m[2][0] = 1.16438356f;  m.m[2][1] =  2.11240179f; m.m[2][2] =  0.0f;
    return m;
}

YuvToRgbMatrix YuvToRgbMatrix::bt709Full()
{
    YuvToRgbMatrix m;
    m.offsetY = 0.0f;
    m.m[0][0] = 1.0f;         m.m[0][1] =  0.0f;        m.m[0][2] =  1.5748f;
    m.m[1][0] = 1.0f;         m.m[1][1] = -0.18732427f; m.m[1][2] = -0.46812427f;
    m.m[2][0] = 1.0f;         m.m[2][1] =  1.8556f;     m.m[2][2] =  0.0f;
    return m;
}

YuvToRgbMatrix YuvToRgbMatrix::bt2020()
{
    YuvToRgbMatrix m;
    m.offsetY = 16.0f;
    // BT.2020 limited (近似)
    m.m[0][0] = 1.16438356f;  m.m[0][1] =  0.0f;        m.m[0][2] =  1.67867411f;
    m.m[1][0] = 1.16438356f;  m.m[1][1] = -0.18732427f; m.m[1][2] = -0.65042427f;
    m.m[2][0] = 1.16438356f;  m.m[2][1] =  2.14177232f; m.m[2][2] =  0.0f;
    return m;
}

ColorSpaceId ColorSpace::fromName(const QString& name)
{
    if (name == QLatin1String("bt2020"))
        return ColorSpaceId::Bt2020;
    if (name == QLatin1String("bt709-full"))
        return ColorSpaceId::Bt709Full;
    return ColorSpaceId::Bt709Limited;   ///< 既定
}

bool ColorSpace::needsToneMap(ColorSpaceId id)
{
    return id == ColorSpaceId::Bt2020;   ///< HDR ソースは SDR へトーンマップする
}

YuvToRgbMatrix ColorSpace::matrix(ColorSpaceId id)
{
    switch (id) {
    case ColorSpaceId::Bt709Full:   return YuvToRgbMatrix::bt709Full();
    case ColorSpaceId::Bt2020:      return YuvToRgbMatrix::bt2020();
    default:                        return YuvToRgbMatrix::bt709Limited();
    }
}

} // namespace yave::render
