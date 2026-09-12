#include <chrono>
#include <deque>
#include <memory>
#include <print>
#include <semaphore>
#include <thread>
#include <vector>

const int MAX_CUSTOMERS = 20;  // fire code limit
const int SOFA_CAPACITY = 4;
const int N_BARBERS = 3;

int n_customers = 0;

// mutexes
std::binary_semaphore global_mtx{1};  // protects n_customers and standing_queue
std::binary_semaphore sofa_mtx{1};    // protects sofa_queue
std::binary_semaphore register_mtx{1};  // 1 cash register

// multiplex for the 4 sofa seats
std::counting_semaphore<SOFA_CAPACITY> has_space_on_sofa{SOFA_CAPACITY};

// signals
std::counting_semaphore<> customer_standing{0};
std::counting_semaphore<> customer_on_sofa{0};
std::binary_semaphore payment_ready{0};
std::binary_semaphore receipt_ready{0};

// FIFO queues for store per-customer semaphores
std::deque<std::shared_ptr<std::binary_semaphore>> standing_queue;
std::deque<std::shared_ptr<std::binary_semaphore>> sofa_queue;

// little helper
inline void while_held(std::binary_semaphore& sema, auto callable) {
    sema.acquire();
    callable();
    sema.release();
}

void barber(int id) {
    while (1) {
        // wait for customer from sofa, pop, and signal chair ready
        customer_on_sofa.acquire();

        // NOW we do the 2 way rendezous

        // parallelise it by using state specific mutexes here
        // instead of global
        using sema = decltype(sofa_queue)::value_type;
        // little hack to avoid typing
        sema next_cust;
        while_held(sofa_mtx, [&] {
            next_cust = sofa_queue.front();
            sofa_queue.pop_front();
            next_cust->release();
            // ^ this just tell the customer to move to barber chair
            // we need to now wait for it as well
            next_cust->acquire();
        });

        std::println("barber {} ready for cut", id);

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        next_cust->release();

        // INITIALLY I reused the same ticket again but that binds the barber
        // customer all the way to payment + turns it into STRICT alternating
        // ping pong of the same sema; what I have now allows for any barber
        // to take payment from any customer

        // next_cust->acquire();  // wait for cust to stand up
        // basically we NEED to break the double consecutive release that
        // would've happened otherwise, so we need to sync both threads again
        // note that if you're reusing like this, or even if not

        // accept payment from any customer at the cash register
        payment_ready.acquire();
        std::println("barber {} accepted payment", id);
        receipt_ready.release();
    }
}

// ushers move standing to sofa, if we leave it to barbers and all 3 are
// occupied standing ones will remain standing even when sofa is empty Why can't
// they do it themselves? FIFO, you NEED a serialiser third party to do the FIFO
void usher(int id) {
    while (1) {
        customer_standing.acquire();  // some cust is standing
        has_space_on_sofa.acquire();  // got a ticket for seating on sofa
        while_held(global_mtx, [&] {
            auto front = standing_queue.front();
            standing_queue.pop_front();
            front->release();  // the inidividual fifo signal
            // THIS IS WRONG AT THIS STAGE
            // and is why the whole 2 way is needed
            // so far this would've just signalled customer to sit
            // but the cutomer might not have added him to the sofa q yet
            // customer_on_sofa.release();  // the global signal

            // reusing the customer sema
            // the cust WILL release again once he sits
            front->acquire();
        });
    }
}

void customer(int id) {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds((id + 1) * 150));
        std::println("customer {} arrived", id);

        // 1. enterShop() - check MAX_CUSTOMERS, balk if full, else add to
        // standing_queue
        bool balk = false;
        auto sofa_ticket = std::make_shared<std::binary_semaphore>(0);
        while_held(global_mtx, [&] {
            balk = n_customers == MAX_CUSTOMERS;
            if (!balk) {
                n_customers++;
                standing_queue.push_back(sofa_ticket);
                customer_standing.release();
            }
        });
        if (balk) {
            std::println("customer {} balked", id);
            continue;
        }

        // 2. sitOnSofa() - wait for sofa spot, usher will do the signalling
        sofa_ticket->acquire();
        // the same all over again but with barber and customer than user and
        // customer
        auto cut_ticket = std::make_shared<std::binary_semaphore>(0);
        while_held(sofa_mtx, [&] { sofa_queue.push_back(cut_ticket); });
        sofa_ticket->release();      // signal the usher
        customer_on_sofa.release();  // signal barbers

        // 3. getHairCut() - now wait for barber chair, leave sofa, get haircut
        cut_ticket->acquire();  // barber will release it
        // what the cust holds is just that space sema
        has_space_on_sofa.release();
        // we need the 2 way sync again
        cut_ticket->release();

        std::println("cust {} ready for haircut", id);
        // reusing same sema gain
        cut_ticket->acquire();  // barber will release when done
        std::println("cust {} done with haircut", id);

        while_held(register_mtx, [&] {
            std::println("customer {} is paying", id);
            payment_ready.release();
            receipt_ready.acquire();
            std::println("customer {} paid", id);
        });

        while_held(global_mtx, [&] { n_customers--; });
    }
}

int main() {
    std::vector<std::thread> threads;

    for (int i = 0; i < N_BARBERS; i++) threads.emplace_back(barber, i);
    for (int i = 0; i < SOFA_CAPACITY; i++) threads.emplace_back(usher, i);
    for (int i = 0; i < 10; i++) threads.emplace_back(customer, i);

    threads[0].join();  // wait forever
}
