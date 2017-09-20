/*  ***********    Includes    *********     */
    #include<stdio.h>
    #include<stdlib.h>
    #include<string.h>
    #include<unistd.h>
    #include<errno.h>
    #include <sys/ioctl.h>
    #include<termios.h>
    #include<ctype.h>

/*  ***********    Defines    *********     */
    #define CTRL_KEY(k) (k&0x1f)

    struct buff {
        char *b;
        int len;
    };
    #define BUFF_INIT {NULL, 0}
    #define MICRO_VERSION "0.0.1"

    enum editor_keys  {
        ARROW_LEFT = 1000,
        ARROW_UP,
        ARROW_RIGHT,
        ARROW_DOWN
    };

/*  ***********    Declarations    *********     */
    struct config{
        int x;
        int y;
        int rows;
        int cols;
        struct termios init_termios;
    };
    struct config _micro;

    void enableRawMode();  
    void disableRawMode(); 
    void die(char*);
    void _microRefreshScreen();
    void _microKeyProcessor();
    int ReadKeyPress();
    int getWindowSize(int*,int*);
    void initMicro();
    void drawBoundary(struct buff*);
    void bAppend(struct buff*, const char*, int);
    void bFree(struct buff*);
    void _microCursor(int);


/*  ***********    Main    *********     */
    int main(){
        enableRawMode();     //enter raw mode
        initMicro();
        while(1){
            _microRefreshScreen();
            _microKeyProcessor();
        }

        return 0;
    }


/*  ***********    Terminal Functions    *********     */
    void initMicro(){
        _micro.x = 10;
        _micro.y = 10;
        if(getWindowSize(&_micro.rows,&_micro.cols)==-1)
            die("getWindowSize, initMicro");
    }

    void enableRawMode(){
        if(tcgetattr(STDIN_FILENO,&_micro.init_termios) == -1) 
            die("tcgetattr, enableRawMode");

        struct termios raw = _micro.init_termios;
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
        if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&_micro.init_termios) == -1)                //save initial settings of terminal
            die("tcsetattr, disableRawMode");
    }

    int ReadKeyPress(){
        int cread;
        char c;
        while((cread = read(STDIN_FILENO,&c,1)) != 1){              
            if(cread == -1 && errno != EAGAIN) 
            die("read, main");
        }
        if (c == '\x1b') {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
            if (seq[0] == '[') {
              switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
              }
            }
            return '\x1b';
          } else {
            return c;
          }
    }

    int getCursorPosition(int *rows, int *cols) {
        char buf[32];
        unsigned int i = 0;
        if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
        while (i < sizeof(buf) - 1) {
            if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
            if (buf[i] == 'R') break;
            i++;
        }
        buf[i] = '\0';
        if (buf[0] != '\x1b' || buf[1] != '[') return -1;
        if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
        return 0;
      }      

    int getWindowSize(int*rows,int *cols){
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
            if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) 
                return -1;
            return getCursorPosition(rows, cols);
          return -1;
        } 
        else {
          *cols = ws.ws_col;
          *rows = ws.ws_row;
          return 0;
        }
    }

    void die(char *e){                      //kill program with error message
        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        perror(e);
        exit(EXIT_FAILURE);
    }

/*  ***********    Input Functions    *********     */

    void _microCursor(int key){
        switch(key){
            case ARROW_UP : 
            if(_micro.y !=0){
            _micro.y--;
            }
                        break;
            case ARROW_DOWN : 
            if(_micro.y != _micro.rows -1){
            _micro.y++;
            }
                        break;
            case ARROW_LEFT :
            if(_micro.x!=0){
                _micro.x--;
            } 
                        break;
            case ARROW_RIGHT : 
            if(_micro.x != _micro.cols -1){
                _micro.x++;
            }
                        break;
        }
    }

    void _microKeyProcessor(){
        int c = ReadKeyPress();
        switch (c) {
          case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
          case ARROW_UP:
          case ARROW_DOWN:
          case ARROW_LEFT:
          case ARROW_RIGHT:
          _microCursor(c);
            break;
        }
    }


/*  ***********    Output Functions    *********     */
    void bAppend(struct buff *a, const char *s, int len) {
        char *new = realloc(a->b, a->len + len);
        if (new == NULL) return;
        memcpy(&new[a->len], s, len);
        a->b = new;
        a->len += len;
    }

    void bFree(struct buff *a) {
        free(a->b);
    }

    void _microRefreshScreen(){
        struct buff a = BUFF_INIT;
        bAppend(&a, "\x1b[?25l", 6);
        bAppend(&a, "\x1b[H", 3); 

        drawBoundary(&a);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", _micro.y + 1, _micro.x + 1);
        bAppend(&a, buf, strlen(buf));
        bAppend(&a, "\x1b[?25h", 6);
        write(STDOUT_FILENO, a.b, a.len);
        bFree(&a);
    }
    
    void drawBoundary(struct buff *a){
        for(int y=0;y<_micro.rows;y++){
            if (y == _micro.rows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                  "Micro editor -- version %s", MICRO_VERSION);
                if (welcomelen > _micro.cols) welcomelen = _micro.cols;
                int padding = (_micro.cols - welcomelen) / 2;
                if (padding) {
                  bAppend(a, "#", 1);
                  padding--;
                }
                while (padding--) bAppend(a, " ", 1);
                bAppend(a, welcome, welcomelen);
              } else {
                bAppend(a, "#", 1);
              }
            bAppend(a, "\x1b[K", 3);
            if (y < _micro.rows - 1) {
              bAppend(a, "\r\n", 2);
            }
        }
    }