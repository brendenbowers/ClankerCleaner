// ESP-IDF's log goes through esp_log_set_vprintf, from any task. spdlog's console sink writes to
// the stdout it saw when it was created, which is the main task's, so that stdout is swapped for
// one that copies into the buffer before passing the text on to the console.
//
// Writers only copy into the buffer and notify the readers; they never touch a socket, since
// lwIP's own task logs too.

#include "Logging/LogCapture.ESP32.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>

#include <esp_heap_caps.h>
#include <esp_log.h>

namespace ClankerCleaner::Logging::Capture
{
    namespace
    {
        constexpr std::size_t BufferSize = 16 * 1024;
        constexpr std::size_t MaxReaders = 4;
        // Longer ESP-IDF lines are cut short in the copy; the console still gets them whole.
        constexpr std::size_t MaxEspLogLine = 384;

        char* buffer = nullptr;
        Position head = 0;
        // A spinlock rather than a mutex: any task can log, and the copies are short.
        portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
        std::array<TaskHandle_t, MaxReaders> readers{};
        FILE* console = nullptr;

        void Append(const char* data, std::size_t size)
        {
            if(!buffer || size == 0)
            {
                return;
            }

            std::array<TaskHandle_t, MaxReaders> toNotify;
            portENTER_CRITICAL(&lock);
            if(size > BufferSize)
            {
                head += size - BufferSize;
                data += size - BufferSize;
                size = BufferSize;
            }
            const std::size_t start = head % BufferSize;
            const std::size_t first = std::min(size, BufferSize - start);
            std::copy(data, data + first, buffer + start);
            std::copy(data + first, data + size, buffer);
            head += size;
            toNotify = readers;
            portEXIT_CRITICAL(&lock);

            for(TaskHandle_t reader : toNotify)
            {
                if(reader)
                {
                    xTaskNotifyGive(reader);
                }
            }
        }

        int OnEspLog(const char* format, va_list arguments)
        {
            va_list copy;
            va_copy(copy, arguments);
            char line[MaxEspLogLine];
            const int length = std::vsnprintf(line, sizeof(line), format, copy);
            va_end(copy);
            if(length > 0)
            {
                Append(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1));
            }
            // Straight to the console: the main task's stdout would copy it a second time.
            return std::vfprintf(console, format, arguments);
        }

        ssize_t WriteStdout(void* /*cookie*/, const char* data, std::size_t size)
        {
            Append(data, size);
            std::fwrite(data, 1, size, console);
            std::fflush(console);
            return static_cast<ssize_t>(size);
        }
    }

    void Start()
    {
        buffer = static_cast<char*>(heap_caps_malloc(BufferSize, MALLOC_CAP_SPIRAM));
        if(!buffer)
        {
            buffer = static_cast<char*>(heap_caps_malloc(BufferSize, MALLOC_CAP_8BIT));
        }
        if(!buffer)
        {
            return;
        }

        console = stdout;
        cookie_io_functions_t functions{};
        functions.write = WriteStdout;
        if(FILE* copying = fopencookie(nullptr, "w", functions))
        {
            std::setvbuf(copying, nullptr, _IOLBF, 256);
            stdout = copying;
        }
        esp_log_set_vprintf(OnEspLog);
    }

    Position Oldest()
    {
        portENTER_CRITICAL(&lock);
        Position position = head > BufferSize ? head - BufferSize : 0;
        if(position > 0)
        {
            // Skip the partial line the buffer starts in.
            while(position < head && buffer[position % BufferSize] != '\n')
            {
                ++position;
            }
            position = std::min(position + 1, head);
        }
        portEXIT_CRITICAL(&lock);
        return position;
    }

    std::size_t Read(Position& position, char* out, std::size_t size, bool& dropped)
    {
        portENTER_CRITICAL(&lock);
        dropped = head - position > BufferSize;
        if(dropped)
        {
            position = head - BufferSize;
        }
        const std::size_t count = static_cast<std::size_t>(std::min<Position>(size, head - position));
        const std::size_t start = position % BufferSize;
        const std::size_t first = std::min(count, BufferSize - start);
        std::copy(buffer + start, buffer + start + first, out);
        std::copy(buffer, buffer + (count - first), out + first);
        position += count;
        portEXIT_CRITICAL(&lock);
        return count;
    }

    bool AddReader(TaskHandle_t task)
    {
        bool added = false;
        portENTER_CRITICAL(&lock);
        if(buffer)
        {
            auto slot = std::find(readers.begin(), readers.end(), nullptr);
            if(slot != readers.end())
            {
                *slot = task;
                added = true;
            }
        }
        portEXIT_CRITICAL(&lock);
        return added;
    }

    void RemoveReader(TaskHandle_t task)
    {
        portENTER_CRITICAL(&lock);
        std::replace(readers.begin(), readers.end(), task, static_cast<TaskHandle_t>(nullptr));
        portEXIT_CRITICAL(&lock);
    }
}
