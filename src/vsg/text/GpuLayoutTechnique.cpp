/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/Draw.h>
#include <vsg/core/Array2D.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/Text.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// GpuLayoutTechniqueArrayState类
// 用于GPU布局技术的数组状态，支持GPU端文本布局计算和广告牌效果
// 在渲染时动态计算顶点位置，实现文本始终面向相机的效果
class VSG_DECLSPEC GpuLayoutTechniqueArrayState : public Inherit<ArrayState, GpuLayoutTechniqueArrayState>
{
public:
    // 从技术、文本和广告牌标志创建数组状态
    GpuLayoutTechniqueArrayState(const GpuLayoutTechnique* in_technique, const Text* in_text, bool in_billboard) :
        technique(in_technique),
        text(in_text),
        billboard(in_billboard)
    {
    }

    // 拷贝构造函数
    GpuLayoutTechniqueArrayState(const GpuLayoutTechniqueArrayState& rhs) :
        Inherit(rhs),
        technique(rhs.technique),
        text(rhs.text),
        billboard(rhs.billboard)
    {
    }

    // 从ArrayState创建数组状态
    explicit GpuLayoutTechniqueArrayState(const ArrayState& rhs) :
        Inherit(rhs)
    {
    }

    // 克隆数组状态
    ref_ptr<ArrayState> cloneArrayState() override
    {
        return GpuLayoutTechniqueArrayState::create(*this);
    }

    // 从另一个数组状态克隆，但保留当前技术、文本和广告牌标志
    ref_ptr<ArrayState> cloneArrayState(ref_ptr<ArrayState> arrayState) override
    {
        auto clone = GpuLayoutTechniqueArrayState::create(*arrayState);
        clone->technique = technique;
        clone->text = text;
        clone->billboard = billboard;
        return clone;
    }

    // 获取顶点数组（支持GPU布局和广告牌效果）
    // 根据实例索引计算字形位置，并应用广告牌变换
    // instanceIndex: 实例索引，表示当前处理的字形在文本中的位置
    // 返回值：计算后的顶点数组
    ref_ptr<const vec3Array> vertexArray(uint32_t instanceIndex) override
    {
        // 计算当前字形的位置（累积前面所有字形的水平/垂直偏移）
        float horiAdvance = 0.0;
        float vertAdvance = 0.0;
        for (uint32_t i = 0; i < instanceIndex; ++i)
        {
            uint32_t glyph_index = technique->textArray->at(i);
            if (glyph_index == 0)
            {
                // 将索引0视为换行符
                vertAdvance -= 1.0;
                horiAdvance = 0.0;
            }
            else
            {
                // 累加前面字形的水平前进距离
                const GlyphMetrics& glyph_metrics = text->font->glyphMetrics->at(glyph_index);
                horiAdvance += glyph_metrics.horiAdvance;
            }
        }

        // 获取当前字形的索引和度量
        uint32_t glyph_index = technique->textArray->at(instanceIndex);
        const GlyphMetrics& glyph_metrics = text->font->glyphMetrics->at(glyph_index);

        // 计算广告牌效果
        auto textLayout = technique->layoutValue->value();
        dmat4 transform_to_local;
        if (billboard && !localToWorldStack.empty() && !worldToLocalStack.empty())
        {
            // 计算广告牌变换矩阵，使文本始终面向相机
            const dmat4& mv = localToWorldStack.back();
            const dmat4& inverse_mv = worldToLocalStack.back();
            dvec3 center_eye = mv * dvec3(textLayout.position);
            dmat4 billboard_mv = computeBillboardMatrix(center_eye, (double)textLayout.billboardAutoScaleDistance);
            transform_to_local = inverse_mv * billboard_mv;
        }
        else
        {
            // 如果没有广告牌效果，只进行平移
            transform_to_local = vsg::translate(textLayout.position);
        }

        // 计算6个顶点的位置（每个字形由两个三角形组成，共6个顶点）
        auto new_vertices = vsg::vec3Array::create(6);
        auto src_vertex_itr = vertices->begin();
        for (auto& v : *new_vertices)
        {
            const auto& sv = *(src_vertex_itr++);

            // 计算顶点的位置（基于字形度量、布局和对齐）
            vec3 pos = textLayout.horizontal * (horiAdvance + textLayout.horizontalAlignment + glyph_metrics.horiBearingX + sv.x * glyph_metrics.width) +
                       textLayout.vertical * (vertAdvance + textLayout.verticalAlignment + glyph_metrics.horiBearingY + (sv.y - 1.f) * glyph_metrics.height);

            // 应用变换到顶点
            v = vec3(transform_to_local * dvec3(pos));
        }

        return new_vertices;
    }

    const GpuLayoutTechnique* technique = nullptr;  // 指向GPU布局技术的指针
    const Text* text = nullptr;  // 指向文本对象的指针
    bool billboard = false;  // 是否需要广告牌效果
};

// 赋值辅助函数
// 如果源值和目标值不同，则更新目标值并设置更新标志
// dest: 目标值
// src: 源值
// updated: 更新标志
template<typename T>
void assignValue(T& dest, const T& src, bool& updated)
{
    if (dest == src) return;
    dest = src;
    updated = true;
}

// 设置单个文本对象的GPU布局技术
// 为文本对象创建渲染子图，使用GPU进行文本布局计算
// text: 要设置的文本对象
// minimumAllocation: 最小分配大小
// options: 选项对象
void GpuLayoutTechnique::setup(Text* text, uint32_t minimumAllocation, ref_ptr<const Options> options)
{
    // 检查必要的对象是否存在
    if (!text || !(text->text) || !text->font || !text->layout) return;

    auto& layout = text->layout;

    // 计算文本的边界范围
    textExtents = layout->extents(text->text, *(text->font));

    // 跟踪布局和文本数组是否更新
    bool textLayoutUpdated = false;
    bool textArrayUpdated = false;

    struct ConvertString : public ConstVisitor
    {
        Font& font;
        ref_ptr<uintArray>& textArray;
        bool& updated;
        uint32_t minimumSize = 0;
        uint32_t allocatedSize = 0;
        uint32_t size = 0;

        ConvertString(Font& in_font, ref_ptr<uintArray>& in_textArray, bool& in_updated, uint32_t in_minimumSize) :
            font(in_font),
            textArray(in_textArray),
            updated(in_updated),
            minimumSize(in_minimumSize) {}

        void allocate(uint32_t requiredSize)
        {
            size = requiredSize;

            if (textArray && requiredSize < static_cast<uint32_t>(textArray->valueCount()))
            {
                allocatedSize = static_cast<uint32_t>(textArray->valueCount());
                textArray->dirty();
                return;
            }

            allocatedSize = std::max(requiredSize, minimumSize);
            textArray = uintArray::create(allocatedSize, 0u);
            textArray->properties.dataVariance = DYNAMIC_DATA;

            updated = true;
        }

        void apply(const stringValue& text) override
        {
            allocate(static_cast<uint32_t>(text.value().size()));

            auto itr = textArray->begin();
            for (auto& c : text.value())
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(uint32_t(c)), updated);
            }
        }
        void apply(const wstringValue& text) override
        {
            allocate(static_cast<uint32_t>(text.value().size()));

            auto itr = textArray->begin();
            for (auto& c : text.value())
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(uint32_t(c)), updated);
            }
        }
        void apply(const ubyteArray& text) override
        {
            allocate(static_cast<uint32_t>(text.valueCount()));

            auto itr = textArray->begin();
            for (const auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
        void apply(const ushortArray& text) override
        {
            allocate(static_cast<uint32_t>(text.valueCount()));

            auto itr = textArray->begin();
            for (const auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
        void apply(const uintArray& text) override
        {
            allocate(static_cast<uint32_t>(text.valueCount()));

            auto itr = textArray->begin();
            for (const auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
    };

    ConvertString converter(*(text->font), textArray, textArrayUpdated, minimumAllocation);
    text->text->accept(converter);

    if (converter.allocatedSize == 0) return;

    uint32_t num_quads = converter.size;

    // set up the layout data in a form digestible by the GPU.
    if (!layoutValue)
    {
        layoutValue = TextLayoutValue::create();
        layoutValue->properties.dataVariance = DYNAMIC_DATA;
    }

    bool billboard = false;
    auto& layoutStruct = layoutValue->value();
    if (auto standardLayout = layout.cast<StandardLayout>(); standardLayout)
    {
        assignValue(layoutStruct.position, standardLayout->position, textLayoutUpdated);
        assignValue(layoutStruct.horizontal, standardLayout->horizontal, textLayoutUpdated);
        assignValue(layoutStruct.vertical, standardLayout->vertical, textLayoutUpdated);
        assignValue(layoutStruct.color, standardLayout->color, textLayoutUpdated);
        assignValue(layoutStruct.outlineColor, standardLayout->outlineColor, textLayoutUpdated);
        assignValue(layoutStruct.outlineWidth, standardLayout->outlineWidth, textLayoutUpdated);

        billboard = standardLayout->billboard;
        assignValue(layoutStruct.billboardAutoScaleDistance, standardLayout->billboardAutoScaleDistance, textLayoutUpdated);

        layoutValue->dirty();
    }

    // assign alignment offset
    auto alignment = layout->alignment(text->text, *(text->font));
    assignValue(layoutStruct.horizontalAlignment, alignment.x, textLayoutUpdated);
    assignValue(layoutStruct.verticalAlignment, alignment.y, textLayoutUpdated);

    if (!vertices)
    {
        vertices = vec3Array::create(6);

        float leadingEdgeGradient = 0.1f;

        vertices->set(0, vec3(0.0f, 1.0f, 2.0f * leadingEdgeGradient));
        vertices->set(1, vec3(0.0f, 0.0f, leadingEdgeGradient));
        vertices->set(2, vec3(1.0f, 1.0f, leadingEdgeGradient));

        vertices->set(3, vec3(0.0f, 0.0f, leadingEdgeGradient));
        vertices->set(4, vec3(1.0f, 0.0f, 0.0f));
        vertices->set(5, vec3(1.0f, 1.0f, leadingEdgeGradient));
    }

    if (!draw)
        draw = Draw::create(6, num_quads, 0, 0);
    else
        draw->instanceCount = num_quads;

    ref_ptr<StateGroup> stateGroup = scenegraph.cast<StateGroup>();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    if (!stateGroup)
    {
        stateGroup = StateGroup::create();
        scenegraph = stateGroup;

        auto shaderSet = text->shaderSet ? text->shaderSet : createTextShaderSet(options);

        auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);

        auto& sharedObjects = text->font->sharedObjects;
        if (!sharedObjects) sharedObjects = SharedObjects::create();

        DataList arrays;
        config->assignArray(arrays, "inPosition", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

        if (billboard)
        {
            config->shaderHints->defines.insert("BILLBOARD");
        }
        if (!text->font->atlasImageInfo)
        {
            text->font->createFontImages();
        }
        config->assignTexture("textureAtlas", {text->font->atlasImageInfo}, 0);
        config->assignTexture("glyphMetrics", {text->font->glyphImageInfo}, 0);

        config->assignDescriptor("textLayout", layoutValue);
        config->assignDescriptor("text", textArray);

        if (sharedObjects)
            sharedObjects->share(config, [](auto gpc) { gpc->init(); });
        else
            config->init();

        config->copyTo(stateGroup, sharedObjects);

        bindVertexBuffers = BindVertexBuffers::create(0, arrays);

        // setup geometry
        auto drawCommands = Commands::create();
        drawCommands->addChild(bindVertexBuffers);
        drawCommands->addChild(draw);
        stateGroup->addChild(drawCommands);

        // Assign ArrayState for CPU mapping of vertices
        stateGroup->prototypeArrayState = GpuLayoutTechniqueArrayState::create(this, text, billboard);
    }
    else
    {
    }
}
// 设置文本组的GPU布局技术
// 注意：目前还不支持文本组
// textGroup: 要设置的文本组
// minimumAllocation: 最小分配大小
// options: 选项对象
void GpuLayoutTechnique::setup(TextGroup* textGroup, uint32_t minimumAllocation, ref_ptr<const Options> options)
{
    info("GpuLayoutTechnique::setup(", textGroup, ", ", minimumAllocation, options, ") not yet supported");
}
