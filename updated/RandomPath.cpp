#include <math.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <set>

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

LLVMContext &getGlobalContext()
{
    static LLVMContext context;
    return context;
}

class Node
{
public:
    std::pair<std::string, std::pair<int, int>> data;
    Node *next;
    Node *prev;
    int type = -1; // 0 = add , 1 = sub , 2 = mul, 3 = div
    int value = 0;
};

std::set<Node **> inputsTree;

void generatePath(BasicBlock *BB);
std::string getSimpleNodeLabel(const BasicBlock *Node);
std::pair<std::string, std::pair<int, int>> findVar(std::string key);
Node **searchTree(std::string key);
std::vector<std::string> split(const std::string &s, char seperator);
void printInputsDomain();
void printRandomInput();
void handleArithmeticalParent(Node **parent, std::pair<int, int> domain);
void modifyDomain(std::string cmp, Node **parent, int bound);
void addArithNode(Instruction &I);

std::set<std::string> path;
std::set<std::pair<std::string, std::pair<int, int>>> varDomain;

int main(int argc, char **argv)
{
    // Read the IR file.
    LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(argv[1], Err, Context);
    if (M == nullptr)
    {
        fprintf(stderr, "error: failed to load LLVM IR file \"%s\"", argv[1]);
        return EXIT_FAILURE;
    }

    for (auto &F : *M)
        if (strncmp(F.getName().str().c_str(), "main", 4) == 0)
        {
            BasicBlock *BB = dyn_cast<BasicBlock>(F.begin());
            llvm::outs() <<getSimpleNodeLabel(BB) << "\n";
            generatePath(BB);
        }
    printInputsDomain();
    printRandomInput();
    return 0;
}

void generatePath(BasicBlock *BB)
{
    const Instruction *TInst = BB->getTerminator();
    unsigned NSucc = TInst->getNumSuccessors();
    BasicBlock *Succ = NULL;
    if (NSucc == 1)
    {
        Succ = TInst->getSuccessor(0);
        llvm::outs() << getSimpleNodeLabel(Succ) << "\n";

        //    generatePath(Succ);
    }
    else if (NSucc > 1)
    {
        srand(time(NULL));
        unsigned rnd = std::rand() / (RAND_MAX / NSucc); // rand() return a number between 0 and RAND_MAX
        Succ = TInst->getSuccessor(rnd);
        llvm::outs() << getSimpleNodeLabel(Succ) << "\n";
        path.insert(getSimpleNodeLabel(Succ));

        // Calculate and Evaluate proper input
    }
    if (Succ == NULL)
    {
        return;
    }
    llvm::StringRef comparedBool = TInst->getOperand(0)->getName();
    if (strncmp(TInst->getOpcodeName(), "br", 2) == 0)
    {
        // llvm::outs() << *TInst->getOperand(0)->getType() << "\n";
        std::string type_str;
        llvm::raw_string_ostream rso(type_str);
        TInst->getOperand(0)->getType()->print(rso);
        if (true)
        {
            for (Instruction &I : *BB)
            {
                // llvm::outs() << I.getOpcodeName() << "\n";

                // compare instruction
                if (strncmp(I.getOpcodeName(), "icmp", 4) == 0 && I.getName().compare(comparedBool) == 0)
                {
                    std::string strInst;
                    llvm::raw_string_ostream(strInst) << I;
                    // first operand of icmp instruction
                    llvm::Value *v1 = I.getOperand(0);
                    // second operand (constant) of icmp instruction
                    llvm::Value *v2 = I.getOperand(1);

                    std::string target = *std::next(path.begin(), path.size() - 1);

                    // backtrack
                    std::string strOpr;
                    llvm::raw_string_ostream(strOpr) << *I.getOperand(0);
                    // llvm::outs() << strInst << "\n";

                    std::vector<std::string> args = split(strOpr, '=');
                    std::string operand2Inst;
                    llvm::raw_string_ostream(operand2Inst) << *I.getOperand(1);
                    // llvm::outs() << strInst << "\n";

                    std::vector<std::string> opArgs = split(operand2Inst, ' ');
                    //llvm::outs() << "args1" << opArgs[1] << "\n";
                    
                    // llvm::outs() << "lowerbound: " << lowerbound << "\n";
                    Node **leaf = searchTree(args[0]);
                    llvm::outs() << "[ICMP Inst]: Leaf name: " << (*leaf)->data.first << "\n";
                    Node *parent = (*leaf)->prev;
                    llvm::outs() << "[ICMP Inst]: parent name: " << parent->data.first << "\n";

                    // Equal
                    if (strInst.find("eq") != std::string::npos)
                    {
                    }
                    // Not Equal
                    else if (strInst.find("ne") != std::string::npos)
                    {
                    }

                    // signed greater than
                    else if (strInst.find("sgt") != std::string::npos)
                    {
                        // check if target is label 1 then conditiion must be true ( v1 > v2)
                        if (target.compare(TInst->getOperand(1)->getName()) == 0)
                        {
                            int upperbound = atoi(opArgs[1].c_str());
                            modifyDomain("lt", &parent, upperbound);
                        }
                        // condition must be false in order to reach the target
                        else if (target.compare(TInst->getOperand(2)->getName()) == 0)
                        {
                            int lowerbound = atoi(opArgs[1].c_str());
                            modifyDomain("gt", &parent, lowerbound + 1);
                        }
                    }
                    // signed greater or equal
                    else if (strInst.find("sge") != std::string::npos)
                    {
                        // check if target is label 1 then conditiion must be true ( v1 > v2)
                        if (target.compare(TInst->getOperand(1)->getName()) == 0)
                        {
                            int upperbound = atoi(opArgs[1].c_str());
                            modifyDomain("lt", &parent, upperbound - 1);
                        }
                        // condition must be false in order to reach the target
                        else if (target.compare(TInst->getOperand(2)->getName()) == 0)
                        {
                            int lowerbound = atoi(opArgs[1].c_str());
                            //llvm::outs() << "lowerbound---"<< lowerbound <<*I.getOperand(1)<< "\n";

                            modifyDomain("gt", &parent, lowerbound);
                        }
                    }
                    // signed less than
                    else if (strInst.find("slt") != std::string::npos)
                    {
                        if (target.compare(TInst->getOperand(1)->getName()) == 0)
                        {
                            int lowerbound = atoi(opArgs[1].c_str());
                            modifyDomain("gt", &parent, lowerbound);
                        }
                        // condition must be false in order to reach the target
                        else if (target.compare(TInst->getOperand(2)->getName()) == 0)
                        {
                            int upperbound = atoi(opArgs[1].c_str());
                            modifyDomain("lt", &parent, upperbound - 1);
                        }
                    }
                    // signed less or equal
                    else if (strInst.find("sle") != std::string::npos)
                    {
                        if (target.compare(TInst->getOperand(1)->getName()) == 0)
                        {
                            int lowerbound = atoi(opArgs[1].c_str());
                            modifyDomain("gt", &parent, lowerbound + 1);
                        }
                        // condition must be false in order to reach the target
                        else if (target.compare(TInst->getOperand(2)->getName()) == 0)
                        {
                            int upperbound = atoi(opArgs[1].c_str());
                            modifyDomain("lt", &parent, upperbound);
                        }
                    }
                }
                // assigning a variable infinite domain
                else if (strncmp(I.getOpcodeName(), "alloca", 6) == 0)
                {
                    std::pair<int, int> domain = {-1000, 1000};
                    std::pair<std::string, std::pair<int, int>> p = {I.getName(), domain};

                    varDomain.insert(p);
                    Node **headP = (Node **)malloc(size_t(new Node()));
                    Node *head = new Node();
                    head->data = p;
                    head->prev = NULL;
                    head->next = NULL;
                    (*headP) = head;
                    inputsTree.insert(headP);
                    llvm::outs() << "[Alloca Inst]: " << headP << "\n";
                }
                // store instruction
                else if (strncmp(I.getOpcodeName(), "store", 5) == 0)
                {
                    // const value bug
                    // llvm::outs() << "[Store Inst]: Source node with data : " << *I.getOperand(0) << "\n";
                    Node **sourceNode = searchTree(I.getOperand(0)->getName());

                    std::string op0Str;
                    llvm::raw_string_ostream(op0Str) << *I.getOperand(0);
                    if (sourceNode == NULL || (*sourceNode)->prev == NULL || op0Str.find('%') == std::string::npos)
                    {
                        continue;
                    }
                    StringRef name;
                    for (auto op = I.op_begin(); op != I.op_end(); op++)
                    {
                        Value *v = op->get();
                        name = v->getName();
                    }
                    llvm::outs() << "[Store Inst]: Source node with data : " << (*sourceNode)->data.first << "\n";
                    // fix domain value
                    std::string strInst;
                    llvm::raw_string_ostream(strInst) << *I.getOperand(0);
                    // llvm::outs() << "isnt: " << I << "\n";
                    llvm::outs() << "[Store Inst]: Creating node with data : " << name << "\n";
                    std::vector<std::string> args = split(strInst, '=');
                    std::pair<std::string, std::pair<int, int>> p = {name, (*sourceNode)->data.second};
                    ///////////
                    varDomain.insert(p);

                    Node **node = searchTree(p.first);
                    (*node)->prev = *sourceNode;
                    (*node)->data = p;
                    (*node)->next = NULL;
                    (*sourceNode)->next = (*node);
                }
                // load instruction
                else if (strncmp(I.getOpcodeName(), "load", 4) == 0)
                {
                    std::string strInst;
                    llvm::raw_string_ostream(strInst) << I;
                    // llvm::outs() << strInst << "\n";

                    std::vector<std::string> args = split(strInst, '=');
                    std::pair<int, int> domain = findVar(I.getOperand(0)->getName().str()).second;
                    llvm::outs() << "[Load Inst]: Creating node with data : " << args[0] << "\n";
                    std::pair<std::string, std::pair<int, int>> p = {args[0], domain};

                    varDomain.insert(p);
                    // problem in finding source
                    // llvm::outs() << "[Load Inst]: Value : " << I.getOperand(1) << "\n";

                    Node **sourceNode = searchTree(I.getOperand(0)->getName());
                    llvm::outs() << "[Load Inst]: Source node with data : " << (*sourceNode)->data.first << "\n";
                    Node *node = new Node();
                    node->data = p;
                    node->prev = *sourceNode;
                    (*sourceNode)->next = node;
                    node->next = NULL;
                }
                // add instruction
                else if (strncmp(I.getOpcodeName(), "add", 3) == 0 || strncmp(I.getOpcodeName(), "sub", 3) == 0 || strncmp(I.getOpcodeName(), "mul", 3) == 0)
                {
                    // backtrack
                    addArithNode(I);
                }
            }
        }
    }

    generatePath(Succ);
}

std::pair<std::string, std::pair<int, int>> findVar(std::string key)
{
    std::pair<std::string, std::pair<int, int>> res;
    for (std::pair<std::string, std::pair<int, int>> v : varDomain)
    {
        if (v.first.compare(key) == 0)
        {
            return v;
        }
    }

    return res;
}

void addArithNode(Instruction &I)
{
    llvm::Value *op1 = I.getOperand(0);
    // Const
    llvm::Value *op2 = I.getOperand(1);

    std::string strInst;
    llvm::raw_string_ostream(strInst) << *op1;
    // llvm::outs() << strInst << "\n";

    std::vector<std::string> args = split(strInst, '=');

    std::string operand2Inst;
    llvm::raw_string_ostream(operand2Inst) << *op2;
    std::vector<std::string> opArgs = split(operand2Inst, ' ');

    std::pair<std::string, std::pair<int, int>> p = {I.getName(), {-1000, 1000}};

    Node **sourceNode = searchTree(args[0]);
    llvm::outs() << "[Arith Inst]: Source node with data : " << (*sourceNode)->data.first << "\n";
    Node *node = new Node();
    node->data = p;
    if (strncmp(I.getOpcodeName(), "add", 3) == 0)
    {
        node->type = 0;
    }
    else if (strncmp(I.getOpcodeName(), "sub", 3) == 0)
    {
        node->type = 1;
    }
    else if (strncmp(I.getOpcodeName(), "mul", 3) == 0)
    {
        node->type = 2;
    }

    // node->type = 0;
    node->value = atoi(opArgs[1].c_str());
    node->prev = *sourceNode;
    llvm::outs() << "[Arith Inst]: Destination node with data : " << node->data.first << "\n";
    (*sourceNode)->next = node;
    node->next = NULL;
}

void printInputsDomain()
{
    for (Node **n : inputsTree)
    {
        if ((*n)->data.first.compare("retval") == 0)
        {
            continue;
        }
        llvm::outs() << "Input " << (*n)->data.first << " Domain: " << (*n)->data.second.first << ", " << (*n)->data.second.second << "\n";
    }
}

void printRandomInput()
{
    for (Node **n : inputsTree)
    {
        // srand(time(NULL));
        // if((*n)->data.second.second == 0){
        //     (*n)->data.second.second = 1;
        // }
        int random = (*n)->data.second.first + ( std::rand() % ( (*n)->data.second.first - (*n)->data.second.second + 1 ) );
        // int rnd = std::rand() / ((*n)->data.second.first / (*n)->data.second.second);
        
        if ((*n)->data.first.compare("retval") == 0)
        {
            continue;
        }
        llvm::outs() << "Input " << (*n)->data.first << " Value: " << random << "\n";
    }
}

Node **searchTree(std::string key)
{
    Node *temp = NULL;
    for (Node **n : inputsTree)
    {
        temp = *n;
        // llvm::outs() << "[SearchTree]: source node:" << temp->data.first << "\n";
        // llvm::outs() << "[SearchTree]: next node:" << temp->prev << "\n";
        if (temp->data.first.compare(key) == 0)
        {
            //  llvm::outs() << "[SearchTree]: Result: " << temp->data.first << "\n";
            return n;
        }
        while (temp->next != NULL)
        {
            if (temp->data.first.compare(key) == 0)
            {
                // llvm::outs() << "[SearchTree]: Result: " << temp->data.first << "\n";
                return &temp->prev->next;
            }
            temp = temp->next;
            // llvm::outs() << "[SearchTree]: source node:" << temp->data.first << "\n";
        }
        if (temp->data.first.compare(key) == 0)
        {
            // llvm::outs() << "[SearchTree]: Result: " << temp->data.first << "\n";
            return &temp->prev->next;
        }
    }
    return NULL;
}

std::string getSimpleNodeLabel(const BasicBlock *Node)
{
    if (!Node->getName().empty())
        return Node->getName().str();
    std::string Str;
    raw_string_ostream OS(Str);
    Node->printAsOperand(OS, false);
    return OS.str();
}

std::vector<std::string> split(const std::string &s, char seperator)
{
    std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while ((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring(s.substr(prev_pos, pos - prev_pos));

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

    return output;
}

void modifyDomain(std::string cmp, Node **parent, int bound)
{
    float divRes;
    if (cmp.compare("gt") == 0)
    {
        int lowerbound = bound;

        if (lowerbound < (*parent)->data.second.second)
        {
            // std::pair<int, int> domain = {std::max(lowerbound, parent->data.second.first) + 1, parent->data.second.second};
            // llvm::outs() << "[ICMP Inst]: Domain: " << cmp << ", " << bound << "\n";
            while (parent != NULL)
            {
                std::pair<int, int> domain = {std::max(lowerbound, (*parent)->data.second.first), (*parent)->data.second.second};
                llvm::outs() << "[ICMP Inst]: " << (*parent)->data.first << " Domain: " << domain.first << ", " << domain.second << "\n";
                // llvm::outs() << "PT: " << parent->type << "\n";
                if ((*parent)->type == -1)
                {
                    (*parent)->data.second = domain;
                }
                else
                {
                    switch ((*parent)->type)
                    {
                    // add
                    case 0:
                        lowerbound -= (*parent)->value;
                        domain.first -= (*parent)->value;
                        break;
                    // sub
                    case 1:
                        lowerbound += (*parent)->value;
                        domain.first += (*parent)->value;
                        break;
                    // mul
                    case 2:
                        divRes = (float)lowerbound / (float)(*parent)->value;
                        lowerbound /= (*parent)->value;

                        if (divRes < 0)
                        {
                            domain.first /= (int)divRes - 1;
                            int temp = domain.second;
                            domain.second = -domain.first;
                            domain.first = -temp;
                            lowerbound = -temp;
                            llvm::outs() << "[ICMP Inst MUL]: Domain: " << domain.first << ", " << domain.second << "\n";
                            modifyDomain("lt", parent, domain.second);
                            return;
                        }

                        domain.first /= (int)divRes + 1;
                        break;
                    // div
                    case 3:
                        lowerbound *= (*parent)->value;
                        domain.first *= (*parent)->value;
                        if (domain.first > domain.second)
                        {
                            domain.second = -domain.second;
                            domain.first = -domain.first;
                        }
                        break;
                    default:
                        break;
                    }

                    // (*parent)->data.second = domain;
                    // Node **src = searchTree((*parent)->prev->data.first);
                    (*parent)->prev->data.second = domain;
                    // handleArithmeticalParent(&parent, domain);
                }
                // parent->data.second;
                if ((*parent)->prev == NULL)
                    break;
                parent = &(*parent)->prev;
            }
        }
    }
    else if (cmp.compare("lt") == 0)
    {
        int upperbound = bound;
        if (upperbound > (*parent)->data.second.first)
        {
            // std::pair<int, int> domain = {std::max(lowerbound, parent->data.second.first) + 1, parent->data.second.second};
            // llvm::outs() << "[ICMP Inst]: Domain: " << domain.first << ", " << domain.second << "\n";
            while ((*parent) != NULL)
            {
                std::pair<int, int> domain = {(*parent)->data.second.first, std::min(upperbound, (*parent)->data.second.second)};
                llvm::outs() << "[ICMP Inst]: " << (*parent)->data.first << " Domain: " << domain.first << ", " << domain.second << "\n";
                // llvm::outs() << "PT: " << parent->type << "\n";
                if ((*parent)->type == -1)
                {
                    (*parent)->data.second = domain;
                }
                else
                {
                    switch ((*parent)->type)
                    {
                    // add
                    case 0:
                        upperbound -= (*parent)->value;
                        domain.second -= (*parent)->value;
                        break;
                    // sub
                    case 1:
                        upperbound += (*parent)->value;
                        domain.second += (*parent)->value;
                        break;
                    // mul
                    case 2:
                        divRes = (float)upperbound / (float)(*parent)->value;
                        upperbound /= (*parent)->value;

                        if (divRes < 0)
                        {
                            domain.first /= (int)divRes - 1;
                            int temp = domain.second;
                            domain.second = -domain.first;
                            domain.first = -temp;
                            upperbound = -temp;
                            llvm::outs() << "[ICMP Inst MUL]: Domain: " << domain.first << ", " << domain.second << "\n";
                        }
                        else
                            domain.first /= (int)divRes + 1;
                        break;
                    // div
                    case 3:
                        upperbound *= (*parent)->value;
                        domain.second *= (*parent)->value;
                        if (domain.first > domain.second)
                        {
                            int temp;
                            temp = domain.first;
                            domain.first = domain.second;
                            domain.second = temp;
                        }
                        break;
                    default:
                        break;
                    }

                    // (*parent)->data.second = domain;
                    // Node **src = searchTree((*parent)->prev->data.first);
                    (*parent)->prev->data.second = domain;
                    // handleArithmeticalParent(&parent, domain);
                }
                // parent->data.second;
                if ((*parent)->prev == NULL)
                    break;
                parent = &(*parent)->prev;
            }
        }
    }

    // Parent found
    llvm::outs() << "[ICMP Inst]: Name: " << (*parent)->data.first << " Range :  " << (*parent)->data.second.first << " , " << (*parent)->data.second.second << "\n";
}