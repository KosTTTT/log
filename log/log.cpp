#include "log.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <queue>
#include <memory>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace l;
static char8_t const REL_LOG_DIR[] = u8"./Log/";
static char8_t const LOG_FILE_NAME[] = u8"log.txt";
static void hm1(std::string const & message,
                std::u8string const & fileName, struct LoIn & ret);

struct LoIn
{
    bool log_error;
    std::thread::id thread_id;
    boost::interprocess::ipcdetail::OS_process_id_t proc_id;
    std::u8string file_name;
    std::string message;
};
class LogThread
{
    friend void l::exit();
public:
    LogThread();
    ~LogThread();
    void putInfo(LoIn && info);
private:
    /**
     * @brief mark working thread for stop. It will quit when all logs have written
     */
    void stop();
    void handle_data(LoIn && info);
    void run();
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<LoIn> m_q;
    bool m_work;
    std::jthread m_thread;
};
static std::once_flag g_flag_init;
static std::once_flag g_flag_deinit;
static std::unique_ptr<LogThread> g_log_thread;

inline
LogThread::LogThread():
    m_work(true),
    m_thread(&LogThread::run, this)
{}
inline
LogThread::~LogThread()
{
    stop();
}
inline void LogThread::putInfo(LoIn && info)
{
    {
        std::lock_guard<std::mutex> const l(m_mutex);
        if(m_work == false)
            return;
        m_q.emplace(std::move(info));
    }
    m_cv.notify_all();
}

void LogThread::run()
{
    try
    {
        while(true)
        {
            std::unique_lock<std::mutex> l(m_mutex);
            m_cv.wait(l, [&]() -> bool
            {
                return !m_q.empty() || !m_work;
            });
            if(m_work == false && m_q.empty())
            {
                break;
            }
            if(m_q.empty() == false)
            {
                LoIn dat = std::move(m_q.front());
                m_q.pop();
                l.unlock();
                handle_data(std::move(dat));
            }
        }
    }
    catch (...)
    {}
}
inline void LogThread::stop()
{
    {
        std::lock_guard<std::mutex> const l(m_mutex);
        m_work =  false;
    }
    m_cv.notify_all();
}

void l::Log(std::string const & message, std::u8string const & fileName)
{
    assert(g_log_thread);
    LoIn in;
    hm1(message, fileName, in);
    in.log_error = false;
    g_log_thread->putInfo(std::move(in));
}

void l::Log(std::string const & message)
{
    Log(message, LOG_FILE_NAME);
}
void l::LogEr(std::string const & message)
{
    LogEr(message, LOG_FILE_NAME);
}
void l::LogEr(std::string const & message, std::u8string const & fileName)
{
    assert(g_log_thread);
    LoIn in;
    hm1(message, fileName, in);
    in.log_error = true;
    g_log_thread->putInfo(std::move(in));
}
void l::exit()
{
    assert(g_log_thread);
    std::call_once(g_flag_deinit, [&]()
    {
        g_log_thread->stop();
        if(g_log_thread->m_thread.joinable())
            g_log_thread->m_thread.join();
    });
}
void l::init()
{
    std::call_once(g_flag_init, [&]()
    {
        std::filesystem::path const REL_DIR = std::filesystem::u8path(REL_LOG_DIR).make_preferred();
        if(std::filesystem::exists(REL_DIR) == false)
        {
            std::filesystem::create_directory(REL_DIR);
        }
        g_log_thread.reset(new LogThread{});
    });
}
void LogThread::handle_data(LoIn && info)
{
    //**inhance a message
    std::string all_mess;
    {
        std::string th_id;
        {
            std::ostringstream ss;
            ss<<info.thread_id;
            th_id = ss.str();
        }
        std::string proc_id;
        {
            std::ostringstream ss;
            ss<<info.proc_id;
            proc_id = ss.str();
        }
        std::string time;
        {
            std::ostringstream ss;
            std::time_t t = std::time(nullptr);
            std::tm tm = *std::localtime(&t);
            ss << std::put_time(&tm, "%H:%M:%S %d/%m/%Y");
            time = ss.str();
        }
        if(info.log_error)
        {
            all_mess+="error ";
        }
        all_mess+=time +" "+ proc_id+ " " + th_id+ " " + info.message;
    }
    //**
    std::filesystem::path const LOG_FILE = std::filesystem::u8path(REL_LOG_DIR + info.file_name).make_preferred();
    std::ofstream file(LOG_FILE, std::ios::out | std::ios::app);
    boost::interprocess::file_lock fl(LOG_FILE.native().c_str());
    {
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> const e_lock(fl);
        file<<all_mess<<std::endl;
#ifndef NDEBUG
        std::cout<<all_mess<<std::endl;
#endif
    }
}
inline void hm1(std::string const & message,
                std::u8string const & fileName, struct LoIn & in)
{
    in.thread_id = std::this_thread::get_id();
    in.proc_id = boost::interprocess::ipcdetail::get_current_process_id();
    in.file_name = fileName;
    in.message = message;
}
