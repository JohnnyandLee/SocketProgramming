#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <cstring>
#include <sstream>
using namespace std;

void readFile();
void handleInput(string);
vector<vector <int> > findIntersect(string);
vector<string> splitString(string);
int sendName();
int openServer();
string getNameList();
string handleName(char*);
string replaceString(string);
void updateData(string);
vector<string> getReviseName(string);
vector<int> getReviseInterval(string);
string vectorTostring(vector<vector<int> >);
vector<vector <int> > updateInterval(string, vector<int>);

// data structure below is used for storing which name with its available time intervals
map<string, vector<vector <int> > > intervals;
map<string, vector<vector <int> > >::iterator iter;
const int MAXBUFLEN = 8192;
const char* MYPORT = "22172";

int initialSockM();
int initialSockB();

// data structure below is used for establishing a datagram socket server
int sockfdR;
struct addrinfo hintsR, *servinfoR, *pR;
int rvR;
struct sockaddr_storage their_addr;
char buf[MAXBUFLEN];
socklen_t addr_len;
char s[INET6_ADDRSTRLEN];

// data structure below is used for communicating with main server via a UDP socket
int sockfdM;
struct addrinfo hintsM, *servinfoM, *pM;
int rvM;

int main(int argc, char* argv[]){
    initialSockM();
    initialSockB();
    readFile();
    sendName();

    openServer();
    
    return 0;
}

/** This function reads a given file "b.txt".
 *  Each line represents available time intervals for the given name.
 *  It will call handleInput to save the information to the map data structure.
 */
void readFile(){
    fstream file;
    file.open("b.txt");
    string s;
    while(getline(file, s)){
        handleInput(s);
    }
    file.close();
}

/** This function will handle the given line to store the information to the map data structure.
 *  It will handle some cases containing several while space ' '.
 */
void handleInput(string s){
    int namefrom = 0;
    while(s.at(namefrom) == ' ') namefrom++;
    
    int len = 1;
    while(s.at(namefrom + len) != ' ' && s.at(namefrom + len) != ';') len++;
    
    string name = s.substr(namefrom, len);

    int index = namefrom + len;
    while(s.at(index) != '[') index++;
    vector<vector<int> > vec;
    
    index++;
    while(1){
        if(s.at(index) == ' ' || s.at(index) == ',') index++;
        else if(s.at(index) == '['){
            index++;
            vector<int> v;

            int start = 0;
            while(s.at(index) != ','){
                if(s.at(index) == ' ') index++;
                else{
                    int temp = s.at(index) - '0';
                    start = start * 10 + temp;
                    index++;
                }
            }
            v.push_back(start);

            index++;
            
            int end = 0;
            while(s.at(index) != ']'){
                if(s.at(index) == ' ') index++;
                else{
                    int temp = s.at(index) - '0';
                    end = end * 10 + temp;
                    index++;
                }
            }
            v.push_back(end);
            vec.push_back(v);

            index++;
            
        }
        else if(s.at(index) == ']') break;
    }
    intervals.insert(pair<string, vector<vector <int> > >(name, vec));
}

/** This function will find the intersect of available time intervals for the several given names.
 *  It will call splitString to get the name list(vector<string>).
 */
vector<vector <int> > findIntersect(string s){
    
    vector<vector <int> > vec;
    int count[101];
    for(int i = 0; i < 101; i++) count[i] = 0;
    
    vector <string> name = splitString(s);
    for(int i = 0; i < name.size(); i++){
        iter = intervals.find(name[i]);
        vector<vector <int> > vec1 = iter->second;
        for(int j = 0; j < vec1.size(); j++){
            vector<int> v = vec1[j];
            for(int k = v[0]; k < v[1]; k++) count[k]++;
        }
    }

    int i = 0;
    while(i < 101){
        if(count[i] == name.size()){
            vector<int> v;
            v.push_back(i);
            i++;
            while(count[i] == name.size()){
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

/** This function will split the string to a name list(vector<string>).
 *  There are ';' symbols between each names.
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

/** This function will send the names in serverA to the main server.
 *  It will call gerNameList to get the sending message.
 */
int sendName(){
    int numbytes;
    string name = getNameList();
    const char * cname = name.c_str();

    if ((numbytes = sendto(sockfdM, cname, strlen(cname), 0,pM->ai_addr, pM->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    cout << "ServerB finished sending a list of usernames to Main Server.\n";

    return 0;
}

/** This function will interate the map data structure to get the names containing in serverA.
 *  Forming a string that contains all names in serverA.
 *  There are ';' symbols between each names.
 */
string getNameList(){
    string res = "B";
    for(iter = intervals.begin(); iter != intervals.end(); iter++){
        res = res + iter->first;
        res = res + ";";
    }
    
    int len = res.length();
    return res.substr(0, len - 1);
}

/** This function will wait the message from the Main server.
 *  If the first character is '$', it means serverB needs to update its time intervals.
 *  If the first character is not '$', it means serverB needs to find the intersection for the query names.
 *  It will send the required message back to the Main server and wait message again. 
 */
int openServer(){
    int numbytes;

    while(1){
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfdR, buf, MAXBUFLEN-1 , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        buf[numbytes] = '\0';
        if(buf[0] == '$') updateData((string)buf);
        else {
            cout << "Server B received the usernames from Main Server using UDP over port 22172.\n";

            string res = handleName(buf);
            string temp = buf;
            string name = replaceString(temp);
            cout << "Found the intersection result: " << res << " for " << name <<".\n";

            const char * cname = res.c_str();

            if ((numbytes = sendto(sockfdM, cname, strlen(cname), 0,pM->ai_addr, pM->ai_addrlen)) == -1) {
                perror("talker: sendto");
                exit(1);
            }

            cout << "Server B finished sending the response to Main Server.\n";
        }
    }
    return 0;
}

/** This function will call findIntersect to get the final available time slots.
 *  Then it will convert the time slots to string.
 */
string handleName(char* buf){
    string temp = buf;
    vector<vector <int> > vec = findIntersect(temp);
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

/** Initial a UDP socket that ServerB can connmunicate with Main Server
 *  This block of code refer to Beej's Guide page 36-37:A datagram "client" demo
 */
int initialSockM(){
    memset(&hintsM, 0, sizeof hintsM);
    hintsM.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hintsM.ai_socktype = SOCK_DGRAM;

    if ((rvM = getaddrinfo("localhost", "23172", &hintsM, &servinfoM)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rvM));
        return 1;
    }

 // loop through all the results and make a socket
    for(pM = servinfoM; pM != NULL; pM = pM->ai_next) {
        if ((sockfdM = socket(pM->ai_family, pM->ai_socktype,pM->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (pM == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    return 0;
}

/** Initial a UDP socket that Main Server can connmunicate with ServerB
 *  This block of code refer to Beej's Guide page 34-35:A datagram sockets "server" demo
 */
int initialSockB(){
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

    cout << "ServerB is up and running using UDP on port 22172.\n";

    return 0;
}

/** This function just replaces the string contains ';' to ', '
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

/** This function will update the intervals from client picked.
 *  It will call getReviseName to get the names needed to be updated
 *  It will call getReviseInterval to get the interval from client picked.
 *  When it is done, it will send the message to the Main server.
 *  The argument format will be s:$alice;bob[1,2]
 */
void updateData(string s){
    vector<string> name = getReviseName(s);
    vector<int> interval = getReviseInterval(s);
    
    printf("Register a meeting at [%d,%d] and update the availability for the following users:\n", interval[0], interval[1]);

    for(int i = 0; i < name.size(); i++){
        iter = intervals.find(name[i]);
        vector<vector <int> > vec = iter->second;
        cout << name[i] << " updated from " << vectorTostring(vec) << "to ";

        updateInterval(name[i], interval);
        
        iter = intervals.find(name[i]);
        vector<vector <int> > vec2 = iter->second;
        cout << vectorTostring(vec2) << endl;
    }

    string res = "OK";
    const char * cname = res.c_str();
    int numbytes;

    if ((numbytes = sendto(sockfdM, cname, strlen(cname), 0,pM->ai_addr, pM->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    
    cout << "Notified Main Server that registration has finished.\n";
}

/** This function will get the name list from argument s.
 */
vector<string> getReviseName(string s){
    vector<string> res;
    int index = 1;
    int from = 1;
    while(s.at(index) != '['){
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

/** This function will get the picked interval from argument s.
 */
vector<int> getReviseInterval(string s){
    vector<int> res;
    int index = 1;
    while(s.at(index) != '[') index++;
    index++;

    int start = 0;
    int end = 0;

    while(s.at(index) != ','){
        int temp = s.at(index) - '0';
        start = start * 10 + temp;
        index++;
    }
    res.push_back(start);

    index++;
    while(s.at(index) != ']'){
        int temp = s.at(index) - '0';
        end = end * 10 + temp;
        index++;
    }
    res.push_back(end);

    return res;
}

/** This function will convert the vector to a string format.
 */
string vectorTostring(vector<vector<int> > vec){
    string res = "[";
    for(int i = 0; i < vec.size(); i++){
        vector<int> v = vec[i];
        if(i != 0) res = res + ",";
        res = res + "[";

        int a = v[0];
        stringstream sa;
        sa << a;
        string stra = sa.str();
        res = res + stra + ",";

        int b = v[1];
        stringstream sb;
        sb << b;
        string strb = sb.str();
        res = res + strb;

        res = res + "]";
    }
    
    res = res + "]";
    return res;
}

/** This function will update the map.
 *  It will use the map to find the original time intervals.
 *  The second argument is the picked time slot, so use it to update the original time intervals.
 *  There are 4 condition needed to be handled.
 *  1. *-----*  2. *-----* 3.*-----*  4. *-----*
 *     *-----*     *---*        *--*      *---*
 */
vector<vector <int> > updateInterval(string name, vector<int> interval){
    int start = interval[0];
    int end = interval[1];
    vector<vector <int> > vec;
    iter = intervals.find(name);
    vector<vector <int> > orignalVec = iter->second;

    for(int i = 0; i < orignalVec.size(); i++){
        if(start == orignalVec[i][0] && end == orignalVec[i][1]);
        else if(start == orignalVec[i][0] && end < orignalVec[i][1]){
            vector <int> temp;
            temp.push_back(end);
            temp.push_back(orignalVec[i][1]);
            vec.push_back(temp);
        }
        else if(start > orignalVec[i][0] && end == orignalVec[i][1]){
            vector <int> temp;
            temp.push_back(orignalVec[i][0]);
            temp.push_back(start);
            vec.push_back(temp);
        }
        else if(start > orignalVec[i][0] && end < orignalVec[i][1]){
            vector <int> temp1;
            temp1.push_back(orignalVec[i][0]);
            temp1.push_back(start);
            vec.push_back(temp1);

            vector <int> temp2;
            temp2.push_back(end);
            temp2.push_back(orignalVec[i][1]);
            vec.push_back(temp2);
        }
        else vec.push_back(orignalVec[i]);
    }
    
    iter = intervals.find(name);
    iter->second = vec;
    return vec;
}