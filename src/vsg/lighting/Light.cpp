/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/lighting/AmbientLight.h>
#include <vsg/lighting/DirectionalLight.h>
#include <vsg/nodes/AbsoluteTransform.h>

using namespace vsg;

// Light类的默认构造函数
// 创建基础光源对象，所有光源类型的基类
Light::Light()
{
}

// Light类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
Light::Light(const Light& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name),  // 光源名称
    color(rhs.color),  // 光源颜色
    intensity(rhs.intensity),  // 光源强度
    shadowSettings(rhs.shadowSettings)  // 阴影设置
{
}

// 比较两个Light对象
// 首先比较基类，然后比较名称、颜色、强度和阴影设置
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int Light::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较名称
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    // 比较颜色
    if ((result = compare_value(color, rhs.color)) != 0) return result;
    // 比较强度
    if ((result = compare_value(intensity, rhs.intensity)) != 0) return result;
    // 比较阴影设置
    return compare_pointer(shadowSettings, rhs.shadowSettings);
}

// 从输入流读取Light对象
// 读取光源的名称、颜色、强度和阴影设置
void Light::read(Input& input)
{
    // 读取光源基本属性
    input.read("name", name);  // 光源名称
    input.read("color", color);  // 光源颜色（RGB）
    input.read("intensity", intensity);  // 光源强度

    // 版本1.1.3及以上：读取阴影设置
    if (input.version_greater_equal(1, 1, 3))
    {
        input.read("shadowSettings", shadowSettings);
    }
}

// 将Light对象写入输出流
// 写入光源的名称、颜色、强度和阴影设置
void Light::write(Output& output) const
{
    // 写入光源基本属性
    output.write("name", name);
    output.write("color", color);
    output.write("intensity", intensity);

    // 版本1.1.3及以上：写入阴影设置
    if (output.version_greater_equal(1, 1, 3))
    {
        output.write("shadowSettings", shadowSettings);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 创建前灯（Headlight）辅助函数
//

// 创建标准的前灯配置
// 创建一个包含环境光和方向光的组合光源，模拟相机前灯效果
// 环境光提供基础照明，方向光提供主要照明
// 返回值：包含环境光和方向光的绝对变换节点
ref_ptr<vsg::Node> vsg::createHeadlight()
{
    // 创建环境光，提供基础的环境照明
    auto ambientLight = vsg::AmbientLight::create();
    ambientLight->name = "ambient";
    ambientLight->color.set(1.0f, 1.0f, 1.0f);  // 白色
    ambientLight->intensity = 0.0044f;  // 较低的环境光强度

    // 创建方向光，模拟相机前灯的主要照明
    auto directionalLight = vsg::DirectionalLight::create();
    directionalLight->name = "headlight";
    directionalLight->color.set(1.0f, 1.0f, 1.0f);  // 白色
    directionalLight->intensity = 0.9956f;  // 较高的方向光强度
    directionalLight->direction.set(0.0, 0.0, -1.0);  // 沿负Z轴方向（相机前方）

    // 创建绝对变换节点，将两个光源组合在一起
    auto absoluteTransform = vsg::AbsoluteTransform::create();
    absoluteTransform->addChild(ambientLight);
    absoluteTransform->addChild(directionalLight);
    return absoluteTransform;
}
