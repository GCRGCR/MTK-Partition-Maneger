// ConsoleApp1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include <conio.h>

#define DEBUG(x) do { \
	if (debugging_enabled) { std::cerr << x << std::endl; } \
} while (0)

//using MTKBootRecords
using namespace std;
//using byte = unsigned char;

typedef unsigned char byte;
const int Null= 0;



class PartitionRecord
{
public:
	byte Type;						// MBR & part.1 : 0x5 , ebr_start_block[i+1]-ebr_start_block[i] , 0xffffffff
	unsigned int  Offset;			// xBR part.x   : 0x83, start_block[i_p] - ebr_start_block[i] , size_block[i_p]
	unsigned int Lenght;  			//		last part. & EBR>0 :    ,                             , size=: 0xffffffff-(start_block[iter_p]-ebr_start_block[1]		EBR<=0:size=: 0xffffffff
	unsigned int SizeMB;			//		!MBR & part.4 : 0x5 , ebr_start_block[i+1]-ebr_start_block[1] ,  0xffffffff 
	unsigned int lngRealSize;						// xBR 510 , 511: 0x55,0xAA
	bool lastPartition;
	bool selected;

	PartitionRecord();
	~PartitionRecord();
	static PartitionRecord Add(const char* rawData, int pNumber);
	static bool ValidPartition(const char* rawData, int pNumber);
	static int ChangeRecord(int,int);
	static void SetPartitionData(PartitionRecord &pr);
};


class MTKBootRecords
{
public:
	static const int C_BR_FILE_SIZE = 512;
	static const int C_PartRecordOffset = 446;
	static const int C_PartRecSize = 16;
	static const int C_SlotPerBR = 4;
	static const int C_MaxBRCount = 8;
	static const int C_BlockSize = 512;
	static int StaticVariable;

	int BRCount;
	unsigned int TotalBlocks;
	unsigned int TotalMB;
	struct BootRecord{
		vector<PartitionRecord> PRs;
		string BRName;
	};

	vector<BootRecord> BootRecords;

private:

public:
	MTKBootRecords();
	~MTKBootRecords();

	MTKBootRecords::BootRecord MTKBootRecords::BRFromFile( const char[ C_BR_FILE_SIZE ],string);
	static unsigned int FourByteToInt(const char* data);
	static bool ValidBR(const char[ C_BR_FILE_SIZE ]);
	vector<MTKBootRecords::BootRecord> AddBootRecords(BootRecord br);
	void MTKBootRecords::PrintBootrecord( );
	void MTKBootRecords::AddTotal (unsigned int a)
	{
		TotalBlocks +=a;
		TotalMB = (long long(TotalBlocks) * C_BlockSize / 0x100000);;
	}

};
int MTKBootRecords::StaticVariable = 1;

bool Continue(char*);
int WaitKey(bool);
int KeyToNumber(int );
int Menu_Main();
int Menu_Change(MTKBootRecords);
int SelectPartition(MTKBootRecords);
int ChangePartition(MTKBootRecords,int);
int SavePartition(MTKBootRecords);
int Keyhandler();

MTKBootRecords ReadFiles();

int _tmain(int argc, _TCHAR* argv[])
{
	
	Menu_Main();
}


int Menu_Main()
{
	MTKBootRecords OrigBRs;
	MTKBootRecords EditedBRs;

	while (true)
	{
		cout << "\nMain Menu:\n\t"
			"Read Boot records\t 1\n"
			"\tChange Partition\t 2\n"
			"\tExit\t\t\t x\n\t\t\t\t";
		switch (WaitKey(true))
		{
		case 0x31:
			OrigBRs=ReadFiles();
			OrigBRs.PrintBootrecord();
			break;
		case 0x32:
			EditedBRs=OrigBRs;
			Menu_Change(EditedBRs);
			break;
		case 0x78:
			return 0;
		default:
			break;
		}
	}

	return 0;
	//EditedBRs.BootRecords[0].PD[0].Type  =111;
}

MTKBootRecords ReadFiles()         // ha egyet már megnyitott akkor ne akarjon többet
{
	fstream myfile;
	int BRCount=0;
	char fileArr[MTKBootRecords::C_BR_FILE_SIZE] = {};
	bool bOpenError=false;
	bool bAtLeast1Valid =false;
	MTKBootRecords OrgBRs;

	string filename = "MBR";
	do
	{
		myfile.open(filename, std::ios::in | std::ios::out ) ;

		if (!myfile.is_open())
		{
			//brvalid vagy brcount
			cout << '\n' + filename + " Open error." ;
			if(  bAtLeast1Valid ) break;
			else if(!Continue("\nContinue read? :"))break;
		}
		else
		{
			cout << filename + " open. \n";
			myfile.read(fileArr, MTKBootRecords::C_BR_FILE_SIZE);
			if ( MTKBootRecords::ValidBR(fileArr) )
			{
				bAtLeast1Valid =true;
			}
			else
			{
				cout << '\n' + filename + " Not valid MTK partition." ;
				if( !Continue("\nContinue read? :"))break;
			}
			OrgBRs.AddBootRecords(OrgBRs.BRFromFile(fileArr, filename));
		}
		//br valid beállítása
		myfile.close();
		BRCount++;
		filename = "EBR" + to_string(BRCount);// + EBRCount;
	}while (bOpenError == false & BRCount<MTKBootRecords::C_MaxBRCount);
	return  OrgBRs;
}

int Menu_Change(MTKBootRecords BRs)
{
	int intSelectedPartition=-1;
	while (true)
	{
		cout << "\nChange Partition Menu:\n\t""Select Partition\t 1\n"
			"\tEdit Partition\t\t 2\n"
			"\tSave New Bootrecords\t 9\n"
			"\tMain Menu\t\t x\n\t\t\t\t";
		switch (WaitKey(true))
		{
		case 0x31:
			intSelectedPartition=SelectPartition( BRs);
			//OrigBRs.PrintBootrecord();
			break;
		case 0x32:
			if (intSelectedPartition<0)
			{
				cout<<"\n\t No selected Partition!";
				break;
			}
			else
			{
					ChangePartition( BRs,intSelectedPartition);
			}
			break;
		case 0x39:
			SavePartition( BRs);

			break;
		case 0x78:
			return 0;
		default:
			break;
		}
	}
}

int SelectPartition(MTKBootRecords BRs)
{
	int intSelected=-1;
	int intSelectedBR=0;
	int intSelectedPartition=0;
	string intSelectedPartitionName="";
	cout <<"Select Boot Record: \n";													//Select a BootRecord
	for (int i = 0; i < BRs.BootRecords.size(); i++)
	{
		cout << '\t'+ BRs.BootRecords[i].BRName << ":\t" << i<<'\n';
	}
	while(true)																			//Wait for input number or exit
	{
		int key=WaitKey(false);
		if(key==0x78) break;
		else if ( 0x0<= KeyToNumber(key) & KeyToNumber(key) < BRs.BootRecords.size())	//Check input range
		{
			intSelectedBR= KeyToNumber(key);
			cout << "\n\t\t" + BRs.BootRecords[intSelectedBR].BRName + " selected.";
			break;
		}
		else cout << "\t\tPlease enter a correct BR number!\n";
	}

	cout <<"\n\tSelect Partition: \n";													//Select a partition
	for (int i = 0; i < BRs.BootRecords[intSelectedBR].PRs.size(); i++)
	{
		cout << "\t\t\tPartition "<< i << ":\t" << i<<'\n';
	}
	while(true)
	{
		int key=WaitKey(false);
		if(key==0x78) break;
		else if ( 0x0<= KeyToNumber(key) & KeyToNumber(key) < BRs.BootRecords[intSelectedBR].PRs.size())
		{
			intSelectedPartition=KeyToNumber(key);

			cout << "\n\t\t\t" + BRs.BootRecords[intSelectedBR].BRName + " partition " << intSelectedPartition << " selected.";
			break;
		}
		else cout << "\t\t\tPlease enter a correct Partition number!\n";
	}

	intSelected=intSelectedBR * 4 +intSelectedPartition;
	return intSelected;

	string str = "123";
	//string:: ( str ) >> intSelectedBR;
	int value = atoi(str.c_str());
	
}



int ChangePartition(MTKBootRecords BRs,int intSelected)
{
	int value=0;
	int BR =intSelected/4;
	int PT =intSelected%4;

	do
	{
		if (system("CLS")) system("clear");
		BRs.PrintBootrecord();
		value=Keyhandler();
		BRs.BootRecords[BR].PRs[PT].Lenght=BRs.BootRecords[BR].PRs[PT].Lenght+value;
		PartitionRecord::SetPartitionData( BRs.BootRecords[BR].PRs[PT]);
		//if (system("CLS")) system("clear");
		//BRs.PrintBootrecord();
	} while (value);

	
	return 0;
}

int Keyhandler()
{
	int retval=0;
	switch (WaitKey(true))
	{
	case 0x71:
		retval=1;
		break;
	case 0x61:
		retval=-1;
		break;
	case 0x77:
		retval=10;
		break;
	case 0x73:
		retval=-10;
		break;
	case 0x65:
		retval=100;
		break;
	case 0x64:
		retval=-100;
		break;
	case 0x74:
		retval=-100;
		break;
	case 0x67:
		retval=-100;
		break;
	case 0x78:
		retval= 0;
		break;

	//default:
		//break;
	}
	return retval;
}

int SavePartition(MTKBootRecords BRs)
{
	return 0;
}




bool Continue(char* str)
{
	//char thing;
	cout << str;
	//cin >> thing;
	if (WaitKey(true) == 'y')
		return true;
	return false;
}

int WaitKey(bool echo)        //lowercasera kell alakítani + számoknál számot adjon vissza
{
	/* while (kbhit())
	printf("you have touched key.\n");
	if (system("CLS")) system("clear");
	//clrscr(); */
	if(echo)
	{
		cout<<"  ";
		int ch=getche();
		cout<<'\n';
		return ch;
	}

	return getch();
}

int KeyToNumber(int key)
{
	if ( 0x30<=key  & key <= 0x39) return key - 0x30;
	return -1;
}

#pragma region MTKBootRecord Class =================================================================================================

MTKBootRecords::MTKBootRecords()
{
	BRCount = 0;
	TotalBlocks = 0;
	TotalMB = 0;
}

bool MTKBootRecords::ValidBR (const char fa[ ] )
{
	//OutputDebugStringA("s" );
	if (unsigned char(fa[510])==0x55 & unsigned char(fa[511])==0xAA) return true;	// magic ID number
	return false;
}

vector<MTKBootRecords::BootRecord> MTKBootRecords::AddBootRecords(BootRecord br)
{
	BootRecords.push_back(br);
	BRCount++;
	return BootRecords;
}

unsigned int  MTKBootRecords::FourByteToInt(const char* data)
{
	unsigned  calc=0;

	calc = unsigned char(*data) * 0x1;
	data++;
	calc += unsigned char(*data) * 0x0100;
	data++;
	calc += unsigned char(*data) * 0x010000;
	data++;
	calc += unsigned char(*data) * 0x01000000;
	return calc;
}

void MTKBootRecords::PrintBootrecord( )
{
	if(BootRecords.size()>0)
	{
		cout << '\n' << "partit:" << '\t' << "Type:" << '\t' << "Offset:" << '\t' << '\t' << "Lenght:" << '\t' << '\t' << "Real size:" << '\t' << "SizeMB:" << '\n';
		for (int b=0;b<BootRecords.size();b++)
		{
			for(int p=0;p<BootRecords[b].PRs.size();p++)
			{
				cout << noshowbase << b << "-" << p << "  "								//"partition:"  <<
					<< showbase << hex << internal << setfill('0')
					<< '\t' << setw(4) << int(BootRecords[b].PRs[p].Type) << " "	//"Type:"  <<
					<< '\t' << setw(10) << BootRecords[b].PRs[p].Offset << " "		//"Offset:" <<
					<< '\t' << setw(10) << BootRecords[b].PRs[p].Lenght				//"Lenght:" <<
					<< '\t' << setw(10) << BootRecords[b].PRs[p].lngRealSize		//"Real size:"
					<< '\t' << dec << setw(0) << BootRecords[b].PRs[p].SizeMB		//"MB size:"
					<< '\n';
			}
		}
		cout << "Total Blocks: " << hex << TotalBlocks << '\t' << "Total MB: " << dec << TotalMB << '\n';
	}
}

MTKBootRecords::BootRecord MTKBootRecords::BRFromFile(const char fa[C_BR_FILE_SIZE],string filename)
{
	bool LastPartition = false;
	BootRecord br = {};
	PartitionRecord pr;

	for (int p = 0; p < C_SlotPerBR & (pr.lastPartition==false); p++)
	{
		pr = pr.Add(&fa[ C_PartRecordOffset + p * C_PartRecSize], p);
		br.PRs.push_back(pr);
		AddTotal(pr.lngRealSize);
	}
	br.BRName=filename;
	return br;
}
MTKBootRecords::~MTKBootRecords()
{
}
#pragma endregion

#pragma region PartitionRecord Class =================================================================================================

PartitionRecord::PartitionRecord()
{
	Type=0;					
	Offset=0;				
	Lenght=0;  				
	//EndPartition=false;	
	lngRealSize=0;
	lastPartition=false;
	selected=false;
}

PartitionRecord PartitionRecord::Add(const char* rawData, int pNumber)
{
	PartitionRecord pr;
	pr.Type = rawData[4];
	pr.Offset = MTKBootRecords::FourByteToInt(&rawData[8]);
	pr.Lenght = MTKBootRecords::FourByteToInt(&rawData[12]);

	if (pNumber < MTKBootRecords::C_SlotPerBR - 1 )
	{
		if (!PartitionRecord::ValidPartition (&rawData[MTKBootRecords::C_PartRecSize],  pNumber+1))
		{
			pr.lastPartition = true;
			//pr.Lenght = 0xffffffff - pr.Lenght;
		}

	}
	SetPartitionData( pr);
	//if(pr.Type!=5)
		//pr.lngRealSize=pr.Lenght;

	//pr.SizeMB=(long long(pr.lngRealSize)*512/0x100000);

	/*for (int i = 0; i < 502; i++)
	{
	//cout << dec << i << '\t' << showbase << hex << internal << setfill('0') << setw(4) << int(rawData[i]) << '\n';
	cout <<dec<< noshowbase << i << "-" << i << "  " << showbase << hex << internal << setfill('0')  << setw(4) << int(rawData[i]) << '\n';
	}*/
	//return NewRecord;
	return pr;
}

void PartitionRecord::SetPartitionData(PartitionRecord &pr)
{
	if (pr.lastPartition == true)
		pr.lngRealSize = 0xffffffff - pr.Lenght;
		else pr.lngRealSize = pr.Lenght;
	if(pr.Type==5)
		pr.lngRealSize=0;//pr.Lenght;
	pr.SizeMB=(long long(pr.lngRealSize)*512/0x100000);

}

bool PartitionRecord::ValidPartition(const char* rawData, int pNumber)
{
	PartitionRecord pr;
	pr.Type = rawData[4];

	if ((pr.Type == 0x05 & (pNumber == 0 | pNumber == 3) | pr.Type == 0x83))
	{return true;}
	else
	{return false;}
}

int ChangeRecord(int record,int value)
{
	return 0;
}


PartitionRecord::~PartitionRecord()
{}
#pragma endregion



/*
//myfile.open("test.txt", std::ios::in | std::ios::out | std::ios::app);
//myfile.seekp(446);
//myfile.read(temp, 1);
//br[p].Type = temp[0];
//br[p].Offset =  dataArr[446 + p * 16+8];
//myfile << "Sikeresen megnyitva.";    // Ezt a file-ba írja
//cout << "Sikeresen megnyitva.";       // Ezt pedig a képernyõre
//for (int i = 0; !myfile.eof() & i<512; i++)
//lReadPos = myfile.tellg();
//lWritePos = myfile.tellp();
//myfile >> dataArr[i]; 



*/



