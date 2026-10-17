# Web crawler

Problem: https://leetcode.doocs.org/en/lc/1242/

Return all links crawled under the same "host" from a start url.
No duplicate crawling.

## Solution

```cpp

#include <vector>
#include <string>
#include <queue>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

class Solution {
    string getHost(const string& url) {
        // "http://news.google.com/articles/123";
        // find the :// and then the next / after that
        int start = url.find("://") + 3;
        int end = url.find('/', start);
        return url.substr(start, end - start);
    }

public:
    vector<string> crawl(string startUrl, HtmlParser htmlParser) {
        string host = getHost(startUrl);

        queue<string> q;
        unordered_set<string> visited;

        mutex mtx;
        condition_variable cv;

        // pending = how many in q + still being processed
        int pending = 1;

        q.push(startUrl);
        visited.insert(startUrl);

        auto worker = [&]() {
            while (true) {
                string url;

                // pop from queue under a lock
                {
                    unique_lock<mutex> lock(mtx);

                    cv.wait(lock, [&]() {
                        return pending == 0 || !q.empty();
                    });

                    if (pending == 0)
                        return;

                    url = q.front();
                    q.pop();
                }

                // Fetch links without holding the mutex
                vector<string> links = htmlParser.getUrls(url);

                {
                    lock_guard<mutex> lock(mtx);

                    for (const string& next : links) {
                        if (getHost(next) == host &&
                            visited.insert(next).second) {
                            q.push(next);
                            ++pending;
                        }
                    }

                    --pending;
                }

                cv.notify_all();
            }
        };

        vector<thread> threads;

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back(worker);
        }

        for (auto& t : threads) {
            t.join();
        }

        return vector<string>(visited.begin(), visited.end());
    }
};
```
