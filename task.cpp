#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <ctime>
#include <iomanip>
#include <sstream>

constexpr int Mwor = 10;
constexpr int TotalWorker = 5;
constexpr int messageSiz = 256;

struct W {
    int WorkT;
    int TotalW;
    int cmdPipe[Mwor][2];
    int statPipe[Mwor][2];
    int NextW;
};

W AllW[TotalWorker + 1];
int Wcount[TotalWorker + 1] = {0};

std::string currTime() {
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "[%a %b %d %H:%M:%S %Y]");
    return oss.str();
}

void makeW(int T, int N) {
    int toW[2];
    int fromW[2];
    
    if (pipe(toW) == -1 || pipe(fromW) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        close(toW[1]);
        close(fromW[0]);
        
        char buf[messageSiz];
        while (true) {
            ssize_t n = read(toW[0], buf, messageSiz);
            if (n <= 0) {
                if (n == -1) perror("read");
                break;
            }
            
            buf[n] = '\0';
            int dur = atoi(buf);
            
            std::cout << currTime() << " Worker " << T << "-" << N 
                      << ": Starting job (duration: " << dur << " seconds)\n";
            
            sleep(dur);
            
            std::cout << currTime() << " Worker " << T << "-" << N << ": Job completed\n";
            
            write(fromW[1], "done", 4);
        }
        
        close(toW[0]);
        close(fromW[1]);
        exit(EXIT_SUCCESS);
    } else {
        close(toW[0]);
        close(fromW[1]);
        
        AllW[T].cmdPipe[N][0] = toW[0];
        AllW[T].cmdPipe[N][1] = toW[1];
        AllW[T].statPipe[N][0] = fromW[0];
        AllW[T].statPipe[N][1] = fromW[1];
    }
}

void setup() {
    for (int i = 1; i <= TotalWorker; i++) {
        AllW[i].WorkT = i;
        AllW[i].TotalW = 0;
        AllW[i].NextW = 0;
    }
    
    std::cout << currTime() << " Starting job management system...\n";
    std::cout << currTime() << " Reading worker configuration...\n";
    
    for (int i = 0; i < TotalWorker; i++) {
        int T, C;
        if (!(std::cin >> T >> C)) {
            std::cerr << "Invalid worker configuration\n";
            exit(EXIT_FAILURE);
        }
        
        if (T < 1 || T > TotalWorker) {
            std::cerr << "Invalid worker type: " << T << " (must be 1-5)\n";
            exit(EXIT_FAILURE);
        }
        
        if (C < 1 || C > Mwor) {
            std::cerr << "Invalid worker count: " << C << " for type " << T 
                      << " (must be 1-" << Mwor << ")\n";
            exit(EXIT_FAILURE);
        }
        
        AllW[T].TotalW = C;
        Wcount[T] = C;
        
        std::cout << currTime() << " Creating " << C << " workers of type " << T << "...\n";
        
        for (int j = 0; j < C; j++) {
            makeW(T, j);
        }
    }
    
    std::cout << currTime() << " Worker configuration complete. Ready to accept jobs.\n";
}

void run() {
    fd_set fds;
    int maxfd = 0;
    
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    maxfd = STDIN_FILENO;
    
    for (int T = 1; T <= TotalWorker; T++) {
        for (int i = 0; i < AllW[T].TotalW; i++) {
            int fd = AllW[T].statPipe[i][0];
            FD_SET(fd, &fds);
            if (fd > maxfd) maxfd = fd;
        }
    }
    
    while (true) {
        fd_set tmp = fds;
        int res = select(maxfd + 1, &tmp, nullptr, nullptr, nullptr);
        
        if (res < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        
        if (FD_ISSET(STDIN_FILENO, &tmp)) {
            int JT, JD;
            if (!(std::cin >> JT >> JD)) {
                std::cout << currTime() << " End of job input received. Waiting for workers to finish...\n";
                break;
            }
            
            if (JT < 1 || JT > TotalWorker) {
                std::cerr << currTime() << " Error: Invalid job type: " << JT << " (must be 1-5)\n";
                continue;
            }
            
            if (JD < 1 || JD > 10) {
                std::cerr << currTime() << " Error: Invalid job duration: " << JD << " (must be 1-10)\n";
                continue;
            }
            
            if (AllW[JT].TotalW == 0) {
                std::cerr << currTime() << " Error: No workers available for job type " << JT << "\n";
                continue;
            }
            
            int WN = AllW[JT].NextW;
            AllW[JT].NextW = (AllW[JT].NextW + 1) % AllW[JT].TotalW;
            
            char buf[messageSiz];
            snprintf(buf, messageSiz, "%d", JD);
            write(AllW[JT].cmdPipe[WN][1], buf, strlen(buf));
            
            std::cout << currTime() << " Dispatched job type " << JT << " (duration: " << JD 
                      << ") to worker " << JT << "-" << WN << "\n";
        }
        
        for (int T = 1; T <= TotalWorker; T++) {
            for (int i = 0; i < AllW[T].TotalW; i++) {
                int fd = AllW[T].statPipe[i][0];
                if (FD_ISSET(fd, &tmp)) {
                    char buf[5];
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n > 0) {
                        // Worker ready for new job
                    }
                }
            }
        }
    }
}

void clean() {
    for (int T = 1; T <= TotalWorker; T++) {
        for (int i = 0; i < AllW[T].TotalW; i++) {
            close(AllW[T].cmdPipe[i][1]);
            close(AllW[T].statPipe[i][0]);
        }
    }
    
    while (wait(nullptr) > 0);
    
    std::cout << currTime() << " All workers finished. System shutdown complete.\n";
}

int main() {
    std::cout << currTime() << " Starting job management system...\n";
    
    setup();
    run();
    clean();
    
    return 0;
}