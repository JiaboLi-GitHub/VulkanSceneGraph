/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/text/TextLayout.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// Font类的默认构造函数
// 创建空的字体对象
Font::Font()
{
}

// 从输入流读取Font对象
// 读取字体的度量信息、字符映射、字形度量和纹理图集
void Font::read(Input& input)
{
    Object::read(input);

    // 读取字体的基本度量信息
    input.read("ascender", ascender);  // 上升高度（字符基线以上的高度）
    input.read("descender", descender);  // 下降高度（字符基线以下的高度）
    input.read("height", height);  // 字体总高度

    // 读取字符映射、字形度量和纹理图集
    input.readObject("charmap", charmap);  // 字符到字形索引的映射
    input.readObject("glyphMetrics", glyphMetrics);  // 字形度量信息数组
    input.readObject("atlas", atlas);  // 字形纹理图集

    // 兼容旧版本：读取选项（0.5.5版本之前）
    if (input.version_less(0, 5, 5))
    {
        ref_ptr<Options> options;
        input.readObject("options", options);
    }
}

// 将Font对象写入输出流
// 写入字体的度量信息、字符映射、字形度量和纹理图集
void Font::write(Output& output) const
{
    Object::write(output);

    // 写入字体的基本度量信息
    output.write("ascender", ascender);
    output.write("descender", descender);
    output.write("height", height);

    // 写入字符映射、字形度量和纹理图集
    output.writeObject("charmap", charmap);
    output.writeObject("glyphMetrics", glyphMetrics);
    output.writeObject("atlas", atlas);

    // 兼容旧版本：写入选项（0.5.5版本之前）
    if (output.version_less(0, 5, 5))
    {
        ref_ptr<Options> options;
        output.writeObject("options", options);
    }
}

// 创建字体图像信息
// 为纹理图集和字形度量创建Vulkan图像信息和采样器
void Font::createFontImages()
{
    // 如果纹理图集图像信息不存在，创建它
    if (!atlasImageInfo)
    {
        // 创建线性采样器用于纹理图集
        auto sampler = Sampler::create();
        sampler->magFilter = VK_FILTER_LINEAR;  // 放大时使用线性过滤
        sampler->minFilter = VK_FILTER_LINEAR;  // 缩小时使用线性过滤
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;  // Mipmap使用线性过滤
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;  // U方向边界处理
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;  // V方向边界处理
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;  // W方向边界处理
        sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;  // 边界颜色为透明黑
        sampler->anisotropyEnable = VK_TRUE;  // 启用各向异性过滤
        sampler->maxAnisotropy = 16.0f;  // 最大各向异性级别
        sampler->maxLod = 12.0;  // 最大LOD级别
        atlasImageInfo = ImageInfo::create(sampler, atlas);
    }
    // 如果字形度量图像信息不存在，创建它
    if (!glyphImageInfo)
    {
        // 创建最近邻采样器用于字形度量（需要精确的数值）
        auto glyphMetricSampler = Sampler::create();
        glyphMetricSampler->magFilter = VK_FILTER_NEAREST;  // 放大时使用最近邻过滤
        glyphMetricSampler->minFilter = VK_FILTER_NEAREST;  // 缩小时使用最近邻过滤
        glyphMetricSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;  // Mipmap使用最近邻过滤
        glyphMetricSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // U方向边界处理
        glyphMetricSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // V方向边界处理
        glyphMetricSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // W方向边界处理
        glyphMetricSampler->unnormalizedCoordinates = VK_TRUE;  // 使用非归一化坐标

        // 将字形度量数据转换为2D数组格式，以便在GPU上作为纹理访问
        uint32_t stride = sizeof(vec4);
        uint32_t numVec4PerGlyph = static_cast<uint32_t>(sizeof(GlyphMetrics) / sizeof(vec4));
        uint32_t numGlyphs = static_cast<uint32_t>(glyphMetrics->valueCount());

        // 创建字形度量的2D数组代理，格式为R32G32B32A32_SFLOAT
        auto glyphMetricsProxy = vec4Array2D::create(glyphMetrics, 0, stride, numVec4PerGlyph, numGlyphs,
                                                     Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
        glyphImageInfo = ImageInfo::create(glyphMetricSampler, glyphMetricsProxy);
    }
}
