//---------------------------------------------------------------------------
#include <windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "WaveAnalysis.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMain *frmMain;

bool bTesting;
bool bGetAudioSetting = false;
DWORD dwStep;
DWORD dwVolumeStep = TEST_END;
bool bWaveOver;
bool bLeftError,bRightError;

WaveAnalysis *WAVE_ANLS = new WaveAnalysis();
DWORD dwErrorCount;
DWORD dwTemp;
bool bLoopBackOut = false;
bool bLoopBackIn = false;
bool bVolumeControl;
bool bSetting = false;

DWORD nowWorkStatus;
//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent* Owner)
	: TForm(Owner)
{
	ReadInISet();
	ScrollBox1->DoubleBuffered = true;
	ScrollBox2->DoubleBuffered = true;
	my_data=new short[buffer_size];

	frmMain->Caption = APP_TITLE;
	Hippo_DEV.In_DevName  = "USB Advanced";
	Hippo_DEV.Out_DevName = "USB Advanced";

	cbOutputMouseEnter(NULL);
	cbInputMouseEnter(NULL);
	DUTScrollBarChange(HippoScrollBar);
	DUTScrollBarChange(DUTScrollBar);

	WAVE_ANLS->ClearFreqWave(FFT_View1);
	WAVE_ANLS->ClearFreqWave(FFT_View2);
	ClearWave(Wave_View1);
	ClearWave(Wave_View2);
	FindHippoDev();
	FindWAVFile();
	CallCommand("control mmsys.cpl,,1");
	bTesting = true;
	PAudio_PARM.HippoVolume	=0;
	PAudio_PARM.DUTVolume	=0;
	nowWorkStatus = STATUS_WAIT_TEST;
}
// 撥放音檔
// 取得設備ID ，mmio API讀取wav檔將音訊資料放置緩衝區，waveout API讀取緩衝區資料並撥出
bool TfrmMain::PlayVoice(int tag,AnsiString AudioFile)
{
	AudioFile = "Audio\\"+AudioFile;
	// MMIO(讀wav檔)
	MMCKINFO	ChunkInfo;
	MMCKINFO	FormatChunkInfo;
	MMCKINFO	DataChunkInfo;

	WAVEFORMATEX   wfx; // 波形音頻流格式的數據結構

	// 選擇設備ID
	int g_selectOutputDevice;
	if(tag ==DUT_TO_HIPPO){
			DUTScrollBarChange(DUTScrollBar);
			g_selectOutputDevice = FindDevWaveOutCount(cbOutput->Text);
	}else if(tag ==HIPPO_TO_DUT){
		DUTScrollBarChange(HippoScrollBar);
		g_selectOutputDevice = FindDevWaveOutCount(Hippo_DEV.Out_DevName);
	}
	int DataSize;
	// Zero out the ChunkInfo structure.
	memset(&ChunkInfo, 0, sizeof(MMCKINFO));
	// 打開WAVE文件，返回一個HMMIO句柄

	HMMIO handle = mmioOpen(AudioFile.c_str(), 0, MMIO_READ);
	// 一個WAVE文件最少包含三個區塊，RIFF為其中最大，整個WAVE文件就是一個RIFF區塊
	// 進入RIFF區塊(RIFF Chunk)
	mmioDescend(handle, &ChunkInfo, 0, MMIO_FINDRIFF);

	// 進入fmt區塊(RIFF子塊，包含音訊結構的信息)
	FormatChunkInfo.ckid = mmioStringToFOURCC("fmt", 0); // 尋找fmt子塊
	mmioDescend(handle, &FormatChunkInfo, &ChunkInfo, MMIO_FINDCHUNK);
	// 進入fmt子塊
	// 讀取wav結構信息
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	mmioRead(handle, (char*) & wfx, FormatChunkInfo.cksize);
	// 跳出fmt區塊
	mmioAscend(handle, &FormatChunkInfo, 0);
	// 進入data區塊(包含所有的數據波形)
	DataChunkInfo.ckid = mmioStringToFOURCC("data", 0);
	mmioDescend(handle, &DataChunkInfo, &ChunkInfo, MMIO_FINDCHUNK);
	// 獲得data區塊的大小
	DataSize = DataChunkInfo.cksize;
	// 分配緩衝區
	WaveOutData = new char[DataSize];
	mmioRead(handle, WaveOutData, DataSize);
	// 打開輸出設備
	if(waveOutOpen(&WaveOut, g_selectOutputDevice, &wfx, 0, 0, WAVE_FORMAT_QUERY)== MMSYSERR_NOERROR)
	{
		// 判斷格式
		waveOutOpen(&WaveOut, g_selectOutputDevice, &wfx, NULL, NULL,
			CALLBACK_NULL);

		// 設置wave header.
		memset(&WaveOutHeader, 0, sizeof(WaveOutHeader));
		WaveOutHeader.lpData = WaveOutData;
		WaveOutHeader.dwBufferLength = DataSize;
		// 準備wave header.
		waveOutPrepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
		// 將緩衝區資料寫入撥放設備(開始撥放).
		waveOutWrite(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
		waveOutSetVolume(WaveOut, PAudio_PARM.AudioVolume.ToInt());
		Delay(200);
		// 開始錄音
		if(!StartRecording(tag))
		{
			// 停止播放並重置管理器
			waveOutReset(WaveOut);
			// 關閉撥放設備
			waveOutClose(WaveOut);
			// 清理用WaveOutPrepareHeader準備的Wave。
			waveOutUnprepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
			// 釋放內存
			if (WaveOutData != NULL) {
				delete[]WaveOutData;
				WaveOutData = NULL;
			}
			plDUT->Caption = "(!)WaveOutDeivce Fail";
			plDUT->Font->Color = clRed;
			return false;
		}
		return true;
	}
	plDUT->Caption = "(!)WaveOutDevice Fail";
	plDUT->Font->Color = clRed;
	return false;
}
bool  TfrmMain::StartRecording(int tag) {
	// 取得設備ID
	int g_selectinputDevice;
	if(tag == DUT_TO_HIPPO){
			g_selectinputDevice = FindDevWaveInCount(Hippo_DEV.In_DevName);
	}else if(tag == HIPPO_TO_DUT){
			g_selectinputDevice = FindDevWaveInCount(cbInput->Text);
	}
	waveFormat.wFormatTag=WAVE_FORMAT_PCM;  // 格式
	waveFormat.nChannels=2;  // 聲道數量（即單聲道，立體聲...）
	waveFormat.nSamplesPerSec=44100;  // 每秒採樣次數(採樣率)
	waveFormat.wBitsPerSample=16;     // 每次採樣Bit(採樣精度)
	waveFormat.nBlockAlign=waveFormat.wBitsPerSample / 8 * waveFormat.nChannels; // 數據阻塞大小
	waveFormat.nAvgBytesPerSec=waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;  // 緩衝區估計
	waveFormat.cbSize=0;//PCM格式 此值忽略

	//  Audio buffers
	WaveBuffers=new WAVEHDR[4];
	//2048 * 每秒採樣byte * 聲道數量
	int WaveBufSize=buffer_size * waveFormat.wBitsPerSample / 8 * waveFormat.nChannels;
	// waveInOpen開啟輸入設備
	if((waveInOpen(&hWaveIn, g_selectinputDevice, &waveFormat, (DWORD) this->Handle, (DWORD) this, CALLBACK_WINDOW)) == MMSYSERR_NOERROR)
	{
		//VirtualAlloc 申請虛擬內存空間
		//lpData: 緩衝區地址
		for (int i = 0; i < 4; i++) 
		{
			WaveBuffers[i].lpData= (char *)VirtualAlloc(0, WaveBufSize, MEM_COMMIT, PAGE_READWRITE);
			WaveBuffers[i].dwBufferLength=WaveBufSize; // 緩衝區長度
			WaveBuffers[i].dwFlags=0; // 緩衝區屬性
			WaveBuffers[i].dwLoops=0; // 播放循環的次數
			// 準備一個波形輸入緩衝區。一旦初始化時，此功能可讓音頻驅動程式和作業系統處理標頭檔或緩衝區。
			if((waveInPrepareHeader(hWaveIn, &WaveBuffers[i], sizeof(WAVEHDR))) == MMSYSERR_NOERROR)
			{   // 發送到指定的波形的音頻輸入裝置的輸入緩衝區。當緩衝區被填滿後，通知應用程式。
				if((waveInAddBuffer(hWaveIn, &WaveBuffers[i], sizeof(WAVEHDR))) == MMSYSERR_NOERROR)
				{    
					if(i==3)
					{        
						// 開始錄音
						PAudio_PARM.dwBUFFER 		= 0;
						PAudio_PARM.dwNumberOfTimes = 0;
						PAudio_PARM.bWaveInOver		= false;
						PAudio_PARM.dwTimeOut		= GetTickCount()+PAudio_PARM.dwSetTimeOut;
						PAudio_PARM.bError 			= false;
						if((waveInStart(hWaveIn)) == MMSYSERR_NOERROR)
						{
							while(!bWaveOver)
								Delay(10);
							return true;
						}
                    }

				}
				else break;
			}
			else break;
		}
	}
	//
	::waveInStop(hWaveIn);
	::waveInReset(hWaveIn);
	::waveInClose(hWaveIn);
	::waveInUnprepareHeader(hWaveIn, &WaveBuffers[0], sizeof(WAVEHDR));//釋放緩衝區空間
	VirtualFree(WaveBuffers[0].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[1].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[3].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[4].lpData, 0, MEM_RELEASE);
	delete[] WaveBuffers;
	//
	return false;
}
//---------------------------------------------------------------------------
// 接收系統消息
void __fastcall TfrmMain::WndProc(TMessage &Message)
{   // 接收聲音消息到緩衝區滿時
	if(Message.Msg == MM_WIM_DATA)
	{
		if(!PAudio_PARM.bWaveInOver)
		{
			DEBUG("Buffer["+AnsiString(PAudio_PARM.dwBUFFER)+"]Full");
			TImage * Wave_View = (TImage *)frmMain->FindComponent("Wave_View"+AnsiString(PAudio_PARM.WaveNum));
			TImage * FFT_View  = (TImage *)frmMain->FindComponent("FFT_View"+AnsiString(PAudio_PARM.WaveNum));
			Oscilloscopes((short *)( WaveBuffers[PAudio_PARM.dwBUFFER].lpData),Wave_View,2);
			WAVE_ANLS->FillBuffer((short *)( WaveBuffers[PAudio_PARM.dwBUFFER].lpData),FFT_View,PAudio_PARM.dwFFT_Vertex);
			if(PAudio_PARM.dwBUFFER==3)
			{
				waveInAddBuffer(hWaveIn,&WaveBuffers[0], sizeof(WAVEHDR));
				PAudio_PARM.dwBUFFER = 0;
			}
			else
			{
				waveInAddBuffer(hWaveIn,&WaveBuffers[PAudio_PARM.dwBUFFER+1], sizeof(WAVEHDR));
				PAudio_PARM.dwBUFFER++;
			}

			if(GetTickCount()>frmMain->PAudio_PARM.dwTimeOut                   //錄音時間到
				|| PAudio_PARM.dwNumberOfTimes+1 == PAudio_PARM.dwTotalFrames    //取到幀數已到
				|| PAudio_PARM.bError)                                         //頻率波形不對
			{
				PAudio_PARM.bWaveInOver		= true;
				DEBUG("Stop Playing");
				// 停止播放並重置管理器
				waveOutReset(WaveOut);
				// 關閉撥放設備
				waveOutClose(WaveOut);
				// 清理用WaveOutPrepareHeader準備的Wave。
				waveOutUnprepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
				// 釋放內存
				if (WaveOutData != NULL) {
					delete[]WaveOutData;
					WaveOutData = NULL;
				}
			}
			this->ProcessInput();
		}
	}
	TForm :: WndProc(Message);    //��
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::ProcessInput(void)
{
	//  Update views
	TComboBox *ComboBox;
	if(PAudio_PARM.WaveNum == 1)
		ComboBox  = (TComboBox *)frmMain->FindComponent("cbOutputWAV");
	else ComboBox  = (TComboBox *)frmMain->FindComponent("cbInputWAV");

	if(PAudio_PARM.dwNumberOfTimes>3 && !PAudio_PARM.bError)
	{
		//判斷
		dwTemp = ComboBox->Text.SubString(2,ComboBox->Text.Pos("R")-2).ToInt();
		if(WAVE_ANLS->GetWaveFreq(true,dwTemp) != WAVE_FREQ_PASS
			|| CrestVerify(true,dwTemp) != WAVE_CREST_NUMBER_PASS)
			PAudio_PARM.bError = true;
		dwTemp = ComboBox->Text.SubString(ComboBox->Text.Pos("R")+1,ComboBox->Text.Pos("S")-ComboBox->Text.Pos("R")-1).ToInt();
		if(WAVE_ANLS->GetWaveFreq(false,dwTemp) != WAVE_FREQ_PASS
			|| CrestVerify(false,dwTemp) != WAVE_CREST_NUMBER_PASS)
			PAudio_PARM.bError = true;
	}
	PAudio_PARM.dwNumberOfTimes++;
	if(PAudio_PARM.bWaveInOver)
	{
		::waveInStop(hWaveIn);
		::waveInReset(hWaveIn);
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[0], sizeof(WAVEHDR));//釋放緩衝區空間
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[1], sizeof(WAVEHDR));//釋放緩衝區空間
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[2], sizeof(WAVEHDR));//釋放緩衝區空間
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[3], sizeof(WAVEHDR));//釋放緩衝區空間
		::waveInClose(hWaveIn);
		VirtualFree(WaveBuffers[0].lpData, 0, MEM_RELEASE);
		VirtualFree(WaveBuffers[1].lpData, 0, MEM_RELEASE);
		VirtualFree(WaveBuffers[2].lpData, 0, MEM_RELEASE);
		VirtualFree(WaveBuffers[3].lpData, 0, MEM_RELEASE);
		delete[] WaveBuffers;
		DEBUG("Stop Recording");
		bWaveOver = true;
	}
}
//---------------------------------------------------------------------------
void TfrmMain::Oscilloscopes(short *idata,TImage* Wave_View,DWORD dwMultiple)
{

	int count=buffer_size;
	int iNum = Wave_View->Tag;
	memcpy(my_data, idata, count * sizeof(short));
	float
	long    leftzerolevel=(Wave_View->Height / 2 - 10) / 2;   //左聲道 X軸位置
	long    rightzerolevel=Wave_View->Height / 2 + 10 + leftzerolevel;//右聲道 X軸位置
	double  timescale=leftzerolevel / 32768.0;//16位元  -32768 ~ +32767
	Wave_View->Canvas->Lock();
	Wave_View->Canvas->Brush->Color=clBlack;
	Wave_View->Canvas->Pen->Color=clWhite;
	Wave_View->Canvas->Rectangle(0, 0, Wave_View->Width, Wave_View->Height);
	Wave_View->Canvas->Pen->Color=clYellow;
	Wave_View->Canvas->MoveTo(0, leftzerolevel);
	Wave_View->Canvas->LineTo(Wave_View->Width, leftzerolevel);
	Wave_View->Canvas->MoveTo(0, rightzerolevel);
	Wave_View->Canvas->LineTo(Wave_View->Width, rightzerolevel);
	Wave_View->Canvas->Pen->Color=clAqua;
	double  ts=static_cast<double>(Wave_View->Width) / static_cast<double>(count); // 每一點相距的距離
    int     x;
	Wave_View->Canvas->MoveTo(0, leftzerolevel);
	float y_vertex = 0;
	bool bCrest = false;
	PAudio_PARM.dwLCrest = 0;
	Wave_View->Refresh();
	//Memo3->Lines->Add("---左聲道---");
	for(DWORD i=0; i < (count/dwMultiple) - 1; i+=2)
	{
		x=i * ts*dwMultiple;
		int y=leftzerolevel - static_cast<double>(my_data[i]) * timescale;
		Wave_View->Canvas->LineTo(x, y);
		if(static_cast<double>(my_data[i])>y_vertex) y_vertex = static_cast<double>(my_data[i]);
		//計算波峰數
		if(static_cast<double>(my_data[i])> (PAudio_PARM.dwWAVE_LL_Vertex-1000))
		{
			if(!bCrest)
			{
				bCrest = true;
				PAudio_PARM.dwLCrest++;
			}
		}
		else
		{
			if(bCrest)
			{
				bCrest = false;
			}
		}
	}
	AnsiString asstrSS = y_vertex*0.02583287 *0.707;
	asstrSS.printf("%.3f",asstrSS.ToDouble());
	if(iNum == 1)
	{
		plNumCrestL1->Caption = PAudio_PARM.dwLCrest;
		plLVertex1->Caption = y_vertex;
		plNumVrmsL1->Caption  = asstrSS;
	}
	else
	{
		plNumCrestL2->Caption = PAudio_PARM.dwLCrest;
		plLVertex2->Caption = y_vertex;
		plNumVrmsL2->Caption  = asstrSS;
	}

	Wave_View->Canvas->MoveTo(0, rightzerolevel);
	//Memo3->Lines->Add("---右聲道---");
	Wave_View->Canvas->Pen->Color=clRed;
	y_vertex = 0;
	bCrest = false;
	PAudio_PARM.dwRCrest = 0;
	for(DWORD i=1; i < (count/dwMultiple); i+=2)
    {
		x=i * ts*dwMultiple;
		int y=rightzerolevel - static_cast<double>(my_data[i]) * timescale;
		Wave_View->Canvas->LineTo(x, y);
		if(static_cast<double>(my_data[i])>y_vertex) y_vertex = static_cast<double>(my_data[i]);
		//計算波峰數
		if(static_cast<double>(my_data[i])> PAudio_PARM.dwWAVE_LL_Vertex-1000)
		{
			if(!bCrest)
			{
				bCrest = true;
				PAudio_PARM.dwRCrest++;
			}
		}
		else
		{
			if(bCrest)
			{
				bCrest = false;
			}
		}
	}
	asstrSS = y_vertex*0.02583287 *0.707;
	asstrSS.printf("%.3f",asstrSS.ToDouble());
	if(iNum == 1)
	{
		plNumCrestR1->Caption = PAudio_PARM.dwRCrest;
		plRVertex1->Caption = y_vertex;
		plNumVrmsR1->Caption  = asstrSS;
	}
	else
	{
		plNumCrestR2->Caption = PAudio_PARM.dwRCrest;
		plRVertex2->Caption = y_vertex;
		plNumVrmsR2->Caption  = asstrSS;
	}
	Wave_View->Refresh();
	Wave_View->Canvas->Unlock();
}
//------------------------------------------------------------------------------
void TfrmMain::Delay(ULONG iMilliSeconds) // 原版delay time 用在thread裡面
{
	ULONG iStart;
	iStart = GetTickCount();
	while (GetTickCount() - iStart <= iMilliSeconds)
		Application->ProcessMessages();
}
//------------------------------------------------------------------------------
void __fastcall TfrmMain::Timer1Timer(TObject *Sender)
{

	if(!bTesting)
	{
		bTesting = true;
		switch(dwStep)
		{
			case WAIT_DEV_PLUGIN:
				if(FindHippoDev()
					&& FindDevWaveOutCount(cbOutput->Text) != NOT_FIND_DEV_AUDIO
					&& FindDevWaveInCount(cbInput->Text) != NOT_FIND_DEV_AUDIO)
					{
						nowWorkStatus = STATUS_TESTING;
						UIEnabled(false);
						dwErrorCount = 2;
						WAVE_ANLS->ClearFreqWave(FFT_View1);
						WAVE_ANLS->ClearFreqWave(FFT_View2);
						ClearWave(Wave_View1);
						ClearWave(Wave_View2);
						plDUT->Caption = "Testing...";
						plDUT->Font->Color = clBlue;
						ShowResult("Testing...",clCream);
						bLeftError  = false;
						bRightError = false;
						plRVertex1->Color = clCream;
						plLVertex1->Color = clCream;
						plRVertex2->Color = clCream;
						plLVertex2->Color = clCream;
						frmMain->DosCommand("Taskkill /im Video.UI.exe /F");
						frmMain->DosCommand("Taskkill /im wmplayer.exe /F");
						PAudio_PARM.dwTotalFrames   = 4;
						if(PAudio_PARM.bAutoVolume)
						{
							if(!bVolumeControl)
								dwStep = TEST_AUDIO;
							else dwStep = CORRECT_VOLUME;
						}
						else dwStep = TEST_AUDIO;
						Delay(PAudio_PARM.dwDelay);
					}
				break;
			case CORRECT_VOLUME:
					DEBUG("Adjust The Volume");
					if(VolumeCorrection())
					{
						bLeftError  = false;
						bRightError = false;
						dwStep = TEST_AUDIO;
						PAudio_PARM.dwTotalFrames   = -1;
					}
					else
					{
						if(plDUT->Font->Color != clRed)
						{
							plDUT->Caption = "音量異常(!)Volume Fail";
							plDUT->Font->Color = clRed;
						}
						dwStep = SHOW_RESULT;
					}
				break;
			case TEST_AUDIO:
					PAudio_PARM.WaveNum = 1;
					bWaveOver = false;
					if(PlayVoice(DUT_TO_HIPPO,cbOutputWAV->Text))
						dwStep = WAVE_ANALYSIS;
					else dwStep = SHOW_RESULT;
				break;
			case WAVE_ANALYSIS:
					if(bWaveOver)
					{
						dwTemp = cbOutputWAV->Text.SubString(2,cbOutputWAV->Text.Pos("R")-2).ToInt();
						if(WAVE_ANLS->GetWaveFreq(true,dwTemp) != WAVE_FREQ_PASS
							|| CrestVerify(true,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							plDUT->Caption = "撥放波型異常(!)Playback failed";
							plDUT->Font->Color = clRed;
							bLeftError = true;
						}
						dwTemp = cbOutputWAV->Text.SubString(cbOutputWAV->Text.Pos("R")+1,cbOutputWAV->Text.Pos("S")-cbOutputWAV->Text.Pos("R")-1).ToInt();
						if(WAVE_ANLS->GetWaveFreq(false,dwTemp) != WAVE_FREQ_PASS
							|| CrestVerify(false,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							plDUT->Caption = "撥放波型異常(!)Playback failed";
							plDUT->Font->Color = clRed;
							bLeftError = true;
						}
						if(abs(plRVertex1->Caption.ToDouble()-plLVertex1->Caption.ToDouble())/ (plRVertex1->Caption.ToDouble()+plLVertex1->Caption.ToDouble())
							> PAudio_PARM.dbBalance)
						{
							moDebug->Lines->Add("聲道誤差:"+AnsiString(abs(plRVertex1->Caption.ToDouble()-plLVertex1->Caption.ToDouble())/ (plRVertex1->Caption.ToDouble()+plLVertex1->Caption.ToDouble())));
							plRVertex1->Color = clRed;
							plLVertex1->Color = clRed;
							plDUT->Caption = "撥放波型異常(!)Playback failed";
							plDUT->Font->Color = clRed;
							bLeftError = true;
						}
						if(bLeftError && dwErrorCount<2)
						{
							bLeftError = false;
							WAVE_ANLS->ClearFreqWave(FFT_View1);
							ClearWave(Wave_View1);
							plDUT->Caption = "Testing...";
							plDUT->Font->Color = clBlue;
							dwErrorCount++;
							dwStep = TEST_AUDIO;
						}
						else
						{
							dwErrorCount = 2;
							dwStep = TEST_AUDIO_2;
						}
					}
				break;
			case TEST_AUDIO_2:
					bWaveOver = false;
					PAudio_PARM.WaveNum = 2;
					if(PlayVoice(HIPPO_TO_DUT,cbInputWAV->Text))
						dwStep = WAVE_ANALYSIS_2;
					else dwStep = SHOW_RESULT;
				break;
			case WAVE_ANALYSIS_2:
					if(bWaveOver)
					{
						dwTemp = cbInputWAV->Text.SubString(2,cbInputWAV->Text.Pos("R")-2).ToInt();
						if(WAVE_ANLS->GetWaveFreq(true,dwTemp) != WAVE_FREQ_PASS
							 || CrestVerify(true,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							if(	plDUT->Caption.Pos("failed"))
								plDUT->Caption = "波型異常(!)All Waveform failed";
							else plDUT->Caption = "錄音波型異常(!)Recording failed";
							plDUT->Font->Color = clRed;
							bRightError = true;
						}
						dwTemp = cbInputWAV->Text.SubString(cbInputWAV->Text.Pos("R")+1,cbInputWAV->Text.Pos("S")-cbInputWAV->Text.Pos("R")-1).ToInt();
						if(WAVE_ANLS->GetWaveFreq(false,dwTemp) != WAVE_FREQ_PASS
							|| CrestVerify(false,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							if(	plDUT->Caption.Pos("failed"))
								plDUT->Caption = "波型異常(!)All Waveform failed";
							else plDUT->Caption = "錄音波型異常(!)Recording failed";
							plDUT->Font->Color = clRed;
							bRightError = true;
						}
						if(abs(plRVertex2->Caption.ToDouble()-plLVertex2->Caption.ToDouble())/ (plRVertex2->Caption.ToDouble()+plLVertex2->Caption.ToDouble())
							> PAudio_PARM.dbBalance)
						{
							moDebug->Lines->Add("聲道誤差:"+AnsiString(abs(plRVertex2->Caption.ToDouble()-plLVertex2->Caption.ToDouble())/ (plRVertex2->Caption.ToDouble()+plLVertex2->Caption.ToDouble())));
							plRVertex2->Color = clRed;
							plLVertex2->Color = clRed;
							if(	plDUT->Caption.Pos("failed"))
								plDUT->Caption = "波型異常(!)All Waveform failed";
							else plDUT->Caption = "錄音波型異常(!)Recording failed";
							plDUT->Font->Color = clRed;
							bRightError = true;
						}
						if(bRightError && dwErrorCount<2)
						{
							bRightError = false;
							WAVE_ANLS->ClearFreqWave(FFT_View2);
							ClearWave(Wave_View2);
							plDUT->Caption = "Testing...";
							plDUT->Font->Color = clBlue;
							dwErrorCount++;
							dwStep = TEST_AUDIO_2;
						}
						else
							dwStep = SHOW_RESULT;
					}
				break;
			case SHOW_RESULT:
					if(plDUT->Font->Color == clRed)
					{
						ShowResult("FAIL",clRed);
                    }
					else if(!bLeftError && !bRightError)
					{
						//判斷音量是否符合設定值
						if(PAudio_PARM.HippoVolume==0)
							PAudio_PARM.HippoVolume = HippoScrollBar->Position;
						if(PAudio_PARM.DUTVolume==0)
							PAudio_PARM.DUTVolume = DUTScrollBar->Position;
						if(PAudio_PARM.DUTVolume+PAudio_PARM.VolumeRange>=DUTScrollBar->Position
							&&PAudio_PARM.DUTVolume-PAudio_PARM.VolumeRange<=DUTScrollBar->Position
							&&PAudio_PARM.HippoVolume+PAudio_PARM.VolumeRange>=HippoScrollBar->Position
							&&PAudio_PARM.HippoVolume-PAudio_PARM.VolumeRange<=HippoScrollBar->Position)
						{
							plDUT->Caption = "The Test has finished";
							plDUTVolume->Font->Color = clBlue;
							plHippoVolume->Font->Color = clBlue;
							ShowResult("PASS",clLime);
						}
						else
						{
							plDUT->Caption = "音量異常(!)Volume Fail";
							plDUT->Font->Color = clRed;
							ShowResult("FAIL",clRed);
                        }
					}
					else
					{
						ShowResult("FAIL",clRed);
					}
					UIEnabled(true);
					dwStep = WAIT_DEV_PLUGOUT;
					bVolumeControl = true;
					cbOutput->Enabled = false;
					cbInput->Enabled = false;

					if(bSetting && plResult->Color == clRed)
					{
						btnResetDUTClick(NULL);
					}
					bSetting = false;
					nowWorkStatus = STATUS_TEST_OVER;
				break;
			case WAIT_DEV_PLUGOUT:
					if(FindDevWaveOutCount(cbOutput->Text) == NOT_FIND_DEV_AUDIO
					|| FindDevWaveInCount(cbInput->Text) == NOT_FIND_DEV_AUDIO)
					{
						nowWorkStatus = STATUS_WAIT_TEST;
						plRVertex1->Color = clCream;
						plLVertex1->Color = clCream;
						plRVertex2->Color = clCream;
						plLVertex2->Color = clCream;
						ShowResult("WAIT",clCream);
						plDUT->Caption = "Not Find Device...";
						plDUT->Font->Color = clBlue;
						dwStep = WAIT_DEV_PLUGIN;
					}
				break;
		}
		if(btnDUT->Width != 147)
			bTesting = false;
	}
}
//---------------------------------------------------------------------------
bool TfrmMain::CallCommand(AnsiString Command_line) { // 下Command指令
	UINT Result;
	DWORD dwExitCode;
	STARTUPINFOA StartupInfo = {
		0
	};
	PROCESS_INFORMATION ProcessInfo;
	StartupInfo.cb = sizeof(STARTUPINFO);
	GetStartupInfoA(&StartupInfo);
	StartupInfo.wShowWindow = SW_HIDE;
	AnsiString cmd = getenv("COMSPEC");
	AnsiString Command = cmd + " /c " + Command_line;
	Result = CreateProcessA(NULL, Command.c_str(), NULL, NULL, false, 0, NULL,
		NULL, &StartupInfo, &ProcessInfo);
	if (Result) {
		CloseHandle(ProcessInfo.hThread);
		if (WaitForSingleObject(ProcessInfo.hProcess, INFINITE) != WAIT_FAILED)
		{
			GetExitCodeProcess(ProcessInfo.hProcess, &dwExitCode);
		}
		CloseHandle(ProcessInfo.hProcess);
		return true;
	}
	return false;
}
void TfrmMain::ClearWave(TImage* Wave_View)
{
	long    leftzerolevel=(Wave_View->Height / 2 - 10) / 2;   //左聲道 X軸位置
	long    rightzerolevel=Wave_View->Height / 2 + 10 + leftzerolevel;//右聲道 X軸位置
	Wave_View->Canvas->Lock();
	Wave_View->Canvas->Brush->Color=clBlack;
	Wave_View->Canvas->Pen->Color=clWhite;
	Wave_View->Canvas->Rectangle(0, 0, Wave_View->Width, Wave_View->Height);
	Wave_View->Canvas->Pen->Color=clYellow;
	Wave_View->Canvas->MoveTo(0, leftzerolevel);
	Wave_View->Canvas->LineTo(Wave_View->Width, leftzerolevel);
	Wave_View->Canvas->MoveTo(0, rightzerolevel);
	Wave_View->Canvas->LineTo(Wave_View->Width, rightzerolevel);
	Wave_View->Canvas->Pen->Color=clAqua;
}
bool TfrmMain::FindHippoDev()
{
//取得治具
	for (DWORD InputCount = 0; InputCount < waveInGetNumDevs(); ++InputCount)
	{
		if (!waveInGetDevCaps(InputCount, &Wave_input, sizeof(Wave_input))&&strstr(Wave_input.szPname,Hippo_DEV.In_DevName.c_str()))
		{
			for (DWORD OutputCount = 0; OutputCount < waveOutGetNumDevs(); ++OutputCount)
			{
				if (!waveOutGetDevCaps(OutputCount, &Wave_output, sizeof(Wave_output))&&strstr(Wave_output.szPname,Hippo_DEV.Out_DevName.c_str()))
				{
					Hippo_DEV.In_DevName = AnsiString(Wave_input.szPname);
					Hippo_DEV.Out_DevName = AnsiString(Wave_output.szPname);
					plHippo->Caption 	= AnsiString(Wave_output.szPname)+"、"+Hippo_DEV.In_DevName;
					plHippo->Font->Color = clBlue;
					return true;
				}
			}
		}
	}
	plHippo->Caption 	= "Not Find HippoDevice...";
	plHippo->Font->Color = clRed;
	return false;
}
int TfrmMain::FindDevWaveOutCount(AnsiString DevName)
{
	for (DWORD OutputCount = 0; OutputCount < waveOutGetNumDevs(); ++OutputCount)
	{
		if (waveOutGetDevCaps(OutputCount, &Wave_output, sizeof(Wave_output)) == 0)
		{
			if(AnsiString(Wave_output.szPname).Pos(DevName))
			{
				if(AnsiString(Wave_output.szPname).Pos("USB Advanced"))
				{
					if(AnsiString(Wave_output.szPname) == Hippo_DEV.Out_DevName)
					{
						if(bLoopBackOut)
							return	OutputCount;
						else if(DevName == AnsiString(Wave_output.szPname))
							return	OutputCount;
					}
					else return	OutputCount;
				}
				else
					return	OutputCount;
			}
		}
	}
	return NOT_FIND_DEV_AUDIO;
}
int TfrmMain::FindDevWaveInCount(AnsiString DevName)
{
	for (DWORD InputCount = 0; InputCount < waveInGetNumDevs(); ++InputCount)
	{
		if (waveInGetDevCaps(InputCount, &Wave_input, sizeof(Wave_input)) == 0)
		{
			if(AnsiString(Wave_input.szPname).Pos(DevName))
			{
				if(AnsiString(Wave_input.szPname).Pos("USB Advanced"))
				{
					if(AnsiString(Wave_input.szPname) == Hippo_DEV.In_DevName)
					{
						if(bLoopBackIn)
							return	InputCount;
						else if(DevName == AnsiString(Wave_input.szPname))
							return	InputCount;
					}
					else
						return InputCount;
				}
				else
					return	InputCount;
			}
		}
	}
	return -1;
}
void __fastcall TfrmMain::btnDUTClick(TObject *Sender)
{
	AnsiString SS;
	btnDUT->Width = 0;
	cbOutput->Width = plFrmCbOutIn->Width /2 ;
	cbInput->Width = plFrmCbOutIn->Width /2 ;
	bLoopBackOut = false;
	bLoopBackIn = false;
	if(cbOutput->Text.Pos("#"))
	{
		SS = cbOutput->Text;
		cbOutput->Text = SS.SubString(1,SS.Pos("#")-1).Trim();
	}
	if(cbInput->Text.Pos("#"))
	{
		SS = cbInput->Text;
		cbInput->Text = SS.SubString(1,SS.Pos("#")-1).Trim();
	}
	if(cbOutput->Text != Hippo_DEV.Out_DevName)
	{
		if(cbOutput->Text.Pos("-"))
		{
			SS = cbOutput->Text;
			SS = SS.SubString(SS.Pos("-")+1,SS.Length()).Trim();
			cbOutput->Clear();
			cbOutput->Items->Add(SS);
			cbOutput->ItemIndex = 0;
		}
	}
	else bLoopBackOut = true;

	if(cbInput->Text != Hippo_DEV.In_DevName)
	{
		if(cbInput->Text.Pos("-"))
		{
			SS = cbInput->Text;
			SS = SS.SubString(SS.Pos("-")+1,SS.Length()).Trim();
			cbInput->Clear();
			cbInput->Items->Add(SS);
			cbInput->ItemIndex = 0;
		}
	}
	else bLoopBackIn = true;

	dwStep = WAIT_DEV_PLUGIN;
	cbOutput->Enabled = false;
	cbInput->Enabled = false;
	rb_TwoB->Enabled = false;
	rb_One->Enabled = false;
	bVolumeControl = true;
	bTesting = false;
	bSetting = true;
}
void __fastcall TfrmMain::cbOutputMouseEnter(TObject *Sender)
{
	if(!bGetAudioSetting)
	{
		bGetAudioSetting = true;
		int iCount = cbOutput->ItemIndex;
		if(iCount==-1) iCount = 0;
		cbOutput->Clear();
		for (DWORD nC1 = 0; nC1 < waveOutGetNumDevs(); ++nC1) {
			if (waveOutGetDevCaps(nC1, &Wave_output, sizeof(Wave_output)) == 0) {
				cbOutput->Items->Add(Wave_output.szPname);
			}
		}
		cbOutput->ItemIndex = iCount;
		bGetAudioSetting = false;
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::cbInputMouseEnter(TObject *Sender)
{
	if(!bGetAudioSetting)
	{
		bGetAudioSetting = true;
		int iCount = cbInput->ItemIndex;
		if(iCount==-1) iCount = 0;
		cbInput->Clear();
		for (DWORD nC1 = 0; nC1 < waveInGetNumDevs(); ++nC1) {
			if (waveInGetDevCaps(nC1, &Wave_input, sizeof(Wave_input)) == 0) {
				cbInput->Items->Add(Wave_input.szPname);
			}
		}
		cbInput->ItemIndex = iCount;
		bGetAudioSetting = false;
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::DUTScrollBarChange(TObject *Sender)
{
	TScrollBar* ScrollBar = (TScrollBar *)Sender;
	if(ScrollBar->Name.Pos("DUT"))
	{
		plDUTVolume->Caption = "VolumeDUT:"+AnsiString(DUTScrollBar->Position)+"%";
		int fNum = (float)DUTScrollBar->Position *655.35;
		AnsiString SS = IntToHex(fNum,4);
		PAudio_PARM.AudioVolume = "0x"+SS+SS;
		PAudio_PARM.dwAudioVolumeDUT = DUTScrollBar->Position;
		if(PAudio_PARM.DUTVolume != 0)
		{
			if(DUTScrollBar->Position<PAudio_PARM.DUTVolume+PAudio_PARM.VolumeRange && DUTScrollBar->Position>PAudio_PARM.DUTVolume-PAudio_PARM.VolumeRange)
				plDUTVolume->Font->Color = clBlue;
			else plDUTVolume->Font->Color = clRed;
		}
		else plDUTVolume->Font->Color = clWindowText;
	}
	else
	{
		plHippoVolume->Caption = "VolumeHippo:"+AnsiString(HippoScrollBar->Position)+"%";
		int fNum = (float)HippoScrollBar->Position *655.35;
		AnsiString SS = IntToHex(fNum,4);
		PAudio_PARM.AudioVolume = "0x"+SS+SS;
		PAudio_PARM.dwAudioVolumeHippo = HippoScrollBar->Position;
		if(PAudio_PARM.HippoVolume != 0)
		{
			if(HippoScrollBar->Position<PAudio_PARM.HippoVolume+PAudio_PARM.VolumeRange && HippoScrollBar->Position>PAudio_PARM.HippoVolume-PAudio_PARM.VolumeRange)
				plHippoVolume->Font->Color = clBlue;
			else plHippoVolume->Font->Color = clRed;
		}
		else plHippoVolume->Font->Color = clWindowText;
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::btnTestClick(TObject *Sender)
{
	bVolumeControl = false;
	dwStep = WAIT_DEV_PLUGIN;
}
//---------------------------------------------------------------------------
void TfrmMain::FindWAVFile()
{
	TSearchRec Sr;
	TStringList*FileList=new TStringList();
	cbOutputWAV->Items->Clear();
	cbInputWAV->Items->Clear();
	PAudio_PARM.bStereo = Findfilemsg("Audio\\Config.ini","[STEREO_TEST]",1)== "true"?true:false;
	if(FindFirst("Audio\\*.wav",faAnyFile,Sr)==0)
	{
		do
		{
			FileList->Add(Sr.Name);
			cbOutputWAV->Items->Add(Sr.Name);
			cbInputWAV->Items->Add(Sr.Name);
		}
		while(FindNext(Sr)==0);
		FindClose(Sr);
	}
	if(PAudio_PARM.bStereo)
		rb_TwoBClick(rb_TwoB);
	else
		rb_TwoBClick(rb_One);
	delete FileList;
}
AnsiString TfrmMain::Findfilemsg(AnsiString filename, AnsiString findmsg,
	int rownum) { // 找檔案找到字串行回傳幾行後的字串
	ifstream lanfile(filename.c_str());
	std::string filemsg;
	if (lanfile.is_open()) {
		while (!lanfile.eof()) {
			getline(lanfile, filemsg);
			if (strstr(filemsg.c_str(), findmsg.c_str())) {
				for (int i = 0; i < rownum; i++)
					getline(lanfile, filemsg);
				lanfile.close();
				return(AnsiString)filemsg.c_str();
			}
		}
		lanfile.close();
		return "";
	}
	else
		return "";
}
void __fastcall TfrmMain::btnResetDUTClick(TObject *Sender)
{
	//bGetAudioSetting = false;
	cbOutput->Width = (plFrmCbOutIn->Width-150) /2 ;
	cbInput->Width = (plFrmCbOutIn->Width-150) /2 ;
	btnDUT->Width = 147;
	cbOutput->Enabled = true;
	cbInput->Enabled = true;
	rb_TwoB->Enabled = true;
	rb_One->Enabled = true;
	bTesting = true;
	PAudio_PARM.HippoVolume=0;
	PAudio_PARM.DUTVolume=0;
	dwStep = WAIT_DEV_PLUGIN;
	plDUTVolume->Font->Color = clWindowText;
	plHippoVolume->Font->Color = clWindowText;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::rb_TwoBClick(TObject *Sender)
{
	TRadioButton *Rbtn = (TRadioButton *) Sender;
	if(Rbtn->Name.Pos("Two"))
	{
		AnsiString OUT_WAV = Findfilemsg("Audio\\Config.ini","[STEREO_WAV]",2);
		AnsiString IN_WAV  = Findfilemsg("Audio\\Config.ini","[STEREO_WAV]",4);
		for(int i =0;i<=cbOutputWAV->Items->Count;i++)
		{
			if(OUT_WAV.Pos(cbOutputWAV->Items->Strings[i]))
			   cbOutputWAV->ItemIndex = i;
		}
		for(int i =0;i<=cbInputWAV->Items->Count;i++)
		{
			if(IN_WAV.Pos(cbInputWAV->Items->Strings[i]))
			   cbInputWAV->ItemIndex = i;
		}
		PAudio_PARM.bStereo = true;
	}
	else
	{
		AnsiString OUT_WAV = Findfilemsg("Audio\\Config.ini","[MONO_WAV]",2);
		AnsiString IN_WAV  = Findfilemsg("Audio\\Config.ini","[MONO_WAV]",4);
		for(int i =0;i<=cbOutputWAV->Items->Count;i++)
		{
			if(OUT_WAV.Pos(cbOutputWAV->Items->Strings[i]))
			   cbOutputWAV->ItemIndex = i;
		}
		for(int i =0;i<=cbInputWAV->Items->Count;i++)
		{
			if(IN_WAV.Pos(cbInputWAV->Items->Strings[i]))
			   cbInputWAV->ItemIndex = i;
		}
		PAudio_PARM.bStereo = false;
    }
}
//---------------------------------------------------------------------------
bool TfrmMain::ReadInISet()
{
	AnsiString FILE_DUT_SET_INI = "Audio\\Config.ini";
	AnsiString astrTemp;
	if(FileExists(FILE_DUT_SET_INI))
	{
		try
		{
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[STEREO_TEST]",1);
			PAudio_PARM.bStereo = astrTemp=="true"?true:false;
			if(PAudio_PARM.bStereo)
			{
				rb_TwoBClick(rb_TwoB);
				rb_TwoB->Checked = true;
			}
			else
			{
				rb_TwoBClick(rb_One);
				rb_One->Checked = true;
			}
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[AUDIO_VOLUME_HIPPO]",1);
			PAudio_PARM.dwAudioVolumeHippo  = astrTemp != "" ? astrTemp.ToInt() : 50;
			HippoScrollBar->Position 		= PAudio_PARM.dwAudioVolumeHippo;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[AUDIO_VOLUME_DUT]",1);
			PAudio_PARM.dwAudioVolumeDUT	= astrTemp != "" ? astrTemp.ToInt() : 50;
			DUTScrollBar->Position   		= PAudio_PARM.dwAudioVolumeDUT;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[FFT_VERTEX(1~200)]",1);
			PAudio_PARM.dwFFT_Vertex 		= astrTemp != "" ? astrTemp.ToInt() : 10;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[WAVE_LOWER_LIMIT_VERTEX(1~32677)]",1);
			PAudio_PARM.dwWAVE_LL_Vertex	= astrTemp != "" ? astrTemp.ToInt() : 20000;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[WAVE_UPPER_LIMIT_VERTEX(1~32677)]",1);
			PAudio_PARM.dwWAVE_UL_Vertex	= astrTemp != "" ? astrTemp.ToInt() : 30000;
			PAudio_PARM.bAutoVolume			= Findfilemsg(FILE_DUT_SET_INI,"[VolumeCorrect]",1)=="true"?true:false;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[DELAY]",1);
			PAudio_PARM.dwDelay	= astrTemp != "" ? astrTemp.ToInt() : 1500;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[TIMEOUT](NO-TEST:-1)",1);
			PAudio_PARM.dwSetTimeOut	= astrTemp != "" ? astrTemp.ToInt() : 2000;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[VOLUME_RANGE]",1);
			PAudio_PARM.VolumeRange	= astrTemp != "" ? astrTemp.ToInt() : 10;			
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[CREST_%]",1);
			PAudio_PARM.dblCrest	= astrTemp != "" ? astrTemp.ToDouble()/100 :0.05;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[FREQ_%]",1);
			PAudio_PARM.dblFreq	= astrTemp != "" ? astrTemp.ToDouble()/100 : 0.05;
			astrTemp = Findfilemsg(FILE_DUT_SET_INI,"[CHANNEL BALANCE(%)]",1);
			PAudio_PARM.dbBalance	= astrTemp != "" ? astrTemp.ToDouble()/100 : 0.1;
			return true;
		}
		catch(...)
		{
			return false;
		}
	}
	else return false;
}
void TfrmMain::SaveInISet()
{
	AnsiString totalmsg = ""; // 檔案內容
	AnsiString astrTemp;
	astrTemp = PAudio_PARM.bStereo ? "true":"false";
	totalmsg =  "[STEREO_TEST]\n"+astrTemp;
	totalmsg += "\n[AUDIO_VOLUME_HIPPO]\n"+AnsiString(PAudio_PARM.dwAudioVolumeHippo);
	totalmsg += "\n[AUDIO_VOLUME_DUT]\n"+AnsiString(PAudio_PARM.dwAudioVolumeDUT);
	totalmsg += "\n[MONO_WAV]\nOUT_WAV:\nL1R2SinWave.wav\nIN_WAV:\nL1R1SinWave.wav";
	totalmsg += "\n[STEREO_WAV]\nOUT_WAV:\nL1R2SinWave.wav\nIN_WAV:\nL1R2SinWave.wav";
	totalmsg += "\n[WAVE_LOWER_LIMIT_VERTEX(1~32677)]\n"+AnsiString(PAudio_PARM.dwWAVE_LL_Vertex);
	totalmsg += "\n[WAVE_UPPER_LIMIT_VERTEX(1~32677)]\n"+AnsiString(PAudio_PARM.dwWAVE_UL_Vertex);
	totalmsg += "\n[FFT_VERTEX(1~200)]\n"+AnsiString(PAudio_PARM.dwFFT_Vertex);
	astrTemp = PAudio_PARM.bAutoVolume ? "true":"false";
	totalmsg += "\n[VolumeCorrect]\n"+astrTemp;
	totalmsg += "\n[DELAY]\n"+AnsiString(PAudio_PARM.dwDelay);
	totalmsg += "\n[TIMEOUT](NO-TEST:-1)\n"+AnsiString(PAudio_PARM.dwSetTimeOut);
	totalmsg += "\n[VOLUME_RANGE]\n"+AnsiString(PAudio_PARM.VolumeRange);
	totalmsg += "\n[CREST_%]\n"+AnsiString(AnsiString(PAudio_PARM.dblCrest*100));
	totalmsg += "\n[FREQ_%]\n"+AnsiString(AnsiString(PAudio_PARM.dblFreq*100));
	totalmsg += "\n[CHANNEL BALANCE(%)]\n"+AnsiString(AnsiString(PAudio_PARM.dbBalance*100));

	// 寫入檔案
	ofstream oConfigFile("Audio\\Config.ini");
	oConfigFile << totalmsg.c_str();
	oConfigFile.close();
}
void __fastcall TfrmMain::btnCorrectVolumeClick(TObject *Sender)
{
	bool bEnabled = cbOutput->Enabled;
	UIEnabled(false);
	if(VolumeCorrection())
	{
		plDUT->Caption = "Volume adjustment succeeded";
		plDUT->Font->Color = clBlue;
		ShowResult("PASS",clLime);
	}
	else
	{
		if(plDUT->Font->Color != clRed)
		{
			plDUT->Caption = "(!)Volume adjustment failed";
			plDUT->Font->Color = clRed;
		}
		ShowResult("FAIL",clRed);
	}
	UIEnabled(true);
	cbOutput->Enabled = bEnabled;
	cbInput->Enabled = bEnabled;
}
//---------------------------------------------------------------------------
void TfrmMain::UIEnabled(bool bEnable)
{
	btnCorrectVolume->Enabled = bEnable;
	btnDUT->Enabled = bEnable;
	DUTScrollBar->Enabled = bEnable;
	HippoScrollBar->Enabled = bEnable;
	btnResetDUT->Enabled = bEnable;
	btnTest->Enabled = bEnable;
	cbOutput->Enabled = bEnable;
	cbInput->Enabled = bEnable;
	cbOutputWAV->Enabled = bEnable;
	cbInputWAV->Enabled = bEnable;
}
bool TfrmMain::VolumeCorrection()
{
	UIEnabled(false);
	dwVolumeStep = WAIT_DEV_PLUGIN;
	int iResult,iStatus;
	bool bError = false;
	DWORD dwUL_VolumeValue   = 100;
	DWORD dwLL_VolumeValue   = 0;
	DWORD dwTemp;
	while(dwVolumeStep)
	{
		switch(dwVolumeStep)
		{
			case WAIT_DEV_PLUGIN:
				if(FindHippoDev()
					&& FindDevWaveOutCount(cbOutput->Text) != NOT_FIND_DEV_AUDIO
					&& FindDevWaveInCount(cbInput->Text) != NOT_FIND_DEV_AUDIO)
				{
					WAVE_ANLS->ClearFreqWave(FFT_View1);
					WAVE_ANLS->ClearFreqWave(FFT_View2);
					ClearWave(Wave_View1);
					ClearWave(Wave_View2);
					plDUT->Caption = "Testing...";
					plDUT->Font->Color = clBlue;
					ShowResult("Testing...",clCream);
					bLeftError  = false;
					bRightError = false;
					dwVolumeStep = TEST_AUDIO;
				}
				else
				{
					plDUT->Caption = "Not Find Device...";
					plDUT->Font->Color = clRed;
					ShowResult("FAIL",clRed);
					dwVolumeStep = TEST_END;
				}
				break;
			case TEST_AUDIO:
				bWaveOver = false;
				PAudio_PARM.WaveNum = 1;
				if(PlayVoice(DUT_TO_HIPPO,cbOutputWAV->Text))
					dwVolumeStep = WAVE_ANALYSIS;
				else
				{
					bError = true;
					dwVolumeStep = TEST_END;
				}
				break;
			case WAVE_ANALYSIS:
				if(bWaveOver)
				{
					dwTemp = cbOutputWAV->Text.SubString(2,cbOutputWAV->Text.Pos("R")-2).ToInt();
					iResult = WAVE_ANLS->GetWaveFreq(true,dwTemp);
					iStatus = iResult;
					if(iResult != WAVE_FREQ_PASS)
					{
						plDUT->Caption = "撥放波型異常(!)Playback failed";
						plDUT->Font->Color = clRed;
						bLeftError = true;
						if(iResult == ERROR_WAVE_MIXING)
							dwVolumeStep = AUDIO_ZOOM_OUT_1;
						else if(iResult == ERROR_WAVE_MUTE)
							dwVolumeStep = AUDIO_ZOOM_IN_1;
					}
					else
					{   //檢查時域波峰
						if((DWORD)plLVertex1->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500
							|| (DWORD)plRVertex1->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500)
							dwVolumeStep = AUDIO_ZOOM_IN_1;
						else if((DWORD)plLVertex1->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500
							|| (DWORD)plRVertex1->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500)
							dwVolumeStep = AUDIO_ZOOM_OUT_1;
					}

					dwTemp = cbOutputWAV->Text.SubString(cbOutputWAV->Text.Pos("R")+1,cbOutputWAV->Text.Pos("S")-cbOutputWAV->Text.Pos("R")-1).ToInt();
					iResult = WAVE_ANLS->GetWaveFreq(false,dwTemp);
					if(iResult != WAVE_FREQ_PASS)
					{
						plDUT->Caption = "撥放波型異常(!)Playback failed";
						plDUT->Font->Color = clRed;
						bLeftError = true;
						if(iResult == ERROR_WAVE_MIXING)
						{
							if(iStatus == ERROR_WAVE_MUTE)
							{    //左右聲道不一致
								bError = true;
								dwVolumeStep = TEST_AUDIO_2;
								dwUL_VolumeValue   = 100;
								dwLL_VolumeValue   = 0;
							}
							else
								dwVolumeStep = AUDIO_ZOOM_OUT_1;
						}
						else if(iResult == ERROR_WAVE_MUTE)
						{
							if(iStatus == ERROR_WAVE_MIXING)
							{   //左右聲道不一致
								bError = true;
								dwVolumeStep = TEST_AUDIO_2;
								dwUL_VolumeValue   = 100;
								dwLL_VolumeValue   = 0;
							}
							else
								dwVolumeStep = AUDIO_ZOOM_IN_1;
						}
					}
					else
					{   //檢查時域波峰
						if((DWORD)plLVertex1->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500
							|| (DWORD)plRVertex1->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500)
							dwVolumeStep = AUDIO_ZOOM_IN_1;
						else if((DWORD)plLVertex1->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500
							|| (DWORD)plRVertex1->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500)
							dwVolumeStep = AUDIO_ZOOM_OUT_1;
					}

					if(dwVolumeStep != AUDIO_ZOOM_IN_1 && dwVolumeStep != AUDIO_ZOOM_OUT_1 )
					{
						dwVolumeStep = TEST_AUDIO_2;
						dwUL_VolumeValue   = 100;
						dwLL_VolumeValue   = 0;
					}
				}
				break;
			case AUDIO_ZOOM_IN_1:
				if(DUTScrollBar->Position<=100)
				{
					dwTemp = (dwUL_VolumeValue + DUTScrollBar->Position)/2;
					if(dwTemp != (DWORD)DUTScrollBar->Position)
					{
						dwLL_VolumeValue = DUTScrollBar->Position;
						DUTScrollBar->Position = dwTemp;
						DUTScrollBarChange(DUTScrollBar);
						WAVE_ANLS->ClearFreqWave(FFT_View1);
						ClearWave(Wave_View1);
						plDUT->Caption = "Testing...";
						plDUT->Font->Color = clBlue;
						dwVolumeStep = TEST_AUDIO;
						break;
					}
				}
				bError = true;
				dwVolumeStep = TEST_AUDIO_2;
				dwUL_VolumeValue   = 100;
				dwLL_VolumeValue   = 0;
				break;
			case AUDIO_ZOOM_OUT_1:
				if(DUTScrollBar->Position>0)
				{
					dwTemp = (DUTScrollBar->Position+dwLL_VolumeValue)/2;
					if(dwTemp != (DWORD)DUTScrollBar->Position)
					{
						dwUL_VolumeValue = DUTScrollBar->Position;
						DUTScrollBar->Position = dwTemp;
						DUTScrollBarChange(DUTScrollBar);
						bLeftError = false;
						WAVE_ANLS->ClearFreqWave(FFT_View1);
						ClearWave(Wave_View1);
						plDUT->Caption = "Testing...";
						plDUT->Font->Color = clBlue;
						dwVolumeStep = TEST_AUDIO;
						break;
					}
				}
				bError = true;
				dwVolumeStep = TEST_AUDIO_2;
				dwUL_VolumeValue   = 100;
				dwLL_VolumeValue   = 0;
				break;
			case TEST_AUDIO_2:
				bWaveOver = false;
				PAudio_PARM.WaveNum = 2;
				if(PlayVoice(HIPPO_TO_DUT,cbInputWAV->Text))
					dwVolumeStep = WAVE_ANALYSIS_2;
				else
				{
					bError = true;
					dwVolumeStep = TEST_END;
                }
				break;
			case WAVE_ANALYSIS_2:
				if(bWaveOver)
				{
					dwTemp = cbInputWAV->Text.SubString(2,cbInputWAV->Text.Pos("R")-2).ToInt();
					iResult = WAVE_ANLS->GetWaveFreq(true,dwTemp);
					iStatus = iResult;
					if(iResult != WAVE_FREQ_PASS)
					{
						if(	plDUT->Caption.Pos("failed"))
							plDUT->Caption = "波型異常(!)All Waveform failed";
						else plDUT->Caption = "錄音波型異常(!)Recording failed";
						plDUT->Font->Color = clRed;
						bRightError = true;
						if(iResult == ERROR_WAVE_MIXING)
							dwVolumeStep = AUDIO_ZOOM_OUT_2;
						else if(iResult == ERROR_WAVE_MUTE)
							dwVolumeStep = AUDIO_ZOOM_IN_2;
					}
					else
					{   //檢查時域波峰
						if((DWORD)plLVertex2->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500
							|| (DWORD)plRVertex2->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500)
							dwVolumeStep = AUDIO_ZOOM_IN_2;
						else if((DWORD)plLVertex2->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500
							|| (DWORD)plRVertex2->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500)
							dwVolumeStep = AUDIO_ZOOM_OUT_2;
					}
					dwTemp = cbInputWAV->Text.SubString(cbInputWAV->Text.Pos("R")+1,cbInputWAV->Text.Pos("S")-cbInputWAV->Text.Pos("R")-1).ToInt();
					iResult = WAVE_ANLS->GetWaveFreq(false,dwTemp);
					if(iResult != WAVE_FREQ_PASS)
					{
						if(	plDUT->Caption.Pos("failed"))
							plDUT->Caption = "波型異常(!)All Waveform failed";
						else plDUT->Caption = "錄音波型異常(!)Recording failed";
						plDUT->Font->Color = clRed;
						bRightError = true;
						if(iResult == ERROR_WAVE_MIXING)
						{
							if(iStatus == ERROR_WAVE_MUTE)
							{    //左右聲道不一致
								bError = true;
								dwVolumeStep = TEST_AUDIO_2;
								dwUL_VolumeValue   = 100;
								dwLL_VolumeValue   = 0;
							}
							else
								dwVolumeStep = AUDIO_ZOOM_OUT_2;
						}
						else if(iResult == ERROR_WAVE_MUTE)
						{
							if(iStatus == ERROR_WAVE_MIXING)
							{    //左右聲道不一致
								bError = true;
								dwVolumeStep = TEST_AUDIO_2;
								dwUL_VolumeValue   = 100;
								dwLL_VolumeValue   = 0;
							}
							else
								dwVolumeStep = AUDIO_ZOOM_IN_2;
						}
					}
					else
					{   //檢查時域波峰
						if((DWORD)plLVertex2->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500
							|| (DWORD)plRVertex2->Caption.ToInt()<PAudio_PARM.dwWAVE_LL_Vertex+500)
							dwVolumeStep = AUDIO_ZOOM_IN_2;
						else if((DWORD)plLVertex2->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500
							|| (DWORD)plRVertex2->Caption.ToInt()>PAudio_PARM.dwWAVE_UL_Vertex-500)
							dwVolumeStep = AUDIO_ZOOM_OUT_2;
					}

					if(dwVolumeStep != AUDIO_ZOOM_IN_2 && dwVolumeStep != AUDIO_ZOOM_OUT_2 )
					{
						dwVolumeStep = TEST_END;
					}
				}
				break;
			case AUDIO_ZOOM_IN_2:
				if(HippoScrollBar->Position<=100)
				{
					dwTemp = (dwUL_VolumeValue + HippoScrollBar->Position)/2;
					if(dwTemp != (DWORD)HippoScrollBar->Position)
					{
						dwLL_VolumeValue = HippoScrollBar->Position;
						HippoScrollBar->Position = dwTemp;
						DUTScrollBarChange(HippoScrollBar);
						WAVE_ANLS->ClearFreqWave(FFT_View2);
						ClearWave(Wave_View2);
						plDUT->Caption = "Testing...";
						plDUT->Font->Color = clBlue;
						dwVolumeStep = TEST_AUDIO_2;
						break;
					}
				}
				bError = true;
				dwVolumeStep = TEST_END;
				break;
			case AUDIO_ZOOM_OUT_2:
				if(HippoScrollBar->Position>0)
				{
					dwTemp = (dwLL_VolumeValue + HippoScrollBar->Position)/2;
					if(dwTemp != (DWORD)HippoScrollBar->Position)
					{
						dwUL_VolumeValue = HippoScrollBar->Position;
						HippoScrollBar->Position = dwTemp;
						DUTScrollBarChange(HippoScrollBar);
						bLeftError = false;
						WAVE_ANLS->ClearFreqWave(FFT_View2);
						ClearWave(Wave_View2);
						plDUT->Caption = "Testing...";
						plDUT->Font->Color = clBlue;
						dwVolumeStep = TEST_AUDIO_2;
						break;
					}
				}
				bError = true;
				dwVolumeStep = TEST_END;
				break;
		}
		Delay(100);
	}

	return !bError;
}
DWORD TfrmMain::CrestVerify(bool bLeft,float freq)
{
	if(freq < 1)
	{
		float num = freq*23;
		if(bLeft)
		{
			if((num+num*PAudio_PARM.dblCrest)>=PAudio_PARM.dwLCrest
				&&(num-num*PAudio_PARM.dblCrest)<=PAudio_PARM.dwLCrest)
			return WAVE_CREST_NUMBER_PASS;
		}
		else
		{
			if((num+num*PAudio_PARM.dblCrest)>=PAudio_PARM.dwRCrest
				&&(num-num*PAudio_PARM.dblCrest)<=PAudio_PARM.dwRCrest)
			return WAVE_CREST_NUMBER_PASS;
		}
		return WAVE_CREST_NUMBER_FAIL;
	}
	return WAVE_CREST_NUMBER_PASS;
}



void __fastcall TfrmMain::FormClose(TObject *Sender, TCloseAction &Action)
{
	SaveInISet();
	delete[] my_data;
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::plWaveSwitchClick(TObject *Sender)
{
	if(frmMain->Height >400)
	{
		plWaveSwitch->Caption = "V";
		frmMain->Height =362;
	}
	else
	{
		plWaveSwitch->Caption = "^";
		frmMain->Height =863;
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::plFFTSwitchClick(TObject *Sender)
{
	//plFFTSwitch
	if(frmMain->Width >800)
	{
		plFFTSwitch->Caption = ">";
		frmMain->Width =529;
	}
	else
	{
		plFFTSwitch->Caption = "<";
		frmMain->Width =1196;
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmMain::Timer2Timer(TObject *Sender)
{
	if(Hippo_DEV.In_DevName== "USB Advanced" || Hippo_DEV.Out_DevName== "USB Advanced")
	{
		FindHippoDev();
	}
	else
	{
		Timer2->Enabled = false;
		btnDUT->Enabled = true;
	}
}
//---------------------------------------------------------------------------




void __fastcall TfrmMain::plResultDblClick(TObject *Sender)
{
	plFFTSwitch->Caption = "<";
	frmMain->Width =1454;	
}
//---------------------------------------------------------------------------
AnsiString TfrmMain::DosCommand(AnsiString sCmdline)
{
	PROCESS_INFORMATION proc = {0}; //關於進程資訊的一個結構
	long ret;
	bool sPipe;
	STARTUPINFOA start = {0};
	SECURITY_ATTRIBUTES sa = {0};
	HANDLE hReadPipe ;
	HANDLE hWritePipe;
	AnsiString sOutput;
	AnsiString sBuffer;
	unsigned long lngBytesRead;
	char cBuffer[256];
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor=0;
	sa.bInheritHandle = TRUE;
	sPipe=::CreatePipe(&hReadPipe, &hWritePipe,&sa, 0); //創建管道
	if (!sPipe)
	{
	sOutput="CreatePipe failed. Error: "+AnsiString(GetLastError());
	//memoMsg->Lines->Add("CreatePipe failed. Error: "+AnsiString(GetLastError()));
	return sOutput;
	}
	start.cb = sizeof(STARTUPINFOA);
	start.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	start.hStdOutput = hWritePipe;
	start.hStdError = hWritePipe;
	start.wShowWindow = SW_HIDE;
	sBuffer = sCmdline;
	ret =::CreateProcessA(0, sBuffer.c_str(), &sa, &sa, TRUE, NORMAL_PRIORITY_CLASS, 0, 0, &start, &proc);
	if (ret == 0)
	{
	sOutput="Bad command or filename";
	//memoMsg->Lines->Add("Bad command or filename");
	return sOutput;
	}
	::CloseHandle(hWritePipe);
	DWORD dw = WaitForSingleObject(proc.hProcess, 2000);
	if(dw == WAIT_TIMEOUT)
	{
		::CloseHandle(proc.hProcess);
		::CloseHandle(proc.hThread);
		::CloseHandle(hReadPipe);
		return "";
	}
	do
	{
	memset(cBuffer,'\0',sizeof(cBuffer));
	ret = ::ReadFile(hReadPipe, &cBuffer, 255, &lngBytesRead, 0);
	//替換字串
	for(unsigned long i=0; i< lngBytesRead; i++){
		if(cBuffer[i] == '\0'){
			cBuffer[i] = ' ';
		}else if(cBuffer[i] == '\n'){
			cBuffer[i] = ' ';
		}
	}
	sBuffer=StrPas(cBuffer);
	sOutput = sOutput+sBuffer;
	Application->ProcessMessages();

	} while (ret != 0 );
	::CloseHandle(proc.hProcess);
	::CloseHandle(proc.hThread);
	::CloseHandle(hReadPipe);
	return sOutput;
}
void TfrmMain::getmsg(TMessage & Msg)
{
  PCOPYDATASTRUCT pcp;
  pcp = (PCOPYDATASTRUCT)Msg.LParam;
  char* TraceMsg;
  TraceMsg = (char*)pcp->lpData;
  String STRTEMP=TraceMsg;
  if(STRTEMP == "AudioStatus")
  {
	HWND prg2Handle;
	prg2Handle = FindWindow(NULL,"MPCaptureTool");
	if (prg2Handle == NULL){
		//ShowMessage("! MPCaptureTool NULL !");
	}
	else
	{
		char Msg[255];
		strcpy(Msg, AnsiString(nowWorkStatus).c_str());
		COPYDATASTRUCT *pcp = new COPYDATASTRUCT;
		pcp->dwData = 0;
		pcp->cbData = sizeof(Msg);
		pcp->lpData = &Msg;
		SendMessage(prg2Handle,WM_COPYDATA, (WPARAM)NULL, (LPARAM)pcp);
		delete pcp;
	}
  }
}
