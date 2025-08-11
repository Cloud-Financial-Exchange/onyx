import queue
import sys
import time
import threading

from matplotlib import pyplot as plt

ARRIVAL_TIME = 0.000001
SERVICE_TIME = 0.000035

def customer_arrival(q, interval):
    i = 0
    while i < 100:
        time.sleep(interval)
        q.put(i)
        i += interval

def service_customer(q, service_time):
    while True:
        if not q.empty():
            customer = q.get()
            time.sleep(service_time)

def logging(q):
    data = []
    duration = 6
    
    while duration:
        data += [q.qsize()]
        duration -= 1
        time.sleep(10)
    
    plt.plot(data)
    plt.savefig("./figs/tmp/queue.pdf")
    print(data)



if __name__ == "__main__":
    arrival = ARRIVAL_TIME
    service = SERVICE_TIME

    if (len(sys.argv)) >= 3:
        arrival = float(sys.argv[1])
        service = float(sys.argv[2])

    q = queue.Queue()

    # Start the customer arrival thread
    arrival_thread = threading.Thread(target=customer_arrival, args=(q, arrival))
    arrival_thread.start()

    # Start the service thread
    service_thread = threading.Thread(target=service_customer, args=(q, service))
    service_thread.start()

    # Logging thread
    logging_thread = threading.Thread(target=logging, args=(q,))
    logging_thread.start()

    # Wait for both threads to finish
    arrival_thread.join()
    service_thread.join()
    logging_thread.join()    
