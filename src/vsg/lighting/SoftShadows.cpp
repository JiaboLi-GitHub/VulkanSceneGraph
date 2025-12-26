/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/SoftShadows.h>

using namespace vsg;

// SoftShadows类的构造函数
// 创建软阴影设置，产生柔化的阴影边缘（半影效果）
// in_shadowMaps: 阴影贴图数量
// in_penumbraRadius: 半影半径，控制阴影边缘的柔化程度
SoftShadows::SoftShadows(uint32_t in_shadowMaps, float in_penumbraRadius) :
    Inherit(in_shadowMaps),
    penumbraRadius(in_penumbraRadius)  // 半影半径
{
}

// SoftShadows类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
SoftShadows::SoftShadows(const SoftShadows& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    penumbraRadius(rhs.penumbraRadius)  // 半影半径
{
}

// 比较两个SoftShadows对象
// 首先比较基类，然后比较半影半径
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int SoftShadows::compare(const Object& rhs_object) const
{
    int result = ShadowSettings::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较半影半径
    return compare_value(penumbraRadius, rhs.penumbraRadius);
}

// 从输入流读取SoftShadows对象
// 读取软阴影设置和半影半径
void SoftShadows::read(Input& input)
{
    ShadowSettings::read(input);

    // 读取半影半径
    input.read("penumbraRadius", penumbraRadius);
}

// 将SoftShadows对象写入输出流
// 写入软阴影设置和半影半径
void SoftShadows::write(Output& output) const
{
    ShadowSettings::write(output);

    // 写入半影半径
    output.write("penumbraRadius", penumbraRadius);
}
