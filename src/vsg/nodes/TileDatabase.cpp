/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/tile.h>
#include <vsg/nodes/TileDatabase.h>

using namespace vsg;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TileDatabaseSettings
//
// 构造函数：创建瓦片数据库设置对象
// 瓦片数据库设置用于配置瓦片数据库的各种参数，包括范围、级别、投影、图层等
TileDatabaseSettings::TileDatabaseSettings()
{
}

// 拷贝构造函数：从另一个瓦片数据库设置创建新的设置对象
// rhs: 要拷贝的瓦片数据库设置对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝所有设置参数，包括范围、瓦片数量、最大级别、投影、椭球模型、图层配置等
TileDatabaseSettings::TileDatabaseSettings(const TileDatabaseSettings& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    extents(rhs.extents),
    noX(rhs.noX),
    noY(rhs.noY),
    maxLevel(rhs.maxLevel),
    originTopLeft(rhs.originTopLeft),
    lodTransitionScreenHeightRatio(rhs.lodTransitionScreenHeightRatio),
    projection(rhs.projection),
    ellipsoidModel(copyop(rhs.ellipsoidModel)),
    imageLayer(rhs.imageLayer),
    imageLayerCallback(rhs.imageLayerCallback),
    detailLayer(rhs.detailLayer),
    detailLayerCallback(rhs.detailLayerCallback),
    elevationLayer(rhs.elevationLayer),
    elevationLayerCallback(rhs.elevationLayerCallback),
    elevationScale(rhs.elevationScale),
    skirtRatio(rhs.skirtRatio),
    mipmapLevelsHint(rhs.mipmapLevelsHint),
    lighting(rhs.lighting),
    shaderSet(copyop(rhs.shaderSet))
{
}

int TileDatabaseSettings::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(extents, rhs.extents)) != 0) return result;
    if ((result = compare_value(noX, rhs.noX)) != 0) return result;
    if ((result = compare_value(noY, rhs.noY)) != 0) return result;
    if ((result = compare_value(maxLevel, rhs.maxLevel)) != 0) return result;
    if ((result = compare_value(originTopLeft, rhs.originTopLeft)) != 0) return result;
    if ((result = compare_value(lodTransitionScreenHeightRatio, rhs.lodTransitionScreenHeightRatio)) != 0) return result;
    if ((result = compare_value(projection, rhs.projection)) != 0) return result;
    if ((result = compare_pointer(ellipsoidModel, rhs.ellipsoidModel)) != 0) return result;
    if ((result = compare_value(imageLayer, rhs.imageLayer)) != 0) return result;
    if ((result = compare_value(detailLayer, rhs.detailLayer)) != 0) return result;
    if ((result = compare_value(elevationLayer, rhs.elevationLayer)) != 0) return result;
    if ((result = compare_value(elevationScale, rhs.elevationScale)) != 0) return result;
    if ((result = compare_value(skirtRatio, rhs.skirtRatio)) != 0) return result;
    if ((result = compare_value(maxTileDimension, rhs.maxTileDimension)) != 0) return result;
    if ((result = compare_value(mipmapLevelsHint, rhs.mipmapLevelsHint)) != 0) return result;
    if ((result = compare_value(lighting, rhs.lighting)) != 0) return result;
    return compare_pointer(shaderSet, rhs.shaderSet);
}

void TileDatabaseSettings::read(vsg::Input& input)
{
    input.read("extents", extents);
    input.read("noX", noX);
    input.read("noY", noY);
    input.read("maxLevel", maxLevel);
    input.read("originTopLeft", originTopLeft);
    input.read("lodTransitionScreenHeightRatio", lodTransitionScreenHeightRatio);
    input.read("projection", projection);
    input.readObject("ellipsoidModel", ellipsoidModel);
    input.read("imageLayer", imageLayer);
    if (input.version_greater_equal(1, 1, 9))
    {
        input.read("detailLayer", detailLayer);
        input.read("elevationLayer", elevationLayer);
        input.read("elevationScale", elevationScale);
        input.read("skirtRatio", skirtRatio);
        input.read("maxTileDimension", maxTileDimension);
    }
    else
    {
        input.read("terrainLayer", elevationLayer);
    }
    input.read("mipmapLevelsHint", mipmapLevelsHint);

    if (input.version_greater_equal(0, 7, 1))
    {
        input.read("lighting", lighting);
        input.readObject("shaderSet", shaderSet);
    }
}

void TileDatabaseSettings::write(vsg::Output& output) const
{
    output.write("extents", extents);
    output.write("noX", noX);
    output.write("noY", noY);
    output.write("maxLevel", maxLevel);
    output.write("originTopLeft", originTopLeft);
    output.write("lodTransitionScreenHeightRatio", lodTransitionScreenHeightRatio);
    output.write("projection", projection);
    output.writeObject("ellipsoidModel", ellipsoidModel);
    output.write("imageLayer", imageLayer);
    if (output.version_greater_equal(1, 1, 9))
    {
        output.write("detailLayer", detailLayer);
        output.write("elevationLayer", elevationLayer);
        output.write("elevationScale", elevationScale);
        output.write("skirtRatio", skirtRatio);
        output.write("maxTileDimension", maxTileDimension);
    }
    else
    {
        output.write("terrainLayer", elevationLayer);
    }
    output.write("mipmapLevelsHint", mipmapLevelsHint);

    if (output.version_greater_equal(0, 7, 1))
    {
        output.write("lighting", lighting);
        output.writeObject("shaderSet", shaderSet);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TileDatabase
//
// 构造函数：创建瓦片数据库节点
// 瓦片数据库节点用于管理和加载分层的瓦片数据（如地图瓦片、地形瓦片等）
TileDatabase::TileDatabase()
{
}

// 拷贝构造函数：从另一个瓦片数据库节点创建新的瓦片数据库节点
// rhs: 要拷贝的瓦片数据库节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝设置和子节点
TileDatabase::TileDatabase(const TileDatabase& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    settings(copyop(rhs.settings)),
    child(copyop(rhs.child))
{
}

// 比较两个瓦片数据库节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较设置和子节点
int TileDatabase::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_pointer(settings, rhs.settings)) != 0) return result;
    return compare_pointer(child, rhs.child);
}

// 从输入流读取瓦片数据库节点对象
// input: 输入流对象
// 读取设置，然后读取数据库
void TileDatabase::read(vsg::Input& input)
{
    Node::read(input);

    input.readObject("settings", settings);

    readDatabase(input.options);
}

// 将瓦片数据库节点对象写入输出流
// output: 输出流对象
// 写入设置（不写入子节点，因为它是动态加载的）
void TileDatabase::write(vsg::Output& output) const
{
    Node::write(output);

    output.writeObject("settings", settings);
}

// 读取瓦片数据库
// options: 读取选项对象
// 返回: 如果成功加载则返回true，否则返回false
// 根据设置创建瓦片读取器，然后读取根瓦片
bool TileDatabase::readDatabase(vsg::ref_ptr<const vsg::Options> options)
{
    // 如果设置不存在或子节点已存在，返回false
    if (!settings || child) return false;

    // 如果设置了椭球模型，将其设置为辅助对象
    if (settings->ellipsoidModel) setObject("EllipsoidModel", settings->ellipsoidModel);

    // 创建瓦片读取器
    auto tileReader = tile::create(settings, options);

    // 创建本地选项，将瓦片读取器插入到读取器列表的开头
    auto local_options = options ? vsg::clone(options) : vsg::Options::create();
    local_options->readerWriters.insert(local_options->readerWriters.begin(), tileReader);

    // 读取根瓦片
    auto result = vsg::read("root.tile", local_options);

    // 将结果转换为节点
    child = result.cast<vsg::Node>();
    if (!child)
    {
        // 如果失败，检查是否是读取错误
        auto error = result.cast<ReadError>();
        if (error)
            warn("TileDatabase::readDatabase() imageLayer = ", settings->imageLayer, " failed to load. Error: ", error->message);
        else
            warn("TileDatabase::readDatabase() imageLayer = ", settings->imageLayer, " failed to load.");
    }

    return child.valid();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  createBingMapsSettings
//
std::string_view vsg::find_field(const std::string& source, const std::string_view& start_match, const std::string_view& end_match)
{
    auto start_pos = source.find(start_match);
    if (start_pos == std::string::npos) return {};

    start_pos += start_match.size();

    auto end_pos = source.find(end_match, start_pos);
    if (end_pos == std::string::npos) return {};

    return {&source[start_pos], end_pos - start_pos};
}

void vsg::replace(std::string& source, const std::string_view& match, const std::string_view& replacement)
{
    for (;;)
    {
        auto pos = source.find(match);
        if (pos == std::string::npos) break;
        source.erase(pos, match.size());
        source.insert(pos, replacement);
    }
}

ref_ptr<TileDatabaseSettings> vsg::createBingMapsSettings(const std::string& imagerySet, const std::string& culture, const std::string& key, ref_ptr<const Options> options)
{
    auto settings = TileDatabaseSettings::create();

    // setup BingMaps settings
    settings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
    settings->noX = 2;
    settings->noY = 2;
    settings->maxLevel = 19;
    settings->originTopLeft = true;
    settings->lighting = true;
    settings->projection = "EPSG:3857"; // Spherical Mecator

    if (!key.empty())
    {
        // read the meta data to set up the direct imageLayer URL.
        std::string metadata_url("https://dev.virtualearth.net/REST/V1/Imagery/Metadata/{imagerySet}?output=xml&key={key}");
        vsg::replace(metadata_url, "{imagerySet}", imagerySet);
        vsg::replace(metadata_url, "{key}", key);

        vsg::info("metadata_url = ", metadata_url);

        auto txt_options = vsg::clone(options);
        txt_options->extensionHint = ".txt";

        if (auto metadata = vsg::read_cast<vsg::stringValue>(metadata_url, txt_options))
        {
            auto& str = metadata->value();
            vsg::info("metadata = ", str);

            std::string copyright(vsg::find_field(str, "<Copyright>", "</Copyright>"));
            std::string brandLogoUri(vsg::find_field(str, "<BrandLogoUri>", "</BrandLogoUri>"));
            std::string zoomMax(vsg::find_field(str, "<ZoomMax>", "</ZoomMax>"));

            vsg::info("copyright = ", copyright);
            vsg::info("brandLogoUri = ", brandLogoUri);
            vsg::info("zoomMax = ", zoomMax);

            settings->setValue("copyright", copyright);
            settings->setValue("logo", brandLogoUri);

            if (!brandLogoUri.empty())
            {
                auto logo = vsg::read_cast<vsg::Data>(brandLogoUri, options);
                vsg::info("logo = ", logo);
                // TODO need to create a logo subgraph
            }

            auto imageUrl = vsg::find_field(str, "<ImageUrl>", "</ImageUrl>");
            if (!imageUrl.empty())
            {
                std::string url(imageUrl);

                auto sumbdomain = vsg::find_field(str, "<ImageUrlSubdomains><string>", "</string>");
                if (!sumbdomain.empty())
                {
                    vsg::replace(url, "{subdomain}", sumbdomain);
                }

                vsg::replace(url, "{culture}", culture);

                settings->imageLayer = url;
            }
        }
    }

    if (settings->imageLayer.empty())
    {
        // direct access fallback
        settings->imageLayer = "https://ecn.t3.tiles.virtualearth.net/tiles/h{quadkey}.jpeg?g=1236";
    }

    return settings;
}

ref_ptr<TileDatabaseSettings> vsg::createOpenStreetMapSettings(ref_ptr<const Options> /*options*/)
{
    auto settings = vsg::TileDatabaseSettings::create();
    settings->extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
    settings->noX = 1;
    settings->noY = 1;
    settings->maxLevel = 17;
    settings->originTopLeft = true;
    settings->lighting = false;
    settings->projection = "EPSG:3857"; // Spherical Mecator
    settings->imageLayer = "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png";

    return settings;
}
