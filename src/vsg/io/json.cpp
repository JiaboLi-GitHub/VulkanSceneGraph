/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Objects.h>
#include <vsg/core/Value.h>
#include <vsg/io/JSONParser.h>
#include <vsg/io/Path.h>
#include <vsg/io/json.h>
#include <vsg/io/mem_stream.h>

#include <fstream>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// json ReaderWriter - JSON文件读取器/写入器
//
// 构造函数：创建JSON读取器/写入器对象
// JSON格式用于存储元数据和配置信息
json::json()
{
}

// 检查扩展名是否支持
// ext: 文件扩展名
// 返回: 如果扩展名为.json则返回true
bool json::supportedExtension(const Path& ext) const
{
    return ext == ".json";
}

// 从输入流读取JSON文件（内部方法）
// fin: 输入流对象
// 返回: 解析后的对象（Object或Objects）
// 解析JSON文件，支持JSON对象和JSON数组两种格式
ref_ptr<Object> json::_read(std::istream& fin, ref_ptr<const Options>) const
{
    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    if (fileSize == 0) return {};

    JSONParser parser;

    parser.buffer.resize(fileSize);

    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(parser.buffer.data()), fileSize);

    ref_ptr<Object> result;

    // 跳过空白字符
    parser.pos = parser.buffer.find_first_not_of(" \t\r\n", 0);
    if (parser.pos == std::string::npos) return {};

    // 根据第一个字符判断是JSON对象还是JSON数组
    if (parser.buffer[parser.pos] == '{')
    {
        // JSON对象
        JSONtoMetaDataSchema schema;
        parser.read_object(schema);
        result = schema.object;

        info("Read JSON object, result = ", result);
    }
    else if (parser.buffer[parser.pos] == '[')
    {
        // JSON数组
        JSONtoMetaDataSchema schema;
        parser.read_array(schema);
        result = schema.objects;

        info("Read JSON array, result = ", result);
    }
    else
    {
        warn("Parsing error, could not find opening { or [.");
    }

    return result;
}

ref_ptr<Object> json::read(const Path& filename, ref_ptr<const Options> options) const
{
    Path ext = (options && options->extensionHint) ? options->extensionHint : lowerCaseFileExtension(filename);
    if (!supportedExtension(ext)) return {};

    Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    std::ifstream fin(filenameToUse, std::ios::ate | std::ios::binary);
    return _read(fin, options);
}

ref_ptr<Object> json::read(std::istream& fin, ref_ptr<const Options> options) const
{
    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    return _read(fin, options);
}

ref_ptr<Object> json::read(const uint8_t* ptr, size_t size, ref_ptr<const Options> options) const
{
    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    mem_stream fin(ptr, size);
    return _read(fin, options);
}

bool json::getFeatures(Features& features) const
{
    ReaderWriter::FeatureMask supported_features = static_cast<ReaderWriter::FeatureMask>(ReaderWriter::READ_FILENAME | ReaderWriter::READ_ISTREAM | ReaderWriter::READ_MEMORY);
    features.extensionFeatureMap[".json"] = supported_features;

    return true;
}
