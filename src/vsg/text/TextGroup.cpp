/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/TextGroup.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// 比较两个TextGroup对象
// 首先比较基类，然后比较子对象列表
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int TextGroup::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较子对象列表
    return compare_pointer_container(children, rhs.children);
}

// 从输入流读取TextGroup对象
// 读取文本组的字体、着色器集、布局技术和子对象列表
void TextGroup::read(Input& input)
{
    Node::read(input);

    // 读取字体、着色器集和布局技术
    input.readObject("font", font);
    input.readObject("shaderSet", shaderSet);
    input.readObject("technique", technique);

    // 读取子文本对象列表
    input.readObjects("children", children);

    // 设置文本组渲染
    setup(0, input.options);
}

// 将TextGroup对象写入输出流
// 写入文本组的字体、着色器集、布局技术和子对象列表
void TextGroup::write(Output& output) const
{
    Node::write(output);

    // 写入字体、着色器集和布局技术
    output.writeObject("font", font);
    output.writeObject("shaderSet", shaderSet);
    output.writeObject("technique", technique);

    // 写入子文本对象列表
    output.writeObjects("children", children);
}

// 添加子文本对象
// 将文本对象添加到组中，并从文本对象中提取共享的渲染属性
// text: 要添加的文本对象
void TextGroup::addChild(ref_ptr<Text> text)
{
    // 如果组中没有字体/着色器集/技术，从第一个子对象中获取
    if (!font) font = text->font;
    if (!shaderSet) shaderSet = text->shaderSet;
    if (!technique) technique = text->technique;

    // TextGroup负责渲染相关的属性，因此清空子对象的这些属性
    text->technique = {};
    text->font = {};
    text->shaderSet = {};

    // 将文本对象添加到子对象列表
    children.push_back(text);
}

// 设置文本组渲染
// 初始化布局技术并调用其setup方法
// minimumAllocation: 最小分配大小
// options: 选项对象
void TextGroup::setup(uint32_t minimumAllocation, ref_ptr<const Options> options)
{
    // 如果没有子对象，直接返回
    if (children.empty()) return;

    // 如果没有布局技术，创建CPU布局技术
    if (!technique) technique = CpuLayoutTechnique::create();

    // 调用布局技术的setup方法进行设置
    technique->setup(this, minimumAllocation, options);
}
