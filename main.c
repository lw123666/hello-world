#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMGSIZE 1440*1024

struct BPB {
    int BytsPerSec;
    int SecPerClus;
    int RsvdSecCnt;
    int NumFATs;
    int RootEntCnt;
    int FATSz16;
} bpb;

struct Entry {
    char Name[13];//文件名，包括扩展名
    int FstClus;//开始簇号
    int Type; // 文件为0,目录为1
    struct Entry *Next, *Children;
} root;

unsigned char image[IMGSIZE];

extern void my_print(char c, int flag);

void lowerStr(char* str) {
    int i;
    for (i = 0; str[i] != 0; ++i) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 'a' - 'A';
        }
    }
}

int getIntValue(int base, int len)
{
    int result = 0;
    int i;
    for (i = base + len - 1; i >= base; --i) {
        result = result * 256 + image[i];
    }
    return result;
}
//一次读取六字节,奇数号高三位,偶数号低三位
int nextClus(int clus) {
    int num = clus / 2;
    int value = getIntValue(bpb.RsvdSecCnt * bpb.BytsPerSec + num * 3, 3);
    if (clus % 2 == 0) {
        return value & 0x000fff;
    } else {
        return (value & 0xfff000) / 0x1000;
    }
}


void setBPB(){
    bpb.BytsPerSec = getIntValue(11, 2);
    bpb.SecPerClus = getIntValue(13, 1);
    bpb.RsvdSecCnt = getIntValue(14, 2);
    bpb.NumFATs = getIntValue(16, 1);
    bpb.RootEntCnt = getIntValue(17, 2);
    bpb.FATSz16 = getIntValue(22, 2);
}

void createTree(struct Entry* entry) {
    int clus = entry->FstClus;
    int isRoot = 1;
    int base = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec;
    if (clus >= 2) {
        // 数据区
        isRoot = 0;
        base = base + bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
    }
    int i;
    for (i = base; ; i += 32) {
        // 完成与否
        if (isRoot == 1 && i >= base + bpb.RootEntCnt * 32) {
            break;
        } else if (isRoot == 0 && i >= base + bpb.BytsPerSec * bpb.SecPerClus) {
            if ((clus = nextClus(clus)) >= 0xff7) {
                break;
            } else {
                i = base = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec + bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
            }
        }
        // 过滤
        if (image[i] != '.' && image[i] != 0 && image[i] != 5 && image[i] != 0xE5
            && image[i + 0xB] != 0xF && (image[i + 0xB] & 0x2) == 0) {
            struct Entry* new_entry = malloc(sizeof(struct Entry));
            
            int j;
            for (j = i; j < i + 8 && image[j] != 0x20; j++) {
                new_entry->Name[j-i] = image[j];
            }
            
            if ((image[i + 0xB] & 0x10) == 0) {
                // 文件
                new_entry->Type = 0;
                new_entry->Name[j-i]='.';
                int k;
                for (k = i + 8; k < i + 11 && image[k] != 0x20; k++) {
                    j++;
                    new_entry->Name[j - i] = image[k];
                }
                j++;
                new_entry->Name[j-i]=0;
               lowerStr(new_entry->Name);
            } else {

                new_entry->Type = 1;
                new_entry->Name[j-i]=0;
                lowerStr(new_entry->Name);
            }
            new_entry->FstClus = getIntValue(i + 26, 2);
            new_entry->Next = NULL;
            new_entry->Children = NULL;

            if (entry->Children == NULL) {
                entry->Children = new_entry;
            } else {
                struct Entry* ptr = entry->Children;
                while (ptr->Next != NULL) {
                    ptr = ptr->Next;
                }
                ptr->Next = new_entry;
            }

            if (new_entry->Type == 1) {
                createTree(new_entry);
            }
        }
    }
}

void printAll(struct Entry* entry, char* fullpath) {
    my_print('/',0);
    int i;
    for(i=0;fullpath[i]!=0;i++){
        my_print(fullpath[i], 0);
    }
    my_print(':',0);
    my_print('\n',0);
    struct Entry* ptr = entry->Children;
    while(ptr!=NULL){
        int k;
        for(k=0;ptr->Name[k];k++){
            my_print(ptr->Name[k], ptr->Type);
        }
        my_print(' ', 0);
        ptr=ptr->Next;
    }
    my_print('\n', 0);
    ptr=entry->Children;
    while (ptr != NULL) {
        char* new_path = malloc(strlen(fullpath) + strlen(ptr->Name) + 2);
        strcpy(new_path, fullpath);
        if (*fullpath != 0) {
            strcat(new_path, "/");
        }
        if (ptr->Type == 1 ) {
            strcat(new_path, ptr->Name);
            printAll(ptr, new_path);
        }
        free(new_path);
        ptr = ptr->Next;
    }
}

void printFile(struct Entry* entry) {
    int clus = entry->FstClus;
    while (1) {
        int i;
        int base = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec + bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
        for (i = 0; i < bpb.BytsPerSec * bpb.SecPerClus; ++i) {
            my_print(image[i+base], 0);
        }
        if ((clus = nextClus(clus)) >= 0xff7) {
            break;
        }
    }
}

struct Entry* findFile(struct Entry* entry, char* path, char* target) {
    if (strcmp(path, target) == 0) {
        return entry;
    }
    struct Entry* ptr = entry->Children;
    while (ptr != NULL) {
        char* new_path = malloc(strlen(path) + strlen(ptr->Name) + 2);
        strcpy(new_path, path);
        strcat(new_path, ptr->Name);

        if (strcmp(new_path, target) == 0) {
            free(new_path);
            return ptr;
        }
        if (ptr->Type == 1) {
            strcat(new_path, "/");
            struct Entry* result = findFile(ptr, new_path, target);
            if (result != NULL) {
                free(new_path);
                return result;
            }
        }
        free(new_path);
        ptr = ptr->Next;
    }
    return NULL;
}
int getFiles(struct Entry* entry){
    int fileNum=0;
    struct Entry* ptr=entry->Children;
    while(ptr!=NULL){
        if(ptr->Type==0){
            fileNum++;
        }
        if(ptr->Type==1){
            fileNum=fileNum+getFiles(ptr);
        }
        ptr=ptr->Next;
    }
    return fileNum;
}

int getDirs(struct Entry* entry){
    int dirNum=0;
    struct Entry* ptr=entry->Children;
    while(ptr!=NULL){
        if(ptr->Type==1){
            dirNum++;
            dirNum=dirNum+getDirs(ptr);
        }
        ptr=ptr->Next;
    }
    return dirNum;
}

//n为缩进数,为显示包含关系
void printCount(struct Entry* entry, int n){
    int fileNum=getFiles(entry);
    int dirNum=getDirs(entry);
    char file[10];
    char dir[10];

    int i;
    i = 0;
    if(fileNum==0){
        file[i]='0';
        i++;
    }

    while (fileNum) {
        char c = fileNum % 10 + '0';
        file[i] = c;
        i++;
        fileNum = fileNum / 10;
    }
    file[i]='\0';
    i = 0;
    if(dirNum==0){
        dir[i]='0';
        i++;
    }

    while (dirNum) {
        char c = dirNum % 10 + '0';
        dir[i] = c;
        i++;
        dirNum = dirNum / 10;
    }
    dir[i]='\0';
    for(i=0;i<n;i++){
        my_print(' ', 0);
    }
    for(i=0;entry->Name[i]!='\0';i++){
        my_print(entry->Name[i], 0);
    }
    my_print(':', 0);
    my_print(' ', 0);
    for(i=0;file[i]!='\0';i++){
        my_print(file[i], 0);
    }
    char fileNext[]=" file(s), ";
    for(i=0;fileNext[i]!='\0';i++){
        my_print(fileNext[i], 0);
    }
    for(i=0;dir[i]!='\0';i++){
        my_print(dir[i], 0);
    }
    char dirNext[]=" dir(s)";
    for(i=0;dirNext[i]!='\0';i++){
        my_print(dirNext[i], 0);
    }
    my_print('\n', 0);
    struct Entry* ptr=entry->Children;
    while (ptr!=NULL){
        if(ptr->Type==1){
            printCount(ptr, n+2);
        }
        ptr=ptr->Next;
    }
}

int main() {
    FILE *f;
    f = fopen("a.img", "rb");
    fread(image, IMGSIZE, 1, f);
    fclose(f);
    setBPB();
    root.Type = 1;
    createTree(&root);
    char command[1024];
    while (1) {

        fgets(command, 1024, stdin);
        lowerStr(command);
        int len = strlen(command);
        if (command[len - 1] == '\n') {
            command[--len] = 0;
        }
        if (command[len - 1] == '\r') {
            command[--len] = 0;
        }
        if (command[len - 1] == '/') {
            command[--len] = 0;
        }
        if(strcmp(command, "exit")==0){
            exit(0);
        }else if(command[0]=='l'&&command[1]=='s'&&command[3]==0){
            printAll(&root, "");
        }else if(strstr(command, "ls ")==command){
            char *path = command + 3;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=findFile(&root, "", path);
            if(entry==NULL){
                char error[]="This path does not exits!";
                int k;
                for(k=0;error[k]!='\0';k++){
                    my_print(error[k], 2);
                }
                my_print('\n', 0);
            }else if(entry->Type==0){
                char error[]="This is not a directory!";
                int k;
                for(k=0;error[k]!='\0';k++){
                    my_print(error[k], 2);
                }
                my_print('\n', 0);
            }else{
                printAll(entry, path);
            }

        }else if(strstr(command, "cat ")==command){
            char *path = command + 4;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=findFile(&root, "", path);
            if(entry==NULL||entry->Type!=0){
                char error[]="This is not an output file!";
                int k;
                for(k=0;error[k]!='\0';k++){
                    my_print(error[k], 2);
                }
                my_print('\n', 0);
            }else{
                printFile(entry);
            }


        }else if(strstr(command, "count ")==command){
            char *path = command + 6;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=findFile(&root, "", path);
            if(entry==NULL||entry->Type!=1){
                char error[]="This is not a directory!";
                int k;
                for(k=0;error[k]!='\0';k++){
                    my_print(error[k], 2);
                }
                my_print('\n', 0);
            }else{
                printCount(entry, 0);
            }


        }else{
            char error[]="This is not a valid operation!";
            int k;
            for(k=0;error[k]!='\0';k++){
                my_print(error[k], 2);
            }
            my_print('\n', 0);
        }
    }
    return 0;
}
