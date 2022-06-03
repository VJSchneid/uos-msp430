#include <uos/detail/task_list.hpp>
#include <uos/device/msp430/scheduler.hpp>

namespace uos::dev::msp430 {
  
template<typename TaskData>
using task_list = ::uos::task_list<TaskData, scheduler>;

}