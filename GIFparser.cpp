

/************************************************
GIF FILE PARSER
//THIS VERSION IS TESTED WITH VALID GIF FOR BOTH ANIMATED AND NON-ANIMATED FILES.
//ALSO, IT IS TESTED FOR CORRUPTED GIF FILE.  THE PROGRAM DISPLAYS ERROR AND EXIST.
/*********************************************/
//VER4.0 Comments added and GIF parser summary printing is added. Comments Extn section modified to accept multiple
//sub-data blocks until Block Termination Byte is received


#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "stdafx.h"

// SET TO 1 ONLY IF YOU WANT TO PRINT ALL UNCOMPRESSED BYTES IN THE GIF. SUITABLE ONLY FOR SMALL FILES OR WHEN OUTPUT REDIRECTED TO A FILE
#define PRINT_OUT_ALL_UNCOMPRESSED_BYTES 0

#define MAX_GIF_FILE_SIZE 3072000  //3MByte

unsigned char InGifFileBuf[MAX_GIF_FILE_SIZE];
int InFileBufCount = 0;
int processGIFFile();

unsigned char tmpGifFileBuf[MAX_GIF_FILE_SIZE];
int tmpGifFileCount = 0;


#define EXT_INTR   0x21 //EXTENSION INTRODUCER
#define IMG_DSPTR  0x2C //IMAGE DESCRIPTOR
#define TRAILER    0x3B //TRAILER

//STANDARD BLOCKS OF A GIF FILE
unsigned char HeaderBlock_89a[6] = { 0x47,0x49,0x46,0x38,0x39,0x61 }; //GIF89a
unsigned char HeaderBlock_87a[6] = { 0x47,0x49,0x46,0x38,0x37,0x61 }; //GIF87a

typedef struct
{
	int canvas_width;
	int canvas_Height;
	unsigned char GifFields;
	unsigned char GifBackgroundColorIndex;
	unsigned char GifPixelAspectRatio;
}
GIFLocalScreenDescriptor;

GIFLocalScreenDescriptor GIFLTDescriptor;


//Structure for storing the data of Graphic Control Extension
typedef struct
{
	unsigned char GCE_packed_field;
	unsigned char GCE_delay_time[3];
	unsigned char GCE_transparent_color_ind;

} GCE_struct;


//Structure for storing data of Application Extension
typedef struct
{
	unsigned char APP_Ext_application_id[9];
	unsigned char APP_Ext_auth_code[4];

} APP_Ext_struct;


unsigned char BlockTypeByte = 0;
int gblClrTableSize = 0;

int ClrResolutionBits = 0;


//int ProcessImageData();
int LocalColourTable();

FILE *fp = NULL;

//unsigned char ColourPallet[20][100] = { "Default Colour","White","Red","Blue","Black" };
//int MapTheColourPallet(unsigned char ch[3]);


int N = 0;
unsigned char GCTChar = 0, GCTFlag = 0;
int NumOfColours = 0, GCTSize = 0;



//VARIABLES FOR STORING GIF FILE PARAMETERS
unsigned char GIFVerison[3] = {};

int TotalImageDataBlock = 0;
int Extensions_Handler();

//Extension blocks
int Graphic_Cntrl_Extn();
int Plaintext_Extn();
int Application_Extn();
int Comment_Extn();
int ImageData_Blocks_count = 0;
int GCE_Blocks_Count = 0;
int Appli_Ext_Blocks_count = 0;
int Plaintext_Blocks_count = 0;
int Comment_Blocks_Count = 0;


long lzw_uncompress_data_block(const unsigned char* data_block, int data_block_length, unsigned char* output, unsigned long output_length) {

	if (data_block == NULL || output == NULL) {
		printf("Error: uncompress data block function error - uninitialized input or output array.\n");
		return -1;
	};

	//assuming first byte of the data block is the LZW minimum code size
	int minimum_code_size = (int)data_block[0];
	int current_code_size = minimum_code_size;

	typedef struct t_codetable {
		unsigned char index;
		t_codetable* previous_index_ptr;
		unsigned short length;
	} t_codetable;

	if (minimum_code_size < 2 || minimum_code_size > 8) {
		printf("Error: incorrectly encoded image data block - LZW minimum code size is out of range.\n");
		return -1;
	};

	t_codetable codetable[4096];   // allocate a codetable of maximal length

	for (int i = 0; i < (1 << minimum_code_size); i++) {
		codetable[i].index = (unsigned char)i;
		codetable[i].previous_index_ptr = NULL;
		codetable[i].length = 1;
	};

	int clear_code = (1 << minimum_code_size);
	int end_of_information_code = clear_code + 1;
	int next_codetable_position = clear_code + 2;

	int current_subblock_length = 0;
	int current_subblock_position = 0;
	int current_block_position = 1;
	int current_output_position = 0;
	int bits_read = 0;
	int bitmask_position = 8; // set to 8 to indicate that we need to read new byte in the first run of the cycle
	unsigned char current_byte = 0;
	int codeword = 0;
	unsigned char bitmask[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	t_codetable* previous_code_ptr = NULL;
	t_codetable* current_code_ptr = NULL;

	//	current_subblock_length = data_block[1];

	while (1) {  // this cycles through data subblocks

		if (current_subblock_position == 0) {
			/* assumptions:
			current block position is next subblock length byte
			*/

			current_subblock_length = data_block[current_block_position];

			// check if subblock end is 0 (expectedly or unexpectedly)
			if (current_subblock_length == 0) {
				if (current_block_position != data_block_length - 1) {
					printf("Error: incorrectly encoded image data block - 0x00 block terminator encountered inside the block\n");
					return -1;
				}
				else break;  // we have reached the end of the data block
			};

			// check if current subblock length does not reach out of the whole data_block array
			if (current_block_position + current_subblock_length >= data_block_length) {
				printf("Error: incorrectly encoded image data block - incorrect sub-block length \n");
				return -1;
			};

			current_block_position++;
			current_subblock_position++;
		};

		// while subblock position <= subblock length, continue reading bits from subblock bytes and process
		while (current_subblock_position <= current_subblock_length) {
			// if current_byte was completely processed, read new byte
			if (bitmask_position > 7) {
				current_byte = data_block[current_block_position];
				bitmask_position = 0;
				current_block_position++;
				current_subblock_position++;
			};

			// read necessary number of bits from the byte
			while (bits_read < current_code_size + 1) {
				if (current_byte & bitmask[bitmask_position]) codeword += 1 << bits_read;
				bits_read++;
				bitmask_position++;
				if (bitmask_position > 7) break; //we have read all bits in the byte and need to escape the cycle to read another one
			};
			if (bits_read >= current_code_size + 1) break;
		};

		/* if we are here, we either have:
		- a complete codeword in codeword (bits_read==current_code_size + 1), so we need to process the codeword
		- processed the last byte in the subblock (current_subblock_position>current_subblock_length),
		so we need to move to first byte of next subblock
		*/

		if (bits_read > current_code_size + 1) {
			//this should never happen
			printf("Error: Unexpected internal LZW decompression error\n");
			return -1;
		}

		if (bits_read == current_code_size + 1) {
			// process the codeword
			// printf("%d\t%d\t%d\n", codeword, current_code_size, next_codetable_position);

			if (codeword == end_of_information_code) {
				// end_of_information code

				if (8 * (data_block_length - current_block_position - 1) + 7 - bitmask_position>current_code_size) {
					// if end_of_information code is not the last codeword before end of data block, i.e. if another codeword
					// fits in before the end of the block
					printf("Error: LZW uncompression error - end-of-information code encountered before end of block\n");
					return -1;
				}
				else {
					// reached end of compressed data
					// no action
				};
			}

			else if (codeword == clear_code) {
				// clear_code

				previous_code_ptr = NULL;
				next_codetable_position = clear_code + 2;
				current_code_size = minimum_code_size;

			}

			else {
				// any non-special codeword

				if (codeword < next_codetable_position) {
					// the codeword is already in the codetable

					//	output{ CODE } to output stream
					//	let K be the first index in { CODE }
					//	add{ CODE - 1 } + K to the code table

					current_code_ptr = &codetable[codeword];

					for (int i = codetable[codeword].length; i > 0; i--) {
						output[current_output_position + i - 1] = ((t_codetable)(*current_code_ptr)).index;
						current_code_ptr = ((t_codetable)(*current_code_ptr)).previous_index_ptr;
					};

					current_output_position += codetable[codeword].length;

					if (previous_code_ptr) {
						codetable[next_codetable_position].index = output[current_output_position - 1];
						//codetable[next_codetable_position].index = output[codetable[codeword].length - 1];
						codetable[next_codetable_position].previous_index_ptr = previous_code_ptr;
						codetable[next_codetable_position].length = ((t_codetable)(*previous_code_ptr)).length + 1;
						next_codetable_position++;
					}
					else {
						//	no action
					};

					previous_code_ptr = &codetable[codeword];

				}
				else if (codeword == next_codetable_position) {
					//  the codeword is not in the codetable yet and needs to be added

					//  let K be the first index of{ CODE - 1 }
					//	add{ CODE - 1 }+K to code table
					//	output{ CODE - 1 }+K to index stream

					codetable[next_codetable_position].index = ((t_codetable)(*previous_code_ptr)).index;
					codetable[next_codetable_position].length = ((t_codetable)(*previous_code_ptr)).length + 1;
					codetable[next_codetable_position].previous_index_ptr = previous_code_ptr;

					current_code_ptr = &codetable[next_codetable_position];

					for (int i = codetable[next_codetable_position].length; i > 0; i--) {
						output[current_output_position + i - 1] = ((t_codetable)(*current_code_ptr)).index;
						current_code_ptr = ((t_codetable)(*current_code_ptr)).previous_index_ptr;
					};


					next_codetable_position++;
					current_output_position += codetable[codeword].length;
					previous_code_ptr = &codetable[codeword];

				}
				else if (codeword > next_codetable_position) {
					printf("Error: Error LZW decompressing - code %d encountered, but codetable size is %d\n", codeword, next_codetable_position);
					return -1;
				};

				// check if codetable size requires increasing code length 				
				if ((next_codetable_position >= 1 << (current_code_size + 1)) /*&& (current_code_size < 11)*/) current_code_size++;
				//next_codetable_position++;
			};

			bits_read = 0;
			codeword = 0;
		};

		if (current_subblock_position > current_subblock_length) {
			//reinitialize the subblock position counters and move to next byte, which should be the length byte of next subblock.
			//Then go to the beginning of the outer while loop
			current_subblock_position = 0;
		};
	};
	return current_output_position;
};


//ProcessImageData() function - Reads actual Image bytes and puts them into InGifFileBuf
int ProcessImageData(unsigned int height, unsigned int width) {
	unsigned char ch = 0;
	int NextDataSubBlockSize = 0;
	InFileBufCount = 0;

	ImageData_Blocks_count++;

	ch = fgetc(fp);
	if (ferror(fp)) {
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}

	printf("\n\nINSIDE IMAGE DATA BLOCK PROCESSING \n");

	InGifFileBuf[InFileBufCount++] = ch;
	printf("LZW MINIMUM CODE SIZE = %d\n", ch);

	ch = fgetc(fp);
	if (ferror(fp) || feof(fp)) {
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}

	while (ch != 0x00) {   // ch is the length of the next subblock
		InGifFileBuf[InFileBufCount++] = ch;
		NextDataSubBlockSize = ch;
		for (int i = 0; i<NextDataSubBlockSize; i++) {
			ch = fgetc(fp);

			if (ferror(fp) || feof(fp)) {
				printf("Error in reading the file. Quit!!\n");
				return 0;
			}

			InGifFileBuf[InFileBufCount++] = ch;
		}

		ch = fgetc(fp);

		if (ferror(fp)|| feof(fp)) {
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
	}

	InGifFileBuf[InFileBufCount] = ch;
	// InFileBufCount+1 is the length of the image data block, image data block is stored in InGifFileBuf

	unsigned char* uncompressed = NULL;
	if (NULL == (uncompressed = (unsigned char*)malloc(width*height * sizeof(unsigned char)))) {
		printf("Error: Not enough memory to process image data block.\n");
		return 0;
	}

	long uncompressed_length = lzw_uncompress_data_block(InGifFileBuf, InFileBufCount + 1, uncompressed, width*height);
	if (uncompressed_length < 0) {
		free(uncompressed);
		return 0;
	}

	if (uncompressed_length != width*height) {
		printf("Error: Unexpected number of bytes in the uncompressed stream.\n");
		free(uncompressed);
		return 0;
	}

	if (PRINT_OUT_ALL_UNCOMPRESSED_BYTES) {
		//set this global variable to TRUE only if you want to print out all the uncompressed image bytes.
		//suitable only for small size GIFs or when output is directed to a file.

		printf("UNCOMPRESSED BYTES:\n");
		
		for (long i = 0; i < uncompressed_length; i++) {
			if (i%width == 0) printf("\n");
			printf("%3d ", uncompressed[i]);
		}
	}

	printf("\n\nNUMBER OF BYTES IN UNCOMPRESSED DATA BLOCK: %d\n", uncompressed_length);
	printf("\n\nEND OF IMAGE DATA BLOCK PROCESSING\n");

	free(uncompressed);

	return 1;
};


//main() function - Receives GIF File file and calls processGIFFile() for parsing
int main(int argc, char *argv[])
{

	unsigned char fileName[100];
	int retValue = 0;

	memset(InGifFileBuf, 0, sizeof(InGifFileBuf));

	printf("LET'S START\n");


	printf("ENTER THE SOURCE .GIF FILE FOR PARSING (max. 100 characters):\n");
	scanf_s("%s", &fileName, 100);
	fopen_s(&fp, (const char *)fileName, "rb");
	printf("name = %s \n", fileName);




	if (fp == NULL)
	{
		printf("Sorry, The .gif source file not found\n");
		return(0);
	}
	else
	{

		printf("SOURCE .GIF FILE OPEN SUCCESSFUL!!!!!\n");
	}


	retValue = processGIFFile();
	fclose(fp);
	if (retValue == 0)
	{
		printf("\n!!!!!! GIF FILE NOT PROCESSED\n ");
		return 0;
	}
	else
	{

		printf("\nGIF FILE SUCCESSFULLY PROCESSED");
		printf("\n**********************************************");
		printf("\nBrief Summary of GIF Parser:");
		printf("\n**********************************************");
		printf("\nNo.of Image Data Blocks = %d", ImageData_Blocks_count);
		printf("\nNo.of Graphic Control Extension Blocks = %d", GCE_Blocks_Count);
		printf("\nNo.of Application Extension Blocks = %d", Appli_Ext_Blocks_count);
		printf("\nNo.of Plaintext Extension Blocks = %d", Plaintext_Blocks_count);
		printf("\nNo.of Comment Extension Blocks = %d", Comment_Blocks_Count);
		printf("\n**********************************************");
		printf("\n");

	}


	return 1;

} //end of main



  //processGIFFile() - It reads GIF file and parses it; calls varous functions to complete the task
int processGIFFile()
{

	int i = 0, j = 0, k = 0;
	long int m = 0;
	unsigned char ch[3] = { 0 };
	unsigned char ch1 = 0;


	InFileBufCount = 0;

	//   memcpy(InGifFileBuf,0x00,200);
	fseek(fp, 0, SEEK_SET);

	for (i = 0; i<6; i++)
	{
		ch[0] = fgetc(fp);  //Header (6Bytes) reading
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}

		InGifFileBuf[InFileBufCount++] = ch[0];

		// printf("%2x ",ch[0]&0x00ff);

	}



	printf("\n\n\n");
	printf("THE 6 HEADER BYTES ARE: ");
	for (i = 0; i<6; i++)
		printf("%x ", InGifFileBuf[i]);
	printf("\n");


	//FIRST CHECK FOR EXISTANCE OF A HEADER BLOCK
	i = memcmp(InGifFileBuf, HeaderBlock_89a, 6);
	j = memcmp(InGifFileBuf, HeaderBlock_87a, 6);

	if ((i != 0) && (j != 0))
	{
		printf("Error! The source file is not a valid .GIF file\n");

		printf("Header Block not found\n");

		return(0);
	}

	//   printToConsole("\nHEADER BLOCK FOUND!!!!!!\n",24,0);

	InFileBufCount = 6;

	//copy the .GIF VERSION
	if (i == 0)
	{
		GIFVerison[0] = HeaderBlock_89a[3];
		GIFVerison[1] = HeaderBlock_89a[4];
		GIFVerison[2] = HeaderBlock_89a[5];
	}
	else if (j == 0)
	{
		GIFVerison[0] = HeaderBlock_87a[3];
		GIFVerison[1] = HeaderBlock_87a[4];
		GIFVerison[2] = HeaderBlock_87a[5];
	}

	printf("\nThe Version of GIF is %c%c%c\n", GIFVerison[0], GIFVerison[1], GIFVerison[2]);

	if (memcmp(GIFVerison, "89a", 3) != 0 && memcmp(GIFVerison, "87a", 3) != 0) {
		printf("Not a valid GIF File\n");
		return(0);
	}

	InFileBufCount = 0;
	for (i = 0; i<7; i++)
	{
		ch[0] = fgetc(fp); //LSD
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return(0);
		}

		InGifFileBuf[InFileBufCount++] = ch[0];


	}

	printf("\n\n\n");
	printf("THE 7 BYTES OF LOGICAL SCREEN DESCRPTOR ARE: ");
	for (i = 0; i<7; i++)
		printf("%x ", InGifFileBuf[i]);
	printf("\n");

	InFileBufCount = 0;

	//NEXT GET THE CANVAS WIDTH
	//COPY THE 2ND BYTES FIRST AS IT WILL BE IN LITTLE-ENDING FORMAT
	ch[0] = InGifFileBuf[InFileBufCount++];
	ch[1] = InGifFileBuf[InFileBufCount++];
	GIFLTDescriptor.canvas_width = (((ch[1]) << 8) + (ch[0]));
	printf("\nCANVAS WIDTH = %d pixels\n", GIFLTDescriptor.canvas_width);


	//NEXT GET THE CANVAS CANVAS HEIGHT
	//COPY THE 2ND BYTES FIRST AS IT WILL BE IN LITTLE-ENDING FORMAT
	ch[0] = InGifFileBuf[InFileBufCount++];
	ch[1] = InGifFileBuf[InFileBufCount++];
	GIFLTDescriptor.canvas_Height = (ch[1] << 8) + (ch[0]);
	printf("CANVAS HEIGHT = %d pixels\n", GIFLTDescriptor.canvas_Height);

	//NEXT BYTE INDICATES THE EXISTANCE OF GLOBAL COLOUR TABLE(GCT) AND ITS SIZE, IF EXISTS
	ch[0] = InGifFileBuf[InFileBufCount++];
	printf("NEXT BYTE GCTChar = %x \n", ch[0]);

	GCTChar = ch[0];

	if ((ch[0] & 0x0080) == 0x80)
	{
		printf("GLOBAL COLOUR TABLE EXISTS\n");

		GCTFlag = 1;

		N = 0;
		ch1 = ch[0] & 0x00000070;  //bit b6,b5,b4
		N = (ch1 >> 4);
		printf("THE COLOUR RESOLUTION OF THE IMAGE = N = %d\n", N);
		printf("THE IMAGE CONTAINS (N+1) BITS/PIXEL = %d BITS/PIXEL\n", N + 1);
		m = (long)pow(2, (N + 1));
		printf("NO OF ENTRIES IN THE GLOBAL COLOUR TABLE = %ld\n", m);

		ClrResolutionBits = N + 1;

		ch[0] = ch[0] >> 3; //sort flag -indicating the images are arranged in decreasing frequency/importance

							//NEXT BYTE INDIATES THE BACKGROUND COLOUR INDEX
		ch[0] = InGifFileBuf[InFileBufCount++];
		printf("BACKGROUND COLOUR INDEX = %d\n", ch[0]);


		//NEXT BYTE INDIATES THE PIXEL ASPECT RATIO
		ch[0] = InGifFileBuf[InFileBufCount++];
		printf("THE PIXEL ASPECT RATIO = N = %d ", ch[0]);

		if (ch[0]>0)
		{
			printf(" NOW THE PIXEL ASPECT RATIO = (N+15)/64 = %d \n", (ch[0] + 15) / 64);
		}


		printf("\n\n");
		printf("GLOBAL COLOUR TABLE STARTS\n");

		printf("FIRST, CALCULATE THE NO. OF COLOURS AND GCT SIZE\n");

		//NEXT BYTES GIVE THE GCT COLOUR VALUES
		//FIRST CALCULATE THE NUMBER OF COLOURS (2 * (N+1)) IN THE FILE
		NumOfColours = (int)pow(2, (N + 1));
		GCTSize = NumOfColours * 3; //EACH COLOUR TAKES 3 BYTES

		printf("NUMBER OF COLOURS IN THE FILE = 2^(N+1) = %d \n", NumOfColours);
		printf("NUMBER OF BYTES IN THE GLOBAL COLOUR TABLE = (NO.OF COLOURS) *3) = %d \n", GCTSize);

		//READ THE NUMBER OF BYTES, 3 AT A TIME REQUIRED FOR EACH OF THE COLOUR

		printf("THE COLOUR BYTES ARE AS FOLLOWS\n");


		fseek(fp, 13, SEEK_SET);
		InFileBufCount = 0;


		InFileBufCount = 0;
		k = (int)(3 * pow(2, (N + 1)));
		for (i = 0; i<k; i++)
		{

			ch[0] = fgetc(fp);
			if (ferror(fp) || feof(fp)) {
				printf("Error in reading the file. Quit!!\n");
				return 0;
			}
			InGifFileBuf[InFileBufCount] = ch[0];
			ch[1] = InGifFileBuf[InFileBufCount];
			InFileBufCount++;
		}

		printf("Index\tR\tG\tB\n===========================\n");

		for (i = 0; i<InFileBufCount; i += 3)
		{
			printf("%d\t%x\t%x\t%x\n", i / 3, InGifFileBuf[i], InGifFileBuf[i + 1], InGifFileBuf[i + 2]);
		}
		printf("\nEND OF GLOBAL COLOUR TABLE (GCT) BYTES\n");

	}
	else
	{
		printf("GLOBAL COLOUR TABLE DOES NOT EXIST\n");
	}

	printf("NEXT BYTES ARE\n");
	InFileBufCount = 0;

	ch[0] = fgetc(fp);
	if (ferror(fp) || feof(fp)) {
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	InGifFileBuf[InFileBufCount] = ch[0];
	ch[1] = InGifFileBuf[InFileBufCount];
	InFileBufCount++;
	
	printf("%x ", InGifFileBuf[i]);


	printf("\nEND OF NEXT BYTE READING\n");

	BlockTypeByte = InGifFileBuf[0];

	unsigned int width = 0;
	unsigned int height = 0;

	//GET ALL THE CHARACTERS IN THE FILE UNTIL TRAILER (0X3B) IS ENCOUNTERED;
	while (BlockTypeByte != 0x3B)
	{
		printf("NEXT BLOCK IS... ");
		switch (BlockTypeByte)
		{
		case 0x2c:printf("IT IS AN IMAGE DESCRIPTION BLOCK\n");
			printf("FIRST 10 BYTES INDICATE IMAGE DESCRIPTION BLOCK\n");
			InFileBufCount = 1;

			for (i = 0; i<9; i++)
			{

				ch[0] = fgetc(fp);
				if (ferror(fp) || feof(fp))
				{
					printf("Error in reading the file. Quit!!\n");
					return 0;
				}
				InGifFileBuf[InFileBufCount] = ch[0];
				ch[1] = InGifFileBuf[InFileBufCount];
				InFileBufCount++;
			}
			for (i = 0; i<InFileBufCount; i++)
			{
				printf("%x ", InGifFileBuf[i]);
			}

			printf("\nEND OF FIRST 10 BYTES\n");

			// NEED DIMENSIONS OF THE IMAGE!!! (parse the 10 bytes)

			ch[0] = InGifFileBuf[1];
			ch[1] = InGifFileBuf[2];
			printf("\nIMAGE POSITION ON CANVAS (LEFT) = %d\n", ((ch[1]) << 8) + (ch[0]));


			ch[0] = InGifFileBuf[3];
			ch[1] = InGifFileBuf[4];
			printf("IMAGE POSITION ON CANVAS (TOP) = %d\n", ((ch[1]) << 8) + (ch[0]));


			ch[0] = InGifFileBuf[5];
			ch[1] = InGifFileBuf[6];
			width = ((ch[1]) << 8) + (ch[0]);
			printf("IMAGE WIDTH = %u pixels\n", width);

			ch[0] = InGifFileBuf[7];
			ch[1] = InGifFileBuf[8];
			height = ((ch[1]) << 8) + (ch[0]);
			printf("IMAGE HEIGHT = %u pixels\n", height);

			printf("\n\nCHECK THE LAST BYTE TO FIND, COULD BE LOCAL COLOUR TABLE OR DIRECT IMAGE DATA\n");
			ch[0] = InGifFileBuf[9];
			if ((ch[0] & 0x80) == 0x80)
			{

				ch1 = ch[0] & 0x00000070;  //bit b6,b5,b4
				N = (ch1 >> 4);

				printf("LOCAL COLOUR TABLE BLOCK FOLLOWS\n");
				if(LocalColourTable()==0) return 0;
			}
			else
			{
				printf("\n\nIMAGE DATA BLOCK FOLLOWS\n");
				if (ProcessImageData(height, width) == 0) return 0;

			}
			break;

		case 0x21:printf("IT IS AN EXTENSION BLOCK\n");

			printf("CALL THE FUNCTION TO IDENTIFY THE TYPE OF EXTENSION BLOCK\n");
			if (Extensions_Handler() == 0) return 0;
			break;


		case 0x3B:printf("THAT'S IT!! TRAILER BLOCK REACHED!!!");
			break;


		default: printf("Error: Unknown BlockTypeByte %x \n", BlockTypeByte);
			return 0;
			break;


		} //END OF SWITCH


		printf("\n\nCOPY THE NEXT BYTE FOR SWITCH FOR COMPARISON\n");
		BlockTypeByte = fgetc(fp);
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		printf("BlockTypeByte = %x\n", BlockTypeByte);

	}//END OF WHILE

	printf("***********************END PARSING ENTIRE FILE***********************\n");

	return(1);
}



//LocalColourTable() function - Reads LCT Bytes; Block structure is same as GCT
int LocalColourTable()
{

	int i = 0, j = 0;
	unsigned char ch[2];
	//SIZE OF LOCAL COLOUR TABLE ALREADY CALCULATED
	//SIZE= 3 * 2 ^(N+1)

	printf("THE COLOUR RESOLUTION OF THE IMAGE BLOCK = N = %d\n", N);
	printf("THE IMAGE BLOCK CONTAINS (N+1) BITS/PIXEL = %d BITS/PIXEL\n", N + 1);
	printf("NO OF ENTRIES IN THE LOCAL COLOUR TABLE = %ld\n", (long)pow(2, (N + 1)));



	printf("\n\nINSIDE LOCAL COLOUR TABLE BLOCK PROCESSING \n");
	printf("THE LOCAL TABLE COLOUR BYTES ARE \n");
	InFileBufCount = 0;
	j = (int)(3 * pow(2, (N + 1)));
	for (i = 0; i<j; i++)
	{

		ch[0] = fgetc(fp);
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		InGifFileBuf[InFileBufCount] = ch[0];
		ch[1] = InGifFileBuf[InFileBufCount];
		InFileBufCount++;
	}

	printf("Index\tR\tG\tB\n===========================\n");

	for (i = 0; i<InFileBufCount; i += 3)
	{
		printf("%d\t%x\t%x\t%x\n", i / 3, InGifFileBuf[i], InGifFileBuf[i + 1], InGifFileBuf[i + 2]);
	}

	printf("\nEND OF LOCAL COLOUR TABLE BYTES BYTES\n\n");

	return 1;
}


//Extensions_Handler() - This function is called when GIF Extension block is encountered
//This Function checks Extension block type and calls the corresponding Extension function
int Extensions_Handler()
{
	//Get the 2nd byte to identify what type of Extension block
	//0xF9-Graphics Control Extension
	//0x01-Plane Text Extension
	//0xFF Application Extension
	//0xFE Comment Extension

	unsigned char ch;

	ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("Type of an extension block = %x \n", ch);

	if (ch == 0XF9)
	{
		printf("It's a Graphic Control Extension Block\n");
		if (Graphic_Cntrl_Extn() == 0) return 0;
	}
	else if (ch == 0X01)
	{
		printf("It's a Plaintext Extension Block\n");
		if(Plaintext_Extn()==0) return 0;
	}
	else if (ch == 0XFF)
	{
		printf("It's an Application Extension Block\n");
		if (Application_Extn() == 0) return 0;
	}
	else if (ch == 0XFE)
	{
		printf("It's a Comment Extension Block\n");
		if(Comment_Extn()==0) return 0;
	}
	else
	{
		printf("Wrong Extension\n");
		return 0;

	}

	return 1;
}

//Graphic_Cntrl_Extn() - It parses the animation related bytes
int Graphic_Cntrl_Extn()
{
	unsigned char BlockSize = 0, tmpBuf[100], ch, ch1, delayTime[2], transparent_colour_index;
	int i = 0;
	GCE_struct GCE_Data;

	printf("Inside Graphic Control Block \n");
	//read the blocksize byte
	BlockSize = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The Blocksize is = %d \n", BlockSize);

	//read the packed filed
	//bit 1-8 format , 4-6 Displosal method,7=user input flag, 8-transparent colour flag
	ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The packed byte is =%x\n", ch);
	GCE_Data.GCE_packed_field = ch;

	tmpBuf[i] = ch;
	ch = ch & 0x1c;
	ch1 = ch >> 2;
	printf("The Disposal method = %x \n", ch1);

	ch = ch & 0x02;
	ch1 = ch >> 1;
	printf("The User input flag = %x \n", ch1);

	ch = ch & 0x01;
	printf("The transparet colour flag = %x \n", ch);

	//delay time - 2 bytes
	delayTime[0] = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	delayTime[1] = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The delay time flag value is %x %x\n", delayTime[0], delayTime[1]);

	for (int z = 0; z<2; z++)
		GCE_Data.GCE_delay_time[z] = delayTime[z];
	//delay time - 2 bytes
	transparent_colour_index = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The transparent_colour_index %d\n", transparent_colour_index);
	GCE_Data.GCE_transparent_color_ind = transparent_colour_index;
	//block terminator should be here
	ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}

	if (ch == 0)

	{
		GCE_Blocks_Count++;
		printf("!!!!GOOD! Block terminator of GCE reached\n");
	}

	else

	{
		printf("!!!!SOMETHING IS WRONG!! Block terminator of GCE block not found\n");
		return 0;
	}
	return 1;
}



//Application_Extn() - Reads application name, version as well as application sub-block data
int Application_Extn()
{
	unsigned char BlockSize = 0;
	unsigned char tmpBuf[100];
	int i = 0;
	APP_Ext_struct APP_Ext_Data;

	printf("Inside Application Block\n");

	//read the blocksize byte
	BlockSize = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The block size is = %d\n", BlockSize);

	if (BlockSize != 0x0b)
	{
		printf("Something is wrong in the application extension, size = %d\n", BlockSize);
		return 0;
	}

	//Next 8 bytes are the Application Identifier
	printf("The Application Identifer is ");
	for (i = 0; i<8; i++)
	{
		//tmpBuf[i]=fgetc(fp);
		APP_Ext_Data.APP_Ext_application_id[i] = fgetc(fp);
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		printf("%c", APP_Ext_Data.APP_Ext_application_id[i]);
	}
	printf("\n");
	APP_Ext_Data.APP_Ext_application_id[i] = 0;


	//Next 3 bytes are the Application Authentic Code = version
	printf("The Application Auth Code is ");
	for (i = 0; i<3; i++)
	{
		tmpBuf[i] = fgetc(fp);
		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		printf("%c", tmpBuf[i]);
		APP_Ext_Data.APP_Ext_auth_code[i] = tmpBuf[i];
	}
	printf("\n");


	do
	{

		//LENGTH OF DATA SUBBLOCK
		printf("\nThe length of data sub block is ");
		tmpBuf[0] = fgetc(fp);

		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		printf("%d \n", tmpBuf[0]);

		printf("Reading Data Sub-block of size %d ...\n", tmpBuf[0]);
		printf("The read characters are:\n");
		for (i = 0; i<tmpBuf[0]; i++)
		{
			tmpBuf[1] = fgetc(fp);

			if (ferror(fp) || feof(fp))
			{
				printf("Error in reading the file. Quit!!\n");
				return 0;
			}
			printf("%x ", tmpBuf[1]);
		}
	} while (tmpBuf[0] != 0);//read until length of subblock is 0


	printf(" term char = %x \n", tmpBuf[0]);

	if (tmpBuf[0] == 0)
	{
		Appli_Ext_Blocks_count++;
		printf("!!!!GOOD! Block terminator of Application Extension block reached\n");
	}

	return 1;
}



//Plaintext_Extn() - Reads the plain text contained in the block. Usage of this extension is Very rare
int Plaintext_Extn()
{

	unsigned char BlockSize = 0, ch;

	int i = 0;
	printf("Inside the Plaintext block\n");

	//read the blocksize byte
	BlockSize = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	printf("The number of bytes to skip is = %d\n ", BlockSize);

	//skip so many bytes
	for (i = 0; i<BlockSize; i++)
		ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}

	//copy the image data till 0x00 is encountered
	ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}
	i = 0;
	while ((ch != 0x00) & (i<0xffff))
		ch = fgetc(fp);
	if (ferror(fp) || feof(fp))
	{
		printf("Error in reading the file. Quit!!\n");
		return 0;
	}

	if (ch != 0)
	{
		printf("ERROR!! Block terminator is not found!!!\n");
		return 0;
	}

	else

	{

		Plaintext_Blocks_count++;
		printf("block terminator is found in plaintext extn block\n");

	}
	return 1;
}



//Comment_Extn()- Extracts comment text of GIF file. Usage of this extension is Very rare
int Comment_Extn()
{
	unsigned char BlockSize = 0;
	unsigned char tmpBuf[100];
	int i = 0;
	printf("Inside the Comment Extension\n");

	do
	{

		//LENGTH OF COMMENTS SUBBLOCK
		printf("\nThe length of Comment sub block is ");
		tmpBuf[0] = fgetc(fp);

		if (ferror(fp) || feof(fp))
		{
			printf("Error in reading the file. Quit!!\n");
			return 0;
		}
		printf("%x \n", tmpBuf[0]);


		printf("Reading comment subblock of %d size ...\n", tmpBuf[0]);

		printf("The sub-block characters are: \n");

		for (i = 0; i<tmpBuf[0]; i++)
		{
			tmpBuf[1] = fgetc(fp);

			if (ferror(fp) || feof(fp))
			{
				printf("Error in reading the file. Quit!!\n");
				return 0;
			}
			printf("%x ", tmpBuf[1]);
		}
	} while (tmpBuf[0] != 0);//read until length of subblock is 0


	printf(" term char = %x \n", tmpBuf[0]);

	if (tmpBuf[0] != 0)
	{
		printf("ERROR!! Block terminator not found in Comment Extn!!!\n");
		return 0;
	}

	else

	{
		Comment_Blocks_Count++;
		printf("Block terminator found in Comment Extn\n");
	}
	return 1;
}







