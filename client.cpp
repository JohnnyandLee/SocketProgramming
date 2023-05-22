#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>
using namespace std;

void *get_in_addr(struct sockaddr*);
int initialSock();
int interact();
vector<string> handleReply(string);
string scheduleInterval(string);
vector<vector<int> > stringToVector(string);
string validOrNot(string, vector<vector<int> >);
bool checkValidInput(string);

const char* PORT = "24172"; // the port client will be connecting to
const int MAXDATASIZE = 8192; // max number of bytes we can get at once

// data structure below is used for establishing a TCP socket
int sockfd, numbytes;
char buf[MAXDATASIZE];
struct addrinfo hints, *servinfo, *p;
int rv;
char s[INET6_ADDRSTRLEN];

//bool twoLine = false;
int whichCase = 0;

int main(){
    cout<<"Client is up and running.\n";
    interact();
    return 0;
}

/** Initial a TCP socket connect to the Main Server
 *  This block of code refer to Beej's Guide page 32-33:A Simple Stream Client
 */
int initialSock(){

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
            //perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            //perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

    freeaddrinfo(servinfo); // all done with this structure

    return 0;
}

/** get sockaddr, IPv4 or IPv6:
 *
 */
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/** This is the function that the client can interact with the Main server.
 *  It will continuously ask the client to enter the usernames to check schedule availability.
 *  If there are some time slots the client can choose, then ask the client to choose 1 time slot.
 */
int interact(){

    while(1){
        initialSock();
        string input;
        do{
            cout<<"Please enter the usernames to check schedule availability:\n";
            getline(cin, input);
        }while(!checkValidInput(input));

        const char* cinput = input.c_str();
        if (send(sockfd, cinput, strlen(cinput), 0) == -1) perror("send");
        cout<<"Client finished sending the usernames to Main Server.\n";

        if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';
        
        //To get the dynamic port number, we need to use getsockname()
        //This refer to the EE450_Project_Spring_2023 page 18
        struct sockaddr_in my_addr;
        socklen_t addrlen = sizeof(my_addr);
        getsockname(sockfd, (struct sockaddr *)&my_addr, &addrlen);

        string s = buf;
        vector<string> res = handleReply(s);
        if(whichCase == 1){
            printf("Client received the reply from the Main Server using TCP over port %hu:\n",my_addr.sin_port);
            const char* name = res[0].c_str();
            const char* interval = res[1].c_str();
            printf("Time intervals %s works for %s.\n",interval , name);
            if(res[1].length() > 2){
                string req = scheduleInterval(res[1]);
                cout << "Sent the request to register " << req << " as the meeting time for " << name << ".\n";
                const char* creq = req.c_str();
                if (send(sockfd, creq, strlen(creq), 0) == -1) perror("send");
                if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }
                cout << "Received the notification that registration has finished.\n";
            }
        }
        else if(whichCase == 2){
            printf("Client received the reply from the Main Server using TCP over port %hu:\n",my_addr.sin_port);
            const char* badName = res[0].c_str();
            printf("%s do not exist.\n", badName);
        }
        else if(whichCase == 3){
            printf("Client received the reply from the Main Server using TCP over port %hu:\n",my_addr.sin_port);
            const char* badName = res[0].c_str();
            printf("%s do not exist.\n", badName);

            printf("Client received the reply from the Main Server using TCP over port %hu:\n",my_addr.sin_port);
            const char* name = res[1].c_str();
            const char* interval = res[2].c_str();
            printf("Time intervals %s works for %s.\n",interval , name);
            if(res[2].length() > 2){
                string req = scheduleInterval(res[2]);
                cout << "Sent the request to register " << req << " as the meeting time for " << name << ".\n";
                const char* creq = req.c_str();
                if (send(sockfd, creq, strlen(creq), 0) == -1) perror("send");
                if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }
                cout << "Received the notification that registration has finished.\n";
            }
        }

        cout<<"-----Start a new request-----\n";

        close(sockfd);
    }
    return 0;
}

/** This fuction handle the received message.
 *  Save invalid name, valid name and available time slots into vector<string>.
 */
vector<string> handleReply(string s){
    vector<string> res;
    int len = s.length();

    if(buf[1] == '@'){
        whichCase = 1;
        int i = 2;
        int from = 2;
        while(buf[i] != '[') i++;
        res.push_back(s.substr(from, i- from));
        res.push_back(s.substr(i, s.length()));
    }
    else if(buf[len - 1] == '@'){
        whichCase = 2;
        int i = 1;
        int from = 1;
        while(buf[i] != '@') i++;
        res.push_back(s.substr(from, i- from));
    }
    else{
        whichCase = 3;
        int i = 1;
        int from = 1;
        while(buf[i] != '@') i++;
        res.push_back(s.substr(from, i- from));
        from = i + 1;
        while(buf[i] != '[') i++;
        res.push_back(s.substr(from, i- from));
        res.push_back(s.substr(i, s.length()));
    }

    return res;
}

/** This fuction asks the client to pick a available time slot. 
 *  It will determine if the input time slot is valid or not. If invalid, pick again.
 */
string scheduleInterval(string intervals){
    vector<vector<int> > vec = stringToVector(intervals);

    cout << "Please enter the final meeting time to register an meeting:\n";

    string input;
    getline(cin, input);
    string inputModify = validOrNot(input, vec);
    while(inputModify.length() == 0){
        printf("Time interval %s is not valid. Please enter again:\n", input.c_str());
        getline(cin, input);
        inputModify = validOrNot(input, vec);
    }

    return inputModify;
}

/** 
 *  This fuction converts the available time slots(string) into vector<vector<int> >.
 */
vector<vector<int> > stringToVector(string s){
    vector<vector<int> > vec;
    int i = 1;
    while(1){
        if(s.at(i) == ' ' || s.at(i) == ',') i++;
        else if(s.at(i) == '['){
            i++;
            vector<int> v;

            int start = 0;
            while(s.at(i) != ','){
                if(s.at(i) == ' ') i++;
                else{
                    int temp = s.at(i) - '0';
                    start = start * 10 + temp;
                    i++;
                }
            }
            v.push_back(start);

            i++;
            
            int end = 0;
            while(s.at(i) != ']'){
                if(s.at(i) == ' ') i++;
                else{
                    int temp = s.at(i) - '0';
                    end = end * 10 + temp;
                    i++;
                }
            }
            v.push_back(end);
            vec.push_back(v);

            i++;
        }
        else if(s.at(i) == ']') break;
    }

    return vec;
}

/** This fuction will determine the input time slot is valid or not.
 */
string validOrNot(string input, vector<vector<int> > vec){
    if(input.length() < 5 || input.at(0) != '[' || input.at(input.length()-1) != ']') return "";

    bool comma = false;
    for(int j = 0; j < input.length(); j++){
        if(input.at(j) == ','){
            comma = true;
            break;
        }
    }
    if(!comma) return "";

    string res = "[";
    int start = 0;
    int end = 0;
    int i = 1;
    while(input.at(i) == ' ') i++;
    while(input.at(i) != ','){
        if(input.at(i) != ' '){
            int temp = input.at(i) - '0';
            if(temp < 0 || temp > 9) return "";
            start = start * 10 + temp;
        }
        i++;
    }
    stringstream start_ss;
    start_ss << start;
    string str_start = start_ss.str();
    res = res + str_start;
    res = res + ",";

    i++;
    while(input.at(i) == ' ') i++;
    while(input.at(i) != ']'){
        if(input.at(i) != ' '){
            int temp = input.at(i) - '0';
            if(temp < 0 || temp > 9) return "";
            end = end * 10 + temp;
        }
        i++;
    }
    stringstream end_ss;
    end_ss << end;
    string str_end = end_ss.str();
    res = res + str_end;
    res = res + "]";
    
    if(start >= end) return "";

    for(int j = 0; j < vec.size(); j++){
        if(start < vec[j][0]) return "";
        if(end <= vec[j][1]) return res;
    }

    return "";
}

/**   This fuction will check the client input is valid or not.
 */
bool checkValidInput(string s){
    for(int i = 0; i < s.length(); i++){
        if(s.at(i) != ' ') return true;
    }
    cout << "You enter all space, please enter valid names.\n";
    return false;
}