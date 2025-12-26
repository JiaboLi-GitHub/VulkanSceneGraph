/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/DirectionalLight.h>

using namespace vsg;

// DirectionalLight类的默认构造函数
// 创建方向光源，模拟平行光（如太阳光），所有光线都来自同一方向
DirectionalLight::DirectionalLight()
{
}

// DirectionalLight类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
DirectionalLight::DirectionalLight(const DirectionalLight& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    direction(rhs.direction),  // 光线方向向量
    angleSubtended(rhs.angleSubtended)  // 光源张角（用于软阴影计算）
{
}

// 比较两个DirectionalLight对象
// 首先比较基类，然后比较方向和角度
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int DirectionalLight::compare(const Object& rhs_object) const
{
    int result = Light::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较方向向量
    if ((result = compare_value(direction, rhs.direction)) != 0) return result;
    // 比较光源张角
    return compare_value(angleSubtended, rhs.angleSubtended);
}

// 从输入流读取DirectionalLight对象
// 读取方向光源的方向和张角
void DirectionalLight::read(Input& input)
{
    Light::read(input);

    // 读取光线方向向量
    input.read("direction", direction);

    // 版本1.1.2及以上：读取光源张角
    if (input.version_greater_equal(1, 1, 2))
    {
        input.read("angleSubtended", angleSubtended);
    }
}

// 将DirectionalLight对象写入输出流
// 写入方向光源的方向和张角
void DirectionalLight::write(Output& output) const
{
    Light::write(output);

    // 写入光线方向向量
    output.write("direction", direction);

    // 版本1.1.2及以上：写入光源张角
    if (output.version_greater_equal(1, 1, 2))
    {
        output.write("angleSubtended", angleSubtended);
    }
}
