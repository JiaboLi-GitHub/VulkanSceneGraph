/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/ConstVisitor.h>
#include <vsg/core/External.h>

#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>

#include <map>
#include <unordered_map>

using namespace vsg;

// CollectIDs辅助类
// 用于收集对象ID的访问者，在序列化/反序列化过程中使用
class CollectIDs : public ConstVisitor
{
public:
    CollectIDs() {}

    // 访问对象并分配ID
    // 为每个对象分配唯一的ID，避免重复分配
    void apply(const Object& object) override
    {
        auto itr = _objectIDMap.find(&object);
        if (itr == _objectIDMap.end())
        {
            ObjectID id = _objectID++;
            _objectIDMap[&object] = id;
            object.traverse(*this);
        }
    }

    using ObjectID = uint32_t;
    using ObjectIDMap = std::unordered_map<const Object*, ObjectID>;

    ObjectID _objectID = 0;  // 当前对象ID计数器
    ObjectIDMap _objectIDMap;  // 对象到ID的映射

    // 对象ID范围结构
    // 用于记录外部文件中对象的ID范围
    struct ObjectIDRange
    {
        const Object* object = nullptr;  // 对象指针
        ObjectID startID = 0;  // 起始ID
        ObjectID endID = 0;  // 结束ID
    };

    using ObjectIDRangeMap = std::map<Path, ObjectIDRange>;
    ObjectIDRangeMap objectIDRangeMap;  // 文件路径到ID范围的映射
};

// External类的默认构造函数
// 创建空的外部对象引用
External::External()
{
}

// External类的构造函数
// 使用路径-对象映射初始化外部对象引用
// in_entries: 路径到对象的映射
External::External(const PathObjects& in_entries) :
    entries(in_entries)
{
}

// External类的构造函数
// 使用单个文件名和对象初始化外部对象引用
// filename: 外部文件路径
// object: 要引用的对象
External::External(const vsg::Path& filename, ref_ptr<Object> object) :
    entries{{filename, object}}
{
}

// External类的析构函数
External::~External()
{
}

// 从输入流读取External对象
// 读取外部文件引用信息，并加载外部文件中的对象
void External::read(Input& input)
{
    entries.clear();

    Object::read(input);

    // 读取选项
    input.read("options", options);

    CollectIDs collectIDs;

    // 读取外部文件数量
    uint32_t count = input.readValue<uint32_t>("NumEntries");
    Paths filenames(count);
    // 读取每个外部文件的ID范围和文件名
    for (auto& filename : filenames)
    {
        CollectIDs::ObjectIDRange objectIDRange;
        input.read("StartID_EndID_Filename", objectIDRange.startID, objectIDRange.endID, filename);
        collectIDs.objectIDRangeMap[filename] = objectIDRange;
    }

    // 读取外部文件中的对象
    if (options)
    {
        entries = vsg::read(filenames, options);
    }
    else
    {
        entries = vsg::read(filenames, input.options);
    }

    // 从文件中收集对象ID
    for (auto itr = entries.begin(); itr != entries.end(); ++itr)
    {
        const auto& objectIDRange = collectIDs.objectIDRangeMap[itr->first];
        collectIDs._objectID = objectIDRange.startID;
        if (itr->second)
            // 如果对象存在，遍历并收集ID
            itr->second->accept(collectIDs);
        else
        {
            // 如果对象不存在，将ID范围标记为nullptr
            for (uint32_t objectID = objectIDRange.startID; objectID <= objectIDRange.endID; ++objectID)
            {
                input.objectIDMap[objectID] = nullptr;
            }
        }
    }

    // 将收集到的对象ID映射到输入的对象ID映射中
    for (auto [object, objectID] : collectIDs._objectIDMap)
    {
        input.objectIDMap[objectID] = const_cast<Object*>(object);
    }
}

// 将External对象写入输出流
// 序列化外部文件引用信息，并写入外部文件
void External::write(Output& output) const
{
    Object::write(output);

    // 写入选项
    output.write("options", options);

    CollectIDs collectIDs;
    collectIDs._objectID = output.objectID;

    // 为每个外部对象收集ID范围
    for (auto& [filename, externalObject] : entries)
    {
        if (filename && externalObject)
        {
            // 记录起始ID，遍历对象收集ID，然后记录ID范围
            auto startObjectID = collectIDs._objectID;
            externalObject->accept(collectIDs);
            collectIDs.objectIDRangeMap[filename] = CollectIDs::ObjectIDRange{externalObject, startObjectID, collectIDs._objectID};
        }
        else
        {
            // 如果对象不存在，ID范围为空
            collectIDs.objectIDRangeMap[filename] = CollectIDs::ObjectIDRange{nullptr, collectIDs._objectID, collectIDs._objectID};
        }
    }
    uint32_t idEnd = collectIDs._objectID;
    output.objectID = idEnd;

    // 将对象ID传递到输出的对象ID映射中
    for (auto& [object, objectID] : collectIDs._objectIDMap)
    {
        output.objectIDMap[object] = objectID;
    }

    // 写入外部文件数量
    output.writeValue<uint32_t>("NumEntries", entries.size());
    // 写入每个外部文件的ID范围和文件名
    for (auto itr = entries.begin(); itr != entries.end(); ++itr)
    {
        auto& objectIDRange = collectIDs.objectIDRangeMap[itr->first];
        output.write("StartID_EndID_Filename", objectIDRange.startID, objectIDRange.endID, itr->first);
    }

    // 写入外部文件
    for (auto& [filename, externalObject] : entries)
    {
        // 如果需要写入对象，则调用ReaderWriter写入
        if (filename && externalObject)
        {
            vsg::write(externalObject, filename, output.options);
        }
    }
}
