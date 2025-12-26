/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/commands/PipelineBarrier.h>
#include <vsg/core/compare.h>
#include <vsg/io/Logger.h>
#include <vsg/io/write.h>
#include <vsg/lighting/AmbientLight.h>
#include <vsg/lighting/DirectionalLight.h>
#include <vsg/lighting/HardShadows.h>
#include <vsg/lighting/PercentageCloserSoftShadows.h>
#include <vsg/lighting/PointLight.h>
#include <vsg/lighting/SoftShadows.h>
#include <vsg/lighting/SpotLight.h>
#include <vsg/nodes/RegionOfInterest.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/vk/Context.h>

using namespace vsg;

//////////////////////////////////////
//
// TraverseChildrenOfNode - 遍历节点子节点的辅助节点类
//
namespace vsg
{
    // 辅助节点类：用于遍历指定节点的子节点
    // 用于阴影贴图渲染，允许从视图节点遍历场景图
    class TraverseChildrenOfNode : public Inherit<Node, TraverseChildrenOfNode>
    {
    public:
        // 构造函数：创建遍历节点子节点的辅助节点
        // in_node: 要遍历其子节点的节点
        explicit TraverseChildrenOfNode(Node* in_node) :
            node(in_node) {}

        observer_ptr<Node> node;

        // 模板遍历方法：遍历关联节点的子节点
        // N: 节点类型
        // V: 访问者类型
        template<class N, class V>
        static void t_traverse(N& in_node, V& visitor)
        {
            if (auto ref_node = in_node.node.ref_ptr()) ref_node->traverse(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
    };
    VSG_type_name(TraverseChildrenOfNode);

    // 计算级联阴影贴图的实用分割距离
    // n: 近平面距离
    // f: 远平面距离
    // i: 当前级联索引
    // m: 总级联数量
    // lambda: 混合因子（0=均匀分割，1=对数分割）
    // 返回: 分割距离
    // 使用对数分割和均匀分割的混合，提供更好的阴影质量
    inline double Cpractical(double n, double f, double i, double m, double lambda)
    {
        double Clog = n * std::pow((f / n), (i / m));
        double Cuniform = n + (f - n) * (i / m);
        return Clog * lambda + Cuniform * (1.0 - lambda);
    };

} // namespace vsg

//////////////////////////////////////
//
// ViewDescriptorSetLayout - 视图描述符集布局，从视图相关状态获取描述符集布局
//
// 构造函数：创建视图描述符集布局对象（默认）
// 视图描述符集布局用于延迟获取视图相关状态的描述符集布局
ViewDescriptorSetLayout::ViewDescriptorSetLayout()
{
}

// 比较两个视图描述符集布局对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较视图描述符集布局指针
int ViewDescriptorSetLayout::compare(const Object& rhs_object) const
{
    int result = DescriptorSetLayout::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer(_viewDescriptorSetLayout, rhs._viewDescriptorSetLayout);
}

// 从输入流读取视图描述符集布局对象
// input: 输入流对象
void ViewDescriptorSetLayout::read(Input& input)
{
    Object::read(input);
}

// 将视图描述符集布局对象写入输出流
// output: 输出流对象
void ViewDescriptorSetLayout::write(Output& output) const
{
    Object::write(output);
}

// 编译视图描述符集布局
// context: 编译上下文对象
// 从视图相关状态获取描述符集布局并编译它
void ViewDescriptorSetLayout::compile(Context& context)
{
    if (!_viewDescriptorSetLayout && context.viewDependentState && context.viewDependentState->descriptorSetLayout)
    {
        _viewDescriptorSetLayout = context.viewDependentState->descriptorSetLayout;
        _viewDescriptorSetLayout->compile(context);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BindViewDescriptorSets - 绑定视图描述符集的状态命令
//
// 构造函数：创建绑定视图描述符集对象
// 槽位2：在绑定管线之后执行
// 用于绑定视图相关状态的描述符集（光源数据、阴影贴图等）
BindViewDescriptorSets::BindViewDescriptorSets() :
    Inherit(2), // slot 2
    pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
    firstSet(0)
{
}

// 比较两个绑定视图描述符集对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、管线绑定点、布局和第一个集合索引
int BindViewDescriptorSets::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(pipelineBindPoint, rhs.pipelineBindPoint))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    return compare_value(firstSet, rhs.firstSet);
}

// 从输入流读取绑定视图描述符集对象
// input: 输入流对象
// 读取管线绑定点、布局和第一个集合索引
void BindViewDescriptorSets::read(Input& input)
{
    StateCommand::read(input);

    input.readValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    input.read("layout", layout);
    input.read("firstSet", firstSet);
}

// 将绑定视图描述符集对象写入输出流
// output: 输出流对象
// 写入管线绑定点、布局和第一个集合索引
void BindViewDescriptorSets::write(Output& output) const
{
    StateCommand::write(output);

    output.writeValue<uint32_t>("pipelineBindPoint", pipelineBindPoint);
    output.write("layout", layout);
    output.write("firstSet", firstSet);
}

// 编译绑定视图描述符集
// context: 编译上下文对象
// 编译管线布局和视图相关状态
void BindViewDescriptorSets::compile(Context& context)
{
    layout->compile(context);
    if (context.viewDependentState) context.viewDependentState->compile(context);
}

// 记录绑定视图描述符集命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 绑定视图相关状态的描述符集到管线
void BindViewDescriptorSets::record(CommandBuffer& commandBuffer) const
{
    if (commandBuffer.viewDependentState)
    {
        commandBuffer.viewDependentState->bindDescriptorSets(commandBuffer, pipelineBindPoint, layout->vk(commandBuffer.deviceID), firstSet);
    }
}

//////////////////////////////////////
//
// ViewDependentState - 视图相关状态，管理视图相关的渲染状态（光源、阴影贴图等）
//
// 构造函数：使用视图创建视图相关状态对象
// in_view: 关联的视图对象
// 视图相关状态用于管理每个视图的特定渲染状态，包括光源数据、阴影贴图、视口数据等
ViewDependentState::ViewDependentState(View* in_view) :
    view(in_view)
{
    // info("ViewDependentState::ViewDependentState(view = ", view, ")");
}

// 析构函数：销毁视图相关状态对象
ViewDependentState::~ViewDependentState()
{
}

// 创建阴影贴图图像
// width: 图像宽度
// height: 图像高度
// levels: 数组层数（用于级联阴影贴图）
// format: 图像格式（通常为深度格式）
// usage: 图像使用标志
// 返回: 阴影贴图图像对象
// 创建用于存储阴影贴图的2D数组图像
ref_ptr<Image> createShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage)
{
    auto image = Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return image;
}

// 获取光源的活动阴影设置
// light: 光源对象
// 返回: 阴影设置对象
// 首先查找精确匹配的阴影设置覆盖，如果没有则查找空键的覆盖（用于所有未匹配的光源），最后返回光源的默认阴影设置
ref_ptr<ShadowSettings> ViewDependentState::getActiveShadowSettings(const Light* light) const
{
    // 查找精确匹配
    auto itr = shadowSettingsOverride.find(ref_ptr<const Light>(light));
    if (itr != shadowSettingsOverride.end())
    {
        return itr->second;
    }

    // 如果存在空键条目，则使用它来覆盖所有未匹配的光源
    itr = shadowSettingsOverride.find({});
    if (itr != shadowSettingsOverride.end())
    {
        return itr->second;
    }

    return light->shadowSettings;
}

// 初始化视图相关状态
// requirements: 资源需求对象
// 初始化光源数据缓冲区、视口数据缓冲区、阴影贴图图像和描述符集
// 根据视图特征和资源需求确定最大光源数量、阴影贴图数量和大小
void ViewDependentState::init(ResourceRequirements& requirements)
{
    // 检查视图相关状态是否已经初始化
    if (lightData) return;

    // 如果没有着色器集，使用标准PBR着色器集
    if (!shaderSet)
    {
        // 回退到使用标准PBR着色器集
        shaderSet = vsg::createPhysicsBasedRenderingShaderSet();
    }

    auto descriptorConfigurator = DescriptorConfigurator::create(shaderSet);

    // 默认值
    uint32_t maxNumberLights = 64;
    uint32_t maxViewports = 1;

    uint32_t shadowWidth = 2048;
    uint32_t shadowHeight = 2048;
    uint32_t maxShadowMaps = 8;

    const auto& viewDetails = requirements.views[view];

    // 如果视图需要记录光源或阴影贴图
    if ((view->features & (RECORD_LIGHTS | RECORD_SHADOW_MAPS)) != 0)
    {
        // 计算实际光源数量和阴影贴图数量
        uint32_t numLights = static_cast<uint32_t>(viewDetails.lights.size());
        uint32_t numShadowMaps = 0;
        for (auto& light : viewDetails.lights)
        {
            if (auto shadowSettings = getActiveShadowSettings(light))
            {
                numShadowMaps += shadowSettings->shadowMapCount;
            }
        }

        // 根据资源需求范围调整最大光源数量
        if (numLights < requirements.numLightsRange[0])
            maxNumberLights = requirements.numLightsRange[0];
        else if (numLights > requirements.numLightsRange[1])
            maxNumberLights = requirements.numLightsRange[1];
        else
            maxNumberLights = numLights;

        // 根据资源需求范围调整最大阴影贴图数量
        if (numShadowMaps < requirements.numShadowMapsRange[0])
            maxShadowMaps = requirements.numShadowMapsRange[0];
        else if (numShadowMaps > requirements.numShadowMapsRange[1])
            maxShadowMaps = requirements.numShadowMapsRange[1];
        else
            maxShadowMaps = numShadowMaps;

        // 从资源需求获取阴影贴图大小
        shadowWidth = requirements.shadowMapSize.x;
        shadowHeight = requirements.shadowMapSize.y;
    }
    else
    {
        maxNumberLights = 0;
        maxShadowMaps = 0;
    }

    // 计算光源数据大小：1个vec4用于指定光源数量，最大条目是聚光灯（每个光源4个vec4），每个阴影贴图需要8个vec4
    uint32_t lightDataSize = 1 + maxNumberLights * 4 + maxShadowMaps * 8;

#if 0
    if (active)
    {
        info("void ViewDependentState::init(ResourceRequirements& requirements) view = ", view, ", active = ", active);
        info("    viewDetails.indices.size() = ", viewDetails.indices.size());
        info("    viewDetails.bins.size() = ", viewDetails.bins.size());
        info("    viewDetails.lights.size() = ", viewDetails.lights.size());
        info("    maxViewports = ", maxViewports);
        info("    maxNumberLights = ", maxNumberLights);
        info("    maxShadowMaps = ", maxShadowMaps);
        info("    lightDataSize = ", lightDataSize);
        info("    shadowWidth = ", shadowWidth);
        info("    shadowHeight = ", shadowHeight);
        info("    requirements.numLightsRange = ", requirements.numLightsRange);
        info("    requirements.numShadowMapsRange = ", requirements.numShadowMapsRange);
        info("    requirements.shadowMapSize = ", requirements.shadowMapSize);
    }
#endif

    lightData = vec4Array::create(lightDataSize);
    lightData->setValue("name", "lightData");
    lightData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    lightDataBufferInfo = BufferInfo::create(lightData.get());
    descriptorConfigurator->assignDescriptor("lightData", BufferInfoList{lightDataBufferInfo});

    viewportData = vec4Array::create(maxViewports);
    viewportData->setValue("name", "viewportData");
    viewportData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewportDataBufferInfo = BufferInfo::create(viewportData.get());
    descriptorConfigurator->assignDescriptor("viewportData", BufferInfoList{viewportDataBufferInfo});

    // set up ShadowMaps
    auto shadowMapDirectSampler = Sampler::create();
    shadowMapDirectSampler->minFilter = VK_FILTER_NEAREST;
    shadowMapDirectSampler->magFilter = VK_FILTER_NEAREST;
    shadowMapDirectSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapDirectSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapDirectSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapDirectSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    auto shadowMapSampler = Sampler::create();
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#define HARDWARE_PCF 1
#if HARDWARE_PCF == 1
    shadowMapSampler->minFilter = VK_FILTER_LINEAR;
    shadowMapSampler->magFilter = VK_FILTER_LINEAR;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_LESS;
#else
    shadowMapSampler->minFilter = VK_FILTER_NEAREST;
    shadowMapSampler->magFilter = VK_FILTER_NEAREST;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
#endif
    if (maxShadowMaps > 0)
    {
        shadowDepthImage = createShadowImage(shadowWidth, shadowHeight, maxShadowMaps, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = maxShadowMaps;

        auto depthImageInfo = ImageInfo::create(nullptr, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

        descriptorConfigurator->assignTexture("shadowMaps", ImageInfoList{depthImageInfo});
    }
    else
    {
        //
        // fallback to provide a descriptor image to use when the ViewDependentState shadow map generation is not active
        //
        Data::Properties properties;
        properties.format = VK_FORMAT_D32_SFLOAT;
        properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

        auto shadowMapData = floatArray3D::create(1, 1, 1, 0.0f, properties);
        shadowDepthImage = Image::create(shadowMapData);
        shadowDepthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = 1;

        auto depthImageInfo = ImageInfo::create(nullptr, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

        descriptorConfigurator->assignTexture("shadowMaps", ImageInfoList{depthImageInfo});
    }

    auto shadowMapDirectSamplerInfo = ImageInfo::create(shadowMapDirectSampler, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto shadowMapSamplerInfo = ImageInfo::create(shadowMapSampler, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    descriptorConfigurator->assignTexture("shadowMapDirectSampler", ImageInfoList{shadowMapDirectSamplerInfo});
    descriptorConfigurator->assignTexture("shadowMapShadowSampler", ImageInfoList{shadowMapSamplerInfo});

    // assign the DescriptorSet and layout created by the descriptorConfigurator
    for (size_t set = 0; set < descriptorConfigurator->descriptorSets.size(); ++set)
    {
        if (auto ds = descriptorConfigurator->descriptorSets[set])
        {
            descriptorSet = ds;
            descriptorSetLayout = ds->setLayout;
        }
    }

    // 如果不活动则不启用阴影贴图
    if (maxShadowMaps == 0) return;

    // 创建开关以切换每个阴影贴图层级的渲染到纹理子图
    preRenderSwitch = Switch::create();

    preRenderCommandGraph = CommandGraph::create();
    preRenderCommandGraph->submitOrder = -1; // 在主渲染之前执行
    preRenderCommandGraph->addChild(preRenderSwitch);

    // 创建遍历视图子节点的辅助节点
    auto tcon = TraverseChildrenOfNode::create(view);

    Mask shadowMask = 0x1; // TODO: 我们是否从主场景继承？如何继承？

    // 创建阴影贴图视口状态
    auto viewportState = ViewportState::create(VkExtent2D{shadowWidth, shadowHeight});

    // 为每个阴影贴图创建视图和渲染图
    ref_ptr<View> first_view;
    shadowMaps.resize(maxShadowMaps);
    for (auto& shadowMap : shadowMaps)
    {
        // 第一个视图使用继承视点特征创建，后续视图从第一个视图复制
        if (first_view)
        {
            shadowMap.view = View::create(*first_view);
        }
        else
        {
            first_view = View::create(ViewFeatures(INHERIT_VIEWPOINT));
            shadowMap.view = first_view;
        }

        shadowMap.view->mask = shadowMask;
        shadowMap.view->camera = Camera::create();
        shadowMap.view->addChild(tcon);
        shadowMap.view->camera->viewportState = viewportState;

        // 创建阴影贴图渲染图
        shadowMap.renderGraph = RenderGraph::create();
        shadowMap.renderGraph->addChild(shadowMap.view);
        preRenderSwitch->addChild(MASK_ALL, shadowMap.renderGraph);
    }
}

// 更新视图相关状态
// requirements: 资源需求对象
// 更新预渲染命令图的最大槽位
void ViewDependentState::update(ResourceRequirements& requirements)
{
    if (preRenderCommandGraph)
    {
        preRenderCommandGraph->maxSlots.merge(requirements.maxSlots);
    }
}

// 编译视图相关状态
// context: 编译上下文对象
// 编译描述符集和阴影贴图渲染资源（渲染通道、帧缓冲区等）
void ViewDependentState::compile(Context& context)
{
    if (compiled) return;
    compiled = true;

    CPU_INSTRUMENTATION_L1_NC(context.instrumentation, "ViewDependentState compile", COLOR_COMPILE);

    // 编译描述符集
    descriptorSet->compile(context);

    // 如果视图需要记录阴影贴图且存在预渲染命令图，编译阴影贴图渲染资源
    if ((view->features & RECORD_SHADOW_MAPS) != 0 && preRenderCommandGraph && !preRenderCommandGraph->device)
    {
        preRenderCommandGraph->device = context.device;

        // TODO
        preRenderCommandGraph->queueFamily = 0;

        auto extent = shadowDepthImage->extent;

        // 编译阴影深度图像
        shadowDepthImage->compile(context);

        // 为每个阴影贴图创建渲染通道和帧缓冲区
        uint32_t layer = 0;
        for (const auto& shadowMap : shadowMaps)
        {
            // 创建深度缓冲区视图（每个阴影贴图使用数组的一个层）
            auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
            depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            depthImageView->subresourceRange.baseMipLevel = 0;
            depthImageView->subresourceRange.levelCount = 1;
            depthImageView->subresourceRange.baseArrayLayer = layer;
            depthImageView->subresourceRange.layerCount = 1;
            depthImageView->compile(context);

            // 附件描述
            RenderPass::Attachments attachments(1);
            // 深度附件
            attachments[0].format = shadowDepthImage->format;
            attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            // 子通道描述（仅使用深度附件，无颜色附件）
            AttachmentReference ignoreColorReference = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
            AttachmentReference depthReference = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            RenderPass::Subpasses subpassDescription(1);
            subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescription[0].colorAttachments.emplace_back(ignoreColorReference);
            subpassDescription[0].depthStencilAttachments.emplace_back(depthReference);

            // 子通道依赖（确保阴影贴图写入和读取之间的同步）
            RenderPass::Dependencies dependencies(2);

            // 外部到子通道0的依赖（从片段着色器读取到早期深度测试写入）
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            // 子通道0到外部的依赖（从深度测试写入到片段着色器读取）
            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            // 创建渲染通道
            auto renderPass = RenderPass::create(context.device, attachments, subpassDescription, dependencies);

            // 创建帧缓冲区
            auto fbuf = Framebuffer::create(renderPass, ImageViews{depthImageView}, extent.width, extent.height, 1);

            // 配置渲染图
            auto rendergraph = shadowMap.renderGraph;
            rendergraph->renderArea.offset = VkOffset2D{0, 0};
            rendergraph->renderArea.extent = VkExtent2D{extent.width, extent.height};
            rendergraph->framebuffer = fbuf;

            // 设置清除值（深度清除为0.0）
            rendergraph->clearValues.resize(1);
            rendergraph->clearValues[0].depthStencil = VkClearDepthStencilValue{0.0f, 0};

            ++layer;
        }

        // 使用图像屏障将初始阴影贴图数组布局转换为VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        // 这样即使只有部分阴影贴图通过渲染到纹理通道设置，整个阴影贴图也可以在片段着色器中使用
        auto initLayoutBarrier = ImageMemoryBarrier::create(
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            shadowDepthImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, static_cast<uint32_t>(shadowMaps.size())});

        auto pipelinBarrier = PipelineBarrier::create(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, initLayoutBarrier);

        context.commands.push_back(pipelinBarrier);
    }
}

// 清除视图相关状态数据
// 清空所有光源列表（环境光、方向光、点光源、聚光灯）
void ViewDependentState::clear()
{
    //debug("ViewDependentState::clear() bufferIndex = ", bufferIndex);

    // 清除数据
    ambientLights.clear();
    directionalLights.clear();
    pointLights.clear();
    spotLights.clear();
}

// 遍历视图相关状态（记录遍历）
// rt: 记录遍历对象
// 更新光源数据，计算阴影贴图相机参数，并执行阴影贴图预渲染
// 这是视图相关状态的核心方法，负责每帧更新光源信息和生成阴影贴图
void ViewDependentState::traverse(RecordTraversal& rt) const
{
    //GPU_INSTRUMENTATION_L1_NC(rt.instrumentation, *rt.getCommandBuffer(), "ViewDependentState", COLOR_RECORD_L1);
    CPU_INSTRUMENTATION_L1_NC(rt.instrumentation, "ViewDependentState", COLOR_RECORD_L1);

    // 如果视图不需要记录阴影贴图，直接返回
    if ((view->features & RECORD_SHADOW_MAPS) == 0) return;

    // useful reference : https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
    // PCF filtering : https://github.com/SaschaWillems/Vulkan/issues/231
    // sampler2DArrayShadow
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineDepthStencilStateCreateInfo.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdSetDepthBoundsTestEnable.html
    //
    // Game industry SIGGRAPH presentation
    // https://www.realtimeshadows.com/sites/default/files/Playing%20with%20Real-Time%20Shadows_0.pdf
    //
    // Soft shadows:
    // https://ogldev.org/www/tutorial42/tutorial42.html
    // https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf
    // https://andrew-pham.blog/2019/08/03/percentage-closer-soft-shadows/
    // https://github.com/vsgopenmw-dev/vsgopenmw/blob/master/files/shaders/lib/view/shadow.glsl

    bool requiresPerRenderShadowMaps = false;
    uint32_t shadowMapIndex = 0;
    uint32_t numShadowMaps = static_cast<uint32_t>(shadowMaps.size());
    if (preRenderSwitch)
        preRenderSwitch->setAllChildren(false);
    else
        numShadowMaps = 0;

    // Lambda函数：计算视锥体边界（未裁剪）
    // n: 近平面Z值（裁剪空间）
    // f: 远平面Z值（裁剪空间）
    // clipToWorld: 从裁剪空间到世界空间的变换矩阵
    // 返回: 世界空间中的边界框
    // 将视锥体的8个角点从裁剪空间变换到世界空间
    auto computeFrustumBounds = [&](double n, double f, const dmat4& clipToWorld) -> dbox {
        dbox bounds;
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, f));

        return bounds;
    };

    // Lambda函数：计算视锥体边界（裁剪近平面）
    // n: 近平面Z值（裁剪空间）
    // f: 远平面Z值（裁剪空间）
    // clipToWorld: 从裁剪空间到世界空间的变换矩阵
    // 返回: 世界空间中的边界框（裁剪近平面后）
    // 在齐次坐标和笛卡尔坐标之间转换时，内部线段（两点之间的线段部分）可能变成外部线段（线段除了两点之间的部分）。
    // 特别是，对于穿过透视投影近平面的线段会发生这种情况。因此，此函数排除视锥体在近平面错误一侧的部分，
    // 避免问题，并避免为无限外部线段给出无限边界。
    auto computeFrustumBoundsClipped = [&](double n, double f, const dmat4& clipToWorld) -> dbox {
        std::array<dvec4, 8> corners{{
            clipToWorld * dvec4(-1.0, -1.0, n, 1.0),
            clipToWorld * dvec4(-1.0, 1.0, n, 1.0),
            clipToWorld * dvec4(1.0, -1.0, n, 1.0),
            clipToWorld * dvec4(1.0, 1.0, n, 1.0),
            clipToWorld * dvec4(-1.0, -1.0, f, 1.0),
            clipToWorld * dvec4(-1.0, 1.0, f, 1.0),
            clipToWorld * dvec4(1.0, -1.0, f, 1.0),
            clipToWorld * dvec4(1.0, 1.0, f, 1.0),
        }};
        std::array<std::pair<int, int>, 12> edges{{
            {0, 1},
            {1, 3},
            {3, 2},
            {2, 0},
            {0, 4},
            {1, 5},
            {2, 6},
            {3, 7},
            {4, 5},
            {5, 7},
            {7, 6},
            {6, 4},
        }};

        dbox bounds;

        std::array<bool, 8> clipped;
        for (unsigned int i = 0; i < corners.size(); ++i)
        {
            clipped[i] = corners[i].w < corners[i].z;
            if (!clipped[i])
                bounds.add(corners[i].xyz / corners[i].w);
        }
        for (const auto& [start, end] : edges)
        {
            if (clipped[start] != clipped[end])
            {
                // add the point where z=w on the line
                const auto& p1 = corners[start];
                const auto& p2 = corners[end];
                double a = (p1.z - p1.w) / (p1.z - p2.z - p1.w + p2.w);
                dvec4 p = p1 * (1.0 - a) + p2 * a;
                bounds.add(p.xyz / p.w);
            }
        }
        return bounds;
    };

    // info("\n\nViewDependentState::traverse(", &rt, ", ", &view, ") numShadowMaps = ", numShadowMaps);

    // 缓存通用视图参数
    auto projectionMatrix = view->camera->projectionMatrix->transform();
    auto viewMatrix = view->camera->viewMatrix->transform();
    auto inverse_viewMatrix = inverse(viewMatrix);
    // 计算视图方向和上方向（在世界空间中）
    auto view_direction = normalize(dvec3(0.0, 0.0, -1.0) * (projectionMatrix * viewMatrix));
    auto view_up = normalize(dvec3(0.0, -1.0, 0.0) * (projectionMatrix * viewMatrix));

    // 从投影矩阵提取近远平面距离
    auto clipToEye = inverse(projectionMatrix);
    auto n = -(clipToEye * dvec3(0.0, 0.0, 1.0)).z;
    auto f = -(clipToEye * dvec3(0.0, 0.0, 0.0)).z;

    // 如果场景图中找到了感兴趣区域，使用它们来限制近远平面值
    if (!rt.regionsOfInterest.empty())
    {
        dbox eyeSpaceRegionBounds;
        for (auto& [mv, regionOfInterest] : rt.regionsOfInterest)
        {
            for (auto& v : regionOfInterest->points)
            {
                eyeSpaceRegionBounds.add(mv * v);
            }
        }

        if (eyeSpaceRegionBounds)
        {
            double regionNear = -eyeSpaceRegionBounds.max.z;
            double regionFar = -eyeSpaceRegionBounds.min.z;
            if (regionNear > n) { n = regionNear; }
            if (regionFar < f) { f = regionFar; }
        }
    }

    // 设置光源数据
    auto light_itr = lightData->begin();
    uint32_t numLightDataChanges = 0;

    // Lambda函数：分配光源数据（vec4）
    // value: 要分配的值
    // 如果值已更改，标记数据为脏
    auto assignLightData = [&](const vec4& value) -> void {
        if (*light_itr != value)
        {
            *light_itr = value;
            ++numLightDataChanges;
        }
        ++light_itr;
    };

    // Lambda函数：分配光源数据（4个浮点数）
    // x, y, z, w: 要分配的4个浮点数值
    auto assignLightData4 = [&](float x, float y, float z, float w) -> void {
        vec4 value(x, y, z, w);
        if (*light_itr != value)
        {
            *light_itr = value;
            ++numLightDataChanges;
        }
        ++light_itr;
    };

    // 首先写入光源数量（环境光、方向光、点光源、聚光灯）
    assignLightData4(static_cast<float>(ambientLights.size()),
                     static_cast<float>(directionalLights.size()),
                     static_cast<float>(pointLights.size()),
                     static_cast<float>(spotLights.size()));

    // 光源数据要求 = vec4 * (环境光数量 + 3 * 方向光数量 + 3 * 点光源数量 + 4 * 聚光灯数量 + 4 * 阴影贴图数量)

    for (const auto& entry : ambientLights)
    {
        auto light = entry.second;
        assignLightData4(light->color.r, light->color.g, light->color.b, light->intensity);
    }

    for (auto& [mv, light] : directionalLights)
    {
        // info("   light ", light->className(), ", light->shadowMapCount = ", light->shadowMapCount);

        // assign basic direction light settings to light data
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        assignLightData4(light->color.r, light->color.g, light->color.b, light->intensity);
        assignLightData4(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), 0.0f);

        auto shadowSettings = getActiveShadowSettings(light);
        uint32_t activeNumShadowMaps = shadowSettings ? std::min(shadowSettings->shadowMapCount, numShadowMaps - shadowMapIndex) : 0;
        if (shadowSettings)
        {
            if (shadowSettings->type_info() == typeid(HardShadows))
            {
                assignLightData4(static_cast<float>(activeNumShadowMaps), -1.0f, -1.0f, 0.0f);
            }
            else if (shadowSettings->type_info() == typeid(SoftShadows))
            {
                const SoftShadows& pcfShadowSettings = static_cast<const SoftShadows&>(*shadowSettings);
                assignLightData4(static_cast<float>(activeNumShadowMaps), pcfShadowSettings.penumbraRadius, -1.0f, 0.0f);
            }
            else if (shadowSettings->type_info() == typeid(PercentageCloserSoftShadows))
            {
                assignLightData4(static_cast<float>(activeNumShadowMaps), 0.1f /* todo: calculate blocker search radius */, std::tan(light->angleSubtended / 2), 0.0f);
            }
        }
        else
            assignLightData4(0.0f, 0.0f, 0.0f, 0.0f);

        if (activeNumShadowMaps == 0) continue;

        // set up shadow map rendering backend
        requiresPerRenderShadowMaps = true;

        // compute directional light space
        // light direction in world coords
        auto light_direction = normalize(light->direction * (inverse_3x3(mv * inverse_viewMatrix)));
#if 0
        info("   directional light : light direction in world = ", light_direction, ", light->shadowMapCount = ", light->shadowMapCount);
        info("      light->direction in model = ", light->direction);
        info("      view_direction in world = ", view_direction);
        info("      view_up in world = ", view_up);
#endif
        auto light_x_direction = cross(light_direction, view_direction);
        auto light_x_up = cross(light_direction, view_up);

        auto light_x = (length(light_x_direction) > length(light_x_up)) ? normalize(light_x_direction) : normalize(light_x_up);
        auto light_y = cross(light_x, light_direction);
        auto light_z = light_direction;

        // clamp the near and far values
        if (n > maxShadowDistance)
        {
            // near plane further than maximum shadow distance so no need to generate shadow maps
            continue;
        }
        if (f > maxShadowDistance)
        {
            f = maxShadowDistance;
        }

        auto updateCamera = [&](double clip_near_z, double clip_far_z, const dmat4& clipToWorld) -> void {
            const auto& shadowMap = shadowMaps[shadowMapIndex];
            preRenderSwitch->children[shadowMapIndex].mask = MASK_ALL;

            const auto& camera = shadowMap.view->camera;
            auto lookAt = camera->viewMatrix.cast<LookAt>();
            auto ortho = camera->projectionMatrix.cast<Orthographic>();

            if (!lookAt) camera->viewMatrix = lookAt = LookAt::create();
            if (!ortho) camera->projectionMatrix = ortho = Orthographic::create();

            auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
            auto sm_eye = (ws_bounds.min + ws_bounds.max) * 0.5 - light_z * (0.5 * length(ws_bounds.max - ws_bounds.min));

            lookAt->eye = sm_eye;
            lookAt->center = sm_eye + light_z;
            lookAt->up = light_y;

            auto ls_bounds = computeFrustumBounds(clip_near_z, clip_far_z, lookAt->transform() * clipToWorld);

            ortho->left = ls_bounds.min.x;
            ortho->right = ls_bounds.max.x;
            ortho->bottom = ls_bounds.min.y;
            ortho->top = ls_bounds.max.y;
            ortho->nearDistance = -ls_bounds.max.z;
            ortho->farDistance = -ls_bounds.min.z;

            dmat4 shadowMapProjView = camera->projectionMatrix->transform() * camera->viewMatrix->transform();

            dmat4 shadowMapTM = scale(0.5, 0.5, 1.0) * translate(1.0, 1.0, shadowMapBias) * shadowMapProjView * inverse_viewMatrix;

            // convert tex gen matrix to float matrix and assign to light data
            mat4 m(shadowMapTM);

            assignLightData(m[0]);
            assignLightData(m[1]);
            assignLightData(m[2]);
            assignLightData(m[3]);

            // info("m = ", m);

            m = inverse(m);

            assignLightData(m[0]);
            assignLightData(m[1]);
            assignLightData(m[2]);
            assignLightData(m[3]);

            // advance to the next shadowMap
            shadowMapIndex++;
        };

#if 0
        info("     light_x = ", light_x);
        info("     light_y = ", light_y);
        info("     light_z = ", light_z);
#endif

#if 0
        double range = f - n;
        info("    n = ", n, ", f = ", f, ", range = ", range);
#endif
        auto clipToWorld = inverse(projectionMatrix * viewMatrix);

        if (activeNumShadowMaps > 1)
        {
            double m = static_cast<double>(activeNumShadowMaps);
            for (double i = 0; i < m; i += 1.0)
            {
                dvec3 eye_near(0.0, 0.0, -Cpractical(n, f, i, m, lambda));
                dvec3 eye_far(0.0, 0.0, -Cpractical(n, f, i + 1.0, m, lambda));

                auto clip_near = projectionMatrix * eye_near;
                auto clip_far = projectionMatrix * eye_far;

                updateCamera(clip_near.z, clip_far.z, clipToWorld);
            }
        }
        else
        {
            dvec3 eye_near(0.0, 0.0, -n);
            dvec3 eye_far(0.0, 0.0, -f);

            auto clip_near = projectionMatrix * eye_near;
            auto clip_far = projectionMatrix * eye_far;

            updateCamera(clip_near.z, clip_far.z, clipToWorld);
        }
    }

    for (auto& [mv, light] : pointLights)
    {
        auto eye_position = mv * light->position;
        assignLightData4(light->color.r, light->color.g, light->color.b, light->intensity);
        assignLightData4(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), 0.0f);
    }

    for (auto& [mv, light] : spotLights)
    {
        auto eye_position = mv * light->position;
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        float cos_innerAngle = static_cast<float>(cos(light->innerAngle));
        float cos_outerAngle = static_cast<float>(cos(light->outerAngle));
        assignLightData4(light->color.r, light->color.g, light->color.b, light->intensity);
        assignLightData4(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), cos_innerAngle);
        assignLightData4(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), cos_outerAngle);

        auto shadowSettings = getActiveShadowSettings(light);
        uint32_t activeNumShadowMaps = shadowSettings ? std::min(shadowSettings->shadowMapCount, numShadowMaps - shadowMapIndex) : 0;
        if (shadowSettings)
        {
            if (shadowSettings->type_info() == typeid(HardShadows))
            {
                assignLightData4(static_cast<float>(activeNumShadowMaps), -1.0f, -1.0f, 0.0f);
            }
            else if (shadowSettings->type_info() == typeid(SoftShadows))
            {
                const SoftShadows& pcfShadowSettings = static_cast<const SoftShadows&>(*shadowSettings);
                assignLightData4(static_cast<float>(activeNumShadowMaps), pcfShadowSettings.penumbraRadius, -1.0f, 0.0f);
            }
            else if (shadowSettings->type_info() == typeid(PercentageCloserSoftShadows))
            {
                assignLightData4(static_cast<float>(activeNumShadowMaps), 0.1f /* todo: calculate blocker search radius */, static_cast<float>(light->radius), 0.0f);
            }
        }
        else
            assignLightData4(0.0f, 0.0f, 0.0f, 0.0f);

        if (activeNumShadowMaps == 0) continue;

        // set up shadow map rendering backend
        requiresPerRenderShadowMaps = true;

        // compute spot light space
        // light direction in world coords
        auto light_direction = normalize(light->direction * (inverse_3x3(mv * inverse_viewMatrix)));
        auto light_position = inverse_viewMatrix * mv * light->position;
#if 0
        info("   spot light : light direction in world = ", light_direction, ", light->shadowMapCount = ", light->shadowMapCount);
        info("      light->direction in model = ", light->direction);
        info("      view_direction in world = ", view_direction);
        info("      view_up in world = ", view_up);
#endif
        auto light_x_direction = cross(light_direction, view_direction);
        auto light_x_up = cross(light_direction, view_up);

        auto light_x = (length(light_x_direction) > length(light_x_up)) ? normalize(light_x_direction) : normalize(light_x_up);
        auto light_y = cross(light_x, light_direction);
        auto light_z = light_direction;

        // clamp the near and far values
        if (n > maxShadowDistance)
        {
            // near plane further than maximum shadow distance so no need to generate shadow maps
            continue;
        }
        if (f > maxShadowDistance)
        {
            f = maxShadowDistance;
        }

        auto light_outerAngle = light->outerAngle;
        auto light_intensity = light->intensity;

        auto updateCamera = [&](double clip_near_z, double clip_far_z, const dmat4& clipToWorld) -> void {
            const auto& shadowMap = shadowMaps[shadowMapIndex];
            preRenderSwitch->children[shadowMapIndex].mask = MASK_ALL;

            const auto& camera = shadowMap.view->camera;
            auto lookAt = camera->viewMatrix.cast<LookAt>();
            auto relativeProjection = camera->projectionMatrix.cast<RelativeProjection>();

            if (!lookAt) camera->viewMatrix = lookAt = LookAt::create();
            if (!relativeProjection) camera->projectionMatrix = relativeProjection = RelativeProjection::create(dmat4{}, Perspective::create());

            auto perspective = relativeProjection->projectionMatrix.cast<Perspective>();

            lookAt->eye = light_position;
            lookAt->center = light_position + light_z;
            lookAt->up = light_y;

            perspective->aspectRatio = 1.0;
            perspective->fieldOfViewY = 2.0 * degrees(light_outerAngle);
            perspective->farDistance = sqrt(light_intensity / 0.001);
            perspective->nearDistance = perspective->farDistance / 10000.0;

            dmat4 intermediateProjView = perspective->transform() * camera->viewMatrix->transform();

            auto ls_bounds = computeFrustumBoundsClipped(clip_near_z, clip_far_z, intermediateProjView * clipToWorld);
            ls_bounds.min = dvec3(std::max(-1.0, ls_bounds.min.x), std::max(-1.0, ls_bounds.min.y), std::max(0.0, ls_bounds.min.z));
            ls_bounds.max = dvec3(std::min(1.0, ls_bounds.max.x), std::min(1.0, ls_bounds.max.y), std::min(1.0, ls_bounds.max.z));

            // we need to use the reverse Z depth range without actually reversing depth, as the previous matrix already does that
            auto tweakedOrthographic = [](double left, double right, double bottom, double top, double zNear, double zFar) {
                return dmat4(2.0 / (right - left), 0.0, 0.0, 0.0,
                             0.0, 2.0 / (top - bottom), 0.0, 0.0,
                             0.0, 0.0, 1.0 / (zFar - zNear), 0.0,
                             -(right + left) / (right - left), -(top + bottom) / (top - bottom), -zNear / (zFar - zNear), 1.0);
            };

            relativeProjection->matrix = tweakedOrthographic(ls_bounds.min.x, ls_bounds.max.x, ls_bounds.min.y, ls_bounds.max.y, ls_bounds.min.z, ls_bounds.max.z);

            dmat4 shadowMapProjView = camera->projectionMatrix->transform() * camera->viewMatrix->transform();

            dmat4 shadowMapTM = scale(0.5, 0.5, 1.0 + shadowMapBias) * translate(1.0, 1.0, 0.0) * shadowMapProjView * inverse_viewMatrix;

            // convert tex gen matrix to float matrix and assign to light data
            mat4 m(shadowMapTM);

            assignLightData(m[0]);
            assignLightData(m[1]);
            assignLightData(m[2]);
            assignLightData(m[3]);

            // info("m = ", m);

            m = inverse(m);

            assignLightData(m[0]);
            assignLightData(m[1]);
            assignLightData(m[2]);
            assignLightData(m[3]);

            // advance to the next shadowMap
            shadowMapIndex++;
        };

#if 0
        info("     light_x = ", light_x);
        info("     light_y = ", light_y);
        info("     light_z = ", light_z);
#endif

#if 0
        double range = f - n;
        info("    n = ", n, ", f = ", f, ", range = ", range);
#endif
        auto clipToWorld = inverse(projectionMatrix * viewMatrix);

        if (activeNumShadowMaps > 1)
        {
            double m = static_cast<double>(activeNumShadowMaps);
            for (double i = 0; i < m; i += 1.0)
            {
                dvec3 eye_near(0.0, 0.0, -Cpractical(n, f, i, m, lambda));
                dvec3 eye_far(0.0, 0.0, -Cpractical(n, f, i + 1.0, m, lambda));

                auto clip_near = projectionMatrix * eye_near;
                auto clip_far = projectionMatrix * eye_far;

                updateCamera(clip_near.z, clip_far.z, clipToWorld);
            }
        }
        else
        {
            dvec3 eye_near(0.0, 0.0, -n);
            dvec3 eye_far(0.0, 0.0, -f);

            auto clip_near = projectionMatrix * eye_near;
            auto clip_far = projectionMatrix * eye_far;

            updateCamera(clip_near.z, clip_far.z, clipToWorld);
        }
    }

    // 如果光源数据已更改，标记为脏
    if (numLightDataChanges > 0)
    {
        lightData->dirty();
    }

    // 如果需要每帧渲染阴影贴图且存在预渲染命令图，执行预渲染
    if (requiresPerRenderShadowMaps && preRenderCommandGraph)
    {
        // 共享或复制性能分析工具（用于线程安全）
        if (rt.instrumentation && !preRenderCommandGraph->instrumentation)
        {
            preRenderCommandGraph->instrumentation = shareOrDuplicateForThreadSafety(rt.instrumentation);
        }

        // info("ViewDependentState::traverse(RecordTraversal&) doing pre render command graph. shadowMapIndex = ", shadowMapIndex);
        // 接受记录遍历，生成阴影贴图渲染命令
        preRenderCommandGraph->accept(rt);
    }
}

// 绑定描述符集到命令缓冲区
// commandBuffer: 命令缓冲区对象
// pipelineBindPoint: 管线绑定点（图形或计算）
// layout: 管线布局
// firstSet: 第一个描述符集的索引
// 绑定视图相关状态的描述符集（包含光源数据和阴影贴图）到管线
void ViewDependentState::bindDescriptorSets(CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet)
{
    auto vk = descriptorSet->vk(commandBuffer.deviceID);
    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, 1, &vk, 0, nullptr);
}
