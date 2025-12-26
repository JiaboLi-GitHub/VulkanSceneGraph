/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/PointLight.h>

using namespace vsg;

// PointLight类的默认构造函数
// 创建点光源，从空间中一个点向所有方向发射光线（如灯泡）
PointLight::PointLight()
{
}

// PointLight类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
PointLight::PointLight(const PointLight& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    position(rhs.position),  // 光源位置
    radius(rhs.radius)  // 光源影响半径
{
}

// 比较两个PointLight对象
// 首先比较基类，然后比较位置和半径
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int PointLight::compare(const Object& rhs_object) const
{
    int result = Light::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较光源位置
    if ((result = compare_value(position, rhs.position)) != 0) return result;
    // 比较光源影响半径
    return compare_value(radius, rhs.radius);
}

// 从输入流读取PointLight对象
// 读取点光源的位置和影响半径
void PointLight::read(Input& input)
{
    Light::read(input);

    // 读取光源位置
    input.read("position", position);
    // 版本1.1.3及以上：读取光源影响半径
    if (input.version_greater_equal(1, 1, 3))
    {
        input.read("radius", radius);
    }
}

// 将PointLight对象写入输出流
// 写入点光源的位置和影响半径
void PointLight::write(Output& output) const
{
    Light::write(output);

    // 写入光源位置
    output.write("position", position);
    // 版本1.1.3及以上：写入光源影响半径
    if (output.version_greater_equal(1, 1, 3))
    {
        output.write("radius", radius);
    }
}
