#include <windows.h>
#include <tchar.h>//_tcschr
#include <stdio.h>//fread
#include <CommCtrl.h>//定义控件样式
#include <mmsystem.h>
#include "Resource.h"//资源
#include "ControlStyle.h"//清单文件
#pragma comment(lib, "WINMM.LIB")
#pragma comment(lib,"comctl32.lib")

const int width=400;
const int height=600;

#define REDO_NUM 20//真实撤销次数要减1
#define STEP_BEFORE_DEAD 5//死前留n步
#define RANDOM_STEP_THRESHOLD 8//随机模式下超过8步才算自己打的
#define REDO_SHRESHOLD 10000//撤销积分门槛 默认10000分得一次撤销机会
#define HIGHSCORE_COUNT 10//排行榜数量
#define HIGHSCORE_THRESHOLD 1000//低于1000分不计入高分榜
#define MAX_NAME_LENGTH 64//最大姓名长度

#define BIRTHNEW 1//产生新块计时器ID
#define FRAMETIME 10//动画每帧时间（毫秒）
const int margin_y1=26;

const int score_w=121;
const int score_h=70;

const int score_x=130;
const int score_margin_x=6;

const int vcenter_h=68;

const int border=15;
const int block=80;

const int bottom_text_h=25;

int iblock;//块间隙

const int round=5;//圆角半径

int button_w;
const int button_h=20;
const int button_s=5;

//颜色块
RECT rectName;
RECT rectScore,rectBest;
RECT rectScoreTitle,rectScoreNum;
RECT rectBestTitle,rectBestNum;
RECT rectMain;
RECT rect[4][4];
RECT rectVCenterText;
RECT rectBottomText;
//主窗体
HWND hwnd;

//控件
#define BNUM 5//按钮数量
HWND hwndButton[BNUM];
RECT rectButton[BNUM];

struct
{
	TCHAR Name[sizeof(TCHAR)==1U?13:8];//ansi为10，unicode为8
} sButton[BNUM]=
{
	TEXT("新游戏"),//0
	//	TEXT("保存"),
	TEXT("随机开局"),
	TEXT("撤销(0)"),//2
	TEXT("排行榜"),//3
	TEXT("说明")//4
};
#define NEWGAME 0
#define RANDOMGAME 1
#define REDO 2
#define HIGHSCORE 3
#define README 4

int onmouse=-1;//鼠标悬停按钮ID，未悬停为-1
int onrandom=FALSE;

//变量
HINSTANCE hInst;
unsigned int num[4][4]={0};
unsigned int step=0,step_after_random;
unsigned long score=0;
unsigned long high_score=0;
int has_record_score=FALSE;

static TCHAR buffer_temp[10];
unsigned int newnum_index=0;

//排行榜
struct
{
	TCHAR name[MAX_NAME_LENGTH];
	unsigned long score;
} sHighScore[HIGHSCORE_COUNT];

TCHAR szFilePath[255];
TCHAR szHighScore[sizeof(sHighScore)*HIGHSCORE_COUNT];

//撤销数据
unsigned int can_redo=0;//玩家可撤销次数
unsigned int redo_count=0;//撤销数据数量
unsigned int redo_score=0;//撤销积分
struct
{
	unsigned int score;
	unsigned int num[4][4];
} redo[REDO_NUM];

//设备相关参数
int cxScreen,cyScreen,cyCaption,cxSizeFrame,cySizeFrame;

//字体
const TCHAR FontName[]=TEXT("微软雅黑");//黑体

//颜色
const unsigned long crWhite=RGB(255,255,255);//白色
const unsigned long crBackground=RGB(250,248,239);//背景颜色
const unsigned long crGray=RGB(187,173,160);//分数块
const unsigned long crText=RGB(115,106,98);//深色标题2048
const unsigned long crScoreTitle=RGB(245,235,226);//最高分
const unsigned long crLessEqual8=RGB(119,110,101);//数字低于8为灰色，高于8为白色
const unsigned long cr2=RGB(238,228,218);
const unsigned long cr4=RGB(237,224,200);
const unsigned long cr8=RGB(242,177,121);
const unsigned long cr16=RGB(245,149,99);
const unsigned long cr32=RGB(246,124,95);
const unsigned long cr64=RGB(246,94,59);
#define cr128 RGB(237,204,97)
const unsigned long cr256=cr128;
const unsigned long cr512=cr128;
const unsigned long cr1024=cr128;
const unsigned long cr2048=RGB(237,194,46);

//declare
unsigned long Num2Color(unsigned int num);
void CopyArray(unsigned int result[4][4],unsigned int source[4][4]);
BOOL isAll0(unsigned int num[4][4]);
void NewRecord();
void Redo();
BOOL InHighScore();
TCHAR * int2ptchar(unsigned int i);
BOOL isDead();
void FillRectAdvance(HDC hdc,RECT *rect,unsigned long color);
void NewNum();
BOOL AskStartNewGame();
BOOL JudgeFreshHighScore();
void JudgeAction(HWND hwnd,BOOL move);
void DrawTextAdvance(HDC hdc,TCHAR text[],RECT *rect,long FontSize,int FontWeight,unsigned long color,const TCHAR FontName[],UINT format);
void FreshRedoButton();
void FreshHighScore(TCHAR szName[]);
void SaveGame();
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ChildWndProc (HWND, UINT, WPARAM , LPARAM);
BOOL CALLBACK AboutDlgProc(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam);
BOOL CALLBACK NameDlgProc(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam);

unsigned long Num2Color(unsigned int num)
{
	switch (num)
	{
	case 2:return cr2;break;
	case 4:return cr4;break;
	case 8:return cr8;break;
	case 16:return cr16;break;
	case 32:return cr32;break;
	case 64:return cr64;break;
	case 128:return cr128;break;
	case 256:return cr256;break;
	case 512:return cr512;break;
	case 1024:return cr1024;break;
	case 2048:return cr2048;break;
	default:return cr2048;break;
	}
}

void CopyArray(unsigned int result[4][4],unsigned int source[4][4])
{
	int i,j;
	for (i=0;i<4;i++)
		for (j=0;j<4;j++)
			result[i][j]=source[i][j];
}

void NewRecord()
{
	int i;
	for (i=min(step,REDO_NUM-1);i>=1;i--)
	{
		CopyArray(redo[i].num,redo[i-1].num);
		redo[i].score=redo[i-1].score;
	}
	CopyArray(redo[0].num,num);
	redo[0].score=score;
	if (redo_count<REDO_NUM-1) redo_count++;
	step++;
	step_after_random++;
}

BOOL isAll0(unsigned int num[4][4])
{
	int i,j;
	for (i=0;i<4;i++)
		for (j=0;j<4;j++)
			if (num[i][j]!=0)
			{
				return FALSE;
			}
			return TRUE;
}

void Redo()
{
	if (isAll0(redo[1].num)==FALSE)//非全零
	{
		int i,j;
		CopyArray(num,redo[1].num);
		score=redo[1].score;
		for (i=0;i<=REDO_NUM-2;i++)
		{
			CopyArray(redo[i].num,redo[i+1].num);
			redo[i].score=redo[i+1].score;
		}
		for (i=0;i<4;i++)
			for (j=0;j<4;j++)
				redo[min(step,REDO_NUM-1)].num[i][j]=0;
		redo[min(step,REDO_NUM-1)].score=0;
	}
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	MSG    msg;
	WNDCLASS wndclass;
	static TCHAR szAppName[] = TEXT("2048");
	hInst=hInstance;
	//获得程序路径
	GetModuleFileName(NULL,szFilePath,sizeof(szFilePath));
	(_tcsrchr(szFilePath, _T('\\')))[1] = 0;
	lstrcat(szFilePath,TEXT("2048.SAV"));

	//获得系统参数
	cxScreen = GetSystemMetrics(SM_CXSCREEN);
	cyScreen = GetSystemMetrics(SM_CYSCREEN);
	cyCaption=GetSystemMetrics(SM_CYCAPTION);
	cxSizeFrame=GetSystemMetrics(SM_CXSIZEFRAME);
	cySizeFrame=GetSystemMetrics(SM_CYSIZEFRAME);


	wndclass.style        = CS_HREDRAW | CS_VREDRAW ;
	wndclass.lpfnWndProc  = WndProc ;
	wndclass.cbClsExtra   = 0 ;
	wndclass.cbWndExtra   = 0 ;
	wndclass.hInstance    = hInstance ;
	wndclass.hIcon        = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICONSMALL));
	wndclass.hCursor      = LoadCursor (NULL, IDC_ARROW) ;
	wndclass.hbrBackground= CreateSolidBrush(crBackground);
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName= szAppName ;

	if (!RegisterClass (&wndclass))
	{
		MessageBox (NULL, TEXT ("This program requires Windows NT!"),szAppName, MB_ICONERROR) ;
		return 0 ;
	}

	wndclass.lpfnWndProc   = ChildWndProc ;
	wndclass.cbWndExtra    = sizeof (long) ;
	wndclass.hIcon         = NULL ;
	wndclass.lpszClassName = TEXT("szChildClass") ;

	RegisterClass (&wndclass) ;

	hwnd = CreateWindow(szAppName,
		TEXT ("2048 PC极速版1.0  by Tom Willow"),
		WS_OVERLAPPED|WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX,
		(cxScreen-width)/2,
		(cyScreen-height)/2,
		width+2*cxSizeFrame,//win7下宽度大10，xp下宽度大n（未测量）
		height+cyCaption+2*cySizeFrame,//-10
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow (hwnd, iCmdShow) ;
	UpdateWindow (hwnd) ;

	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg) ;
		DispatchMessage (&msg) ;
	}
	return msg.wParam ;
}

TCHAR * int2ptchar(unsigned int i)
{
	wsprintf(buffer_temp,TEXT("%d"),i);
	return buffer_temp;
}

void FreshRedoButton()
{
	wsprintf(sButton[REDO].Name,TEXT("撤销(%d)"),can_redo);
	EnableWindow(hwndButton[REDO],can_redo>0?TRUE:FALSE);
	InvalidateRect(hwndButton[REDO],NULL,TRUE);
}
BOOL isDead()
{
	int i,j;
	for (i=0;i<4;i++)
		for (j=0;j<4;j++)
			if (num[i][j]==0)
				return FALSE;
	for (i=0;i<4;i++)
		for (j=0;j<3;j++)
			if (num[i][j]==num[i][j+1])
			{
				return FALSE;
				break;
			};
	for (i=0;i<4;i++)
		for (j=0;j<3;j++)
			if (num[j][i]==num[j+1][i])
			{
				return FALSE;
				break;
			}
			return TRUE;
}

void FillRectAdvance(HDC hdc,RECT *rect,unsigned long color)
{
	HBRUSH hBrush;
	hBrush=CreateSolidBrush(color);
	FillRect(hdc,rect,hBrush);
	DeleteObject(hBrush);
}

void FreshMainRect()
{
	HDC hdc;
	HBRUSH hBrush;
	HPEN hPen;
	int i,j;
	hdc=GetDC(hwnd);

	SetBkMode(hdc,TRANSPARENT);
	hBrush=CreateSolidBrush(crGray);
	SelectObject(hdc,hBrush);//分数块背景
	hPen=CreatePen(PS_NULL,0,0);
	SelectObject(hdc,hPen);//去掉画笔
	//主矩形
	RoundRect(hdc,rectMain.left,rectMain.top,rectMain.right,rectMain.bottom,round,round);

	for (i=0;i<4;i++)
		for (j=0;j<4;j++)
		{
			if (num[i][j]!=0)
			{
				FillRectAdvance(hdc,&rect[i][j],Num2Color(num[i][j]));
				DrawTextAdvance(hdc,int2ptchar(num[i][j]),&rect[i][j],(num[i][j]<1024)?26:18,700,(num[i][j]<=8)?crLessEqual8:crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
			}
		};

	DeleteObject(hBrush);
	DeleteObject(hPen);
	ReleaseDC(hwnd,hdc);
}

void NewNum()
{
	unsigned int *p;
	int zero[16];//空白格
	int i,j=0;
	p=&num[0][0];
	for (i=0;i<16;i++)
		if (*(p+i)==0)
		{
			zero[j]=i;//顺序记下空白格的序号
			j++;
		};
	if (j!=0)
	{
		srand(GetTickCount());
		newnum_index=zero[rand() % j];
		p+=newnum_index;
		FreshMainRect();//先刷新画面再出新块
		*p=(rand()%2)?2:4;
		SetTimer(hwnd,BIRTHNEW,FRAMETIME,NULL);
	}
}

BOOL AskStartNewGame()
{
	TCHAR buffer[100];
	TCHAR title[10];
	if (isDead())
	{
		wsprintf(buffer,TEXT("没有可移动的块了。\n是否开始新游戏？\n（您还有%d次撤销机会）"),can_redo);
		lstrcpy(title,TEXT("游戏结束"));
	}
	else
	{
		lstrcpy(buffer,TEXT("是否开始新游戏？"));
		lstrcpy(title,TEXT("新游戏"));
	}
	MessageBeep(0);
	if (IDYES==MessageBox(hwnd,buffer,title,MB_YESNO|MB_ICONQUESTION))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void Fill0(HWND hwnd)
{
	int i,j;
	for (i=0;i<4;i++)
		for (j=0;j<4;j++)
			num[i][j]=0;
	NewNum();
	NewRecord();

	onrandom=FALSE;
	score=0;
	redo_score=0;
	can_redo=0;
	redo_count=0;
	has_record_score=FALSE;
	FreshRedoButton();
	SaveGame();
	//InvalidateRect(hwnd,NULL,TRUE);
}

BOOL InHighScore()
{
	if (score>=sHighScore[HIGHSCORE_COUNT-1].score)
		return TRUE;
	else
		return FALSE;
}

BOOL JudgeFreshHighScore()//返回FALSE则是随机成绩
{
	if (step!=step_after_random && step_after_random<RANDOM_STEP_THRESHOLD)//必须达到撤销存储步骤数才算玩家自己打的，否则名字为“随机”
	{
		FreshHighScore(TEXT("随机"));
		return FALSE;
	}
	else
	{
		DialogBox(hInst,TEXT("IDD_DIALOGNAME"),hwnd,NameDlgProc);
		return TRUE;
	}
}

void FreshHighScore(TCHAR szName[])
{
	unsigned long temp=0;
	TCHAR temp_name[MAX_NAME_LENGTH];
	int i,j;
	for (i=0;i<HIGHSCORE_COUNT;i++)
	{
		if (score>=sHighScore[i].score)
		{
			//留名
			high_score=max(score,sHighScore[0].score);//超过第一名刷新最高分

			temp=sHighScore[i].score;
			lstrcpy(temp_name,sHighScore[i].name);//当前排行存入临时数据

			sHighScore[i].score=score;
			lstrcpy(sHighScore[i].name,szName);//刷新排行榜
			has_record_score=TRUE;
			break;
		}
	}
	if (temp>0)//else的话没有更新榜单
	{
		for (j=HIGHSCORE_COUNT-1;j>i+1;j--)
		{
			sHighScore[j].score=sHighScore[j-1].score;
			lstrcpy(sHighScore[j].name,sHighScore[j-1].name);
		}
		sHighScore[i+1].score=temp;
		lstrcpy(sHighScore[i+1].name,temp_name);
	}
}

void JudgeAction(HWND hwnd,BOOL move)//所有方向键均会经过此处
{
	if (move)
	{
		NewNum();
		NewRecord();
		if (redo_score>=REDO_SHRESHOLD)//每得一万分得一次撤销机会，一次合成超过了1万分也只计一次
		{
			can_redo+=(can_redo<=REDO_NUM-2)?1:0;//如果大于可撤销次数则不加次数，但积累分依然要减掉
			FreshRedoButton();
			redo_score=redo_score % REDO_SHRESHOLD;
		}
		if (onrandom!=TRUE)
			high_score=max(score,high_score);
		InvalidateRect(hwnd,&rectScore,TRUE);
		InvalidateRect(hwnd,&rectBest,TRUE);
		//InvalidateRect(hwnd,&rectMain,TRUE);
		if (onrandom==FALSE) PlaySound((LPCTSTR)IDR_WAVE1,hInst,SND_RESOURCE|SND_ASYNC|SND_NOSTOP);//
		if (isDead())
		{
			if (onrandom!=TRUE)//正常死亡
			{
				if (InHighScore() && score>=HIGHSCORE_THRESHOLD)//进入高分榜，大于1000分
				{
					if (can_redo==0)
					{
						if (JudgeFreshHighScore()==FALSE)//随机成绩，非随机成绩弹出排行榜，手动清零
							if (AskStartNewGame()==TRUE)
							{
								Fill0(hwnd);
							}
					}
					else
					{
						if (AskStartNewGame()==TRUE)
						{
							JudgeFreshHighScore();
							Fill0(hwnd);
						}
					}
				}
				else//没有进入高分榜
				{
					if (AskStartNewGame()==TRUE)
						Fill0(hwnd);
				}
			}
			else//随机开局
			{
			}
		}
	}
	else//没有产生移动，但不代表死亡
	{
		if (isDead())//死亡后再次没有可点的块到达这里
			SendMessage(hwndButton[NEWGAME],WM_LBUTTONDOWN,0,0);
	}
}
void DrawTextAdvance(HDC hdc,TCHAR text[],RECT *rect,long FontSize,int FontWeight,unsigned long color,const TCHAR FontName[],UINT format)
{
	long lfHeight;
	HFONT hf;
	lfHeight = -MulDiv(FontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	hf = CreateFont(lfHeight, 0, 0, 0, FontWeight, 0, 0, 0, 0, 0, 0, 0, 0, FontName);
	SelectObject(hdc,hf);
	SetTextColor(hdc,color);
	DrawText(hdc,text,-1,rect,format);
	DeleteObject(hf);
}

LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		{
			int i,j;
			int dx,dy;
			RECT ClientRect,WindowRect;

			//控件初始化
			InitCommonControls();

			//窗口大小修正，如果没有这段则窗口大小不正确
			GetClientRect(hwnd,&ClientRect);
			GetWindowRect(hwnd,&WindowRect);
			dx=ClientRect.right-width;
			dy=ClientRect.bottom-height;
			SetWindowPos(hwnd,NULL,(cxScreen-width)/2,(cyScreen-height)/2,(WindowRect.right-WindowRect.left)-dx,(WindowRect.bottom-WindowRect.top)-dy,0);

			rectName.left=border;
			rectName.top=margin_y1;
			rectName.right=score_x-score_margin_x;
			rectName.bottom=rectName.top+score_h;

			rectScore.left=score_x;
			rectScore.top=margin_y1;
			rectScore.right=rectScore.left+score_w;
			rectScore.bottom=rectScore.top+score_h;

			SetRect(&rectScoreTitle,rectScore.left,rectScore.top,rectScore.right,(int)(rectScore.top+0.4*(rectScore.bottom-rectScore.top)));
			SetRect(&rectScoreNum,rectScore.left,(int)(rectScore.top+0.4*(rectScore.bottom-rectScore.top)),rectScore.right,rectScore.bottom);

			rectBest.right=width-border;
			rectBest.left=rectBest.right-score_w;
			rectBest.top=margin_y1;
			rectBest.bottom=rectBest.top+score_h;

			SetRect(&rectBestTitle,rectBest.left,rectBest.top,rectBest.right,(int)(rectBest.top+0.4*(rectBest.bottom-rectBest.top)));
			SetRect(&rectBestNum,rectBest.left,(int)(rectBest.top+0.4*(rectBest.bottom-rectBest.top)),rectBest.right,rectBest.bottom);

			rectVCenterText.left=border;
			rectVCenterText.right=width-border;
			rectVCenterText.top=rectScore.bottom;
			rectVCenterText.bottom=rectVCenterText.top+vcenter_h;

			rectMain.left=border;
			rectMain.right=width-border;
			rectMain.top=rectVCenterText.bottom;
			rectMain.bottom=rectMain.top+(rectMain.right-rectMain.left);

			iblock=((rectMain.right-rectMain.left)-block*4)/5;

			for (i=0;i<4;i++)
				for (j=0;j<4;j++)
				{
					rect[i][j].left=rectMain.left+(j+1)*iblock+j*block;
					rect[i][j].right=rect[i][j].left+block;
					rect[i][j].top=rectMain.top+(i+1)*iblock+i*block;
					rect[i][j].bottom=rect[i][j].top+block;
				};

			rectBottomText.left=rectMain.left;
			rectBottomText.right=rectMain.right;
			rectBottomText.top=rectMain.bottom;
			rectBottomText.bottom=rectBottomText.top+bottom_text_h;

			button_w=(width-2*border-(BNUM-1)*button_s)/BNUM;//宽-2个边框 -5个间隙button_s

			for (i=0;i<BNUM;i++)
			{
				rectButton[i].top=height-border-button_h;
				rectButton[i].bottom=height-border;
				rectButton[i].left=border+i*button_s+i*button_w;//边距+间隙+按钮宽
				rectButton[i].right=rectButton[i].left+button_w;
				hwndButton[i] =CreateWindow(
					TEXT("szChildClass"),
					sButton[i].Name,
					WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,//BS_PUSHBUTTON
					rectButton[i].left,
					rectButton[i].top,
					button_w,
					button_h,
					hwnd, (HMENU) i,
					((LPCREATESTRUCT) lParam)->hInstance,
					NULL);
			}

			{
				FILE *file;
				if (_tfopen_s(&file,szFilePath,TEXT("r,ccs=UNICODE"))==0)//成功则为0
				{
					fread(sHighScore,sizeof(sHighScore),1,file);
					fread(num,sizeof(num),1,file);
					fread(&step,sizeof(step),1,file);
					fread(&step_after_random,sizeof(step_after_random),1,file);
					fread(&score,sizeof(score),1,file);
					fread(&high_score,sizeof(high_score),1,file);
					fread(&has_record_score,sizeof(has_record_score),1,file);
					fread(redo,sizeof(redo),1,file);
					fread(&can_redo,sizeof(can_redo),1,file);
					fread(&redo_score,sizeof(redo_score),1,file);
					fread(&redo_count,sizeof(redo_score),1,file);
					_fcloseall();
					FreshRedoButton();
				}
			}

			if (step==0)
				Fill0(hwnd);

			return 0;
		}
	case WM_PAINT:
		{
			int i,j;
			HDC hdc;
			PAINTSTRUCT ps ;
			HBRUSH hBrush;
			HPEN hPen;
			hdc = BeginPaint(hwnd, &ps);

			SetBkMode(hdc,TRANSPARENT);

			hBrush=CreateSolidBrush(crGray);
			SelectObject(hdc,hBrush);//分数块背景

			hPen=CreatePen(PS_NULL,0,0);
			SelectObject(hdc,hPen);//去掉画笔

			DrawTextAdvance(hdc,TEXT("2048"),&rectName,34,0,crText,FontName,DT_CENTER|DT_VCENTER|DT_SINGLELINE);


			//画分数块
			RoundRect(hdc,rectScore.left,rectScore.top,rectScore.right,rectScore.bottom,round,round);

			DrawTextAdvance(hdc,TEXT("分数"),&rectScoreTitle,12,700,crScoreTitle,FontName,DT_CENTER|DT_SINGLELINE|DT_BOTTOM);
			DrawTextAdvance(hdc,int2ptchar(score),&rectScoreNum,24,0,crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_TOP);

			//画最高分块
			RoundRect(hdc,rectBest.left,rectBest.top,rectBest.right,rectBest.bottom,round,round);

			DrawTextAdvance(hdc,TEXT("最高分"),&rectBestTitle,12,700,crScoreTitle,FontName,DT_CENTER|DT_SINGLELINE|DT_BOTTOM);
			DrawTextAdvance(hdc,int2ptchar(high_score),&rectBestNum,24,0,crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_TOP);

			DrawTextAdvance(hdc,TEXT("把这些数合出2048！"),&rectVCenterText,13,0,crText,FontName,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
			DrawTextAdvance(hdc,TEXT("玩法：上下左右，移动色块，合出更大的数字！"),&rectBottomText,13,0,crText,FontName,DT_LEFT|DT_SINGLELINE|DT_VCENTER);

			//主矩形
			RoundRect(hdc,rectMain.left,rectMain.top,rectMain.right,rectMain.bottom,round,round);

			for (i=0;i<4;i++)
				for (j=0;j<4;j++)
				{
					if (num[i][j]!=0)
					{
						FillRectAdvance(hdc,&rect[i][j],Num2Color(num[i][j]));
						DrawTextAdvance(hdc,int2ptchar(num[i][j]),&rect[i][j],(num[i][j]<1024)?26:18,700,(num[i][j]<=8)?crLessEqual8:crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
					}
				};

			DeleteObject(hBrush);
			DeleteObject(hPen);
			EndPaint(hwnd, &ps);
			return 0 ;
		}
	case WM_TIMER:
		{
			switch (wParam)
			{
			case BIRTHNEW:
				{
					static int count=0;
					RECT rectnewnum_index;
					if (count<=10 && ( *(&num[0][0]+newnum_index)==2 || *(&num[0][0]+newnum_index)==4))
					{
						HDC hdc;
						hdc=GetDC(hwnd);
						SetBkMode(hdc,TRANSPARENT);

						rectnewnum_index.left=rect[newnum_index/4][newnum_index%4].left+block/2*(1-count/10.0);
						rectnewnum_index.right=rect[newnum_index/4][newnum_index%4].left+block/2*(1+count/10.0);
						rectnewnum_index.top=rect[newnum_index/4][newnum_index%4].top+block/2*(1-count/10.0);
						rectnewnum_index.bottom=rect[newnum_index/4][newnum_index%4].top+block/2*(1+count/10.0);

						FillRectAdvance(hdc,&rectnewnum_index,Num2Color(*(&num[0][0]+newnum_index)));
						DrawTextAdvance(hdc,int2ptchar(*(&num[0][0]+newnum_index)),&rectnewnum_index,((*(&num[0][0]+newnum_index)<1024)?26:18)*(count/10.0),700,(*(&num[0][0]+newnum_index)<=8)?crLessEqual8:crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_VCENTER);

						ReleaseDC(hwnd,hdc);
						count++;
					}
					else
					{
						KillTimer(hwnd,BIRTHNEW);
						count=0;
						//InvalidateRect(hwnd,&rectMain,FALSE);
					}
					break;
				}
			}
			return 0;
		}
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_UP:
			{
				BOOL move=FALSE;
				int now=0;
				int temp=1;
				int i,j,n;
				for (j=0;j<4;j++)
				{
					now=0;
					temp=1;
					while ((now<4)&&(temp<4))
					{
						if (num[now][j]==0)
						{
							if (num[temp][j]!=0)
							{
								move=TRUE;
								num[now][j]=num[temp][j];
								num[temp][j]=0;
								now++;
								temp=now+1;
							}
							else
							{
								temp++;
							}
						}
						else
						{
							now++;
							temp=now+1;
						}
					}//至此完成移动
					for (i=0;i<3;i++)
					{
						if (num[i][j]==num[i+1][j]&&num[i][j]!=0)//有得分块
						{
							move=TRUE;
							num[i][j]*=2;
							score+=num[i][j];
							redo_score+=num[i][j];
							for (n=i+1;n<3;n++)//底方上移
								num[n][j]=num[n+1][j];
							num[3][j]=0;
						}
					}
				}
				JudgeAction(hwnd,move);
				break;
			}
		case VK_DOWN:
			{
				BOOL move=FALSE;
				int now=0;
				int temp=1;
				int i,j,n;
				for (j=0;j<4;j++)
				{
					now=3;
					temp=2;
					while ((now>-1)&&(temp>-1))
					{
						if (num[now][j]==0)//now即当前指针
						{
							if (num[temp][j]!=0)
							{
								move=TRUE;
								num[now][j]=num[temp][j];
								num[temp][j]=0;
								now--;
								temp=now-1;
							}
							else
							{
								temp--;
							}
						}
						else
						{
							now--;
							temp=now-1;
						}
					}//至此完成移动
					for (i=3;i>0;i--)
					{
						if (num[i][j]==num[i-1][j]&&num[i][j]!=0)
						{
							move=TRUE;
							num[i][j]*=2;
							score+=num[i][j];
							redo_score+=num[i][j];
							for (n=i-1;n>0;n--)//上方下移
								num[n][j]=num[n-1][j];
							num[0][j]=0;
						}
					}
				}
				JudgeAction(hwnd,move);
				break;
			}
		case VK_LEFT:
			{
				BOOL move=FALSE;
				int now=0;
				int temp=1;
				int i,j,n;
				for (i=0;i<4;i++)
				{
					now=0;
					temp=1;
					while ((now<4)&&(temp<4))
					{
						if (num[i][now]==0)
						{
							if (num[i][temp]!=0)
							{
								move=TRUE;
								num[i][now]=num[i][temp];
								num[i][temp]=0;
								now++;
								temp=now+1;
							}
							else
							{
								temp++;
							}
						}
						else
						{
							now++;
							temp=now+1;
						}
					}//至此完成移动
					for (j=0;j<3;j++)
					{
						if (num[i][j]==num[i][j+1]&&num[i][j]!=0)
						{
							move=TRUE;
							num[i][j]*=2;
							score+=num[i][j];
							redo_score+=num[i][j];
							for (n=j+1;n<3;n++)//向左移
								num[i][n]=num[i][n+1];
							num[i][3]=0;
						}
					}
				}
				JudgeAction(hwnd,move);
				break;
			}
		case VK_RIGHT:
			{
				BOOL move=FALSE;
				int now=0;
				int temp=1;
				int i,j,n;
				for (i=0;i<4;i++)
				{
					now=3;
					temp=2;
					while ((now>-1)&&(temp>-1))
					{
						if (num[i][now]==0)//now即当前指针
						{
							if (num[i][temp]!=0)
							{
								move=TRUE;
								num[i][now]=num[i][temp];
								num[i][temp]=0;
								now--;
								temp=now-1;
							}
							else
							{
								temp--;
							}
						}
						else
						{
							now--;
							temp=now-1;
						}
					}//至此完成移动
					for (j=3;j>0;j--)
					{
						if (num[i][j]==num[i][j-1]&&num[i][j]!=0)
						{
							move=TRUE;
							num[i][j]*=2;
							score+=num[i][j];
							redo_score+=num[i][j];
							for (n=j-1;n>0;n--)//向右
								num[i][n]=num[i][n-1];
							num[i][0]=0;
						}
					}
				}
				JudgeAction(hwnd,move);
				break;
			}
		case VK_RETURN:
		case VK_SPACE:
			SendMessage(hwnd,WM_KEYDOWN,0x25+(rand() % 4),0);//随机按4个方向键
			break;
		}
		return 0;
	case WM_MOUSEMOVE:
		{
			int i;
			onmouse=-1;
			for (i=0;i<BNUM;i++)
				InvalidateRect(hwndButton[i],NULL,FALSE);
			return 0;
		}
	case WM_CLOSE:
		MessageBeep(0);
		if (IDOK==MessageBox(hwnd,TEXT("是否退出？\n（当前游戏将被保存）"),TEXT("退出"),MB_OKCANCEL|MB_ICONQUESTION))
		{
			//退出过程中保存文件
			SaveGame();
			break;//跳出switch，执行DefWindowProc，退出
		}
		else
		{
			return 0;//返回消息，不退出
		}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0 ;
	case WM_QUIT:
		return 0;
	}

	return DefWindowProc (hwnd, message, wParam, lParam);
}

void SaveGame()
{
	FILE *file;
	if (_tfopen_s(&file,szFilePath,TEXT("w+,ccs=UNICODE"))==0)//成功则为0
	{
		fwrite(sHighScore,sizeof(sHighScore),1,file);
		fwrite(num,sizeof(num),1,file);
		fwrite(&step,sizeof(step),1,file);
		fwrite(&step_after_random,sizeof(step_after_random),1,file);
		fwrite(&score,sizeof(score),1,file);
		fwrite(&high_score,sizeof(high_score),1,file);
		fwrite(&has_record_score,sizeof(has_record_score),1,file);
		fwrite(redo,sizeof(redo),1,file);
		fwrite(&can_redo,sizeof(can_redo),1,file);
		fwrite(&redo_score,sizeof(redo_score),1,file);
		fwrite(&redo_count,sizeof(redo_count),1,file);
		_fcloseall();
	}
	else
		MessageBox(hwnd,TEXT("进度保存失败。"),TEXT("提示"),0);
}

LRESULT CALLBACK ChildWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		{
			HDC hdc;

			PAINTSTRUCT ps ;
			RECT rect;
			HDC hDCMem;
			HBITMAP hBitmap;
			hdc = BeginPaint(hwnd, &ps);
			GetClientRect (hwnd, &rect);

			hDCMem = CreateCompatibleDC(hdc);
			hBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
			SelectObject(hDCMem, hBitmap);

			FillRectAdvance(hDCMem,&rect,onmouse==GetDlgCtrlID(hwnd)?cr32:crGray);
			SetBkMode(hDCMem,TRANSPARENT);
			DrawTextAdvance(hDCMem,sButton[GetDlgCtrlID(hwnd)].Name,&rect,10,700,crWhite,FontName,DT_CENTER|DT_SINGLELINE|DT_VCENTER);

			BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);
			DeleteObject(hBitmap);
			DeleteDC(hDCMem);

			EndPaint(hwnd, &ps);
			return 0 ;
		}
	case WM_LBUTTONDOWN:
		switch (GetDlgCtrlID(hwnd))
		{
		case NEWGAME://新游戏
			if (AskStartNewGame()==TRUE)
			{
				if (has_record_score==FALSE)//没有记录成绩
					if (InHighScore() && score>=HIGHSCORE_THRESHOLD)//进入高分榜，大于1000分
						JudgeFreshHighScore();//随机成绩，非随机成绩弹出排行榜，然后清零
				Fill0(GetParent(hwnd));
			}
			break;
		case RANDOMGAME:
			if (redo_count>STEP_BEFORE_DEAD)
			{
				MessageBeep(0);
				if (MessageBox(GetParent(hwnd),TEXT("是否随机开局？\n（将开始新游戏）"),TEXT("随机开局"),MB_YESNO|MB_ICONQUESTION)==IDNO)
					break;
			}
			score=0;
			Fill0(hwnd);
			onrandom=TRUE;
			while (isDead()==FALSE)
				SendMessage(GetParent(hwnd),WM_KEYDOWN,0x25+(rand() % 4),0);//随机按4个方向键
			while (redo_count>(REDO_NUM-1)-STEP_BEFORE_DEAD)//留死前N步
			{
				Redo();
				redo_count--;
				step--;
			}
			step_after_random=0;
			redo_count=0;
			can_redo=0;
			FreshRedoButton();
			InvalidateRect(GetParent(hwnd),NULL,TRUE);
			PlaySound((LPCTSTR)IDR_WAVE1,hInst,SND_RESOURCE|SND_ASYNC);
			onrandom=FALSE;
			break;
		case REDO://撤销
			if (can_redo>0)
			{
				if (redo_count>0)
				{
					Redo();
					redo_count--;
					step--;
					step_after_random--;
					can_redo--;
					FreshRedoButton();
					InvalidateRect(GetParent(hwnd),NULL,TRUE);
				}
			}
			break;
		case HIGHSCORE:
			{
				int i;
				MessageBeep(0);
				lstrcpy(szHighScore,TEXT("\t--- 排行榜 ---\n"));
				lstrcat(szHighScore,TEXT("\n 排名    姓名\t  得分"));
				for (i=0;i<10;i++)
				{
					if (sHighScore[i].score!=0)
					{
						wsprintf(buffer_temp,TEXT("\n  %2d      %-10.10s\t  %-d分"),i+1,sHighScore[i].name,sHighScore[i].score);
						lstrcat(szHighScore,buffer_temp);
					}
					else
						break;
				}
				MessageBox(GetParent(hwnd),szHighScore,TEXT("排行榜"),0);
				break;
			}
		case README://说明
			MessageBeep(0);
			DialogBox(hInst,TEXT("IDD_ABOUT"),hwnd,AboutDlgProc);
			break;
		}
		return 0;
	case WM_MOUSEMOVE:
		{
			int i;
			onmouse=GetDlgCtrlID(hwnd);
			for (i=0;i<BNUM;i++)
				InvalidateRect(hwndButton[i],NULL,FALSE);
			return 0;
		}
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

BOOL CALLBACK AboutDlgProc(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hDlg,0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

BOOL CALLBACK NameDlgProc(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		PlaySound((LPCTSTR)IDR_CONGRATULATIONS,hInst,SND_RESOURCE|SND_ASYNC);//|SND_LOOP
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				TCHAR szTemp[MAX_NAME_LENGTH];
				GetWindowText(GetDlgItem(hDlg,IDC_EDITNAME),szTemp,sizeof(szTemp));//此处应该限定名字长度，尚未完成
				if (lstrlen(szTemp)>0)
					FreshHighScore(szTemp);
				else
					FreshHighScore(TEXT("无名"));
				EndDialog(hDlg,0);
				PlaySound(NULL,NULL,NULL);
				SendMessage(hwndButton[HIGHSCORE],WM_LBUTTONDOWN,0,0);
				return TRUE;
			}
		case IDCANCEL:
			FreshHighScore(TEXT("无名"));
			EndDialog(hDlg,0);
			PlaySound(NULL,NULL,NULL);
			SendMessage(hwndButton[HIGHSCORE],WM_LBUTTONDOWN,0,0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}