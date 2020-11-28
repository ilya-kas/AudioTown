//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include "math.h"
#include "stdlib.h"
#include "bass.h"
#include "bassenc_mp3.h"
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
TBitmap *lBitmap,*rBitmap;
TPanel *scroll;

short *lThread;
short *rThread;
DWORD *buff;
int length;        //длина всей песни в отсчётах
int visibleLength; //длина видимой части в отсчётах
int leftPeak;
int rightPeak;
int leftSelected;
int rightSelected;
int cursor; //номер отсчёта, на котором курсор воспроизведения

bool isSelecting = false;
bool isMoving = false;   //средней кнопкой мыши
bool isScrollerMoving = false;
int selectionStart;   //пиксель старта выделения
int prevMoveX;      //пиксель предыдущего положения пыши при перетаскивании

bool isPlaying = false;

HSAMPLE sample;
HCHANNEL channel;
int Scroll_Max;
const int MAX_PEAK_VALUE = 32767;
const int MIN_SCROLL_WIDTH = 10;
const int MIN_PEAKS = 10; //минимальное количество отсчётов, отображаемых на экране

void drawThread(TCanvas *Canvas, short peaks[]);

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
void drawWaves(){
	drawThread(lBitmap->Canvas,lThread);
	drawThread(rBitmap->Canvas,rThread);

	TRect field;
	field.init(0,0,lBitmap->Width,lBitmap->Height);
	lPaintBox->Canvas->CopyRect(field,lBitmap->Canvas,field);
	rPaintBox->Canvas->CopyRect(field,rBitmap->Canvas,field);
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
	lPaintBox = LPaintBox;       //получаем ссылки на рамки для отрисовки каналов
	rPaintBox = RPaintBox;
	lBitmap = new TBitmap();
	rBitmap = new TBitmap();
	lBitmap->Width = lPaintBox->Width;
	lBitmap->Height = lPaintBox->Height;
	rBitmap->Width = rPaintBox->Width;
	rBitmap->Height = rPaintBox->Height;
	scroll = Scroll;
	Scroll_Max = ScrollerContainer->Width;

	if (HIWORD(BASS_GetVersion())!=BASSVERSION) {
		Application->MessageBox(L"Ошибка версии BASS.", NULL, MB_OK);
		exit(0);
	}
	if (!BASS_Init(-1, 44100, BASS_DEVICE_FREQ, Handle, NULL)) {
		Application->MessageBox(L"Ошибка init BASS.", NULL, MB_OK);
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
	scroll->Left = 0;
	scroll->Width = Scroll_Max;

	drawWaves();
}
//---------------------------------------------------------------------------

void drawThread(TCanvas *Canvas, short peaks[]){
	Canvas->Pen->Color = clBlack;      //заполняем фон чёрным цветом
	Canvas->Brush->Color = clBlack;
	Canvas->Rectangle(0,0,lPaintBox->Width,lPaintBox->Height);

	float width = lPaintBox->Width;           //размеры холста
	int height = lPaintBox->Height;
	int start = height/2;
	float peaksPerPixel = visibleLength/width;

	Canvas->Pen->Color = clSilver;      //заполняем фон чёрным цветом
	Canvas->Brush->Color = clSilver;
	Canvas->Rectangle(peakToX(leftSelected),0,peakToX(rightSelected),lPaintBox->Height);

	Canvas->MoveTo(0,start);
	if (visibleLength>0) {
		Canvas->Pen->Color = clLime;
		Canvas->Pen->Width = 2;
		int max;
		int min;
		int lBorder = MAX(leftPeak,leftSelected);
		lBorder = MIN(lBorder,rightPeak);
		int rBorder = MIN(rightPeak,rightSelected);
		rBorder = MAX(rBorder,leftPeak);
		if (peaksPerPixel>1) {
			float p = leftPeak; //номер отсчёта, с которого следующий пиксель
			int hPos;
			int hNeg;
			float next; //номер отсчёта, до которого идёт этот пиксель
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
				hPos = -max;   //высота "столбиков"
				hNeg = -min;   //значения знаков наоборот

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				Canvas->LineTo(i,start+hPos);
				Canvas->LineTo(i,start+hNeg);
#ifdef debug
 //		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif
				p+=peaksPerPixel;
			}
			Canvas->Pen->Color = clGreen;                   //это отрисовка выделенной части
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
				hPos = -max;   //высота "столбиков"
				hNeg = -min;   //значения знаков наоборот

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				Canvas->LineTo(i,start+hPos);
				Canvas->LineTo(i,start+hNeg);
#ifdef debug
//		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif

				p+=peaksPerPixel;
			}
			Canvas->Pen->Color = clLime;
			for (int i=peakToX(rBorder); i <= width; i++) {     //если осталось, то правое лаймовым цветом
				max = -MAX_PEAK_VALUE;
				min = MAX_PEAK_VALUE;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>max)
						max = peaks[j];
					if (peaks[j]<min)
						min = peaks[j];
				}
				hPos = -max;   //высота "столбиков"
				hNeg = -min;   //значения знаков наоборот

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				Canvas->LineTo(i,start+hPos);
				Canvas->LineTo(i,start+hNeg);
#ifdef debug
//		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif

				p+=peaksPerPixel;
			}
		}


		if (peaksPerPixel<=1) {
			Canvas->MoveTo(0,start-(peaks[leftPeak] * height/2 / MAX_PEAK_VALUE));
			Canvas->Brush->Color = clLime;
			for (int i = leftPeak+1; i <lBorder; i++){
				int x = round((i-leftPeak)*width/visibleLength),y = start-(peaks[i] * height/2 / MAX_PEAK_VALUE);
				Canvas->LineTo(x,y);
				if (peaksPerPixel<0.05)
					Canvas->Rectangle(x-3,y-2,x+3,y+4);
			}
			Canvas->Pen->Color = clGreen;
			for (int i=lBorder; i <= rBorder; i++) {
				int x = round((i-leftPeak)*width/visibleLength),y = start-(peaks[i] * height/2 / MAX_PEAK_VALUE);
				Canvas->LineTo(x,y);
				if (peaksPerPixel<0.05)
					Canvas->Rectangle(x-3,y-2,x+3,y+4);
			}
			Canvas->Pen->Color = clLime;
			for (int i = rBorder+1; i <=rightPeak; i++){
				int x = round((i-leftPeak)*width/visibleLength),y = start-(peaks[i] * height/2 / MAX_PEAK_VALUE);
				Canvas->LineTo(x,y);
				if (peaksPerPixel<0.05)
					Canvas->Rectangle(x-3,y-2,x+3,y+4);
			}
		}
	}

	if (leftPeak<=cursor && cursor<=rightPeak) {   //отрисовка курсора проигрывания
		Canvas->Pen->Color = clGray;
		Canvas->MoveTo(peakToX(cursor),0);
		Canvas->LineTo(peakToX(cursor),lPaintBox->Height);
	}

	Canvas->Pen->Color = clRed;        //красная горизонтальная линия 0 уровня
	Canvas->MoveTo(0,lPaintBox->Height/2);
	Canvas->LineTo(lPaintBox->Width,lPaintBox->Height/2);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::LPaintBoxPaint(TObject *Sender){
	drawWaves();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxPaint(TObject *Sender){
	drawWaves();
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormDestroy(TObject *Sender){
#ifdef debug
	fclose(logFile);
#endif
	StopButtonClick(Sender);
	BASS_Free();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta,
		  TPoint &MousePos, bool &Handled){
	if (length == 0) {
		return;
	}
	if (WheelDelta<0 && visibleLength==length) {   //если дальше отдалить нельзя
		return;
	}
#ifdef debug
	fprintf(logFile, "%d %d\n",leftPeak,rightPeak);
#endif
	int newLength = round(visibleLength*(1-0.001*WheelDelta));
	if (newLength>length) {
		newLength = length;
	}
	if (newLength<MIN_PEAKS) {                  //если приблизить ближе нельзя
		return;
	}

	int diff = (visibleLength-newLength)/2; //отступ по краям   |diff|----|diff|
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

	float Max = Scroll_Max;
	scroll->Width = round((visibleLength*Max)/length);
	scroll->Left = round(((leftPeak+rightPeak)*Max)/2/length) - scroll->Width/2;
	if (scroll->Width<MIN_SCROLL_WIDTH)
		scroll->Width = MIN_SCROLL_WIDTH;

	drawWaves();
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
		drawWaves();
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
			if (rightSelected > rightPeak)
				rightSelected = rightPeak;
		}else{
			leftSelected = XToPeak(X);
			if (leftSelected < leftPeak)
				leftSelected = leftPeak;
			rightSelected = XToPeak(selectionStart+1)-1;
		}

		updateInfo();
		drawWaves();
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
		float l =length;
		scroll->Left = round((leftPeak+rightPeak)/2/l*Scroll_Max) - scroll->Width/2;

		updateInfo();
		drawWaves();
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	if (selectionStart==X) {
		leftSelected = 0;
		rightSelected = 0;
		cursor = XToPeak(selectionStart);
		BASS_ChannelSetPosition(channel,cursor*4,BASS_POS_BYTE);

		updateInfo();
		drawWaves();
	}
	isSelecting = false;
	isMoving = false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::LoadButtonClick(TObject *Sender){
	if (!(OpenDialog->Execute()))
		return;

	sample = BASS_SampleLoad(false, OpenDialog->FileName.c_str(),0,0,2,BASS_STREAM_PRESCAN);
	if (!sample) {
		Application->MessageBox(L"Ошибка загрузки песни.", NULL,MB_OK);
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
	channel = BASS_SampleGetChannel(sample,true);

	buff = new DWORD[length];
    PLength->Caption = length;
	if (!BASS_SampleGetData(sample, buff))
	  return;
	for (int i=0; i < length; i++) {
		lThread[i] = LOWORD(buff[i]);
		rThread[i] = HIWORD(buff[i]);
	}
	scroll->Left = 0;
	scroll->Width = Scroll_Max;

	updateInfo();
	drawWaves();
}
//---------------------------------------------------------------------------


void __fastcall TForm1::FormKeyDown(TObject *Sender, WORD &Key, TShiftState Shift){
	if (visibleLength==0)
        return;
	if ((Key == vkA)&&(Shift.Contains(ssCtrl))) {
		leftSelected = 0;
		rightSelected = length;
		updateInfo();
		drawWaves();
	}
	if (Key==VK_DELETE) {
		for (int i=rightSelected+1; i<=length; i++) {
			lThread[i-rightSelected+leftSelected-1] = lThread[i];
			rThread[i-rightSelected+leftSelected-1] = rThread[i];
		}
		length-=rightSelected-leftSelected+1;
		if (visibleLength>length)
			visibleLength=length;
		rightPeak = leftPeak+visibleLength-1;
		if (rightPeak>=length) {
			rightPeak = length-1;
			leftPeak = rightPeak-visibleLength+1;
		}
		leftSelected = 0;
		rightSelected = 0;

		float Max = Scroll_Max;
		scroll->Width = round((visibleLength*Max)/length);
		scroll->Left = round(((leftPeak+rightPeak)*Max)/2/length) - scroll->Width/2;

		buff = new DWORD[length];
		for (int i=0; i<length; i++)
			buff[i] = lThread[i]+ (rThread[i]<<16);
		BASS_SAMPLE info;
		BASS_SampleGetInfo(sample, &info);
		info.length = length*4;
		BASS_SampleSetInfo(sample, &info);
		BASS_SampleSetData(sample, buff);

		drawWaves();
		updateInfo();
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

		buff = new DWORD[length];
		for (int i=0; i<length; i++)
			buff[i] = lThread[i]+ (rThread[i]<<16);
		BASS_SAMPLE info;
		BASS_SampleGetInfo(sample, &info);
		info.length = length*4;
		BASS_SampleSetInfo(sample, &info);
		BASS_SampleSetData(sample, buff);

		drawWaves();
	}
	if (Key==VK_DOWN) {
        for (int i=leftSelected; i <= rightSelected; i++) {
			lThread[i] *= 0.8;
			rThread[i] *= 0.8;
		}

		buff = new DWORD[length];
		for (int i=0; i<length; i++)
			buff[i] = lThread[i]+ (rThread[i]<<16);
		BASS_SAMPLE info;
		BASS_SampleGetInfo(sample, &info);
		info.length = length*4;
		BASS_SampleSetInfo(sample, &info);
		BASS_SampleSetData(sample, buff);

		drawWaves();
	}
	if (Key==VK_SPACE) {
		if (isPlaying)
			PauseButtonClick(Sender);
		else
			PlayButtonClick(Sender);
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ScrollMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	isScrollerMoving = true;
	prevMoveX = Mouse->CursorPos.X;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::ScrollMouseMove(TObject *Sender, TShiftState Shift, int X,
		  int Y){
	if (!isScrollerMoving)
		return;
#ifdef debug
	//fprintf(logFile, "%d %d\n",X,prevMoveX);
#endif

	scroll->Left += Mouse->CursorPos.X-prevMoveX;
	Application->ProcessMessages();
	if (scroll->Left<0)
		scroll->Left=0;
	if (scroll->Left + scroll->Width > Scroll_Max)
		scroll->Left = Scroll_Max - scroll->Width;
    int range = Scroll_Max - scroll->Width + 1; //диапазон левой границы ползунка
	float L = length - visibleLength; //диапазон, в котором может быть левая граница
	leftPeak = scroll->Left*L/range;
	rightPeak = leftPeak+visibleLength-1;
	if (rightPeak>=length) {
		rightPeak = length-1;
		leftPeak = rightPeak-visibleLength+1;
	}
	prevMoveX = Mouse->CursorPos.X;


	updateInfo();
	drawWaves();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ScrollMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	isScrollerMoving = false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::PlayButtonClick(TObject *Sender){
	if (isPlaying)
		return;
	if (length==0)
		return;
	isPlaying = true;
	BASS_ChannelPlay(channel,cursor==0);
	Timer->Enabled = true;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::TimerTimer(TObject *Sender){
	cursor =BASS_ChannelGetPosition(channel,BASS_POS_BYTE)/4;
	if (leftPeak<=cursor && cursor<=rightPeak) {
		drawWaves();
		if (cursor==length)
			StopButtonClick(Sender);
	}
	updateInfo();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StopButtonClick(TObject *Sender){
	isPlaying = false;
	BASS_ChannelPause(channel);
	BASS_ChannelSetPosition(channel,0,0);
	cursor = 0;

	updateInfo();
    drawWaves();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::PauseButtonClick(TObject *Sender){
	if (!isPlaying)
		return;
	isPlaying = false;
    BASS_ChannelPause(channel);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SaveButtonClick(TObject *Sender){
	if (length==0)
		return;
	if (!SaveDialog->Execute())
		return;

	HSTREAM stream = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, NULL);
	BASS_Encode_MP3_StartFile(stream,NULL,BASS_ENCODE_CAST_NOLIMIT & BASS_ENCODE_AUTOFREE,
								SaveDialog->FileName.c_str());
	BASS_Encode_Write(stream,buff,length*4);
	BASS_Encode_Stop(stream);
}
//---------------------------------------------------------------------------

