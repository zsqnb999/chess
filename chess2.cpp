#include <bits/stdc++.h>
#include <windows.h>
#include <chrono>
using namespace std;

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
static const short BOARD_LINES = 1 + 8 + 2;
static const short INFO_Y = BOARD_LINES;
static const short INPUT_Y = BOARD_LINES+1;
static bool ConsoleCharToBoardCell(SHORT x,SHORT y,Pos& out){
	const int originX=4;
	const int originY=1;
	if(y<originY || y>=originY+8) return false;
	if(x<originX || x>=originX+8*2) return false;
	int r=y-originY;
	int c=(x-originX)/2;
	if(!In(r,c)) return false;
	out={r,c};
	return true;
}

static void PrintInfoLine(const string& s){
	ClearLine(INFO_Y);
	cout<<s;
	ClearLine(INPUT_Y);
	GotoXY(0, INPUT_Y);
}

static void DrawAll(const Board& b,WORD defAttr,bool hasSel,Pos sel,const vector<Pos>& marks){
	WORD light=Bg(7),dark=Bg(2);
	WORD wfg=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD bfg=FOREGROUND_BLUE|FOREGROUND_INTENSITY;
	WORD emptyFg=defAttr&0x0F;
	
	auto isMarked=[&](int r,int c){
		for(auto &p: marks) if(p.r==r&&p.c==c) return true;
		return false;
	};
	
	GotoXY(0,0);
	SetAttr(defAttr);
	cout<<"    1  2  3  4  5  6  7  8\n";
	for(int r=0;r<8;r++){
		SetAttr(defAttr);
		cout<<" "<<(r+1)<<"  ";
		for(int c=0;c<8;c++){
			bool isLight=((r+c)%2==0);
			WORD bg=isLight?light:dark;
			
			if(hasSel && sel.r==r && sel.c==c) bg=Bg(6);
			else if(isMarked(r,c)) bg=Bg(5);
			
			WORD fg=emptyFg;
			if(b.s[r][c].c==WHITE) fg=wfg;
			else if(b.s[r][c].c==BLACK) fg=bfg;
			
			SetAttr(bg|fg);
			cout<<b.s[r][c].t<<' ';
		}
		SetAttr(defAttr);
		cout<<"\n";
	}
	SetAttr(defAttr);
	cout<<"轮到："<<(b.sideToMove==WHITE?"白方":"黑方")<<"\n";
	cout<<"左键：点起点再点终点；右键取消。点击后看下面信息行。\n";
	PrintInfoLine("准备就绪：请点击一个己方棋子作为起点。");
}
int main(){
	EnableMouseInput();
	WORD defAttr=GetDefaultAttr();
	Board b; Init(b);
	mt19937 rng((uint32_t)chrono::high_resolution_clock::now().time_since_epoch().count());
	b.sideToMove = (uniform_int_distribution<int>(0,1)(rng)==0 ? WHITE : BLACK);
	bool hasSel=false;
	Pos sel{0,0};
	ClearScreenWin();
	DrawAll(b,defAttr,hasSel,sel,{});
	while(true){
		auto legal=GenLegal(b);
		if(legal.empty()){
			PrintInfoLine(InCheck(b,b.sideToMove) ? "游戏结束：将死" : "游戏结束：逼和");
			break;
		}
		INPUT_RECORD rec;
		DWORD nRead=0;
		ReadConsoleInput(HIn(), &rec, 1, &nRead);
		if(rec.EventType!=MOUSE_EVENT) continue;
		auto& me=rec.Event.MouseEvent;
		if(me.dwEventFlags!=0) continue;
		if(me.dwButtonState & RIGHTMOST_BUTTON_PRESSED){
			hasSel=false;
			DrawAll(b,defAttr,hasSel,sel,{});
			PrintInfoLine("已取消选择。请重新点击起点。");
			continue;
		}
		if(!(me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) continue;
		
		Pos cell;
		if(!ConsoleCharToBoardCell(me.dwMousePosition.X, me.dwMousePosition.Y, cell)){
			PrintInfoLine("你点的位置不在棋盘上。");
			continue;
		}
		if(!hasSel){
			if(b.s[cell.r][cell.c].c!=b.sideToMove){
				PrintInfoLine("请选择己方棋子作为起点。");
				continue;
			}
			hasSel=true;
			sel=cell;
			auto marks=MarkTargets(legal,sel);
			DrawAll(b,defAttr,hasSel,sel,marks);
			
			ostringstream oss;
			oss<<"已选中起点：("<<(sel.r+1)<<","<<(sel.c+1)<<")  请点击终点。";
			PrintInfoLine(oss.str());
			continue;
		}
		Move mv;
		mv.from=sel;
		mv.to=cell;
		mv.isCastle=false;
		mv.isEnPassant=false;
		mv.promo=0;
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
		if(p.t=='P'){
			int promoRow=(b.sideToMove==WHITE?7:0);
			if(mv.to.r==promoRow){
				PrintInfoLine("需要升变：请在此行输入 Q/R/B/N 然后回车。");
				char pr=0;
				cin>>pr;
				pr=toupper((unsigned char)pr);
				if(pr=='Q'||pr=='R'||pr=='B'||pr=='N') mv.promo=pr;
			}
		}
		bool ok=false;
		for(const auto& lm: legal) if(SameMove(lm,mv)){ ok=true; break; }
		if(!ok){
			hasSel=false;
			DrawAll(b,defAttr,hasSel,sel,{});
			PrintInfoLine("非法走法：请重新选择起点。");
			continue;
		}
		b=ApplyMove(b,mv);
		hasSel=false;
		
		DrawAll(b,defAttr,hasSel,sel,{});
		PrintInfoLine("走子成功。请点击下一步的起点。");
	}
	
	SetAttr(defAttr);
	GotoXY(0, INPUT_Y+2);
	system("pause");
	return 0;
}
