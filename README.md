# Aligner
Tired of messy code? Try Aligner™©® and improve your coding style!  

## Compilation
```
gcc -o aligner aligner.c
```
## Usage
```
$ ./aligner fizzbuzz.c
#include                                                               <stdio.h>
#include                                                              <string.h>
#include                                                              <stdlib.h>
char                                                                           *
itoa                                                                           (
int value                                                                      ,
char                                                                           *
result                                                                        ),
charlist                                                                       [
10                                                                            ];
int main                                                                       (
void                                                                          ){
unsigned int i                                                                 ,
max                                                                            ;
char                                                                           *
c                                                                              ;
for                                                                            (
i                                                                              =
0                                                                              ;
i                                                                              <
10                                                                             ;
i                                                                           ++){
charlist                                                                       [
i                                                                             ]=
48                                                                             +
i                                                                             ;}
max                                                                            =
100                                                                            ;
c                                                                              =
malloc                                                                         (
sizeof                                                                         (
char                                                                         )*(
max                                                                            %
10                                                                           ));
for                                                                            (
i                                                                              =
1                                                                              ;
i                                                                             <=
max                                                                            ;
i                                                                           ++){
if                                                                             (
i                                                                              %
15                                                                            ==
0                                                                             ){
fputc                                                                          (
70                                                                             ,
stdout                                                                        );
fputc                                                                          (
105                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                        );
fputc                                                                          (
66                                                                             ,
stdout                                                                        );
fputc                                                                          (
117                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                       );}
else if                                                                        (
i                                                                              %
3                                                                             ==
0                                                                             ){
fputc                                                                          (
70                                                                             ,
stdout                                                                        );
fputc                                                                          (
105                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                       );}
else if                                                                        (
i                                                                              %
5                                                                             ==
0                                                                             ){
fputc                                                                          (
66                                                                             ,
stdout                                                                        );
fputc                                                                          (
117                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                        );
fputc                                                                          (
122                                                                            ,
stdout                                                                       );}
else                                                                           {
itoa                                                                           (
i                                                                              ,
c                                                                             );
fwrite                                                                         (
c                                                                              ,
sizeof                                                                         (
char                                                                          ),
strlen                                                                         (
c                                                                             ),
stdout                                                                       );}
fputc                                                                          (
10                                                                             ,
stdout                                                                       );}
return 0                                                                      ;}
char                                                                           *
itoa                                                                           (
int value                                                                      ,
char                                                                           *
result                                                                        ){
char                                                                           *
ptr                                                                           ,*
ptr1                                                                           ,
tempchar                                                                       ;
ptr                                                                            =
result                                                                         ;
ptr1                                                                           =
result                                                                         ;
int tempvalue                                                                  ;
do                                                                             {
tempvalue                                                                      =
value                                                                          ;
value                                                                         /=
10                                                                            ;*
ptr                                                                          ++=
charlist                                                                      [(
tempvalue                                                                      -
value                                                                          *
10                                                                          )];}
while                                                                          (
value                                                                        );*
ptr                                                                          --=
0                                                                              ;
while                                                                          (
ptr1                                                                           <
ptr                                                                           ){
tempchar                                                                      =*
ptr                                                                           ;*
ptr                                                                         --=*
ptr1                                                                          ;*
ptr1                                                                         ++=
tempchar                                                                      ;}
return result                                                                 ;}
```
