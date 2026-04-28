#include<bits/stdc++.h>
#include <iostream>
#include <windows.h>
using namespace std;
int a[10][10];
char b[10][10];
void SetColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}
int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD defaultColor = csbi.wAttributes;
		for(int i=1;i<=8;i++){
			a[1][i]=a[2][i]=1;
		}
		for(int i=1;i<=8;i++){
			a[7][i]=a[8][i]=2;
		}
		char op; 
		b[1][1]='R';
		b[1][2]='N';
		b[1][3]='B';
		b[1][4]='Q';
		b[1][5]='K';
		b[1][6]='B'; 
		b[1][7]='N';
		b[1][8]='R';
		for(int i=1;i<=8;i++){
			b[2][i]='S'; 
		}
		for(int i=3;i<=6;i++){
			for(int j=1;j<=8;j++){
				b[i][j]='O'; 
			}
		}
		b[8][1]='R';
		b[8][2]='N';
		b[8][3]='B';
		b[8][4]='Q';
		b[8][5]='K';
		b[8][6]='B'; 
		b[8][7]='N';
		b[8][8]='R';
		for(int i=1;i<=8;i++){
			b[7][i]='S'; 
		}
		int x1,x2,y1,y2;
		while(1){
			for(int i=1;i<=8;i++){
				for(int j=1;j<=8;j++){
					if(a[i][j]==1){
						SetColor(FOREGROUND_RED);
					}
					if(a[i][j]==2){
						SetColor(FOREGROUND_BLUE);
					}
					if(a[i][j]==0){
						SetColor(defaultColor);
					}
					cout<<b[i][j];
				}
				cout<<endl;
			}
			SetColor(defaultColor);
			cout<<"红方操作"<<endl;
			cin>>x1>>y1;
			if(x1==0 and y1==0){
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1;
				}
				a[1][5]=0;
				b[1][5]='O';
				a[1][6]=1;
				b[1][6]='R';
				a[1][7]=1;
				b[1][7]='K';
				a[1][8]=0;
				b[1][8]='O';
			}
			else if(x1==0 and y1==1){
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1;
				}
				a[1][1]=0;
				b[1][1]='O';
				a[1][2]=0;
				b[1][2]='O';
				a[1][4]=1;
				b[1][4]='R';
				a[1][3]=1;
				b[1][3]='K';
				a[1][5]=0;
				b[1][5]='O';
			}
			else{
				cin>>x2>>y2;
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1>>x2>>y2;
				}
				if(x2==8 and b[x1][y1]=='S'){
					cin>>op;
					b[x2][y2]=op;
					b[x1][y1]='O';
					a[x2][y2]=a[x1][y1];
					a[x1][y1]=0;
				}
				else{
					a[x2][y2]=a[x1][y1];
					a[x1][y1]=0;
					b[x2][y2]=b[x1][y1];
					b[x1][y1]='O';
				}
			}
			for(int i=1;i<=8;i++){
				for(int j=1;j<=8;j++){
					if(a[i][j]==1){
						SetColor(FOREGROUND_RED);
					}
					if(a[i][j]==2){
						SetColor(FOREGROUND_BLUE);
					}
					if(a[i][j]==0){
						SetColor(defaultColor);
					}
					cout<<b[i][j];
				}
				cout<<endl;
			}
			SetColor(defaultColor);
			cout<<"蓝方操作"<<endl;
			cin>>x1>>y1;
			if(x1==0 and y1==0){
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1;
				}
				a[8][5]=0;
				b[8][5]='O';
				a[8][6]=2;
				b[8][6]='R';
				a[8][7]=2;
				b[8][7]='K';
				a[8][8]=0;
				b[8][8]='O';
			}
			else if(x1==0 and y1==1){
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1;
				}
				a[8][1]=0;
				b[8][1]='O';
				a[8][2]=0;
				b[8][2]='O';
				a[8][4]=2;
				b[8][4]='R';
				a[8][3]=2;
				b[8][3]='K';
				a[8][5]=0;
				b[8][5]='O';
			}
			else{
				cin>>x2>>y2;
				while(a[x1][y1]!=2){
					cout<<"操作失败"<<endl;
					cin>>x1>>y1>>x2>>y2;
				}
				if(x2==1 and b[x1][y1]=='S'){
					cin>>op;
					b[x2][y2]=op;
					b[x1][y1]='O';
					a[x2][y2]=a[x1][y1];
					a[x1][y1]=0;
				}
				else{
					a[x2][y2]=a[x1][y1];
					a[x1][y1]=0;
					b[x2][y2]=b[x1][y1];
					b[x1][y1]='O';
				}
			}
			SetColor(defaultColor);
		}
    return 0;
}
