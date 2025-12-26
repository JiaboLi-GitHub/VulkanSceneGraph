/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/io/Output.h>

using namespace vsg;

// 构造函数：创建输出对象（默认）
// 输出对象用于将VSG对象序列化到输出流
// 初始化对象ID映射（nullptr映射到ID 0）
Output::Output() :
    version{vsgGetVersion()}
{
    objectIDMap[nullptr] = 0;
}

// 构造函数：使用选项创建输出对象
// in_options: 选项对象（包含写入器、路径等配置）
Output::Output(ref_ptr<const Options> in_options) :
    Output()
{
    options = in_options;
}

// 析构函数：销毁输出对象
Output::~Output()
{
}

// 检查版本是否小于指定版本
// major: 主版本号
// minor: 次版本号
// patch: 补丁版本号
// soversion: SO版本号
// 返回: 如果当前版本小于指定版本则返回true
// 用于版本兼容性检查
bool Output::version_less(uint32_t major, uint32_t minor, uint32_t patch, uint32_t soversion) const
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
bool Output::version_greater_equal(uint32_t major, uint32_t minor, uint32_t patch, uint32_t soversion) const
{
    return !version_less(major, minor, patch, soversion);
}
