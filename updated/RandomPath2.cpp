#include <cstdio>
#include <iostream>
#include <set>
#include <iostream>
#include <cstdlib>
#include <ctime>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

LLVMContext &getGlobalContext()
{
    static LLVMContext context;
    return context;
}

void generatePath(BasicBlock *BB, std::set<std::pair<std::string, BasicBlock *>> visitedBlocksInPath);
std::string getSimpleNodeLabel(const BasicBlock *Node);
void reconstruct(BasicBlock * firstBlock, int args[]);

std::set<std::pair<std::string, BasicBlock *>> allBlocks;
std::set<std::pair<std::string, BasicBlock *>> visitedBlocks;

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
    int args[atoi(argv[2])];
    for(int i = 0; i < atoi(argv[2]); i++){
        args[i] = atoi(argv[3+i]);
    }

    // 0. Collecting all basic blocks
    for (auto &F : *M)
        for (auto &BB : F)
            allBlocks.insert(std::pair<std::string, BasicBlock *>(F.getName(), &BB));
    // Printing all blocks
    /*for (auto it=allBlocks.begin(); it != allBlocks.end(); ++it){
        errs() << ' ' << (*it).first;
        (*it).second->dump();
        errs() << '\n';
    }*/
    reconstruct(allBlocks.begin()->second, args);
    for (int i = 0; i < 5 && (visitedBlocks.size() / allBlocks.size() < 1); i++)
    {
        for (auto &F : *M)
            if (strncmp(F.getName().str().c_str(), "main", 4) == 0)
            {
                std::set<std::pair<std::string, BasicBlock *>> visitedBlocksInPath;
                BasicBlock *BB = dyn_cast<BasicBlock>(F.begin());
                llvm::outs() << getSimpleNodeLabel(BB) << "\n";
                generatePath(BB, visitedBlocksInPath);
            }

        llvm::outs() << "---End of path---\n\n";
        errs() << "Total number of blocks: " << allBlocks.size() << '\n';
        errs() << "Total number of visited blocks: " << visitedBlocks.size() << '\n';
        errs() << "Coverage: " << visitedBlocks.size() * 100 / allBlocks.size() << "%\n";
    }

    // errs() << "Total number of blocks: " << allBlocks.size() << '\n';
    // errs() << "Total number of visited blocks: " << visitedBlocks.size() << '\n';
    // errs() << "Coverage: " << visitedBlocks.size() * 100 / allBlocks.size() << "%\n";

    return 0;
}

void generatePath(BasicBlock *BB, std::set<std::pair<std::string, BasicBlock *>> visitedBlocksInPath)
{

    visitedBlocks.insert(std::pair<std::string, BasicBlock *>(BB->getParent()->getName(), BB));
    visitedBlocksInPath.insert(std::pair<std::string, BasicBlock *>(BB->getParent()->getName(), BB));

    const Instruction *TInst = BB->getTerminator();
    unsigned NSucc = TInst->getNumSuccessors();
    if (NSucc == 1)
    {
        BasicBlock *Succ = TInst->getSuccessor(0);
        llvm::outs() << getSimpleNodeLabel(Succ) << "\n";
        generatePath(Succ, visitedBlocksInPath);
    }
    else if (NSucc > 1)
    {
        srand(time(NULL));
        unsigned rnd = std::rand() / (RAND_MAX / NSucc); // rand() return a number between 0 and RAND_MAX
        BasicBlock *Succ = TInst->getSuccessor(rnd);

        // Flip successor if this one is visited
        std::string fname = TInst->getSuccessor(0)->getParent()->getName();
        if (visitedBlocks.find(std::pair<std::string, BasicBlock *>(fname, Succ)) != visitedBlocks.end())
            Succ = TInst->getSuccessor(NSucc - rnd - 1);

        if (visitedBlocksInPath.find(std::pair<std::string, BasicBlock *>(fname, BB)) != visitedBlocksInPath.end() &&
            visitedBlocksInPath.find(std::pair<std::string, BasicBlock *>(fname, Succ)) != visitedBlocksInPath.end())
            Succ = TInst->getSuccessor(1);

        llvm::outs() << getSimpleNodeLabel(Succ) << "\n";
        generatePath(Succ, visitedBlocksInPath);
    }
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


void reconstruct(BasicBlock * firstBlock, int args[]){
    std::set<StringRef> inputs;
    for(Instruction &I: *firstBlock){
        if (strncmp(I.getOpcodeName(), "alloca", 6) == 0){
            inputs.insert(I.getName());
        }
        if (strncmp(I.getOpcodeName(), "store", 5) == 0){
            if(inputs.find(I.getOperand(0)->getName()) != inputs.end()){
                Instruction * temp = new StoreInst(args[0], )
            }
        }
        llvm::outs() << I << "\n";
    }
}