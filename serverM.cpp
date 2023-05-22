#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <sys/wait.h>
#include <signal.h>
using namespace std;

// data structure below is used for storing which name belongs to which server
set<string> serverA;
set<string>::iterator itrA;
set<string> serverB;
set<string>::iterator itrB;

const int MAXBUFLEN = 8192;
const char* MYPORT = "23172";

int getName();
void handleName(char*);
int sendQuery(string, char);
vector <string> splitString(string);
int initialSockM();
int initialSockA();
int initialSockB();
int initialSockT();
void sigchld_handler(int);
void *get_in_addr(struct sockaddr *);
int handleClient(string);
string mixHandle(string, string);
vector<vector<int> > stringToVector(string);
vector<vector <int> > findIntersect(vector<vector<int> >, vector<vector<int> >);
string replaceString(string);
void reviseInterval(int);
int sendRevise(string, char, string);

// data structure below is used for establishing a datagram socket server
int sockfdR;
struct addrinfo hintsR, *servinfoR, *pR;
int rvR;
struct sockaddr_storage their_addr;
char buf[MAXBUFLEN];
socklen_t addr_len;
char s[INET6_ADDRSTRLEN];

// data structure below is used for communicating with serverA via a UDP socket
int sockfdA;
struct addrinfo hintsA, *servinfoA, *pA;
int rvA;

// data structure below is used for communicating with serverB via a UDP socket
int sockfdB;
struct addrinfo hintsB, *servinfoB, *pB;
int rvB;

// data structure below is used for establishing a stream socket server
int sockfdT, new_fdT; // listen on sock_fd, new connection on new_fd
struct addrinfo hintsT, *servinfoT, *pT;
struct sockaddr_storage their_addrT; // connector's address information
socklen_t sin_size;
struct sigaction sa;
int yes = 1;
char s_T[INET6_ADDRSTRLEN];
int rvT;
int BACKLOG = 10; // how many pending connections queue will hold
char bufT[MAXBUFLEN];
string reply;

string toServerA;
string toServerB;
string toClient;
string badName;
const int GOFINDA = 1;
const int GOFINDB = 2;
const int GOFINDAB = 3;

int main(){  
    initialSockM();
    initialSockA();
    initialSockB();
    getName();
    getName();
     
    initialSockT();
    
    return 0;
}

/** Initial a UDP socket that serverA and serverB can connmunicate with Main Server
 *  This block of code refer to Beej's Guide page 34-35:A datagram sockets "server" demo
 */
int initialSockM(){
   memset(&hintsR, 0, sizeof hintsR);
   hintsR.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
   hintsR.ai_socktype = SOCK_DGRAM;
   hintsR.ai_flags = AI_PASSIVE; // use my IP
   
   if ((rvR = getaddrinfo("localhost", MYPORT, &hintsR, &servinfoR)) != 0) {
       fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rvR));
       return 1;
   }
   for(pR = servinfoR; pR != NULL; pR = pR->ai_next) {
      if ((sockfdR = socket(pR->ai_family, pR->ai_socktype,pR->ai_protocol)) == -1) {
           perror("listener: socket");
           continue;
      }

      if (bind(sockfdR, pR->ai_addr, pR->ai_addrlen) == -1) {
         close(sockfdR);
         perror("listener: bind");
         continue;
      }

      break;
    }

    if (pR == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfoR);
    addr_len = sizeof their_addr;
    cout<<"Main Server is up and running.\n";
    return 0; 
}

/** Initial a UDP socket that Main Server can connmunicate with ServerA
 *  This block of code refer to Beej's Guide page 36-37:A datagram "client" demo
 */
int initialSockA(){
    memset(&hintsA, 0, sizeof hintsA);
    hintsA.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hintsA.ai_socktype = SOCK_DGRAM;

    if ((rvA = getaddrinfo("localhost", "21172", &hintsA, &servinfoA)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rvA));
        return 1;
    }

 // loop through all the results and make a socket
    for(pA = servinfoA; pA != NULL; pA = pA->ai_next) {
        if ((sockfdA = socket(pA->ai_family, pA->ai_socktype,pA->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (pA == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    //freeaddrinfo(servinfoA);
}

/** Initial a UDP socket that Main Server can connmunicate with ServerB
 *  This block of code refer to Beej's Guide page 36-37:A datagram "client" demo
 */
int initialSockB(){
    memset(&hintsB, 0, sizeof hintsB);
    hintsB.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hintsB.ai_socktype = SOCK_DGRAM;

    if ((rvB = getaddrinfo("localhost", "22172", &hintsB, &servinfoB)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rvB));
        return 1;
    }

 // loop through all the results and make a socket
    for(pB = servinfoB; pB != NULL; pB = pB->ai_next) {
        if ((sockfdB = socket(pB->ai_family, pB->ai_socktype,pB->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (pB == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    //freeaddrinfo(servinfoB);
}

/** This function can get the name list either from serverA or serverB. 
 *  Main server stores the name list that it knows which name belongs to which server.
 */
int getName(){
    int numbytes;
    if ((numbytes = recvfrom(sockfdR, buf, MAXBUFLEN-1 , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    buf[numbytes] = '\0';
    handleName(buf);

    return 0;
}

/** This function handle the message came from either serverA or serverB. 
 *  It will store the name into a vector. Then store the name list in either in serverA or serverB set.
 */
void handleName(char* buf){
    if(buf[0] == 'A'){
        string temp = buf;
        string tempA = temp.substr(1, temp.length() - 1);
        vector<string> vec = splitString(tempA);
        for(int i = 0; i < vec.size(); i++){
            serverA.insert(vec[i]);
        }
        cout<<"Main Server received the username list from server A using UDP over port 23172.\n";
    }
    else if(buf[0] == 'B'){
        string temp = buf;
        string tempB = temp.substr(1, temp.length() - 1);
        vector<string> vec = splitString(tempB);
        for(int i = 0; i < vec.size(); i++){
            serverB.insert(vec[i]);
        }
        cout<<"Main Server received the username list from server B using UDP over port 23172.\n";
    }
}

/** This function handles the received message to a name list. 
 *  There are ';' symbols between each name.
 */
vector<string> splitString(string s){
    vector<string> res;
    int from = 0;
    int index = 0;
    while(index < s.length()){
        if(s.at(index) == ';'){
             string temp = s.substr(from, index - from);
             res.push_back(temp);
             from = index + 1;
        }
        index++;
    }
    string temp = s.substr(from, index - from);
    res.push_back(temp);
    return res;
}

/** This function will send the queries from the client to either serverA or serverB.
 *  The second argument c will determine which server to send. 
 *  If c = 'A', sends to serverA. If c = 'B', sends to serverB.
 */
int sendQuery(string query, char c){
    int sockfd;
    struct addrinfo *p;
    int numbytes;
    if(c == 'A'){
        sockfd = sockfdA;
        p = pA;
    }
    else{
        sockfd = sockfdB;
        p = pB;
    }
    
    const char * cname = query.c_str();
    
    if ((numbytes = sendto(sockfdA, cname, strlen(cname), 0,p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    if ((numbytes = recvfrom(sockfdR, buf, MAXBUFLEN-1 , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    buf[numbytes] = '\0';

    reply = buf;
    if(c == 'A'){
        cout << "Main Server received from server A the intersection result using UDP over port 23172:\n";
        cout << reply << ".\n";
    }
    else{
        cout << "Main Server received from server B the intersection result using UDP over port 23172:\n";
        cout << reply << ".\n";
    }

    return 0;
}

/** Initial a TCP socket that clients can connmunicate with Main Server
 *  This block of code refer to Beej's Guide page 30-32:A stream socket server demo
 *  It will create a parent TCP socket and wait for client to connect.
 *  If the parent TCP socket accepts a connection, it will create(fork) a child socket to handle the request.
 *  This function will determine each name belongs to which server. There could be invalid names from the client.
 *  The main server will then send the valid name to either serverA or serverB or both.
 *  When the main server gets the information from backend serverA or serverB, it will handle them and send to the client.
 *  If there are available time slots, it will wait the client to pick a time slot. 
 *  Then the main server will notify the backend server to change the available time slot for the valid names. 
 *  Once the backend server notifys the main server they are done, the main server will notify the client they are done.
 */
int initialSockT(){
    memset(&hintsT, 0, sizeof hintsT);
    hintsT.ai_family = AF_UNSPEC;
    hintsT.ai_socktype = SOCK_STREAM;
    hintsT.ai_flags = AI_PASSIVE; // use my IP

    if ((rvT = getaddrinfo(NULL, "24172", &hintsT, &servinfoT)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rvT));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(pT = servinfoT; pT != NULL; pT = pT->ai_next) {
        if ((sockfdT = socket(pT->ai_family, pT->ai_socktype, pT->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfdT, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfdT, pT->ai_addr, pT->ai_addrlen) == -1) {
            close(sockfdT);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfoT); // all done with this structure

    if (pT == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfdT, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1) { // main accept() loop
        sin_size = sizeof their_addrT;
        new_fdT = accept(sockfdT, (struct sockaddr *)&their_addrT, &sin_size);
        if (new_fdT == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addrT.ss_family, get_in_addr((struct sockaddr *)&their_addrT),s_T, sizeof s_T);
        cout<<"Main Server received the request from the client using TCP over port 24172.\n";

        if (!fork()) { // this is the child process
            close(sockfdT); // child doesn't need the listener
            
            int numbytes;
            if ((numbytes = recv(new_fdT, bufT, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }

            bufT[numbytes] = '\0';
            string res = "!";
            if(strlen(bufT) != 0){
                int whichCase = handleClient((string)bufT);

                bool waitClient = true;//0405+ 10% portion

                if(badName.length() != 0){ 
                    res += badName;
                    cout<<badName<<" do not exist. Send a reply to the client.\n";
                }
                res += "@";

                if(whichCase == GOFINDA){
                    cout<<"Found "<<toClient<<" located at Server A. Send to Server A.\n";
                    sendQuery(toServerA, 'A');
                    res += toClient;
                    res += reply;
                    if(reply.length() == 2) waitClient = false; //0405+ 10% portion
                }
                else if(whichCase == GOFINDB){
                    cout<<"Found "<<toClient<<" located at Server B. Send to Server B.\n";
                    sendQuery(toServerB, 'B');
                    res += toClient;
                    res += reply;
                    if(reply.length() == 2) waitClient = false; //0405+ 10% portion
                }
                else if(whichCase == GOFINDAB){
                    cout<<"Found "<<replaceString(toServerA)<<" located at Server A. Send to Server A.\n";
                    cout<<"Found "<<replaceString(toServerB)<<" located at Server B. Send to Server B.\n";
                    sendQuery(toServerA, 'A');
                    string s1 = reply;

                    sendQuery(toServerB, 'B');
                    string s2 = reply;
                    res += toClient;
                    string mix = mixHandle(s1, s2);
                    cout<<"Found the intersection between the results from server A and B:\n";
                    cout<<mix<<".\n";
                    res += mix;
                    if(mix.length() == 2) waitClient = false; //0405+ 10% portion
                }
                else if(whichCase == 0) waitClient = false; //0405+ 10% portion
                
                if (send(new_fdT, res.c_str() , res.length(), 0) == -1) perror("send");
                cout<<"Main Server sent the result to the client.\n";

                //0405+ 10% portion
                if(waitClient){
                    reviseInterval(whichCase);
                }
            }
            close(new_fdT);
            exit(0);
        }
        close(new_fdT); // parent doesn't need this
    }

    return 0;
}

/** This function will save the original error number.
 *  This block of code refer to Beej's Guide page 30:A stream socket server demo
 */
void sigchld_handler(int s){
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/** This function will determine the which name should the Main server send the query to.
 *  If return value = GOFINDA, the main server should just send query to serverA.
 *  If return value = GOFINDB, the main server should just send query to serverB.
 *  If return value = GOFINDAB, the main server should send query to both serverA and serverB.
 *  If return value = 0, the main server directly send those name are not valid(do not exist) back to the client.
 */
int handleClient(string s){
    toServerA = "";
    toServerB = "";
    toClient = "";
    badName = "";
    bool goA = false;
    bool goB = false;

    int i = 0;
    int from = 0;
    while(i < s.length()){
        if(s.at(i) == ' '){
            string temp = s.substr(from, i - from);
            if(serverA.find(temp) != serverA.end()){
                if(toServerA.length() != 0) toServerA += ";";
                if(toClient.length() != 0) toClient += ", ";
                
                toServerA += temp;
                toClient += temp;
                goA = true;
            }
            else if(serverB.find(temp) != serverB.end()){
                if(toServerB.length() != 0) toServerB += ";";
                if(toClient.length() != 0) toClient += ", ";
                
                toServerB += temp;
                toClient += temp;
                goB = true;
            }
            else{
                if(badName.length() != 0) badName += ", ";

                badName += temp;
            }
            from = i + 1;
            while(from < s.length() && s.at(from) == ' ') from++;
            i = from;
        }
        i++;
    }

    if(from < s.length() && s.at(from) != ' '){
        string temp = s.substr(from, s.length() - from);
        if(serverA.find(temp) != serverA.end()){
            if(toServerA.length() != 0) toServerA += ";";
            if(toClient.length() != 0) toClient += ", ";
            toServerA += temp;
            toClient += temp;
            goA = true;
        }
        else if(serverB.find(temp) != serverB.end()){
            if(toServerB.length() != 0) toServerB += ";";
            if(toClient.length() != 0) toClient += ", ";
            toServerB += temp;
            toClient += temp;
            goB = true;
        }
        else{
            if(badName.length() != 0) badName += ", ";
            badName += temp;
        }
    }

    if(goA && !goB) return GOFINDA;
    else if(!goA && goB) return GOFINDB;
    else if(goA && goB) return GOFINDAB;

    return 0;
}

/** This function handle the case for receiving time intervals from both serverA and serverB.
 *  It will call the findIntersect to get the intersection between them and return the string.
 */
string mixHandle(string s1, string s2){
    vector<vector<int> > vec1 = stringToVector(s1);
    vector<vector<int> > vec2 = stringToVector(s2);
    vector<vector<int> > vec = findIntersect(vec1, vec2);

    string res = "[";
    for(int i = 0; i < vec.size(); i++){
        vector <int> v = vec[i];
        res = res + "[";
        ostringstream str0;
        str0 << v[0];
        res = res + str0.str() + ",";
        ostringstream str1;
        str1 << v[1];
        res = res + str1.str();
        if(i != vec.size() - 1) res = res + "],";
        else res = res + "]";
    }

    res = res + "]";
    
    return res;
}

/** This function converts the string for available time slot to a vector.
 *  
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

/** This function will find the intersection of available time slots for serverA and serverB.
 *
 */
vector<vector <int> > findIntersect(vector<vector<int> > vec1, vector<vector<int> > vec2){
    
    vector<vector <int> > vec;
    int count[101];
    for(int i = 0; i < 101; i++) count[i] = 0;
    
    for(int j = 0; j < vec1.size(); j++){
        vector<int> v = vec1[j];
        for(int k = v[0]; k < v[1]; k++) count[k]++;
    }

    for(int j = 0; j < vec2.size(); j++){
        vector<int> v = vec2[j];
        for(int k = v[0]; k < v[1]; k++) count[k]++;
    }
    

    int i = 0;
    while(i < 101){
        if(count[i] == 2){
            vector<int> v;
            v.push_back(i);
            i++;
            while(count[i] == 2){
                i++;
            }
            v.push_back(i);
            vec.push_back(v);
            i++;
        }
        else i++;
    }
    
    return vec;
}

/** This function just replaces the string contains ';' to ', '
 *
 */
string replaceString(string s){
    string res = "";
    int from = 0;
    int i = 0;
    while(i < s.length()){
        if(s.at(i) == ';'){
            res += s.substr(from, i - from); 
            res += ", ";
            from = i + 1;
        }
        i++;
    }
    res += s.substr(from, s.length() - from); 
    return res;
}

/** This function will get the final time slot chose from the client.
 *  It will call sendRevise to send the registration message to either serverA or serverB or both.
 */
void reviseInterval(int whichCase){
    int numbytes;
    if ((numbytes = recv(new_fdT, bufT, MAXBUFLEN-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    if(bufT[0] != '[') return;

    bufT[numbytes] = '\0';
    cout<<"Main Server received the resgistration request from the client.\n";

    if(whichCase == GOFINDA){
        sendRevise(toServerA, 'A', (string)bufT);
    }
    else if(whichCase == GOFINDB){
        sendRevise(toServerB, 'B', (string)bufT);
    }
    else if(whichCase == GOFINDAB){
        sendRevise(toServerA, 'A', (string)bufT);
        sendRevise(toServerB, 'B', (string)bufT);
    }
    
    string res = "Resgistration has finished";
    if (send(new_fdT, res.c_str() , res.length(), 0) == -1) perror("send");
    cout<<"Main Server sent the resgistration has finished to the client.\n";
}

/** This function will send the registration message to either serverA or serverB or both.
 *  If c = 'A', send the registration message to serverA.
 *  If c = 'B', send the registration message to serverB.
 *  Registration time slot is the third argument interval.
 */
int sendRevise(string name, char c, string interval){
    int sockfd;
    struct addrinfo *p;
    int numbytes;
    if(c == 'A'){
        sockfd = sockfdA;
        p = pA;
    }
    else{
        sockfd = sockfdB;
        p = pB;
    }

    string info = "$" + name + interval;
    
    const char * cname = info.c_str();
    
    if ((numbytes = sendto(sockfdA, cname, strlen(cname), 0,p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    printf("Main server sent the registration request to server %c\n", c);

    if ((numbytes = recvfrom(sockfdR, buf, MAXBUFLEN-1 , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    
    buf[numbytes] = '\0';
    
    if(c == 'A') cout << "Main Server received from server A the registration has finished using UDP over port 23172:\n";
    else cout << "Main Server received from server B the registration has finished using UDP over port 23172:\n";

    return 0;
}