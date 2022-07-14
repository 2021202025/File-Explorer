#include <iostream>
#include <string>
#include <dirent.h>
#include <bits/stdc++.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <sys/ioctl.h>

using namespace std;

struct winsize win;
int modLine, outputLine, statusLine, inputLine, redLine;
char currentDirectory[1024];
size_t directorySize = 1024;
bool commandFlag = false;

string ROOT;
string HOME;

vector<dirent *> fileContents;
static struct termios origSettings, newSettings;
static int peek_char = -1;

#define NoOfRows 12
#define clearscreen() printf("\033[H\033[J")
#define clearLine() printf("\033[K")

int cursor = 1;
int strt = 0, end;

stack<string> back;
stack<string> forwardStack;
vector<string> commandModeCommands;

int commandCursorLoc = 14;
int top = 0;
int colRight = 0;
int xOffset = 0;
int bottom = top + NoOfRows;

int commandLine;

bool comparator(struct dirent *&lhs, struct dirent *&rhs)
{
    return lhs->d_name < rhs->d_name;
}

void moveCursor(int x, int y)
{
    cout << "\033[" << x << ";" << y << "H";
    fflush(stdout);
}

void printError(string s)
{
    moveCursor(redLine, 0);
    cout << "\033[1;91m"
         << "ALERT : " << s << "\033[0m";
    moveCursor(cursor, 0);
}

void printStatusLine(string s)
{
    moveCursor(statusLine, 0);
    cout << "\033[1;92m"
         << "STATUS : " << s << "\033[0m";
    moveCursor(commandLine, 14);
}

//Normal Mode Code

void normalScreenDisplay()
{
    //Creating the layout for normal mode
    moveCursor(commandLine - 2, 0);

    cout << "\033[1;93m"
         << "Normal Mode: 'q' to exit the application, ':' to switch to command mode "
         << "\033[0m";

    moveCursor(commandLine, 0);

    for (int i = commandLine; i < statusLine; i++)
        cout << "\033[K";

    moveCursor(cursor, 0);
}

void printFiles()
{
    clearscreen();
    //
    struct stat fileInfo;
    register uid_t uid;
    register gid_t gid;
    struct passwd *wd;
    struct group *gp;
    string tm;
    register struct group *g;
    register struct passwd *pw;

    // struct stat st;

    int n = fileContents.size();

    for (int it = top; it < bottom && it < n; it++)
    {
        string s = "";
        lstat(fileContents[it]->d_name, &fileInfo);

        if (S_ISREG(fileInfo.st_mode))
        {
            s = s + "-";
        }
        else if (S_ISDIR(fileInfo.st_mode))
        {
            s = s + "d";
        }
        else if (S_ISSOCK(fileInfo.st_mode))
        {
            s = s + "s";
        }
        if (fileInfo.st_mode & S_IRUSR)
        {
            s = s + "r";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IWUSR)
        {
            s = s + "w";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IXUSR)
        {
            s = s + "x";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IRGRP)
        {
            s = s + "r";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IWGRP)
        {
            s = s + "w";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IXGRP)
        {
            s = s + "x";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IROTH)
        {
            s = s + "r";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IWOTH)
        {
            s = s + "w";
        }
        else
        {
            s = s + "-";
        }
        if (fileInfo.st_mode & S_IXOTH)
        {
            s = s + "x";
        }
        else
        {
            s = s + "-";
        }

        s = s + " ";

        wd = getpwuid(fileInfo.st_uid);
        gp = getgrgid(fileInfo.st_gid); // Name and size pointer

        s = s + " " + string(wd->pw_name);
        s = s + " " + string(gp->gr_name);

        int fileSize = fileInfo.st_size;
        int fileMB = fileSize / 1048576;
        int fileKB = fileSize / 1024;
        if (fileMB > 1)
            s = s + " " + to_string(fileMB) + " MB";
        // cout << "\t" << fileMB << " MB";
        else if (fileKB > 1)
            s = s + " " + to_string(fileKB) + " KB";
        // cout << "\t" << fileKB << " KB";
        else
            s = s + " " + to_string(fileKB) + " B";
        // cout << "\t" << fileSize << " B";

        // Current time
        string dt = ctime(&fileInfo.st_mtime);

        s = s + " " + string(dt.substr(4, 12));

        // if ((S_ISDIR(fileInfo.st_mode)))
        // {
        //     s = s + " " + "\033[1;94m" + string(fileContents[it]->d_name) + "\033[0m" + "\n";
        // }

        // else
        // {
        //     s = s + " " + "\033[1;97m" + string(fileContents[it]->d_name) + "\033[0m" + "\n";
        // }
        s = s + " " + string(fileContents[it]->d_name) + "\n";

        if (xOffset < s.length())
            s = s.substr(xOffset, colRight);
        else
            s = "\n";

        cout << s;
    }

    normalScreenDisplay();

    return;
}

void setDirectory(char const *dirPath)
{

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    top = 0;
    bottom = top + NoOfRows;
    colRight = win.ws_col;
    commandLine = win.ws_row - 2;
    statusLine = commandLine + 1;
    redLine = commandLine - 3;
    cursor = 0;

    DIR *dir = opendir(dirPath);

    if (!dir)
    {
        printError("Couldn't open directory");
        return;
    }

    chdir(dirPath);
    getcwd(currentDirectory, directorySize);
    fileContents.clear();

    //dirent pointer points to the next entry in the directory steam
    struct dirent *entity;

    //Read the content of the directory and put them into a vector
    entity = readdir(dir);
    while (entity != NULL)
    {
        fileContents.push_back(entity);
        entity = readdir(dir);
    }

    //trying to sort using filename, didn't work for some reason
    sort(fileContents.begin(), fileContents.end(), comparator);

    closedir(dir);

    //setting the final pointers for the display files command
    top = 0;
    bottom = min(top + NoOfRows, (int)fileContents.size());
    cursor = 0;

    printFiles();
    return;
}

//Scrolling up using arrow keys
void scrollUp()
{

    if (cursor > 1)
    {
        cursor--;
        moveCursor(cursor, 0);
        return;
    }
    if (top != 0)
    {
        top--;
        bottom--;
        printFiles();
        moveCursor(cursor, 0);
    }
    else
    {
        printError("Already at the top                              ");
        return;
    }
}

//Scrolling down using arrow keys
void scrollDown()
{

    if (cursor < fileContents.size() && cursor < NoOfRows)
    {
        cursor++;
        moveCursor(cursor, 0);
        return;
    }

    if (bottom != fileContents.size())
    {
        top++;
        bottom++;
        printFiles();
        moveCursor(cursor, 0);
    }
    else
    {
        printError("Already at the bottom                              ");
        return;
    }
}

//Go back a level
void parentLevel()
{
    if ((string)currentDirectory != "/")
    {
        back.push(string(currentDirectory));
        setDirectory("../");
        return;
    }

    // printError("You're already present in the home directory");
    // return;
}

//Handling opening of files using enter key
void pressReturn()
{
    struct stat fileInfo;

    char *fileName = fileContents[cursor + top - 1]->d_name;

    bool isDir = S_ISDIR(fileInfo.st_mode);

    lstat(fileName, &fileInfo);

    //Checking if the file is a directory
    if (!S_ISDIR(fileInfo.st_mode))
    {
        if (fork() == 0)
        {
            char *args[3];
            string file = fileName;
            string command = "vi";
            string f_type = file.substr(file.size() - 3, file.size() - 1);
            command = "xdg-open";

            args[0] = (char *)command.c_str();
            args[1] = (char *)file.c_str();
            args[2] = NULL;

            if (execvp(args[0], args) == -1)
            {
                perror("exec");
            }
        }
    }
    else
    {
        if (fileName == ".")
        {
            return;
        }

        if (fileName == "..")
        {
            parentLevel();
            return;
        }

        back.push(string(currentDirectory));
        string destination = ((string)currentDirectory + '/' + (string)fileName);

        setDirectory(destination.c_str());
    }
    return;
}

//Scrolling using k and l keys
void windowScrollUp()
{
    if (top == 0)
    {
        printError("Already at the top                              ");
        return;
    }

    if (bottom - 1 >= 0)
    {
        if (top - 1 > 0)
        {
            top -= 1;
        }
        else
            top = 0;

        bottom = top + NoOfRows;
        printFiles();
        moveCursor(cursor, 0);
    }
}

void windowScrollDown()
{
    if (bottom == fileContents.size())
    {
        printError("Already at the bottom                              ");
        return;
    }

    if (top + 1 < fileContents.size())
    {
        top += 1;

        if (bottom + 1 <= fileContents.size())
        {
            bottom = top + NoOfRows;
        }

        else
            bottom = fileContents.size();

        printFiles();
        moveCursor(cursor, 0);
    }
}

//Go to the starting directory on h
void homeSweetHome()
{
    back.push(string(currentDirectory));
    setDirectory(ROOT.c_str());
}

//Command Mode Code
//Displays the UI of command mode
void commandScreenDisplay()
{
    moveCursor(commandLine - 2, 0);

    cout << "\033[1;93m"
         << "Command Mode: 'esc' to switch to normal mode "
         << "\033[0m";

    moveCursor(commandLine, 0);
    cout << "\033[1;96m"
         << "Command: ~$ "
         << "\033[0m";

    moveCursor(commandLine, 14);

    for (int i = commandLine; i < statusLine; i++)
        printf("\033[K");

    moveCursor(commandLine, 14);
}

bool checkDestPath(string destination)
{
    struct stat fileInfo;
    stat(destination.c_str(), &fileInfo);
    if (S_ISDIR(fileInfo.st_mode))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool checkSourcePath(string src)
{
    struct stat fileInfo;
    stat(src.c_str(), &fileInfo);
    if (S_ISDIR(fileInfo.st_mode) or S_ISREG(fileInfo.st_mode))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void copyHelper(string src, string destination)
{

    //create a new file and copy the data from source to destination
    ifstream source(src, ios::binary);
    ofstream dest(destination, ios::binary);
    dest << source.rdbuf();

    //Fetch the file permissions and change the permissions to match the old file
    struct stat st;
    stat(src.c_str(), &st);
    chown(destination.c_str(), st.st_uid, st.st_gid);
    chmod(destination.c_str(), st.st_mode);

    source.close();
    dest.close();
}

//copy file functions sets the proper paths based on the inputs provided
void copyFunc(int i)
{
    int n = commandModeCommands.size();
    string source = commandModeCommands[i];
    string destination = commandModeCommands[n - 1];

    // struct stat fileInfo;
    // lstat(source.c_str(), &fileInfo);

    vector<string> sourcePath;

    stringstream sscommand(source);
    string token;
    while (getline(sscommand, token, '/'))
        sourcePath.push_back(token);

    if (source[0] == '/')
    {
        source = commandModeCommands[i];
    }
    else if (source[0] == '.')
    {
        source = (string)currentDirectory + commandModeCommands[i].substr(1);
    }
    else if (source[0] == '~')
    {
        source = (string)ROOT + commandModeCommands[i].substr(1);
    }
    else
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        source = (string)currentDirectory + '/' + tempFileName;
    }

    if (destination[0] == '/')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = destination + '/' + tempFileName;
    }
    else if (destination[0] == '.')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = currentDirectory + destination.substr(1) + '/' + tempFileName;
    }
    else if (destination[0] == '~')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = (string)ROOT + destination.substr(1) + '/' + tempFileName;
    }
    else
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = currentDirectory + destination.substr(1) + '/' + tempFileName;
    }

    printError(source + "             ");
    printStatusLine(destination + "                    ");
    copyHelper(source, destination);
}

//Creates and copies directories and its sub files recursively
void copyDirHelper(string currentDir, string destination)
{

    DIR *dir = opendir(currentDir.c_str());

    if (!dir)
    {
        printError("Couldn't open directory                                                         ");
        printError(currentDir);
        // deletionStatus = false;
        return;
    }

    struct dirent *entity;
    struct stat fileInfo;

    chdir(currentDir.c_str());

    // entity = readdir(dir);
    // while (entity != NULL)
    while ((entity = readdir(dir)))
    {
        lstat(entity->d_name, &fileInfo);
        string directoryName = string(entity->d_name);

        if (S_ISDIR(fileInfo.st_mode))
        {
            if ((directoryName != ".") and (directoryName != ".."))
            {
                mkdir((destination + '/' + directoryName).c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

                // struct stat st;
                // stat(currentDir.c_str(), &st);
                // chown(destination.c_str(), st.st_uid, st.st_gid);
                // chmod(destination.c_str(), st.st_mode);

                printStatusLine(directoryName);

                copyDirHelper(directoryName, destination + '/' + directoryName);
            }
        }
        else
        {
            copyHelper(directoryName, destination + '/' + directoryName);
        }
        // entity = readdir(dir);
    }

    chdir("..");
    closedir(dir);
    return;
}

//Resolves paths for source and destination
void copyDirFunc(int i)
{
    int n = commandModeCommands.size();
    string source = commandModeCommands[i];
    string destination = commandModeCommands[n - 1];

    vector<string> sourcePath;

    stringstream sscommand(source);
    string token;
    while (getline(sscommand, token, '/'))
        sourcePath.push_back(token);

    if (source[0] == '/')
    {
        source = commandModeCommands[i];
    }
    else if (source[0] == '.')
    {
        source = (string)currentDirectory + commandModeCommands[i].substr(1);
    }
    else if (source[0] == '~')
    {
        source = (string)ROOT + commandModeCommands[i].substr(1);
    }
    else
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        source = (string)currentDirectory + '/' + tempFileName;
    }

    if (destination[0] == '/')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = destination + '/' + tempFileName;
    }
    else if (destination[0] == '.')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = currentDirectory + destination.substr(1) + '/' + tempFileName;
    }
    else if (destination[0] == '~')
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = (string)ROOT + destination.substr(1) + '/' + tempFileName;
    }
    else
    {
        string tempFileName = sourcePath[sourcePath.size() - 1];
        destination = currentDirectory + destination.substr(1) + '/' + tempFileName;
    }

    printError(source + "             ");
    printStatusLine(destination + "                           ");
    mkdir(destination.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);

    copyDirHelper(source + '/', destination);
}

void copy()
{
    int n = commandModeCommands.size();
    cout << n << endl;
    struct stat fileInfo;

    for (int i = 1; i < n - 1; i++)
    {
        string loc = "";

        if (commandModeCommands[i][0] == '/')
        {
            loc = commandModeCommands[i];
        }
        else if (commandModeCommands[i][0] == '.')
        {
            loc = (string)currentDirectory + commandModeCommands[i].substr(1);
        }
        else if (commandModeCommands[i][0] == '~')
        {
            loc = (string)ROOT + commandModeCommands[i].substr(1);
        }
        else
        {
            loc = (string)currentDirectory + "/" + commandModeCommands[i];
        }

        lstat(loc.c_str(), &fileInfo);
        // printError(loc + "                                 ");

        if (S_ISDIR(fileInfo.st_mode))
        {
            if (checkSourcePath(loc))
            {
                copyDirFunc(i);
            }
            else
            {
                printStatusLine("Invalid Command                                               ");
            }
        }
        else
        {
            if (checkSourcePath(loc))
            {
                copyFunc(i);
            }
            else
            {
                printStatusLine("Invalid Command                                               ");
            }
        }
    }
}

//Create file
void createFile()
{
    int i = 1;
    int n = commandModeCommands.size();

    while (i < commandModeCommands.size() - 1)
    {

        string destination = commandModeCommands[n - 1];

        if (destination[0] == '.')
        {
            destination = currentDirectory + destination.substr(1) + '/' + commandModeCommands[i];
        }
        else if (destination[0] == '~')
        {
            destination = (string)ROOT + destination.substr(1) + '/' + commandModeCommands[i];
        }
        else if (destination[0] == '/')
        {
            destination = destination + '/' + commandModeCommands[i];
        }

        // printError(source + "                                 ");
        printStatusLine(destination + "                                ");

        ofstream dest(destination, ios::binary);
        dest.close();

        i++;
    }

    setDirectory(currentDirectory);
}

//Create Directory code
void createDirectory()
{
    int i = 1;
    int n = commandModeCommands.size();

    while (i < commandModeCommands.size() - 1)
    {

        string destination = commandModeCommands[n - 1];

        if (destination[0] == '.')
        {
            destination = (string)currentDirectory + destination.substr(1) + '/' + commandModeCommands[i];
        }
        else if (destination[0] == '~')
        {
            destination = (string)ROOT + destination.substr(1) + '/' + commandModeCommands[i];
        }
        else if (destination[0] == '/')
        {
            destination = destination + '/' + commandModeCommands[i];
        }

        // printError(source + "                          ");
        printStatusLine(destination + "                             ");
        // exit(0);
        mkdir(destination.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
        // dest.close();

        i++;
    }

    setDirectory(currentDirectory);
}

void gotoLocation()
{
    int n = commandModeCommands.size();
    if (n == 2)
    {
        back.push(currentDirectory);
        if (commandModeCommands[1][0] == '/')
        {
            setDirectory(commandModeCommands[1].c_str());
        }
        else if (commandModeCommands[1][0] == '.')
        {
            string destination = currentDirectory + commandModeCommands[1].substr(1);
            setDirectory(destination.c_str());
        }
        else if (commandModeCommands[1][0] == '~')
        {
            string destination = (string)ROOT + commandModeCommands[1].substr(1);
            setDirectory(destination.c_str());
        }
    }
    else
    {
        printError("ERROR! Invalid Command                                                        ");
    }
}

void rename()
{
    int n = commandModeCommands.size();
    // cout << n << endl;
    // exit(0);
    if (n != 3)
    {
        printStatusLine("ERROR! Invalid Command                                                          ");
        return;
    }

    string prevName = commandModeCommands[1];
    string newName = commandModeCommands[2];

    if (prevName[0] == '.')
    {
        prevName = currentDirectory + prevName.substr(1);
    }
    else if (prevName[0] == '~')
    {
        prevName = (string)ROOT + prevName.substr(1);
    }

    if (newName[0] == '.')
    {
        newName = currentDirectory + newName.substr(1);
    }
    else if (newName[0] == '~')
    {
        newName = (string)ROOT + newName.substr(1);
    }

    rename(prevName.c_str(), newName.c_str());
    setDirectory(currentDirectory);
}

//search recursilvely inside the current directory
bool search(string currentDir, string toSearch)
{
    DIR *dir = opendir(currentDir.c_str());

    if (!dir)
    {
        printError("Couldn't open directory");
        return false;
    }

    chdir(currentDir.c_str());

    struct dirent *entity;
    struct stat fileInfo;

    entity = readdir(dir);
    while (entity != NULL)
    {
        lstat(entity->d_name, &fileInfo);
        string directoryName = string(entity->d_name);

        if (directoryName == toSearch)
        {
            setDirectory(currentDir.c_str());
            return true;
        }
        if (S_ISDIR(fileInfo.st_mode))
        {
            if (directoryName != "." and directoryName != "..")
            {
                string newDir = currentDir + '/' + directoryName;
                bool check = search(newDir, toSearch);
                if (check)
                {
                    return true;
                }
            }
        }

        entity = readdir(dir);
    }

    chdir("..");
    closedir(dir);
    return false;
}

// bool deletionStatus = true;
//Delete the directory provided
void deleteRecursive(string currentDir)
{

    DIR *dir = opendir(currentDir.c_str());

    if (!dir)
    {
        printError("Couldn't open directory");
        // deletionStatus = false;
        return;
    }

    chdir(currentDir.c_str());

    struct dirent *entity;
    struct stat fileInfo;

    entity = readdir(dir);

    while (entity != NULL)
    {
        lstat(entity->d_name, &fileInfo);
        string directoryName = string(entity->d_name);

        if (S_ISDIR(fileInfo.st_mode))
        {
            if (directoryName != "." and directoryName != "..")
            {
                // string newDir = currentDir + '/' + directoryName;
                deleteRecursive(directoryName);
                rmdir(directoryName.c_str());
            }
        }

        else
        {
            unlink(directoryName.c_str());
        }

        entity = readdir(dir);
    }

    chdir("..");
    closedir(dir);
}

bool deleteDir()
{
    if (currentDirectory != commandModeCommands[1])
    {
        if (commandModeCommands[1][0] == '.')
        {
            commandModeCommands[1] = currentDirectory + commandModeCommands[1].substr(1);
        }
        else if (commandModeCommands[1][0] == '~')
        {
            commandModeCommands[1] = (string)ROOT + commandModeCommands[1].substr(1);
        }

        deleteRecursive(commandModeCommands[1]);
        rmdir(commandModeCommands[1].c_str());
        // return status;
        return 1;
    }
    return 0;
}

void moveFile(int i)
{
    copyFunc(i);
}

//Move implementation using copy and delete functions
void move()
{
    int n = commandModeCommands.size();
    // cout << n << endl;
    struct stat fileInfo;

    string destination = commandModeCommands[n - 1];

    if (destination[0] == '/')
    {
        destination = destination;
    }
    else if (destination[0] == '.')
    {
        destination = currentDirectory + destination.substr(1);
    }
    else if (destination[0] == '~')
    {
        destination = (string)ROOT + destination.substr(1);
    }
    else
    {
        destination = currentDirectory + destination.substr(1);
    }

    for (int i = 1; i < n - 1; i++)
    {
        string loc = "";

        if (commandModeCommands[i][0] == '/')
        {
            loc = commandModeCommands[i];
        }
        else if (commandModeCommands[i][0] == '.')
        {
            loc = (string)currentDirectory + commandModeCommands[i].substr(1);
        }
        else if (commandModeCommands[i][0] == '~')
        {
            loc = (string)ROOT + commandModeCommands[i].substr(1);
        }
        else
        {
            loc = (string)currentDirectory + "/" + commandModeCommands[i];
        }

        lstat(loc.c_str(), &fileInfo);
        printError(destination + "              ");

        if (S_ISDIR(fileInfo.st_mode))
        {
            if (checkSourcePath(loc) and checkDestPath(destination))
            {
                copyDirFunc(i);
                deleteRecursive(loc);
                rmdir(loc.c_str());
                setDirectory(currentDirectory);
            }
            else
            {
                printStatusLine("Invalid Command                                               ");
            }
        }

        else
        {
            if (checkSourcePath(loc) and checkDestPath(destination))
            {
                copyFunc(i);
                int status = unlink(loc.c_str());
                if (!status)
                {
                    setDirectory(currentDirectory);
                    printStatusLine("Move Successful                                                          ");
                }
                else
                {
                    setDirectory(currentDirectory);
                }
            }
            else
            {
                printStatusLine("Invalid Command                                               ");
            }
        }
    }
}

string commandInput()
{

    string consoleCommand;
    commandCursorLoc = 14;

    moveCursor(commandLine, 14);

    while (true)
    {
        char ch = cin.get();

        if (ch == 27)
        {
            normalScreenDisplay();
            return "esc";
        }
        // else if (ch == 'q')
        // {
        //     exit(0);
        // }
        else if (ch == 10)
        {
            commandCursorLoc = 14;
            moveCursor(commandLine, commandCursorLoc);
            return consoleCommand;
        }
        else
        {
            if (ch != 127)
            {
                consoleCommand.push_back(ch);
                cout << ch;
                moveCursor(commandLine, ++commandCursorLoc);
            }
            else
            {
                if (consoleCommand.size() < 1)
                {
                    continue;
                }
                consoleCommand.pop_back();
                commandCursorLoc -= 1;
                commandScreenDisplay();
                cout << consoleCommand;
                moveCursor(commandLine, 14 + consoleCommand.size());
            }
        }
    }
}

//Handling various queries from user
void performActions(string task)
{
    if (task == "copy")
    {
        copy();
    }

    else if (task.size() == 1 and task[0] == 'q')
    {
        exit(0);
    }

    else if (task == "create_file")
    {
        createFile();
    }

    else if (task == "create_dir")
    {
        createDirectory();
    }

    else if (task == "delete_file")
    {
        int n = commandModeCommands.size();
        if (n != 2)
        {
            printStatusLine("ERROR! Invalid Command                                                          ");
        }
        else
        {
            string fileToDelete = commandModeCommands[1];

            if (fileToDelete[0] == '.')
            {
                fileToDelete = currentDirectory + fileToDelete.substr(1);
            }
            else if (fileToDelete[0] == '~')
            {
                fileToDelete = (string)ROOT + fileToDelete.substr(1);
            }

            int status = unlink(fileToDelete.c_str());
            if (status != 0)
            {
                printStatusLine("ERROR! Deletion Failed                                                          ");
            }
            else
            {
                setDirectory(currentDirectory);
                printStatusLine("Delete Successful                                                          ");
            }
        }
    }

    else if (task == "delete_dir")
    {
        int n = commandModeCommands.size();
        if (n != 2)
        {
            printStatusLine("ERROR! Invalid Command                                                          ");
        }
        else
        {
            auto status = deleteDir();

            if (!status)
            {
                printStatusLine("ERROR! Deletion Failed                                                          ");
            }
            else
            {
                setDirectory(currentDirectory);
                printStatusLine("Delete Successful                                                          ");
            }
        }
    }

    else if (task == "move")
    {
        move();
    }

    else if (task == "goto")
    {
        gotoLocation();
    }

    else if (task == "rename")
    {
        rename();
    }

    else if (task == "search")
    {
        int n = commandModeCommands.size();
        if (n != 2)
        {
            printStatusLine("ERROR! Invalid Command                                                          ");
        }
        else
        {
            bool flag = search(currentDirectory, commandModeCommands[1]);
            if (flag)
            {
                printStatusLine("Found                                                          ");
            }
            else
            {
                printStatusLine("Not Found                                                          ");
            }
        }
    }

    else
    {
        printStatusLine("Error, Command not available");
    }
}

void commandMode()
{
    while (true)
    {
        commandScreenDisplay();
        commandModeCommands.clear();
        string command = commandInput();

        if (command != "esc")
        {
            // printNormalMode();
            // break;
            clearLine();
        }
        else
        {
            break;
        }

        stringstream sscommand(command);
        string token;
        while (getline(sscommand, token, ' '))
            commandModeCommands.push_back(token);

        performActions(commandModeCommands[0]);
    }
    return;
}

// void initTerminalDimensions()
// {

//     ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

//     bottom = win.ws_row - 1;
// }

// void window_resize_handler(int signal)
// {
//     initTerminalDimensions();
// }

void normalMode()
{
    //Infinite loop to take user's input and call functions respectively
    while (1)
    {
        // signal(SIGWINCH, window_resize_handler);

        char ch = cin.get();

        if (ch == 'A')
        {
            scrollUp();
        }
        if (ch == 'B')
        {
            scrollDown();
        }
        if (ch == 10)
        {
            pressReturn();
        }
        if (ch == 127)
        {
            parentLevel();
            printError((string)currentDirectory + "       ");
        }
        if (ch == 'k')
        {
            windowScrollUp();
        }
        if (ch == 'l')
        {
            windowScrollDown();
        }
        if (ch == 68)
        {
            if (back.size() > 0)
            {
                string s = back.top();
                back.pop();
                forwardStack.push(string(currentDirectory));
                setDirectory(s.c_str());
            }
            printError((string)currentDirectory + "              ");
        }
        if (ch == 67)
        {
            if (forwardStack.size() > 0)
            {
                string s = forwardStack.top();
                forwardStack.pop();
                back.push(string(currentDirectory));
                setDirectory(s.c_str());
            }
            printError((string)currentDirectory + "           ");
        }
        if (ch == 'h' or ch == 'H')
        {
            homeSweetHome();
            printError((string)currentDirectory + "           ");
        }
        if (ch == ':')
        {
            commandMode();
        }

        if (ch == 'a')
        {
            if (xOffset > 0)
                xOffset--;
            setDirectory(currentDirectory);
        }

        if (ch == 'd')
        {
            xOffset++;
            setDirectory(currentDirectory);
        }

        if (ch == 'q')
        {
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    //using escape sequences to clear the screen
    clearscreen();

    getcwd(currentDirectory, directorySize);

    //Setting up the new terminal settins (setting to non canonical mode)
    tcgetattr(0, &origSettings);
    newSettings = origSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);
    newSettings.c_cc[VMIN] = 1;
    newSettings.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings) != 0)
    {
        printError("Some Error Occured, Not able to switch to Normal Mode");
    }

    // setRoot(currentDirectory);

    //Intializing the root the program first runs in
    char *user_name = getenv("USER");
    // std::cout << user_name << std::endl;
    // exit(1);
    ROOT = "/home/" + string(user_name);
    // HOME = "/home/manu";

    //Getting current window size and setting up the variables required
    // ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    // top = 0;
    // bottom = top + NoOfRows;
    // commandLine = win.ws_row - 2;
    // statusLine = commandLine + 1;
    // redLine = commandLine - 3;
    // cursor = 0;

    //Get the content of directory and execute print the contents
    setDirectory(currentDirectory);
    clearscreen();
    setDirectory(currentDirectory);

    //Calling normal mode
    normalMode();

    //returning the terminal to original settings
    tcsetattr(STDIN_FILENO, TCSANOW, &origSettings);
    clearscreen();

    cout << endl
         << endl
         << endl;
    cout
        << "---------------------------------------------------- Welp! That was FUN ... *Smile in Pain* ----------------------------------------------------" << endl;
    cout << endl
         << endl
         << endl;

    return 0;
}
