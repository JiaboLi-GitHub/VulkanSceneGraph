/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Auxiliary.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Output.h>

using namespace vsg;

// Auxiliary类的构造函数
// 创建辅助对象，用于存储与Object关联的用户对象映射
// object: 要连接的Object对象指针
Auxiliary::Auxiliary(Object* object) :
    _referenceCount(0),
    _connectedObject(object)
{
    //vsg::debug("Auxiliary::Auxiliary(Object = ", object, ") ", this);
}

// Auxiliary类的析构函数
// 清理辅助对象资源
Auxiliary::~Auxiliary()
{
    //vsg::debug("Auxiliary::~Auxiliary() ", this);
}

// 增加引用计数
// 线程安全地增加辅助对象的引用计数
void Auxiliary::ref() const
{
    ++_referenceCount;
    //debug("Auxiliary::ref() ", this, " ", _referenceCount.load());
}

// 减少引用计数
// 当引用计数减到0或1时，自动删除辅助对象
void Auxiliary::unref() const
{
    //debug("Auxiliary::unref() ", this, " ", _referenceCount.load());
    if (_referenceCount.fetch_sub(1) <= 1)
    {
        delete this;
    }
}

// 减少引用计数但不删除对象
// 用于某些特殊情况下需要减少引用计数但不删除对象
void Auxiliary::unref_nodelete() const
{
    //debug("Auxiliary::unref_nodelete() ", this, " ", _referenceCount.load());
    --_referenceCount;
}

// 信号通知连接的Object对象将被删除
// 检查连接的Object对象是否还有其他引用，如果没有则断开连接并允许删除
// 返回值：true表示可以删除，false表示不应该删除
bool Auxiliary::signalConnectedObjectToBeDeleted()
{
    std::scoped_lock<std::mutex> guard(_mutex);

    // 如果连接的Object对象还有引用，则不应该删除
    if (_connectedObject && _connectedObject->referenceCount() > 0)
    {
        // return false, the object should not be deleted
        return false;
    }

    // 断开辅助对象与连接对象的关联
    _connectedObject = nullptr;

    // return true, the object should be deleted
    return true;
}

// 重置连接的Object对象
// 断开辅助对象与连接对象的关联
void Auxiliary::resetConnectedObject()
{
    std::scoped_lock<std::mutex> guard(_mutex);

    _connectedObject = nullptr;
}

// 比较两个Auxiliary对象
// 比较用户对象映射中的键值对
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int Auxiliary::compare(const Auxiliary& rhs) const
{
    auto lhs_itr = userObjects.begin();
    auto rhs_itr = rhs.userObjects.begin();
    // 遍历两个用户对象映射，逐个比较键值对
    while (lhs_itr != userObjects.end() && rhs_itr != rhs.userObjects.end())
    {
        // 比较键
        if (lhs_itr->first < rhs_itr->first) return -1;
        if (lhs_itr->first > rhs_itr->first) return 1;
        // 比较值（对象指针）
        if (int result = vsg::compare_pointer(lhs_itr->second, rhs_itr->second); result != 0) return result;
        ++lhs_itr;
        ++rhs_itr;
    }

    // 只有当一个迭代器到达末尾时才能到达这里
    if (lhs_itr == userObjects.end())
    {
        // 如果左侧已结束，右侧还有元素则左侧更小，否则相等
        if (rhs_itr != rhs.userObjects.end())
            return -1;
        else
            return 0;
    }
    else
    {
        // 左侧还有元素，右侧已结束，则左侧更大
        return 1;
    }
}
