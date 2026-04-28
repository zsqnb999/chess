#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

static bool SendLine(SOCKET s, const string& line){
	string msg = line + "\n";
	const char* p = msg.c_str();
	int left = (int)msg.size();
	while(left>0){
		int n = send(s, p, left, 0);
		if(n<=0) return false;
		p += n; left -= n;
	}
	return true;
}
static bool RecvLine(SOCKET s, string& out){
	out.clear();
	char ch;
	while(true){
		int n = recv(s, &ch, 1, 0);
		if(n<=0) return false;
		if(ch=='\n') break;
		if(ch!='\r') out.push_back(ch);
		if(out.size()>4096) return false;
	}
	return true;
}

int main(int argc,char**argv){
	int port=5555;
	if(argc>=2) port=atoi(argv[1]);
	
	WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	
	SOCKET ls=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(ls==INVALID_SOCKET){ cerr<<"socket failed\n"; return 1; }
	
	int opt=1;
	setsockopt(ls,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
	
	sockaddr_in addr{};
	addr.sin_family=AF_INET;
	addr.sin_port=htons((uint16_t)port);
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	
	if(bind(ls,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){ cerr<<"bind failed\n"; return 1; }
	if(listen(ls,2)==SOCKET_ERROR){ cerr<<"listen failed\n"; return 1; }
	
	cout<<"Server listening on port "<<port<<" ...\n";
	cout<<"Waiting two clients...\n";
	
	sockaddr_in a1{},a2{}; int l1=sizeof(a1), l2=sizeof(a2);
	SOCKET c1=accept(ls,(sockaddr*)&a1,&l1);
	if(c1==INVALID_SOCKET){ cerr<<"accept1 failed\n"; return 1; }
	cout<<"Player1 connected.\n";
	
	SOCKET c2=accept(ls,(sockaddr*)&a2,&l2);
	if(c2==INVALID_SOCKET){ cerr<<"accept2 failed\n"; return 1; }
	cout<<"Player2 connected.\n";
	
	// 先连者白，后连者黑
	SendLine(c1,"COLOR WHITE");
	SendLine(c2,"COLOR BLACK");
	SendLine(c1,"START");
	SendLine(c2,"START");
	
	while(true){
		fd_set rfds; FD_ZERO(&rfds);
		FD_SET(c1,&rfds); FD_SET(c2,&rfds);
		SOCKET mx=max(c1,c2);
		int rc=select((int)mx+1,&rfds,nullptr,nullptr,nullptr);
		if(rc<=0) break;
		
		auto forwardOne=[&](SOCKET from, SOCKET to)->bool{
			if(!FD_ISSET(from,&rfds)) return true;
			string line;
			if(!RecvLine(from,line)) return false;
			if(line.rfind("MOVE ",0)==0 || line.rfind("CHAT ",0)==0){
				if(!SendLine(to,line)) return false;
			}
			return true;
		};
		
		if(!forwardOne(c1,c2)) break;
		if(!forwardOne(c2,c1)) break;
	}
	
	closesocket(c1); closesocket(c2); closesocket(ls);
	WSACleanup();
	return 0;
}
