#include <bits/stdc++.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;
static bool SendLine(SOCKET s, const string& line){
	string msg=line+"\n";
	const char* p=msg.c_str();
	int left=(int)msg.size();
	while(left>0){
		int n=send(s,p,left,0);
		if(n<=0) return false;
		p+=n; left-=n;
	}
	return true;
}
static bool RecvLine(SOCKET s, string& out){
	out.clear();
	char ch;
	while(true){
		int n=recv(s,&ch,1,0);
		if(n<=0) return false;
		if(ch=='\n') break;
		if(ch!='\r') out.push_back(ch);
		if(out.size()>4096) return false;
	}
	return true;
}
enum Color{ NONE=0,WHITE=1,BLACK=2 };
struct Piece{ char t='.'; Color c=NONE; };
static Piece Make(Color c,char t){ return Piece{t,c}; }
struct Pos{ int r,c; };
static bool In(int r,int c){ return r>=0&&r<8&&c>=0&&c<8; }
struct CastlingRights{ bool wk=true,wq=true,bk=true,bq=true; };
struct Move{ Pos from,to; char promo=0; bool isCastle=false; bool isEnPassant=false; };
static bool SameMove(const Move&a,const Move&b){
	return a.from.r==b.from.r&&a.from.c==b.from.c&&a.to.r==b.to.r&&a.to.c==b.to.c
	&&a.promo==b.promo&&a.isCastle==b.isCastle&&a.isEnPassant==b.isEnPassant;
}
struct Board{
	Piece s[8][8];
	Color sideToMove=WHITE;
	CastlingRights cr;
	bool epAvail=false;
	Pos epTarget{-1,-1};
};

static void Init(Board& b){
	for(int r=0;r<8;r++) for(int c=0;c<8;c++) b.s[r][c]=Make(NONE,'.');
	string back="RNBQKBNR";
	for(int c=0;c<8;c++){
		b.s[0][c]=Make(WHITE,back[c]);
		b.s[1][c]=Make(WHITE,'P');
		b.s[6][c]=Make(BLACK,'P');
		b.s[7][c]=Make(BLACK,back[c]);
	}
	b.sideToMove=WHITE;
	b.cr={true,true,true,true};
	b.epAvail=false;
	b.epTarget={-1,-1};
}
static Pos FindKing(const Board& b,Color c){
	for(int r=0;r<8;r++) for(int c2=0;c2<8;c2++)
		if(b.s[r][c2].c==c && b.s[r][c2].t=='K') return {r,c2};
	return {-1,-1};
}
static bool IsSquareAttacked(const Board& b,int r,int c,Color by){
	for(int i=0;i<8;i++) for(int j=0;j<8;j++){
		auto p=b.s[i][j];
		if(p.c!=by) continue;
		int dr=r-i, dc=c-j;
		
		if(p.t=='P'){
			int dir=(by==WHITE?+1:-1);
			if(dr==dir && (dc==1||dc==-1)) return true;
		}else if(p.t=='N'){
			int adr=abs(dr), adc=abs(dc);
			if((adr==2&&adc==1)||(adr==1&&adc==2)) return true;
		}else if(p.t=='K'){
			if(max(abs(dr),abs(dc))==1) return true;
		}else if(p.t=='B'||p.t=='Q'){
			if(abs(dr)==abs(dc) && dr!=0){
				int stepr=(dr>0?1:-1), stepc=(dc>0?1:-1);
				int rr=i+stepr, cc=j+stepc;
				bool ok=true;
				while(rr!=r && cc!=c){
					if(b.s[rr][cc].c!=NONE){ ok=false; break; }
					rr+=stepr; cc+=stepc;
				}
				if(ok) return true;
			}
		}
		if(p.t=='R'||p.t=='Q'){
			if((dr==0&&dc!=0)||(dc==0&&dr!=0)){
				int stepr=(dr==0?0:(dr>0?1:-1));
				int stepc=(dc==0?0:(dc>0?1:-1));
				int rr=i+stepr, cc=j+stepc;
				bool ok=true;
				while(rr!=r || cc!=c){
					if(b.s[rr][cc].c!=NONE){ ok=false; break; }
					rr+=stepr; cc+=stepc;
				}
				if(ok) return true;
			}
		}
	}
	return false;
}
static bool InCheck(const Board& b,Color who){
	auto k=FindKing(b,who);
	if(k.r==-1) return true;
	return IsSquareAttacked(b,k.r,k.c,(who==WHITE?BLACK:WHITE));
}

static Board ApplyMove(const Board& b,const Move& m){
	Board nb=b;
	Color us=b.sideToMove;
	Color them=(us==WHITE?BLACK:WHITE);
	auto p=nb.s[m.from.r][m.from.c];
	auto captured=nb.s[m.to.r][m.to.c];
	
	nb.epAvail=false; nb.epTarget={-1,-1};
	nb.s[m.from.r][m.from.c]=Make(NONE,'.');
	
	if(m.isCastle && p.t=='K'){
		nb.s[m.to.r][m.to.c]=p;
		if(us==WHITE){
			nb.cr.wk=nb.cr.wq=false;
			if(m.to.c==6){ nb.s[0][5]=nb.s[0][7]; nb.s[0][7]=Make(NONE,'.'); }
			else         { nb.s[0][3]=nb.s[0][0]; nb.s[0][0]=Make(NONE,'.'); }
		}else{
			nb.cr.bk=nb.cr.bq=false;
			if(m.to.c==6){ nb.s[7][5]=nb.s[7][7]; nb.s[7][7]=Make(NONE,'.'); }
			else         { nb.s[7][3]=nb.s[7][0]; nb.s[7][0]=Make(NONE,'.'); }
		}
	}else if(m.isEnPassant && p.t=='P'){
		nb.s[m.to.r][m.to.c]=p;
		int capR=m.to.r+(us==WHITE?-1:+1);
		nb.s[capR][m.to.c]=Make(NONE,'.');
	}else{
		nb.s[m.to.r][m.to.c]=p;
		if(m.promo && p.t=='P') nb.s[m.to.r][m.to.c].t=m.promo;
		
		if(p.t=='P' && abs(m.to.r-m.from.r)==2){
			nb.epAvail=true;
			nb.epTarget={(m.from.r+m.to.r)/2,m.from.c};
		}
		
		if(us==WHITE){
			if(p.t=='K') nb.cr.wk=nb.cr.wq=false;
			if(p.t=='R'){
				if(m.from.r==0 && m.from.c==0) nb.cr.wq=false;
				if(m.from.r==0 && m.from.c==7) nb.cr.wk=false;
			}
			if(captured.c==BLACK && captured.t=='R'){
				if(m.to.r==7 && m.to.c==0) nb.cr.bq=false;
				if(m.to.r==7 && m.to.c==7) nb.cr.bk=false;
			}
		}else{
			if(p.t=='K') nb.cr.bk=nb.cr.bq=false;
			if(p.t=='R'){
				if(m.from.r==7 && m.from.c==0) nb.cr.bq=false;
				if(m.from.r==7 && m.from.c==7) nb.cr.bk=false;
			}
			if(captured.c==WHITE && captured.t=='R'){
				if(m.to.r==0 && m.to.c==0) nb.cr.wq=false;
				if(m.to.r==0 && m.to.c==7) nb.cr.wk=false;
			}
		}
	}
	
	nb.sideToMove=them;
	return nb;
}

static void AddIfLegal(const Board& b,vector<Move>& out,const Move& m){
	Board nb=ApplyMove(b,m);
	if(!InCheck(nb,b.sideToMove)) out.push_back(m);
}
static void GenPseudo(const Board& b,vector<Move>& out){
	Color us=b.sideToMove, them=(us==WHITE?BLACK:WHITE);
	for(int r=0;r<8;r++) for(int c=0;c<8;c++){
		auto p=b.s[r][c];
		if(p.c!=us) continue;
		
		if(p.t=='P'){
			int dir=(us==WHITE?+1:-1);
			int startRow=(us==WHITE?1:6);
			int promoRow=(us==WHITE?7:0);
			int r1=r+dir;
			
			if(In(r1,c) && b.s[r1][c].c==NONE){
				Move m{{r,c},{r1,c},0,false,false};
				if(r1==promoRow){
					for(char pr: {'Q','R','B','N'}){ auto pm=m; pm.promo=pr; out.push_back(pm); }
				}else out.push_back(m);
				int r2=r+2*dir;
				if(r==startRow && In(r2,c) && b.s[r2][c].c==NONE)
					out.push_back({{r,c},{r2,c},0,false,false});
			}
			for(int dc: {-1,+1}){
				int cc=c+dc; if(!In(r1,cc)) continue;
				if(b.s[r1][cc].c==them){
					Move m{{r,c},{r1,cc},0,false,false};
					if(r1==promoRow){
						for(char pr: {'Q','R','B','N'}){ auto pm=m; pm.promo=pr; out.push_back(pm); }
					}else out.push_back(m);
				}
				if(b.epAvail && b.epTarget.r==r1 && b.epTarget.c==cc){
					int capR=r1+(us==WHITE?-1:+1);
					if(In(capR,cc) && b.s[capR][cc].c==them && b.s[capR][cc].t=='P')
						out.push_back({{r,c},{r1,cc},0,false,true});
				}
			}
		}else if(p.t=='N'){
			int drs[8]={-2,-2,-1,-1,1,1,2,2};
			int dcs[8]={-1,1,-2,2,-2,2,-1,1};
			for(int k=0;k<8;k++){
				int rr=r+drs[k], cc=c+dcs[k];
				if(!In(rr,cc)) continue;
				if(b.s[rr][cc].c!=us) out.push_back({{r,c},{rr,cc},0,false,false});
			}
		}else if(p.t=='B'||p.t=='R'||p.t=='Q'){
			vector<pair<int,int>> dirs;
			if(p.t=='B'||p.t=='Q'){
				dirs.push_back({1,1}); dirs.push_back({1,-1});
				dirs.push_back({-1,1}); dirs.push_back({-1,-1});
			}
			if(p.t=='R'||p.t=='Q'){
				dirs.push_back({1,0}); dirs.push_back({-1,0});
				dirs.push_back({0,1}); dirs.push_back({0,-1});
			}
			for(auto [dr,dc]: dirs){
				int rr=r+dr, cc=c+dc;
				while(In(rr,cc)){
					if(b.s[rr][cc].c==us) break;
					out.push_back({{r,c},{rr,cc},0,false,false});
					if(b.s[rr][cc].c==them) break;
					rr+=dr; cc+=dc;
				}
			}
		}else if(p.t=='K'){
			for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++){
				if(dr==0&&dc==0) continue;
				int rr=r+dr, cc=c+dc;
				if(!In(rr,cc)) continue;
				if(b.s[rr][cc].c!=us) out.push_back({{r,c},{rr,cc},0,false,false});
			}
			if(us==WHITE && r==0 && c==4){
				if(b.cr.wk && b.s[0][5].c==NONE && b.s[0][6].c==NONE){
					if(!IsSquareAttacked(b,0,4,them) && !IsSquareAttacked(b,0,5,them) && !IsSquareAttacked(b,0,6,them))
						out.push_back({{0,4},{0,6},0,true,false});
				}
				if(b.cr.wq && b.s[0][1].c==NONE && b.s[0][2].c==NONE && b.s[0][3].c==NONE){
					if(!IsSquareAttacked(b,0,4,them) && !IsSquareAttacked(b,0,3,them) && !IsSquareAttacked(b,0,2,them))
						out.push_back({{0,4},{0,2},0,true,false});
				}
			}
			if(us==BLACK && r==7 && c==4){
				if(b.cr.bk && b.s[7][5].c==NONE && b.s[7][6].c==NONE){
					if(!IsSquareAttacked(b,7,4,them) && !IsSquareAttacked(b,7,5,them) && !IsSquareAttacked(b,7,6,them))
						out.push_back({{7,4},{7,6},0,true,false});
				}
				if(b.cr.bq && b.s[7][1].c==NONE && b.s[7][2].c==NONE && b.s[7][3].c==NONE){
					if(!IsSquareAttacked(b,7,4,them) && !IsSquareAttacked(b,7,3,them) && !IsSquareAttacked(b,7,2,them))
						out.push_back({{7,4},{7,2},0,true,false});
				}
			}
		}
	}
}
static vector<Move> GenLegal(const Board& b){
	vector<Move> ps; ps.reserve(128);
	GenPseudo(b,ps);
	vector<Move> le; le.reserve(ps.size());
	for(auto &m: ps) AddIfLegal(b,le,m);
	return le;
}

static void FixFlagsByBoard(const Board& b, Move& m){
	auto p=b.s[m.from.r][m.from.c];
	m.isCastle=false; m.isEnPassant=false;
	if(p.c==b.sideToMove && p.t=='K'){
		if(b.sideToMove==WHITE && m.from.r==0 && m.from.c==4 && m.to.r==0 && (m.to.c==6||m.to.c==2)) m.isCastle=true;
		if(b.sideToMove==BLACK && m.from.r==7 && m.from.c==4 && m.to.r==7 && (m.to.c==6||m.to.c==2)) m.isCastle=true;
	}
	if(p.c==b.sideToMove && p.t=='P' && b.epAvail &&
	   m.to.r==b.epTarget.r && m.to.c==b.epTarget.c &&
	   abs(m.to.c-m.from.c)==1 && b.s[m.to.r][m.to.c].c==NONE){
		m.isEnPassant=true;
	}
}
static HANDLE HOut(){ return GetStdHandle(STD_OUTPUT_HANDLE); }
static HANDLE HIn(){ return GetStdHandle(STD_INPUT_HANDLE); }
static void EnableMouse(){
	DWORD mode=0;
	GetConsoleMode(HIn(), &mode);
	mode |= ENABLE_EXTENDED_FLAGS;
	mode |= ENABLE_MOUSE_INPUT;
	mode |= ENABLE_WINDOW_INPUT;
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(HIn(), mode);
}
static void ClearScreenWin(){
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(HOut(), &csbi);
	DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
	DWORD written=0;
	COORD home{0,0};
	FillConsoleOutputCharacterA(HOut(),' ',cellCount,home,&written);
	FillConsoleOutputAttribute(HOut(),csbi.wAttributes,cellCount,home,&written);
	SetConsoleCursorPosition(HOut(),home);
}
static void GotoXY(short x, short y){
	COORD c{ x,y };
	SetConsoleCursorPosition(HOut(), c);
}
static bool ConsoleToCell(SHORT x, SHORT y, Pos& out){
	const int originX=4; // " 1  " 后第一个格
	const int originY=1;
	if(y<originY || y>=originY+8) return false;
	if(x<originX || x>=originX+8*2) return false;
	int r=y-originY;
	int c=(x-originX)/2;
	if(!In(r,c)) return false;
	out={r,c};
	return true;
}

static void DrawBoard(const Board& b, bool hasSel, Pos sel, const string& info){
	ClearScreenWin();
	cout<<"    1 2 3 4 5 6 7 8\n";
	for(int r=0;r<8;r++){
		cout<<" "<<(r+1)<<"  ";
		for(int c=0;c<8;c++){
			char ch=b.s[r][c].t;
			if(b.s[r][c].c==BLACK) ch=tolower((unsigned char)ch);
			if(hasSel && sel.r==r && sel.c==c) cout<<'['<<ch<<']';
			else cout<<ch<<' ';
		}
		cout<<"\n";
	}
	cout<<"Turn: "<<(b.sideToMove==WHITE?"WHITE":"BLACK")<<"\n";
	cout<<"Mouse: 左键点起点再点终点；右键取消。升变需键盘输入 Q/R/B/N\n";
	cout<<info<<"\n";
}
static void DrawBoardFixed(const Board& b, bool hasSel, Pos sel, const string& info){
	ClearScreenWin();
	cout<<"    1 2 3 4 5 6 7 8\n";
	for(int r=0;r<8;r++){
		cout<<" "<<(r+1)<<"  ";
		for(int c=0;c<8;c++){
			char ch=b.s[r][c].t;
			if(b.s[r][c].c==BLACK) ch=tolower((unsigned char)ch);
			cout<<ch<<' ';
		}
		cout<<"\n";
	}
	cout<<"Turn: "<<(b.sideToMove==WHITE?"WHITE":"BLACK")<<"\n";
	cout<<"Mouse: 左键点起点再点终点；右键取消。升变需键盘输入 Q/R/B/N\n";
	if(hasSel){
		cout<<"Selected: ("<<(sel.r+1)<<","<<(sel.c+1)<<")  "<<info<<"\n";
	}else{
		cout<<info<<"\n";
	}
}
static bool GetMoveByMouse(const Board& b, Move& mv){
	bool hasSel=false;
	Pos sel{0,0};
	string info="请点击己方棋子作为起点。";
	DrawBoardFixed(b,hasSel,sel,info);
	while(true){
		INPUT_RECORD rec;
		DWORD nRead=0;
		if(!ReadConsoleInput(HIn(), &rec, 1, &nRead)) return false;
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto &me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			hasSel=false;
			info="已取消选择，请重新点起点。";
			DrawBoardFixed(b,hasSel,sel,info);
			continue;
		}
		
		if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
		
		Pos cell;
		if(!ConsoleToCell(me.dwMousePosition.X, me.dwMousePosition.Y, cell)){
			info="你点的不在棋盘上。";
			DrawBoardFixed(b,hasSel,sel,info);
			continue;
		}
		
		if(!hasSel){
			if(b.s[cell.r][cell.c].c!=b.sideToMove){
				info="必须点到己方棋子。";
				DrawBoardFixed(b,hasSel,sel,info);
				continue;
			}
			sel=cell;
			hasSel=true;
			info="已选起点，请点击终点。";
			DrawBoardFixed(b,hasSel,sel,info);
			continue;
		}else{
			mv.from=sel;
			mv.to=cell;
			mv.promo=0;
			mv.isCastle=false;
			mv.isEnPassant=false;
			FixFlagsByBoard(b,mv);
			return true;
		}
	}
}
static string EncodeMoveLine(const Move& m){
	ostringstream oss;
	oss<<"MOVE "<<(m.from.r+1)<<" "<<(m.from.c+1)<<" "<<(m.to.r+1)<<" "<<(m.to.c+1);
	if(m.promo) oss<<" "<<m.promo;
	return oss.str();
}
static bool ParseMoveLine(const string& line, Move& m){
	istringstream iss(line);
	string tag; iss>>tag;
	if(tag!="MOVE") return false;
	int x1,y1,x2,y2;
	if(!(iss>>x1>>y1>>x2>>y2)) return false;
	if(x1<1||x1>8||y1<1||y1>8||x2<1||x2>8||y2<1||y2>8) return false;
	m.from={x1-1,y1-1};
	m.to={x2-1,y2-1};
	m.promo=0; m.isCastle=false; m.isEnPassant=false;
	char pr=0;
	if(iss>>pr){
		pr=toupper((unsigned char)pr);
		if(pr=='Q'||pr=='R'||pr=='B'||pr=='N') m.promo=pr;
	}
	return true;
}

int main(int argc,char**argv){
	string host="127.0.0.1";
	int port=5555;
	if(argc>=2) host=argv[1];
	if(argc>=3) port=atoi(argv[2]);
	
	EnableMouse();
	
	WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock==INVALID_SOCKET){ cerr<<"socket failed\n"; return 1; }
	
	sockaddr_in addr{};
	addr.sin_family=AF_INET;
	addr.sin_port=htons((uint16_t)port);
	addr.sin_addr.s_addr=inet_addr(host.c_str());
	
	if(connect(sock,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
		cerr<<"connect failed\n"; return 1;
	}
	string line;
	Color myColor=NONE;
	while(true){
		if(!RecvLine(sock,line)){ cerr<<"server closed\n"; return 1; }
		if(line=="START") break;
		if(line=="COLOR WHITE") myColor=WHITE;
		if(line=="COLOR BLACK") myColor=BLACK;
	}
	if(myColor==NONE){ cerr<<"no color\n"; return 1; }
	
	Board b; Init(b);
	
	while(true){
		auto legal=GenLegal(b);
		if(legal.empty()){
			DrawBoardFixed(b,false,{0,0}, InCheck(b,b.sideToMove)?"游戏结束：将死/无合法走法":"游戏结束：逼和");
			break;
		}
		
		bool myTurn=(b.sideToMove==myColor);
		
		if(myTurn){
			DrawBoardFixed(b,false,{0,0},"轮到你：用鼠标点两下完成一步。");
			Move mv;
			if(!GetMoveByMouse(b,mv)) break;
			auto p=b.s[mv.from.r][mv.from.c];
			if(p.t=='P'){
				int promoRow=(b.sideToMove==WHITE?7:0);
				if(mv.to.r==promoRow){
					DrawBoardFixed(b,false,{0,0},"升变：在键盘输入 Q/R/B/N 后回车。");
					char pr=0;
					cin>>pr;
					pr=toupper((unsigned char)pr);
					if(pr=='Q'||pr=='R'||pr=='B'||pr=='N') mv.promo=pr;
				}
			}
			
			FixFlagsByBoard(b,mv);
			
			bool ok=false;
			for(auto &lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
			if(!ok){
				DrawBoardFixed(b,false,{0,0},"非法走法（不在合法列表）。请重新走。");
				Sleep(900);
				continue;
			}
			
			b=ApplyMove(b,mv);
			if(!SendLine(sock, EncodeMoveLine(mv))){
				DrawBoardFixed(b,false,{0,0},"发送失败，连接断开。");
				break;
			}
		}else{
			DrawBoardFixed(b,false,{0,0},"等待对手走子中...");
			if(!RecvLine(sock,line)){
				DrawBoardFixed(b,false,{0,0},"服务器断开。");
				break;
			}
			if(line.rfind("MOVE ",0)==0){
				Move mv;
				if(!ParseMoveLine(line,mv)){
					DrawBoardFixed(b,false,{0,0},"收到坏数据。");
					break;
				}
				FixFlagsByBoard(b,mv);
				
				bool ok=false;
				for(auto &lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
				if(!ok){
					DrawBoardFixed(b,false,{0,0},"对手走法非法（或你俩规则不同步）。");
					break;
				}
				b=ApplyMove(b,mv);
			}
		}
	}
	
	closesocket(sock);
	WSACleanup();
	return 0;
}
