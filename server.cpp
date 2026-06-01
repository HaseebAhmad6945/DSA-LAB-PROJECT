
#ifdef _WIN32
  #define _WIN32_WINNT 0x0601
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET sock_t;
  #define CLOSE_SOCK(s)  closesocket(s)
  #define SOCK_INVALID   INVALID_SOCKET
  #define SOCK_ERR       SOCKET_ERROR
#else
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <pthread.h>
  typedef int sock_t;
  #define CLOSE_SOCK(s)  close(s)
  #define SOCK_INVALID   (-1)
  #define SOCK_ERR       (-1)
#endif

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#define PORT        9090
#define BUFFER_SIZE 65536

// ─────────────────────────────────────────────
//  One animation step
// ─────────────────────────────────────────────
struct Step {
    int         i, j, extra;   // indices; extra = pivot / min
    std::string type;           // compare | swap | sorted | set | pivot
    std::vector<int> arr;       // full array state at this step
};

// ─────────────────────────────────────────────
//  JSON helpers
// ─────────────────────────────────────────────
static std::string arrayToJson(const std::vector<int>& a) {
    std::string s = "[";
    for (size_t i = 0; i < a.size(); ++i) {
        s += std::to_string(a[i]);
        if (i + 1 < a.size()) s += ",";
    }
    return s + "]";
}

static std::string stepsToJson(const std::vector<Step>& steps) {
    std::string s = "[";
    for (size_t k = 0; k < steps.size(); ++k) {
        const Step& st = steps[k];
        s += "{\"i\":"     + std::to_string(st.i)   +
             ",\"j\":"     + std::to_string(st.j)   +
             ",\"type\":\"" + st.type + "\""         +
             ",\"extra\":" + std::to_string(st.extra)+
             ",\"arr\":"   + arrayToJson(st.arr)     + "}";
        if (k + 1 < steps.size()) s += ",";
    }
    return s + "]";
}

// ─────────────────────────────────────────────
//  BUBBLE SORT  (order = "asc" or "desc")
// ─────────────────────────────────────────────
static std::vector<Step> bubbleSort(std::vector<int> a, const std::string& order) {
    std::vector<Step> steps;
    int n = (int)a.size();
    bool descending = (order == "desc");

    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
            steps.push_back({j, j+1, -1, "compare", a});

            bool shouldSwap = descending ? (a[j] < a[j+1]) : (a[j] > a[j+1]);
            if (shouldSwap) {
                std::swap(a[j], a[j+1]);
                steps.push_back({j, j+1, -1, "swap", a});
            }
        }
        steps.push_back({n-1-i, -1, -1, "sorted", a});
    }
    steps.push_back({0, -1, -1, "sorted", a});
    return steps;
}

// ─────────────────────────────────────────────
//  SELECTION SORT (always ascending)
// ─────────────────────────────────────────────
static std::vector<Step> selectionSort(std::vector<int> a) {
    std::vector<Step> steps;
    int n = (int)a.size();
    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n; ++j) {
            steps.push_back({j, minIdx, minIdx, "compare", a});
            if (a[j] < a[minIdx]) minIdx = j;
        }
        if (minIdx != i) {
            std::swap(a[i], a[minIdx]);
            steps.push_back({i, minIdx, -1, "swap", a});
        }
        steps.push_back({i, -1, -1, "sorted", a});
    }
    steps.push_back({n-1, -1, -1, "sorted", a});
    return steps;
}

// ─────────────────────────────────────────────
//  INSERTION SORT
// ─────────────────────────────────────────────
static std::vector<Step> insertionSort(std::vector<int> a) {
    std::vector<Step> steps;
    int n = (int)a.size();
    steps.push_back({0, -1, -1, "sorted", a});
    for (int i = 1; i < n; ++i) {
        int j = i;
        while (j > 0) {
            steps.push_back({j, j-1, -1, "compare", a});
            if (a[j] < a[j-1]) {
                std::swap(a[j], a[j-1]);
                steps.push_back({j, j-1, -1, "swap", a});
                --j;
            } else break;
        }
        steps.push_back({i, -1, -1, "sorted", a});
    }
    return steps;
}

// ─────────────────────────────────────────────
//  MERGE SORT
// ─────────────────────────────────────────────
static void mergeHelper(std::vector<int>& a, int l, int r, std::vector<Step>& steps) {
    if (l >= r) return;
    int m = (l + r) / 2;
    mergeHelper(a, l, m, steps);
    mergeHelper(a, m+1, r, steps);

    std::vector<int> L(a.begin()+l, a.begin()+m+1);
    std::vector<int> R(a.begin()+m+1, a.begin()+r+1);
    int i = 0, j = 0, k = l;
    while (i < (int)L.size() && j < (int)R.size()) {
        steps.push_back({l+i, m+1+j, -1, "compare", a});
        a[k++] = (L[i] <= R[j]) ? L[i++] : R[j++];
        steps.push_back({k-1, -1, -1, "set", a});
    }
    while (i < (int)L.size()) { a[k++] = L[i++]; steps.push_back({k-1,-1,-1,"set",a}); }
    while (j < (int)R.size()) { a[k++] = R[j++]; steps.push_back({k-1,-1,-1,"set",a}); }
    for (int x = l; x <= r; ++x) steps.push_back({x,-1,-1,"sorted",a});
}

static std::vector<Step> mergeSort(std::vector<int> a) {
    std::vector<Step> steps;
    mergeHelper(a, 0, (int)a.size()-1, steps);
    return steps;
}

// ─────────────────────────────────────────────
//  QUICK SORT
// ─────────────────────────────────────────────
static int partitionQS(std::vector<int>& a, int lo, int hi, std::vector<Step>& steps) {
    int pivot = a[hi], i = lo - 1;
    steps.push_back({hi, -1, hi, "pivot", a});
    for (int j = lo; j < hi; ++j) {
        steps.push_back({j, hi, hi, "compare", a});
        if (a[j] <= pivot) {
            ++i;
            if (i != j) { std::swap(a[i], a[j]); steps.push_back({i,j,hi,"swap",a}); }
        }
    }
    std::swap(a[i+1], a[hi]);
    steps.push_back({i+1, hi, -1, "swap", a});
    steps.push_back({i+1, -1, -1, "sorted", a});
    return i + 1;
}

static void quickHelper(std::vector<int>& a, int lo, int hi, std::vector<Step>& steps) {
    if (lo < hi) {
        int pi = partitionQS(a, lo, hi, steps);
        quickHelper(a, lo, pi-1, steps);
        quickHelper(a, pi+1, hi, steps);
    } else if (lo == hi) {
        steps.push_back({lo, -1, -1, "sorted", a});
    }
}

static std::vector<Step> quickSort(std::vector<int> a) {
    std::vector<Step> steps;
    quickHelper(a, 0, (int)a.size()-1, steps);
    return steps;
}

// ─────────────────────────────────────────────
//  HEAP SORT
// ─────────────────────────────────────────────
static void heapify(std::vector<int>& a, int n, int i, std::vector<Step>& steps) {
    int largest = i, l = 2*i+1, r = 2*i+2;
    if (l < n) { steps.push_back({l,largest,-1,"compare",a}); if (a[l]>a[largest]) largest=l; }
    if (r < n) { steps.push_back({r,largest,-1,"compare",a}); if (a[r]>a[largest]) largest=r; }
    if (largest != i) {
        std::swap(a[i], a[largest]);
        steps.push_back({i,largest,-1,"swap",a});
        heapify(a, n, largest, steps);
    }
}

static std::vector<Step> heapSort(std::vector<int> a) {
    std::vector<Step> steps;
    int n = (int)a.size();
    for (int i = n/2-1; i >= 0; --i) heapify(a, n, i, steps);
    for (int i = n-1; i > 0; --i) {
        std::swap(a[0], a[i]);
        steps.push_back({0,i,-1,"swap",a});
        steps.push_back({i,-1,-1,"sorted",a});
        heapify(a, i, 0, steps);
    }
    steps.push_back({0,-1,-1,"sorted",a});
    return steps;
}

// ─────────────────────────────────────────────
//  URL helpers
// ─────────────────────────────────────────────
static std::string getParam(const std::string& query, const std::string& key) {
    std::string search = key + "=";
    size_t pos = query.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = query.find('&', pos);
    return query.substr(pos, end == std::string::npos ? query.size()-pos : end-pos);
}

static std::vector<int> parseArray(const std::string& s) {
    std::vector<int> a;
    if (s.empty()) return a;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        try { int v = std::stoi(tok); if (v > 0) a.push_back(v); } catch (...) {}
    }
    return a;
}

static std::string buildResponse(int code, const std::string& body) {
    std::string status = code==200 ? "OK" : code==400 ? "Bad Request" : "Not Found";
    std::string r = "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n";
    r += "Content-Type: application/json; charset=utf-8\r\n";
    r += "Access-Control-Allow-Origin: *\r\n";
    r += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
    r += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    r += "Connection: close\r\n\r\n" + body;
    return r;
}

// ─────────────────────────────────────────────
//  Handle one HTTP request
// ─────────────────────────────────────────────
static void handleRequest(sock_t sock) {
    char buf[BUFFER_SIZE] = {0};
#ifdef _WIN32
    recv(sock, buf, BUFFER_SIZE-1, 0);
#else
    read(sock, buf, BUFFER_SIZE-1);
#endif

    std::string req(buf);
    std::istringstream rs(req);
    std::string method, fullPath;
    rs >> method >> fullPath;

    std::string response;

    // CORS preflight
    if (method == "OPTIONS") {
        response = buildResponse(200, "");
        goto send_and_close;
    }

    {
        std::string path = fullPath, query;
        size_t qp = fullPath.find('?');
        if (qp != std::string::npos) { path = fullPath.substr(0,qp); query = fullPath.substr(qp+1); }

        if (path == "/ping") {
            response = buildResponse(200, "{\"status\":\"ok\",\"message\":\"C++ Sorting Server Running\"}");

        } else if (path == "/sort") {
            std::string algo  = getParam(query, "algo");
            std::string order = getParam(query, "order");   // asc / desc (bubble only)
            if (order.empty()) order = "asc";

            std::vector<int> a = parseArray(getParam(query, "arr"));
            if (a.empty() || a.size() > 200) {
                response = buildResponse(400, "{\"error\":\"Invalid array. Send 1-200 positive integers.\"}");
                goto send_and_close;
            }

            std::vector<Step> steps;
            if      (algo == "bubble")    steps = bubbleSort(a, order);
            else if (algo == "selection") steps = selectionSort(a);
            else if (algo == "insertion") steps = insertionSort(a);
            else if (algo == "merge")     steps = mergeSort(a);
            else if (algo == "quick")     steps = quickSort(a);
            else if (algo == "heap")      steps = heapSort(a);
            else {
                response = buildResponse(400, "{\"error\":\"Unknown algorithm\"}");
                goto send_and_close;
            }

            // Build final sorted array from last step
            std::string finalArr = steps.empty() ? arrayToJson(a) : arrayToJson(steps.back().arr);

            std::string json = "{\"algo\":\"" + algo + "\""
                             + ",\"order\":\"" + order + "\""
                             + ",\"original\":" + arrayToJson(a)
                             + ",\"sorted\":"   + finalArr
                             + ",\"steps\":"    + stepsToJson(steps)
                             + "}";
            response = buildResponse(200, json);

        } else {
            response = buildResponse(404, "{\"error\":\"Not found\"}");
        }
    }

send_and_close:
#ifdef _WIN32
    send(sock, response.c_str(), (int)response.size(), 0);
#else
    write(sock, response.c_str(), response.size());
#endif
    CLOSE_SOCK(sock);
}

// ─────────────────────────────────────────────
//  Thread entry point
// ─────────────────────────────────────────────
#ifdef _WIN32
DWORD WINAPI threadFunc(LPVOID arg) {
    sock_t s = *(sock_t*)arg; delete (sock_t*)arg; handleRequest(s); return 0;
}
#else
void* threadFunc(void* arg) {
    sock_t s = *(sock_t*)arg; delete (sock_t*)arg; handleRequest(s); return nullptr;
}
#endif

// ─────────────────────────────────────────────
//  main()
// ─────────────────────────────────────────────
int main() {
#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2,2), &wd) != 0) { std::cerr << "WSAStartup failed\n"; return 1; }
#endif

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == SOCK_INVALID) { std::cerr << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(srv,(sockaddr*)&addr,sizeof(addr)) == SOCK_ERR) { std::cerr<<"bind() failed\n"; return 1; }
    if (listen(srv, 10) == SOCK_ERR) { std::cerr<<"listen() failed\n"; return 1; }

    std::cout << "\n==========================================\n";
    std::cout << "  DSA Sorting Visualizer - C++ Backend  \n";
    std::cout << "  Listening on http://localhost:" << PORT << "\n";
    std::cout << "==========================================\n";
    std::cout << "  GET /ping\n";
    std::cout << "  GET /sort?algo=bubble&order=asc&arr=5,3,1\n";
    std::cout << "  Algorithms: bubble selection insertion\n";
    std::cout << "              merge   quick     heap\n";
    std::cout << "  Press Ctrl+C to stop.\n\n";

    while (true) {
        sockaddr_in ca{};
#ifdef _WIN32
        int cl = sizeof(ca);
#else
        socklen_t cl = sizeof(ca);
#endif
        sock_t client = accept(srv, (sockaddr*)&ca, &cl);
        if (client == SOCK_INVALID) continue;

        sock_t* sp = new sock_t(client);
#ifdef _WIN32
        HANDLE t = CreateThread(NULL, 0, threadFunc, sp, 0, NULL);
        if (t) CloseHandle(t);
#else
        pthread_t t;
        pthread_create(&t, nullptr, threadFunc, sp);
        pthread_detach(t);
#endif
    }

    CLOSE_SOCK(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
