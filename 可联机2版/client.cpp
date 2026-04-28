#include <bits/stdc++.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

// ================== 网络工具 ==================
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

// ================== 棋盘逻辑（保持你原来的） ==================
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

// ================== 控制台 UI 工具（保持你原来的） ==================
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
static void EnableMouseInput(){
	DWORD mode=0;
	GetConsoleMode(HIn(),&mode);
	mode |= ENABLE_EXTENDED_FLAGS;
	mode |= ENABLE_MOUSE_INPUT;
	mode |= ENABLE_WINDOW_INPUT;
	mode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(HIn(),mode);
}

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

// ================== 坐标映射：让“自己这方在下面” ==================
// 说明：内部棋盘 b.s 的坐标不变（白子初始在 row0/1，黑子在 row7/6）
// 只是在显示/鼠标点击时，根据 myColor 做上下翻转（白方客户端翻转，黑方不翻）
static Pos ViewToBoard(Pos v, Color myColor){
	if(myColor==WHITE){
		return { 7 - v.r, v.c }; // 白方显示在下：上下翻转
	}else{
		return v; // 黑方本来就在下
	}
}
static Pos BoardToView(Pos bpos, Color myColor){
	if(myColor==WHITE){
		return { 7 - bpos.r, bpos.c };
	}else{
		return bpos;
	}
}

// ================== 棋盘鼠标点格（返回“视图坐标”0..7） ==================
static const short BOARD_LINES = 1 + 8 + 2;
static const short INFO_Y = BOARD_LINES;
static const short INPUT_Y = BOARD_LINES+1;
static const short PROMO_Y = BOARD_LINES+2; // 额外一行用于升变按钮

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
	GotoXY(0, INPUT_Y);
}

// ================== 绘制棋盘（已加入翻转视角） ==================
static void DrawAll(const Board& b, WORD defAttr, Color myColor, bool hasSel, Pos selBoard, const vector<Pos>& marksBoard){
	WORD light=Bg(7),dark=Bg(2);
	WORD wfg=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD bfg=FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD emptyFg=defAttr&0x0F;
	
	bool markV[8][8]{}; // 标记转成视图坐标后再画
	for(auto &p: marksBoard){
		Pos v=BoardToView(p,myColor);
		if(In(v.r,v.c)) markV[v.r][v.c]=true;
	}
	Pos selV = BoardToView(selBoard,myColor);
	
	GotoXY(0,0);
	SetAttr(defAttr);
	cout<<"    1  2  3  4  5  6  7  8\n";
	for(int r=0;r<8;r++){
		SetAttr(defAttr);
		cout<<" "<<(r+1)<<"  ";
		for(int c=0;c<8;c++){
			bool isLight=((r+c)%2==0);
			WORD bg=isLight?light:dark;
			
			if(hasSel && selV.r==r && selV.c==c) bg=Bg(6);
			else if(markV[r][c]) bg=Bg(5);
			
			Pos brd = ViewToBoard({r,c}, myColor);
			Piece pc = b.s[brd.r][brd.c];
			
			WORD fg=emptyFg;
			if(pc.c==WHITE) fg=wfg;
			else if(pc.c==BLACK) fg=bfg;
			
			SetAttr(bg|fg);
			cout<<pc.t<<' ';
		}
		SetAttr(defAttr);
		cout<<"\n";
	}
	SetAttr(defAttr);
	cout<<"轮到："<<(b.sideToMove==WHITE?"白方":"黑方")
	<<"   你是："<<(myColor==WHITE?"白方":"黑方")<<"\n";
	cout<<"左键：点起点再点终点；右键取消。\n";
	ClearLine(INFO_Y);
	ClearLine(INPUT_Y);
	ClearLine(PROMO_Y);
}

// ================== move flags（保证两端一致） ==================
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

// ================== 协议 MOVE ==================
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

// ================== 鼠标升变：点击 Q/R/B/N ==================
static bool InRange(SHORT x, SHORT l, SHORT r){ return x>=l && x<=r; }

// 返回：'Q'/'R'/'B'/'N'；返回 0 表示取消
static char ChoosePromotionByMouse(WORD defAttr){
	// 在 PROMO_Y 行画按钮
	ClearLine(PROMO_Y);
	GotoXY(0, PROMO_Y);
	SetAttr(defAttr);
	// 固定格式，方便判定点击范围
	//        012345678901234567890123456789...
	// 文本： "升变： [Q]  [R]  [B]  [N]   (右键取消)"
	cout<<"升变： [Q]  [R]  [B]  [N]   (右键取消)";
	// 记录每个按钮大概的 X 范围（与上面字符串对应）
	// "升变："(6含中文? 注意：控制台按字符宽度可能不同，但一般中文占2格会影响X)
	// 为避免中文宽度差异，这里再输出一行纯英文按钮行更稳：
	ClearLine(PROMO_Y+1);
	GotoXY(0, PROMO_Y+1);
	cout<<"PROMOTE: [Q]  [R]  [B]  [N]";
	
	// 按这个英文行来判定点击区域
	// "PROMOTE: "(9) -> '[' 在 x=9
	const SHORT baseY = PROMO_Y+1;
	const SHORT qL=9,  qR=11; // [Q]
	const SHORT rL=14, rR=16; // [R]
	const SHORT bL=19, bR=21; // [B]
	const SHORT nL=24, nR=26; // [N]
	
	while(true){
		INPUT_RECORD rec;
		DWORD nRead=0;
		if(!ReadConsoleInput(HIn(), &rec, 1, &nRead)) return 0;
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto &me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			ClearLine(PROMO_Y);
			ClearLine(PROMO_Y+1);
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

// ================== 主程序（联机 + 鼠标） ==================
int main(int argc,char**argv){
	string host="127.0.0.1";
	int port=5555;
	if(argc>=2) host=argv[1];
	if(argc>=3) port=atoi(argv[2]);
	
	EnableMouseInput();
	WORD defAttr=GetDefaultAttr();
	
	WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sock==INVALID_SOCKET){
		cout<<"socket failed\n";
		return 1;
	}
	
	sockaddr_in addr{};
	addr.sin_family=AF_INET;
	addr.sin_port=htons((uint16_t)port);
	addr.sin_addr.s_addr=inet_addr(host.c_str());
	
	if(connect(sock,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
		cout<<"connect failed: "<<host<<":"<<port<<"\n";
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	
	// 等待服务器分配颜色
	Color myColor=NONE;
	string line;
	while(true){
		if(!RecvLine(sock,line)){
			cout<<"server closed\n";
			return 1;
		}
		if(line=="COLOR WHITE") myColor=WHITE;
		else if(line=="COLOR BLACK") myColor=BLACK;
		else if(line=="START") break;
	}
	if(myColor==NONE){
		cout<<"no color\n";
		return 1;
	}
	
	Board b; Init(b);
	b.sideToMove = WHITE; // 联机固定白先走，双方一致
	
	bool hasSel=false;
	Pos selBoard{0,0};
	
	ClearScreenWin();
	DrawAll(b,defAttr,myColor,hasSel,selBoard,{});
	PrintInfoLine(string("已连接，") + (myColor==WHITE?"你是白方(视角在下)":"你是黑方(视角在下)") + "。");
	
	while(true){
		auto legal=GenLegal(b);
		if(legal.empty()){
			PrintInfoLine(InCheck(b,b.sideToMove) ? "游戏结束：将死" : "游戏结束：逼和");
			break;
		}
		
		bool myTurn = (b.sideToMove==myColor);
		
		// ======= 对手回合：阻塞等待网络 =======
		if(!myTurn){
			hasSel=false;
			DrawAll(b,defAttr,myColor,false,selBoard,{});
			PrintInfoLine("等待对手走子中...");
			
			if(!RecvLine(sock,line)){
				PrintInfoLine("服务器断开。");
				break;
			}
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
			
			b=ApplyMove(b,mv);
			DrawAll(b,defAttr,myColor,false,selBoard,{});
			PrintInfoLine("对手已走子：轮到你。请点击起点。");
			continue;
		}
		
		// ======= 我方回合：读鼠标事件 =======
		INPUT_RECORD rec;
		DWORD nRead=0;
		ReadConsoleInput(HIn(), &rec, 1, &nRead);
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto& me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		
		// 右键取消选中
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			hasSel=false;
			DrawAll(b,defAttr,myColor,false,selBoard,{});
			PrintInfoLine("已取消选择。请重新点击起点。");
			continue;
		}
		
		if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
		
		Pos viewCell;
		if(!ConsoleCharToViewCell(me.dwMousePosition.X, me.dwMousePosition.Y, viewCell)){
			PrintInfoLine("你点的位置不在棋盘上。");
			continue;
		}
		Pos cellBoard = ViewToBoard(viewCell, myColor); // 关键：视角映射到真实棋盘
		
		if(!hasSel){
			if(b.s[cellBoard.r][cellBoard.c].c!=b.sideToMove){
				PrintInfoLine("请选择己方棋子作为起点。");
				continue;
			}
			hasSel=true;
			selBoard=cellBoard;
			auto marks=MarkTargets(legal,selBoard);
			DrawAll(b,defAttr,myColor,true,selBoard,marks);
			
			ostringstream oss;
			oss<<"已选中起点：("<<(selBoard.r+1)<<","<<(selBoard.c+1)<<")  请点击终点。";
			PrintInfoLine(oss.str());
			continue;
		}
		
		// 组装走法
		Move mv;
		mv.from=selBoard;
		mv.to=cellBoard;
		mv.promo=0;
		mv.isCastle=false;
		mv.isEnPassant=false;
		
		FixFlagsByBoard(b,mv);
		
		// 若需要升变 -> 弹出鼠标可点的 Q/R/B/N
		Piece p=b.s[mv.from.r][mv.from.c];
		if(p.t=='P'){
			int promoRow=(b.sideToMove==WHITE?7:0);
			if(mv.to.r==promoRow){
				PrintInfoLine("需要升变：请点击下方 Q/R/B/N。");
				char pr = ChoosePromotionByMouse(defAttr);
				if(pr==0){
					// 取消升变：视为取消本步，回到重新选起点
					hasSel=false;
					DrawAll(b,defAttr,myColor,false,selBoard,{});
					PrintInfoLine("已取消升变选择。请重新选择起点。");
					continue;
				}
				mv.promo=pr;
			}
		}
		
		// 合法性校验
		bool ok=false;
		for(const auto& lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
		if(!ok){
			hasSel=false;
			DrawAll(b,defAttr,myColor,false,selBoard,{});
			PrintInfoLine("非法走法：请重新选择起点。");
			continue;
		}
		
		// 落子 + 发送
		b=ApplyMove(b,mv);
		hasSel=false;
		
		if(!SendLine(sock, EncodeMoveLine(mv))){
			DrawAll(b,defAttr,myColor,false,selBoard,{});
			PrintInfoLine("发送失败：连接断开。");
			break;
		}
		
		DrawAll(b,defAttr,myColor,false,selBoard,{});
		PrintInfoLine("走子成功，已发送给对手。等待对手...");
	}
	
	SetAttr(defAttr);
	GotoXY(0, PROMO_Y+3);
	closesocket(sock);
	WSACleanup();
	system("pause");
	return 0;
}
