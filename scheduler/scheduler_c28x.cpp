#include "scheduler_c28x.h"


namespace emb {


namespace scheduler {


bool basic_scheduler::_initialized = false;
emb::static_vector<basic_scheduler::task, basic_scheduler::max_taskcount> basic_scheduler::_tasks;
emb::chrono::milliseconds basic_scheduler::_delayed_task_start = emb::chrono::milliseconds(0);
emb::chrono::milliseconds basic_scheduler::_delayed_task_delay = emb::chrono::milliseconds(0);
void (*basic_scheduler::_delayed_task)() = basic_scheduler::empty_delayed_task;


} // namespace scheduler


} // namespace emb
