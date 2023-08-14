#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <openssl/md5.h>
#include <thread>
#include <unistd.h>

class Client {
    std::string config, dir;
    int id, port, unique_id; 
    int n; // no of neighbours of this client
    int (*neighbours)[4];
    int f; // no of files
    std::vector <std::string> files;
    std::vector <std::string> ownedFiles; 
    char recvd_msg[1025];

    std::vector<int> accepted_sockets, connected_sockets, downloading_sockets;
    
    std::map <std::string, int> responseIds; 
    std::map <std::string, int> noOfReplies;

    int reqFile, owner, dpth, ownerPort, replies;
    bool asked;

    int intros, readyNeighbours, finishedNeighbours;
    std::vector <std::string> introMsgs;
    bool sentReady;

    public:
        Client(char *config, char *dir) {
            this->config = std::string(config);
            this->dir = std::string(dir); 
            
            reqFile = 0;
            owner = 0;
            asked = false;

            intros = 0;
            readyNeighbours = 0;
            finishedNeighbours = 0;
            sentReady = false; 
            
            mkdir(strcat(dir, "./Downloaded/"), 0777);
            loadConfig();
        }

        void loadConfig() {
            std::fstream config_file;
            config_file.open(config);

            if (config_file.is_open()) {
                config_file >> id >> port >> unique_id; 
                config_file >> n;
                
                neighbours = new int[n][4];
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
            DIR* dirp = opendir(dir.c_str());
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
                sleep(0.10);
            }

            return sock;
        }

        void sendIntro(int socket) {
            char intro[1025];

            sprintf(intro, "I %d %d %d \x01", id, unique_id, port);

            send(socket, intro, strlen(intro), 0);
        }

        bool isSocketDownloading(int socket) {
            auto it = std::find(downloading_sockets.begin(), downloading_sockets.end(), socket);
            return (it != downloading_sockets.end()); 
        }

        void removeDownloading(int socket) {
            auto it = std::find(downloading_sockets.begin(), downloading_sockets.end(), socket);
            downloading_sockets.erase(it);
        }
        
        void findFileAndSend(std::string filename, int depth, int socket, int isOwnFile, int isLastFile, int whoWants) {
            auto it = std::find(ownedFiles.begin(), ownedFiles.end(), filename);
            bool lmao = false;
            
            std::string fid = filename + std::to_string(whoWants);

            if (it != ownedFiles.end()) {
                responseIds.insert({fid, unique_id}); 
                lmao = true;
            } else {
                responseIds.insert({fid, 0}); 
            }
            
            noOfReplies.insert({fid, 0});

            if (depth > 0) {
                for (int i = 0; i < accepted_sockets.size(); i++) {
                    char request[1025];
                
                    sprintf(request, "S %s %d %d %d %d \x01", filename.c_str(), 0, 0, 0, whoWants);
                    send(accepted_sockets[i], request, strlen(request), 0);
                }
                
                auto repliesRecvd = noOfReplies.find(fid);
                while (repliesRecvd == noOfReplies.end() || repliesRecvd->second < n) {
                    repliesRecvd = noOfReplies.find(fid);
                    sleep(0.10);
                }
            }
            
            char resp[1024];
            int ownerFound = responseIds[fid];
            int depthFound = (ownerFound == 0) ? 0 : (ownerFound == unique_id) ? 1 : 2;
            int portFound; 
            
            if (lmao) {
                ownerFound = unique_id;
                depthFound = 1;
                portFound = port; 
            } else {
                ownerFound = responseIds[filename];
            
                depthFound = (ownerFound == 0) ? 0 : (ownerFound == unique_id) ? 1 : 2;
                if (ownerFound == 0) {
                    portFound = 0;
                } else if (ownerFound == unique_id) {
                    portFound = port; 
                } else {
                    portFound = getPortFromUniqueId(ownerFound);
                }
            }
           

            sprintf(resp, "F %s %d %d %d %d %d \x01", filename.c_str(), ownerFound, isOwnFile, depthFound, portFound, whoWants);
            send(socket, resp, strlen(resp), 0);
            
            if (isLastFile) {
                finishedNeighbours++;
            }

        }

        int getPortFromUniqueId(int unique_id) {
            for (int i = 0; i < n; i++) {
                if (neighbours[i][3] == unique_id) {
                    return neighbours[i][1]; 
                }   
            }

            printf("No port with uniqueID, %d\n", unique_id);
            exit(1);

        }

        void setUniqueIdFromSocket(int unique_id, int socket) {
            for (int i = 0; i < n; i++) {
                if (neighbours[i][2] == socket) {
                    neighbours[i][3] = unique_id;
                    break;
                }   
            }   
        }   

        int getSocketFromUniqueId(int unique_id) {
            for (int i = 0; i < n; i++) {
                if (neighbours[i][3] == unique_id) {
                    return neighbours[i][2]; 
                }   
            }

            printf("No socket with uniqueID, %d\n", unique_id);
            exit(1);
        }

        int getFileSize(FILE *fp) {
            fseek(fp, 0, SEEK_END);
            int size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            return size;
        }

        void printMD5(std::string path) {
            FILE *fp = fopen(path.c_str(), "r");

            unsigned char hash[MD5_DIGEST_LENGTH];

            MD5_CTX mdContext;
            MD5_Init(&mdContext);

            char data[1024];
            int bytes;
            while ((bytes = fread(data, 1, 1024, fp)) != 0) {
                MD5_Update(&mdContext, data, bytes);
            }

            MD5_Final(hash, &mdContext);

            for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
                printf("%02x", hash[i]);
            }

            fclose(fp);
        }

        void downloadCurrentFile() {
            int sock;
           
            if (owner == 0) {
                std::cout << "Found " << files[reqFile - 1] << " at " << owner << " with MD5 " << 0 << " at depth " << 0 << "\n";
            } else {
                char buffer[1024];
                
                if (dpth == 1) {
                    sock = getSocketFromUniqueId(owner);
                } else {
                    sock = clientConnect("127.0.0.1", ownerPort); 
                    read(sock, buffer, sizeof(buffer)); 
                }

                sprintf(buffer, "D %s %d \x01", files[reqFile - 1].c_str(), dpth);
                send(sock, buffer, strlen(buffer), 0);

                memset(buffer, '\0', sizeof(buffer));
                
                read(sock, buffer, sizeof(buffer));

                int sz;
                sscanf(buffer, "%d", &sz);

                send(sock, "OK", 2, 0);

                std::string fullPath = dir + "./Downloaded/" + std::string(files[reqFile - 1]);
                FILE *fp = fopen(fullPath.c_str(), "w");

                while (sz > 0) {
                    int count = read(sock, buffer, sizeof(buffer));
                    fwrite(buffer, 1, count, fp);
                    sz -= count;
                }

                fclose(fp);

                std::cout << "Found " << files[reqFile - 1] << " at " << owner << " with MD5 ";
                printMD5(fullPath);
                std::cout << " at depth " << dpth << "\n";

                if (dpth != 1) {
                    close(sock);
                } else {
                    removeDownloading(sock);
                }
            }

            asked = false;

            send(connected_sockets[0], "E \x01", 3, 0); 
        }

        void handleMsg(int socket) {
            char resp[1025];
 
            switch (recvd_msg[0]) {
                case 'I': {
                    int id, unique_id, port;
                    
                    sscanf(recvd_msg, "I %d %d %d", &id, &unique_id, &port);

                    setUniqueIdFromSocket(unique_id, socket);

                    char m[1024];
                    sprintf(m, "Connected to %d with unique-ID %d on port %d\n", id, unique_id, port);  
                    std::string introMsg(m);

                    introMsgs.push_back(introMsg);

                    intros++;
                    if (intros == n) {
                        sort(introMsgs.begin(), introMsgs.end());

                        for (auto it : introMsgs) {
                            std::cout << it ;
                        }

                    }

                } break;
                case 'S': {
                    
                    char f[1025];
                    int depth, isOwnFile, isLastFile, whoWants;

                    sscanf(recvd_msg, "S %s %d %d %d %d", f, &depth, &isOwnFile, &isLastFile, &whoWants);
                    std::string filename(f); 
                    
                    std::thread barm(&Client::findFileAndSend, this, filename, depth, socket, isOwnFile, isLastFile, whoWants);
                    barm.detach();
                } break;
                case 'F': {
                    
                    int owner_recv, isOwnFile, depth, prt, whoWanted;
                    char f[1025];
                    
                    sscanf(recvd_msg, "F %s %d %d %d %d %d", f, &owner_recv, &isOwnFile, &depth, &prt, &whoWanted);
                    
                    if (isOwnFile) { 
                        replies++;
                        if (owner_recv != 0) {
                            if (owner == 0 || owner_recv < owner) {
                                owner = owner_recv;
                                dpth = depth;
                                ownerPort = prt;
                            } 
                        } 

                        if (replies == n) {
                            if (owner != 0 && dpth == 1) {
                                downloading_sockets.push_back(getSocketFromUniqueId(owner));    
                            }

                            std::thread barm(&Client::downloadCurrentFile, this);
                            barm.detach();
                        }
                    } else {
                        std::string filename(f);

                        std::string fid = filename + std::to_string(whoWanted); 

                        int currOwner = responseIds[fid];
                        
                        if (owner_recv != 0) {
                            if (currOwner == 0 || owner_recv < currOwner) {
                                responseIds[fid] = owner_recv;
                                ownerPort = prt;
                            }
                        }
                        
                        noOfReplies[fid]++;
                    }

                } break;
                case 'R': {
                    readyNeighbours++;
                } break;
                case 'D': {

                    char filename[1025];
                    int bruhDepth;

                    sscanf(recvd_msg, "D %s %d", filename, &bruhDepth);

                    std::string fullPath = dir + "/" + std::string(filename);

                    FILE *fp = fopen(fullPath.c_str(), "r");

                    int sz = getFileSize(fp);
                    sprintf(resp, "%d", sz);
                    send(socket, resp, strlen(resp), 0);

                    // should contain OK from the client
                    read(socket, resp, 2);

                    int count;
                    while ((count = fread(resp, 1, sizeof(resp), fp)) > 0) {
                        send(socket, resp, count, 0);
                    }

                    fclose(fp);

                    if (bruhDepth > 1) {
                        close(socket);
                        accepted_sockets.erase(std::find(accepted_sockets.begin(), accepted_sockets.end(), socket));
                    }

                } break;
                case 'E': {
                    send(socket, "e \x01", 3, 0);
                }
            }
                         
            // if we ended up sending two requests
            char *delimeter = strchr(recvd_msg, '\x01');
            
            if (delimeter != NULL) { 
                memset(resp, '\0', sizeof(resp));
                strcpy(resp, delimeter + 1);
                
                memset(recvd_msg, '\0', sizeof(recvd_msg));
                strcpy(recvd_msg, resp);
                
                if (recvd_msg[0]) {
                    handleMsg(socket);
                }
            }
        }

        void main_loop(){
            if (n == 0){
                for (auto it : files){
                    std::cout << "Found " << it.c_str() << " at " << 0 << " with MD5 " << 0 << " at depth " << 0 << std::endl;
                }
                return;
            }    
            int server_socket;
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
                neighbours[i][2] = client_socket;
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
                        if (std::find(downloading_sockets.begin(), downloading_sockets.end(), sd) == downloading_sockets.end()) {
                            int valread = read(sd, recvd_msg, 1024);
                           
                            if (valread == 0) {
                                close(connected_sockets[i]);
                                connected_sockets.erase(connected_sockets.begin() + i);
                            } else {
                                handleMsg(sd);
                            }
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
                
                if (intros == n) {
                    //printf("asked = %d, reqFile = %d, readyNeighbours = %d, finishedNeighbours = %d\n", asked, reqFile, readyNeighbours, finishedNeighbours); 
                    if (!sentReady) {
                        for (int i = 0; i < connected_sockets.size(); i++) {
                            send(connected_sockets[i], "R \x01", 3, 0);
                        }

                        sentReady = true;
                    }

                    if (readyNeighbours == n) {
                        if (!asked) {
                            if (reqFile < f) {
                                for (int i = 0; i < connected_sockets.size(); i++) {
                                    char request[1025];
                                    
                                    int isLastFile = (reqFile == f-1); 

                                    sprintf(request, "S %s %d %d %d %d \x01", files[reqFile].c_str(), 1, 1, isLastFile, unique_id);

                                    send(connected_sockets[i], request, strlen(request), 0);

                                }

                                reqFile++;
                                asked = true;
                                owner = 0;
                                replies = 0;
                            } else if (reqFile == f && finishedNeighbours == n) {
                                for (int i = 0; i < accepted_sockets.size(); i++){
                                    close(accepted_sockets[i]);
                                }

                                accepted_sockets.clear();
                                asked = true;
                            }
                        }
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
