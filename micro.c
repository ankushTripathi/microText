/*  ***********    Includes    *********     */
    #include<stdio.h>
    #include<stdlib.h>
    #include<string.h>
    #include<unistd.h>
    #include<errno.h>
    #include <sys/ioctl.h>
    #include<sys/types.h>
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
    #define MICRO_TAB 8
    enum editor_keys  {
        ARROW_LEFT = 1000,
        ARROW_UP,
        ARROW_RIGHT,
        ARROW_DOWN,
        PAGE_UP,
        PAGE_DOWN,
        HOME_KEY,
        END_KEY,
        DEL
    };

    typedef struct erow {
        int size;
        int rsize;
        char *chars;
        char *render;
    } erow;

/*  ***********    Declarations    *********     */
    struct config{
        int x;
        int y;
        int r_x;
        int row_off;
        int col_off;
        int screenrows;
        int screencols;
        int num_rows;
        erow *row;
        char *filename;
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
    void drawRows(struct buff*);
    void bAppend(struct buff*, const char*, int);
    void bFree(struct buff*);
    void _microCursor(int);
    void microOpen(char *filename);
    void microAppendRow(char*,size_t);
    void microScroll();
    void microUpdateRow(erow*);
    int convertxToRx(erow*,int);
    void drawStatusBar(struct buff *);
    char *strdup (const char *) ;


/*  ***********    Main    *********     */
    int main(int argc,char *argv[]){
        enableRawMode();     //enter raw mode
        initMicro();
        if(argc >= 2){
            microOpen(argv[1]);
        }
        while(1){
            _microRefreshScreen();
            _microKeyProcessor();
        }

        return 0;
    }


/*  ***********    Terminal Functions    *********     */
    void initMicro(){
        _micro.x = 0;
        _micro.y = 0;
        _micro.r_x = 0;
        _micro.row_off = 0;
        _micro.col_off = 0;
        _micro.num_rows = 0;
        _micro.row = NULL;
        _micro.filename = NULL;
        if(getWindowSize(&_micro.screenrows,&_micro.screencols)==-1)
            die("getWindowSize, initMicro");
        _micro.screenrows -= 1;
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
                if (seq[1] >= '0' && seq[1] <= '9') {
                  if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                  if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3' : return DEL;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                  }
                } else {
                  switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                  }
                }
              }else if (seq[0] == 'O') {
                switch (seq[1]) {
                  case 'H': return HOME_KEY;
                  case 'F': return END_KEY;
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
        erow *row = (_micro.y  >= _micro.num_rows) ? NULL : &_micro.row[_micro.y];
        switch(key){
            case ARROW_UP : 
            if(_micro.y !=0){
            _micro.y--;
            }
                        break;
            case ARROW_DOWN : 
            if(_micro.y < _micro.num_rows){
            _micro.y++;
            }
                        break;
            case ARROW_LEFT :
            if(_micro.x!=0){
                _micro.x--;
            } else if(_micro.y > 0){
                _micro.y--;
                _micro.x = _micro.row[_micro.y].size;
            }
                        break;
            case ARROW_RIGHT : 
            if(row && _micro.x < row->size){
                _micro.x++;
            }else if(row && _micro.x == row->size){
                _micro.y++;
                _micro.x = 0;
            }
                break;
        }
        row = (_micro.y >= _micro.num_rows) ? NULL : &_micro.row[_micro.y];
        int row_len = row ? row->size : 0;
        if(_micro.x > row_len){
            _micro.x = row_len;
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
            case PAGE_UP:
            case PAGE_DOWN:
              {
                if (c == PAGE_UP) {
                    _micro.y = _micro.row_off;
                  } else if (c == PAGE_DOWN) {
                    _micro.y = _micro.row_off + _micro.screenrows - 1;
                    if (_micro.y > _micro.num_rows) _micro.y = _micro.num_rows;
                }
                int times = _micro.screenrows;
                while (times--)
                  _microCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
              }
              break;
              case HOME_KEY:
                _micro.x = 0;
                break;
            case END_KEY:
            if(_micro.y < _micro.num_rows)
                _micro.x = _micro.row[_micro.y].size;
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
        microScroll();
        struct buff a = BUFF_INIT;
        bAppend(&a, "\x1b[?25l", 6);
        bAppend(&a, "\x1b[H", 3); 

        drawRows(&a);
        drawStatusBar(&a);

        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (_micro.y - _micro.row_off)+1, (_micro.r_x - _micro.col_off)+1);
        bAppend(&a, buf, strlen(buf));
        bAppend(&a, "\x1b[?25h", 6);
        write(STDOUT_FILENO, a.b, a.len);
        bFree(&a);
    }
    
    void microScroll(){
        _micro.r_x = 0;
        if (_micro.y < _micro.num_rows) {
            _micro.r_x = convertxToRx(&_micro.row[_micro.y], _micro.x);
          }

        if(_micro.y < _micro.row_off){
            _micro.row_off = _micro.y;
        }
        if(_micro.y >= _micro.row_off + _micro.screenrows){
            _micro.row_off = _micro.y-_micro.screenrows+1;
        }
        if(_micro.r_x < _micro.col_off){
            _micro.col_off = _micro.r_x;
        }
        if(_micro.r_x >= _micro.col_off + _micro.screencols){
            _micro.col_off = _micro.r_x-_micro.screencols+1;
        }
    }

    void drawStatusBar(struct buff *a){
        bAppend(a,"\x1b[7m",4);
        char status[80], rstatus[80];
        int len = snprintf(status, sizeof(status), "%.20s - %d lines",_micro.filename ? _micro.filename : "[No Name]", _micro.num_rows);
        int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
        _micro.y + 1, _micro.num_rows);
        if (len > _micro.screencols) len = _micro.screencols;
        bAppend(a, status, len);
        while(len < _micro.screencols){
            if (_micro.screencols - len == rlen) {
                bAppend(a, rstatus, rlen);
                break;
            } else {
                bAppend(a, " ", 1);
                len++;
            }
        }
        bAppend(a,"\x1b[m",3);
    }

    void drawRows(struct buff *a){
        for(int y=0;y<_micro.screenrows;y++){
            int file_row = y+_micro.row_off;
            if(file_row >= _micro.num_rows){
                if (_micro.num_rows == 0 && y == _micro.screenrows / 3) {
                    char welcome[80];
                    int welcomelen = snprintf(welcome, sizeof(welcome),
                      "Micro editor -- version %s", MICRO_VERSION);
                    if (welcomelen > _micro.screencols) welcomelen = _micro.screencols;
                    int padding = (_micro.screencols - welcomelen) / 2;
                    if (padding) {
                      bAppend(a, "#", 1);
                      padding--;
                    }
                    while (padding--) bAppend(a, " ", 1);
                        bAppend(a, welcome, welcomelen);
                  } else {
                        bAppend(a, "#", 1);
                  }
            }else{
                int len = _micro.row[file_row].rsize-_micro.col_off;
                if(len < 0) len = 0;
                if(len > _micro.screencols) len = _micro.screencols;
                bAppend(a,&_micro.row[file_row].render[_micro.col_off],len);
            }
            bAppend(a, "\x1b[K", 3);
              bAppend(a, "\r\n", 2);
        }
    }

    char *strdup (const char *s) {
        char *d = malloc (strlen (s) + 1);   // Space for length plus nul
        if (d == NULL) return NULL;          // No memory
        strcpy (d,s);                        // Copy the characters
        return d;                            // Return the new string
    }

/*  ***********    Row Operations    *********     */ 
int convertxToRx(erow *row,int x){
    int r_x = 0;
    int j;
    for (j = 0; j < x; j++) {
        if (row->chars[j] == '\t')
          r_x += (MICRO_TAB - 1) - (r_x % MICRO_TAB);
        r_x++;
      }
      return r_x;
}

void microUpdateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
      if (row->chars[j] == '\t') tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs*(MICRO_TAB-1) + 1);
    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % MICRO_TAB != 0) row->render[idx++] = ' ';
          } else {
            row->render[idx++] = row->chars[j];
          }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
  }

void microAppendRow(char *s, size_t len){
    _micro.row = realloc(_micro.row,sizeof(erow)*(_micro.num_rows + 1));
    
    int i = _micro.num_rows;
    _micro.row[i].size = len;
    _micro.row[i].chars = malloc(len+1);
    memcpy(_micro.row[i].chars,s,len);
    _micro.row[i].chars[len] = '\0';
    
    _micro.row[i].rsize = 0;
    _micro.row[i].render = NULL;
    microUpdateRow(&_micro.row[i]);

    _micro.num_rows++;
}

/*  ***********    File I/O Functions    *********     */   
    void microOpen(char *filename){
        free(_micro.filename);
        _micro.filename = strdup(filename);

        FILE *fp = fopen(filename,"r");
        if(!fp)
            die("fopen");
        
        char *line = NULL;
        size_t line_cap = 0;
        size_t len;

        while((len = getline(&line,&line_cap,fp))!=-1){
            while(len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                len --;
            microAppendRow(line,len);
        }
        free(line);
        fclose(fp);
    }