/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Input.h>
#include <vsg/io/Options.h>

using namespace vsg;

// 构造函数：使用对象工厂和选项创建输入对象
// in_objectFactory: 对象工厂对象（用于创建反序列化的对象）
// in_options: 选项对象（包含读取器/写入器、路径等配置）
// 输入对象用于从输入流读取VSG对象的序列化数据
Input::Input(ref_ptr<ObjectFactory> in_objectFactory, ref_ptr<const Options> in_options) :
    objectFactory(in_objectFactory),
    options(in_options),
    version{vsgGetVersion()}
{
    // 初始化对象ID映射（ID 0映射到nullptr）
    objectIDMap[0] = nullptr;
}

// 析构函数：销毁输入对象
Input::~Input()
{
}

// 检查版本是否小于指定版本
// major: 主版本号
// minor: 次版本号
// patch: 补丁版本号
// soversion: SO版本号
// 返回: 如果当前版本小于指定版本则返回true
// 用于版本兼容性检查
bool Input::version_less(uint32_t major, uint32_t minor, uint32_t patch, uint32_t soversion) const
{
    return version < VsgVersion{major, minor, patch, soversion};
}

// 检查版本是否大于等于指定版本
// major: 主版本号
// minor: 次版本号
// patch: 补丁版本号
// soversion: SO版本号
// 返回: 如果当前版本大于等于指定版本则返回true
// 用于版本兼容性检查
bool Input::version_greater_equal(uint32_t major, uint32_t minor, uint32_t patch, uint32_t soversion) const
{
    return !version_less(major, minor, patch, soversion);
}
