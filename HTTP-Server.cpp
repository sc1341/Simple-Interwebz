/*
* Simple-InterWebz-v1.0
* Basic webserver implementation that is intended to server a basic static HTML site via GET requests. 
*/

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>


namespace fs = std::filesystem;

using namespace std;

const int BUFFER_SIZE = 4096;
const int LISTEN_PORT = 8080;
const string WEB_SERVE_PATH = "/var/www/cipherchimes/";
const string WEB_HOSTNAME = "cipherchimes.com";
const string SERVER_VERSION = "Simple-InterWebz-v1.0";
const string HTTP_END = "\r\n\r\n";
const string LOG_FILE_NAME = WEB_HOSTNAME + "-" + SERVER_VERSION + "-" + "HTTP-LOG.txt";
const string NOT_FOUND_PAGE = WEB_SERVE_PATH + "/404.html";
const string ACCESS_DENIED_PAGE = WEB_SERVE_PATH + "/403.html";

std::atomic<bool> interrupted(false);

void signalHandler(int signal) {
    std::cout << "Recieved signal. Attempting to shut down the server" << std::endl;
    interrupted = true;
}


int log(string message){
    std::ofstream file(LOG_FILE_NAME, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open log file for appending." << std::endl;
        return 1;
    }
    file << message;
    if (!file) {
        std::cerr << "Failed to write to log file." << std::endl;
        return 1;
    }
    file.close();
    return 0;
}

string httpDate() {
    /**
     * Returns HTTP date format based on system clock time. 
     * :return: string 
    */
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    struct tm buf;
    gmtime_r(&in_time_t, &buf);
    char str[100];
    strftime(str, sizeof(str), "%a, %d %b %Y %H:%M:%S GMT", &buf);
    return str;
}

// Eventually handle POST request
void parseHTTPRequest(){

}

streamsize getFileSize(const string& file_name) {
    /*
    * Get the file size of the file to be sent for HTTP response. 
    */
    try {
        return fs::file_size(file_name);
    } catch (const fs::filesystem_error& err) {
        cerr << "Error retrieving file size: " << err.what() << endl;
        return -1;
    }
}

string generateHTTPResponse(int STATUS_CODE, string CONTENT_TYPE = "text/html", streamsize CONTENT_LENGTH = 0) {
    /**
     * Generate the HTTP response based on the status code and return the string. 
     */
    ostringstream response_stream;
    response_stream << "HTTP/1.0 ";
    // Identify the proper status code response
    if (STATUS_CODE == 200) {
        response_stream << "200 OK";
    } else if (STATUS_CODE == 404) {
        response_stream << "404 NOT FOUND";
    } else if (STATUS_CODE == 403) {
        response_stream << "403 FORBIDDEN";
    } else if (STATUS_CODE == 405){
        response_stream << "405 Method Not Allowed";
    } else if (STATUS_CODE == 418){
        response_stream << "418 I'm a teapot";
    }else {
        response_stream << "500 INTERNAL SERVER ERROR";
    }
    // Finish the rest of the HTTP response. 
    response_stream << "\r\n"
                    << "Date: " << httpDate() << "\r\n"
                    << "Server: " << SERVER_VERSION << "\r\n"
                    << "Content-Type: " << CONTENT_TYPE << "\r\n"
                    << "Content-Length: " << CONTENT_LENGTH
                    << HTTP_END;
    return response_stream.str();
}

ifstream openFile(const std::string& path, streamsize& file_size) {
    /*
    * Open a file within the web server directory and return the file handle. 
    */
    std::string file_name = WEB_SERVE_PATH + "/" + path;
    std::ifstream file(file_name, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << file_name << std::endl;
        return file;
    }
    file.seekg(0, std::ios::end);
    file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    return file;
}



void handleHTTPClient(int client_socket) {
    /*
    * Handles an HTTP client connection by reading input and preparing proper response. 
    */
    char buffer[BUFFER_SIZE];
    std::string request;
    std::ostringstream response_stream;
    // Read the HTTP request
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to read from socket." << std::endl;
        return;
    }
    buffer[bytes_received] = '\0';
    request = buffer;
    
    // Parse the HTTP request to get the file name
    std::istringstream request_stream(request);
    std::string method, path;
    request_stream >> method >> path;
    cout << "METHOD: " << method << std::endl << "PATH: " << path << std::endl;
    if (method != "GET") {
        std::cerr << "Unsupported HTTP method." << std::endl;
        response_stream << generateHTTPResponse(405);
        send(client_socket, NULL, 0, 0);
        return;
    }
    
    // Protect against unauthorized directory traversal 
    if ((path.find("../") != std::string::npos) || (path.find("..\\") != std::string::npos)) {
        size_t pos;
        while ((pos = path.find("../")) != std::string::npos) {
            path.replace(pos, 3, "/");
            response_stream << generateHTTPResponse(404); // just send 404 for now. 
            return;
        }
        while ((pos = path.find("..\\")) != std::string::npos) {
            path.replace(pos, 3, "\\");
            response_stream << generateHTTPResponse(404); // just send 404 for now. 
            return;
        }
    }
    //Handle no param / default path 
    if (path == "/." || path == "/" || path == ""){
        path = "index.html";
    } else if (path.find("teapot")) { // lolll just for funz 
        string response = generateHTTPResponse(418);
        send(client_socket, response.c_str(), response.size(), 0);
        return;
    }

    // Open File
    streamsize file_size = 0;
    std::ifstream file = openFile(path, file_size);
    // Handle 404 error not found
    if (!file.is_open()) {
        string file_name = NOT_FOUND_PAGE;
        std::ifstream not_found_file(file_name, std::ios::binary);
        streamsize file_size = getFileSize(file_name);
        
        if (not_found_file.is_open()) {
            std::string response_header = generateHTTPResponse(404, "text/html", file_size);
            send(client_socket, response_header.c_str(), response_header.size(), 0);
            char file_buffer[BUFFER_SIZE];
            while (!not_found_file.eof()) {
                not_found_file.read(file_buffer, BUFFER_SIZE);
                send(client_socket, file_buffer, not_found_file.gcount(), 0);
            }
        } else {
            std::string response = generateHTTPResponse(404, "text/html", 0);
            send(client_socket, response.c_str(), response.size(), 0);
        }
        return;
    }

    // This eventually needs to be identified by MIME type instead of file name. 
    // Gets the Content-Type attribute for the browser so it can handle images. 
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    string content_type = "text/html";  // Default value
    if (path.find(".js") != string::npos) {
        content_type = "application/javascript";
    } else if (path.find(".css") != string::npos) {
        content_type = "text/css";
    } else if (path.find(".jpg") != string::npos || path.find(".jpeg") != string::npos) {
        content_type = "image/jpeg";
    } else if (path.find(".png") != string::npos) {
        content_type = "image/png";
    }

    // Send HTTP response
    if (file.is_open()) {
        std::string response_header = generateHTTPResponse(200, content_type, file_size);
        send(client_socket, response_header.c_str(), response_header.size(), 0);

        char file_buffer[BUFFER_SIZE];
        while (!file.eof()) {
            file.read(file_buffer, BUFFER_SIZE);
            if (file.bad()) {
                cerr << "Error reading file" << endl;
                break;
            }
            if (!file.eof() && !file) {
                cerr << "File read was truncated" << endl;
                break;
            }
            send(client_socket, file_buffer, file.gcount(), 0);
        }
        file.close();  // Close the file after reading!
    } else {
        cerr << "Could not open file: " << path << endl;
        string response = generateHTTPResponse(404);
        send(client_socket, response.c_str(), response.size(), 0);
    }
    // Close the socket connection, do not do this again in the main loop!
    close(client_socket);
}

int main() {
    std::signal(SIGINT, signalHandler);
    cout << SERVER_VERSION << endl;
    cout << "Starting webserver on port " << LISTEN_PORT << " serving static content at: " << WEB_SERVE_PATH << endl;
    if (chdir(WEB_SERVE_PATH.c_str()) != 0) {
        perror("chdir failed. Please check the path value!");
        return 1;
    }
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Could not create socket." << std::endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Could not bind socket." << std::endl;
        return 1;
    }

    if (listen(server_socket, 10) == -1) {
        std::cerr << "Could not listen on socket." << std::endl;
        return 1;
    }
    std::cout << "Server is started on port " << LISTEN_PORT << std::endl;
    while (!interrupted) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        if (getpeername(client_socket, (struct sockaddr*)&client_addr, &client_addr_len) == -1) {
            std::cerr << "Failed to get origin client IP address." << std::endl;
        } else {
            std::cout << "Client IP: " << inet_ntoa(client_addr.sin_addr) << std::endl;
        }
        if (client_socket == -1) {
            std::cerr << "Could not accept client." << std::endl;
            continue;
        }
        // pass the socket to the HTTP client. 
        handleHTTPClient(client_socket);
    }
    std::cout << "Shutting down server..." << std::endl;
    close(server_socket);
    return 0;
}
