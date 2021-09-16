#include "monitor.h"
#include <unistd.h>
#include <errno.h>
namespace event_engine
{
    bool Monitor::Start(std::string &err)
    {
        stop_fd_ = eventfd(0, 0);
        done_fd_ = eventfd(0, 0);

        /* do some init */

        std::thread t(&Monitor::RunLoop, this);
        t.detach();
        return true;
    }

    void Monitor::RunLoop(void)
    {
        while (true)
        {
            int n = poll(poll_fd_, 1, -1);
            if (poll_fd_[0].events & POLLIN)
            {
                /* stop */
                break;
            }
        }
        delete[] poll_fd_;
        /* do some clear */
        eventfd_write(done_fd_, 1);
    }

    void Monitor::Stop()
    {
        eventfd_write(stop_fd_, 20);
        eventfd_t value;
        eventfd_read(done_fd_, &value);
        return;
    }
}