#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <cerrno>
#include <map>
#include <machine/endian.h>
#include <bitset>
#include <vector>

#define err_quit(m) { perror(m); exit(-1); }

using namespace std;

typedef struct header_t{
    int16_t ID;
    uint8_t flags[2];
    uint16_t QDCOUNT;
    uint16_t ANCOUNT;
    uint16_t NSCOUNT;
    uint16_t ARCOUNT;
} __attribute__((packed)) header_t;

typedef struct {
    int sub;
	char* NAME;
    uint32_t TTL;
    char* CLASS;
    char* TYPE;
    char* RDATA;
}   domain_record;

typedef struct question{
    int nip;
    char* ip;
    int answerd;
    char* QNAME;
    uint16_t QTYPE;
    uint16_t QCLASS;
} __attribute__ ((packed)) question;

typedef struct resource{
    char* NAME;
    uint16_t TYPE;
    uint16_t CLASS;
    uint32_t TTL;
    uint16_t RDLENGTH;
    char* RDATA;
    domain_record* record;
} __attribute__ ((packed)) resource;

char* forward_ip;
map<string, string> domain_info;
map<string, map<string, vector<domain_record>>> domain_search;
map<uint16_t, string> value_type = {{1, "A"}, {28, "AAAA"}, {2, "NS"}, {5, "CNAME"}, {6, "SOA"}, {15, "MX"}, {16, "TXT"}};
map<string, uint16_t> type_value = {{"A", 1}, {"AAAA", 28}, {"NS", 2}, {"CNAME", 5}, {"SOA", 6}, {"MX", 15}, {"TXT", 16}};
map<uint16_t, string> value_class = {{1, "IN"}};
map<string, uint16_t> class_value = {{"IN", 1}};

void read_zone(string path){
    fstream fp;
    //cout << path << endl;
    fp.open(path,ios::in); //open a file to perform read operation using file object
    //cout << path << endl;
    if (!fp.is_open()){
        cout << "zone file open failed!" << endl;
        cout << strerror(errno) << '\n';
		return;
	}
    domain_search.clear();

    string str;
    int first = 0;
    char* dname;
    while(getline(fp, str)){ //read data from file object and put it into string.
        //cout << str << "\n"; //print the data of the string
        char* tmp = strdup(str.c_str());
        if (first == 0){
            dname = strdup(strtok(tmp, " \n\r"));
            //int len = strlen(dname);
            //dname[len - 1] = '\0';
            first = 1; 
        } else {
            domain_record dr;
            dr.NAME = strdup(strtok(tmp, ",\n\r"));
            dr.TTL = stoul(strtok(NULL, ",\n\r"), nullptr, 0);
            dr.CLASS = strdup(strtok(NULL, ",\n\r"));
            dr.TYPE = strdup(strtok(NULL, ",\n\r"));
            dr.RDATA = strdup(strtok(NULL, ",\n\r"));

            if (!strcmp(dr.NAME, "@")){
                dr.NAME = strdup(dname);
                dr.sub = 0;
            } else {
                dr.sub = 1;
                char* subdom = strdup(dr.NAME);
                dr.NAME = new char[strlen(subdom) + 1 + strlen(dname) + 1];
                memset(dr.NAME, 0, strlen(subdom) + 1 + strlen(dname) + 1);
                memcpy(dr.NAME, subdom, strlen(subdom));
                dr.NAME[strlen(subdom)] = '.';
                memcpy(&dr.NAME[strlen(subdom) + 1], dname, strlen(dname));
            }

            domain_search[dr.NAME][dr.TYPE].push_back(dr);
        }
        free(tmp);
    }
    //cout << dname << endl;
    fp.close(); //close the file object.
}

void read_config(string path) {
    fstream fp;
    fp.open(path,ios::in); //open a file to perform read operation using file object
    if (!fp.is_open()){
        cout << "configuration file open failed!" << endl;
		return;
	}

    string str;
    int first = 0;
    while(getline(fp, str)){ //read data from file object and put it into string.
        //cout << str << "\n"; //print the data of the string
        char* tmp = strdup(str.c_str());
        if (first == 0){
            forward_ip = strdup(strtok(tmp, " \n\r"));
            first = 1;
        } else {
            char* domain = strdup(strtok(tmp, " ,\n\r"));
            char* path_to_file = strdup(strtok(NULL, " ,\n\r"));
            //cout << domain << " " << path_to_file << endl;
            //read_zone(path_to_file);
            domain_info[domain] = path_to_file;
        }
        free(tmp);
    }
    fp.close(); //close the file object.
}

int forward(int len, char* send, char* receive){
    header_t* header_d = (header_t*) send;
    cout << header_d->ID << " " << header_d->QDCOUNT << " " << header_d->ANCOUNT << " " << header_d->NSCOUNT << " " << header_d->ARCOUNT << endl;
    header_d->ID = le16toh(header_d->ID);
    header_d->QDCOUNT = be16toh(1);
    header_d->ANCOUNT = le16toh(header_d->ANCOUNT);
    header_d->NSCOUNT = le16toh(header_d->NSCOUNT);
    header_d->ARCOUNT = le16toh(header_d->ARCOUNT);

    struct sockaddr_in sin, csin;
	socklen_t sinlen = sizeof(sin);
    int sockfd, s;


    setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(53);
    cout << forward_ip << endl;
    if (inet_pton(AF_INET, forward_ip, &sin.sin_addr) <= 0)
	    perror("inet_pton error");

	if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");

    int rlen;
    int send_ = sendto(sockfd, send, len, 0, (struct sockaddr*) &sin, sizeof(sin));
    while(1) {
		memset(receive, 0, 1000);
		if((rlen = recvfrom(sockfd, receive, 1000, 0, (struct sockaddr*) &sin, (socklen_t *) &sinlen)) < 0) {
            sendto(sockfd, send, len, 0, (struct sockaddr*) &sin, sizeof(sin));
            //cout << "not receive\n";    
		} else {
            cout << "forward receive length: " << rlen << endl;
			break;
		}
	}
    close(sockfd);
    return rlen;
}

int packed_rdata(char* type, char* rdata, char* tmp){
    memset(tmp, 0, 100);
    //cout << type << endl;
    int rdata_c = 0;

    if (!strcmp(type, "A")){
        inet_pton(AF_INET, rdata, tmp);
        rdata_c += 4;

    } else if (!strcmp(type, "AAAA")){
        inet_pton(AF_INET6, rdata, tmp);
        rdata_c += 16;

    } else if (!strcmp(type, "NS")){
        char* rdata_tmp = strdup(rdata);
        int rdata_l = 0;
        //cout << strlen(domain_search.find("NS")->second.RDATA) << endl;
        char* r_tok = strtok(rdata_tmp, ".\n\r");
        while(r_tok != NULL){
            rdata_l = strlen(r_tok);
            tmp[rdata_c] = (uint8_t)rdata_l;
            rdata_c++;
            memcpy(&tmp[rdata_c], r_tok, rdata_l); 
            rdata_c += rdata_l;
            r_tok = strtok(NULL, ".\n\r");
        }
        tmp[rdata_c++] = '\0';

    } else if (!strcmp(type, "CNAME")){
        char* rdata_tmp = strdup(rdata);
        int rdata_l = 0;
        //cout << strlen(domain_search.find("NS")->second.RDATA) << endl;
        char* r_tok = strtok(rdata_tmp, ".\n\r");
        while(r_tok != NULL){
            rdata_l = strlen(r_tok);
            tmp[rdata_c] = (uint8_t)rdata_l;
            rdata_c++;
            memcpy(&tmp[rdata_c], r_tok, rdata_l); 
            rdata_c += rdata_l;
            r_tok = strtok(NULL, ".\n\r");
        }
        tmp[rdata_c++] = '\0';

    } else if (!strcmp(type, "SOA")){
        char *saveptr1 = NULL, *saveptr2 = NULL;

        int rdata_l = 0;
        char* rdata_tmp = strdup(rdata);
        char* r_tok = strtok_r(rdata_tmp, " \n\r", &saveptr1);

        char* sub_tok;
        for (int j = 0; j < 2; j++){
            char* sub_str = r_tok;

            sub_tok = strtok_r(sub_str, ".\n\r", &saveptr2);
            //cout << sub_tok << endl;
            while(sub_tok != NULL){
                rdata_l = strlen(sub_tok);
                tmp[rdata_c] = (uint8_t)rdata_l;
                rdata_c++;
                memcpy(&tmp[rdata_c], sub_tok, rdata_l); 
                rdata_c += rdata_l;
                sub_tok = strtok_r(NULL, ".\n\r", &saveptr2);
            }
            tmp[rdata_c++] = '\0';
            r_tok = strtok_r(NULL, " \n\r", &saveptr1);
        }

        for (int j = 0; j < 5; j++){
            uint32_t uin32 = be32toh((uint32_t)atoll(r_tok));

            memcpy(&tmp[rdata_c], &uin32, 4);
            rdata_c += 4;
            r_tok = strtok_r(NULL, " \n\r", &saveptr1);
        }

    } else if (!strcmp(type, "MX")){
        char* rdata_tmp = strdup(rdata);
        char* r_tok = strtok(rdata_tmp, " .\n\r");

        uint16_t uin16 = atol(r_tok);
        uin16 = be16toh(uin16);
        memcpy(tmp, &uin16, 2);
        rdata_c += 2;

        r_tok = strtok(NULL, ".\n\r");
        int rdata_l = 0;
        while(r_tok != NULL){
            rdata_l = strlen(r_tok);
            tmp[rdata_c] = (uint8_t)rdata_l;
            rdata_c++;
            memcpy(&tmp[rdata_c], r_tok, rdata_l); 
            rdata_c += rdata_l;
            r_tok = strtok(NULL, ".\n\r");
        }
        tmp[rdata_c++] = '\0';
        
    } else if (!strcmp(type, "TXT")){
        char* rdata_tmp = strdup(rdata);

        int rdata_l = 0;
        char* r_tok = strtok(rdata_tmp, "\"\n\r");
        while(r_tok != NULL){
            rdata_l = strlen(r_tok);
            tmp[rdata_c] = (uint8_t)rdata_l;
            rdata_c++;
            memcpy(&tmp[rdata_c], r_tok, rdata_l); 
            rdata_c += rdata_l;
            r_tok = strtok(NULL, "\"\n\r");
        }
        
    }
    return rdata_c;
}

int check_nip(char* str, char* ip, char* domain){
    char* nip_tmp = strdup(str);
    char* nip_tok = strtok(nip_tmp, " .\n\r");
    int len = 0;
    for (int i = 0; i < 4; i++){
        if (nip_tok == NULL) return 0;
        int in = atoi(nip_tok);
        if (in < 0 || in > 255) return 0;
        len += strlen(nip_tok)+1;
        nip_tok = strtok(NULL, " .\n\r");
    }

    memset(ip, 0, 20);
    memcpy(ip, str, len-1);

    if (nip_tok == NULL) return 0;
    memset(domain, 0, 50);
    memcpy(domain, &str[len], strlen(str) - len);
    return 1;
}

int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in sin, csin;
	socklen_t csinlen = sizeof(csin);

	string path = "";

	if(argc < 3) {
		return -fprintf(stderr, "usage: %s <port-number> <path/to/the/config/file>\n", argv[0]);
	}
	path = argv[2];

    read_config(path);


    setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(strtol(argv[1], NULL, 0));

	if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");

	if(bind(sockfd, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		err_quit("bind");

    char buf[1000];
    while(1) {
        int rlen;
        memset(&buf, 0, sizeof(buf));
		if((rlen = recvfrom(sockfd, buf, 1000, MSG_DONTWAIT, (struct sockaddr*) &csin, &csinlen)) < 0) {

        } else {
            cout << "total receive length: " << rlen << endl;
            // parse header
            header_t* header_d = new header_t;
            memcpy(header_d, buf, sizeof(header_t));

            header_d->ID = be16toh(header_d->ID);
            header_d->QDCOUNT = be16toh(header_d->QDCOUNT);
            header_d->ANCOUNT = be16toh(header_d->ANCOUNT);
            header_d->NSCOUNT = be16toh(header_d->NSCOUNT);
            header_d->ARCOUNT = be16toh(header_d->ARCOUNT);

            //cout << header_d->ID << " " << header_d->QDCOUNT << " " << header_d->ANCOUNT << " ";
            //cout << header_d->NSCOUNT << " " << header_d->ARCOUNT << endl;

            int cnt = 12;

            // parse question section
            vector<question*> ques(header_d->QDCOUNT);
            vector< vector<char*> > search(header_d->QDCOUNT);
            for (int i = 0; i < header_d->QDCOUNT; i++){
                int qname_l;
                ques[i] = new question;
                char* search_name = new char[50];
                memset(search_name, 0, 50);
                while(qname_l = buf[cnt]){
                    cnt++;
                    memcpy(&search_name[cnt-13], &buf[cnt], qname_l); 
                    cnt += qname_l;
                    search_name[cnt-13] = '.';
                }
                search_name[cnt - 12] = '\0';
                cout << search_name << endl;

                char* search_tmp = new char[50];
                ques[i]->ip = new char[20];
                ques[i]->nip = check_nip(search_name, ques[i]->ip, search_tmp);
                cout << "check nip: " << ques[i]->nip << endl;

                if (ques[i]->nip){
                    search[i].push_back(search_tmp);
                    cout << "ip: " << ques[i]->ip << ", search domain: " << search_tmp << endl;
                } else {
                    search[i].push_back(search_name);
                }
                cnt++;
                ques[i]->QNAME = new char[cnt - 13 + 1];
                memcpy(ques[i]->QNAME, &buf[12], cnt - 13 + 1);

                memcpy(&ques[i]->QTYPE, &buf[cnt], 2);
                cnt+=2;

                memcpy(&ques[i]->QCLASS, &buf[cnt], 2);
                cnt+=2;
            }

            // parse additional section
            char orig_add[rlen - cnt];
            memcpy(orig_add, &buf[cnt], rlen - cnt);

            // parse search domain
            for (int i = 0; i < header_d->QDCOUNT; i++){
                char* pos = strchr(search[i][0], '.');
                while (pos != NULL){
                    pos++;
                    if (*pos == '\0') break;
                    char* tmp = pos;
                    //cout << tmp << strlen(tmp) << endl;
                    search[i].push_back(tmp);
                    pos = strchr(pos, '.');
                }
            }
            
            vector< resource* > ans;
            vector<char*> domain(header_d->QDCOUNT);

            for (int i = 0; i < header_d->QDCOUNT; i++){
                ques[i]->answerd = 0;
                int j;
                for(j = 0; j < search[i].size(); j++){
                    if (domain_info.find(search[i][j]) != domain_info.end()){
                        read_zone(domain_info.find(search[i][j])->second);
                        domain[i] = search[i][j];
                        break;
                    }
                }
                if (j == search[i].size()) {
                    ques[i]->answerd = -1;
                    continue;
                }
                cout << "domain: " << domain[i] << endl;

                char* type = strdup(value_type[be16toh(ques[i]->QTYPE)].c_str());
                char* qclass = strdup(value_class[be16toh(ques[i]->QCLASS)].c_str());
                cout << "type: " << type << ", class: " << qclass << endl;

                if (ques[i]->nip){
                    resource* res = new resource;

                    res->NAME = strdup(ques[i]->QNAME);
                    res->TYPE = ques[i]->QTYPE;
                    res->CLASS = ques[i]->QCLASS;
                    res->TTL = be32toh(1);

                    char r_tmp[100];
                    int len = packed_rdata(type, ques[i]->ip, r_tmp);
                    res->RDATA = new char[len];
                    memcpy(res->RDATA, r_tmp, len);
                    res->RDLENGTH = be16toh((uint16_t)len);

                    ques[i]->answerd++;
                    ans.push_back(res);
                    header_d->ANCOUNT++;
                    //map<string, map<string, vector<domain_record>>>::iterator it;
                    //for (it = domain_search.begin(); it != domain_search.end(); it++){
                    //    map<string, vector<domain_record>>::iterator it2;
//
                    //    for (it2 = it->second.begin(); it2 != it->second.end(); it2++){
                    //        if (strcmp((it2->first).c_str(), type)) continue;
//
                    //        for (int it3 = 0; it3 < it2->second.size(); it3++){
                    //            domain_record* itr = &it2->second[it3];
                    //            if (!strcmp(itr->CLASS, qclass) && !strcmp(itr->RDATA, ques[i]->ip)){
                    //                resource* res = new resource;
//
                    //                res->record = itr;
//
                    //                char r_tmp[100];
                    //                int len = packed_rdata("NS", itr->NAME, r_tmp);
                    //                res->NAME = new char[len];
                    //                memcpy(res->NAME, r_tmp, len);
//
                    //                res->TYPE = ques[i]->QTYPE;
                    //                res->CLASS = ques[i]->QCLASS;
                    //                res->TTL = be32toh(1);
//
                    //                len = packed_rdata(type, itr->RDATA, r_tmp);
                    //                res->RDATA = new char[len];
                    //                memcpy(res->RDATA, r_tmp, len);
                    //                res->RDLENGTH = be16toh((uint16_t)len);
//
                    //                ques[i]->answerd++;
                    //                ans.push_back(res);
                    //                header_d->ANCOUNT++;
                    //            }
                    //        }
                    //    }
                    //}
                    continue;
                }
                
                if (domain_search.find(search[i][0]) != domain_search.end() && domain_search[search[i][0]].find(type) != domain_search[search[i][0]].end()){
                    domain_record* itr;
                    for (int j =  0; j <  domain_search[search[i][0]][type].size(); j++){
                        itr = &domain_search[search[i][0]][type][j];
                        if (strcmp(itr->CLASS, qclass)){
                            continue;
                        }
                        resource* res = new resource;

                        res->record = itr;
                        res->NAME = strdup(ques[i]->QNAME);
                        res->TYPE = ques[i]->QTYPE;
                        res->CLASS = ques[i]->QCLASS;
                        res->TTL = be32toh(itr->TTL);

                        char r_tmp[100];
                        int len = packed_rdata(type, itr->RDATA, r_tmp);
                        res->RDATA = new char[len];
                        memcpy(res->RDATA, r_tmp, len);
                        res->RDLENGTH = be16toh((uint16_t)len);

                        ques[i]->answerd++;
                        ans.push_back(res);
                        header_d->ANCOUNT++;
                    }
                }
            }
            for (int i = 0; i < header_d->QDCOUNT; i++){
                if (ques[i]->answerd != -1) {
                    continue;
                }
                char rec_buf[1000];
                int total_len = forward(rlen, buf, rec_buf);
                sendto(sockfd, rec_buf, total_len, 0, (struct sockaddr*) &csin, sizeof(csin));
                continue;
            }
            cout << "ans end!!!\n";

            vector<resource*> auth;
            for (int i = 0; i < header_d->QDCOUNT; i++){
                if (domain[i] == NULL) continue;

                char* type = strdup(value_type[be16toh(ques[i]->QTYPE)].c_str());

                if (!strcmp(type, "NS") && ques[i]->answerd) continue;

                char r_tmp[100];
                if (ques[i]->answerd && ((!strcmp(type, "MX") || !strcmp(type, "SOA") || !strcmp(type, "CNAME")) || (domain[i] != NULL && strcmp(domain[i], search[i][0])))){
                    cout << "check: " << domain[i] << " " << search[i][0] << endl;
                    type = strdup("NS");
                    if (domain_search.find(domain[i]) == domain_search.end() || domain_search[domain[i]].find("NS") == domain_search[domain[i]].end()){
                        continue;
                    }
                    if (domain_search[domain[i]]["NS"].empty()){
                        continue;
                    }

                    for (int j = 0; j < domain_search[domain[i]]["NS"].size(); j++){
                        resource* auth_res = new resource;
                        auth_res->record = &domain_search[domain[i]]["NS"][j];

                        char r_tmp[100];
                        int len = packed_rdata("NS", domain[i], r_tmp);
                        auth_res->NAME = new char[len];
                        memcpy(auth_res->NAME, r_tmp, len);
                        //cout << auth_res->NAME << endl;

                        auth_res->TYPE = be16toh(type_value["NS"]);
                        auth_res->CLASS = be16toh(class_value[domain_search[domain[i]]["NS"][j].CLASS]);
                        auth_res->TTL = be32toh(domain_search[domain[i]]["NS"][j].TTL);
                        len = packed_rdata(type, domain_search[domain[i]]["NS"][j].RDATA, r_tmp);
                        auth_res->RDATA = new char[len];

                        memcpy(auth_res->RDATA, r_tmp, len);
                        auth_res->RDLENGTH = be16toh((uint16_t)len);

                        auth.push_back(auth_res);
                        header_d->NSCOUNT++;
                    }
                } else {
                    if (!strcmp(type, "SOA")){
                        continue;
                    }
                    type = strdup("SOA");
                    if (domain_search.find(domain[i]) == domain_search.end() || domain_search[domain[i]].find("SOA") == domain_search[domain[i]].end()){
                        continue;
                    }

                    for (int j = 0; j < domain_search[domain[i]]["SOA"].size(); j++){
                        resource* auth_res = new resource;
                        auth_res->record = &domain_search[domain[i]]["SOA"][j];

                        char r_tmp[100];
                        int len = packed_rdata("NS", domain[i], r_tmp);
                        auth_res->NAME = new char[len];
                        memcpy(auth_res->NAME, r_tmp, len);

                        //cout << auth_res->NAME << endl;
                        auth_res->TYPE = be16toh(type_value["SOA"]);
                        auth_res->CLASS = be16toh(class_value[domain_search[domain[i]]["SOA"][j].CLASS]);
                        auth_res->TTL = be32toh(domain_search[domain[i]]["SOA"][j].TTL);
                        len = packed_rdata(type, domain_search[domain[i]]["SOA"][j].RDATA, r_tmp);
                        auth_res->RDATA = new char[len];

                        memcpy(auth_res->RDATA, r_tmp, len);
                        auth_res->RDLENGTH = be16toh((uint16_t)len);

                        auth.push_back(auth_res);
                        header_d->NSCOUNT++;
                    }
                }
            }
            cout << "auth end-------------------------------\n";

            // additional section
            vector<resource*> add;
            for (int i = 0; i < ans.size(); i++){
                //if (domain[i] == NULL) continue;

                char* type = strdup(value_type[be16toh(ans[i]->TYPE)].c_str());
                if (!(!strcmp(type, "NS") || !strcmp(type, "MX"))){
                    continue;
                }
                char* sear_add;
                if (!strcmp(type, "NS")){
                    sear_add = ans[i]->record->RDATA;
                } else if(!strcmp(type, "MX")){
                    char* t = strdup(ans[i]->record->RDATA);
                    sear_add = strtok(t, " \n\r");
                    sear_add = strtok(NULL, " \n\r");
                }
                //cout << sear_add << endl;

                if (domain_search.find(sear_add) == domain_search.end() || domain_search[sear_add].find("A") == domain_search[sear_add].end()){
                    continue;
                }

                for (int j = 0; j < domain_search[sear_add]["A"].size(); j++){
                    resource* add_res = new resource;
                    char r_tmp[100];

                    add_res->record = &domain_search[sear_add]["A"][j];

                    char* s_tmp;
                    if (!strcmp(type, "NS")){
                        s_tmp = ans[i]->RDATA;
                    } else if(!strcmp(type, "MX")){
                        s_tmp = &ans[i]->RDATA[2];
                    }
        
                    type = strdup("A");
                    add_res->NAME = strdup(s_tmp);
                    add_res->TYPE = be16toh(type_value["A"]);
                    add_res->CLASS = be16toh(class_value[domain_search[sear_add]["A"][j].CLASS]);
                    add_res->TTL = be32toh(domain_search[sear_add]["A"][j].TTL);
                    int len = packed_rdata(type, domain_search[sear_add]["A"][j].RDATA, r_tmp);
                    add_res->RDATA = new char[len];
                    memcpy(add_res->RDATA, r_tmp, len);
                    add_res->RDLENGTH = be16toh((uint16_t)len);
                    add.push_back(add_res);
                    header_d->ARCOUNT++;
                }
            }
            cout << "add end~~~~~~" << endl;

            //header_d->QDCOUNT = 0;
            //header_d->ARCOUNT = 0;
            //header_d->RCODE = 0;
            header_d->flags[0] |= 1UL << 7;
            header_d->flags[0] |= 1UL << 2;
            //header_d->flags[1] &= ~(1UL << 7);
            
            header_d->ID = be16toh(header_d->ID);
            header_d->QDCOUNT = be16toh(header_d->QDCOUNT);
            header_d->ANCOUNT = be16toh(header_d->ANCOUNT);
            header_d->NSCOUNT = be16toh(header_d->NSCOUNT);
            header_d->ARCOUNT = be16toh(header_d->ARCOUNT);

            cout << header_d->ID << " " << header_d->QDCOUNT << " " << header_d->ANCOUNT << " ";
            cout << header_d->NSCOUNT << " " << header_d->ARCOUNT << endl;

            memset(&buf, 0, sizeof(buf));
            memcpy(&buf, header_d, sizeof(header_t));

            cnt = sizeof(header_t);
            for (int i = 0; i < ques.size(); i++){
                int num = strlen(ques[i]->QNAME) + 1 + 4;
                cout << ques[i]->QNAME << " " << to_string(ques[i]->QTYPE) << " " << to_string(ques[i]->QCLASS) << endl; 
                memcpy(&buf[cnt], ques[i]->QNAME, strlen(ques[i]->QNAME) + 1);
                memcpy(&buf[cnt + strlen(ques[i]->QNAME) + 1], &ques[i]->QTYPE, 2);
                memcpy(&buf[cnt + strlen(ques[i]->QNAME) + 1 + 2], &ques[i]->QCLASS, 2);
                cnt += num;
            }

            for (int i = 0; i < ans.size(); i++){
                if (ans[i] == NULL) continue;

                int num = strlen(ans[i]->NAME) + 1 + 10 + be16toh(ans[i]->RDLENGTH);
                cout << num << endl;
                cout << ans[i]->NAME << " " << to_string(ans[i]->TYPE) << " " << ans[i]->RDATA << endl; 

                memcpy(&buf[cnt], ans[i]->NAME, strlen(ans[i]->NAME) + 1);

                memcpy(&buf[cnt + strlen(ans[i]->NAME) + 1], &ans[i]->TYPE, 10);

                memcpy(&buf[cnt + strlen(ans[i]->NAME) + 1 + 2 + 2 + 4 + 2], ans[i]->RDATA, be16toh(ans[i]->RDLENGTH));
                cnt += num;
            }

            for (int i = 0; i < auth.size(); i++){

                int num = strlen(auth[i]->NAME) + 1 + 10 + be16toh(auth[i]->RDLENGTH);
                cout << num << endl;
                cout << auth[i]->NAME << " " << auth[i]->TYPE << " " << auth[i]->RDATA << endl; 
                memcpy(&buf[cnt], auth[i]->NAME, strlen(auth[i]->NAME) + 1);

                memcpy(&buf[cnt + strlen(auth[i]->NAME) + 1], &auth[i]->TYPE, 10);

                memcpy(&buf[cnt + strlen(auth[i]->NAME) + 1 + 2 + 2 + 4 + 2], auth[i]->RDATA, be16toh(auth[i]->RDLENGTH));
                cnt += num;
            }

            cout << "size of additional sec: " << sizeof(orig_add) << endl;
            memcpy(&buf[cnt], orig_add, sizeof(orig_add));
            cnt += sizeof(orig_add);

            for (int i = 0; i < add.size(); i++){

                int num = strlen(add[i]->NAME) + 1 + 10 + be16toh(add[i]->RDLENGTH);
                cout << num << endl;
                cout << add[i]->NAME << " " << add[i]->TYPE << " " << add[i]->RDATA << endl; 
                memcpy(&buf[cnt], add[i]->NAME, strlen(add[i]->NAME) + 1);

                memcpy(&buf[cnt + strlen(add[i]->NAME) + 1], &add[i]->TYPE, 10);

                memcpy(&buf[cnt + strlen(add[i]->NAME) + 1 + 2 + 2 + 4 + 2], add[i]->RDATA, be16toh(add[i]->RDLENGTH));
                cnt += num;
            }
            cout << endl << endl;
            sendto(sockfd, buf, cnt, 0, (struct sockaddr*) &csin, sizeof(csin));
        }
    }
}