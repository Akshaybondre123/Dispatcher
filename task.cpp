#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <ctime>
#include <cerrno>
#include <iomanip>
#include <sstream>

constexpr int MAX_WORKERS_PER_TYPE = 10;
constexpr int MAX_WORKER_TYPES = 5;
constexpr int BUF_SIZE = 256;

struct WorkerType {
    int type;
    int count;
    int pipes_to_worker[MAX_WORKERS_PER_TYPE][2];
    int pipes_from_worker[MAX_WORKERS_PER_TYPE][2];
    int current_worker;
};

WorkerType workers[MAX_WORKER_TYPES + 1];
int worker_counts[MAX_WORKER_TYPES + 1] = {0};

std::string get_current_time() {
    time_t now = time(nullptr);
    struct tm *tm_now = localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm_now, "[%a %b %d %H:%M:%S %Y]");
    return oss.str();
}

void create_worker(int type, int index) {
    int to_worker[2];
    int from_worker[2];
    
    if (pipe(to_worker) == -1 || pipe(from_worker) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        close(to_worker[1]);
        close(from_worker[0]);
        
        char buf[BUF_SIZE];
        while (true) {
            ssize_t n = read(to_worker[0], buf, BUF_SIZE);
            if (n <= 0) {
                if (n == -1) perror("worker read");
                break;
            }
            
            buf[n] = '\0';
            int duration = atoi(buf);
            
            std::cout << get_current_time() << " Worker " << type << "-" << index 
                      << ": Starting job (duration: " << duration << " seconds)\n";
            
            sleep(duration);
            
            std::cout << get_current_time() << " Worker " << type << "-" << index << ": Job completed\n";
            
            write(from_worker[1], "done", 4);
        }
        
        close(to_worker[0]);
        close(from_worker[1]);
        exit(EXIT_SUCCESS);
    } else {
        close(to_worker[0]);
        close(from_worker[1]);
        
        workers[type].pipes_to_worker[index][0] = to_worker[0];
        workers[type].pipes_to_worker[index][1] = to_worker[1];
        workers[type].pipes_from_worker[index][0] = from_worker[0];
        workers[type].pipes_from_worker[index][1] = from_worker[1];
    }
}

void setup_workers() {
    for (int i = 1; i <= MAX_WORKER_TYPES; i++) {
        workers[i].type = i;
        workers[i].count = 0;
        workers[i].current_worker = 0;
    }
    
    std::cout << get_current_time() << " Reading worker configuration...\n";
    
    for (int i = 0; i < MAX_WORKER_TYPES; i++) {
        int type, count;
        if (!(std::cin >> type >> count)) {
            std::cerr << "Invalid worker configuration\n";
            exit(EXIT_FAILURE);
        }
        
        if (type < 1 || type > MAX_WORKER_TYPES) {
            std::cerr << "Invalid worker type: " << type << " (must be 1-5)\n";
            exit(EXIT_FAILURE);
        }
        
        if (count < 1 || count > MAX_WORKERS_PER_TYPE) {
            std::cerr << "Invalid worker count: " << count << " for type " << type 
                      << " (must be 1-" << MAX_WORKERS_PER_TYPE << ")\n";
            exit(EXIT_FAILURE);
        }
        
        workers[type].count = count;
        worker_counts[type] = count;
        
        std::cout << get_current_time() << " Creating " << count << " workers of type " << type << "...\n";
        
        for (int j = 0; j < count; j++) {
            create_worker(type, j);
        }
    }
    
    std::cout << get_current_time() << " Worker configuration complete. Ready to accept jobs.\n";
}

void distribute_jobs() {
    fd_set read_fds;
    int max_fd = 0;
    
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    max_fd = STDIN_FILENO;
    
    for (int type = 1; type <= MAX_WORKER_TYPES; type++) {
        for (int i = 0; i < workers[type].count; i++) {
            int fd = workers[type].pipes_from_worker[i][0];
            FD_SET(fd, &read_fds);
            if (fd > max_fd) max_fd = fd;
        }
    }
    
    while (true) {
        fd_set tmp_fds = read_fds;
        int activity = select(max_fd + 1, &tmp_fds, nullptr, nullptr, nullptr);
        
        if (activity < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            int job_type, duration;
            if (!(std::cin >> job_type >> duration)) {
                std::cout << get_current_time() << " End of job input received. Waiting for workers to finish...\n";
                break;
            }
            
            if (job_type < 1 || job_type > MAX_WORKER_TYPES) {
                std::cerr << get_current_time() << " Error: Invalid job type: " << job_type << " (must be 1-5)\n";
                continue;
            }
            
            if (duration < 1 || duration > 10) {
                std::cerr << get_current_time() << " Error: Invalid job duration: " << duration << " (must be 1-10)\n";
                continue;
            }
            
            if (workers[job_type].count == 0) {
                std::cerr << get_current_time() << " Error: No workers available for job type " << job_type << "\n";
                continue;
            }
            
            int worker_index = workers[job_type].current_worker;
            workers[job_type].current_worker = (workers[job_type].current_worker + 1) % workers[job_type].count;
            
            char buf[BUF_SIZE];
            snprintf(buf, BUF_SIZE, "%d", duration);
            write(workers[job_type].pipes_to_worker[worker_index][1], buf, strlen(buf));
            
            std::cout << get_current_time() << " Dispatched job type " << job_type << " (duration: " << duration 
                      << ") to worker " << job_type << "-" << worker_index << "\n";
        }
        
        for (int type = 1; type <= MAX_WORKER_TYPES; type++) {
            for (int i = 0; i < workers[type].count; i++) {
                int fd = workers[type].pipes_from_worker[i][0];
                if (FD_ISSET(fd, &tmp_fds)) {
                    char buf[5];
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n > 0) {
                        // Worker is ready for a new job
                    }
                }
            }
        }
    }
}

void cleanup() {
    for (int type = 1; type <= MAX_WORKER_TYPES; type++) {
        for (int i = 0; i < workers[type].count; i++) {
            close(workers[type].pipes_to_worker[i][1]);
            close(workers[type].pipes_from_worker[i][0]);
        }
    }
    
    while (wait(nullptr) > 0);
    
    std::cout << get_current_time() << " All workers have finished. Exiting.\n";
}

int main() {
    std::cout << get_current_time() << " Starting job dispatcher system...\n";
    
    setup_workers();
    distribute_jobs();
    cleanup();
    
    return 0;
}