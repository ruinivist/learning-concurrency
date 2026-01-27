#include <barrier>

std::barrier sync_point(3);

void worker() { sync_point.arrive_and_wait(); }