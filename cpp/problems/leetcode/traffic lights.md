# Traffic lights

Only one road can be used at a time. Each car arrived call will give you the needed
stuff, avoid unnecessary changes.

```cpp
#include <mutex>
using namespace std;

class TrafficLight {
private:
    mutex mtx;
    int greenRoad = 1;

public:
    TrafficLight() {
    }

    void carArrived(
        int carId,
        int roadId,
        int direction,
        function<void()> turnGreen,
        function<void()> crossCar
    ) {
        lock_guard<mutex> lock(mtx);

        if (greenRoad != roadId) {
            turnGreen();
            greenRoad = roadId;
        }

        crossCar();
    }
};
```
