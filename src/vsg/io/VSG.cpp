/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Version.h>
#include <vsg/io/AsciiInput.h>
#include <vsg/io/AsciiOutput.h>
#include <vsg/io/BinaryInput.h>
#include <vsg/io/BinaryOutput.h>
#include <vsg/io/Logger.h>
#include <vsg/io/VSG.h>
#include <vsg/io/mem_stream.h>

using namespace vsg;

// 使用静态句柄，在启动时初始化一次，避免与调用std::locale::classic()相关的多线程问题
auto s_class_locale = std::locale::classic();

// 解析版本字符串
// version_string: 版本字符串（格式：major.minor.patch.soversion）
// 返回: 版本结构体
// 将版本字符串解析为版本号组件
static VsgVersion parseVersion(std::string version_string)
{
    VsgVersion version{0, 0, 0, 0};

    // 将点号替换为空格以便使用stringstream解析
    for (auto& c : version_string)
    {
        if (c == '.') c = ' ';
    }

    std::stringstream str(version_string);

    str >> version.major;
    str >> version.minor;
    str >> version.patch;
    str >> version.soversion;

    return version;
}

// 构造函数：创建VSG读取器/写入器对象
// VSG格式是VSG的原生序列化格式，支持ASCII和二进制两种格式
VSG::VSG() :
    _objectFactory(ObjectFactory::instance())
{
}

// 读取文件头
// fin: 输入流对象
// 返回: 格式信息（格式类型和版本）
// 从输入流读取VSG文件头（"#vsga"或"#vsgb"）和版本信息
VSG::FormatInfo VSG::readHeader(std::istream& fin) const
{
    fin.imbue(s_class_locale);

    const char* match_token_ascii = "#vsga";
    const char* match_token_binary = "#vsgb";
    char read_token[5];
    fin.read(read_token, 5);

    FormatType type = NOT_RECOGNIZED;
    if (std::strncmp(match_token_ascii, read_token, 5) == 0)
        type = ASCII;
    else if (std::strncmp(match_token_binary, read_token, 5) == 0)
        type = BINARY;

    if (type == NOT_RECOGNIZED)
    {
        error("Header token not matched [", read_token, "]");
        return FormatInfo(NOT_RECOGNIZED, VsgVersion{0, 0, 0, 0});
    }

    // 读取版本字符串
    std::string version_string;
    std::getline(fin, version_string);

    auto version = parseVersion(version_string);

    return FormatInfo(type, version);
}

// 写入文件头
// fout: 输出流对象
// formatInfo: 格式信息（格式类型和版本）
// 将VSG文件头（"#vsga"或"#vsgb"）和版本信息写入输出流
void VSG::writeHeader(std::ostream& fout, const FormatInfo& formatInfo) const
{
    if (formatInfo.first == NOT_RECOGNIZED) return;

    fout.imbue(s_class_locale);
    if (formatInfo.first == BINARY)
        fout << "#vsgb";
    else
        fout << "#vsga";

    auto version = formatInfo.second;
    fout << " " << version.major << "." << version.minor << "." << version.patch << "\n";
}

// 从文件读取对象
// filename: 文件名路径
// options: 选项对象
// 返回: 读取的对象，如果失败则返回空指针
// 根据文件扩展名（.vsgb、.vsgt）和文件头确定格式（ASCII或二进制），然后读取对象
vsg::ref_ptr<vsg::Object> VSG::read(const vsg::Path& filename, ref_ptr<const Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "VSG read", COLOR_READ);

    if (!compatibleExtension(filename, options, ".vsgb", ".vsgt")) return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    std::ifstream fin(filenameToUse, std::ios::in | std::ios::binary);
    if (!fin) return {};

    // 读取文件头以确定格式类型和版本
    auto [type, version] = readHeader(fin);
    if (type == BINARY)
    {
        // 使用二进制输入读取
        vsg::BinaryInput input(fin, _objectFactory, options);
        input.filename = filenameToUse;
        input.version = version;
        return input.readObject("Root");
    }
    else if (type == ASCII)
    {
        // 使用ASCII输入读取
        vsg::AsciiInput input(fin, _objectFactory, options);
        input.filename = filenameToUse;
        input.version = version;
        return input.readObject("Root");
    }

    // 返回空指针，因为没有找到加载文件的方法
    return {};
}

vsg::ref_ptr<vsg::Object> VSG::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "VSG read", COLOR_READ);

    if (options && !compatibleExtension(options, ".vsgb", ".vsgt")) return {};

    auto [type, version] = readHeader(fin);
    if (type == BINARY)
    {
        vsg::BinaryInput input(fin, _objectFactory, options);
        input.version = version;
        return input.readObject("Root");
    }
    else if (type == ASCII)
    {
        vsg::AsciiInput input(fin, _objectFactory, options);
        input.version = version;
        return input.readObject("Root");
    }

    return {};
}

vsg::ref_ptr<vsg::Object> VSG::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "VSG read", COLOR_READ);

    if (options && !compatibleExtension(options, ".vsgb", ".vsgt")) return {};

    mem_stream fin(ptr, size);
    return read(fin, options);
}

// 将对象写入文件
// object: 要写入的对象
// filename: 文件名路径
// options: 选项对象
// 返回: 如果成功写入则返回true
// 根据文件扩展名（.vsgb为二进制，.vsga/.vsgt为ASCII）选择格式并写入对象
bool VSG::write(const vsg::Object* object, const vsg::Path& filename, ref_ptr<const Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "VSG write", COLOR_READ);

    auto version = vsgGetVersion();

    // 如果选项中有版本字符串，使用它
    if (options)
    {
        std::string version_string;
        if (options->getValue("version", version_string))
        {
            version = parseVersion(version_string);
        }
    }

    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext == ".vsgb")
    {
        // 二进制格式
        std::ofstream fout(filename, std::ios::out | std::ios::binary);
        writeHeader(fout, FormatInfo{BINARY, version});

        vsg::BinaryOutput output(fout, options);
        output.version = version;
        output.writeObject("Root", object);
        return true;
    }
    else if (ext == ".vsga" || ext == ".vsgt")
    {
        // ASCII格式
        std::ofstream fout(filename, std::ios::out | std::ios::binary);
        writeHeader(fout, FormatInfo{ASCII, version});

        vsg::AsciiOutput output(fout, options);
        output.version = version;
        output.writeObject("Root", object);
        return true;
    }
    else
    {
        return false;
    }
}

bool VSG::write(const vsg::Object* object, std::ostream& fout, ref_ptr<const Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "VSG write", COLOR_WRITE);

    if (options && !compatibleExtension(options, ".vsgb", ".vsgt")) return {};

    auto version = vsgGetVersion();
    bool asciiFormat = true;

    if (options)
    {
        if (options->extensionHint && options->extensionHint == ".vsgb") asciiFormat = false;

        std::string version_string;
        if (options->getValue("version", version_string))
        {
            version = parseVersion(version_string);
        }
    }

    if (asciiFormat)
    {
        writeHeader(fout, FormatInfo(ASCII, version));

        vsg::AsciiOutput output(fout, options);
        output.version = version;
        output.writeObject("Root", object);
        return true;
    }
    else
    {
        writeHeader(fout, FormatInfo(BINARY, version));

        vsg::BinaryOutput output(fout, options);
        output.version = version;
        output.writeObject("Root", object);
        return true;
    }
}

bool VSG::getFeatures(Features& features) const
{
    features.extensionFeatureMap[".vsgb"] = static_cast<FeatureMask>(READ_FILENAME | READ_ISTREAM | READ_MEMORY | WRITE_FILENAME | WRITE_OSTREAM);
    features.extensionFeatureMap[".vsgt"] = static_cast<FeatureMask>(READ_FILENAME | READ_ISTREAM | READ_MEMORY | WRITE_FILENAME | WRITE_OSTREAM);
    return true;
}
