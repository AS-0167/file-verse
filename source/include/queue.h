#pragma once
#include <string>
#include "fifo_queue.h"  // Include the FIFOQueue header

class OperationQueue {
public:
    using Job = std::function<void()>;  // A Job is just a function to be run

    OperationQueue() = default;  // Use FIFOQueue inside

    ~OperationQueue() { stop(); }

    bool enqueue(Job job) {
        fifoQueue.enqueue(job);  // Enqueue a job into the FIFOQueue
        return true;
    }

    void stop() {
        std::cout << "Stopping the queue" << std::endl;
    }

    void worker_loop() {
        while (!fifoQueue.isEmpty()) {
            Job job = fifoQueue.dequeue();
            job();  // Execute job
        }
    }

private:
    FIFOQueue<Job> fifoQueue;  // Using FIFOQueue to store jobs
};
