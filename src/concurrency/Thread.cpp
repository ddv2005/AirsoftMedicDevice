#include "Thread.h"
#include "timing.h"

namespace concurrency {

void Thread::start(const char *name, size_t stackSize, uint32_t priority, int8_t core)
{
    if(core<0)
    {
        auto r = xTaskCreate(callRun, name, stackSize, this, priority, &taskHandle);
        assert(r == pdPASS);
    }
    else
    {
        auto r = xTaskCreatePinnedToCore(callRun, name, stackSize, this, priority, &taskHandle,core);
        assert(r == pdPASS);
    }
}

void Thread::callRun(void *_this)
{
    ((Thread *)_this)->doRun();
}

} // namespace concurrency
