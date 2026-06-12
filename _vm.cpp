#include<stdio.h>
#include<ctype.h>
#include<stdlib.h>
#include<string.h>
#include<string>
#include<map>
#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
extern "C" __declspec(dllimport) int __stdcall SetConsoleCP(unsigned int);
#define CP_UTF8 65001
#endif
using namespace std;

struct Code {        
    char opt[20];
    int operand; // 现在只会有纯数字，不再有 LABEL 字符串了！
};

Code code[2000];     
map<string, int> choseOpt;     

enum opts {
    LOAD, LOADI, STO, ADD, SUB, MULT, DIV, BR, BRF, BRT, EQ, NOTEQ, GT, LES, GE, LE, AND,
    OR, NOT, IN, OUT, CAL, ENTER, RETURN, LOADG, STOG, LOADARR, STOARR, POP, DUP
};

void mapInit() {
    choseOpt["LOAD"] = LOAD;       choseOpt["LOADI"] = LOADI;     choseOpt["STO"] = STO;
    choseOpt["ADD"] = ADD;         choseOpt["SUB"] = SUB;         choseOpt["MULT"] = MULT;
    choseOpt["DIV"] = DIV;         choseOpt["BR"] = BR;           choseOpt["BRF"] = BRF;
    choseOpt["BRT"] = BRT;         choseOpt["EQ"] = EQ;           choseOpt["NOTEQ"] = NOTEQ;     
    choseOpt["GT"] = GT;           choseOpt["LES"] = LES;         choseOpt["GE"] = GE;       
    choseOpt["LE"] = LE;           choseOpt["AND"] = AND;         choseOpt["OR"] = OR;       
    choseOpt["NOT"] = NOT;         choseOpt["IN"] = IN;           choseOpt["OUT"] = OUT;     
    choseOpt["CAL"] = CAL;         choseOpt["ENTER"] = ENTER;     choseOpt["RETURN"] = RETURN;
    choseOpt["LOADG"] = LOADG;     choseOpt["STOG"] = STOG;       
    choseOpt["LOADARR"] = LOADARR; choseOpt["STOARR"] = STOARR;   choseOpt["POP"] = POP;
    choseOpt["DUP"] = DUP;
}

void TESTmachine() {
    FILE* in; char codein[100];       
    int codenum = 0;          
    int top = 0, base = 0, ip = 0;               
    int stack[2000];           

    printf("-------------- 虚拟机 (栈式抽象机) -------------\n\n");
    printf("---请输入目标文件名 (请务必输入纯净指令文件 code.txt)：");
    scanf("%s", codein);
    
    if ((in = fopen(codein, "r")) == NULL) {
        printf("\n打开 %s 错误！请确保编译器已成功生成此文件。\n", codein); exit(-1);      
    }

    // 【超级简化】：因为编译器已经做好了地址回填，现在每一行都必定是 "指令 操作数"
    while (fscanf(in, "%s %d", code[codenum].opt, &code[codenum].operand) == 2) {
        codenum++;
    }
    fclose(in);

    printf("\n-----------读取到的纯净机器指令-------------\n");
    for (int i = 0; i < codenum; i++) {
        printf("%d:\t%s\t\t%d\n", i, code[i].opt, code[i].operand);
    }
        
    stack[0] = 0; stack[1] = 0;
    mapInit();     
    memset(stack, 0, sizeof(stack));
    base = 100; top = 100; 

    printf("\n-----------开始执行-------------\n");
    while (ip < codenum) {        
        Code temp = code[ip];  
        ip++;                  
        switch (choseOpt[temp.opt]) {    
        case LOAD:   { stack[top++] = stack[temp.operand + base]; break; }
        case STO:    { stack[temp.operand + base] = stack[--top]; break; }
        case LOADG:  { stack[top++] = stack[temp.operand]; break; }
        case STOG:   { stack[temp.operand] = stack[--top]; break; }
        case LOADARR:{ int idx = stack[top - 1]; stack[top - 1] = stack[base + temp.operand + idx]; break; }
        case STOARR: { int val = stack[top - 1], idx = stack[top - 2]; stack[base + temp.operand + idx] = val; top -= 2; break; }
        case LOADI:  { stack[top++] = temp.operand; break; }
        case POP:    { top--; break; } 
        case DUP:    { stack[top] = stack[top - 1]; top++; break; } 
        case ADD:    { stack[top - 2] += stack[top - 1]; top--; break; }
        case SUB:    { stack[top - 2] -= stack[top - 1]; top--; break; }
        case MULT:   { stack[top - 2] *= stack[top - 1]; top--; break; }
        case DIV:    { stack[top - 2] /= stack[top - 1]; top--; break; }
        case EQ:     { stack[top - 2] = (stack[top - 2] == stack[top - 1]); top--; break; }
        case NOTEQ:  { stack[top - 2] = (stack[top - 2] != stack[top - 1]); top--; break; }
        case GT:     { stack[top - 2] = (stack[top - 2] > stack[top - 1]); top--; break; }
        case LES:    { stack[top - 2] = (stack[top - 2] < stack[top - 1]); top--; break; }
        case GE:     { stack[top - 2] = (stack[top - 2] >= stack[top - 1]); top--; break; }
        case LE:     { stack[top - 2] = (stack[top - 2] <= stack[top - 1]); top--; break; }
        case AND:    { stack[top - 2] = (stack[top - 2] && stack[top - 1]); top--; break; }
        case OR:     { stack[top - 2] = (stack[top - 2] || stack[top - 1]); top--; break; }
        case NOT:    { stack[top - 1] = !stack[top - 1]; break; }
        case BR:     { ip = temp.operand; break; }
        case BRF:    { if (stack[top - 1] == 0) ip = temp.operand; top--; break; }
        case BRT:    { if (stack[top - 1] != 0) ip = temp.operand; top--; break; }
        case IN:     { printf("\n输入数据: "); scanf("%d", &stack[top++]); break; }
        case OUT:    { printf("输出：%d\n", stack[top - 1]); top--; break; }
        case CAL:    { stack[top++] = ip; ip = temp.operand; break; }
        case ENTER:  { stack[top] = base; stack[top + 1] = stack[top - 1]; base = top; top += 2 + temp.operand; break; }
        case RETURN: { 
            if (base == 100) { ip = codenum; break; } // 安全退出主函数
            int return_val = stack[top - 1]; 
            int old_base = base;   
            ip = stack[old_base + 1];  
            base = stack[old_base];    
            top = old_base;            
            stack[top++] = return_val; 
            break; 
        }
        }
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    TESTmachine();
    return 0;
}
