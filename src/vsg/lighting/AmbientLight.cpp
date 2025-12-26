/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/AmbientLight.h>

using namespace vsg;

// AmbientLight类的默认构造函数
// 创建环境光源，提供均匀的环境照明，没有方向性
AmbientLight::AmbientLight()
{
}

// AmbientLight类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
AmbientLight::AmbientLight(const AmbientLight& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop)
{
}

// 从输入流读取AmbientLight对象
// 读取环境光源的属性（继承自Light基类）
void AmbientLight::read(Input& input)
{
    Light::read(input);
}

// 将AmbientLight对象写入输出流
// 写入环境光源的属性（继承自Light基类）
void AmbientLight::write(Output& output) const
{
    Light::write(output);
}
