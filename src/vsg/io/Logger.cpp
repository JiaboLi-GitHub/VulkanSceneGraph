/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Options.h>

#include <iostream>

using namespace vsg;

namespace vsg
{

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // intercept_streambuf - 拦截标准输出流缓冲区，将std::cout/cerr输出重定向到Logger
    //

    // 拦截流缓冲区类：拦截标准输出流并将输出重定向到日志记录器
    class intercept_streambuf : public std::streambuf
    {
    public:
        // 构造函数：创建拦截流缓冲区
        // in_logger: 日志记录器对象
        // in_level: 日志级别
        explicit intercept_streambuf(Logger* in_logger, Logger::Level in_level) :
            logger(in_logger),
            level(in_level)
        {
        }

        Logger* logger = nullptr;
        Logger::Level level = Logger::LOGGER_INFO;

        // 写入字符序列
        // s: 字符序列
        // n: 字符数量
        // 返回: 写入的字符数量
        // 将字符追加到当前行，等待换行符
        std::streamsize xsputn(const char_type* s, std::streamsize n) override
        {
            std::scoped_lock<std::mutex> lock(_mutex);
            _line.append(s, static_cast<std::size_t>(n));
            return n;
        }

        // 溢出处理（当缓冲区满时调用）
        // c: 要写入的字符
        // 返回: 字符值
        // 如果遇到换行符，将当前行发送到日志记录器
        std::streambuf::int_type overflow(std::streambuf::int_type c) override
        {
            std::scoped_lock<std::mutex> lock(_mutex);
            if (c == '\n')
            {
                logger->log(level, _line);
                _line.clear();
            }
            else
            {
                _line.push_back(static_cast<char>(c));
            }
            return c;
        }

    protected:
        std::string _line;  // 当前行缓冲区
        std::mutex _mutex;  // 互斥锁（用于线程安全）
    };

} // namespace vsg

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Logger - 日志记录器基类，提供统一的日志记录接口
//
// 构造函数：创建日志记录器对象（默认）
// 默认日志级别为LOGGER_INFO（打印信息和以上级别的消息）
Logger::Logger()
{
    // level = LOGGER_ALL; // 打印所有消息
    // level = LOGGER_DEBUG; // 打印调试及以上的消息
    // level = LOGGER_INFO; // 默认，打印信息及以上的消息
    // level = LOGGER_WARN; // 打印警告及以上的消息
    // level = LOGGER_ERROR; // 打印错误及以上的消息
    // level = LOGGER_FATAL; // 打印致命错误及以上的消息
}

// 拷贝构造函数：从另一个日志记录器对象创建新的日志记录器对象
// rhs: 要拷贝的日志记录器对象
// 拷贝日志级别
Logger::Logger(const Logger& rhs) :
    Logger()
{
    level = rhs.level;
}

// 析构函数：销毁日志记录器对象
// 恢复标准输出流的原始缓冲区
Logger::~Logger()
{
    if (_original_cout) std::cout.rdbuf(_original_cout);
    if (_original_cerr) std::cerr.rdbuf(_original_cerr);
}

// 获取日志记录器单例实例
// 返回: 日志记录器实例的引用
// 默认使用标准日志记录器（StdLogger），也可以使用线程日志记录器（ThreadLogger）
ref_ptr<Logger>& Logger::instance()
{
    static ref_ptr<Logger> s_logger = StdLogger::create();
    //static ref_ptr<Logger> s_logger = ThreadLogger::create();
    return s_logger;
}

// 重定向标准输出流
// 将std::cout和std::cerr的输出重定向到日志记录器
// std::cout重定向到INFO级别，std::cerr重定向到ERROR级别
void Logger::redirect_std()
{
    _override_cout.reset(new intercept_streambuf(this, LOGGER_INFO));
    _original_cout = std::cout.rdbuf(_override_cout.get());

    _override_cerr.reset(new intercept_streambuf(this, LOGGER_ERROR));
    _original_cerr = std::cerr.rdbuf(_override_cerr.get());
}

void Logger::debug_stream(PrintToStreamFunction print)
{
    if (level > LOGGER_DEBUG) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    debug_implementation(_stream.str());
}

void Logger::info_stream(PrintToStreamFunction print)
{
    if (level > LOGGER_INFO) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    info_implementation(_stream.str());
}

void Logger::warn_stream(PrintToStreamFunction print)
{
    if (level > LOGGER_WARN) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    warn_implementation(_stream.str());
}

void Logger::error_stream(PrintToStreamFunction print)
{
    if (level > LOGGER_ERROR) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    error_implementation(_stream.str());
}

void Logger::fatal_stream(PrintToStreamFunction print)
{
    if (level > LOGGER_FATAL) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    fatal_implementation(_stream.str());
}

void Logger::log(Level msg_level, const std::string_view& message)
{
    if (level > msg_level) return;
    std::scoped_lock<std::mutex> lock(_mutex);

    switch (msg_level)
    {
    case (LOGGER_DEBUG): debug_implementation(message); break;
    case (LOGGER_INFO): info_implementation(message); break;
    case (LOGGER_WARN): warn_implementation(message); break;
    case (LOGGER_ERROR): error_implementation(message); break;
    case (LOGGER_FATAL): fatal_implementation(message); break;
    default: break;
    }
}

void Logger::log_stream(Level msg_level, PrintToStreamFunction print)
{
    if (level > msg_level) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _stream.str({});
    _stream.clear();

    print(_stream);

    switch (msg_level)
    {
    case (LOGGER_DEBUG): debug_implementation(_stream.str()); break;
    case (LOGGER_INFO): info_implementation(_stream.str()); break;
    case (LOGGER_WARN): warn_implementation(_stream.str()); break;
    case (LOGGER_ERROR): error_implementation(_stream.str()); break;
    case (LOGGER_FATAL): fatal_implementation(_stream.str()); break;
    default: break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StdLogger - 标准日志记录器，将日志输出到标准输出/错误流
//
// 构造函数：创建标准日志记录器对象
StdLogger::StdLogger()
{
}

// 刷新输出流
// 刷新标准输出和标准错误流
void StdLogger::flush()
{
    fflush(stdout);
    fflush(stderr);
}

// 调试消息实现
// message: 消息内容
// 将调试消息输出到标准输出流
void StdLogger::debug_implementation(const std::string_view& message)
{
    fprintf(stdout, "%s%.*s\n", debugPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

// 信息消息实现
// message: 消息内容
// 将信息消息输出到标准输出流
void StdLogger::info_implementation(const std::string_view& message)
{
    fprintf(stdout, "%s%.*s\n", infoPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

// 警告消息实现
// message: 消息内容
// 将警告消息输出到标准错误流
void StdLogger::warn_implementation(const std::string_view& message)
{
    fprintf(stderr, "%s%.*s\n", warnPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

// 错误消息实现
// message: 消息内容
// 将错误消息输出到标准错误流
void StdLogger::error_implementation(const std::string_view& message)
{
    fprintf(stderr, "%s%.*s\n", errorPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

// 致命错误消息实现
// message: 消息内容
// 将致命错误消息输出到标准错误流并抛出异常
void StdLogger::fatal_implementation(const std::string_view& message)
{
    fprintf(stderr, "%s%.*s\n", fatalPrefix.c_str(), static_cast<int>(message.length()), message.data());
    throw vsg::Exception{std::string(message)};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ThreadLogger - 线程日志记录器，在日志消息中包含线程ID信息
//
// 构造函数：创建线程日志记录器对象
// 线程日志记录器在每条日志消息前添加线程标识符，便于多线程调试
ThreadLogger::ThreadLogger()
{
}

// 刷新输出流
// 刷新标准输出和标准错误流
void ThreadLogger::flush()
{
    fflush(stdout);
    fflush(stderr);
}

// 设置线程前缀
// id: 线程ID
// str: 线程前缀字符串
// 为指定线程设置自定义前缀字符串（用于标识线程）
void ThreadLogger::setThreadPrefix(std::thread::id id, const std::string& str)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    _threadPrefixes[id] = str;
}

// 打印线程ID
// out: 输出文件流
// id: 线程ID
// 打印线程标识符（如果已设置自定义前缀则使用前缀，否则使用线程ID）
void ThreadLogger::print_id(FILE* out, std::thread::id id)
{
    if (auto itr = _threadPrefixes.find(id); itr != _threadPrefixes.end())
    {
        fprintf(out, "%s", itr->second.c_str());
    }
    else
    {
        // 如果此线程还没有名称字符串，使用Logger::_stream创建一个，然后分配给_threadPrefixes以供将来使用
        _stream.str({});
        _stream.clear();
        _stream << "thread::id = " << id << " | ";
        const auto& str = _threadPrefixes[id] = _stream.str();
        fprintf(out, "%s", str.c_str());
    }
}

void ThreadLogger::debug_implementation(const std::string_view& message)
{
    print_id(stdout, std::this_thread::get_id());
    fprintf(stdout, "%s%.*s\n", debugPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

void ThreadLogger::info_implementation(const std::string_view& message)
{
    print_id(stdout, std::this_thread::get_id());
    fprintf(stdout, "%s%.*s\n", infoPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

void ThreadLogger::warn_implementation(const std::string_view& message)
{
    print_id(stderr, std::this_thread::get_id());
    fprintf(stderr, "%s%.*s\n", warnPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

void ThreadLogger::error_implementation(const std::string_view& message)
{
    print_id(stderr, std::this_thread::get_id());
    fprintf(stderr, "%s%.*s\n", errorPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

void ThreadLogger::fatal_implementation(const std::string_view& message)
{
    print_id(stderr, std::this_thread::get_id());
    fprintf(stderr, "%s%.*s\n", fatalPrefix.c_str(), static_cast<int>(message.length()), message.data());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// NullLogger - 空日志记录器，不输出任何日志消息（致命错误除外）
//
// 构造函数：创建空日志记录器对象
// 空日志记录器不输出任何日志消息，但致命错误仍会抛出异常
NullLogger::NullLogger()
{
    level = LOGGER_OFF;
}

// 调试消息实现（空实现）
// 不输出任何内容
void NullLogger::debug_implementation(const std::string_view&)
{
}

// 信息消息实现（空实现）
// 不输出任何内容
void NullLogger::info_implementation(const std::string_view&)
{
}

// 警告消息实现（空实现）
// 不输出任何内容
void NullLogger::warn_implementation(const std::string_view&)
{
}

// 错误消息实现（空实现）
// 不输出任何内容
void NullLogger::error_implementation(const std::string_view&)
{
}

// 致命错误消息实现
// message: 消息内容
// 致命错误仍会抛出异常，即使日志级别为OFF
void NullLogger::fatal_implementation(const std::string_view& message)
{
    throw Exception{std::string(message)};
}
