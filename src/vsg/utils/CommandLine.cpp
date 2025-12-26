/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/utils/CommandLine.h>

using namespace vsg;

// CommandLine类的构造函数
// 创建命令行参数解析器，用于解析命令行参数
// argc: 命令行参数数量（指针，可能被修改）
// argv: 命令行参数数组
CommandLine::CommandLine(int* argc, char** argv) :
    _argc(argc),  // 参数数量指针
    _argv(argv)  // 参数数组
{
}

// 读取命令行选项
// 从命令行参数中读取选项并应用到Options对象
// options: 选项对象，用于存储解析后的选项
// 返回值：true表示成功读取，false表示失败
bool CommandLine::read(Options* options)
{
    return (options != nullptr) && options->readOptions(*this);
}
