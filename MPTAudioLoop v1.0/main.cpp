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
// ������
// ���o�]��ID �Ammio APIŪ��wav�ɱN���T��Ʃ�m�w�İϡAwaveout APIŪ���w�İϸ�ƨü��X
bool TfrmMain::PlayVoice(int tag,AnsiString AudioFile)
{
	AudioFile = "Audio\\"+AudioFile;
	// MMIO(Ūwav��)
	MMCKINFO	ChunkInfo;
	MMCKINFO	FormatChunkInfo;
	MMCKINFO	DataChunkInfo;

	WAVEFORMATEX   wfx; // �i�έ��W�y�榡���ƾڵ��c

	// ��ܳ]��ID
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
	// ���}WAVE���A��^�@��HMMIO�y�`

	HMMIO handle = mmioOpen(AudioFile.c_str(), 0, MMIO_READ);
	// �@��WAVE���̤֥]�t�T�Ӱ϶��ARIFF���䤤�̤j�A���WAVE���N�O�@��RIFF�϶�
	// �i�JRIFF�϶�(RIFF Chunk)
	mmioDescend(handle, &ChunkInfo, 0, MMIO_FINDRIFF);

	// �i�Jfmt�϶�(RIFF�l���A�]�t���T���c���H��)
	FormatChunkInfo.ckid = mmioStringToFOURCC("fmt", 0); // �M��fmt�l��
	mmioDescend(handle, &FormatChunkInfo, &ChunkInfo, MMIO_FINDCHUNK);
	// �i�Jfmt�l��
	// Ū��wav���c�H��
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	mmioRead(handle, (char*) & wfx, FormatChunkInfo.cksize);
	// ���Xfmt�϶�
	mmioAscend(handle, &FormatChunkInfo, 0);
	// �i�Jdata�϶�(�]�t�Ҧ����ƾڪi��)
	DataChunkInfo.ckid = mmioStringToFOURCC("data", 0);
	mmioDescend(handle, &DataChunkInfo, &ChunkInfo, MMIO_FINDCHUNK);
	// ��odata�϶����j�p
	DataSize = DataChunkInfo.cksize;
	// ���t�w�İ�
	WaveOutData = new char[DataSize];
	mmioRead(handle, WaveOutData, DataSize);
	// ���}��X�]��
	if(waveOutOpen(&WaveOut, g_selectOutputDevice, &wfx, 0, 0, WAVE_FORMAT_QUERY)== MMSYSERR_NOERROR)
	{
		// �P�_�榡
		waveOutOpen(&WaveOut, g_selectOutputDevice, &wfx, NULL, NULL,
			CALLBACK_NULL);

		// �]�mwave header.
		memset(&WaveOutHeader, 0, sizeof(WaveOutHeader));
		WaveOutHeader.lpData = WaveOutData;
		WaveOutHeader.dwBufferLength = DataSize;
		// �ǳ�wave header.
		waveOutPrepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
		// �N�w�İϸ�Ƽg�J����]��(�}�l����).
		waveOutWrite(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
		waveOutSetVolume(WaveOut, PAudio_PARM.AudioVolume.ToInt());
		Delay(200);
		// �}�l����
		if(!StartRecording(tag))
		{
			// �����í��m�޲z��
			waveOutReset(WaveOut);
			// ��������]��
			waveOutClose(WaveOut);
			// �M�z��WaveOutPrepareHeader�ǳƪ�Wave�C
			waveOutUnprepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
			// ���񤺦s
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
	// ���o�]��ID
	int g_selectinputDevice;
	if(tag == DUT_TO_HIPPO){
			g_selectinputDevice = FindDevWaveInCount(Hippo_DEV.In_DevName);
	}else if(tag == HIPPO_TO_DUT){
			g_selectinputDevice = FindDevWaveInCount(cbInput->Text);
	}
	waveFormat.wFormatTag=WAVE_FORMAT_PCM;  // �榡
	waveFormat.nChannels=2;  // �n�D�ƶq�]�Y���n�D�A�����n...�^
	waveFormat.nSamplesPerSec=44100;  // �C���ļ˦���(�ļ˲v)
	waveFormat.wBitsPerSample=16;     // �C���ļ�Bit(�ļ˺��)
	waveFormat.nBlockAlign=waveFormat.wBitsPerSample / 8 * waveFormat.nChannels; // �ƾڪ���j�p
	waveFormat.nAvgBytesPerSec=waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;  // �w�İϦ��p
	waveFormat.cbSize=0;//PCM�榡 ���ȩ���

	//  Audio buffers
	WaveBuffers=new WAVEHDR[4];
	//2048 * �C���ļ�byte * �n�D�ƶq
	int WaveBufSize=buffer_size * waveFormat.wBitsPerSample / 8 * waveFormat.nChannels;
	// waveInOpen�}�ҿ�J�]��
	if((waveInOpen(&hWaveIn, g_selectinputDevice, &waveFormat, (DWORD) this->Handle, (DWORD) this, CALLBACK_WINDOW)) == MMSYSERR_NOERROR)
	{
		//VirtualAlloc �ӽе������s�Ŷ�
		//lpData: �w�İϦa�}
		for (int i = 0; i < 4; i++) 
		{
			WaveBuffers[i].lpData= (char *)VirtualAlloc(0, WaveBufSize, MEM_COMMIT, PAGE_READWRITE);
			WaveBuffers[i].dwBufferLength=WaveBufSize; // �w�İϪ���
			WaveBuffers[i].dwFlags=0; // �w�İ��ݩ�
			WaveBuffers[i].dwLoops=0; // ����`��������
			// �ǳƤ@�Ӫi�ο�J�w�İϡC�@����l�ƮɡA���\��i�����W�X�ʵ{���M�@�~�t�γB�z���Y�ɩνw�İϡC
			if((waveInPrepareHeader(hWaveIn, &WaveBuffers[i], sizeof(WAVEHDR))) == MMSYSERR_NOERROR)
			{   // �o�e����w���i�Ϊ����W��J�˸m����J�w�İϡC���w�İϳQ�񺡫�A�q�����ε{���C
				if((waveInAddBuffer(hWaveIn, &WaveBuffers[i], sizeof(WAVEHDR))) == MMSYSERR_NOERROR)
				{    
					if(i==3)
					{        
						// �}�l����
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
	::waveInUnprepareHeader(hWaveIn, &WaveBuffers[0], sizeof(WAVEHDR));//����w�İϪŶ�
	VirtualFree(WaveBuffers[0].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[1].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[3].lpData, 0, MEM_RELEASE);
	VirtualFree(WaveBuffers[4].lpData, 0, MEM_RELEASE);
	delete[] WaveBuffers;
	//
	return false;
}
//---------------------------------------------------------------------------
// �����t�ή���
void __fastcall TfrmMain::WndProc(TMessage &Message)
{   // �����n��������w�İϺ���
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

			if(GetTickCount()>frmMain->PAudio_PARM.dwTimeOut                   //�����ɶ���
				|| PAudio_PARM.dwNumberOfTimes+1 == PAudio_PARM.dwTotalFrames    //����V�Ƥw��
				|| PAudio_PARM.bError)                                         //�W�v�i�Τ���
			{
				PAudio_PARM.bWaveInOver		= true;
				DEBUG("Stop Playing");
				// �����í��m�޲z��
				waveOutReset(WaveOut);
				// ��������]��
				waveOutClose(WaveOut);
				// �M�z��WaveOutPrepareHeader�ǳƪ�Wave�C
				waveOutUnprepareHeader(WaveOut, &WaveOutHeader, sizeof(WAVEHDR));
				// ���񤺦s
				if (WaveOutData != NULL) {
					delete[]WaveOutData;
					WaveOutData = NULL;
				}
			}
			this->ProcessInput();
		}
	}
	TForm :: WndProc(Message);    //ȥ
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
		//�P�_
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
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[0], sizeof(WAVEHDR));//����w�İϪŶ�
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[1], sizeof(WAVEHDR));//����w�İϪŶ�
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[2], sizeof(WAVEHDR));//����w�İϪŶ�
		waveInUnprepareHeader(hWaveIn,&WaveBuffers[3], sizeof(WAVEHDR));//����w�İϪŶ�
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
	long    leftzerolevel=(Wave_View->Height / 2 - 10) / 2;   //���n�D X�b��m
	long    rightzerolevel=Wave_View->Height / 2 + 10 + leftzerolevel;//�k�n�D X�b��m
	double  timescale=leftzerolevel / 32768.0;//16�줸  -32768 ~ +32767
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
	double  ts=static_cast<double>(Wave_View->Width) / static_cast<double>(count); // �C�@�I�۶Z���Z��
    int     x;
	Wave_View->Canvas->MoveTo(0, leftzerolevel);
	float y_vertex = 0;
	bool bCrest = false;
	PAudio_PARM.dwLCrest = 0;
	Wave_View->Refresh();
	//Memo3->Lines->Add("---���n�D---");
	for(DWORD i=0; i < (count/dwMultiple) - 1; i+=2)
	{
		x=i * ts*dwMultiple;
		int y=leftzerolevel - static_cast<double>(my_data[i]) * timescale;
		Wave_View->Canvas->LineTo(x, y);
		if(static_cast<double>(my_data[i])>y_vertex) y_vertex = static_cast<double>(my_data[i]);
		//�p��i�p��
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
	//Memo3->Lines->Add("---�k�n�D---");
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
		//�p��i�p��
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
void TfrmMain::Delay(ULONG iMilliSeconds) // �쪩delay time �Φbthread�̭�
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
						plTestTimesNow->Caption = plTestTimesNow->Caption.ToInt()+1;
						btnStop->Caption = "Stop";
						btnStop->Enabled = true;
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
						//frmMain->DosCommand("Taskkill /im Video.UI.exe /F");
						//frmMain->DosCommand("Taskkill /im wmplayer.exe /F");
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
							plDUT->Caption = "���q���`(!)Volume Fail";
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
							plDUT->Caption = "����i�����`(!)Playback failed";
							plDUT->Font->Color = clRed;
							bLeftError = true;
						}
						dwTemp = cbOutputWAV->Text.SubString(cbOutputWAV->Text.Pos("R")+1,cbOutputWAV->Text.Pos("S")-cbOutputWAV->Text.Pos("R")-1).ToInt();
						if(WAVE_ANLS->GetWaveFreq(false,dwTemp) != WAVE_FREQ_PASS
							|| CrestVerify(false,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							plDUT->Caption = "����i�����`(!)Playback failed";
							plDUT->Font->Color = clRed;
							bLeftError = true;
						}
						if(abs(plRVertex1->Caption.ToDouble()-plLVertex1->Caption.ToDouble())/ (plRVertex1->Caption.ToDouble()+plLVertex1->Caption.ToDouble())
							> PAudio_PARM.dbBalance)
						{
							moDebug->Lines->Add("�n�D�~�t:"+AnsiString(abs(plRVertex1->Caption.ToDouble()-plLVertex1->Caption.ToDouble())/ (plRVertex1->Caption.ToDouble()+plLVertex1->Caption.ToDouble())));
							plRVertex1->Color = clRed;
							plLVertex1->Color = clRed;
							plDUT->Caption = "����i�����`(!)Playback failed";
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
								plDUT->Caption = "�i�����`(!)All Waveform failed";
							else plDUT->Caption = "�����i�����`(!)Recording failed";
							plDUT->Font->Color = clRed;
							bRightError = true;
						}
						dwTemp = cbInputWAV->Text.SubString(cbInputWAV->Text.Pos("R")+1,cbInputWAV->Text.Pos("S")-cbInputWAV->Text.Pos("R")-1).ToInt();
						if(WAVE_ANLS->GetWaveFreq(false,dwTemp) != WAVE_FREQ_PASS
							|| CrestVerify(false,dwTemp) != WAVE_CREST_NUMBER_PASS)
						{
							if(	plDUT->Caption.Pos("failed"))
								plDUT->Caption = "�i�����`(!)All Waveform failed";
							else plDUT->Caption = "�����i�����`(!)Recording failed";
							plDUT->Font->Color = clRed;
							bRightError = true;
						}
						if(abs(plRVertex2->Caption.ToDouble()-plLVertex2->Caption.ToDouble())/ (plRVertex2->Caption.ToDouble()+plLVertex2->Caption.ToDouble())
							> PAudio_PARM.dbBalance)
						{
							moDebug->Lines->Add("�n�D�~�t:"+AnsiString(abs(plRVertex2->Caption.ToDouble()-plLVertex2->Caption.ToDouble())/ (plRVertex2->Caption.ToDouble()+plLVertex2->Caption.ToDouble())));
							plRVertex2->Color = clRed;
							plLVertex2->Color = clRed;
							if(	plDUT->Caption.Pos("failed"))
								plDUT->Caption = "�i�����`(!)All Waveform failed";
							else plDUT->Caption = "�����i�����`(!)Recording failed";
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
						//�P�_���q�O�_�ŦX�]�w��
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
							plDUT->Caption = "���q���`(!)Volume Fail";
							plDUT->Font->Color = clRed;
							ShowResult("FAIL",clRed);
                        }
					}
					else
					{
						ShowResult("FAIL",clRed);
					}
					UIEnabled(true);
					dwStep = CHECK_KEEP_LOOP;
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
			case CHECK_KEEP_LOOP:
					if(plDUT->Font->Color == clRed)
						dwStep = WAIT_BUTTON_START;
					else if(btnStop->Caption == "Start")
						dwStep = WAIT_BUTTON_START;
					else if(FindDevWaveOutCount(cbOutput->Text) != NOT_FIND_DEV_AUDIO
					&& FindDevWaveInCount(cbInput->Text) != NOT_FIND_DEV_AUDIO)
					{
						if(plTestTimesNow->Caption.ToInt() < edtTimesOfTest->Text.ToInt())
						{
							plRVertex1->Color = clCream;
							plLVertex1->Color = clCream;
							plRVertex2->Color = clCream;
							plLVertex2->Color = clCream;
							ShowResult("Testing...",clCream);
							plDUT->Font->Color = clBlue;
							dwStep = WAIT_DEV_PLUGIN;
						}
						else
						{
							dwStep = WAIT_BUTTON_START;
                        }
					}
					else
					{
						plDUT->Caption = "(!)�䤣�쭵�T�˸m";
						plDUT->Font->Color = clRed;
						ShowResult("FAIL",clRed);
						dwStep = WAIT_BUTTON_START;
					}
				case WAIT_BUTTON_START:
					btnStop->Caption = "Start";
					btnStop->Enabled = true;
					break;
				break;
		}
		if(btnDUT->Width != 147)
			bTesting = false;
	}
}
//---------------------------------------------------------------------------
bool TfrmMain::CallCommand(AnsiString Command_line) { // �UCommand���O
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
	long    leftzerolevel=(Wave_View->Height / 2 - 10) / 2;   //���n�D X�b��m
	long    rightzerolevel=Wave_View->Height / 2 + 10 + leftzerolevel;//�k�n�D X�b��m
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
//���o�v��
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
					plHippo->Caption 	= AnsiString(Wave_output.szPname)+"�B"+Hippo_DEV.In_DevName;
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
	btnStop->Enabled = true;
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
	int rownum) { // ���ɮק��r���^�ǴX��᪺�r��
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
	AnsiString totalmsg = ""; // �ɮפ��e
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

	// �g�J�ɮ�
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
						plDUT->Caption = "����i�����`(!)Playback failed";
						plDUT->Font->Color = clRed;
						bLeftError = true;
						if(iResult == ERROR_WAVE_MIXING)
							dwVolumeStep = AUDIO_ZOOM_OUT_1;
						else if(iResult == ERROR_WAVE_MUTE)
							dwVolumeStep = AUDIO_ZOOM_IN_1;
					}
					else
					{   //�ˬd�ɰ�i�p
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
						plDUT->Caption = "����i�����`(!)Playback failed";
						plDUT->Font->Color = clRed;
						bLeftError = true;
						if(iResult == ERROR_WAVE_MIXING)
						{
							if(iStatus == ERROR_WAVE_MUTE)
							{    //���k�n�D���@�P
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
							{   //���k�n�D���@�P
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
					{   //�ˬd�ɰ�i�p
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
							plDUT->Caption = "�i�����`(!)All Waveform failed";
						else plDUT->Caption = "�����i�����`(!)Recording failed";
						plDUT->Font->Color = clRed;
						bRightError = true;
						if(iResult == ERROR_WAVE_MIXING)
							dwVolumeStep = AUDIO_ZOOM_OUT_2;
						else if(iResult == ERROR_WAVE_MUTE)
							dwVolumeStep = AUDIO_ZOOM_IN_2;
					}
					else
					{   //�ˬd�ɰ�i�p
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
							plDUT->Caption = "�i�����`(!)All Waveform failed";
						else plDUT->Caption = "�����i�����`(!)Recording failed";
						plDUT->Font->Color = clRed;
						bRightError = true;
						if(iResult == ERROR_WAVE_MIXING)
						{
							if(iStatus == ERROR_WAVE_MUTE)
							{    //���k�n�D���@�P
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
							{    //���k�n�D���@�P
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
					{   //�ˬd�ɰ�i�p
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




void __fastcall TfrmMain::plResultBDblClick(TObject *Sender)
{
	plFFTSwitch->Caption = "<";
	frmMain->Width =1454;	
}
//---------------------------------------------------------------------------
AnsiString TfrmMain::DosCommand(AnsiString sCmdline)
{
	PROCESS_INFORMATION proc = {0}; //����i�{��T���@�ӵ��c
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
	sPipe=::CreatePipe(&hReadPipe, &hWritePipe,&sa, 0); //�Ыغ޹D
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
	//�����r��
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
void __fastcall TfrmMain::plTestTimesNowDblClick(TObject *Sender)
{
	plTestTimesNow->Caption = edtTimesOfTest->Text;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnStopClick(TObject *Sender)
{
	if(btnStop->Caption == "Stop")
	{
		btnStop->Caption = "Start";
		btnStop->Enabled = false;
	}
	else
	{
		btnStop->Caption = "Stop";
		dwStep = WAIT_DEV_PLUGIN;
    }
}
//---------------------------------------------------------------------------
