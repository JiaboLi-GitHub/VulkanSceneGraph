/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <vsg/utils/CommandLine.h>

using namespace vsg;

// 添加读取器/写入器
// reader: 读取器/写入器对象
// 将读取器/写入器添加到组合中
void CompositeReaderWriter::add(ref_ptr<ReaderWriter> reader)
{
    readerWriters.emplace_back(reader);
}

// 从输入流读取组合读取器/写入器
// input: 输入流对象
// 读取读取器/写入器列表
void CompositeReaderWriter::read(Input& input)
{
    readerWriters.clear();
    uint32_t count = input.readValue<uint32_t>("NumReaderWriters");
    for (uint32_t i = 0; i < count; ++i)
    {
        auto rw = input.readObject<ReaderWriter>("ReaderWriter");
        if (rw) readerWriters.push_back(rw);
    }
}

// 将组合读取器/写入器写入输出流
// output: 输出流对象
// 写入读取器/写入器列表
void CompositeReaderWriter::write(Output& output) const
{
    output.writeValue<uint32_t>("NumReaderWriters", readerWriters.size());
    for (const auto& rw : readerWriters)
    {
        output.writeObject("ReaderWriter", rw);
    }
}

// 从文件读取对象
// filename: 文件名路径
// options: 选项对象
// 返回: 读取的对象，如果所有读取器都失败则返回空指针
// 按顺序尝试每个读取器，返回第一个成功读取的对象
vsg::ref_ptr<vsg::Object> CompositeReaderWriter::read(const vsg::Path& filename, ref_ptr<const Options> options) const
{
    for (auto& reader : readerWriters)
    {
        if (auto object = reader->read(filename, options); object.valid()) return object;
    }
    return vsg::ref_ptr<vsg::Object>();
}

// 从输入流读取对象
// fin: 输入流对象
// options: 选项对象
// 返回: 读取的对象，如果所有读取器都失败则返回空指针
// 按顺序尝试每个读取器，返回第一个成功读取的对象
vsg::ref_ptr<vsg::Object> CompositeReaderWriter::read(std::istream& fin, ref_ptr<const Options> options) const
{
    for (auto& reader : readerWriters)
    {
        if (auto object = reader->read(fin, options); object.valid()) return object;
    }
    return vsg::ref_ptr<vsg::Object>();
}

// 从内存缓冲区读取对象
// ptr: 内存缓冲区指针
// size: 缓冲区大小
// options: 选项对象
// 返回: 读取的对象，如果所有读取器都失败则返回空指针
// 按顺序尝试每个读取器，返回第一个成功读取的对象
vsg::ref_ptr<vsg::Object> CompositeReaderWriter::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    for (auto& reader : readerWriters)
    {
        if (auto object = reader->read(ptr, size, options); object.valid()) return object;
    }
    return vsg::ref_ptr<vsg::Object>();
}

// 将对象写入文件
// object: 要写入的对象
// filename: 文件名路径
// options: 选项对象
// 返回: 如果成功写入则返回true
// 按顺序尝试每个写入器，返回第一个成功写入的结果
bool CompositeReaderWriter::write(const vsg::Object* object, const vsg::Path& filename, ref_ptr<const Options> options) const
{
    for (auto& writer : readerWriters)
    {
        if (writer->write(object, filename, options)) return true;
    }
    return false;
}

// 将对象写入输出流
// object: 要写入的对象
// fout: 输出流对象
// options: 选项对象
// 返回: 如果成功写入则返回true
// 按顺序尝试每个写入器，返回第一个成功写入的结果
bool CompositeReaderWriter::write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> options) const
{
    for (auto& writer : readerWriters)
    {
        if (writer->write(object, fout, options)) return true;
    }
    return false;
}

// 从命令行参数读取选项
// options: 选项对象
// arguments: 命令行参数对象
// 返回: 如果任何读取器读取了选项则返回true
// 让每个读取器/写入器读取其特定选项
bool CompositeReaderWriter::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = false;
    for (auto& rw : readerWriters)
    {
        if (rw->readOptions(options, arguments)) result = true;
    }
    return result;
}

// 获取功能特性
// features: 功能特性对象
// 返回: 如果任何读取器/写入器提供了功能特性则返回true
// 收集所有读取器/写入器的功能特性
bool CompositeReaderWriter::getFeatures(Features& features) const
{
    bool result = false;
    for (auto& rw : readerWriters)
    {
        if (rw->getFeatures(features)) result = true;
    }
    return result;
}

bool vsg::getFeatures(ref_ptr<const Options> options, ReaderWriter::Features& features)
{
    if (!options) return false;

    bool result = false;
    for (auto& rw : options->readerWriters)
    {
        if (rw->getFeatures(features)) result = true;
    }
    return result;
}
