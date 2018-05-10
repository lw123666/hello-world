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
    char Name[9];//文件名
    char Ext[4];//扩展名
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

int get_int(int offset, int len)
{
    int result = 0, i;
    for (i = offset + len - 1; i >= offset; --i) {
        result = result * 256 + image[i];
    }
    return result;
}

int get_next_clus(int curr_clus) {
    int clus_id = curr_clus / 2,
            fat_value = get_int(bpb.RsvdSecCnt * bpb.BytsPerSec + clus_id * 3, 3);
    if (curr_clus % 2 == 0) {
        return fat_value & 0x000fff;
    } else {
        return (fat_value & 0xfff000) / 0x1000;
    }
}


void setBPB()
{
    bpb.BytsPerSec = get_int(11, 2);
    bpb.SecPerClus = get_int(13, 1);
    bpb.RsvdSecCnt = get_int(14, 2);
    bpb.NumFATs = get_int(16, 1);
    bpb.RootEntCnt = get_int(17, 2);
    bpb.FATSz16 = get_int(22, 2);
}

void get_dir(struct Entry* entry) {
    int clus = entry->FstClus, is_root = 1, i,
            offset = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec;
    if (clus >= 2) {
        // 数据
        is_root = 0;
        offset += bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
    }
    for (i = offset; ; i += 32) {
        // 完成与否
        if (is_root == 1 && i >= offset + bpb.RootEntCnt * 32) {
            break;
        } else if (is_root == 0 && i >= offset + bpb.BytsPerSec * bpb.SecPerClus) {
            if ((clus = get_next_clus(clus)) >= 0xff7) {
                break;
            } else {
                i = offset = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec
                             + bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
            }
        }
        // 过滤
        if (image[i] != '.' && image[i] != 0 && image[i] != 5 && image[i] != 0xE5
            && image[i + 0xB] != 0xF && (image[i + 0xB] & 0x2) == 0) {
            struct Entry* new_entry = malloc(sizeof(struct Entry));
            // entry name
            int j;
            for (j = i; j < i + 8 && image[j] != 0x20; j++) {
                new_entry->Name[j-i] = image[j];
            }
            new_entry->Name[j-i] = 0;
            lowerStr(new_entry->Name);
            // file or folder
            if ((image[i + 0xB] & 0x10) == 0) {
                // file
                new_entry->Type = 0;
                for (j = i + 8; j < i + 0xB && image[j] != 0x20; j++) {
                    new_entry->Ext[j - i - 8] = image[j];
                }
                new_entry->Ext[j - i - 8] = 0;
               lowerStr(new_entry->Ext);
            } else {

                new_entry->Type = 1;
            }
            new_entry->FstClus = get_int(i + 26, 2);
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
                get_dir(new_entry);
            }
        }
    }
}

void print_dir(struct Entry* entry, char* fullpath) {
    my_print('/',0);
    int i;
    for(i=0;fullpath[i]!=0;i++){
        my_print(fullpath[i], 0);
    }
    my_print(':',0);
    my_print('\n',0);
    struct Entry* ptr = entry->Children;
    while(ptr!=NULL){
        if(ptr->Type==0){
            int k;
            for(k=0;ptr->Name[k];k++){
                my_print(ptr->Name[k], 0);
            }
            my_print('.', 0);
            for(k=0;ptr->Ext[k];k++){
                my_print(ptr->Ext[k], 0);
            }
            my_print(' ', 0);
        }else{
            int k;
            for(k=0;ptr->Name[k];k++){
                my_print(ptr->Name[k], 1);
            }
            my_print(' ', 0);
        }
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
            print_dir(ptr, new_path);
        }
        free(new_path);
        ptr = ptr->Next;
    }
}

void print_file(struct Entry* entry) {
    int clus = entry->FstClus;
    while (1) {
        int i, offset = (bpb.RsvdSecCnt + bpb.FATSz16 * bpb.NumFATs) * bpb.BytsPerSec
                        + bpb.RootEntCnt * 32 + (clus - 2) * bpb.BytsPerSec * bpb.SecPerClus;
        for (i = 0; i < bpb.BytsPerSec * bpb.SecPerClus; ++i) {
            my_print(image[i+offset], 0);
        }
        if ((clus = get_next_clus(clus)) >= 0xff7) {
            break;
        }
    }
}

struct Entry* find_file(struct Entry* entry, char* path, char* target) {
    if (strcmp(path, target) == 0) {
        return entry;
    }
    struct Entry* ptr = entry->Children;
    while (ptr != NULL) {
        char* new_path = malloc(strlen(path) + strlen(ptr->Name) + strlen(ptr->Ext) + 2);
        strcpy(new_path, path);
        strcat(new_path, ptr->Name);
        if (ptr->Type == 0) {
            strcat(new_path, ".");
            strcat(new_path, ptr->Ext);
        }
        if (strcmp(new_path, target) == 0) {
            free(new_path);
            return ptr;
        }
        if (ptr->Type == 1) {
            strcat(new_path, "/");
            struct Entry* result = find_file(ptr, new_path, target);
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
    get_dir(&root);
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
            print_dir(&root, "");
        }else if(strstr(command, "ls ")==command){
            char *path = command + 3;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=find_file(&root, "", path);
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
                print_dir(entry, path);
            }

        }else if(strstr(command, "cat ")==command){
            char *path = command + 4;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=find_file(&root, "", path);
            if(entry==NULL||entry->Type!=0){
                char error[]="This is not an output file!";
                int k;
                for(k=0;error[k]!='\0';k++){
                    my_print(error[k], 2);
                }
                my_print('\n', 0);
            }else{
                print_file(entry);
            }


        }else if(strstr(command, "count ")==command){
            char *path = command + 6;
            if (*path == '/') {
                path++;
            }
            struct Entry* entry=find_file(&root, "", path);
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
