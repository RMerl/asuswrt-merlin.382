/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "btconfig.h"
#include "masterblaster.h"

#define BT_PORT 2398
/* Global Variables */
static int sid, cid, aid;
static int Tag_Count = 0;
static int Patch_Count = 0;
static unsigned short DynMem_Count = 0;
static int Total_tag_lenght = 0;
static BOOL CtrlCBreak = FALSE;
static BOOL print_debug = FALSE;
bdaddr_t BdAddr;
/* Function Declarations */
static int LoadConfFile(const char *path, int basetag, int format);
static int ParseFiles(FILE *fpt, int basetag, int format);
static void LoadPSHeader(UCHAR *HCI_PS_Command,UCHAR opcode,int length,int index);
static BOOL PSOperations(int dd, UCHAR Opcode, int Param1, UINT32 *out);
static BOOL SU_LERxTest(int dev_id, UCHAR channel);
static BOOL SU_LETxTest(int dev_id, UCHAR channel, UCHAR length, UCHAR payload);
static int PSInit(int dd);
static void usage(void);
static int writeHciCommand(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, UCHAR *buf);
static int MemBlkRead(int dd, UINT32 Address,UCHAR *pBuffer, UINT32 Length);
static int MemBlkwrite(int dd, UINT32 Address,UCHAR *pBuffer, UINT32 Length);
static int Dut(int dd);
static int ReadAudioStats(int dd);
static int ReadGlobalDMAStats(int dd);
static int ResetGlobalDMAStats(int dd);
static int ReadTpcTable(int dd);
static int ReadHostInterest(int dd,tBtHostInterest *pHostInt);
static int ReadMemoryBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length );
static int WriteMemoryBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length );
static int write_otpRaw(int dev_id, int address, int length, UCHAR *data);
static int read_otpRaw(int dev_id, int address, int length, UCHAR *data);
static void dumpHex(UCHAR *buf, int length, int col);
static void sig_term(int sig);
static UCHAR LEMode = 0;
static FILE* debug_log_filep;

#define PRINTD(x...) \
do { \
	if(debug_log_filep) \
		fprintf(debug_log_filep,x); \
	if(print_debug) \
		printf(x); \
} while(0);

static struct option main_options[] = {
	{ "help",	0, 0, 'h' },
	{ "device",	1, 0, 'i' },
	{ 0, 0, 0, 0 }
};
//Read the configuration files, file paths are hardcoded in btconfig.h
static int LoadConfFile(const char *path, int basetag, int format){

	FILE *fp;
	//printf("\nOpening file :%s\n",path);
	fp = fopen(path,"r");
	if(fp == NULL){
	//	perror("File open error");
		return FALSE;
	}
	// Parse file
	if(!ParseFiles(fp,basetag,format)){
		printf("\nError :Invalid file format\n");
		return FALSE;
	}	
	// Load conf data to PS
	
	fclose(fp);
	return TRUE;
}


unsigned int uGetInputDataFormat(char **str, struct ST_PS_DATA_FORMAT *pstFormat)
{
	char *pCharLine = *str;
	if(pCharLine[0] != '[') {
		pstFormat->eDataType = eHex;
		pstFormat->bIsArray = TRUE;
		return TRUE;
	}
	switch(pCharLine[1]) {
		case 'H':
		case 'h':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'a') || (pCharLine[3]== 'A')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
				   	printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[H:A
					return FALSE;
				}
			}
			if((pCharLine[3]== 'S') || (pCharLine[3]== 's')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = FALSE;
					//pCharLine += 5;
					*str += 5;
					//printf("\nDEBUG H-1:%s\n",pCharLine);
					return TRUE;
				}
				else {
				   	printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[H:A
					return FALSE;
				}
			}
			else if(pCharLine[3] == ']') {         //[H:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 4;
				return TRUE;
			}
			else {                            //[H:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;					
			}
		}
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;			
		}
		break;

		case 'A':
		case 'a':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'h') || (pCharLine[3]== 'H')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[A:H
					return FALSE;
				}
		 	}
			else if(pCharLine[3]== ']') {         //[A:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 5;
				return TRUE;
			}
			else {                            //[A:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;					
			}
        }
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;			
		}
		break;

		case 'S':
		case 's':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'h') || (pCharLine[3]== 'H')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n");//[A:H
					return FALSE;
				}
		 	}
			else if(pCharLine[3]== ']') {         //[A:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 5;
				return TRUE;
			}
			else {                            //[A:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;					
			}
        }
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;			
		}
		break;
	
		default:
		printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
		return FALSE;
	}
}

unsigned int uReadDataInSection(char *pCharLine, struct ST_PS_DATA_FORMAT stPS_DataFormat)
{
	if(stPS_DataFormat.eDataType == eHex) {
		if(stPS_DataFormat.bIsArray == TRUE) {
            //Not implemented
            		printf("\nNO IMP\n");
            		return (0x0FFF);
		}
		else {
			//printf("\nDEBUG H-2 %d\n",strtol(pCharLine, NULL, 16));
			return (strtol(pCharLine, NULL, 16));
		}
	}
	else {
        //Not implemented
        	printf("\nNO IMP-1\n");
        	return (0x0FFF);
	}
}


static int ParseFiles(FILE *fpt, int basetag, int format){
	int i,j,k,linecnt,ByteCount=0,ByteCount_Org =0,data,Cnt;
	char *str,line[LINE_SIZE_MAX],byte[3];
	int ParseSelection=RAM_PS_SECTION;
	struct ST_PS_DATA_FORMAT stPS_DataFormat;
	struct ST_READ_STATUS   stReadStatus = {0, 0, 0,0};
	unsigned int uReadCount;

	switch(format){	
		case MB_FILEFORMAT_PS:
			linecnt = 0;
			j=0;

			while ((str = fgets(line, LINE_SIZE_MAX, fpt)) != NULL) {
			SKIP_BLANKS(str);
			//Comment line
			if ((str[0]== '/') && (str[1]== '/'))
				continue;
		
			if (str[0]== '#'){
				if (stReadStatus.uSection != 0){
			 		printf("\nParseFiles - Invalid file format %d\r\n",ParseSelection);
			 		return FALSE;
				 }
			 	else {
					 stReadStatus.uSection = 1;
					 continue;
			 	}
			
			}
			if ((str[0]== '/') && (str[1]== '*'))
			{
				str+=2;
				SKIP_BLANKS(str);
				if(!strncmp(str,"PA",2)||!strncmp(str,"Pa",2)||!strncmp(str,"pa",2)){
					
					ParseSelection=RAM_PATCH_SECTION;
				}
				if(!strncmp(str,"DY",2)||!strncmp(str,"Dy",2)||!strncmp(str,"dy",2)){
					
					ParseSelection=RAM_DYN_MEM_SECTION;
				}
				if(!strncmp(str,"PS",2)||!strncmp(str,"Ps",2)||!strncmp(str,"ps",2)){
					
					ParseSelection=RAM_PS_SECTION;
				}
				linecnt = 0;
				stReadStatus.uSection = 0;
				continue;
			}
		
			switch(ParseSelection){

				case RAM_PS_SECTION:
					if (stReadStatus.uSection == 1){ //TagID
		         			SKIP_BLANKS(str);	
						if(!uGetInputDataFormat(&str, &stPS_DataFormat)) {
							return FALSE;
                    				}	
						PsTagEntry[Tag_Count].TagId = uReadDataInSection(str, stPS_DataFormat);
						stReadStatus.uSection = 2;
					}
					else if (stReadStatus.uSection == 2){ //TagLength
						if(!uGetInputDataFormat(&str, &stPS_DataFormat)) {
							return FALSE;
						}
						ByteCount = uReadDataInSection(str, stPS_DataFormat);

		            			if (ByteCount > RAMPS_MAX_PS_DATA_PER_TAG){
			                		printf("\nParseFiles - INVALID %d: One of the table exceeds maximum table size of %d\r\n",ParseSelection, MAX_RADIO_CFG_TABLE_SIZE);
		        				return FALSE;
		        	    		}
		           			 PsTagEntry[Tag_Count].TagLen = (ByteCount & 0xFF);
						 stReadStatus.uSection = 3;
						 stReadStatus.uLineCount = 0;
		         		}
					else if( stReadStatus.uSection == 3) {  //Data
				        	if(stReadStatus.uLineCount == 0) {
							if(!uGetInputDataFormat(&str,&stPS_DataFormat)) {
								return FALSE;
							}
        		            		}
					   	SKIP_BLANKS(str);
                    				stReadStatus.uCharCount = 0;
						uReadCount = (ByteCount > BYTES_OF_PS_DATA_PER_LINE)? BYTES_OF_PS_DATA_PER_LINE: ByteCount;
						if((stPS_DataFormat.eDataType == eHex) && stPS_DataFormat.bIsArray == TRUE) {
                       					while(uReadCount > 0) {
		                 				PsTagEntry[Tag_Count].TagData[stReadStatus.uByteCount] = (UCHAR)(CONV_HEX_DIGIT_TO_VALUE(str[stReadStatus.uCharCount]) << 4) | (UCHAR)(CONV_HEX_DIGIT_TO_VALUE(str[stReadStatus.uCharCount + 1]));
		                   				PsTagEntry[Tag_Count].TagData[stReadStatus.uByteCount+1] = (UCHAR)(CONV_HEX_DIGIT_TO_VALUE(str[stReadStatus.uCharCount + 3]) << 4) | (UCHAR)(CONV_HEX_DIGIT_TO_VALUE(str[stReadStatus.uCharCount + 4]));
					       			stReadStatus.uCharCount += 6; // read two bytes, plus a space;
					       			stReadStatus.uByteCount += 2;
					       			uReadCount -= 2;
                       					}

					   if(ByteCount > BYTES_OF_PS_DATA_PER_LINE) {
					   	   ByteCount -= BYTES_OF_PS_DATA_PER_LINE;
					   }
					   else {
						  ByteCount = 0;
					   }
					}
					else {
						//to be implemented
						printf("\nParseFiles - To be implemented");
					}

					stReadStatus.uLineCount++;
					
					if(ByteCount == 0) {
						stReadStatus.uSection = 0;
						stReadStatus.uCharCount = 0;
						stReadStatus.uLineCount = 0;
						stReadStatus.uByteCount = 0;
					}
					else {
					    stReadStatus.uCharCount = 0;
					}
					
		            		if((stReadStatus.uSection == 0)&&(++Tag_Count == RAMPS_MAX_PS_TAGS_PER_FILE))
		            		{
						printf("\n ParseFiles - INVALID %d: Number of tables exceeds %d\r\n",ParseSelection, RAMPS_MAX_PS_TAGS_PER_FILE);
		            			return FALSE;
					}
		
				}
				break;
				default:
	    	 		{
					printf("\nParseFiles - Invalid file format %d\r\n",ParseSelection);
				        return FALSE;
				}
				break;
	
					
		}
		linecnt++;	
	}
		break;
	case MB_FILEFORMAT_DY:
	{
		linecnt = 0;
		while ((str = fgets(line, LINE_SIZE_MAX, fpt)) != NULL) {
			SKIP_BLANKS(str);
			//Comment line
			if ((str[0]== '/') && (str[1]== '/'))
				continue;
		
			if ((str[0]== '/') && (str[1]== '*'))
			{
				continue;
			}
		
	
			if((linecnt % 2) == 0)
			{
	  			ByteCount = (UINT16)strtol(str, NULL, 16);
		  		RamDynMemOverride.Len= (ByteCount & 0xFF);
			}
	  		else
			{
				for (i=0,k=0; k < ByteCount; i += 2,k++) {
					memcpy(byte, &str[i], 2);
					byte[2] = '\0';
					data = strtoul(byte, NULL, 16);
					RamDynMemOverride.Data[k] = (data & 0xFF);
				}
				DynMem_Count = TRUE;
			}
			linecnt++;
		}
	}
		break;
	case MB_FILEFORMAT_PATCH:
	{
		j=0;
		Cnt=0;
		linecnt = 0;
		while ((str = fgets(line, LINE_SIZE_MAX, fpt)) != NULL) {
			SKIP_BLANKS(str);
			//Comment line
			if ((str[0]== '/') && (str[1]== '/'))
				continue;
		
			if ((str[0]== '/') && (str[1]== '*'))
			{
				continue;
			}
	
			if(linecnt==0)
			{
				ByteCount = (UINT16)strtol(str, NULL, 16);
				ByteCount_Org = ByteCount;
				while(ByteCount > MAX_BYTE_LENGTH){
					RamPatch[Patch_Count].Len= MAX_BYTE_LENGTH;
					Patch_Count ++;
					ByteCount= ByteCount - MAX_BYTE_LENGTH;
				}
				RamPatch[Patch_Count].Len= (ByteCount & 0xFF);
				Patch_Count ++;
	
			}
			else
			{
				while(ByteCount_Org > MAX_BYTE_LENGTH){
					for (i = 0, k=0; i < MAX_BYTE_LENGTH*2; i += 2,k++) {
						memcpy(byte, &str[Cnt], 2);
						byte[2] = '\0';
						data = strtoul(byte, NULL, 16);
						RamPatch[j].Data[k] = (data & 0xFF);
						Cnt += 2;
					}
					j++;
					ByteCount_Org = ByteCount_Org - MAX_BYTE_LENGTH;
				}
				if(j == 0){
					j++;
				}
				for (k=0; k < ByteCount_Org;k++) {
					memcpy(byte, &str[Cnt], 2);
					byte[2] = '\0';
					data = strtoul(byte, NULL, 16);
					RamPatch[j].Data[k] = (data & 0xFF);
					Cnt += 2;
				}
			}
	
			linecnt++;	
		}
			
	}
	break;


	
	}
	return TRUE;
}

static void LoadPSHeader(UCHAR *HCI_PS_Command,UCHAR opcode,int length,int index) {
	
	HCI_PS_Command[0]= opcode;
	HCI_PS_Command[1]= (index & 0xFF);
	HCI_PS_Command[2]= ((index>>8) & 0xFF);	
	HCI_PS_Command[3]= length;	

}


static BOOL PSOperations(int dd, UCHAR Opcode, int Param1, UINT32 *out) {
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int Length,i,j,iRet;
	memset(&buf,0,sizeof(buf));
	switch(Opcode){
		case WRITE_PATCH:
			for(i=0;i< Param1;i++){
				LoadPSHeader(buf,Opcode,RamPatch[i].Len,i);
				for(j=0;j<RamPatch[i].Len;j++){
					buf[4+j]=RamPatch[i].Data[j];
				}
				iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_PS,RamPatch[i].Len + PS_COMMAND_HEADER, buf);
				if(buf[iRet-1] != 0){
					return FALSE;
				}
			}
			break;

		case ENABLE_PATCH:
			Length =0;
			i=0;
			LoadPSHeader(buf,Opcode,Length,i);
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_RESET:
			Length =0;
			i=0;
			LoadPSHeader(buf,Opcode,Length,i);
			buf[8] = (Param1 & 0xFF);
			buf[9] = ((Param1 >>  8) & 0xFF);
			Length = 6;
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,Length + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_READ: {
			UCHAR *len = (UCHAR *)out;
			ssize_t plen = 0;
			Length = len[0] | ( len[1] << 8);
			LoadPSHeader(buf,Opcode,Length,Param1);
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS, Length + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0) {
				return FALSE;
			}
			do {
				plen = read(dd, buf, HCI_MAX_EVENT_SIZE);
				if (plen < 0)
					return FALSE;
			} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
			memcpy((UCHAR *)out, buf + HCI_EVENT_HEADER_SIZE + 1, plen - HCI_EVENT_HEADER_SIZE - 1);
			break;
		}

		case PS_WRITE:
			for(i=0;i< Param1;i++){
				LoadPSHeader(buf,Opcode,PsTagEntry[i].TagLen,PsTagEntry[i].TagId);
				for(j=0;j<PsTagEntry[i].TagLen;j++){
					buf[4+j]=PsTagEntry[i].TagData[j];
				}
				iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PsTagEntry[i].TagLen + PS_COMMAND_HEADER, buf);
				if(buf[iRet-1] != 0){
					return FALSE;
				}
			}
			break;

		case PS_DYNMEM_OVERRIDE:
			LoadPSHeader(buf,Opcode,RamDynMemOverride.Len,RamDynMemOverride.Len);
			for(j=0;j<RamDynMemOverride.Len;j++){
				buf[4+j]=RamDynMemOverride.Data[j];
			}
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_PS,RamDynMemOverride.Len + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_VERIFY_CRC:
			//printf("PSOperations - PS_VERIFY_CRC:VALUE of CRC:%d\r\n",Param1);
			Length =0;
			LoadPSHeader(buf,Opcode,Length,Param1);
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS, PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_GET_LENGTH: {
			ssize_t plen = 0;
			LoadPSHeader(buf,Opcode,0,Param1);
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS, PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			do {
				plen = read(dd, buf, HCI_MAX_EVENT_SIZE);
				if (plen < 0)
					return FALSE;
			} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
			*((UINT16 *)out) = (buf[HCI_EVENT_HEADER_SIZE + 2] << 8) | buf[HCI_EVENT_HEADER_SIZE + 1];
			break;
		}
	}
		return TRUE;
}


static int GetDeviceType(int dd){
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int iRet;
	unsigned int Reg = 0;

	memset(&buf,0,sizeof(buf));
	buf[0] = (FPGA_REGISTER & 0xFF);
	buf[1] = ((FPGA_REGISTER >> 8) & 0xFF);
	buf[2] = ((FPGA_REGISTER >> 16) & 0xFF);
	buf[3] = ((FPGA_REGISTER >> 24) & 0xFF);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY,4, buf);
	if(buf[6] != 0){
		return FALSE;
	}

	Reg = buf[10];
	Reg = ((Reg << 8) | buf[9]);
	Reg = ((Reg << 8) | buf[8]);
	Reg = ((Reg << 8) | buf[7]);
	return Reg;
}
/* PS Operations */
static int PSInit(int dd){
	int i,Crc=0,DevType =0;
   	BOOL BDADDR_Present = 0;	
	BOOL File_Present = 0;
	
	//DevType = GetDeviceType(dd);
	//printf("\nDevice Type:%x\n",DevType);
	if(DevType){
		if(DevType == 0xdeadc0de){
		
			if(!LoadConfFile(PS_ASIC_FILENAME,0xFFFFFFFF,MB_FILEFORMAT_PS)){
				printf("\nPlease copy PS file to :%s\n",PS_ASIC_FILENAME);
				//return FALSE;
			}
			else
				File_Present = 1;
		}
		else{
			if(!LoadConfFile(PS_FPGA_FILENAME,0xFFFFFFFF,MB_FILEFORMAT_PS)){
				printf("\nPlease copy PS file to :%s\n",PS_FPGA_FILENAME);
				File_Present = 1;
				//return FALSE;
			}
			else
				File_Present = 1;
		}
	}
	else{
		if(!LoadConfFile(PS_ASIC_FILENAME,0xFFFFFFFF,MB_FILEFORMAT_PS)){
				printf("\nPlease copy PS file to :%s\n",PS_ASIC_FILENAME);
				File_Present = 1;
				//return FALSE;
		}
		else
			File_Present = 1;
	}

	if(!LoadConfFile(PATCH_FILENAME,0xFFFFFFFF,MB_FILEFORMAT_PATCH)){
		printf("\nPlease copy Patch file to :%s\n",PATCH_FILENAME);
		File_Present = 1;
	}
	else
		File_Present = 1;
	
	if(!File_Present){
		printf("\nPS and Patch files are not present\n");
		return FALSE;
	}
	if(Tag_Count == 0){
		Total_tag_lenght = 10;

	}
	else{
		for(i=0; i<Tag_Count; i++){
			if(PsTagEntry[i].TagId == 1){
				BDADDR_Present = TRUE;
				//printf("ReadPSFiles - BD ADDR is present in Patch File \r\n");
			}
		
			if(PsTagEntry[i].TagLen % 2 == 1){
				Total_tag_lenght = Total_tag_lenght + PsTagEntry[i].TagLen + 1;
			}
			else{
				Total_tag_lenght = Total_tag_lenght + PsTagEntry[i].TagLen;
			}
		
		}
	}
	
	if(Tag_Count > 0 && !BDADDR_Present){
		//printf("\nReadPSFiles - BD ADDR is not present adding 10 extra bytes \r\n");
		Total_tag_lenght=Total_tag_lenght + 10;
	}
	Total_tag_lenght = Total_tag_lenght+ 10 + (Tag_Count*4);

//	printf("\nPSInitialize - PATCH:%d, DYN:%d, TAG:%d Total_tag_lenght:%d\n",Patch_Count,DynMem_Count,Tag_Count,Total_tag_lenght);
	
	if(Patch_Count > 0)
		Crc |= RAM_PATCH_REGION;
	if(DynMem_Count)
		Crc |= RAM_DYN_MEM_REGION;
	if(Tag_Count > 0)
		Crc |= RAM_PS_REGION;

	if(Patch_Count || DynMem_Count || Tag_Count ){
		if(Patch_Count > 0){
			if(!PSOperations(dd,WRITE_PATCH,Patch_Count, NULL)){
				printf("\nPSInitialize - *** WRITE_PATCH FAILED**** \r\n");
				return FALSE;
			}
			if(!PSOperations(dd,ENABLE_PATCH,0, NULL)){
				printf("\nPSInitialize - *** ENABLE_PATCH FAILED**** \r\n");
				return FALSE;
			}
		}
		if(DynMem_Count){
			if(!PSOperations(dd,PS_DYNMEM_OVERRIDE,DynMem_Count, NULL)){
				printf("\nPSInitialize - *** PS_DYNMEM_OVERRIDE FAILED**** \r\n");
				return FALSE;
			}
		}
		if(!PSOperations(dd,PS_RESET,Total_tag_lenght, NULL)){
			printf("\nPSInitialize - *** PS RESET FAILED**** \r\n");
			return FALSE;
		}
		if(Tag_Count > 0){
			if(!PSOperations(dd,PS_WRITE,Tag_Count, NULL)){
				printf("\nPSInitialize - *** PS_WRITE FAILED**** \r\n");
				return FALSE;
			}
		}	
		
	}	
	
	if(!PSOperations(dd,PS_VERIFY_CRC,Crc, NULL)){
		printf("\nVerify CRC failed\n");	
		return FALSE;
	}
	return TRUE;

}

static int writeHciCommand(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, UCHAR *buf){
#ifdef DUMP_DEBUG
#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htobs(d)	(d)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define htobs(d)	bswap_16(d);
#else
#error "Unknown byte order"
#endif
	int i = 0, j = 0;
	uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
	
	printf("\nDump:\n");
	printf("0x%02X ", opcode & 0xff);
	i++;
	printf("0x%02X ", (opcode & 0xff00) >> 8);
	i++;
	printf("0x%02X ", plen);
	i++;
	for (j = 0; j < plen; i++, j++) {
		printf("0x%02X ", buf[j]);
		if (((i+1) % 8) == 0 && i != 0)
			printf("\n");
	}
	if (((i+1) % 8) != 0) printf("\n");
	buf[6] = 0;
	return plen;
#else
	struct hci_filter flt;
        uint16_t opcode, topcode;

	/* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}

	//printf("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d\n", ogf, ocf, plen);
	if (hci_send_cmd(dd, ogf, ocf, plen, buf) < 0) {
		perror("Send failed");
		hci_close_dev(dd);
		exit(EXIT_FAILURE);
	}
	sleep(0.4);
        opcode = (ogf << 10 | ocf);
        do {
	    plen = read(dd, buf,HCI_MAX_EVENT_SIZE);
	    if (plen < 0) {
		    perror("Read failed");
		    hci_close_dev(dd);
		    exit(EXIT_FAILURE);
	    }
	    topcode=(uint16_t)(buf[4] | (buf[5] << 8));
        }while(topcode != opcode);
	return plen;
#endif
}
static int MemBlkRead(int dd,UINT32 Address,UCHAR *pBuffer, UINT32 Length){
	UINT32         Size, ByteLeft,IntCnt;
   	UCHAR          *pData,*pTemp=pBuffer;
	int iRet;	
       	int TempVal;

	IntCnt =0;
       	TempVal = (Length % 4);
	if (TempVal !=0)
       	{
          Length = Length + (4- (Length%4));
       	}
        ByteLeft = Length;
	while (ByteLeft > 0)
   	{
		Size = (ByteLeft > MEM_BLK_DATA_MAX) ? MEM_BLK_DATA_MAX : ByteLeft;
	//	printf("\nMemBlkwrite : Size :%x   Address :%x\n",Size, Address);
		pData = (UCHAR *) malloc(Size + 6);
		pData[0]= 0x00;//depot/esw/projects/azure/AR3001_3_0/src/hci/Hci_Vsc_Proc.c
		pData[1]= (Address & 0xFF);
		pData[2]= ((Address >> 8) & 0xFF);
		pData[3]= ((Address >> 16) & 0xFF);
		pData[4]= ((Address >> 24) & 0xFF);
		pData[5]= Size;
		iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_MEMOP,6,pData);
		if(pData[6]!= 0){
			printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);	
			free(pData);
			return FALSE;
		}
		if ((read(dd, pData,HCI_MAX_EVENT_SIZE)) < 0) {
			perror("Read failed");
			exit(EXIT_FAILURE);
	    	}

	    	if(pData[3]!=3) {
  			perror("Read failed");
			exit(EXIT_FAILURE);
	    	}
	    	memcpy(pTemp,(pData+4),Size);
		pTemp+=Size;
	        IntCnt = Size;
		ByteLeft -= Size;
	    	Address += Size;
		free(pData);
	}
  	return TRUE;
}


static int MemBlkwrite(int dd,UINT32 Address,UCHAR *pBuffer, UINT32 Length){
	UINT32         Size, ByteLeft,IntCnt;
   	UCHAR          *pData;
	int iRet;	
	ByteLeft = Length;
	IntCnt =0;
	while (ByteLeft > 0)
   	{
	      	
		Size = (ByteLeft > MEM_BLK_DATA_MAX) ? MEM_BLK_DATA_MAX : ByteLeft;
	//	printf("\nMemBlkwrite : Size :%x   Address :%x\n",Size, Address);
		pData = (UCHAR *) malloc(Size + 6);
		pData[0]= 0x01;
		pData[1]= (Address & 0xFF);
		pData[2]= ((Address >> 8) & 0xFF);
		pData[3]= ((Address >> 16) & 0xFF);
		pData[4]= ((Address >> 24) & 0xFF);
		pData[5]= Size;
		memcpy(&pData[6],&pBuffer[IntCnt],Size);
		iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_MEMOP,Size+6,pData);
		if(pData[6]!= 0){
			printf("\nwrite memory command faileddue to reason 0x%X\n",pData[6]);	
			free(pData);
			return FALSE;
		}
	      	IntCnt = Size;
      		ByteLeft -= Size;
	      	Address += Size;
		free(pData);
   	}
  	return TRUE;
}

static int Dut(int dd){
	int iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 3; //All scan enabled
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, buf);	
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	sleep(1);
	memset(&buf,0,HCI_MAX_EVENT_SIZE);	
	iRet = writeHciCommand(dd, OGF_TEST_CMD, OCF_ENABLE_DEVICE_UNDER_TEST_MODE, 0, buf);	
	if(buf[6] != 0){
		printf("\nDUT mode command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);	
	buf[0] = 0; //SEQN Track enable =0
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_TEST_MODE_SEQN_TRACKING , 1, buf);	
	if(buf[6] != 0){
		printf("\nTest Mode seqn Tracking failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	return TRUE;
}


void Audio_DumpStats(tAudioStat *AudioStats)
{
  printf("\n\n");
  printf("    Audio Statistics\n");
  printf(">RxCmplt:                 %d\n",AudioStats->RxCmplt);
  printf(">TxCmplt:                 %d\n",AudioStats->TxCmplt);
  printf(">RxSilenceInsert:         %d\n",AudioStats->RxSilenceInsert);
  printf(">RxAirPktDump:            %d\n",AudioStats->RxAirPktDump);
  printf(">MaxPLCGenInterval:       %d\n",AudioStats->MaxPLCGenInterval);
  printf(">RxAirPktStatusGood:      %d\n",AudioStats->RxAirPktStatusGood);
  printf(">RxAirPktStatusError:     %d\n",AudioStats->RxAirPktStatusError);
  printf(">RxAirPktStatusLost:      %d\n",AudioStats->RxAirPktStatusLost);
  printf(">RxAirPktStatusPartial:   %d\n",AudioStats->RxAirPktStatusPartial);
  printf(">SampleMin:               %d\n",AudioStats->SampleMin);
  printf(">SampleMax:               %d\n",AudioStats->SampleMax);
  printf(">SampleCounter:           %d\n",AudioStats->SampleCounter);
  printf("\n\n");

  memset((UCHAR *)AudioStats, 0, sizeof(tAudioStat));
  AudioStats->SampleMax =SHRT_MIN;
  AudioStats->SampleMin =SHRT_MAX;
}

static int ReadAudioStats(int dd){

	tBtHostInterest	HostInt;
	tAudioStat Stats;

	ReadHostInterest(dd, &HostInt);
	if(!HostInt.AudioStatAddr || (HostInt.Version < 0x0300)){
		printf("\nAudio Stat not present\n");
		return FALSE;
	}
	ReadMemoryBlock(dd,HostInt.AudioStatAddr,(UCHAR *)&Stats,sizeof(tAudioStat));
	Audio_DumpStats(&Stats);
	return TRUE;
}

void BRM_DumpStats(tBRM_Stats *Stats)
{
   printf("\n  Link Controller Voice DMA Statistics\n");
   printf("  %22s: %u\n", "VoiceTxDmaIntrs", Stats->VoiceTxDmaIntrs);
   printf("  %22s: %u\n", "VoiceTxPktAvail", Stats->VoiceTxPktAvail);
   printf("  %22s: %u\n", "VoiceTxPktDumped", Stats->VoiceTxPktDumped);
   printf("  %22s: %u\n", "VoiceTxErrors", Stats->VoiceTxErrorIntrs);
   printf("  %22s: %u\n", "VoiceTxDmaErrors", Stats->VoiceTxDmaErrorIntrs);
   printf("  %22s: %u\n", "VoiceTxSilenceInserts", Stats->VoiceTxDmaSilenceInserts);
   printf("\n");
   printf("  %22s: %u\n", "VoiceRxDmaIntrs", Stats->VoiceRxDmaIntrs);
   printf("  %22s: %u\n", "VoiceRxGoodPkts", Stats->VoiceRxGoodPkts);
   printf("  %22s: %u\n", "VoiceRxPktDumped", Stats->VoiceRxPktDumped);
   printf("  %22s: %u\n", "VoiceRxErrors", Stats->VoiceRxErrorIntrs);
   printf("  %22s: %u\n", "VoiceRxCRC", Stats->VoiceRxErrCrc);
   printf("  %22s: %u\n", "VoiceRxUnderOverFlow", Stats->VoiceRxErrUnderOverFlow);
   printf("\n");
   printf("  %22s: %u\n", "SchedOnVoiceError", Stats->SchedOnVoiceError);
   printf("  %22s: %u\n", "VoiceTxReapOnError", Stats->VoiceTxReapOnError);
   printf("  %22s: %u\n", "VoiceRxReapOnError", Stats->VoiceRxReapOnError);
   printf("  %22s: %u\n", "VoiceSchedulingError", Stats->VoiceSchedulingError);

   printf("\n  Link Controller ACL DMA Statistics\n");
   printf("  %22s: %u\n", "DmaIntrs", Stats->DmaIntrs);
   printf("  %22s: %u\n", "ErrWrongLlid", Stats->ErrWrongLlid);
   printf("  %22s: %u\n", "ErrL2CapLen", Stats->ErrL2CapLen);
   printf("  %22s: %u\n", "ErrUnderOverFlow", Stats->ErrUnderOverFlow);
   printf("  %22s: %u\n", "RxBufferDumped", Stats->RxBufferDumped);
   printf("  %22s: %u\n", "ErrWrongLmpPktType", Stats->ErrWrongLmpPktType);
   printf("  %22s: %u\n", "ErrWrongL2CapPktType", Stats->ErrWrongL2CapPktType);
   printf("  %22s: %u\n", "IgnoredPkts", Stats->IgnoredPkts);
   printf("\n");
   printf("  %22s: %u\n", "Data TxBuffers", Stats->DataTxBuffers);
   printf("  %22s: %u\n", "Data RxBuffers", Stats->DataRxBuffers);
   printf("  %22s: %u\n", "LMP TxBuffers", Stats->LmpTxBuffers);
   printf("  %22s: %u\n", "LMP RxBuffers", Stats->LmpRxBuffers);
   printf("  %22s: %u\n", "HEC Errors", Stats->HecFailPkts);
   printf("  %22s: %u\n", "CRC Errors", Stats->CrcFailPkts);

   // Buffer Management
   printf("\n  Buffer Management Statistics\n");
   printf("  %22s: %u\n", "CtrlErrNoLmpBufs", Stats->CtrlErrNoLmpBufs);

   printf("\n  Sniff Statistics\n");
   printf("  %22s: %u\n", "SniffSchedulingError", Stats->SniffSchedulingError);
   printf("  %22s: %u\n", "SniffIntervalNoCorr", Stats->SniffIntervalNoCorr);

   // Other stats
   printf("\n  Other Statistics\n");
   printf("  %22s: %u\n", "ForceOverQosJob", Stats->ForceOverQosJob);
   //printf("  %22s: %u\n", "Temp 1", Stats->Temp1);
   //printf("  %22s: %u\n", "Temp 2", Stats->Temp2);

   // Test Mode Stats
   printf("\n  Test Mode Statistics\n");
   printf("  %22s: %u\n", "TestModeDroppedTxPkts", Stats->TestModeDroppedTxPkts);
   printf("  %22s: %u\n", "TestModeDroppedLmps", Stats->TestModeDroppedLmps);

   // Error Stats
   printf("\n  General Error Statistics\n");
   printf("  %22s: %u\n", "TimePassedIntrs", Stats->TimePassedIntrs);
   printf("  %22s: %u\n", "NoCommandIntrs", Stats->NoCommandIntrs);
}

static int ReadGlobalDMAStats(int dd){
	tBtHostInterest	HostInt;
	tBRM_Stats  Stats;
	
	ReadHostInterest(dd, &HostInt);
	if(!HostInt.GlobalDmaStats || (HostInt.Version < 0x0100)){
		printf("\nGlobal DMA stats not present\n");
		return FALSE;
	}
	ReadMemoryBlock(dd,HostInt.GlobalDmaStats,(UCHAR *)&Stats,sizeof(tBRM_Stats));
	BRM_DumpStats(&Stats);
	return TRUE;
}

static int ResetGlobalDMAStats(int dd){
	tBtHostInterest	HostInt;
	tBRM_Stats  Stats;
	
	ReadHostInterest(dd, &HostInt);
	if(!HostInt.GlobalDmaStats || (HostInt.Version < 0x0100)){
		printf("\nGlobal DMA stats not present\n");
		return FALSE;
	}
	memset(&Stats,0,sizeof(Stats));
	printf("\nHarry\n");
	WriteMemoryBlock(dd,HostInt.GlobalDmaStats,(UCHAR *)&Stats,sizeof(tBRM_Stats));
	printf("\nDMA stattestics has been reset\n");
	return TRUE;
}

static int ReadTpcTable(int dd){
	tBtHostInterest	HostInt;
	tPsSysCfgTransmitPowerControlTable  TpcTable;
	int i;
	
	ReadHostInterest(dd, &HostInt);
	if(!HostInt.TpcTableAddr || (HostInt.Version < 0x0100)){
		printf("\nTPC table not present\n");
		return FALSE;
	}
	ReadMemoryBlock(dd,HostInt.TpcTableAddr,(UCHAR *)&TpcTable,sizeof(TpcTable));
	for(i=0;i< TpcTable.NumOfEntries; i++){
		printf("Level [%d] represents %3d dBm\n",i,TpcTable.t[i].TxPowerLevel);
	}
	return TRUE;
}

/*
static void dump_conf_data(){
	printf("\nTAG_COUNT %d\n",Tag_Count);
	int i=0,j=0;
	for(i=0;i<Tag_Count;i++){
		printf("\nTAG ID :%X    LEN:%X\n",PsTagEntry[i].TagId,PsTagEntry[i].TagLen);
		for(j=0;j<PsTagEntry[i].TagLen;j++)
			printf("\t %x",PsTagEntry[i].TagData[j]);
	}
	printf("\n");
	printf("\nPATCH_COUNT %d\n",Patch_Count);
	for(i=0;i<Patch_Count;i++){
		printf("\tPATCH LEN:%X\t",RamPatch[i].Len);
		for(j=0;j<RamPatch[i].Len;j++)
			printf("\t %x",RamPatch[i].Data[j]);
	}
	printf("\n");

	printf("\tDYMA LEN:%X\t",RamDynMemOverride.Len);
	for(j=0;j<RamDynMemOverride.Len;j++)
		printf("\t %x",RamDynMemOverride.Data[j]);
	printf("\n");

}
*/
static const char *psreset_help =
	"Usage:\n"
	"\n psreset\n";

static void cmd_psreset(int dev_id, int argc, char **argv){
	int dd,Length =0,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	if(argc > 1){
		printf("\n%s\n",psreset_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	PSInit(dd);
	memset(&buf,0,sizeof(buf));
	iRet = writeHciCommand(dd, OGF_HOST_CTL,OCF_RESET,Length,buf);
	if(buf[6] != 0){
		printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
		return;
	}

    // Bttest work around for external 32k


    int IsForeverRepeat=0;
    int IsCmdIdle=0;
    int address=0, width = 0, value=0;
    int loop=0,Reg=0;
    do
    {

        address = 0x00020024;
        width = 4;
        buf[0] = (address & 0xFF);
        buf[1] = ((address >>8) & 0xFF);
        buf[2] = ((address>>16) & 0xFF);
        buf[3] = ((address>>24) & 0xFF);
        buf[4] = (UCHAR)width;  //Memory width
        iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY, 5, buf);	
        if(buf[6] != 0){
            printf("\nRead Memory address failed due to reason 0x%X\n",buf[6]);
            hci_close_dev(dd);	
            return;
        }
        value = buf[10];
        value = ((value << 8) | buf[9]);
        value = ((value << 8) | buf[8]);
        value = ((value << 8) | buf[7]);


        if(value&0x40000000)
            IsForeverRepeat=1;
        else
            IsForeverRepeat=0;


        address = 0x00020020;
        width = 4;
        buf[0] = (address & 0xFF);
        buf[1] = ((address >>8) & 0xFF);
        buf[2] = ((address>>16) & 0xFF);
        buf[3] = ((address>>24) & 0xFF);
        buf[4] = (UCHAR)width;  //Memory width
        iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY, 5, buf);	
        if(buf[6] != 0){
            printf("\nRead Memory address failed due to reason 0x%X\n",buf[6]);
            hci_close_dev(dd);	
            return;
        }
        value = buf[10];
        value = ((value << 8) | buf[9]);
        value = ((value << 8) | buf[8]);
        value = ((value << 8) | buf[7]);

        if((value&0x0000000f)==0x8)
            IsCmdIdle=1;
        else
            IsCmdIdle=0;

    }
    while(!(IsForeverRepeat&IsCmdIdle));//Wait til brm issues forever idle


    address = 0x000200a8;
    width = 4;
    buf[0] = (address & 0xFF);
    buf[1] = ((address >>8) & 0xFF);
    buf[2] = ((address>>16) & 0xFF);
    buf[3] = ((address>>24) & 0xFF);
    buf[4] = (UCHAR)width;  //Memory width
    iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY, 5, buf);	
    if(buf[6] != 0){
        printf("\nRead Memory address failed due to reason 0x%X\n",buf[6]);
        hci_close_dev(dd);	
        return;
    }
    value = buf[10];
    value = ((value << 8) | buf[9]);
    value = ((value << 8) | buf[8]);
    value = ((value << 8) | buf[7]);


    value |= 0x1;

    loop = 0;
    while ( loop < 10 ) {

            loop++;

	    address = 0x000200a8;
	    width = 4;

	    buf[0] = (address & 0xFF);
	    buf[1] = ((address >>8) & 0xFF);
	    buf[2] = ((address>>16) & 0xFF);
	    buf[3] = ((address>>24) & 0xFF);
	    buf[4] = width;  //Memory width
	    buf[5] = (value & 0xFF);
	    buf[6] = ((value >> 8) & 0xFF);
	    buf[7] = ((value >> 16) & 0xFF);
	    buf[8] = ((value >> 24) & 0xFF);
	    buf[9] =  0xFF;
	    buf[10] = 0xFF;
	    buf[11] = 0xFF;
	    buf[12] = 0xFF;
	    iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
	    if(buf[6] != 0){
		printf("\nWrite memory address failed\n");
		hci_close_dev(dd);	
		return;
	    }

	address = 0x00004064;
	width = 4;
	buf[0] = (address & 0xFF);
	buf[1] = ((address >>8) & 0xFF);
	buf[2] = ((address>>16) & 0xFF);
	buf[3] = ((address>>24) & 0xFF);
	buf[4] = (UCHAR)width;  //Memory width
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY, 5, buf);	
	if(buf[6] != 0){
	    printf("\nRead Memory address failed\n");
	    hci_close_dev(dd);	
	    return;
	}
	Reg = buf[10];
	Reg = ((Reg << 8) | buf[9]);
	Reg = ((Reg << 8) | buf[8]);
	Reg = ((Reg << 8) | buf[7]);

	if(Reg & 0x04) {
           break;
        }
    }

//--------------------------------------------------------------


	hci_close_dev(dd);
	printf("\nReset Done\n");
}
static const char *reset_help =
	"Usage:\n"
	"\n reset\n";

static void cmd_reset(int dev_id, int argc, char **argv){
	int dd,Length =0,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	if(argc > 1) {
		printf("\n%s\n",reset_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,sizeof(buf));
	iRet = writeHciCommand(dd, OGF_HOST_CTL,OCF_RESET,Length,buf);
	if(buf[6] != 0){
		printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
		return;
	}
 
	hci_close_dev(dd);
	printf("\nReset Done\n");
}
static const char *rba_help =
	"Usage:\n"
	"\n rba\n";

static void cmd_rba(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	if(argc > 1){
		printf("\n%s\n",rba_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);	
	if(buf[6] != 0){
		printf("\nread bdaddr command failed due to reason 0x%X\n",buf[6] );
		return;
	}
	printf("\nBD ADDRESS: \n");
	int i;
	for(i=iRet-1;i > 7;i--){
		printf("%02X:",buf[i]);
	}
	printf("%X \n\n",buf[7]);
	hci_close_dev(dd);

}

static const char *dtx_help =
	"Usage:\n"
	"\n dtx\n";

static void cmd_dtx(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	if(argc > 1){
		printf("\n%s\n",dtx_help);	
		return;
	}


	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}

        memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_DISABLE_TX, 0, buf);	
	if(buf[6] != 0){
		printf("\nDisable TX command failed due to reason 0x%X\n",buf[6]);
		return;
	}
        else {
             printf("\nDisable TX command passed\n");
        }
	hci_close_dev(dd);

}

static const char *ssm_help =
	"Usage:\n"
	"\n ssm [0|1]\n"
	"\nExample:\n"
	"\tssm 0\t(Sleep disabled)\n"
	"\tssm 1\t(Sleep enabled)\n";

static void cmd_ssm(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	
	if(argc != 2){
		printf("\n%s\n",ssm_help);	
		return;
	}

	if(atoi(argv[1]) > 1){
		printf("\nInvalid sleep mode :%d\n",atoi(argv[1]));
		return;
	}


	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}

        memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = atoi(argv[1]);;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_SLEEP_MODE, 1, buf);	
	if(buf[6] != 0){
		printf("\nSet sleep mode command failed due to reason 0x%X\n",buf[6]);
		return;
	}
        else {
             printf("\nSet sleep mode command passed\n");
        }
	hci_close_dev(dd);

}

static const char *wba_help =
	"Usage:\n"
	"\n wba <bdaddr>\n"
	"\nExample:\n"
	"\n wba 00:03:ff:56:23:89\n";

static void cmd_wba(int dev_id, int argc, char **argv){
	//printf("\nFeature not implemented\n");	
	int dd,iRet;
	bdaddr_t bdaddr;
	
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if(argc < 2){
		printf("\n%s\n",wba_help);	
		return;
	}

	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	str2ba(argv[1],&bdaddr);	
	if((strlen(argv[1]) < 17)||(strlen(argv[1]) > 17)){
		printf("\nInvalid BD address : %s\n",argv[1]);	
		printf("\n%s\n",wba_help);
		hci_close_dev(dd);	
		return;
	}
	LoadPSHeader(buf,PS_WRITE,BD_ADDR_SIZE,BD_ADDR_PSTAG);
	int i,j=0;
	for(i= 0,j=4;i< BD_ADDR_SIZE;i++,j++){
		buf[j] = bdaddr.b[i];
        }
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,BD_ADDR_SIZE + PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\n Write BD address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	memset(&buf,0,sizeof(buf));
	iRet = writeHciCommand(dd, OGF_HOST_CTL,OCF_RESET,0,buf);
	if(buf[iRet-1] != 0){
		printf("\nError: HCI RESET failed\n");
		hci_close_dev(dd);	
		return;
	} 
	memset(&buf,0,sizeof(buf));
	iRet = writeHciCommand(dd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);	
	if(buf[6] != 0){
		printf("\nread bdaddr command failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	printf("\nBD address changed successfully\n");	
	hci_close_dev(dd);	
}

static const char *edutm_help =
	"Usage:\n"
	"\n edutm\n";

static void cmd_edutm(int dev_id, int argc, char **argv){
	int Crc = 0;	
	int dd;
	//UCHAR buf[HCI_MAX_EVENT_SIZE];
	UCHAR ZeroBuf[MEM_BLK_DATA_MAX*2] = {0};	
	if(argc > 1){
		printf("\n%s\n",edutm_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
/*
	Patch_Count = 20;
	for(i=0; i < Patch_Count; i++){
		RamPatch[i].Len = MAX_BYTE_LENGTH;
		memset(&RamPatch[i].Data,0,MAX_BYTE_LENGTH);
	}
	printf("\nCMD DUT MODE\n");
*/
//When Patch file is present write the patch, if not present just enter DUT mode
	if(!LoadConfFile(TESTPATCH_FILENAME,0xFFFFFFFF,MB_FILEFORMAT_PATCH)){
		if(!Dut(dd)){
			hci_close_dev(dd);	
			return;	
		}
		printf("\nDevice is in test mode ...\n");
		hci_close_dev(dd);
		return;
	}
	//dump_conf_data();
	Crc |= RAM_PATCH_REGION;
	if(!MemBlkwrite(dd,(UINT32)MC_BCAM_COMPARE_ADDRESS, ZeroBuf, HCI_3_PATCH_SPACE_LENGTH_1)){
  		printf("\nError in clearing the patch space 1\n"); 
		return;
   	}

   	if(!MemBlkwrite(dd,(UINT32)MC_BCAM_VALID_ADDRESS, ZeroBuf, HCI_3_PATCH_SPACE_LENGTH_1)){
  		printf("\nError in clearing the patch space 2\n"); 
		return;
   	}
	printf("\nLoading Patch from file :%s\n",TESTPATCH_FILENAME);
	if(!PSOperations(dd,WRITE_PATCH,Patch_Count, NULL)){
		printf("EnterDUT_HCI_3 : patch write failed \r\n");
		return;
	}
	if(!PSOperations(dd,ENABLE_PATCH,0, NULL)){
		printf("EnterDUT_HCI_3 : patch enable failed \r\n");
		return;
	}	
	if(!PSOperations(dd,PS_VERIFY_CRC,Crc, NULL)){
		printf("EnterDUT_HCI_3 : verify crc failed \r\n");
		return;
	}
	if(!Dut(dd)){
		hci_close_dev(dd);	
		return;	
	}
	printf("\nDevice is in test mode ...\n");
	hci_close_dev(dd);	
}


static int ReadMemorySmallBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length ){
	int iRet;
   	UCHAR *pData;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	pData = (UCHAR *) malloc(Length + 6);
	memset(pData,0,Length+6);	
	pData[0]= 0x00;  //Memory Read Opcode
	pData[1]= (StartAddress & 0xFF);
	pData[2]= ((StartAddress >> 8) & 0xFF);
	pData[3]= ((StartAddress >> 16) & 0xFF);
	pData[4]= ((StartAddress >> 24) & 0xFF);
	pData[5]= Length;
	
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_MEMOP,Length+6,pData);
	if(pData[6]!= 0){
		printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);	
		free(pData);
		return FALSE;
	}
	int plen =0;
	do{
		plen = read(dd, buf,HCI_MAX_EVENT_SIZE);
		if (plen < 0) {
			free(pData);
			perror("Read failed");
			exit(EXIT_FAILURE);
		}
	}while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_MEMBLK);	
	memcpy(pBufToWrite,(buf+HCI_EVENT_HEADER_SIZE+1),Length);
	free(pData);
	return TRUE;
}

static int ReadMemoryBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length ){
			
	int ModResult,i;
	
	if(Length > MEM_BLK_DATA_MAX){
		ModResult = Length % MEM_BLK_DATA_MAX;	
		for(i=0;i < (Length - ModResult);i += MEM_BLK_DATA_MAX) {
			ReadMemorySmallBlock(dd, (StartAddress + i),(pBufToWrite + i), MEM_BLK_DATA_MAX);
		}
		if(ModResult){
			ReadMemorySmallBlock(dd, (StartAddress + i),(pBufToWrite + i), ModResult);
		}
	}
	else{

		ReadMemorySmallBlock(dd, StartAddress, pBufToWrite, Length);
	}
	return TRUE;
}

static int WriteMemorySmallBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length ){
	int iRet;
   	UCHAR *pData;
	
	printf("\nStart Address:%x Length:%x  %x\n",StartAddress,Length,MEM_BLK_DATA_MAX);
	/*if(Length <= MEM_BLK_DATA_MAX)
		return FALSE; */
	pData = (UCHAR *) malloc(Length + 6);
	memset(pData,0,Length+6);	
	pData[0]= 0x01;  //Write Read Opcode
	pData[1]= (StartAddress & 0xFF);
	pData[2]= ((StartAddress >> 8) & 0xFF);
	pData[3]= ((StartAddress >> 16) & 0xFF);
	pData[4]= ((StartAddress >> 24) & 0xFF);
	pData[5]= Length;
	
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_MEMOP,Length+6,pData);
	if(pData[6]!= 0){
		printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);	
		free(pData);
		return FALSE;
	}
	free(pData);
	return TRUE;
}


static int WriteMemoryBlock(int dd, int StartAddress,UCHAR *pBufToWrite, int Length ){
			
	int ModResult,i;
	
	if(Length > MEM_BLK_DATA_MAX){
		ModResult = Length % MEM_BLK_DATA_MAX;	
		for(i=0;i < (Length - ModResult);i += MEM_BLK_DATA_MAX) {
			WriteMemorySmallBlock(dd, (StartAddress + i),(pBufToWrite + i), MEM_BLK_DATA_MAX);
		}
		if(ModResult){
			WriteMemorySmallBlock(dd, (StartAddress + i),(pBufToWrite + i), ModResult);
		}
	}
	else{

		WriteMemorySmallBlock(dd, StartAddress, pBufToWrite, Length);
	}
	return TRUE;
}


static int ReadHostInterest(int dd,tBtHostInterest *pHostInt){
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int iRet;
	int HostInterestAddress;	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);	
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_HOST_INTEREST, 0, buf);	
	if(buf[6] != 0){
		printf("\nhost interest command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	HostInterestAddress = buf[iRet-1];
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-2]);	
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-3]);	
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-4]);	
	ReadMemoryBlock(dd, HostInterestAddress,(UCHAR*)pHostInt, sizeof(tBtHostInterest));
	
	if(pHostInt->MagicNumber != HI_MAGIC_NUMBER){
		if((pHostInt->MagicNumber != 0xFBAD)|| (pHostInt->Version != 0xDECA))
			return 0;
	}
	return TRUE;
	
}

static int contRxAtGivenChannel(int dd, UCHAR *pString){
	int Address, Mask, Reg, RxFreq,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	//1. Disable all scans and set intervals and scan windows eually
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0; //All scan disabled
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, buf);	
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}	
	short int inq_scan = 0x1000;		
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = (inq_scan&0xFF);
	buf[1] = ((inq_scan >> 8)& 0xFF);
	buf[2] = (inq_scan&0xFF);
	buf[3] = ((inq_scan >> 8)& 0xFF);
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_INQ_ACTIVITY, 4, buf);	
	if(buf[6] != 0){
		printf("\nWrite inquiry scan activity command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}	
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = (inq_scan&0xFF);
	buf[1] = ((inq_scan >> 8)& 0xFF);
	buf[2] = (inq_scan&0xFF);
	buf[3] = ((inq_scan >> 8)& 0xFF);
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_PAGE_ACTIVITY, 4, buf);	
	if(buf[6] != 0){
		printf("\nWrite page scan activity command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	//2. Disbable AGC
	Address = LC_JTAG_MODEM_REGS_ADDRESS + AGC_BYPASS_ADDRESS;
	Mask = AGC_BYPASS_ENABLE_MASK;
	Reg = AGC_BYPASS_ENABLE_SET(1);
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite to AGC bypass register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 3. Disable frequency hoping and set rx frequency

	RxFreq = (int)pString;
	Address = LC_DEV_PARAM_CTL_ADDRESS;
   	Mask = 	LC_DEV_PARAM_CTL_FREQ_HOP_EN_MASK |
          	LC_DEV_PARAM_CTL_RX_FREQ_MASK     |
          	LC_DEV_PARAM_CTL_WHITEN_EN_MASK;
	Reg = LC_DEV_PARAM_CTL_RX_FREQ_SET(RxFreq);
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);
	
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite to Rx Freq register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 4. Enable page scan only (Note: the old way puts device into inq scan mode only ???)
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 2; //Page scan enabled
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, buf);	
	if(buf[6] != 0){
		printf("\nPage scan enable command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 5. Increase correlator
	Address = LC_JTAG_MODEM_REGS_ADDRESS + CORR_PARAM1_ADDRESS;
	Mask = CORR_PARAM1_TIM_THR_MASK;
	Reg = CORR_PARAM1_TIM_THR_SET(0x3f);
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);
	
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 12, buf);	
	if(buf[6] != 0){
		printf("\nWrite to Correlator register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}

	return TRUE;
}
static const char *cwrx_help =
	"Usage:\n"
	"\n cwrx <Channel>\n";

static void cmd_cwrx(int dev_id, int argc, char **argv){

	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	UCHAR channel;
	BOOL Ok = TRUE;
	if(argc != 2){
		printf("\n%s\n",cwrx_help);	
		return;
	}
	
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	channel = atoi(argv[1]);
	if(channel > 78 || channel < 0){
		printf("\nPlease enter channel 0-78!\n");
		return;	
	}

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}

	// Disable sleep mode
	memset(&buf,0,sizeof(buf));
	buf[0] = 0;
	iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
	if(buf[6] != 0){
		printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
		Ok = 0;
	}
       	printf (" Continuoux Rx at channel %d\n",channel);
	Ok = contRxAtGivenChannel(dd, &channel);

         // All modes come here
	if (Ok)
       	{
       		printf (" Continuoux Rx at channel %d Done...\n",channel);
       	}
       	else
       	{
        	printf ("\nERROR ---> Could not enter continuous Rx mode\n");
  	}

	hci_close_dev(dd);
}

static const char *cmdline_help =
	"\n<BTConfig command v.1.2>"
	"\n1. Usage for TX and Con TX:"
	"\n btconfig cmdline [TestMode|DataPattern|PacketType|DataLen|HopMode|TxFreq|Power|RxFreq"
	"\n [TestMode]"
	"\n		MB_NO_TEST : stop to transmit"
	"\n		MB_RX_TEST : reserve feature"
	"\n		MB_TX_TEST : TX mode"
	"\n		MB_CONT_RX_TEST : reserve feature"
	"\n		MB_CONT_TX_TEST : continuous TX"
	"\n		MB_LE_RX_TEST : reserve feature"
	"\n		MB_LE_TX_TEST : LE TX mode"
	"\n [DataPattern]"
	"\n		0 : eBRM_TestMode_Pause"
	"\n		1 : eBRM_TestMode_TX_0"
	"\n		2 : eBRM_TestMode_TX_1"
	"\n		3 : eBRM_TestMode_TX_1010"
	"\n		4 : eBRM_TestMode_TX_PRBS"
	"\n		5 : eBRM_TestMode_Loop_ACL"
	"\n		6 : eBRM_TestMode_Loop_SCO"
	"\n		7 : eBRM_TestMode_Loop_ACL_No_Whitening"
	"\n		8 : eBRM_TestMode_Loop_SCO_No_Whitening"
	"\n		9 : eBRM_TestMode_TX_11110000"
	//"\n		10: eBRM_TestMode_Rx"	
	//"\n		255 : eBRM_TestMode_Exit"	
	"\n [PacketType] DM1,DH1,DM3,DH3,DM5,DM5,2-DH1,2-DH3,2-DH5,3-DH1,3-DH3,3-DH5"
	"\n [DataLen] length of data"
	"\n [HopMode]"
	"\n 	0 => DISABLE"
	"\n 	1 => ENABLE"
	"\n 	fixed to 0 for Continuous TX(1 MHz tone)"	
	"\n [TxFreq] 0~78(0=2402 MHz, 39=2441MHz, 78=2480MHz)"
	"\n [Power] 1 ~ 8"
	"\n		1 : -20 dbm"
	"\n		2 : -16 dbm"
	"\n		3 : -12 dbm"
	"\n		4 : -8 dbm"
	"\n		5 : -4 dbm"
	"\n		6 : 0 dbm"
	"\n		7 : 4 dbm"
	"\n		8 : 8 dbm"	
	"\n [RxFreq] 0 ~ 78 (0=2402 MHz, 39=2441MHz, 78=2480MHz)"
	"\nExample:"
	"\tbtconfig cmdline MB_TX_TEST 4 DM1 100 0 39 0 39\t"
	"\n		TX Mode|DataPattern = PRBS|DM1|length=100 bytes|Hop OFF|TX ch=39|power=-20dbm|RX ch=39"	
	"\tbtconfig cmdline MB_CONT_TX_TEST 3 DM1 100 0 39 8 39\t"	
	"\n		CONT TX Mode|DataPattern=1010|DM1|length=100bytes|Cont TX ON|TX ch=39|power=8dbm|RX ch=39"
	"\n\n2. Usage for LE TX:"
	"\n btconfig cmdline [TestMode|DataPattern|DataLen|TxFreq|"
	"\n [TestMode]"
	"\n		MB_LE_TX_TEST : LE TX mode"	
	"\n [DataPattern] data pattern from 0 to 7"	
	"\n 	0 => PRBS9"
	"\n 	1 => 11110000"	
	"\n 	2 => 10101010"	
	"\n 	3 => PRBS15"	
	"\n 	4 => 11111111"	
	"\n 	5 => 00000000"	
	"\n 	6 => 00001111"	
	"\n 	7 => 01010101"		
	"\n [DataLen] 0 to 37 bytes"		
	"\n [TxFreq] 0 ~ 39"
	"\nExample:"	
	"\n btconfig cmdline MB_LE_TX_TEST 0 30 20"
	"\n		LE TX mode|DataPattern=PRBS9|length=30bytes|TX ch=20"	
	"\n\n3. Usage for STOP/INITIAL TX:"
	"\n btconfig cmdline [TestMode]"
	"\nExample:"
	"\tbtconfig cmdline MB_NO_TEST\t";

//add by Austin for automatic manufacture tool
static void cmdline(int dev_id, int argc, char **argv){
	int dd,iRet,address,width,value,mask;
	bdaddr_t bdaddr;
	tBRM_Control_packet MasterBlaster;
	UCHAR	SkipRxSlot; 	
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	BOOL TestEnabled = 0,Ok = TRUE;
	UINT8 setContTxType;
	tBtHostInterest	HostInt;
	fd_set master, read_fds;
    struct hci_filter flt;
    struct hci_dev_info di;
    struct timeval timeout;
	char BdAddr[18];

	printf("\nrunning command line");
	if(argc < 2){
		printf("\n%s\n",cmdline_help);	
		return;
	}
	
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);	
	if (buf[6] != 0) {
		printf("\nread bdaddr command failed due to reason 0x%X",buf[6]);
		hci_close_dev(dd);
		return;
	}else{
		printf("\nread bdaddr command successfully");		
	}
	
	int i,j;
	char bda[19];
	for (i=iRet-1,j=0;i>7;i--,j+=3) {
		sprintf(&bda[j],"%X",((buf[i]>>4)&0xFF));
		sprintf(&bda[j+1],"%X",(buf[i]&0x0F));
		sprintf(&bda[j+2],":");
	}
	sprintf(&bda[15],"%X",((buf[7]>>4)&0xFF));
	sprintf(&bda[16],"%X",(buf[7]&0x0F));
	bda[18] ='\0';
	str2ba(bda,&bdaddr);
	printf("\nBDAddr = %s",bda);
	
	InitMasterBlaster(&MasterBlaster, &bdaddr, &SkipRxSlot);
#ifndef DUMP_DEBUG
	Ok = ReadHostInterest(dd,&HostInt);	
	if(Ok) {
		if (HostInt.TpcTableAddr && (HostInt.Version >= 0x0100)) {	
         		Ok = ReadMemoryBlock(dd, HostInt.TpcTableAddr, (UCHAR *)&TpcTable, sizeof(TpcTable));
	         	MasterBlaster.testCtrl.Power = TpcTable.NumOfEntries - 1;
      		}
   	}
   	if(!Ok) {
      		printf ("\nCould not load TPC table.");
		sleep (2);
      		Ok = TRUE;
   	}
#endif

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	
	//======================
	//disable Sleep Mode
	//======================
    memset(&buf,0,sizeof(buf));
    buf[0] = 0;		//disable Sleep mode
    iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
    if(buf[6] != 0) {
        printf("\nError: Sleep mode failed due to reason 0x%X",buf[6]);
    }else{
		printf("\nDisable Sleep mode successfully");
	}	

	//======================
	//Enable TX Mode
	//======================
	if (strcmp("MB_TX_TEST", argv[1]) == 0){
		if(argc < 9){
			printf("\n%s\n",cmdline_help);	
			return;
		}
		printf("\nEnable TX Mode\n");
		
		//==================================
		// check it is under test mode
		//==================================
		Ok = Dut(dd);
		if (!Ok) {
			printf("\nERROR ---> Could not enter DUT mode\n");
		}
			
		memset(&buf,0,HCI_MAX_EVENT_SIZE);	

		buf[0] = atoi(argv[2]);		//Data Pattern
		if (strcmp("DM1", argv[3]) == 0)		//PacketType
			buf[1] = 0x03;
		else if (strcmp("DH1", argv[3]) == 0)	
			buf[1] = 0x04;
		else if (strcmp("DM3", argv[3]) == 0)	
			buf[1] = 0x0A;
		else if (strcmp("DH3", argv[3]) == 0)	
			buf[1] = 0x0B;
		else if (strcmp("DM5", argv[3]) == 0)	
			buf[1] = 0x0E;
		else if (strcmp("DH5", argv[3]) == 0)	
			buf[1] = 0x0F;
		else if (strcmp("2-DH1", argv[3]) == 0)	
			buf[1] = 0x24;	
		else if (strcmp("2-DH3", argv[3]) == 0)	
			buf[1] = 0x2A;			
		else if (strcmp("2-DH5", argv[3]) == 0)	
			buf[1] = 0x2E;			
		else if (strcmp("3-DH1", argv[3]) == 0)	
			buf[1] = 0x28;	
		else if (strcmp("3-DH3", argv[3]) == 0)	
			buf[1] = 0x2B;			
		else if (strcmp("3-DH5", argv[3]) == 0)	
			buf[1] = 0x2F;		
		buf[2] = (atoi(argv[4]) & 0xFF);		//DataLen
		buf[3] = (atoi(argv[4])>>8 & 0xFF);		//DataLen
		buf[4] = atoi(argv[5]);		//HopMode
		buf[5] = atoi(argv[6]);		//TxFreq
		buf[6] = atoi(argv[7]);		//Power
		buf[7] = atoi(argv[8]);		//RxFreq		
		buf[8] = MasterBlaster.bdaddr[0]; 
		buf[9] = MasterBlaster.bdaddr[1]; 
		buf[10] = MasterBlaster.bdaddr[2]; 
		buf[11] = MasterBlaster.bdaddr[3]; 
		buf[12] = MasterBlaster.bdaddr[4]; 
		buf[13] = MasterBlaster.bdaddr[5]; 
		buf[14] = SkipRxSlot;

		ba2str((const bdaddr_t *)MasterBlaster.bdaddr, BdAddr); 	
		
		if (strcmp("0", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Pause |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("1", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=all 0 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("2", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=all 1 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("3", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=1010 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("4", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=PRBS |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("5", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_ACL |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("6", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_SCO |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("7", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_ACL_No_Whitening |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("8", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_SCO_No_Whitening |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("9", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=11110000 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
			
		iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_TX_TESTER, 15, buf);	
		if (buf[6] != 0) {
			printf("\nTx Tester command failed due to reason 0x%X",buf[6]);
			printf("\nERROR --> Could not enable master blaster mode");
			TestEnabled = MB_NO_TEST;
			Ok = 0;
		} else {
			printf("\ntx test is in progress. Press type 'btconfig cmdline MB_NO_TEST' to stop the test");
			TestEnabled = MB_TX_TEST;
		}
	}
	//===========================
	//Enable Continuous TX Mode
	//===========================	
	else if (strcmp("MB_CONT_TX_TEST", argv[1]) == 0){
		if(argc < 9){
			printf("\n%s\n",cmdline_help);	
			return;
		}	
		printf("\nEnable Continuous TX Mode");
		
		Ok = Dut(dd);
		if (!Ok) 
			printf("\nERROR ---> Could not enter DUT mode");

		/* Enable master blaster mode */
		if (CW_Single_Tone == MasterBlaster.ContTxType)
			setContTxType = Cont_Tx_Raw_1MHz;
		else
			setContTxType = MasterBlaster.ContTxType;
					
		memset(&buf, 0, HCI_MAX_EVENT_SIZE);
		buf[0] = atoi(argv[2]);		//Data Pattern
		if (strcmp("DM1", argv[3]) == 0)		//PacketType
			buf[1] = 0x03;
		else if (strcmp("DH1", argv[3]) == 0)	
			buf[1] = 0x04;
		else if (strcmp("DM3", argv[3]) == 0)	
			buf[1] = 0x0A;
		else if (strcmp("DH3", argv[3]) == 0)	
			buf[1] = 0x0B;
		else if (strcmp("DM5", argv[3]) == 0)	
			buf[1] = 0x0E;
		else if (strcmp("DH5", argv[3]) == 0)	
			buf[1] = 0x0F;
		else if (strcmp("2-DH1", argv[3]) == 0)	
			buf[1] = 0x24;	
		else if (strcmp("2-DH3", argv[3]) == 0)	
			buf[1] = 0x2A;			
		else if (strcmp("2-DH5", argv[3]) == 0)	
			buf[1] = 0x2E;			
		else if (strcmp("3-DH1", argv[3]) == 0)	
			buf[1] = 0x28;	
		else if (strcmp("3-DH3", argv[3]) == 0)	
			buf[1] = 0x2B;			
		else if (strcmp("3-DH5", argv[3]) == 0)	
			buf[1] = 0x2F;		
		buf[2] = (atoi(argv[4]) & 0xFF);		//DataLen
		buf[3] = (atoi(argv[4])>>8 & 0xFF);		//DataLen
		buf[4] = atoi(argv[5]);		//Continuous TX which is fixed to "Cont_Tx_Raw_1MHz"
		buf[5] = atoi(argv[6]);		//TxFreq
		buf[6] = atoi(argv[7]);		//Power
		buf[7] = atoi(argv[8]);		//RxFreq		
		buf[8] = MasterBlaster.bdaddr[0]; 
		buf[9] = MasterBlaster.bdaddr[1]; 
		buf[10] = MasterBlaster.bdaddr[2]; 
		buf[11] = MasterBlaster.bdaddr[3]; 
		buf[12] = MasterBlaster.bdaddr[4]; 
		buf[13] = MasterBlaster.bdaddr[5]; 
		ba2str((const bdaddr_t *)MasterBlaster.bdaddr, BdAddr); 	
	
		if (strcmp("0", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Pause |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("1", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=all 0 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("2", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=all 1 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("3", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=1010 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("4", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=PRBS |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("5", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_ACL |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("6", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_SCO |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("7", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_ACL_No_Whitening |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("8", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=Loop_SCO_No_Whitening |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		if (strcmp("9", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=11110000 |PacketType=%s | DataLen=%d | HopMode=%d | TxFreq=%d | Power level=%d | RxFreq=%d | BdAddr=0x%s", argv[1], argv[3], atoi(argv[4]), buf[4], buf[5], buf[6], buf[7], BdAddr);	
		
		iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_CONT_TX_TESTER, 14, buf);	
		if(buf[6] != 0){
			printf("\nContinious Tx Tester command failed due to reason 0x%X",buf[6]);
			Ok = FALSE;
		} else
			Ok = TRUE;
        memset(&buf,0,HCI_MAX_EVENT_SIZE);
       	address = 0x00022914;
		value = 0x00200000;
		mask = 0x00200000;
		width = 4;
		buf[0] = (address & 0xFF);
		buf[1] = ((address >>8) & 0xFF);
		buf[2] = ((address>>16) & 0xFF);
		buf[3] = ((address>>24) & 0xFF);
		buf[4] = width;  //Memory width
		buf[5] = (value & 0xFF);
		buf[6] = ((value >> 8) & 0xFF);
		buf[7] = ((value >> 16) & 0xFF);
		buf[8] = ((value >> 24) & 0xFF);
		buf[9] = (mask & 0xFF);
		buf[10] = ((mask >>8) & 0xFF);
		buf[11] = ((mask>>16) & 0xFF);
		buf[12] = ((mask>>24) & 0xFF);
		iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
		if(buf[6] != 0){
			printf("\nWrite memory address failed due to reason 0x%X",buf[6]);
			Ok = FALSE;
		} else 
			Ok = TRUE;
		TestEnabled = MB_CONT_TX_TEST;
		if (Ok) {
			printf("\nContinuous Test is in progress. Press type 'btconfig cmdline MB_NO_TEST' to stop the test");
		} else {
			printf("\nERROR ---> Could not enable master blaster mode");
			TestEnabled = MB_NO_TEST;
		}
	}
	//======================
	//Enable LE TX Mode
	//======================	
	else if (strcmp("MB_LE_TX_TEST", argv[1]) == 0){
		if(argc < 5){
			printf("\n%s\n",cmdline_help);	
			return;
		}	
		printf("\nEnable LE TX Mode");

		buf[0] = atoi(argv[2]);		//Data Pattern
		buf[2] = atoi(argv[3]);		//Data Length
		buf[5] = atoi(argv[4]);		//TX Freq		
			
		if (strcmp("0", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=PRBS9 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("1", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=11110000 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("2", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=10101010 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("3", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=PRBS15 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("4", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=11111111 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("5", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=00000000 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("6", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=00001111 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
		else if (strcmp("7", argv[2]) == 0)
			printf("\nTestMode=%s | DataPattern=01010101 | DataLen=%s | TxFreq=%s", argv[1], argv[3], argv[4]);
			
		//BOOL SU_LETxTest(int dev_id, UCHAR channel, UCHAR length, UCHAR payload);
		Ok = SU_LETxTest(dd, buf[5], buf[2], buf[0]);		
		
		if (Ok) {
			printf("\nLE Test is in progress. Press type 'btconfig cmdline MB_NO_TEST' to stop the test");
			TestEnabled = MB_LE_TX_TEST;	
		} else {
			printf("\nERROR ---> Could not enable master blaster mode");
			TestEnabled = MB_NO_TEST;
		}		
	}
	//=========================
	//Stop to transmit package
	//=========================	
	else if (strcmp("MB_NO_TEST", argv[1]) == 0){
		if(argc > 2){
			printf("\n%s\n",cmdline_help);	
			return;
		}	
		printf("\nStop to transmit package");
		
		//================================================
		//set BT to Sleep mode which is default setting
		//================================================
		memset(&buf,0,HCI_MAX_EVENT_SIZE);	
		buf[0] = 1;
		iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
		if(buf[6] != 0) {
				printf("\nError: Sleep mode failed due to reason 0x%X",buf[6]);
		}else{
			printf("\nEnable Sleep mode successfully");
		}			
		
		//=================================================================
		//send "HCI Reset" command to terminate the package transmission
		//=================================================================
		memset(&buf,0,sizeof(buf));
		iRet = writeHciCommand(dd,OGF_HOST_CTL,OCF_RESET,0,buf);
		if (buf[6] != 0) {
			printf("\nError: HCI RESET failed due to reason 0x%X",buf[6]);
			Ok = FALSE;
		} else
			Ok = TRUE;
		if (!Ok) {
			printf ("\nERROR ---> Could not stop test mode");
		}				
		TestEnabled = MB_NO_TEST;	
	}else{
		printf("\nWrong command");
	}
	
	hci_close_dev(dd);
	printf("\nDone\n");
}
	
static const char *mb_help =
	"Usage:\n"
	"\n mb\n";
	
static void cmd_mb(int dev_id, int argc, char **argv){

	int newfd, FieldNum,dd,iRet,need_raw, iDataSize, address,width,value,mask, fdmax, k, l;
	bdaddr_t bdaddr;
	tBRM_Control_packet MasterBlaster;
	UCHAR	SkipRxSlot; 	
	UCHAR buf[HCI_MAX_ACL_SIZE];
	char FieldAlias;
	BOOL TestEnabled = 0,Ok = TRUE;
	UINT8 setContTxType;
	tBtHostInterest	HostInt;
	fd_set master, read_fds;
    uint32_t m_BerTotalBits, m_BerGoodBits;
    uint8_t m_pattern[16];
    uint16_t m_pPatternlength;
    struct hci_filter flt;
    struct hci_dev_info di;
    struct timeval timeout;
    int bytesRead=0;
    int err;

	if(argc > 1) {
		printf("\n%s\n",mb_help);	
		return;
	}
	
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);	
	if (buf[6] != 0) {
		printf("\nread bdaddr command failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);
		return;
	}
	int i,j;
	char bda[19];
	for (i=iRet-1,j=0;i>7;i--,j+=3) {
		sprintf(&bda[j],"%X",((buf[i]>>4)&0xFF));
		sprintf(&bda[j+1],"%X",(buf[i]&0x0F));
		sprintf(&bda[j+2],":");
	}
	sprintf(&bda[15],"%X",((buf[7]>>4)&0xFF));
	sprintf(&bda[16],"%X",(buf[7]&0x0F));
	bda[18] ='\0';
	str2ba(bda,&bdaddr);
	
	InitMasterBlaster(&MasterBlaster, &bdaddr, &SkipRxSlot);
#ifndef DUMP_DEBUG
	Ok = ReadHostInterest(dd,&HostInt);	
	if(Ok) {
		if (HostInt.TpcTableAddr && (HostInt.Version >= 0x0100)) {	
         		Ok = ReadMemoryBlock(dd, HostInt.TpcTableAddr, (UCHAR *)&TpcTable, sizeof(TpcTable));
	         	MasterBlaster.testCtrl.Power = TpcTable.NumOfEntries - 1;
      		}
   	}
   	if(!Ok) {
      		printf ("\nCould not load TPC table.\n");
		sleep (2);
      		Ok = TRUE;
   	}
#endif

	PrintMasterBlasterMenu (&MasterBlaster);
    m_BerGoodBits = 0;
    m_BerTotalBits = 0;
    m_pattern[0] = 0x0f;
    m_pPatternlength = 1;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	fdmax = 0;
	newfd = -1;

   while (1) {
        read_fds = master;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        iRet = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
        if (iRet == -1) {
            perror("cmd_mb select() error!");
            goto exits_mb;
        }
        if (iRet == 0) continue;

        for(i = 0; i <= fdmax; i++) {
	    if(FD_ISSET(i, &read_fds)) {
	    	if (i==0) {	// input
		    scanf("%s",buf);
		    FieldAlias = (char)buf[0];
		    FieldNum = CheckField(MasterBlaster, &FieldAlias);
		    if (FieldNum == INVALID_MASTERBLASTER_FIELD) {
			printf ("\nERROR ---> Invalid command. Try again.\n");
		        printf ("mb>");
			continue;
      		    }

		    if (!strncmp(&FieldAlias, MasterBlasterMenu[EXX].Alias, 1)) {
			printf("\nExit the Master Blaster Mode without reset\n");
			goto exits_mb;
		    }

	            // if the test is in rx and the key is neither 'd' nor 'g', then stop the test, renew the option, and procced
		    // if the test is in tx and the key is not 'e', then stop the test, renew the option, and procced
		    // if the test is in (LE) continuous rx/tx and the key is not 'j' , then stop the test, renew the option, and procced
		    if (((TestEnabled == MB_RX_TEST) && strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && strncmp(&FieldAlias, MasterBlasterMenu[GB].Alias, 1))
		    	|| ((TestEnabled == MB_TX_TEST) && strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1))
		    	|| ((TestEnabled == MB_CONT_RX_TEST || TestEnabled == MB_CONT_TX_TEST
		        || TestEnabled == MB_LE_RX_TEST || TestEnabled == MB_LE_TX_TEST) &&
		       	(strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)))) {
			printf (" ... Please wait ...");
         		if (MasterBlaster.ContTxMode) {
		        	memset(&buf,0,HCI_MAX_EVENT_SIZE);
				buf[0] = 255;
				iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_CONT_TX_TESTER, 14, buf);	
				if(buf[6] != 0) {
					printf("\nContinious Tx Tester command failed due to reason 0x%X\n",buf[6]);
					Ok = 0;
				} else
					Ok = TRUE;
         		}
			if (TestEnabled == MB_RX_TEST) {
                       	    if (need_raw) {
                            	if (ioctl(dd, HCISETRAW, 0) < 0)
                                    perror("Can't clear raw mode \n");
			    }
                       	    FD_CLR(newfd, &master);
                            newfd = -1;
			    fdmax = 0;
			}
		        // Ok = Diag::Reset (Unit, "");
			//PSInit(dd);
			// The default setting is sleep mode enabled.
                        memset(&buf,0,sizeof(buf));
                        buf[0] = 1;
                        iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
                        if(buf[6] != 0) {
                                printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
                        }

			memset(&buf,0,sizeof(buf));
			iRet = writeHciCommand(dd,OGF_HOST_CTL,OCF_RESET,0,buf);
			if (buf[6] != 0) {
				printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
				Ok = FALSE;
			} else
				Ok = TRUE;
         		if (!Ok) {
		            printf ("\nERROR ---> Could not stop test mode\n");
         		} else if (!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1)
				|| !strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1)
				|| ((TestEnabled != MB_NO_TEST) &&
				(!strncmp(&FieldAlias, MasterBlasterMenu[CR].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[CT].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[LR].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[LT].Alias, 1))) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) {
				TestEnabled = MB_NO_TEST;
			}
	      		sleep(1);
		    } 
		    if (!strncmp(&FieldAlias,MasterBlasterMenu[EX].Alias,1)) {			// Exit
		        TestEnabled = MB_NO_TEST;
		        printf ("\n Exit ..\n");
		    	goto exits_mb;
      		    } else if (!strncmp(&FieldAlias,MasterBlasterMenu[ST].Alias,1)) {		// Stop Test
			TestEnabled = MB_NO_TEST;
		        PrintMasterBlasterMenu (&MasterBlaster);
			continue;
		    } else if (!strncmp(&FieldAlias,MasterBlasterMenu[GB].Alias,1)) {		// get BER
		        printf("\n\tGoodBits %d, total is %d\n", m_BerGoodBits, m_BerTotalBits);
		        printf("mb>\n");
		        continue;
		    } else if (!strncmp(&FieldAlias,MasterBlasterMenu[PO].Alias,1)) {		// set Power
			MasterBlasterMenu[FieldNum].pFunc (&MasterBlaster, &FieldAlias);
      		    } else if (!MasterBlasterMenu[FieldNum].pFunc (&MasterBlaster, MasterBlasterMenu[FieldNum].Options)) {
			printf ("\nERROR ---> Invalid option. Try again.\n");
		        printf ("mb>");
		        continue;
      		    }
		    PrintMasterBlasterMenu(&MasterBlaster);

                // Enable RX test mode
                    if ((!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && (TestEnabled == MB_NO_TEST))		
                    	|| (strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && (TestEnabled == MB_RX_TEST))) {
                        Ok = Dut(dd);
                        if (!Ok) {
                                printf("\nERROR ---> Could not enter DUT mode\n");
                                printf("mb>");
                                continue;
                        }

                        printf(".");
    			if (hci_devinfo(dev_id, &di) < 0) {
        			printf("Can't get device info\n");
				printf("mb>");
				continue;
			}

			need_raw = 0; //!hci_test_bit(HCI_RAW, &di.flags);

                        memset(&buf,0,HCI_MAX_EVENT_SIZE);
                        buf[0] = eBRM_TestMode_Rx;
                        buf[1] = MasterBlaster.testCtrl.Packet;
                        buf[2] = MasterBlaster.testCtrl.DataLen & 0xFF;
                        buf[3] = ((MasterBlaster.testCtrl.DataLen>>8) & 0xFF);
                        buf[4] = MasterBlaster.testCtrl.HopMode;
                        buf[5] = MasterBlaster.testCtrl.TxFreq;
                        buf[6] = MasterBlaster.testCtrl.Power;
                        buf[7] = MasterBlaster.testCtrl.RxFreq;
                        buf[8] = MasterBlaster.bdaddr[0];
                        buf[9] = MasterBlaster.bdaddr[1];
                        buf[10] = MasterBlaster.bdaddr[2];
                        buf[11] = MasterBlaster.bdaddr[3];
                        buf[12] = MasterBlaster.bdaddr[4];
                        buf[13] = MasterBlaster.bdaddr[5];
                        buf[14] = SkipRxSlot;
                        iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_RX_TESTER, 15, buf);
                        if (buf[6] != 0) {
                                printf("\nRx Tester command failed due to reason 0x%X\n",buf[6]);
                                printf("\nERROR --> Could not enable master blaster mode\n");
                                TestEnabled = MB_NO_TEST;
                                Ok = 0;
                        } else {
                            printf(" rx test is in progress. Press 's' to stop the test\n");
                            TestEnabled = MB_RX_TEST;
			    hci_filter_clear(&flt);
                            hci_filter_all_ptypes(&flt);
                            hci_filter_all_events(&flt);

                            if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
                                printf("Can't set filter for hci\n");
                                printf("mb>");
                                continue;
                            }

                            if (need_raw) {
                                if ((err = ioctl(dd, HCISETRAW, 1) < 0)) {
                                    printf("Can't set raw mode on hci %d\n",err);
                                    printf("mb>");
				    continue;
                            	}
                            }
                            newfd = dd;
			    FD_SET(newfd, &master);
                            fdmax = dd;
                        }
                        printf("mb>");
                        continue;
                    } else if ((!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1)) && (TestEnabled == MB_RX_TEST)) {
                        printf(" rx test is in progress. Press 's' to stop the test\n");
                        printf("mb>");
                        continue;
                    }

      		// Enable TX test mode
		    if ((!strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1) && (TestEnabled == MB_NO_TEST))
		    	|| (strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1) && (TestEnabled == MB_TX_TEST))) {
		        // Disable sleep mode
	        	printf (".");
			Ok = TRUE;
			memset(&buf,0,sizeof(buf));
			buf[0] = 0;
			iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
			if(buf[6] != 0) {
				printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
				Ok = 0;
			}
	
			if (!Ok) {
            			printf ("\nERROR ---> Could not disable sleep mode\n");
			        printf ("mb>");
        	    		continue;
         		}

	        	printf (".");
			Ok = Dut(dd);
			if (!Ok) {
				printf("\nERROR ---> Could not enter DUT mode\n");
				printf("mb>");
				continue;
			}

			printf(".");
			memset(&buf,0,HCI_MAX_EVENT_SIZE);
			buf[0] = MasterBlaster.testCtrl.Mode ;
			buf[1] = MasterBlaster.testCtrl.Packet;
			buf[2] = MasterBlaster.testCtrl.DataLen & 0xFF; 
			buf[3] = ((MasterBlaster.testCtrl.DataLen>>8) & 0xFF); 
			buf[4] = MasterBlaster.testCtrl.HopMode;
			buf[5] = MasterBlaster.testCtrl.TxFreq;
			buf[6] = MasterBlaster.testCtrl.Power;
			buf[7] = MasterBlaster.testCtrl.RxFreq;
			buf[8] = MasterBlaster.bdaddr[0]; 
			buf[9] = MasterBlaster.bdaddr[1]; 
			buf[10] = MasterBlaster.bdaddr[2]; 
			buf[11] = MasterBlaster.bdaddr[3]; 
			buf[12] = MasterBlaster.bdaddr[4]; 
			buf[13] = MasterBlaster.bdaddr[5]; 
			buf[14] = SkipRxSlot;
			
			iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_TX_TESTER, 15, buf);	
			if (buf[6] != 0) {
				printf("\nTx Tester command failed due to reason 0x%X\n",buf[6]);
				printf("\nERROR --> Could not enable master blaster mode\n");
				TestEnabled = MB_NO_TEST;
				Ok = 0;
			} else {
				printf(" tx test is in progress. Press 's' to stop the test\n");
				TestEnabled = MB_TX_TEST;
			}
			printf("mb>");
			continue;
		    } else if ((!strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1)) && TestEnabled == MB_TX_TEST) {
			printf(" tx test is in progress. Press 's' to stop the test\n");
			printf("mb>");
			continue;
		    }

		/* Enable (LE) continuous tx/rx test modes */
		    if (((!strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) && (TestEnabled == MB_NO_TEST))
		    	|| (strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1) && (TestEnabled == MB_CONT_RX_TEST
			|| TestEnabled == MB_CONT_TX_TEST || TestEnabled == MB_LE_RX_TEST || TestEnabled == MB_LE_TX_TEST))) {
		        // Disable sleep mode
	        	printf (".");
			Ok = TRUE;
			memset(&buf,0,sizeof(buf));
			buf[0] = 0;
			iRet = writeHciCommand(dd,OGF_VENDOR_CMD,OCF_SLEEP_MODE,1,buf);
			if (buf[6] != 0) {
				printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
				Ok = 0;
			}
	
			if (!Ok) {
            			printf ("\nERROR ---> Could not disable sleep mode\n");
			        printf ("mb>");
        	    		continue;
         		}

			/*  LE Rx Mode */
			if (MasterBlaster.LERxMode == ENABLE) {
				Ok = SU_LERxTest(dd, MasterBlaster.testCtrl.RxFreq);
				TestEnabled = MB_LE_RX_TEST;
			} else if (MasterBlaster.LETxMode == ENABLE) {
				Ok = SU_LETxTest(dd, MasterBlaster.testCtrl.TxFreq, MasterBlaster.testCtrl.DataLen,
						MasterBlaster.LETxParms.PktPayload);
				TestEnabled = MB_LE_TX_TEST;
			} else if (MasterBlaster.ContRxMode == ENABLE) {
				UCHAR RxFreq = MasterBlaster.testCtrl.RxFreq;
				Ok = contRxAtGivenChannel(dd, &RxFreq);
				TestEnabled = MB_CONT_RX_TEST;
			} else /* Continous TX mode */ {
				printf(".");
				Ok = Dut(dd);
				if (!Ok) {
					printf("\nERROR ---> Could not enter DUT mode\n");
					printf("mb>");
					continue;
				}
				/* Enable master blaster mode */
				printf(".");
				if (CW_Single_Tone == MasterBlaster.ContTxType)
					setContTxType = Cont_Tx_Raw_1MHz;
				else
					setContTxType = MasterBlaster.ContTxType;
					
				memset(&buf, 0, HCI_MAX_EVENT_SIZE);
				buf[0] = MasterBlaster.testCtrl.Mode ;
				buf[1] = MasterBlaster.testCtrl.Packet;
				buf[2] = MasterBlaster.testCtrl.DataLen & 0xFF; 
				buf[3] = ((MasterBlaster.testCtrl.DataLen>>8) & 0xFF); 
				buf[4] = MasterBlaster.ContTxType;
				buf[5] = MasterBlaster.testCtrl.TxFreq;
				buf[6] = MasterBlaster.testCtrl.Power;
				buf[7] = MasterBlaster.testCtrl.RxFreq;
				buf[8] = MasterBlaster.bdaddr[0]; 
				buf[9] = MasterBlaster.bdaddr[1]; 
				buf[10] = MasterBlaster.bdaddr[2]; 
				buf[11] = MasterBlaster.bdaddr[3]; 
				buf[12] = MasterBlaster.bdaddr[4]; 
				buf[13] = MasterBlaster.bdaddr[5]; 
				iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_CONT_TX_TESTER, 14, buf);	
				if(buf[6] != 0){
					printf("\nContinious Tx Tester command failed due to reason 0x%X\n",buf[6]);
					Ok = FALSE;
				} else
					Ok = TRUE;

                                memset(&buf,0,HCI_MAX_EVENT_SIZE);
                        	address = 0x00022914;
                        	value = 0x00200000;
                        	mask = 0x00200000;
                    		width = 4;
                        	buf[0] = (address & 0xFF);
                        	buf[1] = ((address >>8) & 0xFF);
                        	buf[2] = ((address>>16) & 0xFF);
                        	buf[3] = ((address>>24) & 0xFF);
                        	buf[4] = width;  //Memory width
                        	buf[5] = (value & 0xFF);
                        	buf[6] = ((value >> 8) & 0xFF);
                        	buf[7] = ((value >> 16) & 0xFF);
                        	buf[8] = ((value >> 24) & 0xFF);
                        	buf[9] = (mask & 0xFF);
                        	buf[10] = ((mask >>8) & 0xFF);
                        	buf[11] = ((mask>>16) & 0xFF);
                        	buf[12] = ((mask>>24) & 0xFF);
                        	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
                        	if(buf[6] != 0){
                        		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
                        		Ok = FALSE;
				} else 
					Ok = TRUE;

				TestEnabled = MB_CONT_TX_TEST;
			}
			if (Ok) {
				printf("Test is in progress. Press 's' to stop the test\n");
			} else {
				printf("\nERROR ---> Could not enable master blaster mode\n");
				TestEnabled = MB_NO_TEST;
			}
			printf("mb>");
			continue;
		    } else if ((!strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) && TestEnabled) {
		        printf (" Test mode is in progress. Press 's' to stop the test\n");
			printf ("mb>");
			continue;
		    }
	        }
		else if (i == newfd) {
		    bytesRead += read(i, buf+bytesRead, sizeof(buf)-bytesRead);
		    if(buf[0]!= 0x2) {
		       PRINTD("OUT OF SYNC\n");
		       iRet = bytesRead;
		       bytesRead = 0;
		       continue;
		    }
		    if(bytesRead < (buf[3] | (buf[4] << 8)) && bytesRead < sizeof(buf)) {
		       PRINTD("read %d bytes, reading more\n", bytesRead);
		       continue;
		    }
		    else {
		       iRet = bytesRead;
		       bytesRead = 0;
                    }

                    iDataSize = iRet - 5;
		    PRINTD("b[0]=%2x\tb[1]=%2x\tb[2]=%2x\tb[3]=%2x\tb[4]=%2x\n", buf[0],buf[1],buf[2],buf[3],buf[4]);
		    PRINTD("first:%x,nbyte:%d, packet:%d, pattern:%x\n",buf[0], iRet, (uint16_t)(buf[3] | (buf[4] << 8)), buf[5]);
                    if (buf[0] == 0x2) {        // ACL data
                            m_BerTotalBits = m_BerTotalBits + iDataSize * 8;
                            for(j=0,l=0;j<iDataSize;j++,l++) {
                                if (l == m_pPatternlength) l = 0;
                                for(k=0;k<8;k++){
                                    if((m_pattern[l]&(1<<k)) == (buf[5+j]&(1<<k)))
                                    m_BerGoodBits++;
                                }
				PRINTD("byte#:%d, byet:%x, pattern:%x\n", l, buf[5+j],m_pattern[l] );
                            }
                    }
		}
	    }
	}
   }
exits_mb:
        if (need_raw) {
                if (ioctl(dd, HCISETRAW, 0) < 0)
                        perror("Can't clear raw mode \n");
        }
	hci_close_dev(dd);
}
/*
static void cmd_gid(int dev_id, int argc, char **argv){
	printf("\nFeature not implemented\n");
}
*/
static const char *wsm_help =
	"Usage:\n"
	"\n wsm [0|1|2|3]\n"
	"\nExample:\n"
	"\twsm 0\t(Scan disabled)\n"
	"\twsm 1\t(Inquiry scan enabled)\n"
	"\twsm 2\t(Page scan enabled)\n"
	"\twsm 3\t(Inquiry and Page scan enabled)\n";

static void cmd_wsm(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if(argc < 2){
		printf("\n%s\n",wsm_help);	
		return;
	}
	if(atoi(argv[1]) > 3){
		printf("\nInvalid scan mode :%d\n",atoi(argv[1]));
		return;
	}

	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = atoi(argv[1]);
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, buf);	
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return;
	}
	hci_close_dev(dd);
	printf("\nScan Mode set to :%d\n",atoi(argv[1]));
}

static void dumpHex(UCHAR *buf, int length, int col)
{
	int i;
	for (i = 0; i < length; i++) {
		printf("0x%02x ", buf[i]);
		if (((i+1) % col) == 0 && i != 0)
			printf("\n");
	}
	if (((i+1) % col) != 0) printf("\n");
}

static void ReverseHexString(char *pStr)
{
	int i, j;
	char temp;
	int len = strlen(pStr);

	for (i = 0; pStr[i] == ' ' || pStr[i] == '\t'; i++);

	if (pStr[i] == '0' && pStr[i+1] == 'x')
		i += 2;
	
	for (j = len - 1; i < j - 2; i += 2, j -= 2) {
		temp = pStr[i];
		pStr[i] = pStr[j - 1];
		pStr[j - 1] = temp;
		temp = pStr[i + 1];
		pStr[i + 1] = pStr[j];
		pStr[j] = temp;
	}
}

static void GetByteSeq(UCHAR *pDst, UCHAR *pSrc, int Size)
{
	UCHAR LowNibble, Nibble = 0;
	UCHAR *pLastHex;
	UCHAR *pStr = pSrc;

	while (*pStr == ' ' || *pStr == '\t') pStr++;

	if ((pStr[0] == '0') && (pStr[1] == 'x'))
		pStr += 2;

	pLastHex = pStr - 1;
	while (IS_HEX(*(pLastHex + 1)))
		pLastHex++;

	LowNibble = 0;

	while (Size > 0) {
		if (pStr <= pLastHex) {
			Nibble = CONV_HEX_DIGIT_TO_VALUE(*pStr);
			pStr++;
		} else {
			Nibble = 0;
		}

		if (LowNibble) {
			*pDst |= (UCHAR)(Nibble & 0x0F);
			LowNibble = 0;
			pDst++;
			Size--;
		} else {
			*pDst = (UCHAR)((Nibble << 4) & 0xF0);
			LowNibble = 1;
		}
	}
}

unsigned int GetUInt(char **ppLine, unsigned int DefaultValue)
{
  char *pStr = *ppLine;
  unsigned int Value = 0;

  // Is it a hex value?
  if ((*pStr == '0') && (*(pStr+1) == 'x'))
  {
    // We have a hex value

    pStr += 2;

    while (IS_HEX(*pStr))
    {
      Value = CONV_HEX_DIGIT_TO_VALUE(*pStr) + (Value*16);
      pStr++;
    }

  }
  else if (IS_DIGIT(*pStr))
  {
    // We have a decimal value
    while (IS_DIGIT(*pStr))
    {
      Value = CONV_DEC_DIGIT_TO_VALUE(*pStr) + (Value*10);
      pStr++;
    }
  }
  else
  {
    // We don't have a value at all - return default value
    return DefaultValue;
  }

  // Update the BtString ptr
  *ppLine = pStr;
  return Value;
}

static const char *mbr_help =
	"Usage:\n"
	"\n mbr <address> <length> \n"
	"\n Example \n"
	"\n mbr 0x00004FFC 10 \n"
	"\n mbr 0x00004FFC 0x10 \n";

static void cmd_mbr(int dev_id, int argc, char **argv){
	int dd;
	UCHAR buf[HCI_MAX_EVENT_SIZE*20];

	if(argc != 3){
		printf("\n%s\n",mbr_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}

        int length = GetUInt(&(argv[2]),0);
        int address  = GetUInt(&(argv[1]),0);

	if ((address == 0) || (length==0)){
		return;
	}
		memset(&buf,0,HCI_MAX_EVENT_SIZE*20);
    	if(!MemBlkRead(dd,address,buf, length)) {
        	printf("\nmemory bulk read command failed\n");
		return;
    	}
	printf("\ndata: \n");
	int i;
	for(i=0;i < length;i+=4){
        printf("%08X: ",address+i);
		printf("%08X",*((int*)(buf+i)));
        printf("\n");
	}
        printf("\n");
	
	hci_close_dev(dd);

}

static const char *psr_help =
	"Usage:\n"
	"\n psr \n";

static void cmd_psr(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if(argc > 1){
		printf("\n%s\n",psr_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	LoadPSHeader(buf,PS_RESET,0,0);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER+2, buf);
        if(buf[7] != 0){ /* Check for status */
		printf("\n PS Reset failed\n");
		hci_close_dev(dd);	
		return;
	}
	hci_close_dev(dd);
        printf("PS reset done\n");	
}

static const char *rpst_help =
	"Usage:\n"
	"\n rpst <tag id> <tag length> \n"
	"\n Example:\n"
	"\n rpst 1 6 \n";
static void cmd_rpst(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int tag_id,tag_len,i,j;
	if(argc != 3){
		printf("\n%s\n",rpst_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	tag_id = GetUInt(&(argv[1]),0);
	tag_len = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_READ,tag_len,tag_id);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\n read PS tag failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}

	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = read(dd,&buf,HCI_MAX_EVENT_SIZE);
	if(iRet < 0){
		printf("\n read PS tag failed\n");
		hci_close_dev(dd);	
		return;
	}
	printf("\nTag ID :%X\nTag Length:%X\nTag Data:\n",tag_id,tag_len);
	
	for(i=4,j=1;i<iRet;i++,j++){
		printf("%02X ",buf[i]);
		if(j%16 == 0)
			printf("\n");
	}
	printf("\n\n");
	hci_close_dev(dd);	
}
static const char *wpst_help =
	"Usage:\n"
	"\n wpst <tag id> <tag length> <tag data>\n"
	"\n Example:\n"
	"\n wpst 1 6 00 03 F4 55 AB 77 \n";

static void cmd_wpst(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int tag_id,tag_len,i;
	if(argc < 4){
		printf("\n%s\n",wpst_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	tag_id = GetUInt(&(argv[1]),0);
	tag_len = GetUInt(&(argv[2]),0);
	if(argc < tag_len+3){
		printf("\n Tag Data is less than Tag Length\n");
		hci_close_dev(dd);	
		return;
	}
	LoadPSHeader(buf,PS_WRITE,tag_len,tag_id);
	for(i=0;i<tag_len;i++){
		buf[i+4] = strtol(argv[i+3], NULL, 16);
	}
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER + tag_len, buf);
	if(buf[6] != 0){
		printf("\n Write PS tag failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	hci_close_dev(dd);	
}

static const char *setam_help =
	"Usage:\n"
	"\nsetam <storage medium> <access mode>\n"
	"\nstorage medium: 0-RAM  1-EEPROM\n"
	"\naccess mode: 0-Read-only 1-Write-only 2-Read-Write 3- Disabled\n"
	"\nExample:\n"
	"\nsetam 0 3\n";
static void cmd_setam(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int medium,mode;
	if(argc !=3){
		printf("\n%s\n",setam_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	medium = GetUInt(&(argv[1]),0);
	mode = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_SET_ACCESS_MODE,mode,medium);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\nSet Access mode failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	hci_close_dev(dd);
	printf("\nAccess mode changed successfully!\n");	
}

static const char *setap_help =
	"Usage:\n"
	"\nsetap <storage medium> <priority>\n"
	"\nstorage medium: 0-RAM  1-EEPROM\n"
	"\npriority: #Highest number corresponds to highest priority\n"
	"\nExample:\n"
	"\nsetap 0 1\n";

static void cmd_setap(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int medium,priority;
	if(argc !=3){
		printf("\n%s\n",setap_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	medium = GetUInt(&(argv[1]),0);
	priority = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_SET_ACCESS_MODE,priority,medium);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\nSet Access priority failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	hci_close_dev(dd);
	printf("\nPriority changed successfully!\n");
}

static const char *rpsraw_help =
	"Usage:\n"
	"\n rpsraw <offset> <length> \n"
	"\n Example:\n"
	"\n rpsraw 0x012c 10\n";
static void cmd_rpsraw(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int offset,len,i,j;
	if(argc != 3){
		printf("\n%s\n",rpsraw_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	offset = GetUInt(&(argv[1]),0);
	len = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_READ_RAW,len,offset);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\n read PS raw failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}

	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = read(dd,&buf,HCI_MAX_EVENT_SIZE);
	if(iRet < 0){
		printf("\n read PS raw failed\n");
		hci_close_dev(dd);	
		return;
	}
	printf("\nOffset :%X\nLength:%X\nData:\n",offset,len);
	
	for(i=4,j=1;i<iRet;i++,j++){
		printf("%02X ",buf[i]);
		if(j%16 == 0)
			printf("\n");
	}
	printf("\n\n");
	hci_close_dev(dd);	
}
static const char *wpsraw_help =
	"Usage:\n"
	"\n wpsraw <offset> <length> <data>\n"
	"\n Example:\n"
	"\n wpsraw 0x012C 6 00 03 F4 55 AB 77 \n";

static void cmd_wpsraw(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int offset,len,i;
	if(argc < 4){
		printf("\n%s\n",wpsraw_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	offset = GetUInt(&(argv[1]),0);
	len = GetUInt(&(argv[2]),0);
	if(argc < len+3){
		printf("\nData is less than Length\n");
		hci_close_dev(dd);	
		return;
	}
	LoadPSHeader(buf,PS_WRITE_RAW,len,offset);
	for(i=0;i<len;i++){
		buf[i+4] = strtol(argv[i+3], NULL, 16);
	}
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS,PS_COMMAND_HEADER + len, buf);
	if(buf[6] != 0){
		printf("\n Write PS tag failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	hci_close_dev(dd);	
}

static const char *peek_help =
	"\nUsage:"
	"\npeek <address> <width>\n"
	"\nExample:\n"
	"\npeek 0x00004FFC 5\n";
static void cmd_peek(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int address,width,value;
	if(argc < 2){
		printf("\n%s\n",peek_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	address = GetUInt(&(argv[1]),0);
	if(argc == 3)
		width = GetUInt(&(argv[2]),0x4);
	else
		width = 4;

	buf[0] = (address & 0xFF);
	buf[1] = ((address >>8) & 0xFF);
	buf[2] = ((address>>16) & 0xFF);
	buf[3] = ((address>>24) & 0xFF);
	buf[4] = (UCHAR)width;  //Memory width
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_MEMORY, 5, buf);	
	if(buf[6] != 0){
		printf("\nRead Memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	value = buf[10];
	value = ((value << 8) | buf[9]);
	value = ((value << 8) | buf[8]);
	value = ((value << 8) | buf[7]);
	
	printf("\n0x%X : 0x%X \n",address,value);
	//printf("\n0x%X : 0x%02X%02X%02X%02X \n",address,buf[7],buf[8],buf[9],buf[10]);
	hci_close_dev(dd);	
}

static const char *cwtx_help =
	"\nUsage:"
	"\ncwtx <channel number>\n"
	"\nExample:\n"
        "\ncwtx 40"
	"\n\n";

static void cmd_cwtx(int dev_id, int argc, char **argv){
	int dd,iRet, Length = 0;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int channel;
	if(argc != 2){
		printf("\n%s\n",cwtx_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	channel = atoi(argv[1]);
	if(channel > 78 || channel < 0){
		printf("\nPlease enter channel 0-78!\n");
		hci_close_dev(dd);
		return;	
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_HOST_CTL,OCF_RESET,Length,buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0x80;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0xFF;
	buf[6] = 0x08;
	buf[7] = 0xC0;
	buf[8] = 0x00;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	/* hcitool cmd 0x3F 0x06 0x34 0x20 0x02 0x00 0x04 0x88 0xA0 0x00 0x02 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0x34;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x88;
	buf[6] = 0xA0;
	buf[7] = 0x00;
	buf[8] = 0x02;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}

	/* hcitool cmd 0x3F 0x06 0x28 0x20 0x02 0x00 0x04 0x00 0x90 0x05 0x20 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0x28;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x00;
	buf[6] = 0x90;
	buf[7] = 0x05;
	buf[8] = 0x20;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}

	/* hcitool cmd 0x3F 0x06 0x7C 0x08 0x02 0x00 0x04 0x01 0x00 0x00 0x4B 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0x7C;
	buf[1] = 0x08;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x01;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x4B;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}

	
	/* hcitool cmd 0x3F 0x06 0x00 0x08 0x02 0x00 0x04 $number 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	buf[0] = 0x00;
	buf[1] = 0x08;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = (UCHAR)channel; /* Num */
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD,OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	printf("\nEntering continuous wave Tx on channel %d\n",channel);

	hci_close_dev(dd);	
}


static const char *poke_help =
	"\nUsage:"
	"\npoke <address> <value> <mask> <width>\n"
	"\nExample:\n"
        "\npoke 0x580000 0x22005FF 0xFFFFFFFF 4"
	"\n\n";

static void cmd_poke(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	int address,width,value,mask;
	if(argc < 2){
		printf("\n%s\n",poke_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	address = GetUInt(&(argv[1]),0);
	value = GetUInt(&(argv[2]),0);
	printf("\nARGC :%d\n",argc);
	if(argc < 4)
		mask = 0xffffffff;
	else
		mask = GetUInt(&(argv[3]),0xFFFFFFFF);
	if(argc < 5)
		width = 4;
	else
		width = GetUInt(&(argv[4]),0x4);
	buf[0] = (address & 0xFF);
	buf[1] = ((address >>8) & 0xFF);
	buf[2] = ((address>>16) & 0xFF);
	buf[3] = ((address>>24) & 0xFF);
	buf[4] = width;  //Memory width
	buf[5] = (value & 0xFF);
	buf[6] = ((value >> 8) & 0xFF);
	buf[7] = ((value >> 16) & 0xFF);
	buf[8] = ((value >> 24) & 0xFF);
	buf[9] = (mask & 0xFF);
	buf[10] = ((mask >>8) & 0xFF);
	buf[11] = ((mask>>16) & 0xFF);
	buf[12] = ((mask>>24) & 0xFF);
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_WRITE_MEMORY, 13, buf);	
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	printf("\nPoke successful!\n");
	hci_close_dev(dd);	
}



static const char *dump_help =
	"\nUsage:"
	"\ndump audio - Display Audio statistics\n"
	"\ndump dma- Display DMA statistics\n"
	"\ndump dma r - Display and Reset DMA statistics\n"
	"\ndump tpc - Dump TPC tables\n"
	"\nExample:\n"
        "\ndump audio"
        "\ndump dma"
        "\ndump dma r"
        "\ndump tpc"
	"\n";


static void cmd_dump(int dev_id, int argc, char **argv){
	int dd;	
	if(argc < 2){
		printf("\n%s\n",dump_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	if(!strncmp(argv[1],"audio",5)){
		ReadAudioStats(dd);
	}
	else if(!strncmp(argv[1],"dma",3)){
		ReadGlobalDMAStats(dd);
		if(argc == 3 && !strncmp(argv[2],"r",1)){
			ResetGlobalDMAStats(dd);
		}	
	}
	else if(!strncmp(argv[1],"tpc",3)){
		ReadTpcTable(dd);
	}
	else{
		printf("\nInvalid option");
		printf("\n%s\n",dump_help);	
	}
	
	hci_close_dev(dd);	
	return;
}

static const char *rafh_help =
	"\nUsage:"
	"\nrafh <connection handle>\n"
	"\nExample:\n"
        "\nrafh 0x15"
	"\n\n";

static void cmd_rafh(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	short int handle;

	if(argc < 2){
		printf("\n%s\n",rafh_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	handle = GetUInt(&(argv[1]),0);	
	buf[0] = (handle & 0xFF);
	buf[1] = ((handle >>8) & 0xFF);
	iRet = writeHciCommand(dd, OGF_STATUS_PARAM,OCF_READ_AFH_MAP, 2, buf);	
	if(buf[6] != 0){
		printf("\nRead AFH failed due to reason :0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	
	if(buf[9] == 0)
		printf(" AFH is disabled");
	else
		printf(" AFH is enabled");

	handle = (buf[7] | (buf[8] << 8));
	printf("\n AFH chaneel classification for handle: 0x%X",handle);
	int i;
	printf("\n Channel Classification Map :");
	for(i=iRet-1; i>9 ; i--){
		printf("%X",buf[i]);
	}
	printf("\n");
	hci_close_dev(dd);	
}

static const char *safh_help =
	"\nUsage:"
	"\nsafh <host channel classification>\n"
	"\nExample:\n"
        "\nsafh 0x7FFFFFFFFFFFFFFFFFFF"
	"\n\n";

static void cmd_safh(int dev_id, int argc, char **argv){
	int dd,iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	if(argc < 2){
		printf("\n%s\n",safh_help);	
		return;
	}
	int i,j;
	i = strlen(argv[1]);
	if(i > 20 || i < 20){
		printf("\n%s\n",safh_help);	
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(&buf,0,HCI_MAX_EVENT_SIZE);
	const char *map = argv[1];
	char byte[3];
	int data;
	for (i = 0,j=9; i < 20 ; i+=2,j--) {
		memcpy(byte,&map[i],2);
		byte[2] = '\0';
		data = strtol(byte, NULL, 16);
		buf[j] = (data & 0xFF);
	}
	iRet = writeHciCommand(dd, OGF_HOST_CTL,OCF_SET_AFH_CLASSIFICATION,10, buf);	
	if(buf[6] != 0){
		printf("\nSet AFH failed due to reason :0x%X\n",buf[6]);
		hci_close_dev(dd);	
		return;
	}
	printf("\nSet AFH successful!\n");
	hci_close_dev(dd);
}

static const char *wotp_help =
	"\nUsage:"
	"\nwotp <address> <data> [length=1]\n"
	"\nExample:\n"
        "\nwotp 0x15 0x2020 2"
	"\n\n";

static void cmd_wotp(int dev_id, int argc, char **argv)
{
	UINT32 address, length;

	if (argc < 3) {
		printf("\n%s\n", wotp_help);
		return;
	}
	if (argc == 4)
		length = GetUInt(&argv[3], 1);
	else
		length = 1;
	address = GetUInt(&argv[1], 0xffffffff);
	if (address == 0xffffffff) {
		printf("\n%s\n", wotp_help);
		return;
	}
	ReverseHexString(argv[2]);
	if (!write_otpRaw(dev_id, address, length, (UCHAR *)argv[2]))
		printf("Write to OTP sucessful!\n");
}

static int write_otpRaw(int dev_id, int address, int length, UCHAR *data)
{
	int dd, iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return dd;
	}
	memset(&buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = 0x12;				/* write RAW OTP */ 
	buf[1] = address & 0xFF;		/* PS tag */
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = length;			/* Entry Size */
	GetByteSeq(buf + 4, data, 244);	/* Entry Data */
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS, 244 + PS_COMMAND_HEADER, buf); 
	if (buf[6] != 0) {
		printf("\nWrite to OTP failed due to reason :0x%X\n", buf[6]);
		hci_close_dev(dd);
		return buf[6];
	}
	hci_close_dev(dd);
	return 0;
}

static const char *rotp_help =
	"\nUsage:"
	"\nrotp <address> [length=1]\n"
	"\nExample:\n"
        "\nrotp 0x15 2"
	"\n\n";

static void cmd_rotp(int dev_id, int argc, char **argv)
{
	UINT32 address, length;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	if (argc < 2) {
		printf("\n%s\n", rotp_help);
		return;
	}
	if (argc == 3)
		length = GetUInt(&argv[2], 1);
	else
		length = 1;
	address = GetUInt(&argv[1], 0xffffffff);
	if (address == 0xffffffff) {
		printf("\n%s\n", rotp_help);
		return;
	}
	if (!read_otpRaw(dev_id, address, length, buf))
		dumpHex(buf, length, 8);
}

static int read_otpRaw(int dev_id, int address, int length, UCHAR *data)
{
	int dd, iRet, plen;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return dd;
	}
	memset(&buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = 0x11;				/* read OTP */ 
	buf[1] = address & 0xFF;		/* PS tag */
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = length;			/* Entry Size */
	buf[4] = 0x00;				/* Entry Data */
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_PS, 244 + PS_COMMAND_HEADER, buf); 
	if (buf[6] != 0) {
		printf("\nRead from OTP failed due to reason :0x%X\n", buf[6]);
		hci_close_dev(dd);
		return buf[6];
	}
	do {
		plen = read(dd, buf, HCI_MAX_EVENT_SIZE);
		if (plen < 0) {
			perror("Read OTP error\n");
			exit(EXIT_FAILURE);
		}
	} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
	memcpy(data, buf + HCI_EVENT_HEADER_SIZE + 1, length);
	hci_close_dev(dd);
	return 0;
}

static int SU_GetId(int dev_id, char *pStr, tSU_RevInfo *pRetRevInfo)
{
	tSU_RevInfo RevInfo;
	int dd, iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	RevInfo.RomVersion = 0x99999999;
	RevInfo.BuildVersion = 0x99999999;
	RevInfo.RadioFormat = 0xffff;
	RevInfo.SysCfgFormat = 0xffff;

	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return dd;
	}
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_READ_VERSION, 0, buf);
	if (buf[6] != 0) {
		printf("\nRead version failed due to reason :0x%X\n", buf[6]);
		return buf[6];
	}
	RevInfo.RomVersion = buf[7] + (buf[8]<<8) + (buf[9]<<16) + (buf[10]<<24);
	RevInfo.BuildVersion = buf[11] + (buf[12]<<8) + (buf[13]<<16) + (buf[14]<<24);
	return 0;
}

/*static const char *otp_help =
	"\nUsage:"
	"\notp [dump|imp|exp|test|rpid|wpid|rvid|wvid|rba|wba|hid|cpw|cpw|pwridx|ledo] [file]\n"
	"\notp wba <BdAddr>:\n"
	"\n\n";
*/

static void cmd_otp(int dev_id, int argc, char **argv)
{
	UCHAR buf[512], format[16];
	FILE *pF = NULL;
	UINT32 data;
	int i;

	if (argc == 1 || !strcmp(argv[1], "dump")) {
		printf("dump:\n");
		for (i = 0; i < 4; i++) {
			if (read_otpRaw(dev_id, 128 * i, 128, &buf[128*i])) {
				printf("read failed\n");
				return;
			}
		}
		dumpHex(buf, 512, 8);
	} else if (!strcmp(argv[1], "test")) {
		printf("test:\n");
		printf("To be continue.\n");
	} else if (!strcmp(argv[1], "imp")) {
		if (argc < 3 || !*argv[2]) {
			printf("Import file content into OTP. File name is required\n");
			return;
		}
		printf("Import from %s into OTP:\n", argv[2]);
		if (!(pF = fopen(argv[2], "rb"))) {
			printf("Open file failed\n");
			return;
		}
		fread(&buf[0], sizeof(buf), 1, pF);
		fclose(pF);
		for (i = 0; i < 512; i += 4) {
			data = buf[i];
			data <<= 8;
			data += buf[i+1];
			data <<= 8;
			data += buf[i+2];
			data <<= 8;
			data += buf[i+3];
			sprintf((char *)&format, "0x%08x", data);
			if (write_otpRaw(dev_id, i, 4, (UCHAR *)format)) {
				printf("Failed!(%d)\n", i);
				return;
			}
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "exp")) {
		for (i = 0; i < 4; i++) {
			if (read_otpRaw(dev_id, 128 * i, 128, &buf[128*i])) {
				printf("Failed\n");
				return;
			}
		}
		if (argc < 3 || !*argv[2] || (!(pF = fopen(argv[2], "wb")))) {
			/* export the content to the screen */
			dumpHex(buf, 512, 8);
		} else {
			/* export the content to the file */
			fwrite(&buf[0], sizeof(buf), 1, pF);
			fclose(pF);
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "ledo")) {
		int opendrain;
		tSU_RevInfo RevInfo;

		if (SU_GetId(dev_id, NULL, &RevInfo))
			return;

		printf("RomVer:%02X.%02X.%02X.%02X \n", (UINT8)((RevInfo.RomVersion >> (8*3)) & 0xff),
				(UINT8)((RevInfo.RomVersion >> (8*2)) & 0xff),
				(UINT8)((RevInfo.RomVersion >> 8) & 0xff),
				(UINT8)(RevInfo.RomVersion & 0xff));
		if (((UINT8)((RevInfo.RomVersion >> (8*3)) & 0xff) == 0x01) &&
		    ((UINT8)((RevInfo.RomVersion >> (8*2)) & 0xff) == 0x02) &&
		    ((UINT8)((RevInfo.RomVersion >> 8) & 0xff) == 0x02) &&
		    ((UINT8)(RevInfo.RomVersion & 0xff) == 0x00)) {
			UINT8 LedValue[] = {0xCE, 0xDA, 0x04, 0x0C, 0x58,
					    0x04, 0x05, 0x06, 0xff, 0x50,
					    0x40, 0x01, 0x24, 0x08, 0x00,
					    0x00};
			for (opendrain = 112; opendrain < 128; opendrain++) {
				if (write_otpRaw(dev_id, opendrain, 1, &LedValue[opendrain-112])) {
					printf("Failed\n");
					return;
				}
			}
			printf("OTP led opendrain done\n");
		} else {
			printf("Wrong RomVer\n");
		}
	} else if (!strcmp(argv[1], "cpw")) {
		UINT32 cin_value = 0, cout_value = 0;
		char tempStr[8];

		if (argc < 3) {
			printf("\n Enter cin_value : ");
			scanf("%d", &cin_value);
		} else
			cin_value = GetUInt(&argv[2], 0);
		if (cin_value < 0 || cin_value > 128) {
			printf("Invalid cin_value = %d\n", cin_value);
			return;
		}
		if (argc < 4) {
			printf("\n Enter cout_value : ");
			scanf("%d", &cout_value);
		} else
			cout_value = GetUInt(&argv[3], 0);
		if (cout_value < 0 || cout_value > 128) {
			printf("Invalid cout_value = %d\n", cout_value);
			return;
		}
		if (cout_value & 0x01) cin_value += 0x80;
		sprintf(tempStr, "0x%02x", cin_value);
		if (write_otpRaw(dev_id, 4, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		sprintf(tempStr, "0x%02x", cout_value >> 1);
		if (write_otpRaw(dev_id, 5, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		sprintf(tempStr, "0x40");
		if (write_otpRaw(dev_id, 5, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "pwridx")) {
		char tempStr[8];
		sprintf(tempStr, "0x02");
		if (write_otpRaw(dev_id, 21, 1, (UCHAR *)tempStr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "hid")) {
		char tempStr[8];
		UINT32 value = 0;
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter HID value(0|1) : ");
			scanf("%d", &value);
		} else
			value = GetUInt(&argv[2], 0);
		if (value != 0 && value != 1) {
			printf("\n Error: Syntax \"otp hid 0x00|0x01\"\n");
			return;
		}
		sprintf(tempStr, "0x%02x", value);
		if (write_otpRaw(dev_id, 12, 1, (UCHAR *)tempStr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "wpid")) {
		UINT32 offset = 134;
		size_t len = 0;
		char pid[8] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_PID_OFFSET(default 134) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(pid, 0, sizeof(pid));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter PID : ");
			fgets((char *)pid, 7, stdin);
		} else
			strncpy((char *)pid, argv[2], 7);
		len = strlen(pid) - 1;
		if (pid[len] == '\n' || pid[len] == '\r')
			pid[len] = 0;
		ReverseHexString(pid);
		if (write_otpRaw(dev_id, offset, 4, (UCHAR *)pid)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rpid")) {
		UINT32 offset = 134;
		size_t len = 0;
		UCHAR Data[2];
		char *ofs = NULL;
		printf("\n Enter OTP_PID_OFFSET(default 134) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(dev_id, offset, 2, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP PID is 0x%02x%02x\n", Data[1], Data[0]);
	} else if (!strcmp(argv[1], "wvid")) {
		UINT32 offset = 136;
		size_t len = 0;
		char vid[8] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_VID_OFFSET(default 136) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(vid, 0, sizeof(vid));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter VID : ");
			fgets(vid, 8, stdin);
		} else
			strncpy(vid, argv[2], 7);
		len = strlen(vid) - 1;
		if (vid[len] == '\n' || vid[len] == '\r')
			vid[len] = 0;
		ReverseHexString(vid);
		if (write_otpRaw(dev_id, offset, 2, (UCHAR *)vid)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rvid")) {
		UINT32 offset = 136;
		size_t len = 0;
		char *ofs = NULL;
		UCHAR Data[2];
		printf("\n Enter OTP_VID_OFFSET(default 136) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(dev_id, offset, 2, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP VID is 0x%02x%02x\n", Data[1], Data[0]);
	} else if (!strcmp(argv[1], "wba")) {
		UINT32 offset = 128;
		size_t len = 0;
		char bdaddr[16] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_BDA_OFFSET(default 128) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(bdaddr, 0, sizeof(bdaddr));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter BDADDR : ");
			fgets(bdaddr, 16, stdin);
		} else
			strncpy(bdaddr, argv[2], 15);
		len = strlen(bdaddr) - 1;
		if (bdaddr[len] == '\n' || bdaddr[len] == '\r')
			bdaddr[len] = 0;
		ReverseHexString(bdaddr);
		if (write_otpRaw(dev_id, offset, 6, (UCHAR *)bdaddr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rba")) {
		UINT32 offset = 128;
		size_t len = 0;
		char *ofs = NULL;
		UCHAR Data[6];
		printf("\n Enter OTP_BDA_OFFSET(default 128) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(dev_id, offset, 6, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP BDADDR is 0x%02x%02x%02x%02x%02x%02x\n",
				Data[5], Data[4], Data[3], Data[2], Data[1], Data[0]);
	}
}

static const char *plb_help =
	"\nUsage:"
	"\nplb [1|0]\n"
	"\nplb 1\n"
	"\n\n";

static void cmd_plb(int dev_id, int argc, char **argv)
{
	int dd, enable, iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if (argc < 2)
		enable = 1;
	else
		enable = GetUInt(&argv[1], 1);
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = 0x09;					/* audio commmand opcode */
	buf[4] = (enable == 0) ? 0x00 : 0x01;		/* audio command param */
	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_AUDIO_CMD, 8, buf); 
	if (buf[6] != 0) {
		printf("\nError in setting PCM CODEC loopback :0x%X\n", buf[6]);
		hci_close_dev(dd);
		return;
	}
	printf("\nPCM CODEC loopback is %s\n", (enable == 0) ? "OFF" : "ON");
	hci_close_dev(dd);
}

static const char *psw_help =
	"\nUsage:"
	"\npsw [1|0] [Frequency]\n"
	"\npsw 1 3000\n"
	"\n\n";

static void cmd_psw(int dev_id, int argc, char **argv)
{
	int dd, enable, freq, iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	if (argc < 2) {
		enable = 1;
		freq = 440;
	}
	else if (argc < 3) {
		printf("aa\n");
		enable = GetUInt(&argv[1], 1);
		freq = 440;
	} else {
		enable = GetUInt(&argv[1], 1);
		freq = GetUInt(&argv[2], 440);
	}
	if (freq > 3700) {
		printf("Invalid frequency. It should be in the range of 0 to 3700\n");
		return;
	}
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	
	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = 0x0a;					/* audio command opcode */
	buf[4] = (enable == 0) ? 0x00 : 0x01;		/* audio command param */
	buf[5] = 0x00;
	buf[6] = freq & 0xff;
	buf[7] = (freq >> 8) & 0xff;

	iRet = writeHciCommand(dd, OGF_VENDOR_CMD, OCF_AUDIO_CMD, 8, buf); 
	if (buf[6] != 0) {
		printf("\nError in running PCM sine wave playback :0x%X\n", buf[6]);
		hci_close_dev(dd);
		return;
	}
	printf("PCM CODEC PCM sine wave playback is %s\n", (enable == 0) ? "OFF" : "ON");
	hci_close_dev(dd);
}

static const char *lert_help=
	"\nUsage:"
	"\nlert <rx_channel>\n"
	"\nlert 30 \n"
	"\n\n";

static void cmd_lert(int dev_id, int argc, char **argv)
{
	int dd;
	UCHAR channel;
	if (argc < 2) {
		printf("\n%s\n", lert_help);
		return;
	}
	channel = (UCHAR)GetUInt(&argv[1], 0);
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}

	SU_LERxTest(dd, channel);
	hci_close_dev(dd);
}

static BOOL SU_LERxTest(int dd, UCHAR channel)
{
	int iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	if (channel < MB_MIN_FREQUENCY_LE || channel > MB_MAX_FREQUENCY_LE) {
		printf("Invalid rx channel. It should be in the range of %d to %d\n", 
			MB_MIN_FREQUENCY_LE, MB_MAX_FREQUENCY_LE);
		return FALSE;
	}
	
	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = channel;			/* rx_channel */
	iRet = writeHciCommand(dd, OGF_LE_CTL, OCF_LE_RECEIVER_TEST, 1, buf);
	if (buf[6] != 0) {
		printf("\nError in putting the device into LE RX mode\n");
		return FALSE;
	}
	return TRUE;
}

static const char *lett_help=
	"\nUsage:"
	"\nlett <rx_channel> <length> <packet_payload>\n"
	"\nlett 30 30 5\n"
	"\n\n";

static void cmd_lett(int dev_id, int argc, char **argv)
{
	int dd;
	UCHAR channel, length, payload;
	if (argc < 4) {
		printf("\n%s\n", lett_help);
		return;
	}
	channel = (UCHAR)GetUInt(&argv[1], 0);
	length = (UCHAR)GetUInt(&argv[2], 0);
	payload = (UCHAR)GetUInt(&argv[3], 0);

	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	SU_LETxTest(dd, channel, length, payload);
	hci_close_dev(dd);
}

static BOOL SU_LETxTest(int dd, UCHAR channel, UCHAR length, UCHAR payload)
{
	int iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	if (channel < MB_MIN_FREQUENCY_LE || channel > MB_MAX_FREQUENCY_LE) {
		printf("Invalid tx channel. It should be in the range of %d to %d\n", 
			MB_MIN_FREQUENCY_LE, MB_MAX_FREQUENCY_LE);
		return FALSE;
	}
	if (length < MB_MIN_DATALEN_LE || length > MB_MAX_DATALEN_LE) {
		printf("Invalid data length. It should be in the range of %d to %d\n", 
			MB_MIN_DATALEN_LE, MB_MAX_DATALEN_LE);
		return FALSE;
	}
	if (payload > 7) {
		printf("Invalid packet payload. It should be in the range of 0 to 7\n");
		return FALSE;
	}

	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	buf[0] = channel;			/* tx_channel */
	buf[1] = length;			/* length of test data */
	buf[2] = payload;			/* packet payload */
	iRet = writeHciCommand(dd, OGF_LE_CTL, OCF_LE_TRANSMITTER_TEST, 3, buf);
	if (buf[6] != 0) {
		printf("\nError in putting the device into LE TX mode\n");
		return FALSE;
	}
	return TRUE;
}

static const char *lete_help =
	"\nUsage:"
	"\nlete\n"
	"\n\n";

static void cmd_lete(int dev_id, int argc, char **argv)
{
	int dd, iRet;
	UCHAR buf[HCI_MAX_EVENT_SIZE];

	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	memset(buf, 0, HCI_MAX_EVENT_SIZE);
	iRet = writeHciCommand(dd, OGF_LE_CTL, OCF_LE_TEST_END, 0, buf);
	if (buf[6] != 0) {
		printf("\nError in ending LE test\n");
		hci_close_dev(dd);
		return;
	}
	printf("Number of packets = %d\n", buf[7] | (buf[8] << 8));
	hci_close_dev(dd);
}

static const char *tputs_help =
	"\nUsage:"
	"\ntput-s [BD_Addr] [Judgment value] Logfile times"
	"\ntput-s 11:22:33:44:55:66 150 log.txt 10"
	"\n\n";

static void CalculateTput(int dd, UINT16 hci_handle, char *filename, double threshold, int tx_size)
{
	time_t start, finish, checkbreak;
	UCHAR buf[1009];
	FILE *fp = NULL;
	int aclnum = 8;
	int retval;
	unsigned long sentnum = 0;
	double TimeResult = 0;
	fd_set rfds;
	struct hci_filter flt;
	struct timeval tv1, tv2, timeout;
	unsigned long long start_utime, end_utime, time_diff;
	unsigned long long throughput;

	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_set_event(EVT_NUM_COMP_PKTS, &flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}
	start = time(NULL);
	gettimeofday(&tv1, NULL);
	start_utime = tv1.tv_sec*1000000 + tv1.tv_usec;
	while (sentnum < 1024 * tx_size) {
		while (aclnum > 0) {
			aclnum--;
			buf[0] = HCI_ACLDATA_PKT;
			/* ACL packet header */
			buf[1] = hci_handle & 0xFF;
			buf[2] = ((hci_handle >> 8) & 0x0E);
			buf[3] = 1004 & 0xff;
			buf[4] = (1004 >> 8) & 0xff;
			/* L2CAP packet header */
			buf[5] = 1000 & 0xff;
			buf[6] = (1000 >> 8) & 0xff;
			buf[7] = 0x40 & 0xff;
			buf[8] = 0;

			memset(buf+9, sentnum++, 1000);
			while (write(dd, (const void *)buf, 1009) < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				perror("HCI send packet failed");
				exit(EXIT_FAILURE);
			}
		}
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		FD_ZERO(&rfds);
		FD_SET(dd, &rfds);
		retval = select(dd+1, &rfds, NULL, NULL, &timeout);
		if (retval == -1) {
			perror("select()");
			exit(EXIT_FAILURE);
		} else if (retval) {
			/* Data is available now */
			ssize_t plen;
			UCHAR buffer[64];
			int i;
			plen = read(dd, buffer, 64);
			if (plen < 0) {
				perror("HCI read buffer failed");
				exit(EXIT_FAILURE);
			}
			for (i = 0; i < buffer[HCI_EVENT_HEADER_SIZE]; i++)
				aclnum += (buffer[HCI_EVENT_HEADER_SIZE+(i+1)*2+1] | (buffer[HCI_EVENT_HEADER_SIZE+(i+1)*2+2] << 8));
		}
		checkbreak = time(NULL);
		if ((checkbreak - start) >= 300) break;
	}
	finish = time(NULL);
	gettimeofday(&tv2, NULL);
	end_utime = tv2.tv_sec*1000000 + tv2.tv_usec;
	time_diff = end_utime - start_utime;
	throughput = time_diff/1000;
	throughput = (sentnum * 1000)/throughput;
	printf("Transfer Completed! throughput [%0d KB/s]", (int)throughput);
	printf(" result [%s]\n", threshold > throughput ? " Fail " : " Pass ");
	if (filename && *filename)
		fp = fopen(filename, "at+");
	if (fp) {
		fprintf(fp, "Transfer Completed! throughput [%.0f KB/s]", TimeResult);
		fprintf(fp, " result [%s]\n", threshold > TimeResult ? " Fail " : " Pass ");
		fclose(fp);
	}
}

static void cmd_tputs(int dev_id, int argc, char **argv)
{
	int j, dd, iRet, loop = 1, tx_test_size = 1;
	UINT16 Ps_EntrySize = 0;
	UINT16 hci_handle = 0;
	double threshold = 0.0;
	char *filename = NULL;
	struct sigaction sa;
	FILE *fp = NULL;
	bdaddr_t bdaddr;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	UCHAR Ps_Data[HCI_MAX_EVENT_SIZE];
	UINT16 *pPs_Data;
	BOOL Ok = FALSE;
	char timeString[9] = {0};
	char dateString[15] = {0};
	time_t current_time;
	struct tm *time_info;
	tSU_RevInfo RevInfo;

	if (argc < 3) {
		printf("\n%s\n", tputs_help);
		return;
	}
		
	if (str2ba(argv[1],&bdaddr)) {
		printf("\nPlease input valid bdaddr.\n");
		return;
	}
	threshold = atof(argv[2]);
	if (!threshold) {
		printf("\nPlease input valid throughput threshold.\n");
		return;
	}
	if (argc > 3)
		filename = strdup(argv[3]);
	if (argc > 4)
		loop = GetUInt(&argv[4], 1);
	if (argc > 5)
		tx_test_size = GetUInt(&argv[5],1);
	if (dev_id < 0)
		dev_id = hci_get_route(NULL);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	CtrlCBreak = FALSE;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	PSInit(dd);
	memset(buf, 0, sizeof(buf));
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_RESET, 0, buf);
	if (buf[6] != 0) {
		printf("Error: HCI RESET failed.\n");
		hci_close_dev(dd);
		return;
	}
	sleep(1);
	for (j = 0; j < loop; j++) {
		int i = 0;
		if (!j) sleep(1);
		printf("\n-----------------------------------");
		printf("\nTimes %d/%d\n", j + 1, loop);

		time(&current_time);
		time_info = localtime(&current_time);
		strftime(timeString, sizeof(timeString), "%H %M %S", time_info);
		strftime(dateString, sizeof(dateString), "%b %d %Y", time_info);
		if (j == 0) {
			if (filename && *filename)
				fp = fopen(filename, "at+");
			if (fp != NULL)
				fprintf(fp, "\n[%s %s] \nCMD : TPUT-S %s %f %s %d\n",
						dateString, timeString, argv[1], threshold, filename, loop);
			/* SFLAGS FW */
			Ok = PSOperations(dd, PS_GET_LENGTH, PSTAG_RF_TEST_BLOCK_START, (UINT32 *)&Ps_EntrySize);
			if (Ok) {
				Ps_Data[0] = Ps_EntrySize & 0xff;
				Ps_Data[1] = (Ps_EntrySize >> 8) & 0xff;
				Ok = PSOperations(dd, PS_READ, PSTAG_RF_TEST_BLOCK_START, (UINT32 *)&Ps_Data);
				if (Ok) {
					pPs_Data = (UINT16 *)&Ps_Data[0];
					if (*pPs_Data == BT_SOC_INIT_TOOL_START_MAGIC_WORD) {
						RevInfo.RadioFormat = *(pPs_Data + 1);
						RevInfo.RadioContent = *(pPs_Data + 2);
					}
				}
			}

			/* Get syscfg info */
			Ok = PSOperations(dd, PS_GET_LENGTH, PSTAG_SYSCFG_PARAM_TABLE0, (UINT32 *)&Ps_EntrySize);
			if (Ok) {
				Ps_Data[0] = Ps_EntrySize & 0xff;
				Ps_Data[1] = (Ps_EntrySize >> 8) & 0xff;
				Ok = PSOperations(dd, PS_READ, PSTAG_SYSCFG_PARAM_TABLE0, (UINT32 *)&Ps_Data);
				if (Ok) {
					pPs_Data = (UINT16 *)&Ps_Data[0];
					if (*pPs_Data == 0xC1C1) {
						RevInfo.SysCfgFormat = *(pPs_Data + 1);
						RevInfo.SysCfgContent = *(pPs_Data + 2);
					}

				}
			}
			
			if (RevInfo.SysCfgFormat != 0xff) {
				printf("SysCfg -    Format:  %d.%d\n",((RevInfo.SysCfgFormat >> 4) & 0xfff), (RevInfo.SysCfgFormat & 0xf));
				printf("            Content: %d\n", RevInfo.SysCfgContent);
				if (fp) {
					fprintf(fp, "SysCfg -    Format:  %d.%d\n",((RevInfo.SysCfgFormat >> 4) & 0xfff), 
							(RevInfo.SysCfgFormat & 0xf));
					fprintf(fp, "            Content: %d\n", RevInfo.SysCfgContent);
				}
			} else {
				printf("SysCfg - N/A\n");
				if(fp)
					fprintf(fp, "SysCfg - N/A\n");
			}

			/* bd addr */
			memset(&buf, 0, sizeof(buf));
			iRet = writeHciCommand(dd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);
			if (buf[6] != 0) {
				printf("\nCould not read the BD_ADDR (time out)\n");
			} else {
				char temp[16] = {0};
				memset(temp, 0, sizeof(temp));
				sprintf(temp, "%02X%02X%02X%02X%02X%02X", buf[iRet-1], buf[iRet-2],
						buf[iRet-3], buf[iRet-4], buf[iRet-5], buf[iRet-6]);
				printf("\nLocal BDAddress : 0x%s\n", temp);
				if (fp)
					fprintf(fp, "Local BDAddress : 0x%s\n", temp);
			}

			if (fp) {
				fclose(fp);
				fp = NULL;
			}
		}
		printf("Sending packages to 0x%s\n", argv[1]);
		while (i++ < 3) {
			iRet = hci_create_connection(dd, &bdaddr, 0xCC18, 0, 0, &hci_handle, 0);
			if (!iRet || CtrlCBreak) break;
		}

		if (iRet) {
			if (filename && *filename) {
				fp = fopen(filename, "at+");
				if (fp) {
					fprintf(fp, "Transfer Failed! \n");
					fclose(fp);
					fp = NULL;
				}
			}
			printf("Transfer Failed! \n");
			CtrlCBreak = TRUE;
			hci_close_dev(dd);
			return;
		}
		CalculateTput(dd, hci_handle, filename, threshold, tx_test_size);

		hci_disconnect(dd, hci_handle, 0, 30);

		if (CtrlCBreak) break;
        }
	CtrlCBreak = TRUE;
	hci_close_dev(dd);
}

static void cmd_tputr(int dev_id, int argc, char **argv)
{
	int dd, iRet;
	ssize_t plen;
	UINT16 hci_handle = 0;
	UCHAR buf[HCI_MAX_EVENT_SIZE];
	struct hci_filter flt;
	struct sigaction sa;

	if (dev_id < 0)
	       dev_id = hci_get_route(NULL);
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("\nERROR: Can not open HCI device\n");
		return;
	}
	CtrlCBreak = FALSE;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	memset(buf, 0, sizeof(buf));
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_RESET, 0, buf);
	if (buf[6] != 0) {
		printf("Error: HCI RESET failed.\n");
		hci_close_dev(dd);
		return;
	}
	sleep(1);	
	memset(buf, 0, sizeof(buf));
	buf[0] = 0x02;
	iRet = writeHciCommand(dd, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, buf);
	if (buf[6] != 0) {
		printf("Error: Write scan failed\n");
		return;
	}
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}
	printf("Start listening ...\n");
	do {
		plen = read(dd, buf, HCI_MAX_EVENT_SIZE);
		if (plen < 0) {
			printf("reading failed...\n");
			if (errno == EAGAIN || errno == EINTR) continue;
			else {
				perror("HCI read failed");
				exit(EXIT_FAILURE);
			}
		}
		if (buf[1] == EVT_CONN_REQUEST) {
			int i, j;
			ssize_t plen = 0;
			printf("Connection come in\n");
			for (i = 0, j = 3; i < BD_ADDR_SIZE; i++, j++)
				buf[i] = buf[j];
			buf[BD_ADDR_SIZE] = 0x01;
			if (hci_send_cmd(dd, OGF_LINK_CTL, OCF_ACCEPT_CONN_REQ, 7, buf)) {
				printf("Accept connection error\n");
				return;
			}
			do {
				plen = read(dd, buf, HCI_MAX_EVENT_SIZE);
				if (plen < 0) {
					perror("Read failed");
					exit(EXIT_FAILURE);
				}
			} while (buf[1] != EVT_CONN_COMPLETE);
			if (buf[3] == 0) {
				printf("Connection up\n");
			} else {
				printf("Connection failed\n");
			}
			hci_handle = (buf[4] | (buf[5] << 8)) & 0x0EFF;
		} else if (buf[1] == EVT_DISCONN_COMPLETE) {
			UINT16 hdl = buf[4] | (buf[5] << 8);
			printf("Disconnect...\n");
			if (hdl == hci_handle) {
				break;
			}
		} else if (CtrlCBreak) {
			printf("CtrlBreak...\n");
			break;
		}
	} while (plen >= 0);
	CtrlCBreak = TRUE;
	hci_close_dev(dd);
}

static void cleanup()
{
    if (cid>=0) {
        close(cid);
    }
    if (sid>=0) {
        close(sid);
    }
}

int sock_recv(int sockid, unsigned char *buf, int buflen)
{
    int recvbytes;
    recvbytes = recv(sockid, buf, buflen, 0);
    if (recvbytes == 0) {
        printf("Connection close!? zero bytes received\n");
        return -1;
    } else if (recvbytes > 0) {
        return recvbytes;
    }
    return -1;
}

int sock_send(int sockid, unsigned char *buf, int bytes)
{
    int cnt;
    unsigned char* bufpos = buf;
    while (bytes) {
        cnt = write(sockid, bufpos, bytes);
	if (cnt != bytes) 
		printf("cnt:%d,bytes:%d\n",cnt, bytes);

        if (!cnt) {
            break;
        }
        if (cnt == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }

        bytes -= cnt;
        bufpos += cnt;
    }
    return (bufpos - buf);
}

static void cmd_btagent(int dev_id, int argc, char **argv)
{
    int dd, i, j, k, l, iRet, need_raw, rx_enable, iDataSize;
    uint32_t m_BerTotalBits, m_BerGoodBits;
    uint8_t m_pattern[16];
    uint16_t m_pPatternlength;
    int port = BT_PORT;
    struct sigaction sa;
    unsigned char buf[HCI_MAX_ACL_SIZE + 3];
    struct hci_filter flt;
    struct hci_dev_info di;
    struct timeval timeout;
    int bytesRead=0;
/* master file descriptor list */
    fd_set master;
    fd_set read_fds;

/* server address */
    struct sockaddr_in serveraddr;

    int fdmax;

/* listening socket descriptor */
    int listener = -1;

/* newly accept()ed socket descriptor */
    int newfd = -1;

    int nbytes;

/* for setsockopt() SO_REUSEADDR, below */
    int yes = 1;

    int addrlen;

    int err;
                
    if (argc > 1) 
	port = atoi(argv[1]);
    if (port == 0)
	port = BT_PORT;
    else if (port < 0 || port >65534) {
	perror("\nERROR: Invalid port number\n");
	return;
    }

    if (dev_id < 0)
    	dev_id = hci_get_route(NULL);
    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("\nERROR: Can not open HCI device\n");
        return;
    }

    if (hci_devinfo(dev_id, &di) < 0) {
        perror("Can't get device info\n");
        hci_close_dev(dd);
        return;
    }

        need_raw = 0; //!hci_test_bit(HCI_RAW, &di.flags);

        hci_filter_clear(&flt);
        hci_filter_all_ptypes(&flt);
        hci_filter_all_events(&flt);

        if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
                perror("Can't set filter for hci\n");
                hci_close_dev(dd);
                return;
        }

        if (need_raw) {
                if ((err = ioctl(dd, HCISETRAW, 1) < 0)) {
                        printf("Can't set raw mode on hci %d\n",err);
                        hci_close_dev(dd);
                        return;
                }
        }

    CtrlCBreak = FALSE;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* clear the master and temp sets */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /* get the listener */
    if((listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
       	perror("Server-socket() error lol!");
       	return;
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int)) == -1) {
       	perror("Server-setsockopt() error lol!");
	close(listener);
       	return;
    }

    if(setsockopt(listener, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int)) == -1) {
	perror("Server-setsockopt() error TCP_NODELAY\n");
        close(listener);
        return;
    }

    /* bind */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    memset(&(serveraddr.sin_zero), 0, 8);

    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
   	perror("Server-bind() error lol!");
	close(listener);
    	return;
    }

    /* listen */
    if(listen(listener, 10) == -1) {
   	perror("Server-listen() error lol!");
	close(listener);
     	return;
    }

    /* add the listener to the master set */
    FD_SET(listener, &master);

    /* add hci handler to the master set */
    FD_SET(dd, &master);

    FD_SET(0, &master);
    /* keep track of the biggest file descriptor */
    fdmax = listener;
    if (dd > listener) fdmax = dd;

    printf("Start BtAgent, press 'q' to exit.\n");

    rx_enable = 0;
    m_BerGoodBits = 0;
    m_BerTotalBits = 0;
    m_pattern[0] = 0x0f;
    m_pPatternlength = 1;

    while (1) {
       	read_fds = master;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
       	iRet = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
	if (iRet == -1) {
       	    perror("Server-select() error lol!");
	    if (newfd > 0) close(newfd);
	    close(listener);
	    goto exits;
        }
	if (CtrlCBreak) break;
	if (iRet == 0) continue;

       	/*run through the existing connections looking for data to be read*/
       	for(i = 0; i <= fdmax; i++) {
       	    if(FD_ISSET(i, &read_fds)) {

		if(i == 0) {
	            printf("Shutting down btagent\n");
                    iRet = getchar();
		    if (iRet == 'q') goto exits;
		    continue;
		}
	    
            	if(i == listener) {
                    /* handle new connections */
                    addrlen = sizeof(struct sockaddr_in);
                    if((newfd = accept(listener, (struct sockaddr *)&serveraddr, &addrlen)) == -1) {
                        perror("Server-accept() error lol!");
			goto exits;
                    }
                    else {
                        printf("Server-accept() is OK...%d\n",newfd);
                        FD_SET(newfd, &master); /* add to master set */
                        if(newfd > fdmax) 
                             fdmax = newfd;
		    }
                }
                else if (i == newfd) {
                    /* handle data from a client */
                    if((nbytes = sock_recv(i, buf, sizeof(buf))) < 0) {
                    	/* got error or connection closed by client */
                        close(i);
                        /* remove from master set */
                        FD_CLR(i, &master);
                    }
                    else {

                	for (j=0; j<nbytes; j++)
                            printf("%x ",buf[j]);
                	printf("\n");
			if (buf[0] == 0x7) {	// BTAGENT_CMD_EVENT
			    if (buf[3] == 0x01) {	// BTAGENT_CMD_EVENT_GETBER
			    	buf[11] = (m_BerTotalBits & 0xff000000) >> 24;
			    	buf[10] = (m_BerTotalBits & 0xff0000) >> 16;
			    	buf[9] = (m_BerTotalBits & 0xff00) >> 8;
			    	buf[8] = m_BerTotalBits & 0xff;
			    	buf[7] = (m_BerGoodBits & 0xff000000) >> 24;
			    	buf[6] = (m_BerGoodBits & 0xff0000) >> 16;
			    	buf[5] = (m_BerGoodBits & 0xff00) >> 8;
			    	buf[4] = m_BerGoodBits & 0xff;
			    	buf[3] = 1;		// BTAGENT_CMD_EVENT_GETBER
                            	buf[2] = 0;
                            	buf[1] = 9;
                            	buf[0] = 7;
                            	sock_send(newfd, buf, 9+3);
                            	usleep(2000);
			    }
			    else if (buf[3] == 0x02) {	// BTAGENT_CMD_EVENT_PATTERN
				m_pPatternlength = (uint16_t)(buf[1] | (buf[2] << 8));
				m_pPatternlength --;
				if (m_pPatternlength > 16) m_pPatternlength = 16;
				memcpy(m_pattern,&buf[4],m_pPatternlength);
				printf("PatternLength:%d,%x\n",m_pPatternlength,buf[4]);
			    }
			    continue;
                        }
			    
			if (rx_enable == 1) {
			    if ((buf[4] == 0x03) && (buf[5] == 0x0c))
				rx_enable = 0;
			}    
			write(dd, &buf[3], nbytes - 3);
		    }
		}
		else if (i == dd) {
		    bytesRead += read(i, &buf[3+bytesRead], sizeof(buf) - 3 - bytesRead);
		    if(buf[3]!= 0x2) {
		       PRINTD("btagent: OUT OF SYNC\n");
		       iRet = bytesRead;
		       bytesRead = 0;
		       continue;
		    }
		    if(bytesRead < (buf[6] | (buf[7] << 8)) && bytesRead < sizeof(buf) -3) { 
		       PRINTD("btagent: read %d bytes, reading more\n", bytesRead);
		       continue;
		    }
		    else {
		       nbytes = bytesRead;
		       bytesRead = 0;
                    }

		    iDataSize = nbytes - 5;
		    PRINTD("btagent: nbyte:%d, packet:%d, pattern:%x\n",nbytes, (uint16_t)(buf[6] | (buf[7] << 8)), buf[8]);
		    if (buf[3] == 0x2) {	// ACL data
			if (rx_enable) {
			    m_BerTotalBits = m_BerTotalBits + iDataSize * 8;
			    for(j=0,l=0;j<iDataSize;j++,l++) {
				if (l == m_pPatternlength) l = 0;
				for(k=0;k<8;k++){
				    if((m_pattern[l]&(1<<k)) == (buf[8+j]&(1<<k)))
				    m_BerGoodBits++;

				}
				PRINTD("byte#:%d, byet:%x, pattern:%x\n", l, buf[8+j],m_pattern[l] );
			    }
			}
		    }
		    else {
		        if ((buf[7] == 0x5b) && (buf[8] == 0xfc)) {		// Rx start CMD's event
			    rx_enable = 1;
			    m_BerTotalBits = 0;
			    m_BerGoodBits = 0;
			}
			buf[2] = 0;
			buf[1] = (uint16_t)nbytes;
			buf[0] = 3;
			if (newfd > 0) {
			    sock_send(newfd, buf, nbytes+3);
			    usleep(2000);
			}
		    }
		}
	    }
        }
    }
exits:
        if (need_raw) {
                if (ioctl(dd, HCISETRAW, 0) < 0)
                        perror("Can't clear raw mode \n");
        }

    hci_close_dev(dd);
    if (listener > 0) close(listener);
    if (newfd > 0) close(newfd);
    printf("Total:%d,Good:%d\n",m_BerTotalBits, m_BerGoodBits);
}

static void sig_term(int sig)
{
	if (CtrlCBreak) return;
	CtrlCBreak = TRUE;
}

static struct {
	char *cmd;
	char *cmd_option;
	void (*func)(int dev_id, int argc, char **argv);
	char *doc;
} command[] = {
	{ "psreset","      ",   cmd_psreset,    "Download PS files and Reset Target"                },
	{ "reset","      ",   cmd_reset,    "Reset Target"                },
	{ "rba","       ",  cmd_rba,    "Read BD Address"                },
	{ "wba","<bdaddr> ",   cmd_wba,    "Write BD Address"                },
	{ "edutm","       ",  cmd_edutm,    "Enter DUT Mode"                },
	{ "wsm","<mode>  ",   cmd_wsm,    "Write Scan Mode"                },
	{ "mb","       ",   cmd_mb,    "Enter Master Blaster Mode"                },
	{ "mbr","<address> <length>  ",   cmd_mbr,    "Block memory read"                },
	{ "peek","<address> <width>  ",   cmd_peek,    "Read Value of an Address"                },
	{ "poke","<address> <value> <mask> <width>  ",   cmd_poke,    "Write Value to an Address"                },
	{ "cwtx","<channel number> ",   cmd_cwtx,    "Enter Continuous wave Tx"                },
	{ "cwrx","<channel number> ",   cmd_cwrx,    "Enter Continuous wave Rx"                },
	{ "rpst","<length> <id>  ",   cmd_rpst,    "Read PS Tag"                },
	{ "wpst","<length> <id> <data> ",   cmd_wpst,    "Write PS Tag"                },
	{ "psr","       ",   cmd_psr,    "PS Reset"                },
	{ "setap","<storage medium> <priority>",   cmd_setap,    "Set Access Priority"                },
	{ "setam","<storage medium> <access mode>",   cmd_setam,    "Set Access Mode"               },
	{ "rpsraw","<offset> <length>  ",   cmd_rpsraw,    "Read Raw PS"                },
	{ "wpsraw","<offset> <length>  <data>",   cmd_wpsraw,    "Write Raw PS"                },
	{ "ssm","<disable|enable>         ", cmd_ssm, "Set Sleep Mode"      },
	{ "dtx","         ", cmd_dtx, "Disable TX"      },
	{ "dump","<option>         ", cmd_dump, "Display Host Controller Information"      },
	{ "rafh","<connection handle>         ", cmd_rafh, "Read AFH channel Map"      },
	{ "safh","<channel classification>         ", cmd_safh, "Set AFH Host Channel Classification"      },
	{ "wotp", "<address> <data> [length=1]", cmd_wotp, "Write Length (default 1) bytes of Data to OTP started at Address"      },
	{ "rotp", "<address> [length=1]", cmd_rotp, "Read Length (default 1) bytes of Data to OTP started at Address"	},
	{ "otp", "[dump|imp|exp|test|rpid|wpid|rvid|wvid|rba|wba|hid|cpw|pwridx|ledo] [file]; opt wba <BdAddr>", 
		cmd_otp, "Misc OTP operation: dump/import otp content; imp file content into otp; test otp; otp wba <BdAddr>"	}, 
	{ "plb", "[1|0]", cmd_plb, "Enable/disable PCM CODEC loopback"	},
	{ "psw", "[1|0] [Frequency]", cmd_psw, "Enable/disable PCM sine wave playback at frequency (0..3700)"	},
	{ "lert", "<rx_channel>", cmd_lert, "Put unit in LE RX mode at rx_channel (0..39)"	},
	{ "lett", "<tx_channel> <length> <packet_payload>", cmd_lett, "Put unit in LE TX mode at tx_channel (0..39) with packet of given length (0..37) and packet_payload"	},
	{ "lete", "        ", cmd_lete, "End LE test"	},
	{ "tput-s", "[BD_Addr] [Judgment value] Logfile times data_size", cmd_tputs, "Throughput test - sender side"	},
	{ "tput-r", "        ", cmd_tputr, "Throughput test - receiver side"	},
	{ "btagent","<port number>", cmd_btagent, "BT Agent for IQFact" }, 
	{ "cmdline","<port number>", cmdline, "command line for Enable TX test mode" }, 
	{ NULL, NULL, NULL, NULL }
};
/*
	{ "get_id",   cmd_gid,    "Get Chip Identification Number"                },
*/
static void usage(void)
{
	int i;

	printf("btconfig - BTCONFIG Tool ver %s\n", VERSION);
	printf("Usage:\n"
		"\tbtconfig [options] <command> [command parameters]\n");
	printf("Options:\n"
		"\t--help\tDisplay help\n"
		"\t-i dev\tHCI device\n");
	printf("Commands:\n");
	for (i = 0; command[i].cmd; i++)
		printf("\t%-8s %-40s\t%s\n", command[i].cmd,command[i].cmd_option,command[i].doc);
	printf("\n"
		"For more information on the usage of each command use:\n"
		"\tbtconfig <command> --help\n" );
}

int main(int argc, char *argv[])
{
	int opt, i, dev_id = -1;
	bdaddr_t ba;

	while ((opt=getopt_long(argc, argv, "+i:hz", main_options, NULL)) != -1) {
		switch (opt) {
		case 'i':
			dev_id = hci_devid(optarg);
			if (dev_id < 0) {
				perror("Invalid device");
				exit(1);
			}
			break;

		case 'z':
			print_debug = TRUE;
			break;

		case 'h':
		default:
			usage();
			exit(0);
		}
	}

#define DEBUG_FILE_NAME "/tmp/btconfig_debug.log"
	debug_log_filep = fopen(DEBUG_FILE_NAME,"r");
	if(debug_log_filep)
	{
		fclose(debug_log_filep);
		debug_log_filep=fopen(DEBUG_FILE_NAME,"w+");
		PRINTD("<<<<START>>>>>\n");
	}

	argc -= optind;
	argv += optind;
	optind = 0;

	if (argc < 1) {
		usage();
		exit(0);
	}

	if (dev_id != -1 && hci_devba(dev_id, &ba) < 0) {
		perror("Device is not available");
		exit(1);
	}

	for (i = 0; command[i].cmd; i++) {
		if (strcmp(command[i].cmd, argv[0]))
			continue;
		command[i].func(dev_id, argc, argv);
		break;
	}

	if(debug_log_filep)
		fclose(debug_log_filep);
	return 0;
}


// MAster BLaster fucntions
tMasterBlasterField MasterBlasterMenu[] =
{
   {"ContRxMode", "n", "toggle coNtinuous Rx", 0,                     ContRxModeOption,   SetMasterBlasterContRxMode},
   {"ContTxMode", "c", "toggle Continuous Tx", 0,                     ContTxModeOption,   SetMasterBlasterContTxMode},
   {"LERxMode",   "q", "toggle LE Rx mode",    0,                     ContRxModeOption,   SetMasterBlasterLERxMode},
   {"LETxMode",   "w", "toggle LE Tx mode",    0,                     ContTxModeOption,   SetMasterBlasterLETxMode},
   {"LETxPktPayload", "y", "set LE Tx packet payload", 0,             LETxPktPayloadOption, SetMasterBlasterLETxPktPayload},
   {"ContTxType", "u", "toggle continUous Tx Type", Cont_Tx_Raw_1MHz, ContTxTypeOption,   SetMasterBlasterContTxType},
   {"TestMode",   "m", "toggle test Mode",     eBRM_TestMode_TX_1010, TestModeOption,     SetMasterBlasterTestMode},
   {"HopMode",    "h", "toggle Hop mode",      0,                     HopModeOption,      SetMasterBlasterHopMode},
   {"TxFreq",     "t", "set Tx freq",          39,                    NULL,               SetMasterBlasterTxFreq},
   {"RxFreq",     "r", "set Rx freq",          39,                    NULL,               SetMasterBlasterRxFreq},
   {"PacketType", "p", "toggle Packet type",   TxTest_PktType_DH1,    PacketTypeOption,   SetMasterBlasterPacketType},
   {"DataLen",    "l", "set data Length",      15,                    NULL,               SetMasterBlasterDataLen},
   {"Power",      "o", "toggle pOwer",         9,                     NULL,               SetMasterBlasterPower},
   {"BdAddr",     "a", "set bdAddr",           0,                     NULL,               SetMasterBlasterBdAddr},
   {"SetBERType", "k", "set BER type",         eBRM_BERMode_ALL_DATA, BERPacketTypeOption,SetMasterBlasterBERType},
   {"GetBER",     "g", "get BER type",	       0,		      NULL,		  SetMasterBlasterNothing},
   {"EnableRxTest", "d", "Enable rx test mode", 0,                    NULL,               SetMasterBlasterNothing},
   {"EnableTxTest", "e", "Enable tx test mode", 0,                    NULL,               SetMasterBlasterNothing},
   {"EnableTest", "j", "Start test mode",      0,                     NULL,               SetMasterBlasterNothing},
   {"StopTest",   "s", "Stop test mode",       0,                     NULL,               SetMasterBlasterNothing},
   {"Exit",       "x", "eXit",                 0,                     NULL,               SetMasterBlasterNothing},
   {"ExitWithoutReset", "b", "Exit without reset", 0,                 NULL,               SetMasterBlasterNothing},
};

tPsSysCfgTransmitPowerControlTable  TpcTable;

//----------------------------------------------------------------------------

void InitMasterBlaster (tBRM_Control_packet *MasterBlaster, bdaddr_t *BdAddr, UCHAR *SkipRxSlot)
{
   *SkipRxSlot = 0x01;	
   MasterBlaster->testCtrl.Mode     = MasterBlasterMenu[TM].Default;
   MasterBlaster->testCtrl.HopMode  = MasterBlasterMenu[HM].Default;
   MasterBlaster->testCtrl.Packet   = MasterBlasterMenu[PT].Default;
   MasterBlaster->testCtrl.TxFreq   = MasterBlasterMenu[TF].Default;
   MasterBlaster->testCtrl.RxFreq   = MasterBlasterMenu[RF].Default;
   MasterBlaster->testCtrl.Power    = MasterBlasterMenu[PO].Default;
   MasterBlaster->testCtrl.DataLen  = MasterBlasterMenu[DL].Default;
   MasterBlaster->ContTxMode        = MasterBlasterMenu[CT].Default;
   MasterBlaster->ContTxType        = MasterBlasterMenu[CX].Default;
   MasterBlaster->ContRxMode        = MasterBlasterMenu[CR].Default;
   MasterBlaster->BERType           = MasterBlasterMenu[SB].Default;
   MasterBlaster->LERxMode          = MasterBlasterMenu[LR].Default;
   MasterBlaster->LETxMode          = MasterBlasterMenu[LT].Default;
   MasterBlaster->LETxParms.PktPayload = MasterBlasterMenu[LTM].Default;
   memcpy(MasterBlaster->bdaddr,&BdAddr->b[0],6);

   TpcTable.NumOfEntries = 0;
}

//----------------------------------------------------------------------------

int CheckField (tBRM_Control_packet MasterBlaster, char *FieldAlias)
{
   if (((!strncmp(FieldAlias,MasterBlasterMenu[HM].Alias,1)) && MasterBlaster.ContTxMode) ||
       (((!strncmp(FieldAlias,MasterBlasterMenu[TF].Alias,1)) || (!strncmp(FieldAlias,MasterBlasterMenu[RF].Alias,1))) && MasterBlaster.testCtrl.HopMode == 1) ||
       ((!strncmp(FieldAlias,MasterBlasterMenu[CX].Alias,1)) && MasterBlaster.ContTxMode == 0))
   {
	 return INVALID_MASTERBLASTER_FIELD;
   }
   unsigned int i;
   for (i = 0; i < sizeof(MasterBlasterMenu)/sizeof(tMasterBlasterField); ++i)
   {
      if (!strncmp(FieldAlias,MasterBlasterMenu[i].Alias,1))
      {
         return i;
      }
   }

   return INVALID_MASTERBLASTER_FIELD;
}

//----------------------------------------------------------------------------

int GetTestModeOptionIndex (int Value)
{
   unsigned int i;
   for (i = 0; i < sizeof(TestModeOption)/sizeof(tMasterBlasterOption); ++i)
   {
      if (Value == TestModeOption[i].Value)
      {
         return i;
      }
   }
  // assert (0);
   return -1;
}

//----------------------------------------------------------------------------

int GetPacketTypeOptionIndex (int Value)
{
   unsigned int i;
   for (i = 0; i < sizeof(PacketTypeOption)/sizeof(tMasterBlasterOption); ++i)
   {
      if (Value == PacketTypeOption[i].Value)
      {
         return i;
      }
   }
   //assert (0);
   return -1;
}

//----------------------------------------------------------------------------

void PrintMasterBlasterMenu(tBRM_Control_packet *MasterBlaster)
{
	unsigned int i;
	printf ("\n---------- Master Blaster Mode ----------\n\n");
	for (i = 0; i < sizeof(MasterBlasterMenu)/sizeof(tMasterBlasterField); ++i)
	{
		if (((i == HM || i == RF) && (MasterBlaster->ContTxMode == ENABLE)) ||
			((i == TF || i == RF) && (MasterBlaster->testCtrl.HopMode == 1)) ||
			((i == CX) && (MasterBlaster->ContTxMode == DISABLE)) ||
			((i == CX || i == HM || i == TF || i == PT || i == DL || i == PO || i == BA) &&
			(MasterBlaster->ContRxMode == ENABLE)))
		{
			continue;
		}

		printf ("\t%s - %s\n", MasterBlasterMenu[i].Alias, MasterBlasterMenu[i].Usage);
	}
	printf ("\n-----------------------------------------\n\n");

	char BdAddr[18];
	//strcpy(MasterBlaster.bdaddr,BdAddr);

	printf ("ContRxMode: %s\n", ContRxModeOption[MasterBlaster->ContRxMode].Name);
	printf ("ContTxMode: %s\n", ContTxModeOption[MasterBlaster->ContTxMode].Name);
	printf ("LERxMode: %s\n", ContTxModeOption[MasterBlaster->LERxMode].Name);
	printf ("LETxMode: %s\n", ContTxModeOption[MasterBlaster->LETxMode].Name);

	// LE Rx mode
	if (MasterBlaster->LERxMode == ENABLE)
	{
		if (MasterBlaster->testCtrl.RxFreq > MB_MAX_FREQUENCY_LE)
			MasterBlaster->testCtrl.RxFreq = MB_MAX_FREQUENCY_LE;
		printf("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
	}
	// LE Tx mode
	if (MasterBlaster->LETxMode == ENABLE)
	{
		if (MasterBlaster->testCtrl.DataLen > MB_MAX_DATALEN_LE)
			MasterBlaster->testCtrl.DataLen = MB_MAX_DATALEN_LE;
		printf("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
		printf("DataLen:    %d\n", MasterBlaster->testCtrl.DataLen);
		printf("PktPayload: %s\n", LETxPktPayloadOption[MasterBlaster->LETxParms.PktPayload].Name);
	}
	// Continous Rx mode
	else if (MasterBlaster->ContRxMode == ENABLE)
	{
		printf ("BER Type: %s\n",BERPacketTypeOption[MasterBlaster->BERType].Name);
		printf ("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
	}
	// Continous Tx mode and Tx test mode
	else
	{
		printf ("BER Type: %s\n",BERPacketTypeOption[MasterBlaster->BERType].Name);
		if (MasterBlaster->ContTxMode == ENABLE)
		{
			printf ("ContTxType: %s\n", ContTxTypeOption[MasterBlaster->ContTxType].Name);
			if (ContTxTypeOption[MasterBlaster->ContTxType].Value != CW_Single_Tone)
				printf ("TestMode:   %s\n", TestModeOption[GetTestModeOptionIndex(MasterBlaster->testCtrl.Mode)].Name);
			printf ("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
		}
		else
		{
			printf ("TestMode:   %s\n", TestModeOption[GetTestModeOptionIndex(MasterBlaster->testCtrl.Mode)].Name);
			printf ("HopMode:    %s\n", HopModeOption[MasterBlaster->testCtrl.HopMode].Name);

			if (MasterBlaster->testCtrl.HopMode == 0)
			{
				printf ("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
				printf ("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
			}
		}
		if (TpcTable.NumOfEntries > 0)
		{
			printf ("Power:      Step = %d/%d; Level = %d dBm\n", MasterBlaster->testCtrl.Power+1,
			TpcTable.NumOfEntries, TpcTable.t[MasterBlaster->testCtrl.Power].TxPowerLevel);
		}
		else
		{
			printf ("Power:      Step = Max; Level = N/A\n");
		}
		if ((MasterBlaster->ContTxMode == ENABLE && ContTxTypeOption[MasterBlaster->ContTxType].Value == Cont_Tx_Regular) ||
		(MasterBlaster->ContTxMode == DISABLE))
		{
			printf ("PacketType: %s\n", PacketTypeOption[GetPacketTypeOptionIndex(MasterBlaster->testCtrl.Packet)].Name);
			printf ("DataLen:    %d\n", MasterBlaster->testCtrl.DataLen);
		}
		if (ContTxTypeOption[MasterBlaster->ContTxType].Value != CW_Single_Tone) {//for single tone, no bdaddr
			ba2str((const bdaddr_t *)MasterBlaster->bdaddr, BdAddr);
			printf ("BdAddr:     0x%s\n\n",BdAddr);
		}
	}
	printf ("\nmb>\n");
}

//----------------------------------------------------------------------------

int SetMasterBlasterTestMode(tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
	int Value = (int)MasterBlaster->testCtrl.Mode;

	if (ToggleOption (&Value, Option, TestModeOption,
			  sizeof(TestModeOption)/sizeof(tMasterBlasterOption), TM,1))
	{
		MasterBlaster->testCtrl.Mode = (UCHAR)Value;
		// Enable continous Tx should disable continous Rx
		MasterBlaster->ContRxMode = DISABLE;
		MasterBlaster->ContTxMode = DISABLE;
		return TRUE;
	}
	return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterHopMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->testCtrl.HopMode;

   if (ToggleOption (&Value, Option, HopModeOption,
                        sizeof(HopModeOption)/sizeof(tMasterBlasterOption), HM,1))
   {
      MasterBlaster->testCtrl.HopMode = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterTxFreq (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   //char Buffer[20];
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int Value = (int)MasterBlaster->testCtrl.TxFreq;
   int MaxFreq = LEMode ? MB_MAX_FREQUENCY_LE : MB_MAX_FREQUENCY;
   int MinFreq = LEMode ? MB_MIN_FREQUENCY_LE : MB_MIN_FREQUENCY;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter Tx frequency (%d..%d): ", MinFreq, MaxFreq);
      scanf("%d",&NewValue.Value);
//    fgets(NewValue,3,stdin);
      if (MinMaxOption (&Value, &NewValue, MinFreq, MaxFreq))
      {
         MasterBlaster->testCtrl.TxFreq = (UCHAR)Value;
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid Tx frequency.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterRxFreq (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int Value = (int)MasterBlaster->testCtrl.RxFreq;
   int MaxFreq = LEMode ? MB_MAX_FREQUENCY_LE : MB_MAX_FREQUENCY;
   int MinFreq = LEMode ? MB_MIN_FREQUENCY_LE : MB_MIN_FREQUENCY;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter Rx frequency (%d..%d): ", MinFreq, MaxFreq);
      scanf("%d",&NewValue.Value);
      if (MinMaxOption (&Value, &NewValue, MinFreq, MaxFreq))
      {
         MasterBlaster->testCtrl.RxFreq = (UCHAR)Value;
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid Rx frequency.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterPacketType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->testCtrl.Packet;

   if (ToggleOption (&Value, Option, PacketTypeOption,
                        sizeof(PacketTypeOption)/sizeof(tMasterBlasterOption), PT,1))
   {
      MasterBlaster->testCtrl.Packet = (UCHAR)Value;
      MasterBlaster->testCtrl.DataLen = MaxDataLenOption[GetPacketTypeOptionIndex(Value)];
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterDataLen (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int MaxLen = LEMode ? MB_MAX_DATALEN_LE : MB_MAX_DATALEN;
   int MinLen = LEMode ? MB_MIN_DATALEN_LE : MB_MIN_DATALEN;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter data length (%d..%d): ", MinLen, MaxLen);
      scanf("%d",&NewValue.Value);
      if (MinMaxOption (&MasterBlaster->testCtrl.DataLen, &NewValue, MinLen, MaxLen))
      {
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid data length.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterPower (tBRM_Control_packet *MasterBlaster, char *Option)
{
   if (TpcTable.NumOfEntries > MAX_TRANSMIT_POWER_CONTROL_ENTRIES)
   {
      printf ("\nNumber of entries in TPC table exceeds the limit.\n");
      sleep(3);
      return TRUE;
   }

   if (TpcTable.NumOfEntries == 0)
   {
      printf ("\nThere is no entry in TPC table.\n");
      sleep(3);
      return TRUE;
   }

   int Value = (int)MasterBlaster->testCtrl.Power;

   if (ToggleMinMaxOption (&Value, Option, PO, 0, TpcTable.NumOfEntries-1,1))
   {
      MasterBlaster->testCtrl.Power = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterBdAddr (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   char Buffer[20];
   bdaddr_t bdaddr;

   printf ("\n Enter BdAddr: ");
//   gets(Buffer);
   scanf("%s",Buffer);
   str2ba(Buffer,&bdaddr);
   strncpy(MasterBlaster->bdaddr,bdaddr.b,6);
   return TRUE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContTxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContTxMode;

   if (ToggleOption (&Value, Option, ContTxModeOption,
                        sizeof(ContTxModeOption)/sizeof(tMasterBlasterOption), CT,1))
   {
      MasterBlaster->ContTxMode = (UCHAR)Value;
      if (MasterBlaster->ContTxMode == ENABLE)
      {
         // Enable continous Tx should disable continous Rx
         MasterBlaster->ContRxMode = DISABLE;
	 MasterBlaster->LERxMode = DISABLE;
	 MasterBlaster->LETxMode = DISABLE;
	 LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContTxType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContTxType;

   if (ToggleOption (&Value, Option, ContTxTypeOption,
                        sizeof(ContTxTypeOption)/sizeof(tMasterBlasterOption), CX,1))
   {
      MasterBlaster->ContTxType = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterLERxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LERxMode;

   if (ToggleOption (&Value, Option, ContRxModeOption,
                       sizeof(ContRxModeOption)/sizeof(tMasterBlasterOption), LR, 1))
   {
      MasterBlaster->LERxMode = (UCHAR)Value;
      if (MasterBlaster->LERxMode == ENABLE)
      {
         /* Enable continous Tx should disable other modes */
         MasterBlaster->LETxMode = DISABLE;
         MasterBlaster->ContTxMode = DISABLE;
         MasterBlaster->ContRxMode = DISABLE;
         if (MasterBlaster->testCtrl.RxFreq > 39)
         {
            MasterBlaster->testCtrl.RxFreq = 39;
         }
         LEMode = TRUE;
      }
      else
      {
         LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterLETxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LETxMode;

   if (ToggleOption (&Value, Option, ContTxModeOption,
                sizeof(ContTxModeOption)/sizeof(tMasterBlasterOption), LT, 1))
   {
      MasterBlaster->LETxMode = (UCHAR)Value;
      if (MasterBlaster->LETxMode == ENABLE)
      {
         /* Enable continous Tx should disable other modes */
         MasterBlaster->LERxMode = DISABLE;
         MasterBlaster->ContTxMode = DISABLE;
         MasterBlaster->ContRxMode = DISABLE;
         if (MasterBlaster->testCtrl.TxFreq > MB_MAX_FREQUENCY_LE)
         {
            MasterBlaster->testCtrl.TxFreq = 39;
         }
         LEMode = TRUE;
      }
      else
      {
         LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterLETxPktPayload(tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LETxParms.PktPayload;

   if (ToggleOption(&Value, Option, LETxPktPayloadOption,
                    sizeof(LETxPktPayloadOption)/sizeof(tMasterBlasterOption), LTM, 1))
   {
      MasterBlaster->LETxParms.PktPayload = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContRxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContRxMode;
   printf("\n N op\n");
   if (ToggleOption (&Value, Option, ContRxModeOption,
                        sizeof(ContRxModeOption)/sizeof(tMasterBlasterOption), CR,1))
   {
      MasterBlaster->ContRxMode = (UCHAR)Value;
      if (MasterBlaster->ContRxMode == ENABLE)
      {
         // Enable continous Tx should disable continous Rx
         MasterBlaster->ContTxMode = DISABLE;
	 MasterBlaster->LERxMode = DISABLE;
	 MasterBlaster->LETxMode = DISABLE;
	 LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------
int SetMasterBlasterBERType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->BERType;
   if (ToggleOption (&Value, Option, BERPacketTypeOption,
			sizeof(BERPacketTypeOption)/sizeof(tMasterBlasterOption), SB, 1))
   {
      MasterBlaster->BERType = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterNothing (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   UNUSED(MasterBlaster);
   UNUSED(Option);

   return TRUE;
}

//----------------------------------------------------------------------------

int ToggleOption (int *Value, tMasterBlasterOption *Option, tMasterBlasterOption *OptionArray,
                   int Size, int FieldID, int Step)
{
   char Opt = Option->Name[0];

   int Backward = ((Opt - 'A' + 'a') == MasterBlasterMenu[FieldID].Alias[0]);
   int i;
   for (i = 0; i < Size; ++i)
   {
      if (*Value == OptionArray[i].Value)
      {
         if (Backward)
         {
            i = ((i - Step) < 0) ? (Size - Step + i) : (i - Step);
         }
         else
         {
            i = (i + Step) % Size;
         }
         *Value = OptionArray[i].Value;
         return TRUE;
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int MinMaxOption (int *Value,  tMasterBlasterOption *Option, int Min, int Max)
{
   int NewValue = Option->Value;

   if (NewValue < Min || NewValue > Max)
   {
      return FALSE;
   }
   *Value = NewValue;
   return TRUE;
}

//----------------------------------------------------------------------------

int ToggleMinMaxOption (int *Value, char *Option, int FieldID, int Min, int Max, int Step)
{
   char Opt = *Option;
   int Backward = ((Opt - 'A' + 'a') == MasterBlasterMenu[FieldID].Alias[0]);

   if (Backward)
   {
      *Value = ((*Value - Step) < Min) ? (Max + 1 - (Step - (*Value - Min))) : (*Value - Step);
   }
   else
   {
      *Value = ((*Value + Step) > Max) ? (Min + (Step - (Max + 1 - *Value))) : (*Value + Step);
   }
   return TRUE;

}

//----------------------------------------------------------------------------
