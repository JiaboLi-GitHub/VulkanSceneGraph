/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/MemorySlots.h>
#include <vsg/io/Logger.h>

#include <algorithm>

using namespace vsg;

///////////////////////////////////////////////////////////////////////////////
//
// MemorySlots - 内存槽管理器，用于管理固定大小内存块的分配和释放
//

// MemorySlots类的构造函数
// 初始化内存槽管理器，创建初始可用内存槽
// availableMemorySize: 可用内存总大小
// in_memoryTracking: 内存跟踪标志
MemorySlots::MemorySlots(size_t availableMemorySize, int in_memoryTracking) :
    memoryTracking(in_memoryTracking)
{
    if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
    {
        info("MemorySlots::MemorySlots(", availableMemorySize, ") ", this);
    }

    // 插入初始可用内存槽，从偏移0开始，大小为全部可用内存
    insertAvailableSlot(0, availableMemorySize);

    _totalMemorySize = availableMemorySize;
}

// MemorySlots类的析构函数
// 检查所有内存槽是否正确恢复
MemorySlots::~MemorySlots()
{
    if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
    {
        // 如果只有一个可用内存槽，说明所有内存都已正确恢复
        if (_availableMemory.size() == 1)
        {
            info("MemorySlots::~MemorySlots() ", this, ", all slots restored correctly.");
        }
        else
        {
            info("MemorySlots::~MemorySlots() ", this, ", not all slots restored correctly.");
            info_stream([&](auto& fout) { report(fout); });
        }
    }
    if (memoryTracking & MEMORY_TRACKING_CHECK_ACTIONS)
    {
        check();
    }
}

// 计算总可用内存大小
// 返回所有可用内存槽的总大小
size_t MemorySlots::totalAvailableSize() const
{
    size_t totalSize = 0;
    for (const auto& sizeOffset : _availableMemory)
    {
        totalSize += sizeOffset.first;
    }
    return totalSize;
}

// 计算总保留内存大小
// 返回所有保留内存槽的总大小
size_t MemorySlots::totalReservedSize() const
{
    size_t totalSize = 0;
    for (const auto& sizeOffset : _reservedMemory)
    {
        totalSize += sizeOffset.second;
    }
    return totalSize;
}

// 检查内存槽的一致性
// 验证可用内存和保留内存的总和是否等于总内存大小
// 返回值：true表示一致，false表示不一致
bool MemorySlots::check() const
{
    // 检查可用内存映射和偏移大小映射的大小是否一致
    if (_availableMemory.size() != _offsetSizes.size())
    {
        warn("MemorySlots::check() _availableMemory.size() ", _availableMemory.size(), " != _offsetSizes.size() ", _offsetSizes.size());
    }

    // 计算可用内存总大小
    size_t availableSize = 0;
    for (const auto& offsetSize : _offsetSizes)
    {
        availableSize += offsetSize.second;
    }

    // 计算保留内存总大小
    size_t reservedSize = 0;
    for (const auto& offsetSize : _reservedMemory)
    {
        reservedSize += offsetSize.second;
    }

    // 验证总和是否等于总内存大小
    size_t computedSize = availableSize + reservedSize;
    if (computedSize != _totalMemorySize)
    {
        warn("MemorySlots::check() ", this, " failed, computedSize (", computedSize, ") != _totalMemorySize (", _totalMemorySize, ")");
        warn_stream([&](auto& fout) { report(fout); });

        return false;
    }

    return true;
}

// 报告内存槽状态
// 输出所有可用和保留的内存槽信息到输出流
void MemorySlots::report(std::ostream& out) const
{
    out << "MemorySlots::report() " << this << std::endl;
    // 输出所有可用内存槽
    for (auto& [offset, size] : _offsetSizes)
    {
        out << "    available " << offset << ", " << size << std::endl;
    }

    // 输出所有保留内存槽
    for (auto& [offset, size] : _reservedMemory)
    {
        out << "    reserved " << std::dec << offset << ", " << size << std::endl;
    }
}

// 插入可用内存槽
// 将指定偏移和大小的内存槽添加到可用内存映射中
// offset: 内存偏移
// size: 内存大小
void MemorySlots::insertAvailableSlot(size_t offset, size_t size)
{
    _offsetSizes.emplace(offset, size);
    _availableMemory.emplace(size, offset);
}

// 移除可用内存槽
// 从可用内存映射中移除指定偏移和大小的内存槽
// offset: 内存偏移
// size: 内存大小
void MemorySlots::removeAvailableSlot(size_t offset, size_t size)
{
    _offsetSizes.erase(offset);
    // 在可用内存映射中查找并移除匹配的条目
    auto end = _availableMemory.upper_bound(size);
    for (auto itr = _availableMemory.lower_bound(size); itr != end; ++itr)
    {
        if (itr->second == offset)
        {
            _availableMemory.erase(itr);
            break;
        }
    }
}

// 保留内存槽
// 从可用内存中分配指定大小和对齐要求的内存槽
// size: 需要的内存大小
// alignment: 内存对齐要求
// 返回值：OptionalOffset，包含是否成功和分配的偏移
MemorySlots::OptionalOffset MemorySlots::reserve(size_t size, size_t alignment)
{
    if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
    {
        info("\nMemorySlots::reserve(", size, ", ", alignment, ") ", this);
    }

    // 如果内存已满，返回失败
    if (full()) return OptionalOffset(false, 0);

    // 查找第一个大小大于等于所需大小的可用内存槽
    auto itr = _availableMemory.lower_bound(size);
    while (itr != _availableMemory.end())
    {
        size_t slotSize = itr->first;
        size_t slotStart = itr->second;
        size_t slotEnd = slotStart + slotSize;
        // 计算对齐后的起始位置
        size_t alignedStart = ((slotStart + alignment - 1) / alignment) * alignment;
        size_t alignedEnd = alignedStart + size;
        // 检查对齐后的槽是否足够大
        if (alignedEnd <= slotEnd) // slot big enough
        {
            // 移除可用内存槽
            removeAvailableSlot(slotStart, slotSize);

            // 如果对齐前有剩余空间，将其作为新的可用槽
            if (slotStart < alignedStart) // space before newly reserved slot
            {
                insertAvailableSlot(slotStart, alignedStart - slotStart);
            }

            // 如果对齐后还有剩余空间，将其作为新的可用槽
            if (alignedEnd < slotEnd) // space after newly reserved slot
            {
                slotStart = alignedEnd;
                insertAvailableSlot(slotStart, slotEnd - slotStart);
            }

            // 记录并返回保留的内存槽
            _reservedMemory.emplace(alignedStart, size);

            if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
            {
                info("MemorySlots::reserve(", size, ", ", alignment, ") ", this, " allocated [", alignedStart, ", ", size, "]");
            }

            if (memoryTracking & MEMORY_TRACKING_CHECK_ACTIONS) check();

            return {true, alignedStart};
        }
        else // slot not big enough so advance to the next slot
        {
            ++itr;
        }
    }

    if (memoryTracking & MEMORY_TRACKING_CHECK_ACTIONS) check();

    if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
    {
        info("MemorySlots::reserve(", size, ", ", alignment, ") ", this, " no suitable slots found");
    }
    return {false, 0};
}

// 释放内存槽
// 将保留的内存槽释放回可用内存池，并尝试与相邻的可用槽合并
// offset: 要释放的内存偏移
// size: 要释放的内存大小
// 返回值：true表示成功，false表示失败
bool MemorySlots::release(size_t offset, size_t size)
{
    if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
    {
        info("\nMemorySlots::release(", offset, ", ", size, ") ", this);
    }

    // 查找保留的内存槽
    auto itr = _reservedMemory.find(offset);
    if (itr == _reservedMemory.end())
    {
        // 条目不在保留槽中
        return false;
    }

    // 如果大小不匹配，使用记录的大小
    if (size != itr->second)
    {
        if (memoryTracking & MEMORY_TRACKING_REPORT_ACTIONS)
        {
            info("    reserved slot different size = ", size, ", itr->second = ", itr->second);
        }

        size = itr->second;
    }

    // 从保留列表中移除
    _reservedMemory.erase(itr);

    // 如果没有可用槽，直接插入
    if (_offsetSizes.empty())
    {
        insertAvailableSlot(offset, size);

        if (memoryTracking & MEMORY_TRACKING_CHECK_ACTIONS) check();

        return true;
    }

    size_t slotStart = offset;
    size_t slotEnd = offset + size;

    // 查找下一个可用槽
    auto next_slot_itr = _offsetSizes.lower_bound(slotStart);
    if (next_slot_itr != _offsetSizes.end())
    {
        // 检查前一个槽是否可以合并
        if (next_slot_itr != _offsetSizes.begin())
        {
            auto prev_slot_itr = next_slot_itr;
            --prev_slot_itr;

            size_t prev_slotEnd = prev_slot_itr->first + prev_slot_itr->second;
            if (prev_slotEnd == slotStart)
            {
                // 前一个槽与要释放的槽相邻，合并它们
                slotStart = prev_slot_itr->first;
                removeAvailableSlot(prev_slot_itr->first, prev_slot_itr->second);
            }
        }

        // 检查下一个槽是否可以合并
        if (next_slot_itr->first == slotEnd)
        {
            // 下一个可用槽与释放的槽相邻，扩展新槽并移除下一个可用槽
            slotEnd = next_slot_itr->first + next_slot_itr->second;
            removeAvailableSlot(next_slot_itr->first, next_slot_itr->second);
        }
    }
    else
    {
        // 没有下一个槽，检查最后一个槽是否可以合并
        auto prev_slot_itr = _offsetSizes.rbegin();
        size_t prev_slotEnd = prev_slot_itr->first + prev_slot_itr->second;
        if (prev_slotEnd == slotStart)
        {
            // 前一个槽与要释放的槽相邻，合并它们
            slotStart = prev_slot_itr->first;
            removeAvailableSlot(prev_slot_itr->first, prev_slot_itr->second);
        }
    }

    // 插入合并后的可用槽
    insertAvailableSlot(slotStart, slotEnd - slotStart);

    if (memoryTracking & MEMORY_TRACKING_CHECK_ACTIONS) check();

    return true;
}
