//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include "math.h"
#include "stdlib.h"
#include "bass.h"
//---------------------------------------------------------------------------
#define debug
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#ifdef debug
	FILE *logFile;
#endif

#pragma package(smart_init)
#pragma comment(lib,"bass.lib")
#pragma resource "*.dfm"

TForm1 *Form1;
TPaintBox *lPaintBox;
TPaintBox *rPaintBox;
TScrollBar *scrollBar;

short *lThread;
short *rThread;
int length;        //����� ���� ����� � ��������
int visibleLength; //����� ������� ����� � ��������
int leftPeak;
int rightPeak;
int leftSelected;
int rightSelected;
int cursor; //����� �������, �� ������� ������ ���������������

bool isSelecting = false;
bool isMoving = false;
int selectionStart;   //������� ������ ���������
int prevMoveX;      //������� ����������� ��������� ���� ��� ��������������

HSAMPLE sample;
const int MAX_PEAK_VALUE = 32767;
const int MIN_PEAKS = 10; //����������� ���������� ��������, ������������ �� ������

void drawThread(TPaintBox *paintBox, short peaks[]);

//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
	: TForm(Owner){
#ifdef debug
	logFile =  fopen("log.txt", "w");
#endif
}
//---------------------------------------------------------------------------
int peakToX(int nom){
	float width = lPaintBox->Width;
	float peaksPerPixel = visibleLength/width;
	return (nom-leftPeak)/peaksPerPixel;
}
//---------------------------------------------------------------------------
int XToPeak(int x){
	float width = lPaintBox->Width;
	float peaksPerPixel = visibleLength/width;
	return leftPeak+x*peaksPerPixel;
}
//---------------------------------------------------------------------------
String millisToString(int l){
	String ans;
	if (l/60000)
		ans = l/60000;
	else
		ans = "0";
	ans+=":";
	l%=60000;
	if (l/1000 > 9)
		ans+=l/1000;
	else
		ans+="0"+(String (l/1000));
	ans+=":";
	l%=1000;
	ans+= (l/100)?String(l/100):"0";
	l%=100;
	ans+= (l/10)?String(l/10):"0";
	l%=10;
	ans+= (l)?String(l):"0";
	return ans;
}
//---------------------------------------------------------------------------
void updateInfo(){
	Form1->PLength->Caption = millisToString(length/44.1);
	Form1->PPeaks->Caption = length;
	Form1->PCursor->Caption = millisToString(cursor/44.1);
	Form1->PSelLen->Caption = millisToString((rightSelected-leftSelected+1)/44.1);
	Form1->PLeft->Caption = millisToString(leftPeak/44.1);
	Form1->PRight->Caption = millisToString(rightPeak/44.1);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FormCreate(TObject *Sender){
	lPaintBox = LPaintBox;       //�������� ������ �� ����� ��� ��������� �������
	rPaintBox = RPaintBox;
	scrollBar = ScrollBar;
	if (HIWORD(BASS_GetVersion())!=BASSVERSION) {
		Application->MessageBox(L"������ ������ BASS.", NULL, MB_OK);
		exit(0);
	}
	if (!BASS_Init(-1, 44100, 0, Handle, NULL)) {
		Application->MessageBox(L"������ init BASS.", NULL, MB_OK);
		exit(0);
	}

    length = 1;
	lThread = new short[length];
	rThread = new short[length];
	leftPeak = 0;
	rightPeak = 0;
	visibleLength = 1;
	leftSelected = 0;
	rightSelected = 0;
	scrollBar->PageSize = (visibleLength*scrollBar->Max)/length;

	drawThread(LPaintBox, lThread);     //��������� �������
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

void drawThread(TPaintBox *paintBox, short peaks[]){
	paintBox->Canvas->Pen->Color = clBlack;      //��������� ��� ������ ������
	paintBox->Canvas->Brush->Color = clBlack;
	paintBox->Canvas->Rectangle(0,0,paintBox->Width,paintBox->Height);

	float width = paintBox->Width;           //������� ������
	int height = paintBox->Height;
	int start = height/2;
	float peaksPerPixel = visibleLength/width;

	paintBox->Canvas->Pen->Color = clSilver;      //��������� ��� ������ ������
	paintBox->Canvas->Brush->Color = clSilver;
	paintBox->Canvas->Rectangle(peakToX(leftSelected),0,peakToX(rightSelected),paintBox->Height);

	paintBox->Canvas->MoveTo(0,start);
	if (visibleLength>0) {
		paintBox->Canvas->Pen->Color = clLime;
		paintBox->Canvas->Pen->Width = 2;
		int max;
		int min;
		int lBorder = MAX(leftPeak,leftSelected);
		lBorder = MIN(lBorder,rightPeak);
		int rBorder = MIN(rightPeak,rightSelected);
		rBorder = MAX(rBorder,leftPeak);
		if (peaksPerPixel>1) {
			float p = leftPeak; //����� �������, � �������� ��������� �������
			int hPos;
			int hNeg;
			float next; //����� �������, �� �������� ��� ���� �������
			for (int i=0; i < peakToX(lBorder); i++) {
				max = -MAX_PEAK_VALUE;
				min = MAX_PEAK_VALUE;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>max)
						max = peaks[j];
					if (peaks[j]<min)
						min = peaks[j];
				}
				hPos = -max;   //������ "���������"
				hNeg = -min;   //�������� ������ ��������

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->LineTo(i,start+hPos);
				paintBox->Canvas->LineTo(i,start+hNeg);
#ifdef debug
 //		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif
				p+=peaksPerPixel;
			}
			paintBox->Canvas->Pen->Color = clGreen;                   //��� ��������� ���������� �����
			for (int i=peakToX(lBorder); i < peakToX(rBorder); i++) {
				max = -MAX_PEAK_VALUE;
				min = MAX_PEAK_VALUE;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>max)
						max = peaks[j];
					if (peaks[j]<min)
						min = peaks[j];
				}
				hPos = -max;   //������ "���������"
				hNeg = -min;   //�������� ������ ��������

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->LineTo(i,start+hPos);
				paintBox->Canvas->LineTo(i,start+hNeg);
#ifdef debug
//		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif

				p+=peaksPerPixel;
			}
            paintBox->Canvas->Pen->Color = clLime;
			for (int i=peakToX(rBorder); i <= width; i++) {     //���� ��������, �� ������ �������� ������
				max = -MAX_PEAK_VALUE;
				min = MAX_PEAK_VALUE;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>max)
						max = peaks[j];
					if (peaks[j]<min)
						min = peaks[j];
				}
				hPos = -max;   //������ "���������"
				hNeg = -min;   //�������� ������ ��������

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->LineTo(i,start+hPos);
				paintBox->Canvas->LineTo(i,start+hNeg);
#ifdef debug
//		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif

				p+=peaksPerPixel;
			}
		}


		if (peaksPerPixel<=1) {
			paintBox->Canvas->MoveTo(0,start-(peaks[leftPeak] * height/2 / MAX_PEAK_VALUE));
			for (int i = leftPeak+1; i <=lBorder; i++)
				paintBox->Canvas->LineTo(round((i-leftPeak)*width/visibleLength),
											start-(peaks[i] * height/2 / MAX_PEAK_VALUE));
			paintBox->Canvas->Pen->Color = clGreen;
			for (int i=lBorder+1; i <= rBorder; i++) {
				paintBox->Canvas->LineTo(round((i-leftPeak)*width/visibleLength),
												start-(peaks[i] * height/2 / MAX_PEAK_VALUE));
			}
            paintBox->Canvas->Pen->Color = clLime;
			for (int i = rBorder+1; i <=rightPeak; i++)
				paintBox->Canvas->LineTo(round((i-leftPeak)*width/visibleLength),
											start-(peaks[i] * height/2 / MAX_PEAK_VALUE));
		}
	}

	paintBox->Canvas->Pen->Color = clRed;        //������� �������������� ����� 0 ������
	paintBox->Canvas->MoveTo(0,paintBox->Height/2);
	paintBox->Canvas->LineTo(paintBox->Width,paintBox->Height/2);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::LPaintBoxPaint(TObject *Sender){
	drawThread(lPaintBox, lThread);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxPaint(TObject *Sender){
	drawThread(rPaintBox, rThread);
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormDestroy(TObject *Sender){
#ifdef debug
	fclose(logFile);
#endif
	BASS_Free();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta,
		  TPoint &MousePos, bool &Handled){
	if (length == 0) {
		return;
	}
	if (WheelDelta<0 && visibleLength==length) {   //���� ������ �������� ������
		return;
	}
#ifdef debug
	//fprintf(logFile, "%d\n",WheelDelta);
#endif
	int newLength = round(visibleLength*(1-0.001*WheelDelta));
	if (newLength>length) {
		newLength = length;
	}
	if (newLength<MIN_PEAKS) {                  //���� ���������� ����� ������
		return;
	}

	int diff = (visibleLength-newLength)/2; //������ �� �����   |diff|----|diff|
	leftPeak = leftPeak+diff;
	if (leftPeak<0)
		leftPeak = 0;
	rightPeak = leftPeak+newLength-1;
	if (rightPeak>=length) {
		rightPeak = length-1;
		leftPeak = rightPeak-newLength+1;
	}
	visibleLength = newLength;
	updateInfo();

	float Max = scrollBar->Max;
	scrollBar->PageSize = round((visibleLength*Max)/length);
	scrollBar->Position = round(((leftPeak+rightPeak)*Max)/2/length) - scrollBar->PageSize/2;
	drawThread(LPaintBox, lThread);     //��������� �������
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	if (Button == mbLeft) {
		isSelecting = true;
		selectionStart = X;
		leftSelected = XToPeak(X);
		rightSelected = XToPeak(X+1)-1;

		updateInfo();
		drawThread(LPaintBox, lThread);     //��������� �������
		drawThread(RPaintBox, rThread);
	}
	if (Button == mbMiddle) {
		isMoving = true;
		prevMoveX = X;
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxMouseMove(TObject *Sender, TShiftState Shift, int X,
		  int Y){
	if (isSelecting) {
		if (selectionStart<=X) {
			leftSelected = XToPeak(selectionStart);
			rightSelected = XToPeak(X+1)-1;
		}else{
			leftSelected = XToPeak(X);
			rightSelected = XToPeak(selectionStart+1)-1;
		}

		updateInfo();
		drawThread(LPaintBox, lThread);     //��������� �������
		drawThread(RPaintBox, rThread);
	}
	if (isMoving) {
		int delta = prevMoveX-X;
		prevMoveX = X;
		float width = lPaintBox->Width;
		float peaksPerPixel = visibleLength/width;
		leftPeak += delta * peaksPerPixel;
		if (leftPeak<0) {
			leftPeak = 0;
		}
		rightPeak = leftPeak + visibleLength-1;
		if (rightPeak>=length) {
			rightPeak = length-1;
			leftPeak = rightPeak-visibleLength+1;
		}
		scrollBar->Position = round(((leftPeak+rightPeak)*100)/2/length) - scrollBar->PageSize/2;

		updateInfo();
		drawThread(LPaintBox, lThread);     //��������� �������
		drawThread(RPaintBox, rThread);
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	isSelecting = false;
	isMoving = false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ScrollBarScroll(TObject *Sender, TScrollCode ScrollCode, int &ScrollPos){
	int range = scrollBar->Max-scrollBar->PageSize+1; //�������� ����� ������� ��������
    int L = length - visibleLength; //��������, � ������� ����� ���� ����� �������
	leftPeak = ScrollPos*L/range;
	if (leftPeak<0) {
		leftPeak = 0;
	}
	rightPeak = leftPeak+visibleLength-1;
	if (rightPeak>=length) {
		rightPeak = length-1;
		leftPeak = rightPeak-visibleLength+1;
	}

#ifdef debug
   //	fprintf(logFile, "%d %d\n",ScrollPos,scrollBar->PageSize);
#endif

	updateInfo();
	drawThread(LPaintBox, lThread);     //��������� �������
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::LoadButtonClick(TObject *Sender){
 /*mydecoder := BASS_StreamCreateFile(false,PChar(OpenDialog1.FileName),0,0,BASS_STREAM_DECODE);
 mychannel:=bass_samplegetchannel(mysample,true);*/

	if (!(OpenDialog->Execute()))
		return;

	sample = BASS_SampleLoad(false, OpenDialog->FileName.c_str(),0,0,2,BASS_STREAM_PRESCAN);
	if (!sample) {
		Application->MessageBox(L"������ �������� �����.", NULL,MB_OK);
		PPeaks->Caption = BASS_ErrorGetCode();
		return;
	}
	BASS_SAMPLE info;
	if (!BASS_SampleGetInfo(sample, &info))
	  return;
	DWORD l = info.length;
	length = l/4;
	lThread = new short[length];
	rThread = new short[length];
	leftPeak = 0;
	rightPeak = length;
	visibleLength = length;
	leftSelected = 0;
	rightSelected = 0;

	DWORD* buff = new DWORD[length];
    PLength->Caption = length;
	if (!BASS_SampleGetData(sample, buff))
	  return;
	for (int i=0; i < length; i++) {
		lThread[i] = LOWORD(buff[i]);
		rThread[i] = HIWORD(buff[i]);
	}
	scrollBar->PageSize = (visibleLength*scrollBar->Max)/length;

	updateInfo();
	drawThread(LPaintBox, lThread);     //��������� �������
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormKeyDown(TObject *Sender, WORD &Key, TShiftState Shift){
	if (visibleLength==0)
        return;
	if (Key==VK_DELETE) {

	}
	if (Key==VK_UP) {
		for (int i=leftSelected; i <= rightSelected; i++) {
			if (lThread[i]*1.2>MAX_PEAK_VALUE)
				lThread[i] = MAX_PEAK_VALUE;
			else
				if (lThread[i]*1.2<-MAX_PEAK_VALUE)
					lThread[i] = -MAX_PEAK_VALUE;
				else
					lThread[i] *= 1.2;
			if (rThread[i]*1.2>MAX_PEAK_VALUE)
				rThread[i] = MAX_PEAK_VALUE;
			else
				if (rThread[i]*1.2<-MAX_PEAK_VALUE)
					rThread[i] = -MAX_PEAK_VALUE;
				else
					rThread[i] *= 1.2;
		}

		drawThread(LPaintBox, lThread);     //��������� �������
		drawThread(RPaintBox, rThread);
	}
	if (Key==VK_DOWN) {
        for (int i=leftSelected; i <= rightSelected; i++) {
			lThread[i] *= 0.8;
			rThread[i] *= 0.8;
		}

		drawThread(LPaintBox, lThread);     //��������� �������
		drawThread(RPaintBox, rThread);
	}
}
//---------------------------------------------------------------------------

