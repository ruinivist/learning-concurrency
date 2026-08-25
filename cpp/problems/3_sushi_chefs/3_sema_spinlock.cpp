/*
This is trivial as I can just replace this
std::unique_lock<std::mutex> lck(mtx_);
with
std::lock_guard<Spinlock> guard(lock);
so I'll skip this
*/