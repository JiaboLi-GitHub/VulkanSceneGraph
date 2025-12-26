/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Logger.h>
#include <vsg/io/spirv.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/utils/ShaderCompiler.h>

using namespace vsg;

// 读取文件到缓冲区（模板函数）
// buffer: 输出参数，用于存储文件内容
// filename: 文件名路径
// 返回: 如果成功读取则返回true
// 读取整个文件到指定类型的缓冲区（处理字节对齐）
template<typename T>
bool readFile(T& buffer, const vsg::Path& filename)
{
    std::ifstream fin(filename, std::ios::ate | std::ios::binary);
    if (!fin.is_open()) return false;

    size_t fileSize = fin.tellg();

    using value_type = typename T::value_type;
    size_t valueSize = sizeof(value_type);
    // 计算缓冲区大小（向上取整以处理字节对齐）
    size_t bufferSize = (fileSize + valueSize - 1) / valueSize;
    buffer.resize(bufferSize);

    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    fin.close();

    // buffer.size() * valueSize

    return true;
}

// 构造函数：创建SPIR-V读取器/写入器对象
// SPIR-V是Vulkan的着色器中间表示格式
spirv::spirv()
{
}

// 从文件读取SPIR-V着色器模块
// filename: 文件名路径
// options: 选项对象
// 返回: 着色器模块对象，如果失败则返回空指针
// 读取.spv文件并创建着色器模块对象
vsg::ref_ptr<vsg::Object> spirv::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "spirv read", COLOR_READ);

    if (!compatibleExtension(filename, options, ".spv")) return {};

    vsg::Path found_filename = vsg::findFile(filename, options);
    if (!found_filename) return {};

    auto sm = vsg::ShaderModule::create();
    readFile(sm->code, found_filename);
    return sm;
}

ref_ptr<vsg::Object> spirv::read(std::istream& fin, ref_ptr<const Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "spirv read", COLOR_READ);

    if (!compatibleExtension(options, ".spv")) return {};

    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    using value_type = vsg::ShaderModule::SPIRV::value_type;
    size_t valueSize = sizeof(value_type);
    size_t bufferSize = (fileSize + valueSize - 1) / valueSize;

    auto sm = vsg::ShaderModule::create();
    sm->code.resize(bufferSize);

    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(sm->code.data()), fileSize);

    return sm;
}

ref_ptr<vsg::Object> spirv::read(const uint8_t* ptr, size_t size, ref_ptr<const Options> options) const
{
    CPU_INSTRUMENTATION_L1(options ? options->instrumentation.get() : nullptr);

    if (!compatibleExtension(options, ".spv")) return {};

    using value_type = vsg::ShaderModule::SPIRV::value_type;
    size_t valueSize = sizeof(value_type);
    size_t bufferSize = (size + valueSize - 1) / valueSize;

    auto sm = vsg::ShaderModule::create();
    sm->code.assign(reinterpret_cast<const value_type*>(ptr), reinterpret_cast<const value_type*>(ptr) + bufferSize);

    return sm;
}

// 将着色器模块或着色器阶段写入SPIR-V文件
// object: 要写入的对象（ShaderModule或ShaderStage）
// filename: 文件名路径
// options: 选项对象
// 返回: 如果成功写入则返回true
// 如果着色器代码为空，先编译着色器，然后写入.spv文件
bool spirv::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    CPU_INSTRUMENTATION_L1_NC(options ? options->instrumentation.get() : nullptr, "spirv write", COLOR_WRITE);

    if (!compatibleExtension(filename, options, ".spv")) return false;

    const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
    const vsg::ShaderModule* sm = ss ? ss->module.get() : dynamic_cast<const vsg::ShaderModule*>(object);
    if (sm)
    {
        // 如果着色器代码为空，尝试编译
        if (sm->code.empty())
        {
            vsg::ShaderCompiler sc;
            if (!sc.compile(vsg::ref_ptr<vsg::ShaderStage>(const_cast<vsg::ShaderStage*>(ss))))
            {
                warn("spirv::write() Failed compile to spv.");
                return false;
            }
        }

        // 如果着色器代码不为空，写入文件
        if (!sm->code.empty())
        {
            std::ofstream fout(filename);
            fout.write(reinterpret_cast<const char*>(sm->code.data()), sm->code.size() * sizeof(vsg::ShaderModule::SPIRV::value_type));
            fout.close();
            return true;
        }
    }
    return false;
}

bool spirv::getFeatures(Features& features) const
{
    features.extensionFeatureMap[".spv"] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::WRITE_FILENAME);
    return true;
}
