#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <iomanip>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <limits>

using namespace std;

// Request class
class Request {
public:
    int source_id;
    int request_id;
    double arrival_time;
    double start_service_time;
    double finish_service_time;

    Request(int src_id, int req_id, double arr_time)
        : source_id(src_id), request_id(req_id), arrival_time(arr_time),
        start_service_time(0), finish_service_time(0) {
    }
};

// Source class
class Source {
private:
    uniform_real_distribution<double> dist; // Uniform distribution
    default_random_engine& generator;
    int source_id;

public:
    Source(int id, double min_int, double max_int, default_random_engine& gen)
        : source_id(id), generator(gen), dist(min_int, max_int) {
    }

    double getNextInterval() {
        return dist(generator);
    }

    int getId() const { return source_id; }
};

// Device class
class Device {
private:
    exponential_distribution<double> dist; // Exponential distribution
    default_random_engine& generator;
    int device_id;
    Request* current_request;

public:
    Device(int id, double mean_time, default_random_engine& gen)
        : device_id(id), generator(gen), dist(1.0 / mean_time), current_request(nullptr) {
    }

    double getServiceTime() {
        return dist(generator);
    }

    bool isFree() const { return current_request == nullptr; }

    void startService(Request* request, double current_time) {
        current_request = request;
        request->start_service_time = current_time;
    }

    Request* finishService() {
        Request* finished = current_request;
        current_request = nullptr;
        return finished;
    }

    int getId() const { return device_id; }
};

// Buffer class with FIFO discipline
class Buffer {
private:
    queue<Request*> buffer;
    int max_size;

public:
    Buffer(int size) : max_size(size) {}

    bool isFull() const { return buffer.size() >= max_size; }
    bool isEmpty() const { return buffer.empty(); }
    int getSize() const { return (int)buffer.size(); }
    int getMaxSize() const { return max_size; }

    // Add request to buffer (FIFO)
    void addRequest(Request* request) {
        buffer.push(request);
    }

    // Get next request with packet service discipline
    Request* getNextRequest(int& current_serving_source) {
        if (buffer.empty()) {
            current_serving_source = -1;
            return nullptr;
        }

        // If current packet exists, find request from this source
        if (current_serving_source != -1) {
            queue<Request*> temp;
            Request* found = nullptr;

            while (!buffer.empty()) {
                Request* req = buffer.front();
                buffer.pop();
                if (req->source_id == current_serving_source && found == nullptr) {
                    found = req;
                }
                else {
                    temp.push(req);
                }
            }

            while (!temp.empty()) {
                buffer.push(temp.front());
                temp.pop();
            }

            if (found) {
                return found;
            }
            else {
                current_serving_source = -1;
            }
        }

        if (buffer.empty()) {
            current_serving_source = -1;
            return nullptr;
        }

        queue<Request*> temp;
        Request* best_request = nullptr;
        int best_source = INT_MAX;

        while (!buffer.empty()) {
            Request* req = buffer.front();
            buffer.pop();
            if (req->source_id < best_source) {
                if (best_request) temp.push(best_request);
                best_source = req->source_id;
                best_request = req;
            }
            else {
                temp.push(req);
            }
        }

        while (!temp.empty()) {
            buffer.push(temp.front());
            temp.pop();
        }

        if (best_request) {
            current_serving_source = best_source;
        }

        return best_request;
    }

    // Find request to reject (from source with highest number)
    Request* findRequestToReject() {
        if (buffer.empty()) return nullptr;

        queue<Request*> temp;
        Request* worst_request = nullptr;
        int worst_source = -1;

        while (!buffer.empty()) {
            Request* req = buffer.front();
            buffer.pop();
            if (req->source_id > worst_source) {
                if (worst_request) temp.push(worst_request);
                worst_source = req->source_id;
                worst_request = req;
            }
            else {
                temp.push(req);
            }
        }

        while (!temp.empty()) {
            buffer.push(temp.front());
            temp.pop();
        }

        return worst_request;
    }

    void removeRequest(Request* request) {
        queue<Request*> temp;

        while (!buffer.empty()) {
            Request* req = buffer.front();
            buffer.pop();
            if (req != request) {
                temp.push(req);
            }
        }

        while (!temp.empty()) {
            buffer.push(temp.front());
            temp.pop();
        }
    }
};

// Device selector with round-robin discipline
class DeviceSelector {
private:
    int last_used;
    int num_devices;

public:
    DeviceSelector(int num_devs) : last_used(-1), num_devices(num_devs) {}

    Device* getFreeDevice(vector<Device*>& devices) {
        if (devices.empty()) return nullptr;

        int start_index = (last_used + 1) % num_devices;

        for (int i = 0; i < num_devices; i++) {
            int idx = (start_index + i) % num_devices;
            if (devices[idx]->isFree()) {
                last_used = idx;
                return devices[idx];
            }
        }
        return nullptr;
    }
};

// Event class
class Event {
public:
    double time;
    enum Type { ARRIVAL, DEPARTURE } type;
    int entity_id;
    Request* request;

    Event(double t, Type tp, int id, Request* req = nullptr)
        : time(t), type(tp), entity_id(id), request(req) {
    }

    bool operator>(const Event& other) const {
        return time > other.time;
    }
};

// Main simulation model
class SimulationModel {
private:
    priority_queue<Event, vector<Event>, greater<Event>> calendar;
    vector<Source*> sources;
    vector<Device*> devices;
    Buffer* buffer;
    DeviceSelector* device_selector;
    default_random_engine generator;

    double current_time;
    int current_serving_source;
    int requests_generated;
    int requests_served;
    int requests_rejected;

    vector<int> source_requests;
    vector<int> source_rejections;
    vector<double> source_total_time;
    vector<double> source_waiting_time;
    vector<double> device_busy_time;

public:
    SimulationModel() : current_time(0), current_serving_source(-1),
        requests_generated(0), requests_served(0), requests_rejected(0) {

        random_device rd;
        generator.seed(rd());

        // Create sources
        int num_sources = 3;
        for (int i = 0; i < num_sources; i++) {
            double min_interval = 1.5 + i * 0.5;
            double max_interval = 2.5 + i * 0.5;
            sources.push_back(new Source(i, min_interval, max_interval, generator));
        }

        // Create devices
        int num_devices = 2;
        for (int i = 0; i < num_devices; i++) {
            double mean_time = 2.0 + i * 1.0;
            devices.push_back(new Device(i, mean_time, generator));
        }

        buffer = new Buffer(3);
        device_selector = new DeviceSelector(num_devices);

        source_requests.resize(num_sources, 0);
        source_rejections.resize(num_sources, 0);
        source_total_time.resize(num_sources, 0);
        source_waiting_time.resize(num_sources, 0);
        device_busy_time.resize(num_devices, 0);

        for (int i = 0; i < num_sources; i++) {
            double first_time = sources[i]->getNextInterval();
            calendar.push(Event(first_time, Event::ARRIVAL, i));
        }
    }

    ~SimulationModel() {
        for (auto source : sources) delete source;
        for (auto device : devices) delete device;
        delete buffer;
        delete device_selector;
    }

    void processArrival(int source_id) {
        requests_generated++;
        source_requests[source_id]++;
        Request* request = new Request(source_id, source_requests[source_id], current_time);

        double next_time = current_time + sources[source_id]->getNextInterval();
        calendar.push(Event(next_time, Event::ARRIVAL, source_id));

        Device* free_device = device_selector->getFreeDevice(devices);
        if (free_device) {
            double service_time = free_device->getServiceTime();
            free_device->startService(request, current_time);
            calendar.push(Event(current_time + service_time, Event::DEPARTURE,
                free_device->getId(), request));
        }
        else {
            if (!buffer->isFull()) {
                buffer->addRequest(request);
            }
            else {
                Request* rejected_request = buffer->findRequestToReject();
                if (rejected_request) {
                    source_rejections[rejected_request->source_id]++;
                    requests_rejected++;
                    buffer->removeRequest(rejected_request);
                    delete rejected_request;
                }
                buffer->addRequest(request);
            }
        }
    }

    void processDeparture(int device_id) {
        Device* device = devices[device_id];
        Request* finished_request = device->finishService();

        if (finished_request) {
            requests_served++;
            finished_request->finish_service_time = current_time;

            double total_time = finished_request->finish_service_time -
                finished_request->arrival_time;
            double waiting_time = finished_request->start_service_time -
                finished_request->arrival_time;

            source_total_time[finished_request->source_id] += total_time;
            source_waiting_time[finished_request->source_id] += waiting_time;
            device_busy_time[device_id] += (finished_request->finish_service_time -
                finished_request->start_service_time);

            delete finished_request;
        }

        if (!buffer->isEmpty()) {
            Request* next_request = buffer->getNextRequest(current_serving_source);
            if (next_request) {
                buffer->removeRequest(next_request);

                Device* free_device = device_selector->getFreeDevice(devices);
                if (free_device) {
                    double service_time = free_device->getServiceTime();
                    free_device->startService(next_request, current_time);
                    calendar.push(Event(current_time + service_time, Event::DEPARTURE,
                        free_device->getId(), next_request));
                }
            }
        }
    }

    void run(double max_time = 1000.0, int max_requests = 1000) {
        cout << "=== SIMULATION MODEL VARIANT 6 ===" << endl;
        cout << "DISCIPLINES:" << endl;
        cout << "- Infinite sources" << endl;
        cout << "- Uniform request distribution" << endl;
        cout << "- Exponential service time" << endl;
        cout << "- FIFO buffering" << endl;
        cout << "- Rejection by source priority" << endl;
        cout << "- Packet service" << endl;
        cout << "- Round-robin device selection" << endl;
        cout << "Parameters: " << sources.size() << " sources, "
            << devices.size() << " devices, buffer: " << buffer->getMaxSize() << endl;
        cout << "Max time: " << max_time << " units" << endl;
        cout << "Max requests: " << max_requests << endl;
        cout << "----------------------------------------" << endl;

        while (!calendar.empty() && current_time < max_time &&
            requests_served < max_requests) {

            Event event = calendar.top();
            calendar.pop();
            current_time = event.time;

            if (event.type == Event::ARRIVAL) {
                processArrival(event.entity_id);
            }
            else if (event.type == Event::DEPARTURE) {
                processDeparture(event.entity_id);
            }
        }

        printResults();
    }

    void printResults() {
        cout << "\n=== SIMULATION RESULTS ===" << endl;
        cout << "Total simulation time: " << current_time << " units" << endl;
        cout << "Requests generated: " << requests_generated << endl;
        cout << "Requests served: " << requests_served << endl;
        cout << "Requests rejected: " << requests_rejected << endl;

        cout << "\n--- SOURCE CHARACTERISTICS ---" << endl;
        cout << setw(10) << "Source" << setw(12) << "Requests"
            << setw(12) << "Rejected" << setw(12) << "P_reject"
            << setw(12) << "T_total" << setw(12) << "T_wait" << endl;

        for (size_t i = 0; i < sources.size(); i++) {
            int served_requests = source_requests[i] - source_rejections[i];
            double reject_prob = source_requests[i] > 0 ?
                (double)source_rejections[i] / source_requests[i] : 0;
            double avg_total_time = served_requests > 0 ?
                source_total_time[i] / served_requests : 0;
            double avg_waiting_time = served_requests > 0 ?
                source_waiting_time[i] / served_requests : 0;

            string source_name = "S" + to_string(i + 1);
            cout << setw(10) << source_name
                << setw(12) << source_requests[i]
                << setw(12) << source_rejections[i]
                << setw(12) << fixed << setprecision(3) << reject_prob
                << setw(12) << fixed << setprecision(2) << avg_total_time
                << setw(12) << fixed << setprecision(2) << avg_waiting_time
                << endl;
        }

        cout << "\n--- DEVICE CHARACTERISTICS ---" << endl;
        cout << setw(10) << "Device" << setw(15) << "Utilization" << endl;

        for (size_t i = 0; i < devices.size(); i++) {
            double utilization = current_time > 0 ? device_busy_time[i] / current_time : 0;
            string device_name = "D" + to_string(i + 1);
            cout << setw(10) << device_name
                << setw(15) << fixed << setprecision(3) << utilization
                << endl;
        }

        cout << "\n--- DISCIPLINE ANALYSIS ---" << endl;
        string current_packet = (current_serving_source == -1) ? "none" : "S" + to_string(current_serving_source + 1);
        cout << "Packet service: Current packet = " << current_packet << endl;
        cout << "Rejections: Total rejected = " << requests_rejected << endl;
        cout << "Buffer: Max size = " << buffer->getMaxSize()
            << ", Current size = " << buffer->getSize() << endl;
    }
};

int main() {
    SimulationModel model;
    model.run(1000.0, 1000);

    cout << "\nPress Enter to exit...";
    cin.get();

    return 0;
}