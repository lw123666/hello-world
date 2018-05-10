#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef unsigned char u1;//一个字节
typedef unsigned short u2;//两个字节
typedef unsigned int u4;//四个字节

const u2 BytsPerSec=0x200;//每扇区字节数
const u1 SecPerClus=0x1;//每簇扇区数
const u1 RsvdSecCnt=0x1;//Boot记录占用的扇区数
const u1 NumFATs=0x2;//FAT表个数
const u2 RootEntCnt=0xE0;//根目录最大文件数
const u2 FATSz=0x9;//FAT扇区

#pragma pack (1) /*1字节对其*/

typedef struct RootEntry{
    char DIR_Name[11];//文件名加扩展名
    u1 DIR_Attr;//文件属性
    char reserved[10];
    u2 DIR_WrtTime;
    u2 DIR_WrtDate;
    u2 DIR_FstClus;//开始簇号
    u4 DIR_FileSize;//文件大小
}RootEntry;

typedef struct Node{
    char realName[13];
    u1 DIR_Attr;
    u2 DIR_FstClus;
    struct Node* son[128];
}Node;
#pragma pack ()

void createTree(FILE* file, Node* root);
void getChild(FILE* file, Node* temp, int startClus);
int getFAT(FILE* file, int num);//获得某一项的FAT值
void printAll(Node* root, char* path);
void printPath(Node* root, char* path);
void printFile(Node* root, char* path, FILE* file);
void count(Node* root, char* path);
void countPrint(Node* root, int n);//n代表缩进
int getFiles(Node* root);
int getDirs(Node* root);
extern void print(char c, int flag);//一个字节一个字节地输出,flag是颜色标志

int main(){
    
    FILE* fat12;
    fat12=fopen("aa.img", "rb");

    Node r;
    Node* root=&r;

    createTree(fat12, root);
    printf("Please enter your orders:\n");
    char input[128];
    
    //当输入exit时退出循环退出程序

    
    while(1){
        gets(input);
        if((input[0]=='l')&&(input[1]=='s')&&(input[2]=='\0')){
            char path[128];
            path[0]='/';
            path[1]=':';
            path[2]='\0';
            printAll(root, path);
        }else if((input[0]=='l')&&(input[1]=='s')&&(input[2]==' ')){
            char path[128];
            int i;
            for(i=0;i<128;i++){
                if(input[i+3]=='\0'){
                    path[i]='\0';
                    break;
                }else{
                    path[i]=input[i+3];
                }
            }
            printPath(root, path);
        }else if((input[0]=='c')&&(input[1]=='a')&&(input[2]=='t')&&(input[3]==' ')){
            char path[128];
            int i;
            for(i=0;i<128;i++){
                if(input[i+4]=='\0'){
                    path[i]='\0';
                    break;
                }else{
                    path[i]=input[i+4];
                }
            }
            printFile(root, path, fat12);
        }else if((input[0]=='c')&&(input[1]=='o')&&(input[2]=='u')&&(input[3]=='n')&&(input[4]=='t')&&(input[5]==' ')){
            char path[128];
            int i;
            for(i=0;i<128;i++){
                if(input[i+6]=='\0'){
                    path[i]='\0';
                    break;
                }else{
                    path[i]=input[i+6];
                }
            }
            count(root, path);
        }else if((input[0]=='e')&&(input[1]=='x')&&(input[2]=='i')&&(input[3]=='t')&&(input[4]=='\0')){
            break;
        }else{
            char out[]="This is not a valid operation!";
            int i;
            for(i=0;i<128;i++){
                if(out[i]=='\0'){
                    print('\n', 2);
                    break;
                }else{
                    print(out[i], 2);
                }
            }
        }
    }

    exit(0);

}

/*创建文件树*/
void createTree(FILE* file, Node* root){
    int base = (RsvdSecCnt + NumFATs * FATSz) * BytsPerSec;//根目录偏移
    RootEntry t;
    RootEntry* temp=&t;
    Node n;
    Node* node=&n;
    int i=0;
    
    
 for(i = 0;i<RootEntCnt;i++){
       
        fseek(file, base, SEEK_SET);
        
        fread(temp, 1, 32, file);
        

        node->DIR_Attr=temp->DIR_Attr;
        node->DIR_FstClus=temp->DIR_FstClus;
        base = base + 32;
        
        //空条目不做处理
        if(temp->DIR_Name[0] == '\0')
            continue;
        
        //过滤非目标文件,即文件名包含非数字,非空格,非字母
        int j=0;
        bool target = true;
        for(j=0;j<11;j++){
            if(!(((temp->DIR_Name[j]>=48)&&(temp->DIR_Name[j]<=57))||
                 ((temp->DIR_Name[j]>=65)&&(temp->DIR_Name[j]<=90))||
                 ((temp->DIR_Name[j]>=97)&&(temp->DIR_Name[j]<=122))||(temp->DIR_Name[j]==' '))){
                target= false;
                break;
            }
        }

        if(!target)
            continue;
        
        int k = 0;
        if(temp->DIR_Attr!=0x10){
            //文件
            int len=-1;
            bool blank=false;//有空格和无空格处理方式不同
            for(k=0;k<11;k++){
                if(temp->DIR_Name[k]!=' '){
                    len++;
                    node->realName[len]=temp->DIR_Name[k];
                }else{
                    blank= true;
                    len++;
                    node->realName[len]='.';
                    while(temp->DIR_Name[k]==' ') k++;
                    k--;
                }
            }
            if(blank){
                len++;
                node->realName[len]='\0';
            }else{
                node->realName[8]='.';
                for(k=8;k<11;k++){
                    node->realName[k+1]=temp->DIR_Name[k];
                }
                node->realName[12]='\0';
            }
            root->son[i]=node;
        }else{
            //目录
            int len=-1;
            for(k=0;k<11;k++){
                if(temp->DIR_Name[k]==' '){
                    len++;
                    node->realName[len]='\0';
                    break;
                }else{
                    len++;
                    node->realName[len]=temp->DIR_Name[k];
                }
            }
            root->son[i]=node;
            
            getChild(file, node, node->DIR_FstClus);
        }
        
    }

}

void getChild(FILE* file, Node* temp, int startClus){
    int base=BytsPerSec*(RsvdSecCnt+FATSz*NumFATs+(RootEntCnt*32+BytsPerSec-1)/BytsPerSec);
    int currentClus=startClus;
    int value=0;
    while(value<0xff8){
        value=getFAT(file, currentClus);
        if(value==0xff7){
            break;//坏簇，跳出
        }
        char* data=(char*)malloc(BytsPerSec*SecPerClus);//用于暂存该簇的所有数据
        char* content=data;

        int start=base+(currentClus-2)*SecPerClus*BytsPerSec;
        
        fseek(file, start, SEEK_SET);
       
        fread(content, 1, BytsPerSec*SecPerClus, file);
        
        //处理该簇的数据，与根目录区的条目格式相同
        int count=SecPerClus*BytsPerSec;
        int loop=0;
        while(loop<count){
            int i=0;
            //空条目不处理
            if(content[loop]=='\0'){
                loop=loop+32;
                continue;
            }

            int j;
            bool target=true;
            for(j=loop;j<loop+11;j++){
                if(!(((content[j]>=48)&&(content[j]<=57))||
                     ((content[j]>=65)&&(content[j]<=90))||
                     ((content[j]>=97)&&(content[j]<=122))||(content[j]==' '))){
                    target= false;
                    break;
                }
            }
            if(!target){
                loop=loop+32;
                continue;
            }
            RootEntry c;
            RootEntry* child=&c;
            Node n;
            Node* node=&n;
            int b=start+loop;
            fseek(file, b, SEEK_SET);
            fread(child, 1, 32, file);

            node->DIR_Attr=child->DIR_Attr;
            node->DIR_FstClus=child->DIR_FstClus;
            
            int k=0;
            if(child->DIR_Attr!=0x10){
                //文件
                int len=-1;
                bool blank=false;//有空格和无空格处理方式不同,默认文件格式为txt
                for(k=0;k<11;k++){
                    if(child->DIR_Name[k]!=' '){
                        len++;
                        node->realName[len]=child->DIR_Name[k];
                    }else{
                        blank= true;
                        len++;
                        node->realName[len]='.';
                        while(child->DIR_Name[k]==' ') k++;
                        k--;
                    }
                }
                if(blank){
                    len++;
                    node->realName[len]='\0';
                }else{
                    node->realName[8]='.';
                    for(k=8;k<11;k++){
                        node->realName[k+1]=child->DIR_Name[k];
                    }
                    node->realName[13]='\0';
                }
                int c=loop/32;
                
                temp->son[c]=node;
                
            }else{
                //目录
                int len=-1;
                for(k=0;k<11;k++){
                    if(child->DIR_Name[k]==' '){
                        len++;
                        node->realName[len]='\0';
                        break;
                    }else{
                        len++;
                        node->realName[len]=child->DIR_Name[k];
                    }
                }
                int c=loop/32;
                temp->son[c]=node;
                
                getChild(file, node, node->DIR_FstClus);
            }
            loop=loop+32;
        }
        
        free(data);
        currentClus=value;
    }
}


/*读取num号FAT项所在的连续两个字节，从中获得FAT值*/
int getFAT(FILE* file, int num){
    int fat1 = RsvdSecCnt * BytsPerSec;//fat1表的偏移
    int fatPos = fat1 + num*3/2;//该fat项的偏移
    int type = 0;
    if(num%2 == 0){
        type = 0;
    }else{
        type = 1;
    }
    //读取该fat项所处的两个字节
    u2 bytes;
    u2* b_ptr = &bytes;
    fseek(file, fatPos, SEEK_SET);
    fread(b_ptr, 1, 2, file);
    
    //如果是偶数号，取byte1和byte2的低四位;如果是奇数号，取byte2和byte1的高四位
    if(type == 0){
        return (bytes<<4)>>8;
    }else{
        return bytes>>4;
    }


}

void printAll(Node* root, char* path){
    int i=0;
    for(i=0;i<128;i++){
        if(path[i]=='\0'){
            print('\n',0);
            break;
        }else{
            print(path[i],0);
        }
    }
    printf(
    if(root->son[0]==NULL){
        print('\n', 0);
        return;
    }else{
        for(i=0;i<128;i++){
            if(root->son[i]==NULL){
                print('\n', 0);
                break;
            }else{
                int k;
                int flag=0;//0是文件，1是目录
                /*Node t;
                Node* temp=&t;
                temp=root->son[i];
                if(temp->DIR_Attr!=0x10){
                   flag=0;
                }else{
                   flag=1;
                }*/
               /* for(k=0;k<13;k++){
                    if(temp->realName[k]=='\0'){
                        print(' ', flag);
                        break;
                    }else{
                        print(temp->realName[k], flag);
                    }
                }*/
            }
        }
      /*  for(i=0;i<128;i++){
            if(root->son[i]==NULL){
                break;
            }else if(root->son[i]->DIR_Attr!=0x10){
                continue;
            }else{
                Node* father=root->son[i];
                int j=0;
                for(j=0;j<128;j++){
                    if(path[j]==':'){
                        int m;
                        for(m=0;m<13;m++){
                            if(father->realName[m]=='\0'){
                                path[j+m]='/';
                                path[j+m+1]=':';
                                path[j+m+2]='\0';
                                break;
                            }else {
                                path[j + m] = father->realName[m];
                            }
                        }
                        break;
                    }
                }
                printAll(father, path);
            }
        }*/
    }
}

void printPath(Node* root, char* path){
    int i=0;
    bool exist=false;
    Node* temp=root;
    while(path[i]!='\0'){
        int k=0;
        char name[13];
        for(k=i+1;k<128;k++){
            if(path[k]=='/'||path[k]=='\0'){
                name[k-i-1]='\0';
                i=k;
                break;
            }else{
                name[k-i-1]=path[k];
            }
        }
        int j;
        for(j=0;j<128;j++){
            if(temp->son[j]==NULL){
                exist = false;
                break;
            }else if(strcmp(name, temp->son[j]->realName)==0){
                exist=true;
                temp=temp->son[j];
                break;
            }else{

            }
        }
        if(!exist){
            break;
        }
    }
    if(!exist){
        char error[]="This path does not exist!";
        int n;
        for(n=0;n<128;n++){
            if(error[n]=='\0'){
                print('\n', 2);
                break;
            }else{
                print(error[n], 2);
            }
        }
        return;
    }else{
        path[i]='/';
        path[i+1]=':';
        path[i+2]='\0';
        printAll(temp, path);
    }
}

void printFile(Node* root, char* path, FILE* file){
    int i=0;
    bool exist=false;
    Node* temp=root;
    while(path[i]!='\0'){
        int k=0;
        char name[13];
        for(k=i+1;k<128;k++){
            if(path[k]=='/'||path[k]=='\0'){
                name[k-i-1]='\0';
                i=k;
                break;
            }else{
                name[k-i-1]=path[k];
            }
        }
        int j;
        for(j=0;j<128;j++){
            if(temp->son[j]==NULL){
                exist = false;
                break;
            }else if(strcmp(name, temp->son[j]->realName)==0){
                exist=true;
                temp=temp->son[j];
                break;
            }else{

            }
        }
        if(!exist){
            break;
        }
    }
    if(!exist){
        char error[]="This path does not exist!";
        int n;
        for(n=0;n<128;n++){
            if(error[n]=='\0'){
                print('\n', 2);
                break;
            }else{
                print(error[n], 2);
            }
        }
        return;
    }
    char* data=(char*)malloc(10000);
    char* text;
    int startClus=temp->DIR_FstClus;
    int base=BytsPerSec*(RsvdSecCnt+FATSz*NumFATs+(RootEntCnt*32+BytsPerSec-1)/BytsPerSec);
    int currentClus=startClus;
    int value=0;
    while(value<0xff8){
        value=getFAT(file, currentClus);
        if(value==0xff7){
            break;//坏簇，跳出
        }
        char* data=(char*)malloc(BytsPerSec*SecPerClus);//用于暂存该簇的所有数据
        char* content=data;

        int start=base+(currentClus-2)*SecPerClus*BytsPerSec;
        fseek(file, start, SEEK_SET);
        fread(content, 1, BytsPerSec*SecPerClus, file);

        strcat(text, content);
        free(data);
        currentClus=value;
    }
    int l=0;
    for(l=0;l<10000;l++){
        if(text[l]=='\0'){
            print('\n', 0);
        }else{
            print(text[l], 0);
        }
    }
    free(data);
}

void count(Node* root, char* path){
    int i=0;
    bool exist=false;
    Node* temp=root;
    while(path[i]!='\0'){
        int k=0;
        char name[13];
        for(k=i+1;k<128;k++){
            if(path[k]=='/'||path[k]=='\0'){
                name[k-i-1]='\0';
                i=k;
                break;
            }else{
                name[k-i-1]=path[k];
            }
        }
        int j;
        for(j=0;j<128;j++){
            if(temp->son[j]==NULL){
                exist = false;
                break;
            }else if(strcmp(name, temp->son[j]->realName)==0){
                exist=true;
                temp=temp->son[j];
                break;
            }else{

            }
        }
        if(!exist){
            break;
        }
    }
    if(!exist){
        char error[]="This path does not exist!";
        int n;
        for(n=0;n<128;n++){
            if(error[n]=='\0'){
                print('\n', 2);
                break;
            }else{
                print(error[n], 2);
            }
        }
        return;
    }
    countPrint(temp, 0);

}

int getFiles(Node* root){
    int fileNum=0;
    int i=0;
    for(i=0;i<128;i++){
        Node* temp=root->son[i];
        if(temp==NULL){
            break;
        }else if(temp->DIR_Attr!=0x10){
            fileNum++;
        }else{
            fileNum=fileNum+getFiles(temp);
        }
    }
    return fileNum;
}

int getDirs(Node* root){
    int dirNum=0;
    int i=0;
    for(i=0;i<128;i++){
        Node* temp=root->son[i];
        if(temp==NULL){
            break;
        }else if(temp->DIR_Attr==0x10){
            dirNum++;
            dirNum=dirNum+getDirs(temp);
        }else{

        }
    }
    return dirNum;
}

void countPrint(Node* root, int n) {
    int i = 0;
    for (i = 0; i < n; i++) {
        print(' ', 0);
    }
    int fileNum = getFiles(root);
    int dirNum = getDirs(root);
    char file[10];
    char dir[10];
    for (i = 0; i < 10; i++) {
        file[i] = dir[i] = '#';
    }
    i = 0;
    while (fileNum) {
        char c = fileNum % 10 + '0';
        file[i] = c;
        i++;
        fileNum = fileNum / 10;
    }
    i = 0;
    while (dirNum) {
        char c = dirNum % 10 + '0';
        dir[i] = c;
        i++;
        dirNum = dirNum / 10;
    }
    for (i = 0; i < 13; i++) {
        if (root->realName[i] == '\0') {
            print(':', 0);
            print(' ', 0);
            break;
        } else {
            print(root->realName[i], 0);
        }
    }
    for (i = 0; i < 10; i++) {
        if (file[i] == '#') {
            print(' ', 0);
            print('f', 0);
            print('i', 0);
            print('l', 0);
            print('e', 0);
            print('(', 0);
            print('s', 0);
            print(')', 0);
            print(',', 0);
            print(' ', 0);
        } else {
            print(file[i], 0);
        }
    }
    for (i = 0; i < 10; i++) {
        if (dir[i] == '#') {
            print(' ', 0);
            print('d', 0);
            print('i', 0);
            print('r', 0);
            print('(', 0);
            print('s', 0);
            print(')', 0);
            print('\n', 0);
        } else {
            print(dir[i], 0);
        }
    }
    for (i = 0; i < 128; i++) {
        Node *child = root->son[i];
        if (child == NULL) {
            break;
        } else if (child->DIR_Attr == 0x10) {
            countPrint(child, n + 2);
        } else {

        }
    }
}
