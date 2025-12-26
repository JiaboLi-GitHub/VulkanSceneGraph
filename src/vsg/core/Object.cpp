/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Allocator.h>
#include <vsg/core/Auxiliary.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Object.h>
#include <vsg/core/Visitor.h>

#include <vsg/io/Input.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Output.h>

using namespace vsg;

// Object类的默认构造函数
// 初始化引用计数为0，辅助对象指针为空
Object::Object() :
    _referenceCount(0),
    _auxiliary(nullptr)
{
}

// Object类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
Object::Object(const Object& rhs, const CopyOp& copyop) :
    Object()
{
    // 将当前拷贝构造的对象赋值到copyop.duplicate中，以便后续可以引用
    if (copyop.duplicate)
    {
        if (auto itr = copyop.duplicate->find(&rhs); itr != copyop.duplicate->end())
        {
            itr->second = this;
        }
    }

    // 如果源对象的辅助对象是唯一附加的，需要创建自己的辅助对象并复制其ObjectMap
    if (rhs._auxiliary && rhs._auxiliary->getConnectedObject() == &rhs)
    {
        // 源对象的辅助对象是唯一附加的，需要创建自己的辅助对象并复制其ObjectMap
        auto& userObjects = getOrCreateAuxiliary()->userObjects;
        userObjects = rhs._auxiliary->userObjects;
        // 如果存在重复映射，则对用户对象也进行拷贝操作
        if (copyop.duplicate)
        {
            for (auto itr = userObjects.begin(); itr != userObjects.end(); ++itr)
            {
                itr->second = copyop(itr->second);
            }
        }
    }
}

// Object类的赋值运算符
// 实现对象的赋值操作，复制辅助对象中的用户对象映射
Object& Object::operator=(const Object& rhs)
{
    //debug("Object& operator=(const Object&)");

    // 自赋值检查，避免不必要的操作
    if (&rhs == this) return *this;

    // 如果源对象有辅助对象，复制其用户对象映射
    if (rhs._auxiliary)
    {
        // 源对象的辅助对象是唯一附加的，需要创建自己的辅助对象并复制其ObjectMap
        auto& userObjects = getOrCreateAuxiliary()->userObjects;
        userObjects = rhs._auxiliary->userObjects;
    }

    return *this;
}

// Object类的析构函数
// 清理辅助对象的引用
Object::~Object()
{
    //debug("Object::~Object() ", this);

    // 如果存在辅助对象，减少其引用计数
    if (_auxiliary)
    {
        _auxiliary->unref();
    }
}

// 尝试删除对象的内部方法
// 在引用计数为0时调用，检查是否可以安全删除对象
void Object::_attemptDelete() const
{
    // 当引用计数为0时调用_delete会发生什么？需要决定是否应该测试这种错误的应用程序用法。

    // 如果存在附加的辅助对象，向其发送删除信号，给它一个机会决定删除是否合适
    // 如果没有附加辅助对象，则直接删除
    if (_auxiliary == nullptr || _auxiliary->signalConnectedObjectToBeDeleted())
    {
        //debug("Object::_delete() ", this, " calling delete");

        delete this;
    }
    else
    {
        debug("Object::_delete() ", this, " choosing not to delete");
    }
}

// 克隆对象
// 如果存在重复映射，返回已存在的克隆对象；否则返回当前对象的引用
ref_ptr<Object> Object::clone(const CopyOp& copyop) const
{
    // 如果存在重复映射，检查是否已经克隆过
    if (copyop.duplicate)
    {
        auto itr = copyop.duplicate->duplicates.find(this);
        if (itr != copyop.duplicate->duplicates.end()) return itr->second;
    }
    // 默认情况下返回当前对象的引用（浅拷贝）
    return ref_ptr<Object>(const_cast<Object*>(this));
}

// 比较两个对象
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int Object::compare(const Object& rhs) const
{
    // 如果是同一个对象，直接返回0
    if (this == &rhs) return 0;
    
    // 比较类型信息
    auto this_id = std::type_index(typeid(*this));
    auto rhs_id = std::type_index(typeid(rhs));
    if (this_id < rhs_id) return -1;
    if (this_id > rhs_id) return 1;

    // 比较辅助对象
    if (_auxiliary == rhs._auxiliary) return 0;
    return _auxiliary ? (rhs._auxiliary ? _auxiliary->compare(*rhs._auxiliary) : 1) : (rhs._auxiliary ? -1 : 0);
}

// 接受访问者模式的可变访问者
// 允许访问者访问并可能修改对象
void Object::accept(Visitor& visitor)
{
    visitor.apply(*this);
}

// 接受访问者模式的常量访问者
// 允许访问者访问对象但不允许修改
void Object::accept(ConstVisitor& visitor) const
{
    visitor.apply(*this);
}

// 接受记录遍历访问者
// 用于渲染遍历过程中记录命令
void Object::accept(RecordTraversal& visitor) const
{
    visitor.apply(*this);
}

// 从输入流读取对象数据
// 用于反序列化，读取用户对象映射
void Object::read(Input& input)
{
    // 读取用户对象的数量
    auto numObjects = input.readValue<uint32_t>("userObjects");
    if (numObjects > 0)
    {
        // 获取或创建辅助对象，然后读取所有用户对象
        auto& objectMap = getOrCreateAuxiliary()->userObjects;
        for (; numObjects > 0; --numObjects)
        {
            std::string key = input.readValue<std::string>("key");
            input.readObject("object", objectMap[key]);
        }
    }
}

// 将对象数据写入输出流
// 用于序列化，写入用户对象映射
void Object::write(Output& output) const
{
    if (_auxiliary)
    {
        // 如果有唯一的辅助对象，需要写出其ObjectMap条目
        auto& userObjects = _auxiliary->userObjects;
        output.writeValue<uint32_t>("userObjects", userObjects.size());
        // 遍历并写入所有用户对象
        for (auto& entry : userObjects)
        {
            output.write("key", entry.first);
            output.writeObject("object", entry.second.get());
        }
    }
    else
    {
        // 没有辅助对象，写入0个用户对象
        output.writeValue<uint32_t>("userObjects", 0);
    }
}

// 设置用户对象
// 将对象存储到辅助对象的用户对象映射中
void Object::setObject(const std::string& key, ref_ptr<Object> object)
{
    getOrCreateAuxiliary()->setObject(key, object);
}

// 获取用户对象（可变版本）
// 根据键值从辅助对象中获取用户对象
Object* Object::getObject(const std::string& key)
{
    if (!_auxiliary) return nullptr;
    return _auxiliary->getObject(key);
}

// 获取用户对象（常量版本）
// 根据键值从辅助对象中获取用户对象，返回常量指针
const Object* Object::getObject(const std::string& key) const
{
    if (!_auxiliary) return nullptr;
    return _auxiliary->getObject(key);
}

// 获取用户对象的引用指针（可变版本）
// 返回智能指针，提供自动内存管理
ref_ptr<Object> Object::getRefObject(const std::string& key)
{
    if (!_auxiliary) return {};
    return _auxiliary->getRefObject(key);
}

// 获取用户对象的引用指针（常量版本）
// 返回常量智能指针，提供自动内存管理
ref_ptr<const Object> Object::getRefObject(const std::string& key) const
{
    if (!_auxiliary) return {};
    return _auxiliary->getRefObject(key);
}

// 移除用户对象
// 从辅助对象的用户对象映射中删除指定键的对象
void Object::removeObject(const std::string& key)
{
    if (_auxiliary)
    {
        _auxiliary->userObjects.erase(key);
    }
}

// 设置辅助对象
// 替换当前的辅助对象，管理引用计数
void Object::setAuxiliary(Auxiliary* auxiliary)
{
    // 如果存在旧的辅助对象，重置其连接并减少引用
    if (_auxiliary)
    {
        _auxiliary->resetConnectedObject();
        _auxiliary->unref();
    }

    // 设置新的辅助对象
    _auxiliary = auxiliary;

    // 如果新辅助对象不为空，增加其引用计数
    if (auxiliary)
    {
        auxiliary->ref();
    }
}

// 获取或创建辅助对象
// 如果辅助对象不存在，则创建一个新的并返回
Auxiliary* Object::getOrCreateAuxiliary()
{
    //debug("Object::getOrCreateAuxiliary() _auxiliary=",  _auxiliary);
    if (!_auxiliary)
    {
        // 创建新的辅助对象并设置引用计数
        _auxiliary = new Auxiliary(this);
        _auxiliary->ref();
    }
    return _auxiliary;
}

// 重载new运算符
// 使用VSG的自定义内存分配器，指定对象类型的亲和性
void* Object::operator new(std::size_t count)
{
    return vsg::allocate(count, vsg::ALLOCATOR_AFFINITY_OBJECTS);
}

// 重载delete运算符
// 使用VSG的自定义内存释放器
void Object::operator delete(void* ptr)
{
    vsg::deallocate(ptr);
}
