#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
extern "C" __declspec(dllimport) int __stdcall SetConsoleCP(unsigned int);
#define CP_UTF8 65001
#endif
#define keywordSum 26
#define MAX 26             
#define RES_MAX 15         
#define MAXBUF 255         
#define maxvartablep 500   
using namespace std;

// ==========================================
// 1. 符号表与全局状态定义
// ==========================================
struct vartable { char name[20]; int address; int is_array; int size; int is_global; };
struct fun_table { char name[30]; int address; int can_num; } fun_table[maxvartablep];
struct vartable global_table[maxvartablep];
int global_num = 0, global_datap = 0; 
FILE* fin, * fout;
char token[40], token1[40];
int lineindex = 1, Plineindex = 1;
char scanin[50], scanout[50], parseout[50]; 
int error_flag = 0; 
int TESTparse(); int TESTscan(FILE *fpin, FILE *fpout); 
int ch = ' ';             
int Line_NO = 1, labelp = 1, datap = 0, fun_var = 0;
struct keywords { char lexptr[MAXBUF]; }; struct keywords symtable[MAX]; 
char str[MAX][RES_MAX] = { "int", "for", "if", "else", "do", "call", "while", "read", "write", "main", "switch", "case", "function", "return", "char", "break", "default" };


// 2. 目标代码生成与回填 (Backpatching) 引擎

struct Instruction {
    string opt;
    int operand;
    bool is_jump;
    bool has_operand;
};
vector<Instruction> code_buffer;
map<int, int> label_map;

// 压入带操作数的指令
void emit(string opt, int operand, bool is_jump = false) {
    code_buffer.push_back({opt, operand, is_jump, true});
}

// 压入无操作数指令
void emit_no_op(string opt) {
    code_buffer.push_back({opt, 0, false, false});
}

// 记录 LABEL 对应的实际指令行号
void mark_label(int label_id) {
    label_map[label_id] = code_buffer.size();
}

// 打印最终格式化的代码
void write_codes(FILE* out_file) {
    // 1. 供人阅读的带表头格式
    fprintf(out_file, "Index    OP Code      Operand\n");
    fprintf(out_file, "--------------------------------------------------------------\n");
    for (size_t i = 0; i < code_buffer.size(); i++) {
        // 地址回填：把 LABEL 替换成真实的指令索引
        if (code_buffer[i].is_jump) {
            code_buffer[i].operand = label_map[code_buffer[i].operand];
        }
        
        if (code_buffer[i].has_operand) {
            fprintf(out_file, "%-8d %-12s %d\n", (int)i, code_buffer[i].opt.c_str(), code_buffer[i].operand);
        } else {
            fprintf(out_file, "%-8d %-12s -\n", (int)i, code_buffer[i].opt.c_str());
        }
    }

    // 2. 供虚拟机执行的纯净代码 (写入 code.txt)
    FILE* vm_code = fopen("code.txt", "w");
    if (vm_code) {
        for (size_t i = 0; i < code_buffer.size(); i++) {
            fprintf(vm_code, "%s %d\n", code_buffer[i].opt.c_str(), code_buffer[i].operand);
        }
        fclose(vm_code);
    }
}

// ==========================================
// 3. 抽象语法树 (AST) 节点与导出引擎
// ==========================================
struct ASTNode {
    string type; int line; string name; string kind; string value; string op;
    bool is_mutable = true; ASTNode* left = nullptr; ASTNode* right = nullptr;
    ASTNode* condition = nullptr; ASTNode* body = nullptr; ASTNode* elseBody = nullptr;
    vector<ASTNode*> children;
};
struct node { string st; vector<node*> son; ASTNode* ast = nullptr; 
    node* addSon(string s) { node* Son = new node(); Son->st = s; son.push_back(Son); return Son; }
} *Root;
void out(node* p) { p->addSon(token1); }

void printIndent(int indent, FILE* f) { for(int i=0; i<indent; i++) fprintf(f, "  "); }
void printASTJSON(ASTNode* node, int indent, FILE* f) {
    if (!node) { fprintf(f, "null"); return; }
    printIndent(indent, f); fprintf(f, "{\n"); indent++;
    printIndent(indent, f); fprintf(f, "\"type\": \"%s\",\n", node->type.c_str());
    printIndent(indent, f); fprintf(f, "\"loc\": { \"line\": %d }", node->line);

    if (node->type == "Identifier") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"name\": \"%s\"", node->name.c_str());
        if(!node->kind.empty()) { fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"kind\": \"%s\"", node->kind.c_str()); }
    } else if (node->type == "BasicLit") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"kind\": \"%s\",\n", node->kind.c_str());
        printIndent(indent, f); fprintf(f, "\"value\": %s,\n", node->value.c_str());
        printIndent(indent, f); fprintf(f, "\"raw\": \"%s\"", node->value.c_str());
    } else if (node->type == "VarDecl") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"is_mutable\": %s,\n", node->is_mutable ? "true" : "false");
        printIndent(indent, f); fprintf(f, "\"identifier\": \n"); printASTJSON(node->left, indent, f);
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"typeExpr\": {\n");
        printIndent(indent+1, f); fprintf(f, "\"type\": \"TypeNode\",\n");
        printIndent(indent+1, f); fprintf(f, "\"name\": \"int\"\n"); printIndent(indent, f); fprintf(f, "}");
        if (node->right) { fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"value\": \n"); printASTJSON(node->right, indent, f); }
    } else if (node->type == "BinaryExpr" || node->type == "AssignExpr") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"op\": \"%s\",\n", node->op.c_str());
        printIndent(indent, f); fprintf(f, "\"left\": \n"); printASTJSON(node->left, indent, f); fprintf(f, ",\n");
        printIndent(indent, f); fprintf(f, "\"right\": \n"); printASTJSON(node->right, indent, f);
    } else if (node->type == "FuncDecl") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"name\": \"%s\",\n", node->name.c_str());
        printIndent(indent, f); fprintf(f, "\"params\": [\n");
        for (size_t i = 0; i < node->children.size(); i++) {
            printASTJSON(node->children[i], indent+1, f);
            if (i < node->children.size() - 1) fprintf(f, ",\n"); else fprintf(f, "\n");
        }
        printIndent(indent, f); fprintf(f, "],\n"); printIndent(indent, f); fprintf(f, "\"body\": \n");
        printASTJSON(node->body, indent, f);
    } else if (node->type == "Program" || node->type == "BlockStmt" || node->type == "VarDeclList") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"body\": [\n");
        for (size_t i = 0; i < node->children.size(); i++) {
            if (node->children[i] == NULL) continue;
            printASTJSON(node->children[i], indent+1, f);
            if (i < node->children.size() - 1) fprintf(f, ",\n"); else fprintf(f, "\n");
        }
        printIndent(indent, f); fprintf(f, "]");
    } else if (node->type == "ReadStmt" || node->type == "WriteStmt" || node->type == "ReturnStmt") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"expression\": \n"); printASTJSON(node->left, indent, f);
    } else if (node->type == "IfStmt") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"condition\": \n"); printASTJSON(node->condition, indent, f); fprintf(f, ",\n");
        printIndent(indent, f); fprintf(f, "\"body\": \n"); printASTJSON(node->body, indent, f);
        if (node->elseBody) { fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"elseBody\": \n"); printASTJSON(node->elseBody, indent, f); }
    } else if (node->type == "WhileStmt" || node->type == "DoWhileStmt") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"condition\": \n"); printASTJSON(node->condition, indent, f); fprintf(f, ",\n");
        printIndent(indent, f); fprintf(f, "\"body\": \n"); printASTJSON(node->body, indent, f);
    } else if (node->type == "CallExpr") {
        fprintf(f, ",\n"); printIndent(indent, f); fprintf(f, "\"name\": \"%s\",\n", node->name.c_str());
        printIndent(indent, f); fprintf(f, "\"arguments\": [\n");
        for (size_t i = 0; i < node->children.size(); i++) {
            printASTJSON(node->children[i], indent+1, f);
            if (i < node->children.size() - 1) fprintf(f, ",\n"); else fprintf(f, "\n");
        }
        printIndent(indent, f); fprintf(f, "]");
    }
    fprintf(f, "\n"); indent--; printIndent(indent, f); fprintf(f, "}");
}

// ---------程序声明----------- //
void init(); int program(); int declaration_list(node* fa, struct vartable table[], int &num);
int declaration_stat(node* fa, struct vartable table[], int &num, int is_global);
int statement_list(node* fa, struct vartable table[], int &num); int statement(node* fa, struct vartable table[], int &num);
int if_stat(node* fa, struct vartable table[], int &num); int while_stat(node* fa, struct vartable table[], int &num);
int dowhile_stat(node* fa, struct vartable table[], int &num); int for_stat(node* fa, struct vartable table[], int &num);
int switch_stat(node* fa, struct vartable table[], int &num); int read_stat(node* fa, struct vartable table[], int &num);
int write_stat(node* fa, struct vartable table[], int &num); int return_stat(node* fa, struct vartable table[], int &num);
int compound_stat(node* fa, struct vartable table[], int &num); int expression_stat(node* fa, struct vartable table[], int &num);
int expression(node* fa, struct vartable table[], int &num); int bool_expr(node* fa, struct vartable table[], int &num);
int additive_expr(node* fa, struct vartable table[], int &num); int term(node* fa, struct vartable table[], int &num);
int factor(node* fa, struct vartable table[], int &num); void fun_declaration(node* fa);
int canshu(struct vartable table[], int &num, int &can_num, ASTNode* func_ast); 
void fun_body(node* fa, struct vartable table[], int &num);
void main_declaration(node* fa); int call_stat(node* fa, struct vartable table[], int &num);
int name_def(char *name, struct vartable table[], int &num, int is_array, int size, int is_global);
int lookup(struct vartable local_table[], int local_num, char *name, int *paddress, int *is_global, int *is_array);
int fun_lookup(char *name, int *paddress, int num);

// 主函数 
int main(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    printf("----------------分析程序----------------\n"); 
    printf("请输入词法分析源程序文件名:\n"); scanf("%s", scanin);
    printf("请输入词法及语义分析输出文件名:\n"); scanf("%s", scanout);
    printf("请输入语法及语义分析输出文件名:\n"); scanf("%s", parseout);
    
    int es = 0; fin = fopen(scanin, "r"); fout = fopen(scanout, "w");
    init(); es = TESTscan(fin, fout); 
    fclose(fin); fclose(fout);

    if (es > 0) { printf("词法分析失败!\n"); return 0; }
    else printf("词法分析成功!\n");
    
    error_flag = 0;
    fout = fopen(parseout, "w");
    es = TESTparse(); 
    
    FILE* tree_fout = fopen("out_tree.json", "w"); 
    if (tree_fout) { printASTJSON(Root->ast, 0, tree_fout); fclose(tree_fout); }

    write_codes(fout); // 执行格式化输出和回填

    if (error_flag == 0 && es == 0) {
        printf("语法语义分析成功!!!\n");
        printf(" -> 美化中间代码已保存至: %s\n", parseout);
        printf(" -> 纯净虚拟机代码保存至: code.txt (供 vm.exe 直接执行)\n");
        printf(" -> 完美JSON抽象语法树已保存至: out_tree.json\n");
    } else {
        printf("\n? 发现了语法错误！(已启用错误恢复，跳过错误代码并生成了部分语法树)\n");
    }
    if(fin) fclose(fin); if(fout) fclose(fout);
    return 0;
}

void init() { for(int j=0; j<MAX; j++) strcpy(symtable[j].lexptr, str[j]); }
int Iskeyword(char * is_res) { for(int i=0; i<MAX; i++) if(strcmp(symtable[i].lexptr, is_res) == 0) return 1; return 0; } 
int IsDigit(char c) { return (c >= '0' && c <= '9'); }
int IsLetter(char c) { return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A') || (c == '_')); }

void lex_error(FILE *fpout, int line, const char *msg, const char *lexeme) {
    printf("\n>>> [词法错误] 第 %d 行: %s '%s' <<<\n", line, msg, lexeme);
    if (fpout) fprintf(fpout, "LEX_ERROR\t%s\t%d\t%s\n", lexeme, line, msg);
}

int TESTscan(FILE *fpin, FILE *fpout){
    char arr[MAXBUF], arr1[MAXBUF];
    int es = 0;
    Line_NO = 1;

    while((ch = fgetc(fpin)) != EOF){
        if(ch == ' ' || ch == '\t' || ch == '\r') continue;
        if(ch == '\n') { Line_NO++; continue; }

        if(IsLetter((char)ch)){
            int j = 0, j1 = 0;
            do {
                if(j < MAXBUF - 1) {
                    arr1[j1++] = (char)ch;
                    arr[j++] = (char)((ch >= 'A' && ch <= 'Z') ? ch + 32 : ch);
                }
                ch = fgetc(fpin);
            } while(ch != EOF && (IsLetter((char)ch) || IsDigit((char)ch)));
            if(ch != EOF) ungetc(ch, fpin);
            arr[j] = '\0'; arr1[j1] = '\0';
            if (Iskeyword(arr)) fprintf(fpout, "%s\t%s\t%d\n", arr, arr1, Line_NO);
            else fprintf(fpout, "ID\t%s\t%d\n", arr1, Line_NO);
            continue;
        }

        if(IsDigit((char)ch)){
            int j = 0, has_letter = 0;
            do {
                if(j < MAXBUF - 1) arr[j++] = (char)ch;
                if(IsLetter((char)ch)) has_letter = 1;
                ch = fgetc(fpin);
            } while(ch != EOF && (IsDigit((char)ch) || IsLetter((char)ch)));
            if(ch != EOF) ungetc(ch, fpin);
            arr[j] = '\0';
            if(!has_letter) fprintf(fpout, "NUM\t%s\t%d\n", arr, Line_NO);
            else { lex_error(fpout, Line_NO, "非法标识符，标识符不能以数字开头:", arr); es = 1; }
            continue;
        }

        switch(ch){
            case '+': ch=fgetc(fpin); if(ch=='+') fprintf(fpout,"++\t++\t%d\n",Line_NO); else if(ch=='=') fprintf(fpout,"+=\t+=\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,"+\t+\t%d\n",Line_NO); } break;
            case '-': ch=fgetc(fpin); if(ch=='-') fprintf(fpout,"--\t--\t%d\n",Line_NO); else if(ch=='=') fprintf(fpout,"-=\t-=\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,"-\t-\t%d\n",Line_NO); } break;
            case '>': ch=fgetc(fpin); if(ch=='=') fprintf(fpout,">=\t>=\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,">\t>\t%d\n",Line_NO); } break;
            case '<': ch=fgetc(fpin); if(ch=='=') fprintf(fpout,"<=\t<=\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,"<\t<\t%d\n",Line_NO); } break;
            case '!': ch=fgetc(fpin); if(ch=='=') fprintf(fpout,"!=\t!=\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,"!\t!\t%d\n",Line_NO); } break;
            case '=': ch=fgetc(fpin); if(ch=='=') fprintf(fpout,"==\t==\t%d\n",Line_NO); else { if(ch!=EOF) ungetc(ch,fpin); fprintf(fpout,"=\t=\t%d\n",Line_NO); } break;
            case '*': fprintf(fpout,"*\t*\t%d\n",Line_NO); break;
            case '/':
                ch = fgetc(fpin);
                if(ch == '*') {
                    int start_line = Line_NO, prev = 0, curr;
                    while((curr = fgetc(fpin)) != EOF) {
                        if(curr == '\n') Line_NO++;
                        if(prev == '*' && curr == '/') break;
                        prev = curr;
                    }
                    if(curr == EOF) { lex_error(fpout, start_line, "多行注释未闭合:", "/*"); es = 1; }
                } else {
                    if(ch != EOF) ungetc(ch, fpin);
                    fprintf(fpout,"/\t/\t%d\n",Line_NO);
                }
                break;
            case '(': fprintf(fpout,"(\t(\t%d\n",Line_NO); break; case ')': fprintf(fpout,")\t)\t%d\n",Line_NO); break;
            case '[': fprintf(fpout,"[\t[\t%d\n",Line_NO); break; case ']': fprintf(fpout,"]\t]\t%d\n",Line_NO); break;
            case ';': fprintf(fpout,";\t;\t%d\n",Line_NO); break; case ',': fprintf(fpout,",\t,\t%d\n",Line_NO); break;
            case ':': fprintf(fpout,":\t:\t%d\n",Line_NO); break; case '{': fprintf(fpout,"{\t{\t%d\n",Line_NO); break;
            case '}': fprintf(fpout,"}\t}\t%d\n",Line_NO); break;
            default: { char bad[2] = { (char)ch, '\0' }; lex_error(fpout, Line_NO, "无法识别的字符:", bad); es = 1; break; }
        }
    }
    return es;
}

int name_def(char *name, struct vartable table[], int &num, int is_array, int size, int is_global){
    for(int i = 0; i < num; i++) if(strcmp(table[i].name, name) == 0) return 22; 
    strcpy(table[num].name, name); table[num].is_array = is_array; table[num].size = size; table[num].is_global = is_global;
    if(is_global) { table[num].address = global_datap; global_datap += size; } 
    else { table[num].address = datap; datap += size; }
    num++; return 0;
}
int fun_def(char *name, int address, int can_num){
    for(int i=0; i<fun_var; i++) if(strcmp(fun_table[i].name, name) == 0) return 32;
    strcpy(fun_table[fun_var].name, name); fun_table[fun_var].address = address; fun_table[fun_var].can_num = can_num; fun_var++; return 0;
}
int lookup(struct vartable local_table[], int local_num, char *name, int *paddress, int *is_global, int *is_array){
    for(int i = 0; i < local_num; i++){ if(strcmp(local_table[i].name, name) == 0){ *paddress = local_table[i].address; *is_global = 0; *is_array = local_table[i].is_array; return 0; } }
    for(int i = 0; i < global_num; i++){ if(strcmp(global_table[i].name, name) == 0){ *paddress = global_table[i].address; *is_global = 1; *is_array = global_table[i].is_array; return 0; } }
    return 12; 
} 
int fun_lookup(char *name, int *paddress, int num){
    for(int i=0; i<fun_var; i++) if(strcmp(fun_table[i].name, name) == 0 && fun_table[i].can_num == num){ *paddress = fun_table[i].address; return 0; }
    return 33; 
}

int TESTparse() {
    if ((fin = fopen(scanout, "r")) == NULL) return 10;
    return program();
}

void check_match(const char* expected, node* p, const char* err_msg) {
    if (strcmp(token, expected) == 0) {
        if (p) out(p);
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    } else {
        printf("\n>>> [语法错误] 第 %d 行: 期望 '%s', 但得到 '%s' (%s)<<<\n", lineindex, expected, token, err_msg);
        error_flag = 1;
        if (strcmp(token, "EOF") != 0 && strcmp(token, "#") != 0) {
            if (!(strcmp(expected, ")") == 0 && strcmp(token, "{") == 0)) {
                fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            }
        }
    }
}

int program() {
    Root = new node(); Root->st = "<programe>";
    ASTNode* ast = new ASTNode(); ast->type = "Program"; ast->line = lineindex;
    
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    emit("BR", 0, true); 
    
    while (strcmp(token, "int") == 0) {
        long fileadd = ftell(fin); char next_tok[40], next_tok1[40]; int next_line;
        fscanf(fin, "%s\t%s\t%d\n", next_tok, next_tok1, &next_line);
        if(strcmp(next_tok, "function") == 0 || strcmp(next_tok, "main") == 0) { fseek(fin, fileadd, SEEK_SET); break; } 
        else {
            fseek(fin, fileadd, SEEK_SET); declaration_stat(Root, global_table, global_num, 1);
            if (Root->son.back()->ast) {
                if (Root->son.back()->ast->type == "VarDeclList") {
                    for (auto c : Root->son.back()->ast->children) ast->children.push_back(c);
                } else ast->children.push_back(Root->son.back()->ast);
            }
        }
    }

    while (strcmp(token, "function") == 0) {
        fun_declaration(Root);
        if (Root->son.back()->ast) ast->children.push_back(Root->son.back()->ast);
    }
    if (strcmp(token, "main") != 0) { printf(">>> [语法错误] 第 %d 行: 缺少 main 函数 <<<\n", lineindex); error_flag = 1; return 9; }
    main_declaration(Root);
    if (Root->son.back()->ast) ast->children.push_back(Root->son.back()->ast);
    Root->ast = ast;
    return 0;
}

void fun_declaration(node* fa) {
    node* p = fa->addSon("<fun_declaration>"); out(p);
    ASTNode* ast = new ASTNode(); ast->type = "FuncDecl"; ast->line = lineindex;
    int num = 0, can_num = 0; struct vartable local_table[maxvartablep]; char fun_name[20];
    
    int func_address = labelp++; mark_label(func_address);
    emit("ENTER", 20); 

    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    if (strcmp(token, "ID") == 0) { strcpy(fun_name, token1); ast->name = token1; out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); }
    
    check_match("(", p, "函数参数缺失左括号");
    if(strcmp(token, "int") == 0) canshu(local_table, num, can_num, ast);
    fun_def(fun_name, func_address, can_num);
    check_match(")", p, "函数参数缺失右括号");
    
    datap = 2; fun_body(p, local_table, num);
    ast->body = p->son.back()->ast; 
    
    mark_label(labelp++); emit_no_op("RETURN");
    p->ast = ast;
}

void main_declaration(node* fa) {
    node* p = fa->addSon("<main_declaration>"); out(p);
    ASTNode* ast = new ASTNode(); ast->type = "FuncDecl"; ast->name = "main"; ast->line = lineindex;
    int num = 0; struct vartable local_table[maxvartablep];
    
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    check_match("(", p, "main函数缺失左括号"); check_match(")", p, "main函数缺失右括号");
    
    mark_label(0); emit("ENTER", 20); 
    datap = 2; fun_body(p, local_table, num);
    ast->body = p->son.back()->ast;
    emit_no_op("RETURN"); 
    p->ast = ast;
}

int canshu(struct vartable table[], int &num, int &can_num, ASTNode* func_ast){
    int start_num = num; 
    while(strcmp(token, "int") == 0){
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        if(strcmp(token, "ID") == 0) { 
            name_def(token1, table, num, 0, 1, 0); can_num++; 
            ASTNode* param = new ASTNode(); param->type = "Identifier"; param->name = token1; param->kind = "int"; param->line = lineindex;
            func_ast->children.push_back(param);
        }
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        if(strcmp(token, ",") == 0) { fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); } 
        else if(strcmp(token, ")") == 0) break;
    }
    for(int i = 0; i < can_num; i++) table[start_num + i].address = -1 - can_num + i;
    return 0;
}

void fun_body(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("fun_body");
    ASTNode* ast = new ASTNode(); ast->type = "BlockStmt"; ast->line = lineindex;
    check_match("{", p, "缺少函数体左大括号");
    
    declaration_list(p, table, num);
    if (p->son.back()->ast) {
        ASTNode* decls = p->son.back()->ast;
        if (decls->type == "VarDeclList") for(auto c : decls->children) ast->children.push_back(c);
        else ast->children.push_back(decls);
    }
    statement_list(p, table, num);
    if (p->son.back()->ast) {
        for(auto c : p->son.back()->ast->children) ast->children.push_back(c);
    }
    check_match("}", p, "缺少函数体右大括号");
    p->ast = ast;
}

int declaration_list(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("declaration_list");
    ASTNode* ast = new ASTNode(); ast->type = "VarDeclList";
    while (strcmp(token, "int") == 0) {
        declaration_stat(p, table, num, 0);
        if (p->son.back()->ast) {
            ASTNode* decls = p->son.back()->ast;
            if (decls->type == "VarDeclList") for(auto c : decls->children) ast->children.push_back(c);
            else ast->children.push_back(decls);
        }
    }
    p->ast = ast; return 0;
}

int declaration_stat(node* fa, struct vartable table[], int &num, int is_global) {
    node* p = fa->addSon("<declaration_stat>"); out(p);
    ASTNode* ast = new ASTNode(); ast->type = "VarDeclList";
    
    while(true) {
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        char var_name[20]; strcpy(var_name, token1); out(p);
        
        ASTNode* decl = new ASTNode(); decl->type = "VarDecl"; decl->line = lineindex;
        ASTNode* idNode = new ASTNode(); idNode->type = "Identifier"; idNode->name = var_name; idNode->kind = "int";
        decl->left = idNode;
        
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        int is_array = 0, arr_size = 1;
        if (strcmp(token, "[") == 0) {
            is_array = 1; out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            if (strcmp(token, "NUM") == 0) { arr_size = atoi(token1); out(p); }
            fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); check_match("]", p, "数组缺失右中括号");
        }
        if (name_def(var_name, table, num, is_array, arr_size, is_global) != 0) {
            printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 重复声明 <<<\n", lineindex, var_name);
            error_flag = 1;
        }
        
        if (strcmp(token, "=") == 0) {
            out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            expression(p, table, num); 
            decl->right = p->son.back()->ast;
            if(is_global) emit("STOG", table[num-1].address); else emit("STO", table[num-1].address);
        }
        ast->children.push_back(decl);
        
        if (strcmp(token, ",") == 0) { out(p); continue; }
        else if (strcmp(token, ";") == 0) { out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); break; }
        else { error_flag = 1; break; }
    }
    if (ast->children.size() == 1) p->ast = ast->children[0];
    else p->ast = ast;
    return 0;
}

int statement_list(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<state_ment>");
    ASTNode* ast = new ASTNode(); ast->type = "BlockStmt"; ast->line = lineindex;
    while (strcmp(token, "}") && strcmp(token, "case") && strcmp(token, "default") && strcmp(token, "break") && strcmp(token, "EOF") != 0) {
        statement(p, table, num);
        if (p->son.back()->ast) ast->children.push_back(p->son.back()->ast);
    }
    p->ast = ast; return 0;
}

int statement(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<statement>");
    if (strcmp(token, "if") == 0)    if_stat(p, table, num);
    else if (strcmp(token, "while") == 0) while_stat(p, table, num);
    else if (strcmp(token, "do") == 0)    dowhile_stat(p, table, num);
    else if (strcmp(token, "for") == 0)   for_stat(p, table, num);
    else if (strcmp(token, "switch")== 0) switch_stat(p, table, num);
    else if (strcmp(token, "read") == 0)  read_stat(p, table, num);
    else if (strcmp(token, "write") == 0) write_stat(p, table, num);
    else if (strcmp(token, "return")== 0) return_stat(p, table, num);
    else if (strcmp(token, "{") == 0)     compound_stat(p, table, num);
    else if (strcmp(token, "call") == 0)  call_stat(p, table, num);
    else expression_stat(p, table, num); 
    
    if (!p->son.empty() && p->son.back()->ast) p->ast = p->son.back()->ast;
    return 0;
}

int dowhile_stat(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<dowhile_stat>");
    ASTNode* ast = new ASTNode(); ast->type = "DoWhileStmt"; ast->line = lineindex;
    int label_start = labelp++; mark_label(label_start); out(p);
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    check_match("{", p, "do后缺少大括号");
    statement_list(p, table, num); ast->body = p->son.back()->ast;
    check_match("}", p, "do缺少结束大括号");
    check_match("while", p, "缺少while关键字"); check_match("(", p, "while缺少左括号");
    expression(p, table, num); ast->condition = p->son.back()->ast;
    emit("BRT", label_start, true); 
    check_match(")", p, "while缺少右括号"); check_match(";", p, "do-while缺少分号");
    p->ast = ast; return 0;
}

int switch_stat(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<switch_stat>"); out(p); 
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); check_match("(", p, "");
    expression(p, table, num); check_match(")", p, ""); check_match("{", p, "");
    int label_end = labelp++; 
    while (strcmp(token, "case") == 0) {
        out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        char case_val[20]; strcpy(case_val, token1); out(p);
        emit_no_op("DUP"); emit("LOADI", atoi(case_val)); emit_no_op("EQ");
        int label_next_case = labelp++; emit("BRF", label_next_case, true);
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); check_match(":", p, "");
        statement_list(p, table, num);
        if (strcmp(token, "break") == 0) {
            emit("BR", label_end, true);
            out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            if (strcmp(token, ";") == 0) { out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); }
        }
        mark_label(label_next_case);
    }
    if (strcmp(token, "default") == 0) {
        out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); check_match(":", p, "");
        statement_list(p, table, num);
    }
    check_match("}", p, ""); mark_label(label_end); emit_no_op("POP");
    return 0;
}

int return_stat(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<return_stat>"); ASTNode* ast = new ASTNode(); ast->type = "ReturnStmt"; ast->line = lineindex; out(p);
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    expression(p, table, num); ast->left = p->son.back()->ast;
    check_match(";", p, "return缺少分号");
    emit_no_op("RETURN"); p->ast = ast; return 0;
}

int if_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<if_stat>"); ASTNode* ast = new ASTNode(); ast->type = "IfStmt"; ast->line = lineindex; int label1,label2; out(p);
	fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
	check_match("(", p, "if缺少左括号");
	expression(p,table,num); ast->condition = p->son.back()->ast;
	check_match(")", p, "if缺少右括号");
	label1 = labelp++; emit("BRF", label1, true); 
	statement(p,table,num); ast->body = p->son.back()->ast;
	label2 = labelp++; emit("BR", label2, true);
	mark_label(label1);		
	if (strcmp(token, "else") == 0) {
		out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
		statement(p,table,num); ast->elseBody = p->son.back()->ast;
	}
	mark_label(label2); p->ast = ast; return 0;
}

int while_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<while_stat>"); ASTNode* ast = new ASTNode(); ast->type = "WhileStmt"; ast->line = lineindex; int label1,label2;
	label1 = labelp++; mark_label(label1); out(p);
	fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
	check_match("(", p, "while缺少左括号");
	expression(p,table,num); ast->condition = p->son.back()->ast;
	check_match(")", p, "while缺少右括号");
	label2 = labelp++; emit("BRF", label2, true);			
	statement(p,table,num); ast->body = p->son.back()->ast;
	emit("BR", label1, true);		
	mark_label(label2); p->ast = ast; return 0;
}

int for_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<for_stat>"); out(p); int label1,label2,label3,label4; 
	fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); check_match("(", p, "");
	expression(p,table,num); emit_no_op("POP"); check_match(";", p, "");
	label1 = labelp++; mark_label(label1); expression(p,table,num);
	label2 = labelp++; emit("BRF", label2, true); label3 = labelp++; emit("BR", label3, true);
	check_match(";", p, ""); label4 = labelp++; mark_label(label4);
	expression(p,table,num); emit_no_op("POP"); emit("BR", label1, true); check_match(")", p, "");
	mark_label(label3); statement(p,table,num); emit("BR", label4, true);
	mark_label(label2); return 0;
}

int write_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<write_stat>"); ASTNode* ast = new ASTNode(); ast->type="WriteStmt"; ast->line=lineindex; out(p);
	fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
	expression(p,table,num); ast->left = p->son.back()->ast;
	check_match(";", p, "write缺少分号"); emit_no_op("OUT"); p->ast=ast; return 0;
}

int read_stat(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<read_stat>"); ASTNode* ast = new ASTNode(); ast->type="ReadStmt"; ast->line=lineindex; out(p); 
    int address = 0, is_gl = 0, is_arr = 0;
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    if(strcmp(token, "ID") == 0) {
        ASTNode* id = new ASTNode(); id->type="Identifier"; id->name=token1; ast->left=id; out(p);
        if (lookup(table, num, token1, &address, &is_gl, &is_arr) != 0) {
            printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 未声明 <<<\n", lineindex, token1);
            error_flag = 1;
        } else {
            emit_no_op("IN");
            if(is_gl) emit("STOG", address); else emit("STO", address);
        }
        fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    } else { error_flag = 1; }
    check_match(";", p, "read缺少分号"); p->ast=ast; return 0;
}

int compound_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<compound_stat>"); out(p);
	fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
	statement_list(p,table,num); p->ast = p->son.back()->ast;
	check_match("}", p, "复合语句缺少大括号"); return 0;
}

int expression_stat(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<expression_stat>");
	if (strcmp(token, ";") == 0) { out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); return 0; }
	expression(p,table,num); p->ast = p->son.back()->ast; emit_no_op("POP");
	check_match(";", p, "表达式语句缺少分号"); return 0;
}

int call_stat(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<call_stat>"); ASTNode* ast = new ASTNode(); ast->type="CallExpr"; ast->line=lineindex; int address = 0, canshunum = 0; char fun_name[20];
    fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
    if (strcmp(token, "ID") == 0) { strcpy(fun_name, token1); ast->name = token1; out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); }
    check_match("(", p, "call缺少左括号");
    while(strcmp(token, ")") != 0 && strcmp(token, "EOF") != 0) {
        expression(p, table, num); ast->children.push_back(p->son.back()->ast); canshunum++;
        if(strcmp(token, ",") == 0) { out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); } 
        else if(strcmp(token, ")") == 0) break; else { error_flag = 1; break; }
    }
    check_match(")", p, "call缺少右括号");
    if (fun_lookup(fun_name, &address, canshunum) != 0) {
        printf("\n>>> [语义错误] 第 %d 行: 函数 '%s' 未声明或参数个数不匹配 <<<\n", lineindex, fun_name);
        error_flag = 1;
    } else {
        emit("CAL", address); emit_no_op("POP");
    }
    check_match(";", p, "call缺少分号"); p->ast=ast; return 0;
}

int expression(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<expression>"); 
    if (strcmp(token, "ID")==0){
        long fileadd = ftell(fin); char next_tok[20], next_tok1[40]; int l;
        fscanf(fin, "%s\t%s\t%d\n", next_tok, next_tok1, &l);
        int is_array_access = 0; if(strcmp(next_tok, "[") == 0) is_array_access = 1;

        if (strcmp(next_tok, "=")==0 || is_array_access) {
            fseek(fin, fileadd, SEEK_SET); int addr = 0, is_gl = 0, is_arr = 0;
            int found = (lookup(table, num, token1, &addr, &is_gl, &is_arr) == 0);
            if (!found) {
                printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 未声明 <<<\n", lineindex, token1);
                error_flag = 1;
            }
            
            ASTNode* assign = new ASTNode(); assign->type="AssignExpr"; assign->op="="; assign->line=lineindex;
            ASTNode* idNode = new ASTNode(); idNode->type="Identifier"; idNode->name=token1; assign->left=idNode;

            char var_name[20]; strcpy(var_name, token1); out(p);
            fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);

            int has_index = (strcmp(token, "[") == 0);
            if(has_index) {
                if (found && !is_arr) {
                    printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 不是数组 <<<\n", lineindex, var_name);
                    error_flag = 1;
                }
                out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); expression(p, table, num); check_match("]", p, "数组赋值缺少右括号");
            }
            if(strcmp(token, "=") != 0) { 
                if(found) {
                    if(has_index) { if(is_gl) emit("LOADGARR", addr); else emit("LOADARR", addr); } 
                    else { if(is_gl) emit("LOADG", addr); else emit("LOAD", addr); }
                }
                p->ast = idNode; return 0;
            }
            out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            bool_expr(p, table, num); assign->right = p->son.back()->ast;
            
            if(found) {
                if(has_index) { if(is_gl) emit("STOGARR", addr); else emit("STOARR", addr); } 
                else if(is_arr) {
                    printf("\n>>> [语义错误] 第 %d 行: 数组 '%s' 赋值缺少下标 <<<\n", lineindex, var_name);
                    error_flag = 1;
                } else if(is_gl) emit("STOG", addr); else emit("STO", addr);
            }
            emit("LOADI", 0); p->ast = assign; return 0;
        } else { fseek(fin, fileadd, SEEK_SET); bool_expr(p, table, num); p->ast=p->son.back()->ast; }
    } else { bool_expr(p, table, num); p->ast=p->son.back()->ast; }
    return 0;
}

int bool_expr(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<bool_expr>"); additive_expr(p,table,num); ASTNode* left_ast = p->son.back()->ast;
	if (strcmp(token, ">") == 0 || strcmp(token, ">=") == 0 || strcmp(token, "<") == 0 || strcmp(token, "<=") == 0 || strcmp(token, "==") == 0 || strcmp(token, "!=") == 0) {
		out(p); char token2[20]; strcpy(token2,token);
		fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
		additive_expr(p,table,num); ASTNode* right_ast = p->son.back()->ast;
        ASTNode* bin = new ASTNode(); bin->type="BinaryExpr"; bin->op=token2; bin->line=lineindex; bin->left=left_ast; bin->right=right_ast;
		if(strcmp(token2,">")==0) emit_no_op("GT"); if(strcmp(token2,">=")==0) emit_no_op("GE");
		if(strcmp(token2,"<")==0) emit_no_op("LES"); if(strcmp(token2,"<=")==0) emit_no_op("LE");
		if(strcmp(token2,"==")==0) emit_no_op("EQ"); if(strcmp(token2,"!=")==0) emit_no_op("NOTEQ");
        p->ast = bin;
	} else p->ast = left_ast;
	return 0;
}

int additive_expr(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<additive_expr>"); term(p,table,num); ASTNode* curr_ast = p->son.back()->ast;
	while (strcmp(token, "+") == 0 || strcmp(token, "-") == 0) {
		out(p); char token2[20]; strcpy(token2,token);
		fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
		term(p,table,num); ASTNode* right_ast = p->son.back()->ast;
        ASTNode* bin = new ASTNode(); bin->type="BinaryExpr"; bin->op=token2; bin->line=lineindex; bin->left=curr_ast; bin->right=right_ast; curr_ast=bin;
		if(strcmp(token2,"+")==0) emit_no_op("ADD"); if(strcmp(token2,"-")==0) emit_no_op("SUB");
	}
    p->ast = curr_ast; return 0;
}

int term(node* fa,struct vartable table[],int &num) {
	node* p = fa->addSon("<term>"); factor(p,table,num); ASTNode* curr_ast = p->son.back()->ast;
	while (strcmp(token, "*") == 0 || strcmp(token, "/") == 0) {
		out(p); char token2[20]; strcpy(token2,token);
		fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
		factor(p,table,num); ASTNode* right_ast = p->son.back()->ast;
        ASTNode* bin = new ASTNode(); bin->type="BinaryExpr"; bin->op=token2; bin->line=lineindex; bin->left=curr_ast; bin->right=right_ast; curr_ast=bin;
		if(strcmp(token2,"*")==0) emit_no_op("MULT"); if(strcmp(token2,"/")==0) emit_no_op("DIV");
	}
    p->ast = curr_ast; return 0;
}

int factor(node* fa, struct vartable table[], int &num) {
    node* p = fa->addSon("<factor>"); 
    if (strcmp(token, "(") == 0) {
        out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
        expression(p, table, num); p->ast = p->son.back()->ast; check_match(")", p, "因式缺少右括号");
    } else {
        if (strcmp(token, "ID") == 0) {
            int addr = 0, is_gl = 0, is_arr = 0;
            int found = (lookup(table, num, token1, &addr, &is_gl, &is_arr) == 0);
            if (!found) {
                printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 未声明 <<<\n", lineindex, token1);
                error_flag = 1;
            }
            ASTNode* id = new ASTNode(); id->type="Identifier"; id->name=token1; id->line=lineindex;
            out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex);
            if(strcmp(token, "[") == 0) {
                if (found && !is_arr) {
                    printf("\n>>> [语义错误] 第 %d 行: 变量 '%s' 不是数组 <<<\n", lineindex, id->name.c_str());
                    error_flag = 1;
                }
                out(p); fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); expression(p, table, num); check_match("]", p, "因式数组缺少右方括号");
                if (found && is_arr) emit("LOADARR", addr);
            } else if(found) { if(is_gl) emit("LOADG", addr); else emit("LOAD", addr); }
            p->ast = id;
        } else if(strcmp(token, "NUM") == 0){
            ASTNode* num_ast = new ASTNode(); num_ast->type="BasicLit"; num_ast->kind="INT"; num_ast->value=token1; num_ast->line=lineindex;
            out(p); emit("LOADI", atoi(token1));	
            fscanf(fin, "%s\t%s\t%d\n", token, token1, &lineindex); p->ast=num_ast;
        } else { error_flag=1; }
    }
    return 0;
}
