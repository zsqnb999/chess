#include <bits/stdc++.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

// ================== 网络工具 ==================
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
struct LineReader{
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
// 返回要显示的“中文棋子”（用UTF-8字面量）
static const char* 中文棋子(const Piece& pc){
	if(pc.c==NONE) return "·"; // 空格子
	// 你内部：WHITE 在 row0/1，BLACK 在 row7/6
	// 这里按颜色区分“红/黑”或“白/黑”字形：你可按喜好调整
	if(pc.c==WHITE){
		switch(pc.t){
			case 'K': return "帅";
			case 'Q': return "后";
			case 'R': return "车";
			case 'B': return "象";
			case 'N': return "马";
			case 'P': return "兵";
		}
	}else{ // BLACK
		switch(pc.t){
			case 'K': return "将";
			case 'Q': return "后";
			case 'R': return "车";
			case 'B': return "象";
			case 'N': return "马";
			case 'P': return "卒";
		}
	}
	return "?";
}
static WORD Bg(WORD bg){ return (bg<<4); }
static void SetAttr(WORD attr){ SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),attr); }
static WORD GetDefaultAttr(){
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
	return csbi.wAttributes;
}
static HANDLE HOut(){ return GetStdHandle(STD_OUTPUT_HANDLE); }
static HANDLE HIn(){ return GetStdHandle(STD_INPUT_HANDLE); }
static void GotoXY(short x, short y){
	COORD c{ x,y };
	SetConsoleCursorPosition(HOut(),c);
}
static void ClearScreenWin(){
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(HOut(), &csbi);
	DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
	DWORD written=0;
	COORD home{0,0};
	FillConsoleOutputCharacterA(HOut(), ' ', cellCount, home, &written);
	FillConsoleOutputAttribute(HOut(), csbi.wAttributes, cellCount, home, &written);
	SetConsoleCursorPosition(HOut(), home);
}
static void ClearLine(short y){
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(HOut(), &csbi);
	DWORD written=0;
	COORD pos{0,y};
	FillConsoleOutputCharacterA(HOut(), ' ', csbi.dwSize.X, pos, &written);
	SetConsoleCursorPosition(HOut(), pos);
}
static void EnableInputMode(){
	DWORD mode=0;
	GetConsoleMode(HIn(),&mode);
	mode |= ENABLE_EXTENDED_FLAGS;
	mode |= ENABLE_MOUSE_INPUT;
	mode |= ENABLE_WINDOW_INPUT;
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	mode &= ~ENABLE_INSERT_MODE;
	SetConsoleMode(HIn(),mode);
	FlushConsoleInputBuffer(HIn());
}
static char GetKeyChar(const KEY_EVENT_RECORD& ke){
	char ch=ke.uChar.AsciiChar;
	if(ch>='a' && ch<='z') ch=ch-'a'+'A';
	return ch;
}
static string Trim(string s){
	auto issp=[](unsigned char c){ return isspace(c)!=0; };
	while(!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
	while(!s.empty() && issp((unsigned char)s.back())) s.pop_back();
	return s;
}

// ================== 接收队列（大厅 + 对局都用它） ==================
struct NetQueue{
	mutex m;
	deque<string> q;
	HANDLE ev=nullptr; // 手动复位
};
static void NetPush(NetQueue& nq, const string& s){
	lock_guard<mutex> lk(nq.m);
	nq.q.push_back(s);
	SetEvent(nq.ev);
}
static bool NetPop(NetQueue& nq, string& s){
	lock_guard<mutex> lk(nq.m);
	if(nq.q.empty()) return false;
	s=nq.q.front();
	nq.q.pop_front();
	if(nq.q.empty()) ResetEvent(nq.ev);
	return true;
}
static void ReceiverThread(SOCKET sock, NetQueue* nq){
	LineReader lr;
	string line;
	while(lr.ReadLine(sock,line)){
		NetPush(*nq,line);
	}
	NetPush(*nq,"__断开连接__");
}

// 阻塞等一条网络消息（从队列取）
static bool WaitNetLine(NetQueue& nq, string& out){
	while(true){
		DWORD w=WaitForSingleObject(nq.ev, INFINITE);
		if(w!=WAIT_OBJECT_0) return false;
		if(NetPop(nq,out)) return true;
	}
}

// ================== 棋盘逻辑（你的原版 + 修正黑方后翼易位那段） ==================
enum Color{ NONE=0,WHITE=1,BLACK=2 };
struct Piece{ char t='.'; Color c=NONE; };
static Piece Make(Color c,char t){ return Piece{t,c}; }

struct Pos{ int r,c; };
static bool In(int r,int c){ return r>=0&&r<8&&c>=0&&c<8; }

struct CastlingRights{ bool wk=true,wq=true,bk=true,bq=true; };

struct Move{
	Pos from,to;
	char promo=0;
	bool isCastle=false;
	bool isEnPassant=false;
};
static bool SameMove(const Move&a,const Move&b){
	return a.from.r==b.from.r&&a.from.c==b.from.c&&
	a.to.r==b.to.r&&a.to.c==b.to.c&&
	a.promo==b.promo&&a.isCastle==b.isCastle&&a.isEnPassant==b.isEnPassant;
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
		Piece p=b.s[i][j];
		if(p.c!=by) continue;
		int dr=r-i,dc=c-j;
		
		if(p.t=='P'){
			int dir=(by==WHITE?+1:-1);
			if(dr==dir && (dc==1||dc==-1)) return true;
		}else if(p.t=='N'){
			int adr=abs(dr),adc=abs(dc);
			if((adr==2&&adc==1)||(adr==1&&adc==2)) return true;
		}else if(p.t=='K'){
			if(max(abs(dr),abs(dc))==1) return true;
		}else if(p.t=='B'||p.t=='Q'){
			if(abs(dr)==abs(dc) && dr!=0){
				int stepr=(dr>0?1:-1),stepc=(dc>0?1:-1);
				int rr=i+stepr,cc=j+stepc;
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
				int rr=i+stepr,cc=j+stepc;
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
	Pos k=FindKing(b,who);
	if(k.r==-1) return true;
	return IsSquareAttacked(b,k.r,k.c,(who==WHITE?BLACK:WHITE));
}

static Board ApplyMove(const Board& b,const Move& m){
	Board nb=b;
	Color us=b.sideToMove;
	Color them=(us==WHITE?BLACK:WHITE);
	
	Piece p=nb.s[m.from.r][m.from.c];
	Piece captured=nb.s[m.to.r][m.to.c];
	
	nb.epAvail=false;
	nb.epTarget={-1,-1};
	
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
	Color us=b.sideToMove;
	Color them=(us==WHITE?BLACK:WHITE);
	
	for(int r=0;r<8;r++) for(int c=0;c<8;c++){
		Piece p=b.s[r][c];
		if(p.c!=us) continue;
		
		if(p.t=='P'){
			int dir=(us==WHITE?+1:-1);
			int startRow=(us==WHITE?1:6);
			int promoRow=(us==WHITE?7:0);
			int r1=r+dir;
			
			if(In(r1,c) && b.s[r1][c].c==NONE){
				Move m{{r,c},{r1,c},0,false,false};
				if(r1==promoRow){
					for(char pr: {'Q','R','B','N'}){ Move pm=m; pm.promo=pr; out.push_back(pm); }
				}else out.push_back(m);
				
				int r2=r+2*dir;
				if(r==startRow && In(r2,c) && b.s[r2][c].c==NONE)
					out.push_back({{r,c},{r2,c},0,false,false});
			}
			
			for(int dc: {-1,+1}){
				int cc=c+dc;
				if(!In(r1,cc)) continue;
				
				if(b.s[r1][cc].c==them){
					Move m{{r,c},{r1,cc},0,false,false};
					if(r1==promoRow){
						for(char pr: {'Q','R','B','N'}){ Move pm=m; pm.promo=pr; out.push_back(pm); }
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
				int rr=r+drs[k],cc=c+dcs[k];
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
				int rr=r+dr,cc=c+dc;
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
				int rr=r+dr,cc=c+dc;
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
	vector<Move> pseudo; pseudo.reserve(128);
	GenPseudo(b,pseudo);
	vector<Move> legal; legal.reserve(pseudo.size());
	for(const auto& m: pseudo) AddIfLegal(b,legal,m);
	return legal;
}
static vector<Pos> MarkTargets(const vector<Move>& legal,Pos from){
	vector<Pos> m;
	for(auto &mv: legal) if(mv.from.r==from.r && mv.from.c==from.c) m.push_back(mv.to);
	return m;
}

// ================== 坐标映射（你的原版） ==================
static Pos ViewToBoard(Pos v, Color myColor){
	if(myColor==WHITE) return { 7 - v.r, v.c };
	else return v;
}
static Pos BoardToView(Pos bpos, Color myColor){
	if(myColor==WHITE) return { 7 - bpos.r, bpos.c };
	else return bpos;
}

// ================== 棋盘鼠标点格 ==================
static const short BOARD_LINES = 1 + 8 + 2;
static const short INFO_Y  = BOARD_LINES;
static const short INPUT_Y = BOARD_LINES+1;
static const short PROMO_Y = BOARD_LINES+2;

static bool ConsoleCharToViewCell(SHORT x,SHORT y,Pos& outView){
	const int originX=4;
	const int originY=1;
	if(y<originY || y>=originY+8) return false;
	if(x<originX || x>=originX+8*2) return false;
	int r=y-originY;
	int c=(x-originX)/2;
	if(!In(r,c)) return false;
	outView={r,c};
	return true;
}
static void PrintInfoLine(const string& s){
	ClearLine(INFO_Y);
	GotoXY(0, INFO_Y);
	cout<<s;
	ClearLine(INPUT_Y);
	ClearLine(PROMO_Y);
	ClearLine(PROMO_Y+1);
	GotoXY(0, INPUT_Y);
}

// ================== 最后一步高亮 + 将军红字 ==================
static void DrawAllEx(const Board& b, WORD defAttr, Color myColor,
					  bool hasSel, Pos selBoard, const vector<Pos>& marksBoard,
					  bool hasLast, Pos lastFrom, Pos lastTo){
	WORD light=Bg(7),dark=Bg(2);
	WORD wfg=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD bfg=FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD emptyFg=defAttr&0x0F;
	
	bool checkNow=InCheck(b,b.sideToMove);
	
	bool markV[8][8]{};
	for(auto &p: marksBoard){
		Pos v=BoardToView(p,myColor);
		if(In(v.r,v.c)) markV[v.r][v.c]=true;
	}
	Pos selV=BoardToView(selBoard,myColor);
	
	Pos lastFromV{-1,-1}, lastToV{-1,-1};
	if(hasLast){
		lastFromV=BoardToView(lastFrom,myColor);
		lastToV  =BoardToView(lastTo,myColor);
	}
	
	GotoXY(0,0);
	SetAttr(defAttr);
	cout<<"    1  2  3  4  5  6  7  8\n";
	for(int r=0;r<8;r++){
		SetAttr(defAttr);
		cout<<" "<<(r+1)<<"  ";
		for(int c=0;c<8;c++){
			bool isLight=((r+c)%2==0);
			WORD bg=isLight?light:dark;
			
			if(hasLast && ((lastFromV.r==r && lastFromV.c==c) || (lastToV.r==r && lastToV.c==c))){
				bg=Bg(1);
			}
			if(hasSel && selV.r==r && selV.c==c) bg=Bg(6);
			else if(markV[r][c]) bg=Bg(5);
			
			Pos brd=ViewToBoard({r,c},myColor);
			Piece pc=b.s[brd.r][brd.c];
			
			WORD fg=emptyFg;
			if(pc.c==WHITE) fg=wfg;
			else if(pc.c==BLACK) fg=bfg;
			SetAttr(bg|fg);
// 原来：cout<<pc.t<<' ';
			const char* z = 中文棋子(pc);
			cout << z;

			if(pc.c==NONE) cout << ' '; // 让空格子也占2列
		}
		SetAttr(defAttr);
		cout<<"\n";
	}
	
	if(checkNow){
		WORD red=FOREGROUND_RED|FOREGROUND_INTENSITY;
		SetAttr(red | (defAttr & 0xF0));
		cout<<"轮到："<<(b.sideToMove==WHITE?"白方":"黑方")<<"（被将军）";
		SetAttr(defAttr);
		cout<<"   你是："<<(myColor==WHITE?"白方":"黑方")<<"\n";
	}else{
		SetAttr(defAttr);
		cout<<"轮到："<<(b.sideToMove==WHITE?"白方":"黑方")
		<<"   你是："<<(myColor==WHITE?"白方":"黑方")<<"\n";
	}
	cout<<"左键：点起点再点终点；右键取消。\n";
	
	ClearLine(INFO_Y);
	ClearLine(INPUT_Y);
	ClearLine(PROMO_Y);
	ClearLine(PROMO_Y+1);
}

// ================== flags & 协议 MOVE（你的原版） ==================
static void FixFlagsByBoard(const Board& b, Move& mv){
	mv.isCastle=false;
	mv.isEnPassant=false;
	
	Piece p=b.s[mv.from.r][mv.from.c];
	
	if(p.t=='K'){
		if(b.sideToMove==WHITE && mv.from.r==0 && mv.from.c==4 && mv.to.r==0 && (mv.to.c==6||mv.to.c==2)) mv.isCastle=true;
		if(b.sideToMove==BLACK && mv.from.r==7 && mv.from.c==4 && mv.to.r==7 && (mv.to.c==6||mv.to.c==2)) mv.isCastle=true;
	}
	if(p.t=='P' && b.epAvail &&
	   mv.to.r==b.epTarget.r && mv.to.c==b.epTarget.c &&
	   abs(mv.to.c-mv.from.c)==1 && b.s[mv.to.r][mv.to.c].c==NONE){
		mv.isEnPassant=true;
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
	int r1,c1,r2,c2;
	if(!(iss>>r1>>c1>>r2>>c2)) return false;
	if(r1<1||r1>8||c1<1||c1>8||r2<1||r2>8||c2<1||c2>8) return false;
	m.from={r1-1,c1-1};
	m.to  ={r2-1,c2-1};
	m.promo=0; m.isCastle=false; m.isEnPassant=false;
	char pr=0;
	if(iss>>pr){
		pr=toupper((unsigned char)pr);
		if(pr=='Q'||pr=='R'||pr=='B'||pr=='N') m.promo=pr;
	}
	return true;
}

// ================== 鼠标升变（你的原版） ==================
static bool InRange(SHORT x, SHORT l, SHORT r){ return x>=l && x<=r; }
static char ChoosePromotionByMouse(WORD defAttr){
	ClearLine(PROMO_Y);
	GotoXY(0, PROMO_Y);
	SetAttr(defAttr);
	cout<<"升变： [Q]  [R]  [B]  [N]   (右键取消)";
	ClearLine(PROMO_Y+1);
	GotoXY(0, PROMO_Y+1);
	cout<<"PROMOTE: [Q]  [R]  [B]  [N]";
	
	const SHORT baseY=PROMO_Y+1;
	const SHORT qL=9,qR=11;
	const SHORT rL=14,rR=16;
	const SHORT bL=19,bR=21;
	const SHORT nL=24,nR=26;
	
	while(true){
		INPUT_RECORD rec; DWORD nRead=0;
		if(!ReadConsoleInput(HIn(), &rec, 1, &nRead)) return 0;
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto &me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			ClearLine(PROMO_Y); ClearLine(PROMO_Y+1);
			return 0;
		}
		if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
		
		SHORT x=me.dwMousePosition.X;
		SHORT y=me.dwMousePosition.Y;
		if(y!=baseY) continue;
		
		if(InRange(x,qL,qR)){ ClearLine(PROMO_Y); ClearLine(PROMO_Y+1); return 'Q'; }
		if(InRange(x,rL,rR)){ ClearLine(PROMO_Y); ClearLine(PROMO_Y+1); return 'R'; }
		if(InRange(x,bL,bR)){ ClearLine(PROMO_Y); ClearLine(PROMO_Y+1); return 'B'; }
		if(InRange(x,nL,nR)){ ClearLine(PROMO_Y); ClearLine(PROMO_Y+1); return 'N'; }
	}
}

// ================== 大厅 ==================
struct RoomItem{ string name; int cnt=0; };
struct Rect{ short x1,y1,x2,y2; };
static bool Hit(const Rect& r, short x, short y){
	return y>=r.y1 && y<=r.y2 && x>=r.x1 && x<=r.x2;
}
static string RandomRoomName(){
	unsigned x=(unsigned)GetTickCount();
	x^=(unsigned)rand();
	ostringstream oss;
	oss<<"room_"<<hex<<uppercase<<setw(4)<<setfill('0')<<(x & 0xFFFF);
	return oss.str();
}
static bool RequestRoomList(SOCKET sock, NetQueue& nq, vector<RoomItem>& out, int timeoutMs){
	out.clear();
	if(!SendLine(sock,"LIST")) return false;
	
	DWORD start=GetTickCount();
	bool begun=false;
	
	while(true){
		DWORD now=GetTickCount();
		DWORD left=(now-start >= (DWORD)timeoutMs)?0:(timeoutMs-(now-start));
		DWORD w=WaitForSingleObject(nq.ev,left);
		if(w==WAIT_TIMEOUT) return false;
		
		string line;
		while(NetPop(nq,line)){
			if(line=="__断开连接__") return false;
			if(line=="ROOM_BEGIN"){ begun=true; continue; }
			if(line=="ROOM_END"){ return begun; }
			if(line.rfind("ROOM ",0)==0){
				istringstream iss(line);
				string tag,name; int cnt;
				iss>>tag>>name>>cnt;
				if(!name.empty()) out.push_back({name,cnt});
			}
		}
	}
}
static void 画大厅(const vector<RoomItem>& rooms, WORD defAttr,
				Rect& btnRefresh, Rect& btnCreate, Rect& btnJoin, Rect& btnQuit,
				vector<Rect>& roomRects){
	ClearScreenWin();
	SetAttr(defAttr);
	cout<<"====== 大厅（键盘：C创建 R刷新 J加入 ESC退出；鼠标点房间加入）======\n";
	
	short y=1,x=0;
	GotoXY(x,y); cout<<"[刷新R]"; btnRefresh={x,y,(short)(x+5),y}; x+=8;
	GotoXY(x,y); cout<<"[创建C]"; btnCreate ={x,y,(short)(x+5),y}; x+=8;
	GotoXY(x,y); cout<<"[加入J]"; btnJoin   ={x,y,(short)(x+5),y}; x+=8;
	GotoXY(x,y); cout<<"[退出ESC]"; btnQuit  ={x,y,(short)(x+7),y};
	
	GotoXY(0,3);
	cout<<"房间列表（房间名  人数）:\n";
	
	roomRects.clear();
	int maxShow=16;
	for(int i=0;i<(int)rooms.size() && i<maxShow;i++){
		short yy=(short)(5+i);
		GotoXY(0,yy);
		ostringstream oss;
		oss<<left<<setw(18)<<rooms[i].name<<"  "<<rooms[i].cnt<<"/2";
		string line=oss.str();
		cout<<line;
		roomRects.push_back({0,yy,(short)max<int>(0,(int)line.size()-1),yy});
	}
	if(rooms.empty()){
		GotoXY(0,5);
		cout<<"（暂无房间，按 C 创建）";
	}
	
	ClearLine(INFO_Y);
	ClearLine(INPUT_Y);
	ClearLine(PROMO_Y);
	ClearLine(PROMO_Y+1);
	GotoXY(0,INFO_Y);
	cout<<"提示：cmd下 C 一定可用（用AsciiChar判断）";
}

static string 输入房间名(WORD defAttr){
	string s;
	ClearLine(INPUT_Y);
	GotoXY(0,INPUT_Y);
	SetAttr(defAttr);
	cout<<"请输入房间名(字母数字_-)，回车确认，ESC取消：";
	
	while(true){
		INPUT_RECORD rec; DWORD nRead=0;
		ReadConsoleInput(HIn(), &rec, 1, &nRead);
		if(rec.EventType!=KEY_EVENT) continue;
		auto &ke=rec.Event.KeyEvent;
		if(!ke.bKeyDown) continue;
		
		if(ke.wVirtualKeyCode==VK_ESCAPE) return "";
		if(ke.wVirtualKeyCode==VK_RETURN) return s;
		
		if(ke.wVirtualKeyCode==VK_BACK){
			if(!s.empty()) s.pop_back();
		}else{
			char ch=ke.uChar.AsciiChar;
			if(ch==0) continue;
			if(isalnum((unsigned char)ch) || ch=='_' || ch=='-'){
				if(s.size()<32) s.push_back(ch);
			}
		}
		
		ClearLine(INPUT_Y);
		GotoXY(0,INPUT_Y);
		cout<<"请输入房间名(字母数字_-)，回车确认，ESC取消："<<s;
	}
}

static bool 等待开始并拿颜色(SOCKET sock, NetQueue& nq, Color& myColor){
	myColor=NONE;
	while(true){
		HANDLE hs[2]={HIn(), nq.ev};
		DWORD w=WaitForMultipleObjects(2, hs, FALSE, INFINITE);
		
		if(w==WAIT_OBJECT_0){
			INPUT_RECORD rec; DWORD nRead=0;
			ReadConsoleInput(HIn(), &rec, 1, &nRead);
			if(rec.EventType==KEY_EVENT && rec.Event.KeyEvent.bKeyDown &&
			   rec.Event.KeyEvent.wVirtualKeyCode==VK_ESCAPE){
				SendLine(sock,"LEAVE");
				return false;
			}
			continue;
		}
		
		string line;
		while(NetPop(nq,line)){
			if(line=="__断开连接__") return false;
			if(line=="COLOR WHITE") myColor=WHITE;
			else if(line=="COLOR BLACK") myColor=BLACK;
			else if(line=="START"){
				if(myColor==NONE) myColor=WHITE;
				return true;
			}else if(line.rfind("ERROR",0)==0){
				return false;
			}
		}
	}
}

// ================== 对局：关键修复点（对手回合从队列取MOVE） ==================
static void PlayGame(SOCKET sock, NetQueue& nq, WORD defAttr, Color myColor){
	Board b; Init(b);
	b.sideToMove=WHITE;
	
	bool hasSel=false;
	Pos selBoard{0,0};
	
	bool hasLast=false;
	Pos lastFrom{0,0}, lastTo{0,0};
	
	ClearScreenWin();
	DrawAllEx(b,defAttr,myColor,hasSel,selBoard,{},hasLast,lastFrom,lastTo);
	PrintInfoLine(string("对局开始，") + (myColor==WHITE?"你是白方(视角在下)":"你是黑方(视角在下)") + "。");
	
	while(true){
		auto legal=GenLegal(b);
		if(legal.empty()){
			PrintInfoLine(InCheck(b,b.sideToMove) ? "游戏结束：将死" : "游戏结束：逼和");
			break;
		}
		
		bool myTurn=(b.sideToMove==myColor);
		
		if(!myTurn){
			hasSel=false;
			DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
			PrintInfoLine("等待对手走子中...");
			
			string line;
			if(!WaitNetLine(nq,line)){
				PrintInfoLine("网络等待失败。");
				break;
			}
			if(line=="__断开连接__"){ PrintInfoLine("连接断开。"); break; }
			if(line=="OPPONENT_LEFT"){ PrintInfoLine("对手离开，对局结束。"); break; }
			if(line.rfind("MOVE ",0)!=0) continue;
			
			Move mv;
			if(!ParseMoveLine(line,mv)){
				PrintInfoLine("收到对手数据格式错误。");
				break;
			}
			FixFlagsByBoard(b,mv);
			
			bool ok=false;
			for(const auto& lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
			if(!ok){
				PrintInfoLine("对手走法非法（双方局面不同步）。");
				break;
			}
			
			lastFrom=mv.from; lastTo=mv.to; hasLast=true;
			b=ApplyMove(b,mv);
			
			DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
			PrintInfoLine("对手已走子：轮到你。请点击起点。");
			continue;
		}
		
		// 我方回合：读鼠标
		INPUT_RECORD rec; DWORD nRead=0;
		ReadConsoleInput(HIn(), &rec, 1, &nRead);
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto &me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			hasSel=false;
			DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
			PrintInfoLine("已取消选择。请重新点击起点。");
			continue;
		}
		if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
		
		Pos viewCell;
		if(!ConsoleCharToViewCell(me.dwMousePosition.X, me.dwMousePosition.Y, viewCell)){
			PrintInfoLine("你点的位置不在棋盘上。");
			continue;
		}
		Pos cellBoard=ViewToBoard(viewCell,myColor);
		
		if(!hasSel){
			if(b.s[cellBoard.r][cellBoard.c].c!=b.sideToMove){
				PrintInfoLine("请选择己方棋子作为起点。");
				continue;
			}
			hasSel=true;
			selBoard=cellBoard;
			auto marks=MarkTargets(legal,selBoard);
			DrawAllEx(b,defAttr,myColor,true,selBoard,marks,hasLast,lastFrom,lastTo);
			
			ostringstream oss;
			oss<<"已选中起点：("<<(selBoard.r+1)<<","<<(selBoard.c+1)<<")  请点击终点。";
			PrintInfoLine(oss.str());
			continue;
		}
		
		Move mv;
		mv.from=selBoard;
		mv.to=cellBoard;
		mv.promo=0;
		mv.isCastle=false;
		mv.isEnPassant=false;
		
		FixFlagsByBoard(b,mv);
		
		Piece p=b.s[mv.from.r][mv.from.c];
		if(p.t=='P'){
			int promoRow=(b.sideToMove==WHITE?7:0);
			if(mv.to.r==promoRow){
				PrintInfoLine("需要升变：请点击下方 Q/R/B/N。");
				char pr=ChoosePromotionByMouse(defAttr);
				if(pr==0){
					hasSel=false;
					DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
					PrintInfoLine("已取消升变选择。请重新选择起点。");
					continue;
				}
				mv.promo=pr;
			}
		}
		
		bool ok=false;
		for(const auto& lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
		if(!ok){
			hasSel=false;
			DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
			PrintInfoLine("非法走法：请重新选择起点。");
			continue;
		}
		
		lastFrom=mv.from; lastTo=mv.to; hasLast=true;
		b=ApplyMove(b,mv);
		hasSel=false;
		
		if(!SendLine(sock, EncodeMoveLine(mv))){
			DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
			PrintInfoLine("发送失败：连接断开。");
			break;
		}
		
		DrawAllEx(b,defAttr,myColor,false,selBoard,{},hasLast,lastFrom,lastTo);
		PrintInfoLine("走子成功，已发送给对手。等待对手...");
	}
	
	SendLine(sock,"LEAVE");
}

// ================== 连接 ==================
static bool ConnectToHost(const string& host, int port, SOCKET& outSock){
	outSock=INVALID_SOCKET;
	addrinfo hints{};
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=IPPROTO_TCP;
	
	addrinfo* res=nullptr;
	string portStr=to_string(port);
	if(getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res)!=0) return false;
	
	for(addrinfo* p=res;p;p=p->ai_next){
		SOCKET s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
		if(s==INVALID_SOCKET) continue;
		
		int flag=1;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
		
		if(connect(s,p->ai_addr,(int)p->ai_addrlen)==0){
			outSock=s;
			freeaddrinfo(res);
			return true;
		}
		closesocket(s);
	}
	freeaddrinfo(res);
	return false;
}

// ================== 主程序 ==================
int main(int argc,char**argv){
	srand((unsigned)time(nullptr));
	string host="127.0.0.1";
	int port=5555;
	if(argc>=2) host=argv[1];
	if(argc>=3) port=atoi(argv[2]);
	
	EnableInputMode();
	WORD defAttr=GetDefaultAttr();
	
	WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	
	SOCKET sock;
	if(!ConnectToHost(host,port,sock)){
		cout<<"连接失败："<<host<<":"<<port<<"\n";
		WSACleanup();
		return 1;
	}
	
	NetQueue nq;
	nq.ev=CreateEvent(nullptr, TRUE, FALSE, nullptr);
	thread(ReceiverThread, sock, &nq).detach();
	
	vector<RoomItem> rooms;
	Rect btnRefresh{},btnCreate{},btnJoin{},btnQuit{};
	vector<Rect> roomRects;
	
	while(true){
		RequestRoomList(sock,nq,rooms,1500);
		画大厅(rooms,defAttr,btnRefresh,btnCreate,btnJoin,btnQuit,roomRects);
		
		string wantJoin;
		
		while(wantJoin.empty()){
			HANDLE hs[2]={HIn(), nq.ev};
			DWORD w=WaitForMultipleObjects(2, hs, FALSE, INFINITE);
			
			if(w==WAIT_OBJECT_0+1){
				string s;
				while(NetPop(nq,s)){
					if(s=="__断开连接__") goto end_all;
				}
				continue;
			}
			
			INPUT_RECORD rec; DWORD nRead=0;
			ReadConsoleInput(HIn(), &rec, 1, &nRead);
			
			if(rec.EventType==KEY_EVENT){
				auto &ke=rec.Event.KeyEvent;
				if(!ke.bKeyDown) continue;
				
				if(ke.wVirtualKeyCode==VK_ESCAPE) goto end_all;
				
				char ch=GetKeyChar(ke);
				if(ch=='R'){
					RequestRoomList(sock,nq,rooms,1500);
					画大厅(rooms,defAttr,btnRefresh,btnCreate,btnJoin,btnQuit,roomRects);
				}else if(ch=='C'){
					wantJoin=RandomRoomName();
				}else if(ch=='J'){
					string typed=Trim(输入房间名(defAttr));
					画大厅(rooms,defAttr,btnRefresh,btnCreate,btnJoin,btnQuit,roomRects);
					if(!typed.empty()) wantJoin=typed;
				}
				continue;
			}
			
			if(rec.EventType==MOUSE_EVENT){
				auto &me=rec.Event.MouseEvent;
				if(me.dwEventFlags!=0 && me.dwEventFlags!=DOUBLE_CLICK) continue;
				if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
				
				short mx=me.dwMousePosition.X, my=me.dwMousePosition.Y;
				
				if(Hit(btnQuit,mx,my)) goto end_all;
				if(Hit(btnRefresh,mx,my)){
					RequestRoomList(sock,nq,rooms,1500);
					画大厅(rooms,defAttr,btnRefresh,btnCreate,btnJoin,btnQuit,roomRects);
					continue;
				}
				if(Hit(btnCreate,mx,my)){ wantJoin=RandomRoomName(); continue; }
				if(Hit(btnJoin,mx,my)){
					string typed=Trim(输入房间名(defAttr));
					画大厅(rooms,defAttr,btnRefresh,btnCreate,btnJoin,btnQuit,roomRects);
					if(!typed.empty()) wantJoin=typed;
					continue;
				}
				
				for(size_t i=0;i<roomRects.size() && i<rooms.size();i++){
					if(Hit(roomRects[i],mx,my)){ wantJoin=rooms[i].name; break; }
				}
			}
		}
		
		ClearLine(INFO_Y); GotoXY(0,INFO_Y);
		cout<<"正在加入房间："<<wantJoin;
		
		if(!SendLine(sock,"JOIN "+wantJoin)) goto end_all;
		
		Color myColor=NONE;
		if(!等待开始并拿颜色(sock,nq,myColor)){
			continue; // 取消或失败，回大厅
		}
		
		PlayGame(sock,nq,defAttr,myColor);
	}
	
	end_all:
	closesocket(sock);
	WSACleanup();
	return 0;
}
