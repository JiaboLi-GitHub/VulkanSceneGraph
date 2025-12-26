/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/Light.h>
#include <vsg/nodes/AbsoluteTransform.h>

using namespace vsg;

// ShadowSettings类的构造函数
// 创建阴影设置对象，用于配置光源的阴影参数
// in_shadowMapCount: 阴影贴图数量
ShadowSettings::ShadowSettings(uint32_t in_shadowMapCount) :
    shadowMapCount(in_shadowMapCount)
{
}

// ShadowSettings类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
ShadowSettings::ShadowSettings(const ShadowSettings& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    shadowMapCount(rhs.shadowMapCount)  // 阴影贴图数量
{
}

// 比较两个ShadowSettings对象
// 首先比较基类，然后比较阴影贴图数量
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int ShadowSettings::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较阴影贴图数量
    return compare_value(shadowMapCount, rhs.shadowMapCount);
}

// 从输入流读取ShadowSettings对象
// 读取阴影贴图数量
void ShadowSettings::read(Input& input)
{
    input.read("shadowMapCount", shadowMapCount);
}

// 将ShadowSettings对象写入输出流
// 写入阴影贴图数量
void ShadowSettings::write(Output& output) const
{
    output.write("shadowMapCount", shadowMapCount);
}
