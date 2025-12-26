/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/HardShadows.h>

using namespace vsg;

// HardShadows类的构造函数
// 创建硬阴影设置，产生清晰的阴影边缘（无柔化）
// in_shadowMaps: 阴影贴图数量
HardShadows::HardShadows(uint32_t in_shadowMaps) :
    Inherit(in_shadowMaps)
{
}

// HardShadows类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
HardShadows::HardShadows(const HardShadows& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
}

// 比较两个HardShadows对象
// 直接调用基类的比较方法
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int HardShadows::compare(const Object& rhs_object) const
{
    return ShadowSettings::compare(rhs_object);
}

// 从输入流读取HardShadows对象
// 读取硬阴影设置（继承自ShadowSettings）
void HardShadows::read(Input& input)
{
    ShadowSettings::read(input);
}

// 将HardShadows对象写入输出流
// 写入硬阴影设置（继承自ShadowSettings）
void HardShadows::write(Output& output) const
{
    ShadowSettings::write(output);
}
