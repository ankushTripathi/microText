/*  ***********    Includes    *********     */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<termios.h>
#include<ctype.h>


/*  ***********    Declarations    *********     */
struct termios init_termios;

void enableRawMode();  
void disableRawMode(); 
void die(char*);


/*  ***********    Main    *********     */
int main(){
    enableRawMode();     //enter raw mode
    while(1){
        char c = '\0';   
        if(read(STDIN_FILENO,&c,1) == -1 && errno != EAGAIN)   //read charecters byte-by-byte
            die("read, main");
        if(c == 'q') break;                 //exit with 'q'  (needs to be changed)
        if(iscntrl(c))                 //check if charecter is control byte
            printf("%d\r\n",c);
        else   
            printf("%d ('%c')\r\n", c, c);
    }

    return 0;
}


/*  ***********    Functions    *********     */
void enableRawMode(){
    if(tcgetattr(STDIN_FILENO,&init_termios) == -1) 
        die("tcgetattr, enableRawMode");

    struct termios raw = init_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);  //disable special inputs like ctrl+q , break conditions
    raw.c_oflag &= ~(OPOST);                                    //disable default carraige return output
    raw.c_cflag |= (CS8);                                       //disable some other flags
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);            //turn off auto-echo, disable canon mode , ctr+c etx
    raw.c_cc[VMIN] = 0;                                         //set min required input bytes
    raw.c_cc[VTIME] = 1;                                        //set max timeout (1 => 1/10th of sec)

    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) == -1)            //save terminal settings
        die("tcsetattr, enableRawMode");                 
    atexit(disableRawMode);                         //return to canon mode at exit
}

void disableRawMode(){
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&init_termios) == -1)                //save initial settings of terminal
        die("tcsetattr, disableRawMode");
}

void die(char *e){                      //kill program with error message
    perror(e);
    exit(EXIT_FAILURE);
}