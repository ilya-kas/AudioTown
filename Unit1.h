//---------------------------------------------------------------------------

#ifndef Unit1H
#define Unit1H
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.Buttons.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Dialogs.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
	TSpeedButton *PlayButton;
	TSpeedButton *PauseButton;
	TSpeedButton *StopButton;
	TPanel *LengthPanel;
	TPanel *PeaksPanel;
	TPanel *PLength;
	TPanel *PPeaks;
	TPanel *CursorPanel;
	TPanel *PSelectedLength;
	TPanel *PCursor;
	TPanel *PSelLen;
	TPanel *LeftPanel;
	TPanel *RightPanel;
	TPanel *PLeft;
	TPanel *PRight;
	TPaintBox *LPaintBox;
	TPaintBox *RPaintBox;
	TScrollBar *ScrollBar;
	TSpeedButton *SpeedButton1;
	TSpeedButton *SpeedButton2;
	TOpenDialog *OpenDialog;
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall LPaintBoxPaint(TObject *Sender);
	void __fastcall RPaintBoxPaint(TObject *Sender);
	void __fastcall FormDestroy(TObject *Sender);
	void __fastcall FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta,
		  TPoint &MousePos, bool &Handled);
	void __fastcall RPaintBoxMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y);
	void __fastcall RPaintBoxMouseMove(TObject *Sender, TShiftState Shift, int X, int Y);
	void __fastcall RPaintBoxMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y);
	void __fastcall ScrollBarScroll(TObject *Sender, TScrollCode ScrollCode, int &ScrollPos);
	void __fastcall SpeedButton1Click(TObject *Sender);


private:	// User declarations
public:		// User declarations
	__fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
