/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/read.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建着色器阶段对象（默认）
// 着色器阶段用于定义管线中的一个着色器阶段（顶点、片段、几何、计算等）
ShaderStage::ShaderStage()
{
}

// 拷贝构造函数：从另一个着色器阶段对象创建新的着色器阶段对象
// rhs: 要拷贝的着色器阶段对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝掩码、标志、阶段类型、着色器模块、入口点名称和特化常量
ShaderStage::ShaderStage(const ShaderStage& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    mask(rhs.mask),
    flags(rhs.flags),
    stage(rhs.stage),
    module(copyop(rhs.module)),
    entryPointName(rhs.entryPointName),
    specializationConstants(rhs.specializationConstants)
{
}

// 构造函数：使用着色器阶段类型、入口点名称和着色器模块创建着色器阶段对象
// in_stage: 着色器阶段标志（顶点、片段、几何、计算等）
// in_entryPointName: 着色器入口点函数名称
// shaderModule: 着色器模块对象（包含SPIR-V代码）
ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, ref_ptr<ShaderModule> shaderModule) :
    stage(in_stage),
    module(shaderModule),
    entryPointName(in_entryPointName)
{
}

// 构造函数：使用着色器阶段类型、入口点名称、源代码和编译设置创建着色器阶段对象
// in_stage: 着色器阶段标志
// in_entryPointName: 着色器入口点函数名称
// source: 着色器源代码（GLSL等）
// hints: 着色器编译设置（可选）
// 从源代码创建着色器模块（将在编译时转换为SPIR-V）
ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const std::string& source, ref_ptr<ShaderCompileSettings> hints) :
    stage(in_stage),
    module(ShaderModule::create(source, hints)),
    entryPointName(in_entryPointName)
{
}

// 构造函数：使用着色器阶段类型、入口点名称和SPIR-V代码创建着色器阶段对象
// in_stage: 着色器阶段标志
// in_entryPointName: 着色器入口点函数名称
// code: SPIR-V字节码
ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const ShaderModule::SPIRV& code) :
    stage(in_stage),
    module(ShaderModule::create(code)),
    entryPointName(in_entryPointName)
{
}

// 构造函数：使用着色器阶段类型、入口点名称、源代码和SPIR-V代码创建着色器阶段对象
// in_stage: 着色器阶段标志
// in_entryPointName: 着色器入口点函数名称
// source: 着色器源代码（用于调试和重新编译）
// code: SPIR-V字节码（已编译的代码）
ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const std::string& source, const ShaderModule::SPIRV& code) :
    stage(in_stage),
    module(ShaderModule::create(source, code)),
    entryPointName(in_entryPointName)
{
}

// 析构函数：销毁着色器阶段对象
ShaderStage::~ShaderStage()
{
}

int ShaderStage::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(flags, rhs.flags))) return result;
    if ((result = compare_value(stage, rhs.stage))) return result;
    if ((result = compare_pointer(module, rhs.module))) return result;
    if ((result = compare_value(entryPointName, rhs.entryPointName))) return result;

    if (specializationConstants.size() < rhs.specializationConstants.size()) return -1;
    if (specializationConstants.size() > rhs.specializationConstants.size()) return 1;
    if (specializationConstants.empty()) return 0;

    auto rhs_itr = rhs.specializationConstants.begin();
    for (auto lhs_itr = specializationConstants.begin(); lhs_itr != specializationConstants.end(); ++lhs_itr, ++rhs_itr)
    {
        if ((result = compare_value(lhs_itr->first, rhs_itr->first))) return result;
        if ((result = compare_pointer(lhs_itr->second, rhs_itr->second))) return result;
    }

    return 0;
}

// 从文件读取着色器阶段对象
// stage: 着色器阶段标志
// entryPointName: 着色器入口点函数名称
// filename: 文件路径
// options: 读取选项
// 返回: 着色器阶段对象
// 从文件读取着色器阶段或着色器模块，如果是着色器模块则创建着色器阶段
ref_ptr<ShaderStage> ShaderStage::read(VkShaderStageFlagBits stage, const std::string& entryPointName, const Path& filename, ref_ptr<const Options> options)
{
    auto object = vsg::read(filename, options);
    if (!object) return {};

    auto st = object.cast<vsg::ShaderStage>();
    if (!st)
    {
        auto sm = object.cast<vsg::ShaderModule>();
        return ShaderStage::create_if(sm.valid(), stage, entryPointName, sm);
    }

    st->stage = stage;
    st->entryPointName = entryPointName;
    return st;
}

// 从输入流读取着色器阶段对象
// stage: 着色器阶段标志
// entryPointName: 着色器入口点函数名称
// fin: 输入流
// options: 读取选项
// 返回: 着色器阶段对象
// 从输入流读取着色器阶段或着色器模块，如果是着色器模块则创建着色器阶段
ref_ptr<ShaderStage> ShaderStage::read(VkShaderStageFlagBits stage, const std::string& entryPointName, std::istream& fin, ref_ptr<const Options> options)
{
    auto object = vsg::read(fin, options);
    if (!object) return {};

    auto st = object.cast<vsg::ShaderStage>();
    if (!st)
    {
        auto sm = object.cast<vsg::ShaderModule>();
        return ShaderStage::create_if(sm.valid(), stage, entryPointName, sm);
    }

    st->stage = stage;
    st->entryPointName = entryPointName;
    return st;
}

void ShaderStage::read(Input& input)
{
    Object::read(input);

    if (input.version_greater_equal(1, 0, 9))
    {
        input.read("mask", mask);
    }

    input.readValue<int32_t>("stage", stage);
    input.read("entryPointName", entryPointName);
    input.readObject("module", module);

    specializationConstants.clear();
    uint32_t numValues = input.readValue<uint32_t>("NumSpecializationConstants");
    for (uint32_t i = 0; i < numValues; ++i)
    {
        uint32_t id = input.readValue<uint32_t>("id");
        input.readObject("data", specializationConstants[id]);
    }
}

void ShaderStage::write(Output& output) const
{
    Object::write(output);

    if (output.version_greater_equal(1, 0, 9))
    {
        output.write("mask", mask);
    }

    output.writeValue<int32_t>("stage", stage);
    output.write("entryPointName", entryPointName);
    output.writeObject("module", module);

    output.writeValue<uint32_t>("NumSpecializationConstants", specializationConstants.size());
    for (auto& [id, data] : specializationConstants)
    {
        output.writeValue<uint32_t>("id", id);
        output.writeObject("data", data);
    }
}

// 应用着色器阶段到管线着色器阶段创建信息
// context: 编译上下文对象
// stageInfo: 管线着色器阶段创建信息（输出参数）
// 填充着色器阶段信息，包括阶段类型、模块句柄、入口点名称和特化常量（如果存在）
void ShaderStage::apply(Context& context, VkPipelineShaderStageCreateInfo& stageInfo) const
{
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage;
    stageInfo.module = module->vk(context.deviceID);
    stageInfo.pName = entryPointName.c_str();

    // 如果存在特化常量，打包它们
    if (specializationConstants.empty())
    {
        stageInfo.pSpecializationInfo = nullptr;
    }
    else
    {
        // 计算打包数据的总大小
        uint32_t packedDataSize = 0;
        for (auto& id_data : specializationConstants)
        {
            packedDataSize += static_cast<uint32_t>(id_data.second->dataSize());
        }

        // 从临时内存分配特化映射条目和数据缓冲区
        auto mapEntries = context.scratchMemory->allocate<VkSpecializationMapEntry>(specializationConstants.size());
        auto packedData = context.scratchMemory->allocate<uint8_t>(packedDataSize);
        uint32_t offset = 0;
        uint32_t i = 0;
        // 填充映射条目并复制数据
        for (auto& [id, data] : specializationConstants)
        {
            mapEntries[i++] = VkSpecializationMapEntry{id, offset, data->dataSize()};
            std::memcpy(packedData + offset, static_cast<uint8_t*>(data->dataPointer()), data->dataSize());
            offset += static_cast<uint32_t>(data->dataSize());
        }

        // 分配并填充特化信息
        auto specializationInfo = context.scratchMemory->allocate<VkSpecializationInfo>(1);

        stageInfo.pSpecializationInfo = specializationInfo;

        // 将着色器阶段的值分配到特化信息中
        specializationInfo->mapEntryCount = static_cast<uint32_t>(specializationConstants.size());
        specializationInfo->pMapEntries = mapEntries;
        specializationInfo->dataSize = packedDataSize;
        specializationInfo->pData = packedData;
    }
}

// 编译着色器阶段
// context: 编译上下文对象
// 编译着色器模块（如果存在）
void ShaderStage::compile(Context& context)
{
    if (module)
    {
        module->compile(context);
    }
}
