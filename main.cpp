/**
 * Written by Gregory Heskett (gheskett)
 *
 * This is a command line tool intended to convert audio into a lossless encoding of the Nintendo AST format found in games such as Super Mario Galaxy and Mario Kart: Double Dash.
 * The resulting audio file is also compatible with lossy interpretations of AST as seen in The Legend of Zelda: Twilight Princess.
 *
 * v1.2 Released 8/29/17
 *
 */


 /**
 * Usage: ASTCreate.exe <input file> [optional arguments]
 *
 * OPTIONAL ARGUMENTS
 *	-o [output file]                           (default: same as input minus extension)
 *	-s [loop start sample]                     (defaut: 0)
 *	-t [loop start in microseconds]            (ex: 30000000 would be the equivalent of 30 seconds, or 960000 samples with a sample rate of 32000 Hz)
 *	-n                                         (disables looping)
 *	-e [loop end sample / total samples]       (default: number of samples in source file)
 *	-f [loop end in microseconds / total time]
 *	-r [sample rate]                           (default: same as source file / argument intended to change speed of audio rather than size)
 *	-h                                         (shows help text)
 *
 * USAGE EXAMPLES
 *	ASTCreate.exe inputfile.wav -o outputfile.ast -s 158462 -e 7485124
 *	ASTCreate.exe "use quotations if filename contains spaces.wav" -n -f 95000000
 * 
 * Note: This program will only work with WAV files (.wav) encoded with 16-bit PCM.  If the source file is anything other than a WAV file, please make a separate conversion first.  Also please ensure the input/output filenames do not contain Unicode characters.
 *
 */


#include "stdafx.h"
#include <string>
#include <intrin.h>
#include <stdio.h>
#include <windows.h>

using namespace std;

// Stores help text
const char *help = "\nUsage: ASTCreate.exe <input file> [optional arguments]\n\nOPTIONAL ARGUMENTS\n"
	"	-o [output file]                           (default: same as input minus extension)\n"
	"	-s [loop start sample]                     (defaut: 0)\n"
	"	-t [loop start in microseconds]            (ex: 30000000 would be the equivalent of 30 seconds, or 960000 samples with a sample rate of 32000 Hz)\n"
	"	-n                                         (disables looping)\n"
	"	-e [loop end sample / total samples]       (default: number of samples in source file)\n"
	"	-f [loop end in microseconds / total time]\n"
	"	-r [sample rate]                           (default: same as source file / argument intended to change speed of audio rather than size)\n"
	"	-h                                         (shows help text)\n\n"
	"USAGE EXAMPLES\n"
	"	ASTCreate.exe inputfile.wav -o outputfile.ast -s 158462 -e 7485124\n"
	"	ASTCreate.exe \"use quotations if filename contains spaces.wav\" -n -f 95000000\n\n"
	"Note: This program will only work with WAV files (.wav) encoded with 16-bit PCM.  If the source file is anything other than a WAV file, please make a separate conversion first.  Also please ensure the input/output filenames do not contain Unicode characters.\n\n";

// Used to store essential AST and WAV data
class ASTInfo {
	string filename; // Stores filename being used for AST
	unsigned int customSampleRate; // Stores sample rate used for AST
	unsigned int sampleRate; // Stores sample rate of original WAV file

	unsigned short numChannels; // Stores number of channels found in original WAV file
	unsigned int numSamples; // Stores the number of samples being used for the AST
	unsigned short isLooped = 65535; // Stores value determining whether or not the AST is looped (65535 = true, 0 = false)
	unsigned int loopStart = 0; // Stores starting loop point
	unsigned int astSize; // Stores total file size of AST (minus 64)
	unsigned int wavSize; // Stores size of audio found in source WAV

	unsigned int blockSize = 10080; // Stores block size used (default for AST is 10080 bytes per channel)
	unsigned int excBlkSz; // Stores the size of the last block being written to the AST file
	unsigned int numBlocks; // Stores the number of blocks being used in the AST file
	unsigned int padding; // Stores a value between 0 and 32 to compensate with the final block to round it to a multiple of 32 bytes

public:
	int grabInfo(int, char**); // Retrieves header info from input WAV file, then later writes new AST file if no errors occur 
	int getWAVData(FILE*); // Grabs and stores important WAV header info
	int assignValue(char*, char*); // Parses through user arguments and overrides default settings
	int writeAST(FILE*); // Entry point for writing the AST file
	void printHeader(FILE*); // Writes AST header to output file (and swaps endianness)
	void printAudio(FILE*, FILE*); // Writes all audio data to AST file (Big Endian)
};

// Main method
int main(int argc, char **argv)
{
	// Displays an error if there are not at least two arguments provided
	if (argc < 2) {
		printf(help);
		return 1;
	}

	ASTInfo createFile; // Creates a class used for storing essential AST and WAV data

	// Returns 1 if the program runs into an error
	int failure = createFile.grabInfo(argc, argv);
	if (failure == 1)
		return 1;

	return 0;
}

// Retrieves header info from input WAV file, then later writes new AST file if no errors occur 
int ASTInfo::grabInfo (int argc, char **argv) {
	this->filename = argv[1];
	
	// Checks for input of more than one input file (via *)
	if (this->filename.find("*") != -1) {
		printf("ERROR: Program is only capable of opening a single input file at a time.  Please enter an exact file name (avoid using '*').\n\n%s", help);
		return 1;
	}

	// Opens input file
	FILE *sourceWAV = fopen(this->filename.c_str(), "rb");
	if (!sourceWAV) {
		if (this->filename.compare("-h") == 0 && argc == 2)
			printf(help);
		else
			printf("ERROR: Cannot find/open input file!\n\n%s", help);
		return 1;
	};

	// Checks for file (extention) validity
	string tmp = "";
	int wavE = 4; // Allows extention .wave to slide by
	if (strlen(this->filename.c_str()) >= 4)
		tmp = this->filename.substr(this->filename.length() - 4, 4);
	if (tmp.compare(".wav") != 0) {
		if (strlen(this->filename.c_str()) >= 5)
			tmp = this->filename.substr(this->filename.length() - 5, 5);
		if (tmp.compare(".wave") != 0) { 
			if (this->filename.find(".") != string::npos)
				printf("ERROR: Source file must be a WAV file!\n\n%s", help);
			else
				printf("ERROR: Source file contains no extension!  The filename should be followed with \".wav\", assuming the source is indeed a WAV file.\n%s", help);
			fclose(sourceWAV);
			return 1;
		}
		wavE = 5;
	}

	// Changes .wav extension to .ast
	this->filename = this->filename.substr(0, this->filename.length() - wavE);
	this->filename += ".ast";

	int exit = this->getWAVData(sourceWAV); // Grabs WAV header info
	if (exit == 1)
		return 1;

	// Parses through user arguments
	bool helpState = false; // Stores boolean value determining whether or not to print out help text
	for (int count = 2; count < argc; count++) {
		if (argv[count][0] == '-') {
			if (strlen(argv[count]) != 2) { // Ensures arguments are two characters
				printf(help);
				return 1;
			}
			if (argc - 1 == count) {
				if (argv[count][1] != 'n' && argv[count][1] != 'h')
					exit = 1;
				else
					exit = assignValue(argv[count], NULL);
			}
			else {
				exit = assignValue(argv[count], argv[count + 1]);
			}
			if (argv[count][1] != 'n' && argv[count][1] != 'h') {
				count++;
			}
			if (argv[count][1] == 'h')
				helpState = true;
		}
		else {
			exit = 1;
		}
		if (exit == 1) { // Exits the program if user arguments are invalid
			printf(help);
			return 1;
		}
	}
	if (helpState == true) // Prints help text if prompted
		printf(help);

	exit = this->writeAST(sourceWAV);
	fclose(sourceWAV);
	return exit;

}

// Parses through user arguments and overrides default settings
int ASTInfo::assignValue(char *c1, char *c2) {

	int slash, colon;
	uint64_t time;
	long double rounded;
	uint64_t samples;
	string c2str;

	char value = c1[1];
	switch (value) {
	case 'h': // Breaks out if asking for help text since it is printed somewhere else in the program
		break;
	case 'n': // Disables looping
		this->isLooped = 0;
		break;
	case 'o': // Changes name of output file (if given valid filename)
		c2str = c2;
		slash = c2str.find_last_of("/\\");
		if (c2str.find_last_of("/\\") == string::npos)
			slash = -1;
		colon = c2str.find_last_of(":");
		if (c2str.find_last_of(":") == string::npos)
			colon = -1;

		if (c2str.find("*") != string::npos || c2str.find("?") != string::npos || c2str.find("\"") != string::npos || colon > slash
		  || c2str.find("<") != string::npos || c2str.find(">") != string::npos || c2str.find("|") != string::npos) {
			printf("WARNING: Output filename \"%s\" contains illegal characters.  Output argument will be ignored.\n", c2);
		}
		else {
			this->filename = c2;
		}
		break;
	case 's': // Sets starting loop point
		this->loopStart = atoi(c2);
		break;
	case 't': // Sets starting loop point (in microseconds)
		time = atol(c2);
		rounded = ((long double) time / 1000000.0);
		rounded = rounded * (long double) this->sampleRate + 0.5;
		time = (uint64_t) rounded;
		this->loopStart = (int) rounded;
		break;
	case 'e': // Sets end point of AST file
		samples = atoi(c2);
		if (samples == 0) {
			printf("ERROR: Total number of samples cannot be zero!\n");
			return 1;
		}
		if (this->numSamples < (unsigned int) samples)
			samples = (uint64_t) this->numSamples;
		else
			this->numSamples = (unsigned int) samples;
		this->wavSize = this->numSamples * 2 * this->numChannels;
		break;
	case 'f': // Sets end point of AST file (in microseconds)
		time = atol(c2);
		if (time == 0) {
			printf("ERROR: Ending point of AST cannot be set to zero microseconds!\n");
			return 1;
		}
		rounded = ((long double) time / 1000000.0);
		rounded = rounded * (long double) this->sampleRate + 0.5;
		samples = (uint64_t) rounded;
		if (samples == 0) {
			printf("ERROR: End point of AST is effectively zero!  Please enter a larger value of microseconds (not milliseconds).\n");
			return 1;
		}
		if (this->numSamples < (unsigned int) samples)
			samples = (uint64_t) this->numSamples;
		else
			this->numSamples = (unsigned int) samples;
		this->wavSize = this->numSamples * 2 * this->numChannels;
		break;
	case 'r': // Sets custom sample rate (does not affect loop times entered, but does intentionally affect playback speed)
		this->customSampleRate = atoi(c2);
		if (this->customSampleRate == 0)
			this->customSampleRate = this->sampleRate;
		break;
	default: // Returns error if argument is invalid
		return 1;
	}
	return 0;
}

// Grabs and stores important WAV header info
int ASTInfo::getWAVData(FILE *sourceWAV) {
	const char _riff[] = "RIFF";
	const char _wavefmt[] = "WAVE";
	const char _data[] = "data";
	const char _fmt[] = "fmt ";

	// Checks for use of RIFF WAV file
	char riff[5];
	char wavefmt[5];
	fseek(sourceWAV, 0, SEEK_SET);
	fread(&riff, 4, 1, sourceWAV);
	fseek(sourceWAV, 8, SEEK_SET);
	fread(&wavefmt, 4, 1, sourceWAV);
	riff[4] = '\0';
	wavefmt[4] = '\0';
	if (strcmp(_riff, riff) != 0 || strcmp(_wavefmt, wavefmt) != 0) {
		printf("ERROR: Header contents of WAV are invalid or corrupted.  Please be sure your input file is a RIFF WAV audio file.\n");
		return 1;
	}

	// Stores size of chunks
	unsigned int chunkSZ;

	// Searches for fmt chunk
	char fmt[5];
	bool isFmt = false;
	while (fread(&fmt, 4, 1, sourceWAV) == 1) {
		fmt[4] = '\0';
		if (strcmp(_fmt, fmt) == 0) {
			isFmt = true;
			break;
		}
		if (fread(&chunkSZ, 4, 1, sourceWAV) != 1)
			break;
		fseek(sourceWAV, chunkSZ, SEEK_CUR);
	}
	if (!isFmt) {
		printf("ERROR: No 'fmt' chunk could be found in WAV file.  The source file is likely corrupted.\n");
		return 1;
	}

	// Checks for use of PCM
	unsigned short PCM;
	fseek(sourceWAV, 4, SEEK_CUR);
	fread(&PCM, 2, 1, sourceWAV);
	if (PCM != 1 && PCM != 65534)
		printf("CRITICAL WARNING: Source WAV file may not use PCM!\n");

	// Ensures source file uses anywhere between 1 and 16 channels total
	fread(&this->numChannels, 2, 1, sourceWAV);
	if (this->numChannels > 16 || this->numChannels < 1) {
		printf("ERROR: Invalid number of channels!  Please stick with a file containing 1-16 channels.\n");
	}

	// Sets sample rate
	fread(&this->sampleRate, 4, 1, sourceWAV);
	this->customSampleRate = this->sampleRate;
	
	// Checks to see if bit rate is 16 bits per sample
	short bitrate;
	fseek(sourceWAV, 6, SEEK_CUR);
	fread(&bitrate, 2, 1, sourceWAV);
	if (bitrate != 16) {
		printf("ERROR: Invalid bit rate!  Please make sure you are using 16-bit PCM.\n");
		return 1;
	}

	// Searches for data chunk
	char data[5];
	bool isData = false;
	fseek(sourceWAV, 12, SEEK_SET);
	while (fread(&data, 4, 1, sourceWAV) == 1) {
		data[4] = '\0';
		if (strcmp(_data, data) == 0) {
			isData = true;
			break;
		}
		if (fread(&chunkSZ, 4, 1, sourceWAV) != 1)
			break;
		fseek(sourceWAV, chunkSZ, SEEK_CUR);
	}
	if (!isData) {
		printf("ERROR: No 'data' chunk could be found in WAV file.  Either the source contains no audio or is corrupted.\n");
		return 1;
	}

	fread(&this->wavSize, 4, 1, sourceWAV); // Sets total size of audio

	this->numSamples = (unsigned int) (this->wavSize) / ((unsigned int) this->numChannels * 2); // Sets total number of audio samples

	return 0;
}

// Entry point for writing the AST file
int ASTInfo::writeAST(FILE *sourceWAV)
{
	// Calculates number of blocks and size of last block
	this->excBlkSz = (this->numSamples * 2) % this->blockSize;
	this->numBlocks = (this->numSamples * 2) / this->blockSize;
	if (this->excBlkSz != 0)
		this->numBlocks++;
	this->padding = 32 - (this->excBlkSz % 32);
	if (this->padding == 32)
		this->padding = 0;

	this->astSize = wavSize + (this->numBlocks * 32) + (this->padding * this->numChannels); // Stores size of AST

	// Ensures output file extension is .ast
	string tmp = "";
	if (this->filename.length() >= 4)
		tmp = this->filename.substr(this->filename.length() - 4, this->filename.length());
	if (_strcmpi(tmp.c_str(), ".ast") != 0)
		this->filename += ".ast";
	if (_strcmpi(this->filename.c_str(), ".ast") == 0) {
		printf("ERROR: Output filename can not be restricted exclusively to .ast extension!\n\n%s", help);
		return 1;
	}

	// Ensures WAV file has audio
	if (this->numBlocks == 0) {
		printf("ERROR: Source WAV contains no audio data!\n");
		return 1;
	}

	// Compensates by setting extra block size to the general block size if it's set to zero
	if (this->excBlkSz == 0)
		this->excBlkSz = this->blockSize;

	// Prevents starting loop point from being as large or larger than the end point
	if (this->loopStart >= this->numSamples)
		this->loopStart = 0;

	// Checks to make sure sample rate is not zero
	if (this->customSampleRate == 0) {
		printf("ERROR: Source file has a sample rate of 0 Hz!\n");
		return 1;
	}

	if (this->filename.find("\\") != string::npos || this->filename.find("/") != string::npos) {
		int size = this->filename.find_last_of("/\\");
		CreateDirectory(this->filename.substr(0, size+1).c_str(), NULL);
	}

	// Creates AST file
	FILE *outputAST = fopen(this->filename.c_str(), "wb");
	if (!outputAST) {
		printf("ERROR: Couldn't create file.\n");
		return 1;
	}

	// Prints AST information to user
	string loopStatus = "true";
	if (this->isLooped == 0) {
		loopStatus = "false";
		this->loopStart = 0;
	}
	printf("File opened successfully!\n\n	AST file size: %d bytes\n	Sample rate: %d Hz\n	Is looped: %s\n", this->astSize + 64, this->customSampleRate, loopStatus.c_str());
	if (this->isLooped == 65535)
		printf("	Starting loop point: %d samples\n", this->loopStart);
	printf("	End of stream: %d samples\n	Number of channels: %d", this->numSamples, this->numChannels);
	if (this->numChannels == 1)
		printf(" (mono)");
	else if (this->numChannels == 2)
		printf(" (stereo)");

	printf("\n\nWriting %s...", this->filename.c_str());

	printHeader(outputAST); // Writes header info to output

	printAudio(sourceWAV, outputAST); // Writes audio to AST file

	printf("...DONE!\n");
	fclose(outputAST);
	return 0;
}

// Writes AST header to output file (and swaps endianness)
void ASTInfo::printHeader(FILE *outputAST) {
	fprintf(outputAST, "STRM"); // Prints "STRM" at 0x0000

	uint32_t fourByteInt = _byteswap_ulong(this->astSize); // Prints total size of all future AST block chunks (file size - 64) at 0x0004
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	fourByteInt = 268435712; // Prints a hex of 0x00010010 at 0x0008 (contains PCM16 encoding information)
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	uint16_t twoByteShort = _byteswap_ushort(this->numChannels); // Prints number of channels at 0x000C
	fwrite(&twoByteShort, sizeof(twoByteShort), 1, outputAST);

	fwrite(&this->isLooped, sizeof(this->isLooped), 1, outputAST); // Prints 0xFFFF if looped and 0x0000 if not looped at 0x000E

	fourByteInt = _byteswap_ulong(this->customSampleRate); // Prints sample rate at 0x0010
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	fourByteInt = _byteswap_ulong(this->numSamples); // Prints total number of samples at 0x0014
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	fourByteInt = _byteswap_ulong(this->loopStart); // Prints starting loop point (in samples) at 0x0018
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	fourByteInt = _byteswap_ulong(this->numSamples); // Prints end loop point (in samples) at 0x001C (same as 0x0014)
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	// Prints size of first block at 0x0020
	if (this->numBlocks == 1) {
		fourByteInt = _byteswap_ulong(this->excBlkSz + padding);
	}
	else {
		fourByteInt = _byteswap_ulong(this->blockSize);
	}
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	// Fills in last 28 bytes with all 0s (except for 0x0028, which has a hex of 0x7F)
	fourByteInt = 0;
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);
	fourByteInt = 127; // Likely denotes playback volume of AST (always set to 127, or 0x7F)
	fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);
	fourByteInt = 0;
	for (int x = 0; x < 5; ++x)
		fwrite(&fourByteInt, sizeof(fourByteInt), 1, outputAST);

	return;
}

// Writes all audio data to AST file (Big Endian)
void ASTInfo::printAudio(FILE *sourceWAV, FILE *outputAST) {
	uint32_t length = this->blockSize; // Stores size of audio chunk in block
	uint32_t paddedLength = _byteswap_ulong(length); // Stores current block size along with padding (Big Endian)
	const uint16_t zeroPad = 0; // Contains the hex of 0x0000 used for padding
	unsigned short offset = this->numChannels; // Stores an offset used in for loops to compensate with variable channels

	uint16_t *block = (uint16_t*) malloc(this->blockSize * this->numChannels);
	const unsigned int headerPad = 0; // Contains the hex of 0x00000000 used for padding in header

	length *= this->numChannels; // Changes length from block size to audio size

	for (unsigned int x = 0; x < numBlocks; ++x) {

		// Writes block header
		fprintf(outputAST, "BLCK"); // Writes "BLCK" at 0x0000 index of block

		// Adds padding to paddedLength during the last block
		if (x == this->numBlocks - 1) {
			memset(block, 0, this->blockSize * this->numChannels); // Clears old audio data stored in block array (possibly unnecessary)
			paddedLength = (this->excBlkSz + this->padding);
			length = (this->excBlkSz) * this->numChannels;
			paddedLength = _byteswap_ulong(paddedLength);
		}

		fwrite(&paddedLength, sizeof(paddedLength), 1, outputAST); // writes block size at 0x0004 index of block
		for (unsigned int y = 0; y < 6; ++y)
			fwrite(&headerPad, sizeof(headerPad), 1, outputAST); // writes 24 bytes worth of 0s at 0x0008 index of block

		fread(&block[0], length, 1, sourceWAV); // reads one block worth of data from source WAV file

		for (unsigned int y = 0; y < length / 2; ++y) // converts endianness to Big Endian
			block[y] = _byteswap_ushort(block[y]);

		for (unsigned int y = 0; y < this->numChannels; ++y) {
			unsigned int z = y;
			for (z; z < length / 2; z += offset) // Prints out audio data in channel order
				fwrite(&block[z], sizeof(uint16_t), 1, outputAST);

			if (x == this->numBlocks - 1) { // Prints 32-byte padding at the end of the stream
				for (z = 0; z < padding; z += 2)
					fwrite(&zeroPad, sizeof(uint16_t), 1, outputAST);
			}
		}
	}
	free(block);
}