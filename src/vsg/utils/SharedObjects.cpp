
#include <vsg/io/Logger.h>
#include <vsg/io/Options.h>
#include <vsg/utils/SharedObjects.h>

using namespace vsg;

// SharedObjects类的构造函数
// 创建共享对象管理器，用于管理和共享场景图中的对象
SharedObjects::SharedObjects() :
    suitableForSharing(SuitableForSharing::create())  // 创建适合共享判断器
{
}

// SharedObjects类的析构函数
SharedObjects::~SharedObjects()
{
}

// 检查文件是否适合共享
// 检查文件扩展名是否在排除列表中
// filename: 文件路径
// 返回值：true表示适合共享，false表示不适合
bool SharedObjects::suitable(const Path& filename) const
{
    return excludedExtensions.count(vsg::lowerCaseFileExtension(filename)) == 0;
}

// 检查是否包含指定的已加载对象
// 检查共享对象中是否已存在指定文件名和选项的对象
// filename: 文件路径
// options: 选项对象
// 返回值：true表示已存在，false表示不存在
bool SharedObjects::contains(const Path& filename, ref_ptr<const Options> options) const
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);

    auto loadedObject_id = std::type_index(typeid(LoadedObject));
    auto itr = _sharedObjects.find(loadedObject_id);
    if (itr == _sharedObjects.end()) return false;

    auto& loadedObjects = itr->second;
    auto key = LoadedObject::create(filename, options);
    return loadedObjects.find(key) != loadedObjects.end();
}

// 添加共享对象
// 将对象添加到共享对象集合中
// object: 要添加的对象
// filename: 文件路径
// options: 选项对象
void SharedObjects::add(ref_ptr<Object> object, const Path& filename, ref_ptr<const Options> options)
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);

    auto loadedObject_id = std::type_index(typeid(LoadedObject));
    auto& loadedObjects = _sharedObjects[loadedObject_id];

    auto key = LoadedObject::create(filename, options, object);
    loadedObjects.insert(key);
}

// 移除共享对象
// 从共享对象集合中移除指定文件名和选项的对象
// filename: 文件路径
// options: 选项对象
// 返回值：true表示成功移除，false表示对象不存在
bool SharedObjects::remove(const Path& filename, ref_ptr<const Options> options)
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);

    auto loadedObject_id = std::type_index(typeid(LoadedObject));
    auto itr = _sharedObjects.find(loadedObject_id);
    if (itr == _sharedObjects.end()) return false;

    auto& loadedObjects = itr->second;

    auto key = LoadedObject::create(filename, options);
    if (auto lo_itr = loadedObjects.find(key); lo_itr != loadedObjects.end())
    {
        loadedObjects.erase(lo_itr);
        return true;
    }
    else
    {
        return false;
    }
}

// 清空所有共享对象
// 清除所有默认对象和共享对象
void SharedObjects::clear()
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);
    _defaults.clear();
    _sharedObjects.clear();
}

// 修剪共享对象
// 移除没有外部引用的共享对象（引用计数为1的对象）
// 使用观察者指针来避免本地引用阻止对象被修剪
void SharedObjects::prune()
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);

    auto loadedObject_id = std::type_index(typeid(LoadedObject));

    // 记录每个LoadedObject对象的观察者指针，以便清除它们，防止本地引用阻止修剪
    auto& loadedObjects = _sharedObjects[loadedObject_id];
    std::vector<observer_ptr<Object>> observedLoadedObjects(loadedObjects.size());
    auto observedLoadedObject_itr = observedLoadedObjects.begin();
    for (auto& object : loadedObjects)
    {
        auto& loadedObject = static_cast<LoadedObject&>(*object);
        *(observedLoadedObject_itr++) = loadedObject.object;
        loadedObject.object = {};  // 清除引用
    }

    // 记录每个共享默认对象的观察者指针，以便清除它们，防止本地引用阻止修剪
    std::vector<observer_ptr<Object>> observedDefaults(_defaults.size());
    auto observedDefaults_itr = observedDefaults.begin();
    for (auto defaults_itr = _defaults.begin(); defaults_itr != _defaults.end(); ++defaults_itr)
    {
        *(observedDefaults_itr++) = defaults_itr->second;
    }
    _defaults.clear();

    // 修剪没有外部引用的共享对象（引用计数为1）
    bool prunedObjects = false;
    do
    {
        prunedObjects = false;
        for (auto itr = _sharedObjects.begin(); itr != _sharedObjects.end(); ++itr)
        {
            auto id = itr->first;
            if (id != loadedObject_id)
            {
                auto& objects = itr->second;
                for (auto object_itr = itr->second.begin(); object_itr != itr->second.end();)
                {
                    if ((*object_itr)->referenceCount() == 1)
                    {
                        // vsg::info("pruning ", *object_itr);
                        object_itr = objects.erase(object_itr);
                        prunedObjects = true;
                    }
                    else
                    {
                        ++object_itr;
                    }
                }
            }
        }
    } while (prunedObjects);

    // 恢复已加载对象的引用，并移除已释放的对象
    observedLoadedObject_itr = observedLoadedObjects.begin();
    for (auto object_itr = loadedObjects.begin(); object_itr != loadedObjects.end();)
    {
        auto& loadedObject = static_cast<LoadedObject&>(*(*object_itr));
        loadedObject.object = *(observedLoadedObject_itr++);
        if (!loadedObject.object)
        {
            // vsg::info("pruning loadedObject ", *object_itr);
            object_itr = loadedObjects.erase(object_itr);
        }
        else
        {
            ++object_itr;
        }
    }

    // 重新分配仍有引用的默认对象
    for (const auto& observerDefault : observedDefaults)
    {
        ref_ptr<Object> defaultObject = observerDefault;
        if (defaultObject)
        {
            const auto& object = *defaultObject;
            _defaults[std::type_index(typeid(object))] = defaultObject;
        }
    }
}

// 报告共享对象状态
// 输出共享对象的详细信息到日志
// output: 日志输出对象
void SharedObjects::report(vsg::LogOutput& output)
{
    std::scoped_lock<std::recursive_mutex> lock(_mutex);
    output("SharedObjects::report(..) ", this, " {");
    output.in();
    // 输出默认对象信息
    output("SharedObjects::_defaults ", _defaults.size(), " {");
    output.in();
    for (auto& [type, object] : _defaults)
    {
        output(type.name(), ", object = ", object, " ", object->referenceCount());
    }
    output.out();
    output("}");

    // 输出共享对象信息
    output("SharedObjects::_sharedObjects ", SharedObjects::_sharedObjects.size(), " {");
    output.in();
    for (auto& [type, objects] : _sharedObjects)
    {
        output(type.name(), ", objects = ", objects.size(), " {");
        output.in();
        for (auto& object : objects)
        {
            if (auto loadedObject = object.cast<LoadedObject>())
            {
                output("loadedObject = ", loadedObject, " ", object->referenceCount(), " ", loadedObject->filename);
            }
            else
            {
                output("object = ", object, " ", object->referenceCount());
            }
        }
        output.out();
        output("}");
    }
    output.out();
    output("}");
    output.out();
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LoadedObject - 已加载对象，用于表示从文件加载的对象
//

// LoadedObject类的构造函数
// 创建已加载对象，用于在共享对象中标识从文件加载的对象
// in_filename: 文件路径
// in_options: 选项对象
// in_object: 加载的对象
LoadedObject::LoadedObject(const Path& in_filename, ref_ptr<const Options> in_options, ref_ptr<Object> in_object) :
    filename(in_filename),  // 文件路径
    options(Options::create_if(in_options, *in_options)),  // 选项对象（创建副本）
    object(in_object)  // 加载的对象
{
    // 清除选项中的sharedObjects引用，避免循环引用
    if (options) options->sharedObjects = {};
}

// 遍历LoadedObject（可变访问者）
// 将访问者应用到加载的对象
// visitor: 访问者对象
void LoadedObject::traverse(Visitor& visitor)
{
    if (object) object->accept(visitor);
}

// 遍历LoadedObject（常量访问者）
// 将访问者应用到加载的对象
// visitor: 常量访问者对象
void LoadedObject::traverse(ConstVisitor& visitor) const
{
    if (object) object->accept(visitor);
}

// 比较两个LoadedObject对象
// 首先比较基类，然后比较文件名和选项
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int LoadedObject::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    // 比较文件名
    if ((result = filename.compare(rhs.filename))) return result;
    // 比较选项
    return compare_pointer(options, rhs.options);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SuitableForSharing - 适合共享判断器，用于判断对象是否适合共享
//

// 应用访问者到Object对象
// 如果对象适合共享，继续遍历
// object: 要检查的对象
void SuitableForSharing::apply(const Object& object)
{
    if (suitableForSharing) object.traverse(*this);
}

// 应用访问者到PagedLOD对象
// PagedLOD对象不适合共享（因为它们是分页的）
void SuitableForSharing::apply(const PagedLOD&)
{
    suitableForSharing = false;
}
