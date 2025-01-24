#include <stdio.h>
#ifdef __sun
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#include <unistd.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

int x = 0, y = 0;
string regexPattern;
string replacePattern;
string hint;
string prompt;

vector<string> lines;
vector<vector<string>> coloredLines;
vector<string> replaced;
vector<vector<string>> captures;
vector<int> caplen;

vector<vector<string>> captureSolutions;
vector<string> replaceSolutions;

enum app_state_t {APP_MENU, APP_PLAY, APP_LOAD_LEVEL};
app_state_t appState = APP_MENU;

enum menu_choices_t {MC_START, MC_CONTINUE, MC_LOAD, MC_EXIT};
int menuChoice = 1;

bool isExtended = false;
bool isGlobal = false;
bool isReplaceMode = false;
bool match = false;
bool finished = false;
bool started = false;

//Key code bindings
static const int CTRL_E = 5;
static const int CTRL_G = 7;
static const int CTRL_X = 24;
static const int ENTER_KEY = 10;

//Menu things
static const int MENU_WIDTH = 54;
static const int MENU_HEIGHT = 8;
static const int MAX_MENU_OPTION = 3;

void mouseAction(int mx, int my)
{
    if (my == 2)
    {
        if (mx < 27)
            isExtended = !isExtended;
        else if (mx > 31 && mx < 45)
            isGlobal = !isGlobal;
        //else if (mx > 38 && mx < 57)
        //    isReplaceMode = !isReplaceMode;
    }
    else if (my == 0)
    {
        y = 0;
        x = mx-16;
        if (mx-16 > regexPattern.length())
            x = regexPattern.length();
    }
    else if (my == 1)
    {
        y = 1;
        x = mx-16;
        if (mx-16 > replacePattern.length())
            x = replacePattern.length();
    }
}

void keyAction( int ch )
{
    switch (ch)
    {
        case KEY_MOUSE:
            MEVENT event;
            getmouse(&event);
            mouseAction(event.x,event.y);
            break;
        case KEY_BACKSPACE:
        case 127:
        //Capture backspace on xterm too (^H)
        case 8:
            if (y == 0 && x > 0 && regexPattern.length() >= x)
            {
                regexPattern.erase(regexPattern.begin()+x-1);
                x--;
            }
            if (y == 1 && x > 0 && replacePattern.length() >= x)
            {
                replacePattern.erase(replacePattern.begin()+x-1);
                x--;
            } 
            break;
        case KEY_LEFT:
            if (x>0)
                x--;
            break;
        case KEY_RIGHT:
            if (y == 0 && x < regexPattern.length())
                x++;
            if (y == 1 && x < replacePattern.length())
                x++;
            break;
        case KEY_UP:
            y = 0;
            if (x > regexPattern.length())
                x = regexPattern.length();
            break;
        case KEY_DOWN:
            y = 1;
            if (x > replacePattern.length())
                x = replacePattern.length();
            break;
        case CTRL_X:
            clear();
            refresh();
            appState = APP_MENU;
            break;
        case CTRL_E:
            isExtended = !isExtended;
            break;
        case CTRL_G:
            isGlobal = !isGlobal;
            break;
        default:
            if (ch >= 32 && ch < 127)
            {
                if (y == 0)
                    regexPattern.insert(regexPattern.begin()+x, ch);
                else
                    replacePattern.insert(replacePattern.begin()+x, ch);
                x++;
            } 
    }
}

bool checkSolution()
{
    if (isReplaceMode)
    {
        if (replaced.size() != replaceSolutions.size())
            return false;
        for (int i = 0; i < replaced.size(); i++)
        {
            if (replaced[i] != replaceSolutions[i])
            {
                hint = "Check number ";
                hint += to_string(i+1);
                hint += "  Expected \"" + replaceSolutions[i] + "\"";
                hint += " got \"" + replaced[i] + "\"";
                return false;
            }
        }
        return true;
    }
    else if (!match)
    {
        if (captureSolutions.size() != captures.size())
            return false;
        for (int i = 0; i < captureSolutions.size(); i++)
        {
            while (captureSolutions[i].size() > captures[i].size())
            {
                captures[i].push_back("");
            }
            for (int j = 1; j < captureSolutions[i].size(); j++)
            {
                if (captureSolutions[i][j] != captures[i][j])
                {
                    hint = "Check line ";
                    hint += to_string(i+1);
                    hint += ", Capture group \\";
                    hint += to_string(j);
                    hint += "  ";
                    hint += "Expected \"" +captureSolutions[i][j]+"\" ";
                    hint += "got \"" + captures[i][j] +"\"";
                    return false;
                }
            }
        }
        return true;
    }
    else
    {
        if (captureSolutions.size() != captures.size())
            return false;
        for (int i = 0; i < captureSolutions.size(); i++)
        {
            if (captureSolutions[i][0] == "")
            {
                if (captures[i].size() != 0)
                {
                    hint = "Line number ";
                    hint += to_string(i+1);
                    hint += " should not be captured";
                    return false;
                }
            }
            else
            {
                if (captures[i].size() == 0)
                {
                    hint = "Line number ";
                    hint += to_string(i+1);
                    hint += " should be captured";
                    return false;
                }
            }
        }
        return true;
    }

        
}

//Returns a pointer to a new window centered on screen. If w and h = 0, window will be of the size of the screen
WINDOW* createNewWindow(int height, int width) {
    if (width > 0 && height > 0) {
        return newwin(
            height, 
            width, 
            (getmaxy(stdscr) - height) / 2, 
            (getmaxx(stdscr) - width) / 2);
    } else {
        return newwin(getmaxy(stdscr), getmaxx(stdscr), 0, 0);
    }
}

//Draws a status bar, with a text
void drawStatusBar(WINDOW *window, const char* message = "Status Bar Text") {
    move(getmaxy(window) - 1,0);
    attron(COLOR_PAIR(4));
    addstr(message);
    attroff(COLOR_PAIR(4));
}

void drawMenu(WINDOW *menuWindow, int selectedItem) {    
    box(menuWindow, 0, 0);
    wattron(menuWindow, COLOR_PAIR(6)|A_BOLD);
    mvwaddstr(menuWindow, 0, 2, "~regexProgram~");
    wattroff(menuWindow, COLOR_PAIR(6)|A_BOLD);

    mvwprintw(menuWindow, 2, 2, "START from the beginning.");
    mvwprintw(menuWindow, 3, 2, "CONTINUE from where you left. (Not implemented)");
    mvwprintw(menuWindow, 4, 2, "LOAD a level to start from. (Not implemented)");
    mvwprintw(menuWindow, 5, 2, "EXIT");

    switch (selectedItem) {
        case MC_START:
            wattron(menuWindow, A_BOLD | A_REVERSE);
            mvwprintw(menuWindow, 2, 2, "START from the beginning.");
            wattroff(menuWindow, A_BOLD | A_REVERSE);
            break;
        case MC_CONTINUE:
            wattron(menuWindow, A_BOLD | A_REVERSE);
            mvwprintw(menuWindow, 3, 2, "CONTINUE from where you left. (Not implemented)");
            wattroff(menuWindow, A_BOLD | A_REVERSE);
            break;
        case MC_LOAD:
            wattron(menuWindow, A_BOLD | A_REVERSE);
            mvwprintw(menuWindow, 4, 2, "LOAD a level to start from. (Not implemented)");
            wattroff(menuWindow, A_BOLD | A_REVERSE);
            break;
        case MC_EXIT:
            wattron(menuWindow, A_BOLD | A_REVERSE);
            mvwprintw(menuWindow, 5, 2, "EXIT");
            wattroff(menuWindow, A_BOLD | A_REVERSE);
            break;
        default:
            break;
    }

    wrefresh(menuWindow);
}

void menuKeyAction(WINDOW *menuWindow, int *menuOption) {
    int ch = wgetch(menuWindow);

    switch (ch) {
        case KEY_UP:
            (*menuOption)--;
            if (*menuOption < 0) {
                *menuOption = MAX_MENU_OPTION;
            }
            break;
        case KEY_DOWN:
            (*menuOption)++;
            if (*menuOption > MAX_MENU_OPTION) {
                *menuOption = 0;            
            }
            break;
        case ENTER_KEY:
            switch (*menuOption) {
                case MC_START:
                     MC_CONTINUE:
                        appState = APP_PLAY;
                        break;
                case MC_LOAD:
                    appState = APP_LOAD_LEVEL;
                    break;
                case MC_EXIT:
                    endwin();
                    printf("Thank you, Bye!\n");
                    exit(0);
                    break;
                default:
                    appState = APP_PLAY;
                    break;
            }
            break;
        default: 
            appState = APP_PLAY;
            break;
    }
}

void drawLevelSelectionList(WINDOW *window, int* level) {
    wclear(window);
    box(window, 0, 0);
    mvwprintw(window, 1, 2, "Please select a level from the list: ");

    //TODO: Maybe we should try first the checks of the existance of the directory, know beforehand the items
    // and then print them accordingly.
    fs::path levelsPath("./problems"); //TODO: allow changing directory or assign it by arguments
    if (!fs::exists(levelsPath)) appState = APP_MENU; //Return to main menu (temporary fix)
    fs::directory_iterator end_itr; // default construction yields past-the-end
    int cursorHeightCount = 3;
    for (fs::directory_iterator itr(levelsPath); itr != end_itr; ++itr) {
        //cout << itr->path().leaf() << endl;
        mvwprintw(window, cursorHeightCount, 2, itr->path().filename().string().data());
        cursorHeightCount++;
    }
}

void drawScreen(WINDOW *window)
{
    clear();
    //Draw the status / messsage bar at the bottom
    drawStatusBar(window, "Press [Control + X] to go back to main menu.");

    //Draw the base interface
    move(0,0);
    addstr("Regex Pattern: ");
    move(0,16);
    addstr(regexPattern.c_str());
    move(1,0);
    addstr("Replace With:  ");
    move(1,16);
    addstr(replacePattern.c_str());
    move(2,0);
    addstr("Extended Regex (Ctrl-E) [ ]    Global Tag (Ctrl-G) [ ]      ");
    if (isReplaceMode)
        addstr("--Replace Mode--");
    else
        addstr("--Search Mode--");

    if (isExtended)
        mvaddch(2,25,'X');
    if (isGlobal)
        mvaddch(2,52,'X');

    move(5,0);
    addstr("HINT:  ");
    if (!checkSolution())
        addstr(hint.c_str());
    else
    {
        attron(COLOR_PAIR(3));
        addstr("CORRECT! Press any key to continue");
        attroff(COLOR_PAIR(3));
    }
    move(4,0);
    addstr("TASK:  ");
    addstr(prompt.c_str());

    //Draw line numbers
    mvaddstr(7, 0, "#");
    attron(COLOR_PAIR(5));
    for (int i = 0; i < lines.size(); i++) {
        mvprintw(8+i, 0, "%d", i+1);
    }
    attroff(COLOR_PAIR(5));

    //Draw the rest
    for (int i = 0; i < lines.size(); i++)
    {
        int color = 1;
        
        move(7, 3);
        addstr("Text:");
        move(8+i, 3);
        for (int j = 0; j < coloredLines[i].size(); j++)
        {
            if (j%2!=0)
            {
                color = 1 - color;
                attron(COLOR_PAIR(color+1));
            }
            addstr(coloredLines[i][j].c_str()); 
            if (j%2!=0)
                attroff(COLOR_PAIR(color+1));
        }
        if (isReplaceMode)
        {
            move(7, 53);
            addstr("Replaced String:");
            move(8+i, 53); 
            addstr(replaced[i].c_str());
        }
        else
        {
            int loc = 53;
            move(6, loc);
            addstr("Captured String:");
            for (int j = 0; j < caplen.size() && j < 9; j++)
            {
                move (7, loc);
                addstr("\\");
                addch(j+'0');
                loc += caplen[j];
            }
            
            loc = 53;
            for (int j = 0; j < captures[i].size(); j++)
            {
                move(8+i, loc); 
                addstr(captures[i][j].c_str());
                loc += caplen[j];
            }
        }
    }
    move(y, x+16);
}

void performRegex()
{
    replaced.clear();
    caplen.clear();
    caplen.push_back(5);
    hint = "";
    bool valid = true;
    regex r;
    try
    {  
        if (isExtended)
        {
            r.assign(regexPattern.c_str(), regex::extended);
        }
        else
        {
            if (!regexPattern.empty() && regexPattern.back() != '\\')
                r.assign(regexPattern.c_str(), regex::basic);
            else
                valid = false;
        }
    }
    catch (regex_error &e)
    {
        valid = false;
    }
    if (!valid)
        hint = "Invalid regex!";

    for (int i = 0; i < lines.size(); i++)
    {
        coloredLines[i].clear();
        captures[i].clear();
        if (!valid)
        {
            replaced.push_back("");
            coloredLines[i].push_back(lines[i]);
            continue;
        }
        smatch m;
        string s= lines[i];
        auto words_begin = sregex_iterator(s.begin(), s.end(), r);
        auto words_end = sregex_iterator();

        for (sregex_iterator j = words_begin; j != words_end; j++)
        {
            m = *j;
            coloredLines[i].push_back(m.prefix().str());
            coloredLines[i].push_back(m.str());
            if (!isGlobal)
                break;
        }
        if (words_begin == words_end)
            coloredLines[i].push_back(s);
        else
            coloredLines[i].push_back(m.suffix().str());

        if (!isGlobal && regex_search (s,m,r)) {
            int numcap = 0;
            for (auto x:m) 
            {
                captures[i].push_back(x.str());
                if (numcap >= caplen.size())
                    caplen.push_back(5);
                if (x.str().length() > caplen[numcap])
                    caplen[numcap] = x.str().length()+3;
                numcap++;
            }
        }

        if (isGlobal)
            replaced.push_back(regex_replace(s,r,replacePattern));
        else
            replaced.push_back(regex_replace(s,r,replacePattern,regex_constants::format_first_only));
    }
}

void getFile(string s)
{
    ifstream infile(s);
    string line;
    getline(infile,line);
    
    lines.clear();
    coloredLines.clear();
    captures.clear();
    replaced.clear();
    caplen.clear();

    captureSolutions.clear();
    replaceSolutions.clear();
    if (line == "--prompt")
    {
        string p;
        getline(infile, p);
        prompt = p;
        getline(infile,line);
    }
    if (line == "--line")
    {
        for (string s; getline(infile,s);)
        {
            if (s.length() >= 2 && s[0] == '-' && s[1] == '-')
            {
                line = s;
                break;
            }
            lines.push_back(s);
            coloredLines.push_back(vector<string>());
            captures.push_back(vector<string>());
            captureSolutions.push_back(vector<string>());
        }
    }
    if (line == "--capture" || line == "--match")
    {
        match = line == "--match";
        isReplaceMode = false;
        string s;
        int i = 0;
        while (getline(infile,s))
        {
            if (i == lines.size())
                i = 0;
            else
            {
                captureSolutions[i].push_back(s);
                i++;
            }
        }
    }
    else if (line == "--replace")
    {
        isReplaceMode = true;
        for (string s; getline(infile,s);)
        {
            replaceSolutions.push_back(s);
        }
    }
}

void initCurses()
{
    initscr();
    clear();
    start_color();
    use_default_colors();
    cbreak();
    noecho();
    mousemask(BUTTON1_PRESSED, NULL);    
    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(2, COLOR_BLACK, COLOR_BLUE);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_WHITE);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);
    init_pair(6, COLOR_GREEN, -1);
}

int main ()
{    
    WINDOW *appWindow;
    WINDOW *menuWindow;
    WINDOW *levelLoadWindow;

    //Initial highlighted menu option
    int menuOption = 0;

    int level = 1;
    int ch = 0;
    getFile("problems/problem" + to_string(level));
    initCurses();

    //Create app window
    appWindow = createNewWindow(0, 0);
    //Create menu window
    menuWindow = createNewWindow(MENU_HEIGHT, MENU_WIDTH);
    //Create level load window
    levelLoadWindow = createNewWindow(18, 56);
    //Enable keypad (arrows and so) for both windows
    keypad(appWindow, TRUE);
    keypad(menuWindow, TRUE);    
    refresh();
    
    while(true)
    {
        if (appState == APP_MENU) {
            drawMenu(menuWindow, menuOption);            
            menuKeyAction(menuWindow, &menuOption);
        } else if (appState == APP_LOAD_LEVEL) {
            //Clear the menu, draw the file list
            drawStatusBar(appWindow, "Press [Control + X] to go back to main menu.");
            wrefresh(appWindow);
            drawLevelSelectionList(levelLoadWindow, &level);
            wrefresh(levelLoadWindow);
            ch = getch();
        } else {
            if (checkSolution())
            {
                regexPattern = "";
                replacePattern = "";
                x = 0;
                y = 0;
                if (level == 10)
                    break;
                level++;
                getFile("problems/problem" + to_string(level));
            }
            
            performRegex();
            drawScreen(appWindow);
            ch = getch();
            keyAction(ch);
        }
    }
    endwin();
    printf("end\n");
    return 0;
}
