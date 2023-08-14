#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>

class Client {
    const char *config, *dir;
    int id, port, unique_id; 
    int n; // no of neighbours of this client
    int (*neighbours)[2];
    int f; // no of files
    std::vector <std::string> files;
    std::vector <std::string> ownedFiles; 
    char recvd_msg[1025];

    int reqFile, owner, replies;
    bool asked;

    int intros;
    std::vector <std::string> introMsgs;

    public:
        Client(char *config, char *dir) {
            this->config = config;
            this->dir = dir; 
            
            memset(recvd_msg, '\0', sizeof(recvd_msg));

            reqFile = 0;
            owner = 0;
            asked = false;

            intros = 0;
            loadConfig();
        }

        void loadConfig() {
            std::fstream config_file;
            config_file.open(config);

            if (config_file.is_open()) {
                config_file >> id >> port >> unique_id; 
                config_file >> n;
                
                neighbours = new int[n][2];
                for (int i = 0; i < n; i++) {
                    config_file >> neighbours[i][0];
                    config_file >> neighbours[i][1]; 
                }
                
                config_file >> f;
                for (int i = 0; i < f; i++) {
                    std::string filename;
                    config_file >> filename;

                    files.push_back(filename);
                }
            }
            sort(files.begin(), files.end());
        }
        
        void getOwnedFiles(){
            DIR* dirp = opendir(dir);
            struct dirent *dp;

            while ((dp = readdir(dirp)) != NULL) {
                if (dp->d_type != DT_DIR) { 
                    ownedFiles.push_back(dp->d_name);
                }
            }

            sort(ownedFiles.begin(), ownedFiles.end());

            for (auto it : ownedFiles) {
                std::cout << it << std::endl;
            }

            closedir(dirp);
        }

        int clientConnect(std::string addr, int port) {
            struct sockaddr_in serv_addr;
            int sock;

            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == 0) {
                perror("socket failed");
                exit(1);
            }
            
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);

            inet_pton(AF_INET, addr.c_str(), &serv_addr.sin_addr);
            while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                sleep(1);
            }

            return sock;
        }

        void sendIntro(int socket) {
            char intro[1025];

            sprintf(intro, "I %d %d %d", id, unique_id, port);

            send(socket, intro, strlen(intro), 0);
        }

        int findFile(char *f, int depth) {
            std::string filename(f);

            if (depth == 0) {
                auto it = std::find(ownedFiles.begin(), ownedFiles.end(), filename);

                if (it == ownedFiles.end()) {
                    return 0;
                } else {
                    return unique_id;
                }
            } else {
                // useless since we won't be having any depth 2 queries in this phase 
                return 0;
            }
        }

        void handleMsg(int socket) {
            char resp[1025];

            switch (recvd_msg[0]) {
                case 'I': {
                    int id, unique_id, port;
                    
                    sscanf(recvd_msg, "I %d %d %d", &id, &unique_id, &port);

                    char m[1024];
                    sprintf(m, "Connected to %d with unique-ID %d on port %d\n", id, unique_id, port);  
                    std::string introMsg(m);

                    introMsgs.push_back(introMsg);

                    intros++;
                    if (intros == n) {
                        sort(introMsgs.begin(), introMsgs.end());

                        for (auto it : introMsgs) {
                            std::cout << it; 
                        }
                    }
                } break;
                case 'S': {
                    char filename[1025];
                    int depth;

                    sscanf(recvd_msg, "S %s %d", filename, &depth);
                    
                    int res = findFile(filename, depth);
                    sprintf(resp, "F %d", res);

                    send(socket, resp, strlen(resp), 0);
                } break;
                case 'F': {
                    int owner_recv;
                   
                    replies++;
                    sscanf(recvd_msg, "F %d", &owner_recv);
                    
                    if (owner_recv != 0) {
                        if (owner == 0 || owner_recv < owner) {
                            owner = owner_recv;
                        } 
                    } 

                    if (replies == n) {
                        std::cout << "Found " << files[reqFile - 1] << " at " << owner << " with MD5 " << 0 << " at depth " << ((owner == 0) ? 0 : 1) << std::endl;
                        asked = false; 
                    }
                } break;
            }

            memset(recvd_msg, '\0', sizeof(recvd_msg));
        }

        void main_loop(){

            if (n == 0) {
                for (auto it : files) {
                    printf("Found %s at 0 with MD5 0 at depth 0\n", it.c_str());
                }

                return;
            }

            int server_socket;
            std::vector<int> accepted_sockets, connected_sockets;
            struct sockaddr_in address;
            fd_set readfds;
            int max_sd; 
            
            server_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (server_socket == 0) {
                perror("socket failed");
                exit(1);
            }
            
            int opt = 1;
            if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
                perror("setsockopt");
                exit(1);
            } 
            
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("bind failed");
                exit(1);
            } 

            if (listen(server_socket, n) < 0) {
                perror("listen failed");
                exit(1);
            } 
            
            int addrlen = sizeof(address);
            
            for (int i = 0; i < n; i++) {
                int client_socket = clientConnect("127.0.0.1", neighbours[i][1]);
                connected_sockets.push_back(client_socket);
            }
            
            while (1) {               
                FD_ZERO(&readfds);
                
                FD_SET(server_socket, &readfds);
                max_sd = server_socket;

                for (int i = 0; i < accepted_sockets.size(); i++) {
                    int sd = accepted_sockets[i];

                    FD_SET(sd, &readfds);

                    if (sd > max_sd) {
                       max_sd = sd;
                    } 
                }  

                for (int i = 0; i < connected_sockets.size(); i++) {
                    int sd = connected_sockets[i];

                    FD_SET(sd, &readfds);

                    if (sd > max_sd) {
                       max_sd = sd;
                    } 
                }                

                int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

                if (FD_ISSET(server_socket, &readfds)) {
                    int client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);                     

                    accepted_sockets.push_back(client_socket);

                    sendIntro(client_socket);
                } 
                
                for (int i = 0; i < connected_sockets.size(); i++) {
                    int sd = connected_sockets[i];
                    
                    if (FD_ISSET(sd, &readfds)) {
                        int valread = read(sd, recvd_msg, 1024);
                       
                        if (valread == 0) {
                            close(connected_sockets[i]);
                            connected_sockets.erase(connected_sockets.begin() + i);
                        } else {
                            handleMsg(sd);
                        }
                    }
                }
                
                for (int i = 0; i < accepted_sockets.size(); i++) {
                    int sd = accepted_sockets[i];

                    if (FD_ISSET(sd, &readfds)) {
                        int valread = read(sd, recvd_msg, 1024);
                        
                        if (valread == 0) {
                            close(accepted_sockets[i]);
                            accepted_sockets.erase(accepted_sockets.begin() + i);
                        } else {
                            handleMsg(sd);
                        }
                    }
                }        

                if (intros == n && !asked) {
                    if (reqFile < f) {
                        for (int i = 0; i < connected_sockets.size(); i++) {
                            char request[1025];
                        
                            sprintf(request, "S %s %d", files[reqFile].c_str(), 0);
                            send(connected_sockets[i], request, strlen(request), 0);
                        }

                        reqFile++;
                        asked = true;
                        owner = 0;
                        replies = 0;
                    } else if (reqFile == f) {
                        for (int i = 0; i < connected_sockets.size(); i++) {
                            close(connected_sockets[i]);
                        }
                        connected_sockets.clear();
                        asked = true;
                    }
                }

                if (connected_sockets.empty() && accepted_sockets.empty()) {
                    break;
                }
 
            }
        }
};

int main(int argc, char **argv) { 
    if (argc == 3) {
        Client client(argv[1], argv[2]); 
        
        client.getOwnedFiles();
        client.main_loop();
    }

}
