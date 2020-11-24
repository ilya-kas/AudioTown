//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "Unit1.h"
#include "math.h"
#include "stdlib.h"
#include "bass.h"
//---------------------------------------------------------------------------
#define debug
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

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
int *lThread;
int *rThread;
int length;        //длина всей песни в отсчётах
int visibleLength; //длина видимой части в отсчётах
int leftPeak;
int rightPeak;
int leftSelected;
int rightSelected;
HSAMPLE sample;
const int MAX_PEAK_VALUE = 32767;
const int MIN_PEAKS = 10; //минимальное количество отсчётов, отображаемых на экране
void drawThread(TPaintBox *paintBox, int peaks[]);
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

void __fastcall TForm1::FormCreate(TObject *Sender){
	lPaintBox = LPaintBox;       //получаем ссылки на рамки для отрисовки каналов
	rPaintBox = RPaintBox;
 	scrollBar = ScrollBar;
	BASS_Init(-1, 44100, 0, Handle, NULL);

    length = 30000;
	lThread = new int[length];
	rThread = new int[length];
	leftPeak = 0;
	rightPeak = length;
	visibleLength = length;
	leftSelected = 0;
	rightSelected = 0;
	scrollBar->PageSize = (visibleLength*scrollBar->Max)/length;

	drawThread(LPaintBox, lThread);     //отрисовка каналов
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

void drawThread(TPaintBox *paintBox, int peaks[]){
	paintBox->Canvas->Pen->Color = clBlack;      //заполняем фон чёрным цветом
	paintBox->Canvas->Brush->Color = clBlack;
	paintBox->Canvas->Rectangle(0,0,paintBox->Width,paintBox->Height);

	float width = paintBox->Width;           //размеры холста
	int height = paintBox->Height;
	int start = height/2;
	float peaksPerPixel = visibleLength/width;

	paintBox->Canvas->Pen->Color = clSilver;      //заполняем фон чёрным цветом
	paintBox->Canvas->Brush->Color = clSilver;
	paintBox->Canvas->Rectangle(peakToX(leftSelected),0,peakToX(rightSelected),paintBox->Height);

	if (visibleLength>0) {
		paintBox->Canvas->Pen->Color = clLime;
		paintBox->Canvas->Pen->Width = 2;
		int sumPos;
		int sumNeg;
		int lBorder = max(leftPeak,leftSelected);
		lBorder = min(lBorder,rightPeak);
		int rBorder = min(rightPeak,rightSelected);
		rBorder = max(rBorder,leftPeak);
		if (peaksPerPixel>1) {
			float p = leftPeak; //номер отсчёта, с которого следующий пиксель
			int hPos;
			int hNeg;
			int count; //отсчётов в данном пикселе
			float next; //номер отсчёта, до которого идёт этот пиксель
			for (int i=0; i < peakToX(lBorder); i++) {
				sumPos = 0;      //разделение на положительные и отрицательные
				sumNeg = 0;
				count = 0;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>0)
						sumPos-=peaks[j];
					if (peaks[j]<0)
						sumNeg-=peaks[j];
					count++;
				}
				hPos = sumPos / count;   //высота "столбиков"
				hNeg = sumNeg / count;   //значения знаков наоборот

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->MoveTo(i,start+hPos);
				paintBox->Canvas->LineTo(i,start+hNeg);
#ifdef debug
 //		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif
				p+=peaksPerPixel;
			}
			paintBox->Canvas->Pen->Color = clGreen;                   //это отрисовка выделенной части
			for (int i=peakToX(lBorder); i < peakToX(rBorder); i++) {
				sumPos = 0;
				sumNeg = 0;
				count = 0;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>0)
						sumPos-=peaks[j];
					if (peaks[j]<0)
						sumNeg-=peaks[j];
					count++;
				}
				hPos = sumPos / count;
				hNeg = sumNeg / count;

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->MoveTo(i,start+hPos);
				paintBox->Canvas->LineTo(i,start+hNeg);
#ifdef debug
//		fprintf(logFile, "%d %f\n",sumPos,(floor(p+peaksPerPixel)-floor(p)));
#endif

				p+=peaksPerPixel;
			}
            paintBox->Canvas->Pen->Color = clLime;
			for (int i=peakToX(rBorder); i <= width; i++) {     //если осталось, то правое лаймовым цветом
				sumPos = 0;
				sumNeg = 0;
				count = 0;
				next = p+peaksPerPixel;
				for (int j = floorf(p); j < next; j++) {
					if (peaks[j]>0)
						sumPos-=peaks[j];
					if (peaks[j]<0)
						sumNeg-=peaks[j];
					count++;
				}
				hPos = sumPos / count;
				hNeg = sumNeg / count;

				hPos = hPos * height/2 / MAX_PEAK_VALUE;
				hNeg = hNeg * height/2 / MAX_PEAK_VALUE;

				paintBox->Canvas->MoveTo(i,start+hPos);
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

	paintBox->Canvas->Pen->Color = clRed;        //красная горизонтальная линия 0 уровня
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
	if (WheelDelta<0 && visibleLength==length) {   //если дальше отдалить нельзя
		return;
	}
#ifdef debug
	//fprintf(logFile, "%d\n",WheelDelta);
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

	float Max = scrollBar->Max;
	scrollBar->PageSize = round((visibleLength*Max)/length);
	scrollBar->Position = round(((leftPeak+rightPeak)*Max)/2/length) - scrollBar->PageSize/2;
	drawThread(LPaintBox, lThread);     //отрисовка каналов
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

bool isSelecting = false;
int selectionStart;

void __fastcall TForm1::RPaintBoxMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	isSelecting = true;
	selectionStart = X;
	leftSelected = XToPeak(X);
	rightSelected = XToPeak(X+1)-1;
	drawThread(LPaintBox, lThread);     //отрисовка каналов
	drawThread(RPaintBox, rThread);
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
		drawThread(LPaintBox, lThread);     //отрисовка каналов
		drawThread(RPaintBox, rThread);
	}
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RPaintBoxMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y){
	isSelecting = false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ScrollBarScroll(TObject *Sender, TScrollCode ScrollCode, int &ScrollPos){
	int range = scrollBar->Max-scrollBar->PageSize+1; //диапазон левой границы ползунка
    int L = length - visibleLength; //диапазон, в котором может быть левая граница
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

	drawThread(LPaintBox, lThread);     //отрисовка каналов
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------


void __fastcall TForm1::SpeedButton1Click(TObject *Sender){
 /*mydecoder := BASS_StreamCreateFile(false,PChar(OpenDialog1.FileName),0,0,BASS_STREAM_DECODE);
 mychannel:=bass_samplegetchannel(mysample,true);*/

	if (!(OpenDialog->Execute()))
		return;

	String *filePath = new String(OpenDialog->FileName);
	sample = BASS_SampleLoad(false,filePath,0,0,1,BASS_STREAM_PRESCAN);
	BASS_SAMPLE* info;
	BASS_SampleGetInfo(sample,info);
	length = info->length/4;
	lThread = new int[length];
	rThread = new int[length];
	leftPeak = 0;
	rightPeak = length;
	visibleLength = length;
	leftSelected = 0;
	rightSelected = 0;

	DWORD* buff = new DWORD[length];
	BASS_SampleGetData(sample, buff);
	for (int i=0; i < length; i++) {
		lThread[i] = LOWORD(buff[i]);
		rThread[i] = HIWORD(buff[i]);
	}
	scrollBar->PageSize = (visibleLength*scrollBar->Max)/length;

	drawThread(LPaintBox, lThread);     //отрисовка каналов
	drawThread(RPaintBox, rThread);
}
//---------------------------------------------------------------------------

