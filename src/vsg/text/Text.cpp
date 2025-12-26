/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/Text.h>

#include "shaders/text_ShaderSet.cpp"

using namespace vsg;

// 从输入流读取Text对象
// 读取文本的字体、着色器集、布局技术、布局和文本内容
void Text::read(Input& input)
{
    Node::read(input);

    // 读取字体对象
    input.readObject("font", font);

    // 版本0.5.2及以上：读取着色器集
    if (input.version_greater_equal(0, 5, 2))
    {
        input.readObject("shaderSet", shaderSet);
    }

    // 读取布局技术、布局和文本内容
    input.readObject("technique", technique);
    input.readObject("layout", layout);
    input.readObject("text", text);

    // 设置文本渲染
    setup(0, input.options);
}

// 将Text对象写入输出流
// 写入文本的字体、着色器集、布局技术、布局和文本内容
void Text::write(Output& output) const
{
    Node::write(output);

    // 写入字体对象
    output.writeObject("font", font);

    // 版本0.5.2及以上：写入着色器集
    if (output.version_greater_equal(0, 5, 2))
    {
        output.writeObject("shaderSet", shaderSet);
    }

    // 写入布局技术、布局和文本内容
    output.writeObject("technique", technique);
    output.writeObject("layout", layout);
    output.writeObject("text", text);
}

// 设置文本渲染
// 初始化布局和布局技术，然后调用技术的setup方法
// minimumAllocation: 最小分配大小
// options: 选项对象
void Text::setup(uint32_t minimumAllocation, ref_ptr<const Options> options)
{
    // 如果没有布局，创建标准布局
    if (!layout) layout = StandardLayout::create();
    // 如果没有布局技术，创建CPU布局技术
    if (!technique) technique = CpuLayoutTechnique::create();

    // 调用布局技术的setup方法进行设置
    technique->setup(this, minimumAllocation, options);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 创建文本着色器集
//

// 创建或获取文本着色器集
// 如果options中已存在文本着色器集，则返回它；否则创建新的
// options: 选项对象，可能包含已缓存的着色器集
// 返回值：文本着色器集的引用指针
ref_ptr<ShaderSet> vsg::createTextShaderSet(ref_ptr<const Options> options)
{
    if (options)
    {
        // 检查options对象中是否已经分配了着色器集，如果有则返回它
        if (auto itr = options->shaderSets.find("text"); itr != options->shaderSets.end()) return itr->second;
    }

    // 创建新的文本着色器集
    return text_ShaderSet();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CountGlyphs访问者 - 用于统计文本中的字形数量
//

// 统计stringValue类型文本中的字形数量
void CountGlyphs::apply(const stringValue& text)
{
    count += text.value().size();
}

// 统计wstringValue类型文本中的字形数量
void CountGlyphs::apply(const wstringValue& text)
{
    count += text.value().size();
}

// 统计ubyteArray类型文本中的字形数量
void CountGlyphs::apply(const ubyteArray& text)
{
    count += text.size();
}

// 统计ushortArray类型文本中的字形数量
void CountGlyphs::apply(const ushortArray& text)
{
    count += text.size();
}

// 统计uintArray类型文本中的字形数量
void CountGlyphs::apply(const uintArray& text)
{
    count += text.size();
}
