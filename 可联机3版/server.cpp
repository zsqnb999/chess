#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

static string Trim(string s){
	auto issp=[](unsigned char c){ return isspace(c)!=0; };
	while(!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
	while(!s.empty() && issp((unsigned char)s.back())) s.pop_back();
	return s;
}

static bool SendAll(SOCKET s, const char* data, int len){
	int left=len;
	while(left>0){
		int n=send(s, data+(len-left), left, 0);
		if(n<=0) return false;
		left-=n;
	}
	return true;
}
static bool SendLine(SOCKET s, const string& line){
	string msg=line+"\n";
	return SendAll(s, msg.c_str(), (int)msg.size());
}

struct LineReader {
	string buf;
	bool ReadLine(SOCKET s, string& out){
		out.clear();
		while(true){
			auto pos=buf.find('\n');
			if(pos!=string::npos){
				string line=buf.substr(0,pos);
				buf.erase(0,pos+1);
				if(!line.empty() && line.back()=='\r') line.pop_back();
				out=line;
				return true;
			}
			char tmp[4096];
			int n=recv(s,tmp,(int)sizeof(tmp),0);
			if(n<=0) return false;
			buf.append(tmp,tmp+n);
			if(buf.size()>(1u<<20)) return false;
		}
	}
};

struct ClientState{
	SOCKET sock=INVALID_SOCKET;
	string room;
	bool inRoom=false;
	bool started=false;
	int side=0; // 1白 2黑
	mutex sendMtx;
};

struct Room{
	shared_ptr<ClientState> white;
	shared_ptr<ClientState> black;
};

static mutex g_mtx;
static unordered_map<string, Room> g_rooms;

static bool ValidRoomName(const string& room){
	if(room.empty() || room.size()>32) return false;
	for(char ch: room){
		unsigned char c=(unsigned char)ch;
		if(!(isalnum(c) || ch=='_' || ch=='-')) return false;
	}
	return true;
}

static bool SafeSend(const shared_ptr<ClientState>& st, const string& line){
	if(!st || st->sock==INVALID_SOCKET) return false;
	lock_guard<mutex> lk(st->sendMtx);
	return SendLine(st->sock, line);
}

static shared_ptr<ClientState> GetPartnerLocked(const shared_ptr<ClientState>& me){
	if(!me->inRoom) return nullptr;
	auto it=g_rooms.find(me->room);
	if(it==g_rooms.end()) return nullptr;
	Room &rm=it->second;
	if(rm.white && rm.white.get()==me.get()) return rm.black;
	if(rm.black && rm.black.get()==me.get()) return rm.white;
	return nullptr;
}

static void RemoveFromRoomLocked(const shared_ptr<ClientState>& me){
	if(!me->inRoom) return;
	auto it=g_rooms.find(me->room);
	if(it!=g_rooms.end()){
		Room &rm=it->second;
		if(rm.white && rm.white.get()==me.get()) rm.white.reset();
		if(rm.black && rm.black.get()==me.get()) rm.black.reset();
		if(!rm.white && !rm.black) g_rooms.erase(it);
	}
	me->inRoom=false;
	me->started=false;
	me->room.clear();
	me->side=0;
}

static vector<pair<string,int>> SnapshotRooms(){
	vector<pair<string,int>> v;
	lock_guard<mutex> lk(g_mtx);
	for(auto &kv: g_rooms){
		int cnt=0;
		if(kv.second.white) cnt++;
		if(kv.second.black) cnt++;
		v.push_back({kv.first,cnt});
	}
	sort(v.begin(), v.end(), [](auto& a, auto& b){
		if(a.second!=b.second) return a.second>b.second;
		return a.first<b.first;
	});
	return v;
}

static void PrintRooms(){
	auto v=SnapshotRooms();
	system("cls");
	cout<<"[服务器] 房间数: "<<v.size()<<"\n";
	cout<<"------------------------------\n";
	for(auto &it: v){
		cout<<setw(20)<<left<<it.first<<"  "<<it.second<<"/2\n";
	}
	cout<<"------------------------------\n";
}

static void DoList(const shared_ptr<ClientState>& me){
	auto v=SnapshotRooms();
	SafeSend(me,"ROOM_BEGIN");
	for(auto &it: v){
		SafeSend(me,"ROOM "+it.first+" "+to_string(it.second));
	}
	SafeSend(me,"ROOM_END");
}

static void LeaveCurrent(const shared_ptr<ClientState>& me){
	shared_ptr<ClientState> other;
	bool hadGame=false;
	{
		lock_guard<mutex> lk(g_mtx);
		other=GetPartnerLocked(me);
		hadGame=me->started;
		if(other) RemoveFromRoomLocked(other);
		RemoveFromRoomLocked(me);
	}
	SafeSend(me,"LEFT");
	if(hadGame && other){
		SafeSend(other,"OPPONENT_LEFT");
		SafeSend(other,"INFO 对手离开，回到大厅");
	}
	PrintRooms();
}

static void JoinRoom(const shared_ptr<ClientState>& me, const string& room){
	if(!ValidRoomName(room)){
		SafeSend(me,"ERROR 房间名不合法（只能字母数字_-，长度<=32）");
		return;
	}
	
	shared_ptr<ClientState> partner;
	bool startNow=false;
	
	{
		lock_guard<mutex> lk(g_mtx);
		
		if(me->started){
			SafeSend(me,"ERROR 你正在对局中");
			return;
		}
		if(me->inRoom && !me->started) RemoveFromRoomLocked(me);
		
		Room &rm=g_rooms[room];
		if(!rm.white){
			rm.white=me;
			me->inRoom=true; me->started=false; me->room=room; me->side=1;
		}else if(!rm.black){
			rm.black=me;
			me->inRoom=true; me->started=true; me->room=room; me->side=2;
			partner=rm.white;
			if(partner) partner->started=true;
			startNow=true;
		}else{
			SafeSend(me,"ERROR 房间已满");
			return;
		}
	}
	
	SafeSend(me,"JOINED "+room);
	if(me->side==1){
		SafeSend(me,"COLOR WHITE");
		SafeSend(me,"WAITING");
	}else{
		SafeSend(me,"COLOR BLACK");
	}
	
	if(startNow && partner){
		SafeSend(partner,"START");
		SafeSend(me,"START");
	}
	
	PrintRooms();
}

static void ClientThread(shared_ptr<ClientState> me){
	LineReader lr;
	string line;
	SafeSend(me,"HELLO 已连接服务器");
	
	while(true){
		if(!lr.ReadLine(me->sock,line)){
			LeaveCurrent(me);
			break;
		}
		line=Trim(line);
		if(line.empty()) continue;
		
		if(line=="LIST"){ DoList(me); continue; }
		if(line=="LEAVE"){ LeaveCurrent(me); continue; }
		
		if(line.rfind("JOIN ",0)==0){
			JoinRoom(me, Trim(line.substr(5)));
			continue;
		}
		
		if(line.rfind("MOVE ",0)==0 || line.rfind("CHAT ",0)==0){
			shared_ptr<ClientState> other;
			bool started=false;
			{
				lock_guard<mutex> lk(g_mtx);
				started=me->started;
				other=GetPartnerLocked(me);
			}
			if(!started || !other){
				SafeSend(me,"ERROR 没有对手");
				continue;
			}
			SafeSend(other,line);
			continue;
		}
		
		if(line=="PING"){ SafeSend(me,"PONG"); continue; }
		
		SafeSend(me,"ERROR 未知命令");
	}
	
	closesocket(me->sock);
	me->sock=INVALID_SOCKET;
}

int main(int argc,char**argv){
	int port=5555;
	if(argc>=2) port=atoi(argv[1]);
	
	WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	
	SOCKET ls=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(ls==INVALID_SOCKET){ cerr<<"socket 失败\n"; return 1; }
	
	int opt=1;
	setsockopt(ls,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
	
	sockaddr_in addr{};
	addr.sin_family=AF_INET;
	addr.sin_port=htons((uint16_t)port);
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	
	if(bind(ls,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){ cerr<<"bind 失败\n"; return 1; }
	if(listen(ls,64)==SOCKET_ERROR){ cerr<<"listen 失败\n"; return 1; }
	
	PrintRooms();
	cout<<"[服务器] 监听端口 "<<port<<" ...\n";
	
	while(true){
		sockaddr_in ca{}; int clen=sizeof(ca);
		SOCKET c=accept(ls,(sockaddr*)&ca,&clen);
		if(c==INVALID_SOCKET) continue;
		
		int flag=1;
		setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
		
		auto st=make_shared<ClientState>();
		st->sock=c;
		thread(ClientThread, st).detach();
	}
	
	closesocket(ls);
	WSACleanup();
	return 0;
}
