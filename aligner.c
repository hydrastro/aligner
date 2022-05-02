#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

int main (int argc, const char *argv[]) {
    setbuf(stdout, NULL);
    FILE *stream;
    char outputBuffer[80], readChar[1], rb[2];
    int writtenChars, readSymbols, i;
    bool readingSymbols;
    if(argc < 2){
        fputc(101, stdout);
        fputc(114, stdout);
        fputc(114, stdout);
        fputc(111, stdout);
        fputc(114, stdout);
        fputc(58, stdout);
        fputc(32, stdout);
        fputc(110, stdout);
        fputc(111, stdout);
        fputc(32, stdout);
        fputc(102, stdout);
        fputc(105, stdout);
        fputc(108, stdout);
        fputc(101, stdout);
        fputc(32, stdout);
        fputc(112, stdout);
        fputc(114, stdout);
        fputc(111, stdout);
        fputc(118, stdout);
        fputc(105, stdout);
        fputc(100, stdout);
        fputc(101, stdout);
        fputc(100, stdout);
        fputc(10, stdout);

        return -1;
    }
    rb[0] = 114;
    rb[1] = 98;
    stream = fopen(argv[1], rb);
    if(stream == NULL) {
        fputc(97, stdout);
        fputc(110, stdout);
        fputc(32, stdout);
        fputc(101, stdout);
        fputc(114, stdout);
        fputc(114, stdout);
        fputc(111, stdout);
        fputc(114, stdout);
        fputc(32, stdout);
        fputc(111, stdout);
        fputc(99, stdout);
        fputc(99, stdout);
        fputc(117, stdout);
        fputc(114, stdout);
        fputc(101, stdout);
        fputc(100, stdout);
        fputc(32, stdout);
        fputc(119, stdout);
        fputc(104, stdout);
        fputc(105, stdout);
        fputc(108, stdout);
        fputc(101, stdout);
        fputc(32, stdout);
        fputc(111, stdout);
        fputc(112, stdout);
        fputc(101, stdout);
        fputc(110, stdout);
        fputc(105, stdout);
        fputc(110, stdout);
        fputc(103, stdout);
        fputc(32, stdout);
        fputc(102, stdout);
        fputc(105, stdout);
        fputc(108, stdout);
        fputc(101, stdout);
        fputc(10, stdout);

        return -1;
    }
    writtenChars = 0;
    readSymbols = 0;
    readingSymbols = false;
    while(fread(readChar, sizeof(char), 1, stream) == 1) {
        if(writtenChars == 0 && *readChar == 35) {
           fputc(35, stdout);
           for(i = 0; i < 7; i++) {
               fread(readChar, sizeof(char), 1, stream);
               fputc(*readChar, stdout);
           }
           while(fread(readChar, sizeof(char), 1, stream) == 1 && *readChar != 10) {
               if(!isspace(*readChar)) {
                   outputBuffer[readSymbols] = *readChar;
                   readSymbols++;
               }
           }
           for(i = 0; i < 72 - readSymbols; i++) {
               fputc(32, stdout);
           }
           for(i = 0; i < readSymbols; i++) {
               fputc(outputBuffer[i], stdout);
           }
           readSymbols = 0;
           fputc(10, stdout);
        }
        if(isalnum(*readChar)) {
            if(readSymbols != 0) {
                for(i = 0; i < 80 - writtenChars - readSymbols; i++) {
                    fputc(32, stdout);
                }
                for(i = 0; i < readSymbols; i++) {
                    fputc(outputBuffer[i], stdout);
                }
                fputc(10, stdout);
                writtenChars = 0;
                readingSymbols = false;
                readSymbols = 0;
            }
            if(!isspace(*readChar) || writtenChars > 0) {
                fputc(*readChar, stdout);
                writtenChars++;
            }
        } else if(isspace(*readChar)) {
            if(readingSymbols == false) {
                if(*readChar != 10){
                    fputc(*readChar, stdout);
                    writtenChars++;
                }
            }
        } else {
            outputBuffer[readSymbols] = *readChar;
            readSymbols++;
            if(readingSymbols == false) {
                readingSymbols = true;
            }
        }
    }
    if(readSymbols != 0) {
        for(i = 0; i < 80 - writtenChars - readSymbols; i++) {
            fputc(32, stdout);
        }
        for(i = 0; i < readSymbols; i++) {
            fputc(outputBuffer[i], stdout);
        }
    }
    fputc(10, stdout);
    fclose(stream);

    return 0;
}
